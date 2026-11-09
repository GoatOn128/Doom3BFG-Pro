#pragma once

#include "engine/framework/file_savegame.h"
#include "engine/sys/sys_lobby_types.h"
#include "shared/idlib/containers/list.h"
#include "shared/idlib/dict.h"
#include "shared/idlib/parallelism/thread.h"

#include <cstdint>

constexpr int MAX_SAVEGAMES = 16;
constexpr int MAX_FILES_WITHIN_SAVEGAME = 10;
constexpr int MAX_FOLDER_NAME_LENGTH = 64;
constexpr int MAX_FILENAME_LENGTH_PATTERN = 8;
constexpr const char* SAVEGAME_DETAILS_FILENAME = "game.details";
constexpr const char* SAVEGAME_AUTOSAVE_FOLDER = "AUTOSAVE";
constexpr const char* SAVEGAME_DETAIL_FIELD_CHECKSUM = "checksum";
constexpr const char* SAVEGAME_GAME_DIRECTORY_PREFIX = "GAME-";
constexpr const char* SAVEGAME_PROFILE_DIRECTORY_PREFIX = "";
constexpr const char* SAVEGAME_RAW_DIRECTORY_PREFIX = "";

enum saveGameError_t {
  SAVEGAME_E_NONE = 0,
  SAVEGAME_E_CANCELLED = 1 << 0,
  SAVEGAME_E_INSUFFICIENT_ROOM = 1 << 1,
  SAVEGAME_E_CORRUPTED = 1 << 2,
  SAVEGAME_E_UNABLE_TO_SELECT_STORAGE_DEVICE = 1 << 3,
  SAVEGAME_E_UNKNOWN = 1 << 4,
  SAVEGAME_E_INVALID_FILENAME = 1 << 5,
  SAVEGAME_E_STEAM_ERROR = 1 << 6,
  SAVEGAME_E_FOLDER_NOT_FOUND = 1 << 7,
  SAVEGAME_E_FILE_NOT_FOUND = 1 << 8,
  SAVEGAME_E_DLC_NOT_FOUND = 1 << 9,
  SAVEGAME_E_INVALID_USER = 1 << 10,
  SAVEGAME_E_PROFILE_TOO_BIG = 1 << 11,
  SAVEGAME_E_DISC_SWAP = 1 << 12,
  SAVEGAME_E_INCOMPATIBLE_NEWER_VERSION = 1 << 13,
  SAVEGAME_E_BITS_USED = 14
};

enum saveGameModeBitfield_t {
  SAVEGAME_MBF_NONE = 0,
  SAVEGAME_MBF_LOAD = 1 << 0,
  SAVEGAME_MBF_SAVE = 1 << 1,
  SAVEGAME_MBF_DELETE_FOLDER = 1 << 2,
  SAVEGAME_MBF_DELETE_ALL_FOLDERS = 1 << 3,
  SAVEGAME_MBF_ENUMERATE = 1 << 4,
  SAVEGAME_MBF_NO_COMPRESS = 1 << 5,
  SAVEGAME_MBF_ENUMERATE_FILES = 1 << 6,
  SAVEGAME_MBF_DELETE_FILES = 1 << 7,
  SAVEGAME_MBF_READ_DETAILS = 1 << 8,
  SAVEGAME_MBF_KEEP_FOLDER = 1 << 9,
  SAVEGAME_MBF_CLEAR_EXISTING = 1 << 10,
  SAVEGAME_MBF_DELETE_CONSOLE = 1 << 11,
  SAVEGAME_MBF_RESET_OWNER = 1 << 12
};

using saveGameHandle_t = int;
using savegameUserId_t = int;

class idSaveGameDetails {
public:
  idSaveGameDetails();
  void Clear();
  void ClearDescriptors();
  void SetDescriptorString(const char* key, const char* value);
  void SetDescriptorInt(const char* key, int value);
  void WriteDescriptorsToIniFile(idFile_SaveGame* file) const;
  idStr GetMapName() const;
  const char* GetSlotName() const { return slotName.data; }

  bool operator==(const idSaveGameDetails& other) const;
  bool operator<(const idSaveGameDetails& other) const;

  idDict descriptors;
  bool damaged{};
  std::int64_t date{};
  idStrStatic<MAX_FOLDER_NAME_LENGTH> slotName;
};

using saveFileEntryList_t = idList<idFile_SaveGame*>;
using saveGameDetailsList_t = idList<idSaveGameDetails>;

class idSaveLoadParms {
public:
  idSaveLoadParms();
  ~idSaveLoadParms();

  void Init();
  void SetDefaults(int newInputDevice = -1);
  void SetDescription(const idSaveGameDetails& value) { description = value; }
  const saveGameHandle_t& GetHandle() const { return handle; }

