#include "engine/sys/sys_savegame.h"

#include "engine/sys/sys_signin.h"
#include "engine/sys/win32/win_localuser.h"
#include "shared/idlib/csystems/cvarsystemlocal.h"
#include "shared/idlib/hashing/md.h"
#include "shared/idlib/sys/sys_time.h"

#include <algorithm>
#include <chrono>
#include <filesystem>
#include <fstream>
#include <vector>

extern idCVar saveGame_verbose;
extern idCVar saveGame_enable;
extern idCVar savegame_error;

idCVar savegame_winInduceDelay(
    "savegame_winInduceDelay", "0", CVAR_INTEGER,
    "on windows, this is a delay induced before any file operation occurs");
extern idCVar saveGame_checksum;

namespace {
namespace fs = std::filesystem;

fs::path SaveRoot() { return fs::current_path() / "savegame"; }

fs::path SaveDirectory(const idSaveLoadParms& parms) {
  return SaveRoot() / fs::path(parms.directory.data);
}

bool MatchFileName(const fs::path& path, const idSaveLoadParms& parms) {
  const std::string name = path.filename().string();
  const std::string prefix = parms.pattern.data;
  const std::string suffix = parms.postPattern.data;
  if (!prefix.empty() && prefix != "*" && name.rfind(prefix, 0) != 0) return false;
  if (!suffix.empty() && suffix != "*" &&
      (name.size() < suffix.size() || name.compare(name.size() - suffix.size(), suffix.size(), suffix) != 0))
    return false;
  return true;
}

std::uint32_t ByteSwap32(std::uint32_t value) {
  return ((value & 0x000000ffU) << 24U) | ((value & 0x0000ff00U) << 8U) |
         ((value & 0x00ff0000U) >> 8U) | ((value & 0xff000000U) >> 24U);
}

std::int64_t FileDate(const fs::path& path) {
  std::error_code error;
  const auto stamp = fs::last_write_time(path, error);
  return error ? 0 : static_cast<std::int64_t>(stamp.time_since_epoch().count());
}
}

void Sys_ExecuteSavegameCommandAsyncImpl(idSaveLoadParms* savegameParms) {
  idSaveGameManager* manager = idSaveGameManager::ActiveManager();
  if (manager == nullptr) {
    savegameParms->errorCode = SAVEGAME_E_UNKNOWN;
    savegameParms->callbackSignal.Raise();
    return;
  }
  idSaveGameThread& thread = manager->GetSaveGameThread();
  thread.data.saveLoadParms = savegameParms;
  if (thread.threadHandle == 0)
    thread.StartWorkerThread("Savegame", CORE_ANY, THREAD_NORMAL, 0x40000);
  thread.SignalWork();
}

idLocalUserWin* GetLocalUserFromSaveParms(const saveGameThreadArgs_t* data) {
  if (data == nullptr || data->saveLoadParms == nullptr || data->saveLoadParms->inputDeviceId < 0)
    return nullptr;
  idSignInManagerBase* signIn = Sys_GetActiveSignInManager();
  if (signIn == nullptr) return nullptr;
  idLocalUser* user = signIn->GetLocalUserByInputDevice(data->saveLoadParms->inputDeviceId);
  if (user == nullptr || data->saveLoadParms->userId != idStr::Hash(user->GetGamerTag())) return nullptr;
  return dynamic_cast<idLocalUserWin*>(user);
}

int idSaveGameThread::Load() {
  idSaveLoadParms* parms = data.saveLoadParms;
  const fs::path folder = SaveDirectory(*parms);
  std::error_code error;
  if (!fs::is_directory(folder, error)) {
    parms->errorCode = SAVEGAME_E_FOLDER_NOT_FOUND;
    return -1;
  }
  int result = 0;
  for (int i = 0; i < parms->files.Num() && result == 0 && !parms->cancelled; ++i) {
    idFile_SaveGame* output = parms->files[i];
    if (output == nullptr) continue;
    std::ifstream stream(folder / output->GetName(), std::ios::binary);
    if (!stream) {
      output->error = true;
      if ((output->type & SAVEGAMEFILE_OPTIONAL) == 0) {
        parms->errorCode = SAVEGAME_E_FILE_NOT_FOUND;
        result = -1;
      }
      continue;
    }
    stream.seekg(0, std::ios::end);
    std::size_t length = static_cast<std::size_t>(stream.tellg());
    stream.seekg(0, std::ios::beg);
    std::uint32_t expectedChecksum = 0;
    const bool checksummed = (output->type & (SAVEGAMEFILE_BINARY | SAVEGAMEFILE_COMPRESSED)) != 0 &&
                             saveGame_checksum.GetBool() && length >= sizeof(expectedChecksum);
    if (checksummed) {
      stream.read(reinterpret_cast<char*>(&expectedChecksum), sizeof(expectedChecksum));
      expectedChecksum = ByteSwap32(expectedChecksum);
      length -= sizeof(expectedChecksum);
    }
    std::vector<std::uint8_t> bytes(length);
    if (length != 0) stream.read(reinterpret_cast<char*>(bytes.data()), static_cast<std::streamsize>(length));
    if (!stream && length != 0) {
      output->error = true;
      parms->errorCode = SAVEGAME_E_CORRUPTED;
      result = -1;
      continue;
    }
    if (checksummed && MD5_BlockChecksum(bytes.data(), bytes.size()) != expectedChecksum) {
      output->error = true;
      parms->errorCode = SAVEGAME_E_CORRUPTED;
      result = -1;
      continue;
    }
    output->Clear(true);
    output->SetLength(length);
    if (length != 0) output->Write(bytes.data(), length);
    output->Seek(0, FS_SEEK_SET);
    output->MakeReadOnly();
  }
  if (parms->cancelled) parms->errorCode = SAVEGAME_E_CANCELLED;
  if (parms->errorCode != SAVEGAME_E_NONE)
    for (int i = 0; i < parms->files.Num(); ++i) if (parms->files[i] != nullptr) parms->files[i]->error = true;
  return result;
}

