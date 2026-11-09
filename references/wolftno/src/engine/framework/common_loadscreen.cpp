#include "engine/framework/common_local.h"

#include "engine/decls/declloadingscreen.h"
#include "engine/decls/declswfresource.h"
#include "engine/framework/gamespawninfo.h"
#include "engine/framework/loadscreen.h"
#include "engine/framework/rendermanager.h"
#include "engine/framework/session.h"
#include "engine/framework/swfhelper.h"
#include "engine/framework/usercmdgen.h"
#include "engine/guis/swf/swf.h"
#include "engine/guis/swf/swf_spriteinstance.h"
#include "engine/guis/swf/swf_textinstance.h"
#include "engine/models/rendermodelgui.h"
#include "engine/renderer/declmaterial.h"
#include "engine/renderer/renderersystem.h"
#include "engine/renderer/virtualtexture.h"
#include "engine/sound/soundsystem.h"
#include "engine/sound/soundshader.h"
#include "engine/sys/sys_public.h"
#include "engine/sys/sys_savegame.h"
#include "shared/idlib/csystems/cvarsystemlocal.h"
#include "shared/idlib/lib.h"
#include "shared/idlib/sys/sys_time.h"

extern idCVar in_joystickButtons;
extern idCVar com_production;
extern idCVar feature_lowgore;
extern int com_editors;
extern idCVar joy_pitchSpeed;
extern idCVar joy_yawSpeed;
extern idCVar m_sensitivity;
extern idCVar in_invertLook;
extern idCVar com_showMapLoadStatus;
extern idCVar net_drawDebugHud;
extern idCVar r_useSMP;
extern idCVar swf_tweakSafeAreaWidth;
extern const char* pacifierStateToString[];

namespace {
class ScopedSystemHeap {
public:
  ScopedSystemHeap() { idMem::Singleton()->PushHeap(HEAP_SYSTEMHEAP); }
  ~ScopedSystemHeap() { idMem::Singleton()->PopHeap(); }
};
}

idCVar com_newLoadScreen("com_newLoadScreen","1",CVAR_BOOL,
                         "Enable new load screen code",nullptr);
idCVar com_LoadScreenDevMessage("com_LoadScreenDevMessage","0",CVAR_BOOL,
                                "Enable load screen dev message",nullptr);
idCVar demo_overrideLoadingScreen(
    "demo_overrideLoadingScreen","",0,
    "Name of the loading screen GUI to use when loading a level",nullptr);
idCVar swf_loadScreenAlternate("swf_loadScreenAlternate","0",CVAR_INTEGER,
                               "alternate loadscreen to use",nullptr);
idCVar swf_loadScreenText("swf_loadScreenText","-1",CVAR_INTEGER,
                          "alternate loadscreentext to use",nullptr);
idCVar com_spFinishFeedbackMaxTimeInSeconds(
    "com_spFinishFeedbackMaxTimeInSeconds","5",CVAR_INTEGER,"",nullptr);
idCVar swf_connectionNumber("swf_connectionNumber","01",0,"Test",nullptr);

void UpdateLevelLoadPacifier_TEMP_WILL_GO_AWAY() {
  common->UpdateLevelLoadPacifier(1.0f,static_cast<pacifierState_t>(18),false,
                                  false);
}

