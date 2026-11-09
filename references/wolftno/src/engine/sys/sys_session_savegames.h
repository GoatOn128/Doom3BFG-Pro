#pragma once

#include "engine/sys/sys_savegame.h"

class idSaveGameProcessorLoadFiles : public idSaveGameProcessor {
public:
  const char* Name() const override { return "idSaveGameProcessorLoadFiles"; }
  bool InitLoadFiles(const char* folder, const saveFileEntryList_t& files,
                     idSaveGameManager::packageType_t type = idSaveGameManager::PACKAGE_GAME);
  bool Process() override;
};

class idSaveGameProcessorDelete : public idSaveGameProcessor {
public:
  const char* Name() const override { return "idSaveGameProcessorDelete"; }
  bool InitDelete(const char* folder,
                  idSaveGameManager::packageType_t type = idSaveGameManager::PACKAGE_GAME);
  bool Process() override;
};

class idSaveGameProcessorSaveFiles : public idSaveGameProcessor {
public:
  const char* Name() const override { return "idSaveGameProcessorSaveFiles"; }
  bool InitSave(const char* folder, const saveFileEntryList_t& files,
                const idSaveGameDetails& description,
                idSaveGameManager::packageType_t type = idSaveGameManager::PACKAGE_GAME,
                bool clearExisting = false, bool resetOwner = false);
  bool Process() override;
};

class idSaveGameProcessorEnumerateGames : public idSaveGameProcessor {
public:
  const char* Name() const override { return "idSaveGameProcessorEnumerateGames"; }
  bool Process() override;
};
