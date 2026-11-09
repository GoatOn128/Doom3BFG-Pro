#include "engine/sys/sys_savegame.h"

#include "shared/idlib/csystems/cvarsystemlocal.h"
#include "shared/idlib/lib.h"
#include "shared/idlib/sys/sys_time.h"

#include <algorithm>
#include <cstdlib>
#include <iterator>

idCVar saveGame_verbose("saveGame_verbose", "0", CVAR_BOOL | CVAR_SYSTEM, "debug spam");
idCVar saveGame_enable("saveGame_enable", "1", CVAR_BOOL, "are savegames enabled");
idCVar saveGame_minSaveTime("saveGame_minSaveTime", "2000", CVAR_FLOAT,
                            "minimum time to show the save icon");
idCVar saveGame_checksum("saveGame_checksum", "1", CVAR_BOOL, "data integrity check");

namespace {
idSaveGameManager* activeSaveGameManager = nullptr;
const char* const saveGameErrorStrings[] = {
    "SAVEGAME_E_CANCELLED", "SAVEGAME_E_INSUFFICIENT_ROOM", "SAVEGAME_E_CORRUPTED",
    "SAVEGAME_E_UNABLE_TO_SELECT_STORAGE_DEVICE", "SAVEGAME_E_UNKNOWN",
    "SAVEGAME_E_INVALID_FILENAME", "SAVEGAME_E_STEAM_ERROR", "SAVEGAME_E_FOLDER_NOT_FOUND",
    "SAVEGAME_E_FILE_NOT_FOUND", "SAVEGAME_E_DLC_NOT_FOUND", "SAVEGAME_E_INVALID_USER",
    "SAVEGAME_E_PROFILE_TOO_BIG", "SAVEGAME_E_DISC_SWAP",
    "SAVEGAME_E_INCOMPATIBLE_NEWER_VERSION"};
}

idStr GetSaveGameErrorString(int errorMask) {
  idStr result;
  for (int bit = 0; bit < 17 && errorMask > 0; ++bit) {
    const int mask = 1 << bit;
    if ((errorMask & mask) == 0) continue;
    errorMask ^= mask;
    result.Append(bit < static_cast<int>(std::size(saveGameErrorStrings))
                      ? saveGameErrorStrings[bit]
                      : "SAVEGAME_E_UNKNOWN_BIT");
    if (errorMask > 0) result.Append(", ");
  }
  return result;
}

idSaveGameDetails::idSaveGameDetails() { Clear(); }

void idSaveGameDetails::Clear() {
  ClearDescriptors();
  damaged = false;
  date = 0;
  slotName.Clear();
}

void idSaveGameDetails::ClearDescriptors() { descriptors.Clear(); }

void idSaveGameDetails::SetDescriptorString(const char* key, const char* value) {
  descriptors.Set(key, value);
}

void idSaveGameDetails::SetDescriptorInt(const char* key, int value) {
  descriptors.SetInt(key, value);
}

void idSaveGameDetails::WriteDescriptorsToIniFile(idFile_SaveGame* file) const {
  descriptors.WriteToIniFile(file);
}

bool idSaveGameDetails::operator==(const idSaveGameDetails& other) const {
  return slotName.Icmp(other.slotName.data) == 0;
}

bool idSaveGameDetails::operator<(const idSaveGameDetails& other) const { return date > other.date; }

bool SavegameReadDetailsFromFile(idFile* file, idSaveGameDetails* details) {
  if (file == nullptr || details == nullptr) return false;
  details->damaged = false;
  if (!details->descriptors.ReadFromIniFile(file)) details->damaged = true;
  const idKeyValue* ignore = details->descriptors.FindKey("ignore_checksum");
  const bool ignoreChecksum = ignore != nullptr && std::strtol(ignore->value.data, nullptr, 10) != 0;
  if (!ignoreChecksum) {
    const idKeyValue* checksumValue = details->descriptors.FindKey(SAVEGAME_DETAIL_FIELD_CHECKSUM);
    const int expected = checksumValue ? static_cast<int>(std::strtol(checksumValue->value.data, nullptr, 10)) : 0;
    details->descriptors.Delete(SAVEGAME_DETAIL_FIELD_CHECKSUM);
    if (expected == 0 || static_cast<int>(details->descriptors.Checksum()) != expected)
      details->damaged = true;
  }
  return true;
}

