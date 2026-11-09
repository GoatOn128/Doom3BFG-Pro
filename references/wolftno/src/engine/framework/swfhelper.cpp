#include "engine/framework/swfhelper.h"

#include "engine/framework/common.h"
#include "engine/framework/entity_presentable.h"
#include "engine/framework/file_savegame.h"
#include "engine/framework/game_local.h"
#include "engine/framework/gamespawninfo.h"
#include "engine/framework/gamesystem.h"
#include "engine/framework/mainmenu.h"
#include "engine/framework/playerprofile.h"
#include "engine/framework/savegame_swf.h"
#include "engine/framework/session.h"
#include "engine/guis/swf/swf.h"
#include "engine/guis/swf/swf_spriteinstance.h"
#include "engine/renderer/declmaterial.h"

namespace idSysSavegame {
constexpr const char* AUTOSAVE_FOLDER = "AUTOSAVE";
}

namespace {
idSWFScriptVar Undefined() { return idSWFScriptVar(); }

class idSWFScriptFunction_GetProfileCorruptOverwrite final
    : public idSWFScriptFunction_RefCounted {
public:
  idSWFScriptFunction_GetProfileCorruptOverwrite(
      idPlayerProfile* profile_,idProfileMgr* profileMgr_)
      : profile(profile_),profileMgr(profileMgr_) {}

  idSWFScriptVar Call(idSWFScriptObject*,idSWFParmList&) override {
    common->ClearDialog(GDM_CORRUPT_PROFILE,nullptr,0);
    profile->PrepareForNewUser();
    profile->SaveSettings(true);
    return Undefined();
  }

  idPlayerProfile* profile;
  idProfileMgr* profileMgr;
};

class idSWFScriptFunction_GetProfileCorruptAccept final
    : public idSWFScriptFunction_RefCounted {
public:
  explicit idSWFScriptFunction_GetProfileCorruptAccept(
      idPlayerProfileGame* profileGame_) : profileGame(profileGame_) {}

  idSWFScriptVar Call(idSWFScriptObject*,idSWFParmList&) override {
    idPlayerProfile* const profile =
        reinterpret_cast<idPlayerProfile*>(profileGame);
    common->ClearDialog(GDM_CORRUPT_PROFILE,nullptr,0);
    profile->PrepareForNewUser();
    profile->SaveSettings(true);
    return Undefined();
  }

  idPlayerProfileGame* profileGame;
};

class idSWFScriptFunction_GetProfileCorruptCancel final
    : public idSWFScriptFunction_RefCounted {
public:
  explicit idSWFScriptFunction_GetProfileCorruptCancel(
      idPlayerProfileGame* profileGame_) : profileGame(profileGame_) {}

  idSWFScriptVar Call(idSWFScriptObject*,idSWFParmList&) override {
    common->ClearDialog(GDM_CORRUPT_PROFILE,nullptr,0);
    reinterpret_cast<idPlayerProfile*>(profileGame)->LoadSettings();
    return Undefined();
  }

  idPlayerProfileGame* profileGame;
};

class idSWFScriptFunction_OverwriteSaveCancel final
    : public idSWFScriptFunction_RefCounted {
public:
  idSWFScriptVar Call(idSWFScriptObject*,idSWFParmList&) override {
    common->ClearDialog(GDM_ERROR_LOADING_SAVEGAME,nullptr,0);
    if (gameLocal != nullptr) {
      const idGameSpawnInfo* const spawnInfo = gameLocal->GetGameSpawnInfo();
      idFile_SaveGame* const file = gameLocal->CheckpointGetSaveFile();
      gameLocal->CheckpointProcessRead(file,spawnInfo->currentCheckpointSlot,
                                       spawnInfo->mapName.data);
    }
    return Undefined();
  }
};

class idSWFScriptFunction_GoToStoreYes final
    : public idSWFScriptFunction_RefCounted {
public:
  idSWFScriptVar Call(idSWFScriptObject*,idSWFParmList&) override {
    common->ClearDialog(GDM_NEW_CONTENT_GO_TO_STORE,nullptr,0);
    if (session != nullptr)
      session->SetGoToStoreState(idSession::GO_TO_STORE_YES);
    return Undefined();
  }
};

class idSWFScriptFunction_GoToStoreNo final
    : public idSWFScriptFunction_RefCounted {
public:
  idSWFScriptVar Call(idSWFScriptObject*,idSWFParmList&) override {
    bool active = false;
    common->ClearDialog(GDM_NEW_CONTENT_GO_TO_STORE,nullptr,0);
    common->ClearDialog(GDM_SEASON_PASS_CONTENT_AVAILABLE,nullptr,0);
    if (common->HasDialogMsg(GDM_BOUGHT_CONTENT_NOT_FOUND,&active) && active)
      common->ClearDialog(GDM_BOUGHT_CONTENT_NOT_FOUND,nullptr,0);
    else
      common->Game()->Shell_ShowNewContentAvailable();
    if (session != nullptr)
      session->SetGoToStoreState(idSession::GO_TO_STORE_NO);
    return Undefined();
  }
};

class idSWFScriptFunction_TryAgain final
    : public idSWFScriptFunction_RefCounted {
public:
  idSWFScriptFunction_TryAgain(idSaveGameManager* manager_,
                               idSaveGameProcessor* processor_)
      : manager(manager_),processor(processor_) {}

  idSWFScriptVar Call(idSWFScriptObject*,idSWFParmList&) override {
    common->ClearDialog(GDM_ERROR_SAVING_SAVEGAME,nullptr,0);
    manager->ExecuteProcessor(processor);
    return Undefined();
  }

  idSaveGameManager* manager;
  idSaveGameProcessor* processor;
};

class idSWFScriptFunction_Continue final
    : public idSWFScriptFunction_RefCounted {
public:
  explicit idSWFScriptFunction_Continue(gameDialogMessages_t msg_) : msg(msg_) {}

  idSWFScriptVar Call(idSWFScriptObject*,idSWFParmList&) override {
    common->ClearDialog(msg,nullptr,0);
    return Undefined();
  }

  gameDialogMessages_t msg;
};

class idSWFScriptFunciton_SetTexture final
    : public idSWFScriptFunction_RefCounted {
public:
  explicit idSWFScriptFunciton_SetTexture(idSWF* gui_) : gui(gui_) {}

  idSWFScriptVar Call(idSWFScriptObject*,idSWFParmList& parms) override {
    if (parms.Num() <= 3)
      return Undefined();

    const idStr matName = parms[0].ToString();
    const idStr rootSprite = parms[2].ToString();
    if (matName.Length() <= 0 || gui == nullptr)
      return Undefined();

    idSWFScriptVar root = gui->GetGlobal(rootSprite.data);
    if (!root.IsObject())
      return Undefined();

    idDecl* const decl = idMaterial::resourceList.FindWithInheritance(
        matName.data,true,true);
    if (decl == nullptr)
      return Undefined();

    const idStr childName = parms[1].ToString();
    idSWFScriptVar childObj = root.GetObject()->Get(childName.data);
    idSWFSpriteInstance* const sprite = childObj.GetSprite();
    if (sprite != nullptr) {
      sprite->materialOverride = static_cast<idMaterial*>(decl);
      if (parms[3].ToBool()) {
        sprite->materialWidth = static_cast<std::uint16_t>(
            sprite->materialOverride->GetWidth());
        sprite->materialHeight = static_cast<std::uint16_t>(
            sprite->materialOverride->GetHeight());
      }
      sprite->SetVisible(true);
    }
    return Undefined();
  }

  idSWF* gui;
};

class idSWFScriptFunction_OverwriteSaveAccept final
    : public idSWFScriptFunction_RefCounted {
public:
  idSWFScriptVar Call(idSWFScriptObject*,idSWFParmList&) override {
    common->ClearDialog(GDM_ERROR_LOADING_SAVEGAME,nullptr,0);
    idSaveGameDetails gameDetails;
    if (gameLocal == nullptr)
      return Undefined();

    gameLocal->GetSaveGameDetails(gameDetails,nullptr);
    idStaticList<idFile_SaveGame*,10> files;
    files.Append(gameLocal->CheckpointGetSaveFile());
    session->SaveGameSync(idSysSavegame::AUTOSAVE_FOLDER,files,gameDetails,
                          true,false);
    return Undefined();
  }
};

static_assert(sizeof(idSWFScriptFunciton_SetTexture)==120,
              "set-texture callback ABI drift");
static_assert(sizeof(idSWFScriptFunction_GetProfileCorruptOverwrite)==128,
              "profile-overwrite callback ABI drift");
static_assert(sizeof(idSWFScriptFunction_GetProfileCorruptAccept)==120,
              "profile-accept callback ABI drift");
static_assert(sizeof(idSWFScriptFunction_GetProfileCorruptCancel)==120,
              "profile-cancel callback ABI drift");
static_assert(sizeof(idSWFScriptFunction_TryAgain)==128,
              "retry callback ABI drift");
static_assert(sizeof(idSWFScriptFunction_Continue)==120,
              "continue callback ABI drift");
static_assert(sizeof(idSWFScriptFunction_OverwriteSaveAccept)==112,
              "save-accept callback ABI drift");
static_assert(sizeof(idSWFScriptFunction_OverwriteSaveCancel)==112,
              "save-cancel callback ABI drift");
} // namespace

