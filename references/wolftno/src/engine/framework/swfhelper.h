#pragma once

#include "engine/framework/dialog.h"

class idMainMenu;
class idPlayerProfile;
class idPlayerProfileGame;
class idProfileMgr;
class idSaveGameManager;
class idSaveGameProcessor;
class idSWF;
class idSWFScriptFunction;

class idSWFHelper {
public:
  idSWFHelper() : activeSwf(nullptr), mainMenu(nullptr) {}

  void Init(idMainMenu* mainMenu_);
  void BindSwf(idSWF* swf) { activeSwf = swf; }
  void StorageDeviceLoaded();

  idSWFScriptFunction* GetSetTextureFromSwf(idSWF* gui);
  idSWFScriptFunction* GetProfileCorruptOverwrite(
      idPlayerProfile* playerProfile_, idProfileMgr* profileMgr_);
  idSWFScriptFunction* GetProfileCorruptAccept(
      idPlayerProfileGame* profileGame_);
  idSWFScriptFunction* GetProfileCorruptCancel(
      idPlayerProfileGame* profileGame_);
  idSWFScriptFunction* GetOverwriteSaveAccept();
  idSWFScriptFunction* GetOverwriteSaveCancel();
  idSWFScriptFunction* GetGoToStoreYes();
  idSWFScriptFunction* GetGoToStoreNo();
  idSWFScriptFunction* GetTryAgainProcTimeout(
      idSaveGameManager* manager, idSaveGameProcessor* processor);
  idSWFScriptFunction* GetContinue(gameDialogMessages_t msg_);

  idSWF* activeSwf;
  idMainMenu* mainMenu;
};

static_assert(sizeof(idSWFHelper) == 16, "SWF-helper ABI drift");