void idCommonLocal::UpdateLevelLoadPacifier(float fractionComplete,
                                             pacifierState_t state,
                                             bool force,bool) {
  session->GetSaveGameManager()->Pump();
  if(!idLib::IsMainThread()) return;

  const int now=static_cast<int>(Sys_Milliseconds());
  const bool updateSession=now-Field<int>(0x5AE00)>=100;
  if(updateSession) {
    Field<int>(0x5AE00)=now;
    if(!renderSystem->HasRenderFlag(RENDERFLAG_PRESSEDDURINGCUTSCENE)) {
      inputSettings_t settings{};
      settings.bindset=BIND_DEFAULT;
      settings.mouseSensScale=1.0f;
      settings.joySensScale=1.0f;
      settings.joyPitchSpeed=joy_pitchSpeed.valueFloat;
      settings.joyYawSpeed=joy_yawSpeed.valueFloat;
      settings.mouseSens=m_sensitivity.valueFloat;
      settings.absoluteAngles=false;
      settings.lookInvert=in_invertLook.valueInteger!=0;
      settings.aimAssistAngles={};
      for(int device=0;device<4;++device)
        (void)usercmdGen->GetUsercmd(device,settings,false,0);
      sys->GenerateEvents();
      for(int count=0;count<20;++count) {
        const sysEvent_t event=sys->GetEvent();
        if(event.evType!=SE_NONE &&
           event.inputDevice==common->GetMasterLocalUserInputDevice() &&
           commonLocal.CutScenePress(event,false)) {
          sys->ClearEvents();
          break;
        }
      }
    }
  }

  if(renderSystem->BinkVideoIsPlaying()) {
    session->UpdateSignInManager();
    session->Pump();
    return;
  }
  if(com_newLoadScreen.valueInteger!=0) return;

  if(updateSession) {
    soundSystem->Render(false);
    session->UpdateSignInManager();
    session->Pump();
    session->ProcessSnapAckQueue();
  }

  idSWF* const gui=Field<idSWF*>(0x5ADF0);
  idRenderModelGui* const model=Field<idRenderModelGui*>(0x5ADF8);
  if(gui!=nullptr && model!=nullptr) {
    if(Field<bool>(0x5AE0C)) {
      if(!force) return;
    } else if(!force && now-Field<int>(0x5AE04)<8000) {
      return;
    }
    Field<int>(0x5AE04)=now;
    if(now-Field<int>(0x5AE08)>=8000) {
      Field<int>(0x5AE08)=now;
      gui->SetGlobal("nextTooltip",idSWFScriptVar(true));
      if(com_showMapLoadStatus.valueInteger!=0)
        gui->SetGlobal("mapLoadText",
                       idSWFScriptVar(pacifierStateToString[static_cast<int>(state)]));
    }
    gui->SetGlobal("mapLoadPercent",idSWFScriptVar(fractionComplete*100.0f));
    const int width=renderSystem->GetWidth();
    const int height=renderSystem->GetHeight();
    model->SetViewport(0,0,width,height);
    model->SetVirtualSize(static_cast<int>(renderSystem->GetPixelAspect()*width),
                          height);
    model->Clear(0x28u);
    gui->Render(model,now,false);
    idLobbyBase& lobby=session->GetActingGameStateLobbyBase();
    if(lobby.HasActivePeers() && net_drawDebugHud.valueInteger!=0)
      session->GetActingGameStateLobbyBase().DrawDebugNetworkHUD(model);
    const bool allowSwap=!renderSystem->HasRenderFlag(RENDERFLAG_LOADCOMPLETE);
    renderManager.BuildGuiFrame(model,allowSwap);
    renderManager.RenderFrameAndBeginAutomaticBackgroundSwaps(allowSwap,true);
    if(static_cast<int>(state)==21) renderManager.RenderFrame();
  } else if(renderSystem->GetUsingSMP()) {
    renderSystem->BeginAutomaticBackgroundSwaps(
        renderSystem->GetGameWindow(),
        !renderSystem->HasRenderFlag(RENDERFLAG_LOADCOMPLETE),true);
  } else {
    renderSystem->SyncRenderThread(false);
  }
}

bool idCommonLocal::AreWeRunningPC() const {
  return in_joystickButtons.valueInteger==2;
}

millisecondUnique_t idCommonLocal::PlayLoadSound() {
  idSWF* const gui=Field<idSWF*>(0x5ADF0);
  if(gui==nullptr || soundSystem==nullptr) return {0};
  gui->AllocateEmitter(soundSystem->GetPlayingSoundWorld());
  idSWFScriptVar soundName=gui->GetGlobal("loadingSound");
  if(soundName.type>idSWFScriptVar::SWF_VAR_STRING) return {0};
  idStr name=soundName.ToString();
  const idSoundShader* shader=name.data!=nullptr
      ? static_cast<const idSoundShader*>(
            idSoundShader::resourceList.FindWithInheritance(name.data,true,true))
      : nullptr;
  return gui->PlaySound(shader,SND_CHANNEL_ANY,true);
}

const char* idCommonLocal::GetLoadMaterialForMission(const char* mission,
                                                     bool goingTo) {
  idSWF* const gui=Field<idSWF*>(0x5ADF0);
  if(gui==nullptr || gui->swfResourceMenu==nullptr) return "";
  const auto& entries=gui->swfResourceMenu->loadMissionEntries;
  for(int index=0;index<entries.Num();++index) {
    if(idStr::Cmp(mission,entries[index].mapName.str)==0) {
      const idMaterial* material=goingTo?entries[index].materialTo
                                        :entries[index].materialFrom;
      return material->name.str;
    }
  }
  return "";
}