  idStrStatic<MAX_FOLDER_NAME_LENGTH> directory;
  idStrStatic<MAX_FILENAME_LENGTH_PATTERN> pattern;
  idStrStatic<MAX_FILENAME_LENGTH_PATTERN> postPattern;
  int mode{};
  idSaveGameDetails description;
  saveFileEntryList_t files;
  saveGameDetailsList_t detailList;
  int errorCode{};
  int handledErrorCodes{};
  std::int64_t requiredSpaceInBytes{};
  int skipErrorDialogMask{};
  idSysSignal callbackSignal;
  volatile bool cancelled{};
  savegameUserId_t userId{};
  int inputDeviceId{-1};
  saveGameHandle_t handle{};
};

struct saveGameThreadArgs_t {
  idSaveLoadParms* saveLoadParms{};
};

class idSaveGameThread : public idSysThread {
public:
  ~idSaveGameThread() override;
  int Run() override;
  void CancelOperations() { cancel = true; }

  saveGameThreadArgs_t data;
  volatile bool cancel{};

private:
  int Save();
  int Load();
  int Enumerate();
  int Delete();
  int DeleteAll();
  int DeleteFiles();
  int EnumerateFiles();
};

class idSaveGameProcessor {
  friend class idSaveGameManager;
public:
  idSaveGameProcessor();
  virtual ~idSaveGameProcessor();
  virtual const char* Name() const;
  virtual bool Init();
  virtual bool Process() { return false; }
  virtual bool ValidateLastError() { return false; }
  virtual bool IsSimpleProcessor() const { return true; }
  virtual bool ShouldTimeout() const { return false; }

  void AddCompletedCallback(const idCallback& callback);
  bool IsWorking() const { return working; }
  int GetError() const { return parms.errorCode; }
  saveGameHandle_t GetHandle() const { return parms.handle; }
  idSaveLoadParms& GetParmsNonConst() { return parms; }
  const idSaveLoadParms& GetParms() const { return parms; }

protected:
  idSaveLoadParms parms;
  int savegameLogicTestIterator{};

private:
  bool init{};
  bool working{};
  idList<idCallback*> completedCallbacks;
};

class idSaveGameManager {
public:
  enum packageType_t { PACKAGE_PROFILE, PACKAGE_GAME, PACKAGE_RAW, PACKAGE_NUM };

  idSaveGameManager();
  ~idSaveGameManager();

  saveGameHandle_t ExecuteProcessor(idSaveGameProcessor* processor);
  saveGameHandle_t ExecuteProcessorAndWait(idSaveGameProcessor* processor);
  void Pump();
  void WaitForAllProcessors(bool overrideSimpleProcessorCheck = false);
  void CancelWithHandle(int* handle);
  void CancelWithHandle(const int& handle) { int mutableHandle = handle; CancelWithHandle(&mutableHandle); }
  void CancelAllProcessors(bool forceCancelInFlightProcessor);
  bool SaveIconVisible() const;
  bool GotWork() const;
  bool IsWorking() const { return GotWork(); }
  idSaveGameThread& GetSaveGameThread() { return saveThread; }
  int LastExecutedProcessorHandle() const { return lastExecutedProcessorHandle; }

  static idSaveGameManager* ActiveManager();
  static void SetActiveManager(idSaveGameManager* manager);

private:
  void StartNextProcessor();
  void FinishProcessor(idSaveGameProcessor* localProcessor);

  idSaveGameProcessor* processor{};
  idList<idSaveGameProcessor*> processorQueue;
  bool cancel{};
  idSaveGameThread saveThread;
  int delayStart{};
  int startTime{};
  bool continueProcessing{};
  int submittedProcessorHandle{};
  int executingProcessorHandle{};
  int lastExecutedProcessorHandle{};
  saveGameDetailsList_t enumeratedSaveGames;
  bool storageAvailable{true};
};

idStr GetSaveGameErrorString(int errorMask);
const idStr& GetSaveFolder(idSaveGameManager::packageType_t type);
idStr AddSaveFolderPrefix(const char* folder, idSaveGameManager::packageType_t type);
idStr RemoveSaveFolderPrefix(const char* folder, idSaveGameManager::packageType_t type);
bool SavegameReadDetailsFromFile(idFile* file, idSaveGameDetails* details);
inline bool SavegameReadDetailsFromFile(idFile* file, idSaveGameDetails& details) {
  return SavegameReadDetailsFromFile(file, &details);
}
void Sys_ExecuteSavegameCommandAsync(idSaveLoadParms* savegameParms);
void Sys_ExecuteSavegameCommandAsyncImpl(idSaveLoadParms* savegameParms);
void Sys_SaveGameCheck(bool& exists, bool& autosaveExists);
bool Sys_SaveGameProfileCheck();