int idSaveGameThread::Delete() {
  idSaveLoadParms* parms = data.saveLoadParms;
  std::error_code error;
  if (!fs::exists(SaveDirectory(*parms), error)) {
    parms->errorCode = SAVEGAME_E_FOLDER_NOT_FOUND;
    return -1;
  }
  fs::remove_all(SaveDirectory(*parms), error);
  if (error) { parms->errorCode = SAVEGAME_E_UNKNOWN; return -1; }
  return 0;
}

int idSaveGameThread::DeleteFiles() {
  idSaveLoadParms* parms = data.saveLoadParms;
  const fs::path folder = SaveDirectory(*parms);
  std::error_code error;
  if (!fs::is_directory(folder, error)) { parms->errorCode = SAVEGAME_E_FOLDER_NOT_FOUND; return -1; }
  for (const fs::directory_entry& entry : fs::directory_iterator(folder, error)) {
    if (parms->cancelled) break;
    if (entry.is_regular_file() && MatchFileName(entry.path(), *parms)) fs::remove(entry.path(), error);
    if (error) { parms->errorCode = SAVEGAME_E_UNKNOWN; return -1; }
  }
  if (parms->cancelled) { parms->errorCode = SAVEGAME_E_CANCELLED; return -1; }
  return 0;
}

int idSaveGameThread::DeleteAll() {
  idSaveLoadParms* parms = data.saveLoadParms;
  std::error_code error;
  fs::remove_all(SaveRoot(), error);
  if (error) { parms->errorCode = SAVEGAME_E_UNKNOWN; return -1; }
  return 0;
}

void Sys_SaveGameCheck(bool& exists, bool& autosaveExists) {
  exists = false;
  autosaveExists = false;
  std::error_code error;
  if (!fs::is_directory(SaveRoot(), error)) return;
  for (const fs::directory_entry& entry : fs::directory_iterator(SaveRoot(), error)) {
    if (!entry.is_directory()) continue;
    exists = true;
    const idStr slot = RemoveSaveFolderPrefix(entry.path().filename().string().c_str(),
                                               idSaveGameManager::PACKAGE_GAME);
    if (slot.Icmp(SAVEGAME_AUTOSAVE_FOLDER) == 0) autosaveExists = true;
  }
}

int idSaveGameThread::Save() {
  idSaveLoadParms* parms = data.saveLoadParms;
  if (GetLocalUserFromSaveParms(&data) == nullptr) {
    parms->errorCode = SAVEGAME_E_INVALID_USER;
    return -1;
  }
  const fs::path folder = SaveDirectory(*parms);
  std::error_code error;
  if ((parms->mode & SAVEGAME_MBF_CLEAR_EXISTING) != 0) fs::remove_all(folder, error);
  error.clear();
  fs::create_directories(folder, error);
  if (error) { parms->errorCode = SAVEGAME_E_UNKNOWN; return -1; }

  for (int i = 0; i < parms->files.Num() && !parms->cancelled; ++i) {
    idFile_SaveGame* input = parms->files[i];
    if (input == nullptr) continue;
    const fs::path destination = folder / input->GetName();
    const fs::path temporary = destination.string() + ".temp";
    std::ofstream stream(temporary, std::ios::binary | std::ios::trunc);
    if (!stream) { input->error = true; parms->errorCode = SAVEGAME_E_INSUFFICIENT_ROOM; return -1; }
    const auto* bytes = reinterpret_cast<const std::uint8_t*>(input->GetDataPtr());
    const std::size_t length = static_cast<std::size_t>(input->Length());
    if ((input->type & (SAVEGAMEFILE_BINARY | SAVEGAMEFILE_COMPRESSED)) != 0 && saveGame_checksum.GetBool()) {
      const std::uint32_t checksum = ByteSwap32(MD5_BlockChecksum(bytes, length));
      stream.write(reinterpret_cast<const char*>(&checksum), sizeof(checksum));
    }
    if (length != 0) stream.write(reinterpret_cast<const char*>(bytes), static_cast<std::streamsize>(length));
    stream.close();
    if (!stream) {
      input->error = true;
      parms->errorCode = SAVEGAME_E_INSUFFICIENT_ROOM;
      fs::remove(temporary, error);
      return -1;
    }
    fs::remove(destination, error);
    error.clear();
    fs::rename(temporary, destination, error);
    if (error) { input->error = true; parms->errorCode = SAVEGAME_E_UNKNOWN; return -1; }
  }
  if (parms->cancelled) { parms->errorCode = SAVEGAME_E_CANCELLED; return -1; }
  return 0;
}