idPresentable* idEntity::GetPresentable() {
  if (presentable == nullptr)
    InitPresentableInternal();
  return presentable;
}

void idSWFHelper::StorageDeviceLoaded() {
  if (gameLocal != nullptr)
    gameSystem->SetMemoryFiles();
}

void idSWFHelper::Init(idMainMenu* mainMenu_) {
  if (mainMenu_ != nullptr) {
    mainMenu = mainMenu_;
    activeSwf = mainMenu_->gui;
  }
}

idSWFScriptFunction* idSWFHelper::GetSetTextureFromSwf(idSWF* gui) {
  return new idSWFScriptFunciton_SetTexture(gui);
}

idSWFScriptFunction* idSWFHelper::GetProfileCorruptOverwrite(
    idPlayerProfile* playerProfile_,idProfileMgr* profileMgr_) {
  return new idSWFScriptFunction_GetProfileCorruptOverwrite(
      playerProfile_,profileMgr_);
}

idSWFScriptFunction* idSWFHelper::GetProfileCorruptAccept(
    idPlayerProfileGame* profileGame_) {
  return new idSWFScriptFunction_GetProfileCorruptAccept(profileGame_);
}

idSWFScriptFunction* idSWFHelper::GetProfileCorruptCancel(
    idPlayerProfileGame* profileGame_) {
  return new idSWFScriptFunction_GetProfileCorruptCancel(profileGame_);
}

idSWFScriptFunction* idSWFHelper::GetOverwriteSaveAccept() {
  return new idSWFScriptFunction_OverwriteSaveAccept;
}

idSWFScriptFunction* idSWFHelper::GetOverwriteSaveCancel() {
  return new idSWFScriptFunction_OverwriteSaveCancel;
}

idSWFScriptFunction* idSWFHelper::GetGoToStoreYes() {
  return new idSWFScriptFunction_GoToStoreYes;
}

idSWFScriptFunction* idSWFHelper::GetGoToStoreNo() {
  return new idSWFScriptFunction_GoToStoreNo;
}

idSWFScriptFunction* idSWFHelper::GetTryAgainProcTimeout(
    idSaveGameManager* manager,idSaveGameProcessor* processor) {
  return new idSWFScriptFunction_TryAgain(manager,processor);
}

idSWFScriptFunction* idSWFHelper::GetContinue(gameDialogMessages_t msg_) {
  return new idSWFScriptFunction_Continue(msg_);
}