void idCommonLocal::SetLoadScreenInfo(const idStr& varName,
                                      const idStr& value) {
  ScopedSystemHeap everythingHereGoesInTheGlobalHeap;
  auto& info=Field<idStaticList<idLoadScreenInfo,16>>(0x5B420);
  for(int index=0;index<info.Num();++index) {
    if(info[index].varName.Cmp(varName.data)==0) {
      info[index].value=value;
      return;
    }
  }
  idLoadScreenInfo* const entry=info.Alloc();
  entry->varName=varName;
  entry->value=value;
}

void idCommonLocal::LoadVirtualTextures() {
  if(renderSystem->BinkVideoIsPlaying()) return;
  idSWF* const gui=Field<idSWF*>(0x5ADF0);
  idRenderModelGui* const model=Field<idRenderModelGui*>(0x5ADF8);
  if(gui==nullptr || model==nullptr) {
    renderSystem->SyncRenderThread(false);
    return;
  }
  gui->SetGlobal("mapLoadPercent",idSWFScriptVar(100.0f));
  BeginTerminationThread();
  for(int count=0;count<5;++count) renderManager.RenderFrame();
  renderSystem->SyncRenderThread(true);
  UpdateLevelLoadPacifier(1.0f,static_cast<pacifierState_t>(17),true,false);
  const int seconds=session->GetTitleStorageInt(
      "com_spFinishFeedbackMaxTimeInSeconds",
      com_spFinishFeedbackMaxTimeInSeconds.valueInteger);
  virtualTextureSystem.FinishFeedback(false,seconds);
  EndTerminationThread();
}

void idCommonLocal::StopLevelLoad(bool loadVirtualTextures) {
  int& progress=Field<int>(0x5ADE8);
  if(progress<=0) return;
  if(com_newLoadScreen.valueInteger!=0) {
    progress=0;
  } else {
    const int previous=progress;
    --progress;
    if(previous<2) return;
  }
  if(loadVirtualTextures) LoadVirtualTextures();
  renderManager.BuildGuiFrame(nullptr,false);
  if(com_newLoadScreen.valueInteger==0) {
    idRenderModelGui*& model=Field<idRenderModelGui*>(0x5ADF8);
    if(model!=nullptr) {
      model->Clear(0x28u);
      delete model;
      model=nullptr;
    }
    idSWF*& gui=Field<idSWF*>(0x5ADF0);
    delete gui;
    gui=nullptr;
  }
  if((com_editors&EDITOR_PARTICLE)!=0) UpdateTool(EDITOR_PARTICLE);
  renderSystem->EndAutomaticBackgroundSwaps();
  if(renderSystem->CheckOutOfMemory()) {
    idLib::Printf("map loaded with GL_OUT_OF_MEMORY\n");
    AddDialog(static_cast<gameDialogMessages_t>(131),static_cast<dialogType_t>(1),
              nullptr,nullptr,false,nullptr,0,false,false,false,0);
  }
}

void idCommonLocal::StartLevelLoad(const idStr&,const char*,bool useVideo) {
  renderSystem->SetRenderFlag(RENDERFLAG_LOADING);
  renderSystem->RemoveRenderFlag(RENDERFLAG_LOADCOMPLETE);
  Field<int>(0x5ADE8)=2;
  if(useVideo) {
    swf_loadScreenAlternate.SetInteger(0,true);
    renderSystem->SetRenderFlag(RENDERFLAG_BLOCKLOADINGICON);
    renderManager.BuildGuiFrame(nullptr,true);
    renderManager.RenderFrameAndBeginAutomaticBackgroundSwaps(true,false);
    return;
  }
  if(com_newLoadScreen.valueInteger!=0) {
    idList<idLoadScreenInfo2,5> emptyInfo;
    loadScreen.LaunchLoadScreen(emptyInfo);
    Field<int>(0x5ADE8)=1;
    return;
  }
  renderManager.BuildGuiFrame(nullptr,false);
  idRenderModelGui*& model=Field<idRenderModelGui*>(0x5ADF8);
  if(model!=nullptr) {
    model->Clear(0x28u);
    delete model;
    model=nullptr;
  }
  model=new idRenderModelGui();
  idSWF*& gui=Field<idSWF*>(0x5ADF0);
  if(gui==nullptr || gui->frameRate==0) {
    delete gui;
    gui=new idSWF("tungload_new",nullptr,true);
  }
  if(idLib::production==PROD_BUILDING || com_production.valueInteger!=0) {
    idSWF* test=new idSWF("tungload_new",nullptr,true);
    delete test;
  }
  auto& info=Field<idStaticList<idLoadScreenInfo,16>>(0x5B420);
  for(int index=0;index<info.Num();++index)
    gui->SetGlobal(info[index].varName.data,idSWFScriptVar(info[index].value));
  info.ClearFree();
  gui->LoadAtlasImage(true);
  if(IsMultiplayer() && Field<idRenderModelGui*>(0x5AE20)!=nullptr &&
     Field<idSWF*>(0x5AE10)!=nullptr) {
    ClearDialogs(true);
    session->Pump();
    ShowNextDialog(true);
  }
  UpdateLevelLoadPacifier(0.0f,static_cast<pacifierState_t>(2),true,false);
}

