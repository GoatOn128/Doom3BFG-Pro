#include "engine/framework/loadscreen.h"

#include "engine/framework/common_local.h"
#include "engine/framework/gamespawninfo.h"
#include "engine/framework/rendermanager.h"
#include "engine/guis/swf/swf.h"
#include "engine/guis/swf/swf_spriteinstance.h"
#include "engine/models/rendermodelgui.h"
#include "engine/renderer/renderersystem.h"
#include "shared/idlib/csystems/cvarsystemlocal.h"
#include "shared/idlib/lib.h"
#include "shared/idlib/sys/sys_alloc.h"
#include "shared/idlib/sys/sys_time.h"

#include <cmath>

extern idCVar feature_lowgore;

idCVar loadscreen_rotatetime("loadscreen_rotatetime","5000.0",CVAR_FLOAT,
                             "how much time it should take to achieve one rotation ms");

idLoadScreen loadScreen;

namespace {
void UpdateLoadingMovie(idLoadScreenThread& thread) {
  loadScreen.guiModel->Clear(0x28u);
  if(renderSystem->HasRenderFlag(16)) {
    idSWFParmList parms;
    parms.Append(idSWFScriptVar(true));
    parms.Append(idSWFScriptVar(true));
    parms.Append(idSWFScriptVar(commonLocal.AreWeRunningPC()));
    parms.Append(idSWFScriptVar(feature_lowgore.valueInteger!=0));
    loadScreen.loadingGui->Invoke("Update",parms);
  }
  if(thread.findLogo) {
    thread.findLogo=false;
    idSWFScriptVar logoValue=loadScreen.loadingGui->mainspriteInstance->Get("logoobj");
    thread.logo=logoValue.GetSprite();
    thread.prevCount=static_cast<int>(Sys_Milliseconds());
    thread.current=loadscreen_rotatetime.valueFloat*0.5f;
  }
  if(thread.logo!=nullptr) {
    thread.current+=static_cast<float>(Sys_Milliseconds()-thread.prevCount);
    thread.prevCount=static_cast<int>(Sys_Milliseconds());
    float rotateTime=loadscreen_rotatetime.valueFloat;
    if(thread.current>rotateTime) thread.current-=rotateTime;
    float angle=(thread.current/rotateTime)*6.2831855f;
    if(angle<0.0f||angle>=6.2831855f)
      angle-=std::floor(angle*0.15915494f)*6.2831855f;
    thread.logo->SetScale(std::sin(angle)*100.0f,100.0f);
  }
  loadScreen.loadingGui->Render(loadScreen.guiModel,
                                static_cast<int>(Sys_Milliseconds()),false);
  renderManager.BuildGuiFrame(loadScreen.guiModel,false);
  renderManager.RenderFrame();
}
}

void idLoadScreen::Shutdown(bool deleteThread) {
  if(thread==nullptr) return;
  thread->stopRunning=true;
  if(deleteThread) {
    delete thread;
    thread=nullptr;
  }
}

void idLoadScreen::SetActiveGameSpawn(const idGameSpawnInfo* activeSpawn) {
  spawn=activeSpawn;
}

void idLoadScreen::Init() {
  idMem::Singleton()->PushHeap(HEAP_SYSTEMHEAP);
  thread=new idLoadScreenThread();
  thread->StartWorkerThread("LoadScreen",CORE_1A,THREAD_ABOVE_NORMAL,0x40000);
  if(renderSystem->GetUsingSMP()) {
    renderSystem->MakeCurrent(nullptr);
    thread->SignalWork();
    thread->WaitForThread();
    renderSystem->MakeCurrent(renderSystem->GetGameWindow());
  }
  idMem::Singleton()->PopHeap();
}

void idLoadScreen::LevelLoadCompleted() {
  idLib::Printf("idLoadScreen::LevelLoadCompleted()\n");
  if(renderSystem->GetUsingSMP()&&guiModel!=nullptr) {
    thread->stopRunning=true;
    thread->WaitForThread();
    renderSystem->SyncRenderThread(false);
  }
  delete guiModel;
  guiModel=nullptr;
  delete loadingGui;
  loadingGui=nullptr;
}

void idLoadScreenThread::RenderFrame() { UpdateLoadingMovie(*this); }

int idLoadScreenThread::Run() {
  if(!initialized) {
    renderSystem->AllocThreadGraphicsContext();
    initialized=true;
    return 0;
  }
  isRunning=true;
  while(!stopRunning) {
    const int start=static_cast<int>(Sys_Milliseconds());
    UpdateLoadingMovie(*this);
    while(Sys_Milliseconds()-start<6) Sys_Sleep(1);
  }
  logo=nullptr;
  current=0.0f;
  prevCount=0;
  findLogo=true;
  stopRunning=false;
  isRunning=false;
  return 0;
}

void idLoadScreen::LaunchLoadScreen(const idList<idLoadScreenInfo2,5>& info) {
  idMem::Singleton()->PushHeap(HEAP_SYSTEMHEAP);
  if(renderSystem->GetUsingSMP()) thread->stopRunning=false;
  delete loadingGui;
  loadingGui=nullptr;
  delete guiModel;
  guiModel=nullptr;
  if(spawn!=nullptr) {
    idLib::Printf("idLoadScreen::LaunchLoadScreen( \"%s\" )\n",spawn->loadingScreen.data);
    guiModel=new idRenderModelGui();
    loadingGui=new idSWF("tungload_new",nullptr,true);
    for(int index=0;index<info.Num();++index) {
      idSWFScriptVar value(info[index].value);
      loadingGui->SetGlobal(info[index].varName.data,value);
    }
    idStr map(spawn->mapName.data);
    commonLocal.LoadingScreenSetup(loadingGui,guiModel,*spawn);
    loadingGui->LoadAtlasImage(true);
    loadingGui->Activate(static_cast<int>(Sys_Milliseconds()));
    loadingGui->Render(guiModel,static_cast<int>(Sys_Milliseconds()),false);
    if(renderSystem->GetUsingSMP()) {
      thread->SignalWork();
    } else {
      thread->findLogo=true;
      thread->RenderFrame();
      loadingGui->Render(guiModel,static_cast<int>(Sys_Milliseconds()),false);
      thread->RenderFrame();
    }
  }
  idMem::Singleton()->PopHeap();
}

void idLoadScreen::LevelLoadUpdate() {
  if(renderSystem->GetUsingSMP()||guiModel==nullptr) return;
  guiModel->Clear(0x28u);
  if(renderSystem->HasRenderFlag(16)) {
    idSWFParmList parms;
    parms.Append(idSWFScriptVar(true));
    parms.Append(idSWFScriptVar(true));
    parms.Append(idSWFScriptVar(commonLocal.AreWeRunningPC()));
    parms.Append(idSWFScriptVar(feature_lowgore.valueInteger!=0));
    loadingGui->Invoke("Update",parms);
  }
  loadingGui->Render(guiModel,static_cast<int>(Sys_Milliseconds()),false);
  thread->RenderFrame();
  loadingGui->Render(guiModel,static_cast<int>(Sys_Milliseconds()),false);
  thread->RenderFrame();
}
