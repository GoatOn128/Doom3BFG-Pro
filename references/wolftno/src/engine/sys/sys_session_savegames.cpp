#include "engine/sys/sys_session_savegames.h"

#include "shared/idlib/csystems/cvarsystemlocal.h"
#include "shared/idlib/lib.h"

idCVar savegame_levelTransitionTimeoutMs(
    "savegame_levelTransitionTimeoutMs", "30000", CVAR_INTEGER,
    "(ms) timeout value for nextMap testing");
idCVar savegame_saveTimeoutMs("savegame_saveTimeoutMs", "60000", CVAR_INTEGER,
                              "(ms) timeout value for save testing");
idCVar savegame_error(
    "savegame_error", "0", CVAR_INTEGER,
    "Combination of bits that will simulate and error, see 'savegamePrintErrors'.  0 = no error");
idCVar sys_streamingInstallDebugMapNotInstalled(
    "sys_streamingInstallDebugMapNotInstalled", "", CVAR_ALL, "pretend map isn't installed");
idCVar sys_streamingInstallFirstChunkIdThatNeedInitialInFastStorage(
    "sys_streamingInstallFirstChunkIdThatNeedInitialInFastStorage", "6", CVAR_INTEGER,
    "The lowest chunkId that need initial in fast storage.");

idStr idSaveGameDetails::GetMapName() const {
  return idStr(descriptors.GetString("mapName", ""));
}

bool idSaveGameProcessorLoadFiles::InitLoadFiles(const char* folder,
                                                 const saveFileEntryList_t& inputFiles,
                                                 idSaveGameManager::packageType_t type) {
  if (!idSaveGameProcessor::Init()) return false;
  parms.SetDefaults(-1);
  parms.directory = AddSaveFolderPrefix(folder, type);
  parms.description.slotName = folder != nullptr ? folder : "";
  parms.mode = SAVEGAME_MBF_LOAD;
  for (int i = 0; i < inputFiles.Num(); ++i) parms.files.Append(inputFiles[i]);
  return true;
}

bool idSaveGameProcessorLoadFiles::Process() {
  Sys_ExecuteSavegameCommandAsync(&parms);
  return false;
}

bool idSaveGameProcessorDelete::InitDelete(const char* folder, idSaveGameManager::packageType_t type) {
  if (!idSaveGameProcessor::Init()) return false;
  parms.SetDefaults(-1);
  parms.description.slotName = folder != nullptr ? folder : "";
  parms.directory = AddSaveFolderPrefix(folder, type);
  parms.mode = SAVEGAME_MBF_DELETE_FOLDER;
  return true;
}

bool idSaveGameProcessorDelete::Process() {
  Sys_ExecuteSavegameCommandAsync(&parms);
  return false;
}

bool idSaveGameProcessorSaveFiles::InitSave(const char* folder,
                                            const saveFileEntryList_t& inputFiles,
                                            const idSaveGameDetails& details,
                                            idSaveGameManager::packageType_t type,
                                            bool clearExisting, bool resetOwner) {
  if (!idSaveGameProcessor::Init()) return false;
  if (inputFiles.Num() == 0) {
    idLib::Warning("No files to save.");
    return false;
  }
  parms.directory = AddSaveFolderPrefix(folder, type);
  parms.mode = SAVEGAME_MBF_SAVE;
  if (clearExisting) parms.mode |= SAVEGAME_MBF_CLEAR_EXISTING;
  if (resetOwner) parms.mode |= SAVEGAME_MBF_RESET_OWNER;
  for (int i = 0; i < inputFiles.Num(); ++i) parms.files.Append(inputFiles[i]);
  parms.SetDescription(details);
  parms.description.slotName = folder != nullptr ? folder : "";
  return true;
}

bool idSaveGameProcessorSaveFiles::Process() {
  Sys_ExecuteSavegameCommandAsync(&parms);
  return false;
}

bool idSaveGameProcessorEnumerateGames::Process() {
  parms.mode = SAVEGAME_MBF_ENUMERATE | SAVEGAME_MBF_READ_DETAILS;
  Sys_ExecuteSavegameCommandAsync(&parms);
  return false;
}