const idStr& GetSaveFolder(idSaveGameManager::packageType_t type) {
  static idStrStatic<MAX_FOLDER_NAME_LENGTH> folders[idSaveGameManager::PACKAGE_NUM];
  static bool initialized = false;
  if (!initialized) {
    initialized = true;
    folders[idSaveGameManager::PACKAGE_GAME] = SAVEGAME_GAME_DIRECTORY_PREFIX;
    folders[idSaveGameManager::PACKAGE_PROFILE] = SAVEGAME_PROFILE_DIRECTORY_PREFIX;
    folders[idSaveGameManager::PACKAGE_RAW] = SAVEGAME_RAW_DIRECTORY_PREFIX;
  }
  return folders[type];
}

idStr AddSaveFolderPrefix(const char* folder, idSaveGameManager::packageType_t type) {
  idStr result = GetSaveFolder(type);
  result.Append(folder != nullptr ? folder : "");
  return result;
}

idStr RemoveSaveFolderPrefix(const char* folder, idSaveGameManager::packageType_t type) {
  idStr result(folder != nullptr ? folder : "");
  result.StripLeading(GetSaveFolder(type).data);
  return result;
}

idSaveLoadParms::idSaveLoadParms() { Init(); }
idSaveLoadParms::~idSaveLoadParms() = default;

void idSaveLoadParms::Init() {
  files.ClearFree();
  mode = SAVEGAME_MBF_NONE;
  directory.Clear();
  pattern.Clear();
  postPattern.Clear();
  requiredSpaceInBytes = 0;
  description.Clear();
  detailList.ClearFree();
  callbackSignal.Clear();
  errorCode = SAVEGAME_E_NONE;
  inputDeviceId = -1;
  skipErrorDialogMask = 0;
}

void idSaveLoadParms::SetDefaults(int newInputDevice) {
  Init();
  inputDeviceId = newInputDevice;
  userId = 0;
}

idSaveGameThread::~idSaveGameThread() = default;

idSaveGameProcessor::idSaveGameProcessor() = default;

idSaveGameProcessor::~idSaveGameProcessor() {
  parms.Init();
  for (int i = 0; i < completedCallbacks.Num(); ++i) delete completedCallbacks[i];
  completedCallbacks.ClearFree();
}

const char* idSaveGameProcessor::Name() const { return "idSaveGameProcessor"; }

bool idSaveGameProcessor::Init() {
  if (working) {
    idLib::Warning("[%s] Someone is trying to execute this processor twice, this is really bad!", Name());
    return false;
  }
  parms.cancelled = false;
  parms.SetDefaults(-1);
  savegameLogicTestIterator = 0;
  init = true;
  working = false;
  for (int i = 0; i < completedCallbacks.Num(); ++i) delete completedCallbacks[i];
  completedCallbacks.ClearFree();
  return true;
}

void idSaveGameProcessor::AddCompletedCallback(const idCallback& callback) {
  completedCallbacks.Append(callback.Clone());
}

idSaveGameManager::idSaveGameManager() {
  processorQueue.granularity = 4;
  enumeratedSaveGames.granularity = MAX_SAVEGAMES;
  SetActiveManager(this);
}

idSaveGameManager::~idSaveGameManager() {
  if (ActiveManager() == this) SetActiveManager(nullptr);
  saveThread.StopThread(true);
  processor = nullptr;
}

idSaveGameManager* idSaveGameManager::ActiveManager() { return activeSaveGameManager; }
void idSaveGameManager::SetActiveManager(idSaveGameManager* manager) { activeSaveGameManager = manager; }

saveGameHandle_t idSaveGameManager::ExecuteProcessor(idSaveGameProcessor* value) {
  if (value == nullptr) return 0;
  value->working = true;
  if (processor == value) return value->parms.handle;
  for (int i = 0; i < processorQueue.Num(); ++i)
    if (processorQueue[i] == value) return value->parms.handle;
  processorQueue.Append(value);
  value->parms.handle = ++submittedProcessorHandle;
  return value->parms.handle;
}

void idSaveGameManager::StartNextProcessor() {
  if (cancel || processorQueue.Num() == 0) return;
  int selected = 0;
  for (int i = 0; i < processorQueue.Num(); ++i) {
    if (processorQueue[i]->IsSimpleProcessor()) { selected = i; break; }
  }
  idSaveGameProcessor* next = processorQueue[selected];
  ++executingProcessorHandle;
  for (int i = selected + 1; i < processorQueue.Num(); ++i) processorQueue[i - 1] = processorQueue[i];
  processorQueue.num--;
  processor = next;
  processor->parms.callbackSignal.Raise();
  startTime = static_cast<int>(Sys_Milliseconds());
}