void idCommonLocal::StartLevelLoadGame(const idGameSpawnInfo& spawn,bool useVideo) {
  if(com_newLoadScreen.valueInteger!=0) loadScreen.SetActiveGameSpawn(&spawn);
  StartLevelLoad(spawn.mapName,spawn.loadingScreen.data,useVideo);
  if(!useVideo && com_newLoadScreen.valueInteger==0) {
    LoadingScreenSetup(Field<idSWF*>(0x5ADF0),Field<idRenderModelGui*>(0x5ADF8),spawn);
    Field<idSWF*>(0x5ADF0)->Render(Field<idRenderModelGui*>(0x5ADF8),
                                  static_cast<int>(Sys_Milliseconds()),false);
  }
  UpdateLevelLoadPacifier(0.0f,static_cast<pacifierState_t>(2),true,false);
}

namespace {
void InvokeLoadUpdate(idCommonLocal& local,bool first,bool second) {
  idSWF* const gui=*reinterpret_cast<idSWF**>(
      reinterpret_cast<std::byte*>(&local)+0x5ADF0);
  if(gui==nullptr) return;
  idSWFParmList parms;
  parms.Append(idSWFScriptVar(first));
  parms.Append(idSWFScriptVar(second));
  parms.Append(idSWFScriptVar(local.AreWeRunningPC()));
  parms.Append(idSWFScriptVar(feature_lowgore.valueInteger!=0));
  gui->Invoke("Update",parms);
}
}

void idCommonLocal::EndLevelLoad(bool levelOk) {
  if(!levelOk) return;
  renderSystem->SetRenderFlag(RENDERFLAG_LOADCOMPLETE);
  renderSystem->RemoveRenderFlag(RENDERFLAG_LOADING|RENDERFLAG_BLOCKLOADINGICON);
  renderSystem->SetLoadingIconInfo(0.0f,0.0f,0.0f,0.0f,false);
  if(com_newLoadScreen.valueInteger!=0) loadScreen.LevelLoadUpdate();
  else InvokeLoadUpdate(*this,true,true);
  UpdateLevelLoadPacifier(1.0f,static_cast<pacifierState_t>(21),true,false);
}

void idCommonLocal::ForceLoadingBlack() {
  InvokeLoadUpdate(*this,false,false);
  renderSystem->EndAutomaticBackgroundSwaps();
  UpdateLevelLoadPacifier(1.0f,static_cast<pacifierState_t>(21),true,false);
}