int idSaveGameThread::EnumerateFiles() {
  idSaveLoadParms* parms = data.saveLoadParms;
  const fs::path folder = SaveDirectory(*parms);
  std::error_code error;
  if (!fs::is_directory(folder, error)) { parms->errorCode = SAVEGAME_E_FOLDER_NOT_FOUND; return -1; }
  for (const fs::directory_entry& entry : fs::directory_iterator(folder, error)) {
    if (parms->cancelled) break;
    if (!entry.is_regular_file() || !MatchFileName(entry.path(), *parms)) continue;
    parms->files.Append(new idFile_SaveGame(entry.path().filename().string().c_str(),
                                             SAVEGAMEFILE_AUTO_DELETE));
  }
  if (error) { parms->errorCode = SAVEGAME_E_UNKNOWN; return -1; }
  if (parms->cancelled) { parms->errorCode = SAVEGAME_E_CANCELLED; return -1; }
  return 0;
}

int idSaveGameThread::Enumerate() {
  idSaveLoadParms* parms = data.saveLoadParms;
  parms->detailList.ClearFree();
  std::error_code error;
  if (!fs::is_directory(SaveRoot(), error)) return 0;
  for (const fs::directory_entry& entry : fs::directory_iterator(SaveRoot(), error)) {
    if (parms->cancelled || parms->detailList.Num() >= MAX_SAVEGAMES) break;
    if (!entry.is_directory()) continue;
    idSaveGameDetails details;
    details.slotName = entry.path().filename().string().c_str();
    details.date = FileDate(entry.path());
    if ((parms->mode & SAVEGAME_MBF_READ_DETAILS) != 0) {
      std::ifstream stream(entry.path() / SAVEGAME_DETAILS_FILENAME, std::ios::binary);
      if (stream) {
        std::vector<char> bytes((std::istreambuf_iterator<char>(stream)), std::istreambuf_iterator<char>());
        idFile_Memory detailFile(SAVEGAME_DETAILS_FILENAME);
        if (!bytes.empty()) detailFile.Write(bytes.data(), bytes.size());
        detailFile.Seek(0, FS_SEEK_SET);
        SavegameReadDetailsFromFile(&detailFile, details);
      } else {
        details.damaged = true;
      }
    }
    parms->detailList.Append(details);
  }
  std::sort(parms->detailList.list, parms->detailList.list + parms->detailList.Num());
  if (error) { parms->errorCode = SAVEGAME_E_UNKNOWN; return -1; }
  if (parms->cancelled) { parms->errorCode = SAVEGAME_E_CANCELLED; return -1; }
  return 0;
}

int idSaveGameThread::Run() {
  idSaveLoadParms* parms = data.saveLoadParms;
  if (parms == nullptr) return 0;
  unsigned result = 0;
  try {
    idLocalUserWin* user = GetLocalUserFromSaveParms(&data);
    if (user != nullptr && !user->IsStorageDeviceAvailable())
      parms->errorCode = SAVEGAME_E_UNABLE_TO_SELECT_STORAGE_DEVICE;
    if (savegame_winInduceDelay.GetInteger() > 0)
      Sys_Sleep(static_cast<unsigned long>(savegame_winInduceDelay.GetInteger()));
    if ((parms->mode & SAVEGAME_MBF_SAVE) != 0) result = Save();
    else if ((parms->mode & SAVEGAME_MBF_LOAD) != 0) result = Load();
    else if ((parms->mode & SAVEGAME_MBF_ENUMERATE) != 0) result = Enumerate();
    else if ((parms->mode & SAVEGAME_MBF_DELETE_FOLDER) != 0) result = Delete();
    else if ((parms->mode & SAVEGAME_MBF_DELETE_ALL_FOLDERS) != 0) result = DeleteAll();
    else if ((parms->mode & SAVEGAME_MBF_DELETE_FILES) != 0) result = DeleteFiles();
    else if ((parms->mode & SAVEGAME_MBF_ENUMERATE_FILES) != 0) result = EnumerateFiles();
    if (result != 0 && parms->errorCode == SAVEGAME_E_NONE) parms->errorCode = SAVEGAME_E_UNKNOWN;
  } catch (...) {
    parms->errorCode = SAVEGAME_E_UNKNOWN;
    result = static_cast<unsigned>(-1);
  }
  if (savegame_error.GetInteger() != 0) parms->errorCode = savegame_error.GetInteger();
  parms->callbackSignal.Raise();
  return static_cast<int>(result);
}