void idSaveGameManager::FinishProcessor(idSaveGameProcessor* localProcessor) {
  for (int i = localProcessor->parms.files.Num() - 1; i >= 0; --i) {
    idFile_SaveGame* file = localProcessor->parms.files[i];
    if (file != nullptr && (file->type & SAVEGAMEFILE_AUTO_DELETE) != 0) {
      delete file;
      for (int j = i + 1; j < localProcessor->parms.files.Num(); ++j)
        localProcessor->parms.files[j - 1] = localProcessor->parms.files[j];
      localProcessor->parms.files.num--;
    }
  }
  localProcessor->init = false;
}

void idSaveGameManager::Pump() {
  if (processor == nullptr) {
    if (delayStart > 0) --delayStart;
    else StartNextProcessor();
    if (processor == nullptr) return;
    continueProcessing = true;
  }
  if (!processor->parms.callbackSignal.Wait(0)) return;

  if (!cancel && continueProcessing) {
    if (processor->parms.errorCode != SAVEGAME_E_NONE && !processor->ValidateLastError()) {
      continueProcessing = false;
    } else {
      continueProcessing = processor->Process();
      return;
    }
  }
  if (continueProcessing) return;

  for (int i = 0; i < processor->parms.detailList.Num(); ++i)
    if (processor->parms.detailList[i].damaged) processor->parms.detailList[i].ClearDescriptors();
  processor->working = false;
  idSaveGameProcessor* completed = processor;
  processor = nullptr;
  ++lastExecutedProcessorHandle;
  for (int i = 0; i < completed->completedCallbacks.Num(); ++i) completed->completedCallbacks[i]->Call();
  for (int i = 0; i < completed->completedCallbacks.Num(); ++i) delete completed->completedCallbacks[i];
  completed->completedCallbacks.ClearFree();
  FinishProcessor(completed);
}

saveGameHandle_t idSaveGameManager::ExecuteProcessorAndWait(idSaveGameProcessor* value) {
  const int handle = ExecuteProcessor(value);
  while (handle != 0 && handle > lastExecutedProcessorHandle) {
    Pump();
    Sys_Sleep(10);
  }
  return handle;
}

void idSaveGameManager::WaitForAllProcessors(bool overrideSimpleProcessorCheck) {
  while (GotWork()) {
    idSaveGameProcessor* next = processor != nullptr ? processor : processorQueue[0];
    if (!overrideSimpleProcessorCheck && !next->IsSimpleProcessor()) break;
    saveThread.WaitForThread();
    Pump();
  }
}

void idSaveGameManager::CancelWithHandle(int* handle) {
  if (handle == nullptr || *handle == 0 || *handle <= lastExecutedProcessorHandle) return;
  if (processor != nullptr && processor->parms.handle == *handle) {
    processor->parms.cancelled = true;
    processor->parms.errorCode = SAVEGAME_E_CANCELLED;
    return;
  }
  for (int i = 0; i < processorQueue.Num(); ++i) {
    if (processorQueue[i]->parms.handle == *handle) {
      processorQueue[i]->parms.cancelled = true;
      processorQueue[i]->parms.errorCode = SAVEGAME_E_CANCELLED;
      return;
    }
  }
}

void idSaveGameManager::CancelAllProcessors(bool forceCancelInFlightProcessor) {
  cancel = true;
  if (forceCancelInFlightProcessor && processor != nullptr) processor->parms.callbackSignal.Raise();
  Pump();
  processorQueue.ClearFree();
  cancel = false;
}

bool idSaveGameManager::SaveIconVisible() const {
  if (processor != nullptr || processorQueue.Num() != 0) return true;
  return static_cast<float>(startTime) + saveGame_minSaveTime.GetFloat() > Sys_Milliseconds();
}

bool idSaveGameManager::GotWork() const { return processor != nullptr || processorQueue.Num() != 0; }

void Sys_ExecuteSavegameCommandAsync(idSaveLoadParms* savegameParms) {
  if (savegameParms == nullptr) {
    idLib::Error("Programming Error with [%s]", "Sys_ExecuteSavegameCommandAsync");
    return;
  }
  if (saveGame_enable.GetBool()) {
    Sys_ExecuteSavegameCommandAsyncImpl(savegameParms);
  } else {
    idLib::Warning("Savegames are disabled (saveGame_enable = 0). Skipping physical save to media.");
    savegameParms->errorCode = SAVEGAME_E_CANCELLED;
    savegameParms->callbackSignal.Raise();
  }
}