void idCommonLocal::LoadingScreenSetup(idSWF* setupGui,
                                       idRenderModelGui* renderGuiModel,
                                       const idGameSpawnInfo& mapSpawnInfo) {
  idSWFScriptObject* const setupObject=new idSWFScriptObject();
  const idStr mapName(mapSpawnInfo.mapName);
  const idDeclLoadingScreen* const loadingScreen=
      idDeclLoadingScreen::GetLoadingScreenForMapName(
          mapName.data,mapSpawnInfo.activeLayers);

  const bool checkpointReload=Field<bool>(0x160);
  setupObject->Set("checkpointReload",idSWFScriptVar(checkpointReload));
  if(checkpointReload) {
    std::uint32_t random=static_cast<std::uint32_t>(Sys_Milliseconds());
    if(!mapName.IsEmpty()) {
      const idStr selectedScreen(mapSpawnInfo.loadingScreen);
      if(selectedScreen.IsEmpty() && loadingScreen!=nullptr) {
        renderSystem->SetRenderFlag(RENDERFLAG_WAITFORINPUT);
        setupObject->Set("hassmp",idSWFScriptVar(r_useSMP.valueInteger!=0));
        setupObject->Set("isSideMission",idSWFScriptVar(false));
        setupObject->Set("loadIndex",idSWFScriptVar(-1));
        setupObject->Set("title",idSWFScriptVar(""));
        setupObject->Set("summary",idSWFScriptVar(""));
        if(loadingScreen->tips.Num()>0) {
          int tipIndex=0;
          if(loadingScreen->tips.Num()>1) {
            random=1103515245u*random+12345u;
            tipIndex=static_cast<int>((random>>16)&0x7FFFu)%
                     loadingScreen->tips.Num();
          }
          const char* const tip=loadingScreen->tips[tipIndex].GetLocalizedString();
          setupObject->Set("title",idSWFScriptVar(tip));
          setupObject->Set("summary",idSWFScriptVar(tip));
        }
        int textIndex=swf_loadScreenText.valueInteger;
        if(textIndex<0) {
          random=1103515245u*random+12345u;
          textIndex=static_cast<int>((random>>16)&0x7FFFu)%20+1;
        }
        setupObject->Set("textIndex",idSWFScriptVar(textIndex));
        setupObject->Set("loadIndex",idSWFScriptVar(loadingScreen->loadIndex));
      } else if(!selectedScreen.IsEmpty()) {
        setupObject->Set("isSideMission",idSWFScriptVar(true));
      }
    }

    setupObject->Set("textVisible",
                     idSWFScriptVar(feature_lowgore.valueInteger==0));
    setupGui->SetGlobal(
        "setTexture",
        idSWFScriptVar(common->GetSWFHelper()->GetSetTextureFromSwf(setupGui)));
    setupGui->SetGlobal("loadObject",idSWFScriptVar(setupObject));
    setupGui->Activate(static_cast<int>(Sys_Milliseconds()));
    idSWFParmList parms;
    setupGui->Invoke("ShowLoadingScreen",parms);

    if(loadingScreen!=nullptr && loadingScreen->tauntImage!=nullptr) {
      idSWFSpriteInstance* const sprite=
          setupGui->mainspriteInstance->Get("loadobj").GetSprite();
      if(sprite!=nullptr) {
        const idMaterial* const material=loadingScreen->tauntImage;
        sprite->SetMaterial(material->name.str,-1,-1);
        idSWFScriptObject* const tweenObject=new idSWFScriptObject();
        const float guiToVirtual=renderGuiModel->GetVirtualWidth()/
                                 setupGui->frameWidth;
        const float virtualToGui=setupGui->frameWidth/
                                 renderGuiModel->GetVirtualWidth();
        const float textWidth=static_cast<float>(
            setupGui->GetGlobal("taunttext_width").ToNumber())*guiToVirtual;
        const float safeWidth=renderGuiModel->GetVirtualWidth()*
                              swf_tweakSafeAreaWidth.valueFloat;
        const float imageWidth=static_cast<float>(material->GetWidth())*
                               virtualToGui;
        const float center=guiToVirtual*sprite->GetAbsolutePos().x+
                           imageWidth*0.5f;
        tweenObject->Set("startX",idSWFScriptVar(-textWidth));
        if(renderGuiModel->GetVirtualWidth()-safeWidth<=textWidth+center) {
          if(center-textWidth<=safeWidth) {
            tweenObject->Set(
                "endX",idSWFScriptVar(renderGuiModel->GetVirtualWidth()*0.5f*
                                      virtualToGui));
          } else {
            tweenObject->Set("endX",idSWFScriptVar(
                (center-textWidth*0.5f)*virtualToGui));
          }
        } else {
          tweenObject->Set("endX",idSWFScriptVar(
              (textWidth*0.5f+center)*virtualToGui));
        }
        setupGui->SetGlobal("tweenObject",idSWFScriptVar(tweenObject));
        idSWFParmList tweenParms;
        setupGui->Invoke("UpdateTween",tweenParms);
        tweenObject->Release();
      }
    }
  }

  idSWF* const loadingGui=Field<idSWF*>(0x5ADF0);
  if(loadingGui!=nullptr && com_LoadScreenDevMessage.valueInteger!=0) {
    idSWFTextInstance* const text=loadingGui->GetGlobal("devinfo").GetText();
    if(text!=nullptr) {
      idPlayerProfile* const profile=
          session->GetProfileFromInputDevice(common->GetFocusDeviceNum());
      if(profile!=nullptr) {
        idStr message;
        using GetMessageFn=void(*)(idPlayerProfile*,idStr&);
        const auto vtable=*reinterpret_cast<const void* const* const*>(profile);
        reinterpret_cast<GetMessageFn>(vtable[11])(profile,message);
        text->text=message;
        text->lengthCalculated=false;
        text->visible=true;
      }
    }
  }
  setupObject->Release();
}
