#include "engine/framework/common_local.h"

#include "engine/framework/buildversion.h"
#include "engine/framework/consolehistory.h"
#include "engine/framework/keyinput.h"
#include "engine/framework/loadscreen.h"
#include "engine/framework/session.h"
#include "engine/framework/usercmdgen.h"
#include "engine/framework/rendermanager.h"
#include "engine/framework/console.h"
#include "engine/framework/streamfilecache.h"
#include "engine/framework/sourcecontrolwrapper.h"
#include "engine/framework/dialog.h"
#include "engine/framework/gamesystem.h"
#include "engine/framework/resourcemanager.h"
#include "engine/framework/debuggraph.h"
#include "engine/framework/subtitles.h"
#include "engine/framework/swfhelper.h"
#include "engine/guis/swf/swf.h"
#include "engine/models/rendermodelgui.h"
#include "engine/network/snapshot.h"
#include "engine/renderer/renderersystem.h"
#include "engine/renderer/virtualtexture.h"
#include "engine/renderer/virtualtexturepreloader.h"
#include "engine/renderer/ingamevideo.h"
#include "engine/renderer/jobs/render/parmstate.h"
#include "engine/decls/declmanager.h"
#include "engine/sound/soundsystem.h"
#include "engine/sys/sys_savegame.h"
#include "shared/idlib/csystems/cmdsystemlocal.h"
#include "shared/idlib/csystems/cvarsystemlocal.h"
#include "shared/idlib/filesystem/filesystemlocal.h"
#include "shared/idlib/text/cmdargs.h"
#include "shared/idlib/parallelism/paralleljoblist.h"
#include "shared/idlib/metrics/gaugemetric.h"
#include "shared/idlib/metrics/recoverymetrics.h"
#include "shared/idlib/filesystem/cassystemlocal.h"
#include "engine/sys/sys_public.h"
#include "engine/sys/win32/win_misc.h"
#include "engine/sys/win32/win_main.h"
#include "engine/sys/win32/win_cpu.h"
#include "engine/sys/win32/win_input.h"
#include "engine/sys/sys_streaming_install.h"
#include "engine/renderer/declmaterial.h"
#include "shared/idlib/filter/warningsfilter.h"
#include "shared/idlib/networking/networking.h"
#include "shared/idlib/sys/win32_pc/win_files.h"
#include "shared/idlib/text/lexer.h"

#include <intrin.h>
#include <Windows.h>

class idCollisionModelManager {
public:
  const void* const* vtable;
  void Shutdown() {
    using Fn=void(*)(idCollisionModelManager*);
    reinterpret_cast<Fn>(vtable[2])(this);
  }
  void Init() {
    using Fn=void(*)(idCollisionModelManager*);
    reinterpret_cast<Fn>(vtable[1])(this);
  }
  void WaitForAllQueries();
};
extern idCollisionModelManager* collisionModelManager;
class idRegressionLocal {
public:
  idRegressionLocal();
  ~idRegressionLocal();
};
class idAnimationLocal {
public:
  const void* const* vtable;
  void FreeDynamic() {
    using Fn=void(*)(idAnimationLocal*);
    reinterpret_cast<Fn>(vtable[3])(this);
  }
  void Shutdown() {
    using Fn=void(*)(idAnimationLocal*);
    reinterpret_cast<Fn>(vtable[2])(this);
  }
  void Initialize() {
    using Fn=void(*)(idAnimationLocal*);
    reinterpret_cast<Fn>(vtable[1])(this);
  }
};
class idAnimRecorder {
public:
  static void KillInstance();
};
class idRenderModelTransparency { public: static void Init(); static void Shutdown(); };
class idRenderModelEffects { public: static void Init(); static void Shutdown(); };
class idRenderModelDecal { public: static void Init(); static void Shutdown(); };
class idRenderModelBeam { public: static void Init(); static void Shutdown(); };
class idRenderModelFlare { public: static void Init(); static void Shutdown(); };
class idRenderModelFlareOcclusionQuad { public: static void Init(); static void Shutdown(); };
class idRenderModelCuttableBeams { public: static void Init(); static void Shutdown(); };
class idReportData { public: void Init(); };
extern idAnimationLocal* animation;
extern idTypeInfoTools* typeInfoTools;
extern int com_editors;
extern idPLog pLog;
extern idReportData reportData;
extern idCVar r_mode;
extern idCVar r_fullscreen;
extern idCVar r_multiSamples;
extern idCVar r_windowPosX;
extern idCVar r_windowPosY;
extern idCVar r_swapInterval;
extern idCVar vt_genMissingPages;
extern idCVar fs_savepath;
extern idCVar fs_cachepath;
extern idCVar fs_installpath;
extern idCVar sys_lang;
extern idCVar sys_langJapanese;
extern idCVar com_production;
#include <cstddef>
#include <cstdint>
#include <cstring>
#include <new>

idCVar com_safemode("com_safemode","0",17,
    "deletes the video.cfg & rageConfig.cfg and reverts video settings to game default",nullptr);
idCVar com_skipIntroVideo("com_skipIntroVideo","0",1,"skips the intro video",nullptr);
idCVar com_waitForSavegames("com_waitForSavegames","1",1,
    "during map heap reset, wait for the savegames to complete",nullptr);
idCVar com_headless("com_headless","0",32769,"run without graphics",nullptr);
idCVar com_speeds("com_speeds","0",2,
    "show engine timings. 1 = print on console, 2 = print on screen, 3 = also show graph, 4 = only logging to metrics stream",
    0.0f,4.0f,nullptr);
idCVar com_timescale("timescale","1",4,"scales the time",0.1f,10.0f,nullptr);
idCVar com_slowmoscale("slowmoscale","1",4,"puts the game in slowmo",0.1f,1.0f,nullptr);
idCVar com_editorActive("com_editorActive","0",17,"1 when an editor has focus",nullptr);
idCVar com_captureSamples("com_captureSamples","1",2,
    "set to number of samples for screenshots",nullptr);
idCVar com_capturePath("com_capturePath","",0,
    "optional outpath for screenshots, this path is appended to screeenshots/",nullptr);
idCVar com_captureFilename("com_captureFilename","",0,
    "optional filename for screenshots, this is only used if no filename is given in the screenshot function call",nullptr);
idCVar com_captureTGA("com_captureTGA","0",2,
    "format for capture screenshots, 0 = JPG, 1 = TGA, 2 = PNG",nullptr);
idCVar com_capturePrimes("com_capturePrimes","0",2,
    "number of prime screens to use during capturing",nullptr);
idCVar com_skipStartFlow("com_skipStartFlow","0",1,
    "Do not require user to press start to get to main menu and log 1st controller as main user",nullptr);
idCVar net_usePlatformBackend("net_usePlatformBackend","1",16385,
    "Depending on the platform this will use the backend rather then direct connection. (Steam on PC, Live on xbox360, psn on ps3)",nullptr);
idCVar com_multiplayer("com_multiplayer","0",16385,
    "enable multiplayer code paths (decls and gamecode)",nullptr);
idCVar com_uploadDumps("com_uploadDumps","1",1,"Upload crashdumps on boot",nullptr);
idCVar com_uploadDumpPath("com_writeDumpPath",
    "nfs:4@mgfiles01.ad.machinegames.com/crashdumps/",0,
    "Path to write dumps to",nullptr);
idCVar com_fixedTic("com_fixedTic","0",1,
    "run a single game frame per render frame",nullptr);
idCVar com_useMapHeap("com_useMapHeap","1",1,
    "Use a separate heap for each map load",nullptr);
idCVar com_showMapLoadStatus("com_showMapLoadStatus","0",1,
    "show level load pacifier text",nullptr);
idCVar com_signinOnStart("com_signinOnStart","0",9,
    "sign in a local user on game start (pc, ps3)",nullptr);

const char* pacifierStateToString[]={
  "IDLE","PARSING MAP","LOADING MENUS","LOADING RENDER DATA",
  "LOADING SOUND DATA","LOADING DEFAULT SCRIPT","LOADING AAS",
  "LOADING SAVE GAME","LOADING COLLISION DATA","LOADING MAP SCRIPT",
  "SPAWNING WORLD","CREATING ENTITIES","INITIALIZING ENTITIES",
  "SPAWNING ENTITIES","SPAWNING PLAYER","RUN FRAME","LOAD PLAYER INFO",
  "SPECIFICATION COMPLETE","WAITING FOR PEERS","SAVING GAME",
  "COPYING FILES TO HDD","LOADCOMPLETE"
};

idRenderDebug* GetGameLibRenderDebug() {
  idRenderWorld* const world=common->RW();
  if(world!=nullptr) return common->RW()->GetRenderDebug();
  return nullptr;
}

gameReturnPlayer_t::gameReturnPlayer_t() : view() {
  extern idCVar joy_pitchSpeed;
  extern idCVar joy_yawSpeed;
  extern idCVar m_sensitivity;
  extern idCVar in_invertLook;
  inputSettings.mouseSensScale=1.0f;
  inputSettings.joySensScale=1.0f;
  inputSettings.joyPitchSpeed=joy_pitchSpeed.valueFloat;
  inputSettings.joyYawSpeed=joy_yawSpeed.valueFloat;
  inputSettings.mouseSens=m_sensitivity.valueFloat;
  inputSettings.lookInvert=in_invertLook.valueInteger!=0;
  inputSettings.bindset=BIND_DEFAULT;
  inputSettings.absoluteAngles=false;
  inputSettings.aimAssistAngles={};
  valid=false;
  gui=nullptr;
  inventoryModel=nullptr;
  hudModel=nullptr;
  vibrationLow=0;
  vibrationHigh=0;
  vibrationLeft=0;
  vibrationRight=0;
  visMask=0;
}

gameReturnPlayer_t::~gameReturnPlayer_t()=default;

gameReturn_t::gameReturn_t()
    : localTime(0),serverTime(0),forceOneView(false),players(),debugGuis() {}

gameReturn_t::~gameReturn_t()=default;

idUserCmdMgr::idUserCmdMgr() : cmdBuffer(),writeFrame{},readFrame{} {
  std::memset(cmdBuffer,0,sizeof(cmdBuffer));
  std::memset(writeFrame,0,sizeof(writeFrame));
  std::memset(readFrame,0xFF,sizeof(readFrame));
}

namespace {
struct TerminationThreadView {
  std::byte prefix[88];
  bool threadStopped;
};
struct RegressionView {
  const void* const* vtable;
  void SetName(const char* name) {
    using Fn=void(*)(RegressionView*,const char*);
    reinterpret_cast<Fn>(vtable[8])(this,name);
  }
  bool GetTestInProgress() {
    using Fn=bool(*)(RegressionView*);
    return reinterpret_cast<Fn>(vtable[13])(this);
  }
  void NextTest() {
    using Fn=void(*)(RegressionView*);
    reinterpret_cast<Fn>(vtable[5])(this);
  }
};
class StorageContinueFunction final : public idSWFScriptFunction_RefCounted {
public:
  idSWFScriptVar Call(idSWFScriptObject*,idSWFParmList&) override {
    common->ClearDialog(GDM_INSUFFICENT_STORAGE_SPACE,nullptr,0);
    common->Quit(0);
    return idSWFScriptVar();
  }
};
class ScopedSystemHeap {
public:
  ScopedSystemHeap() { idMem::Singleton()->PushHeap(HEAP_SYSTEMHEAP); }
  ~ScopedSystemHeap() { idMem::Singleton()->PopHeap(); }
};
}

idCommonLocal::idCommonLocal() {
  Field<std::uint32_t>(0x160)=0;
  new (&Field<idSourceControlWrapper>(0x168)) idSourceControlWrapper();
  Field<std::uint16_t>(0x1C8)=0;
  Field<int>(0x1CC)=0;
  Field<void*>(0x1D0)=nullptr;
  Field<std::uint64_t>(0x1D8)=0;
  Field<void*>(0x1E0)=nullptr;
  new (&Field<idRegressionLocal>(0x1F0)) idRegressionLocal();
  Field<std::uint64_t>(0x6CC)=0;
  Field<std::uint64_t>(0x728)=100000;
  Field<std::uint64_t>(0x738)=0;
  Field<std::uint64_t>(0x750)=0;
  new (&Field<gameReturn_t>(0x760)) gameReturn_t();
  new (&Field<gameReturn_t>(0x1A6E0)) gameReturn_t();
  Field<float>(0x34660)=0.0f;
  Field<void*>(0x34678)=nullptr;
  Field<std::uint32_t>(0x34680)=0;
  Field<bool>(0x34684)=false;
  Field<bool>(0x34685)=false;
  Field<int>(0x34688)=0;
  Field<bool>(0x3468C)=true;
  Field<std::uint64_t>(0x34694)=0;
  const idStr metricDescription("Time between successive snapshots");
  const idStr metricName("snapDeltaTime");
  new (&Field<idGaugeMetric>(0x346A0)) idGaugeMetric(metricName,metricDescription);
  Field<void*>(0x34728)=nullptr;
  Field<std::uint64_t>(0x34730)=0;
  for(int index=0;index<16;++index)
    new (&Field<idSnapShot>(0x34738+index*sizeof(idSnapShot))) idSnapShot();
  Field<float>(0x34C38)=0.5f;
  Field<float>(0x34C3C)=0.0f;
  Field<std::uint64_t>(0x34C40)=0;
  new (&Field<idSnapShot>(0x34C58)) idSnapShot();
  new (&Field<idList<idMapChangeListener*,5>>(0x34CA8))
      idList<idMapChangeListener*,5>();

  extern idCVar joy_pitchSpeed;
  extern idCVar joy_yawSpeed;
  extern idCVar m_sensitivity;
  extern idCVar in_invertLook;
  auto* const settings=&Field<inputSettings_t>(0x34CC8);
  for(int index=0;index<4;++index) {
    settings[index].bindset=BIND_DEFAULT;
    settings[index].mouseSensScale=1.0f;
    settings[index].joySensScale=1.0f;
    settings[index].joyPitchSpeed=joy_pitchSpeed.valueFloat;
    settings[index].joyYawSpeed=joy_yawSpeed.valueFloat;
    settings[index].mouseSens=m_sensitivity.valueFloat;
    settings[index].absoluteAngles=false;
    settings[index].lookInvert=in_invertLook.valueInteger!=0;
    settings[index].aimAssistAngles={};
  }
  auto& userCmdMgr=Field<idUserCmdMgr>(0x34D68);
  new (&userCmdMgr) idUserCmdMgr();
  Field<int>(0x5ADE8)=0;
  Field<std::uint64_t>(0x5AE00)=0;
  Field<int>(0x5AE08)=0;
  Field<std::uint16_t>(0x5AE0C)=0;
  Field<void*>(0x5AE10)=nullptr;
  Field<void*>(0x5AE18)=nullptr;
  Field<void*>(0x5AE20)=nullptr;
  new (&Field<idStaticList<idDialogInfo,4>>(0x5AE28))
      idStaticList<idDialogInfo,4>();
  new (&Field<idStaticList<idLoadScreenInfo,16>>(0x5B420))
      idStaticList<idLoadScreenInfo,16>();
  auto* const quitText=&Field<idStr>(0x5B938);
  auto* const stayText=&Field<idStr>(0x5BA00);
  for(int index=0;index<5;++index) {
    new (&quitText[index]) idStr();
    new (&stayText[index]) idStr();
    quitText[index]=va("#str_menu_main_exitgame_0%d",index+1);
    stayText[index]=va("#str_menu_main_staygame_0%d",index+1);
  }
  Field<char>(0x188)=0;
  Field<std::uint64_t>(0x6D8)=0;
  Field<std::uint64_t>(0x6E0)=0;
  Field<std::uint64_t>(0x6E8)=0;
  Field<std::uint64_t>(0x6F0)=0;
  Field<std::uint32_t>(0x6F8)=0;
  Field<std::uint64_t>(0x700)=0;
  Field<std::uint64_t>(0x708)=0;
  Field<std::uint64_t>(0x710)=0;
  Field<std::uint64_t>(0x718)=0;
  Field<std::uint32_t>(0x720)=0;
  Field<int>(0x760)=-1;
  Field<int>(0x764)=-1;
  Field<int>(0x1A6E0)=-1;
  Field<int>(0x1A6E4)=-1;
  Field<std::uint64_t>(0x34664)=0;
  Field<std::uint64_t>(0x3466C)=0;
  Field<void*>(0x34C48)=nullptr;
  Field<void*>(0x34C50)=nullptr;
  Field<void*>(0x34CC0)=nullptr;
  Field<bool>(0x34720)=false;
}

idCommonLocal::~idCommonLocal() {
  idDebugGraph*& graph=Field<idDebugGraph*>(0x34C48);
  if(graph!=nullptr) {
    delete graph;
    graph=nullptr;
    idDebugGraph*& graphSnapRate=Field<idDebugGraph*>(0x34C50);
    delete graphSnapRate;
    graphSnapRate=nullptr;
  }
  idRenderModelGui*& guiModel=Field<idRenderModelGui*>(0x5ADF8);
  delete guiModel;
  guiModel=nullptr;
  idSWF*& loadingGui=Field<idSWF*>(0x5ADF0);
  delete loadingGui;
  loadingGui=nullptr;
  idRenderVideoOverlay*& videoOverlay=Field<idRenderVideoOverlay*>(0x5AE18);
  delete videoOverlay;
  videoOverlay=nullptr;
  idSWFHelper*& swfHelper=Field<idSWFHelper*>(0x34CC0);
  if(swfHelper!=nullptr) {
    idMem::Singleton()->Free(swfHelper,ALIGN_16);
    swfHelper=nullptr;
  }
  auto* const quitText=&Field<idStr>(0x5B938);
  auto* const stayText=&Field<idStr>(0x5BA00);
  for(int index=4;index>=0;--index) {
    stayText[index].~idStr();
    quitText[index].~idStr();
  }
  using LoadInfoList=idStaticList<idLoadScreenInfo,16>;
  using DialogList=idStaticList<idDialogInfo,4>;
  using ListenerList=idList<idMapChangeListener*,5>;
  Field<LoadInfoList>(0x5B420).~LoadInfoList();
  Field<DialogList>(0x5AE28).~DialogList();
  Field<ListenerList>(0x34CA8).~ListenerList();
  Field<idSnapShot>(0x34C58).~idSnapShot();
  for(int index=15;index>=0;--index)
    Field<idSnapShot>(0x34738+index*sizeof(idSnapShot)).~idSnapShot();
  Field<idGaugeMetric>(0x346A0).~idGaugeMetric();
  Field<gameReturn_t>(0x1A6E0).~gameReturn_t();
  Field<gameReturn_t>(0x760).~gameReturn_t();
  Field<idRegressionLocal>(0x1F0).~idRegressionLocal();
  Field<idSourceControlWrapper>(0x168).~idSourceControlWrapper();
}

idCommonLocal commonLocal;
idCommon* common=&commonLocal;

bool idCommonLocal::IsTerminationThreadStopped() const {
  const auto* const thread=Field<TerminationThreadView*>(0x34728);
  return thread!=nullptr && thread->threadStopped;
}

void idCommonLocal::SyncAllBackgroundOperations(bool exceptSounds) {
  renderManager.ClearAllGuiModels();
  virtualTexturePreloader.EndPreloading();
  if(com_waitForSavegames.valueInteger!=0) {
    session->UpdateSignInManager();
    idSaveGameManager* const saveGames=session->GetSaveGameManager();
    if(saveGames->GotWork()) {
      renderSystem->BeginAutomaticBackgroundSwaps(
          renderSystem->GetGameWindow(),false,false);
      session->GetSaveGameManager()->WaitForAllProcessors(false);
      renderSystem->EndAutomaticBackgroundSwaps();
    }
  }
  renderSystem->SyncRenderThread(true);
  virtualTextureSystem.SyncFeedback();
  collisionModelManager->WaitForAllQueries();
  parallelJobManager->WaitForAllJobLists();
  soundSystem->WaitForSoundThread();
  if(!exceptSounds) soundSystem->StopAllSounds();
  streamFileCache->Flush();
}

void idCommonLocal::ResetMapHeap(const bool forceProductionPath) {
  idLib::Printf("idCommonLocal::ResetMapHeap\n");
  SyncAllBackgroundOperations(false);
  ResetNetworkingState();
  renderManager.Clear();
  if((com_editors&2)==0) renderThreadParmState->DefaultRenderParms();
  KillDialog();
  console->ClearEditLine();
  ClearWarnings("");
  videoManager->UnbindAll();
  virtualTextureSystem.FreeLockedPages();
  virtualTextureSystem.ClearFeedback();
  if(forceProductionPath || idLib::production!=PROD_DEVELOPMENT) {
    idVirtualTexture* virtualTexture=&virtualTextureSystem.vts[2];
    for(int index=0;index<14;++index) virtualTexture[index].FreeData();
    session->ShutdownSoundRelatedSystems();
    soundSystem->Shutdown();
    idAnimRecorder::KillInstance();
    idResourceList::FreeAllDynamicResources();
    animation->FreeDynamic();
    declManager->FreeDynamic();
    typeInfoTools->ClearWarnings();
    idAnimRecorder::KillInstance();
    if(idLib::production>=PROD_PRODUCTION) idAtomicString::FreeDynamic();
    streamFileCache->ClearMem(true);
    if(com_useMapHeap.valueInteger!=0 && idLib::production>=PROD_PRODUCTION)
      idMem::Singleton()->ResetMapHeap();
    soundSystem->Init();
    session->InitializeSoundRelatedSystems();
  }
  InitDialog();
  idLib::Printf("idCommonLocal::ResetMapHeap done\n");
}

void idCommonLocal::Init(int argc,const char** argv,const char* cmdline) {
  idLib::Init();
  idParser::SetupGlobalDefines();

  idStr rawCommandLine;
  idCmdArgs args;
  if(cmdline!=nullptr) {
    rawCommandLine=cmdline;
    args.TokenizeString(cmdline,true);
  } else {
    for(int index=0;index<argc;++index) {
      rawCommandLine.Append(" ");
      rawCommandLine.Append(argv[index]);
      args.AppendArg(argv[index]);
    }
  }

  session->Initialize();
  Sys_SetLanguageFromSystem();
  ClearWarnings("Wolfenstein The New Order initialization");
  cmdSystem->Init();
  idCVar::RegisterStaticVars();
  const idStr steamMPBootInviteCheck(cmdline!=nullptr?cmdline:"");
  if(steamMPBootInviteCheck.Find("connect_lobby",false,0,-1)!=-1)
    com_multiplayer.SetBool(true,true);

  int logicalCount=0;
  int coreCount=0;
  int packageCount=0;
  Sys_CPUCount(&logicalCount,&coreCount,&packageCount);
  int jobCount=logicalCount-2;
  if(logicalCount!=coreCount && logicalCount>=8) jobCount=logicalCount-3;
  if(jobCount<1) jobCount=1;
  cvarSystem->SetCVarInteger("jobs_numThreads",jobCount,0);
  if(logicalCount<=4)
    cvarSystem->SetCVarInteger("vt_maxPPF",16,0);
  else if(logicalCount<=6)
    cvarSystem->SetCVarInteger("vt_maxPPF",64,0);
  if(packageCount>1) {
    idLib::Printf("Enabling heavy jobsystem lock because this machine have %d cpu's\n",
                  packageCount);
    cvarSystem->SetCVarInteger("jobs_useHeavyLocks",1,0);
  }

  r_mode.flags&=~0x20000u;
  r_fullscreen.flags&=~0x20000u;
  r_multiSamples.flags&=~0x20000u;
  r_windowPosX.flags&=~0x20000u;
  r_windowPosY.flags&=~0x20000u;
  r_swapInterval.flags&=~0x20000u;
  vt_genMissingPages.flags&=~0x20000u;
  SetCommandLineCVars(args,true);
  idLib::headless=com_headless.valueInteger!=0;
  console->Init();
  warningList.RegisterPrintListener();
  refreshOnPrint.RegisterPrintListener();
  idLib::Printf("%s\n",versionInfo.fullVersion);
  idLib::Printf("%s\n",versionInfo.netVersion);

  if(fs_savepath.valueString[0]=='\0')
    fs_savepath.SetString(Sys_DefaultDevStoragePath(),true);
  if(fs_cachepath.valueString[0]=='\0')
    fs_cachepath.SetString(Sys_DefaultCachePath(),true);
  if(fs_installpath.valueString[0]=='\0')
    fs_installpath.SetString(Sys_Install(),true);
  fileSystem->InitRetail();
  streamingInstallMgrLocal.Init();
  if(com_safemode.valueInteger!=0)
    fileSystem->RemoveFileRetail("wolfConfig.cfg",FSPATH_SAVE);
  warningsFilter.Init();
  reportData.Init();
  idLib::sourceControl=&Field<idSourceControlWrapper>(0x168);
  logFileWriter.RegisterPrintListener();
  parallelJobManager->Init();

  bool resourceRestart=com_skipIntroVideo.valueInteger!=0;
  if(resourceRestart)
    renderSystem->SetRenderFlag(RENDERFLAG_BLOCKLOADINGICONINTRO);
  for(int index=0;index<args.argc;++index) {
    const char* argument=args.Argv(index);
    if(argument[0]=='+') ++argument;
    if(idStr::Icmp(argument,"idstudio")==0) com_editors|=2;
    if(idStr::Icmp(argument,"fromGameRestart")==0 ||
       idStr::Icmp(argument,"pc_currentDisc")==0)
      resourceRestart=true;
    if(idStr::Icmp(argument,"renderbump")==0 ||
       idStr::Icmp(argument,"idstudio")==0 ||
       idStr::Icmp(argument,"dmap")==0)
      cvarSystem->SetCVarBool("r_fullscreen",false,0);
    if(idStr::Icmp(argument,"connect_lobby")==0) {
      ++index;
      session->HandleBootableInvite(_atoi64(args.Argv(index)));
    }
  }

  Sys_DetectCPU();
  Sys_InitNetworking();
  idHeapArray<unsigned char> buildConfig;
  fileSystem->ReadFileRetail("build.cfg",buildConfig,nullptr);
  if(buildConfig.num!=0) {
    idCmdArgs buildArgs;
    buildArgs.TokenizeString(reinterpret_cast<const char*>(buildConfig.buffer),true);
    SetCommandLineCVars(buildArgs,true);
  }
  for(int device=0;device<4;++device) {
    idKeyInput::SetUserDeviceNumForBind(device);
    cmdSystem->AppendCommandText("exec default.cfg -s\n");
    idLib::Printf("Executing default.cfg for device #%d...\n",device);
    cmdSystem->ExecuteCommandBuffer();
  }

  session->InitForge();
  resourceManager->Init();
  casSystem->InitRetail();
  casSystem2->InitRetail();
  Field<bool>(0x34722)=false;
  Field<bool>(0x34723)=false;
  if(idStr::Icmp(sys_lang.valueString.data,"French")==0) {
    cmdSystem->AppendCommandText("exec default_french.cfg\n");
    cmdSystem->ExecuteCommandBuffer();
  }
  idKeyInput::SetUserDeviceNumForBind(0);

  for(int attempt=0;attempt<2;++attempt) {
    void* configBuffer=nullptr;
    const int configLength=fileSystem->ReadFileRetail(
        "wolfConfig.cfg",&configBuffer,nullptr);
    if(configBuffer==nullptr) {
      idLib::Warning("Config file \"%s\" not found","wolfConfig.cfg");
      break;
    }
    int configVersion=-1;
    bool accepted=false;
    idLexer lexer;
    if(lexer.LoadMemory(static_cast<const char*>(configBuffer),
                        static_cast<std::size_t>(configLength),
                        "wolfConfig.cfg")) {
      idToken token;
      if(lexer.ReadToken(token) && token.type==TT_NAME &&
         idStr::Icmp(token.data,"configVersion")==0 &&
         lexer.ReadToken(token) && token.type==TT_NUMBER && token.subtype==3) {
        configVersion=std::atol(token.data);
        if(configVersion==1) {
          cmdSystem->AppendCommandText(lexer.GetScriptP());
          cmdSystem->ExecuteCommandBuffer();
          accepted=true;
        }
      }
    }
    fileSystem->FreeFileRetail(configBuffer);
    if(accepted) break;
    const va mismatchText("mismatch (Expecting %d, found %d)",1,configVersion);
    const char* versionText=configVersion==-1?"missing":mismatchText.buffer;
    idLib::Warning("Config \"%s\" version number %s%s","wolfConfig.cfg",
                   versionText,attempt==0?", removing from FSPATH_SAVE":"");
    if(attempt==0)
      fileSystem->RemoveFileRetail("wolfConfig.cfg",FSPATH_SAVE);
  }

  SetCommandLineCVars(args,false);
  idLib::Printf("Command line is: %s\n",rawCommandLine.data);
  renderSystem->Init();
  cvarSystem->ClearModifiedFlags(0x10000);
  if(com_production.valueInteger==0) cvarSystem->ResetFlaggedVariables(8);
  sys_langJapanese.SetBool(
      idStr::Icmp(sys_lang.valueString.data,"japanese")==0,true);
  if(!idLib::headless && idLib::production!=PROD_PRODUCTION &&
     fileSystem->GetTimestampRetail("default.cfg",false)==0xFFFFFFFFu) {
    const char* savePath=cvarSystem->GetCVarString("fs_savepath","");
    const char* basePath=cvarSystem->GetCVarString("fs_basepath","");
    idLib::FatalError(
        "Could not find default.cfg. Check paths.\nfs_basepath = '%s'\nfs_savepath = '%s'",
        basePath,savePath);
  }
  console->InitGuiModel();
  soundSystem->Init();
  declManager->Init();
  gameSystem->Init();
  animation->Initialize();
  InitLanguageDict();
  sys_lang.flags&=~0x20000u;
  usercmdGen->Init();
  Sys_InitInput();
  resourceManager->Init2(resourceRestart);
  renderSystem->MakeCurrent(renderSystem->GetGameWindow());
  renderThreadParmState->Init();
  idMaterial::InitCompatibleGenericMaterials();
  renderSystem->Init2();
  idRenderModelTransparency::Init();
  idRenderModelEffects::Init();
  idRenderModelDecal::Init();
  idRenderModelBeam::Init();
  idRenderModelFlare::Init();
  idRenderModelFlareOcclusionQuad::Init();
  idRenderModelCuttableBeams::Init();
  console->LoadGraphics();
  InitCommands();
  gameSystem->LoadStaticResources();
  collisionModelManager->Init();
  idMem::Singleton()->SetOutOfMemoryCallback(StaticOutOfMemoryCallback);
  session->Initialize();
  session->InitializeSoundRelatedSystems();
  PrintWarnings();
  Field<idRenderVideoOverlay*>(0x5AE18)=new idSubtitles_VideoOverlay();
  idResourceList::MarkAllStaticResources();
  declManager->MarkStatic();
  loadScreen.Init();
  idSWFHelper*& swfHelper=Field<idSWFHelper*>(0x34CC0);
  if(swfHelper==nullptr) swfHelper=new idSWFHelper();
  LoadMainMenu();
  if(IsMultiplayer()) declManager->PrintMultiplayerDeclOverrides();
  console->ClearNotifyLines();
  consoleHistory.LoadHistoryFile();

  bool commandFound=false;
  bool skipArguments=true;
  for(int index=0;index<args.argc;++index) {
    const char* argument=args.Argv(index);
    if(argument[0]=='+') {
      ++argument;
      if(argument[0]=='\0') argument=args.Argv(++index);
      skipArguments=idStr::Icmp(argument,"set")==0 ||
                    cvarSystem->Find(argument)!=nullptr;
      if(commandFound) cmdSystem->AppendCommandText("\n");
    }
    if(!skipArguments) {
      cmdSystem->AppendCommandText(argument);
      cmdSystem->AppendCommandText(" ");
      commandFound=true;
    }
  }
  if(commandFound)
    cmdSystem->AppendCommandText("\n");
  else if(!idLib::headless)
    Field<idGame*>(0x1D0)->Shell_Show(true);
  Field<bool>(0x161)=true;
  resourceManager->StartupComplete();
  cvarSystem->ClearModifiedCVars();
  CheckStartupStorageRequirements();
}

void idCommonLocal::Shutdown(const int exitCode) {
  if(Field<bool>(0x34684)) return;
  Field<bool>(0x34684)=true;
  SetRefreshOnPrint(false);
  renderSystem->SyncRenderThread(false);
  idSaveGameManager* const saveManager=session->GetSaveGameManager();
  if(saveManager->GetSaveGameThread().threadHandle!=0)
    session->GetSaveGameManager()->GetSaveGameThread().StopThread(true);
  KillDialog();
  soundSystem->WaitForSoundThread();
  soundSystem->StopAllSounds();
  if(renderSystem->BinkVideoIsLoaded()) renderSystem->FreeBinkVideo();
  pLog.Free();
  session->MoveToPressStart();
  gameSystem->FreeGame(Field<idGame*>(0x1D0));
  Field<bool>(0x163)=false;
  Field<idStaticList<idLoadScreenInfo,16>>(0x5B420).ClearFree();
  loadScreen.Shutdown(true);
  virtualTextureSystem.Shutdown();
  session->ShutdownSoundRelatedSystems();
  soundSystem->Shutdown();
  animation->Shutdown();
  const ScopedSystemHeap everythingHereGoesInTheGlobalHeap;

  usercmdGen->Shutdown();
  collisionModelManager->Shutdown();
  idRenderModelTransparency::Shutdown();
  idRenderModelEffects::Shutdown();
  idRenderModelDecal::Shutdown();
  idRenderModelBeam::Shutdown();
  idRenderModelFlare::Shutdown();
  idRenderModelFlareOcclusionQuad::Shutdown();
  idRenderModelCuttableBeams::Shutdown();
  renderSystem->Shutdown();
  streamFileCache->Shutdown();
  declManager->Shutdown();
  gameSystem->Shutdown();
  session->Shutdown();
  parallelJobManager->Shutdown();
  resourceManager->Shutdown();
  casSystem->Shutdown();
  casSystem2->Shutdown();
  cmdSystem->Shutdown();
  logFileWriter.CloseLogFile();
  Field<idSourceControlWrapper>(0x168).Shutdown();
  console->Shutdown();

  idDebugGraph*& graph=Field<idDebugGraph*>(0x34C48);
  if(graph!=nullptr) {
    delete graph;
    graph=nullptr;
    idDebugGraph*& graphSnapRate=Field<idDebugGraph*>(0x34C50);
    delete graphSnapRate;
    graphSnapRate=nullptr;
  }
  idRenderModelGui*& guiModel=Field<idRenderModelGui*>(0x5ADF8);
  delete guiModel;
  guiModel=nullptr;
  idSWF*& loadingGui=Field<idSWF*>(0x5ADF0);
  delete loadingGui;
  loadingGui=nullptr;
  idRenderVideoOverlay*& videoOverlay=Field<idRenderVideoOverlay*>(0x5AE18);
  delete videoOverlay;
  videoOverlay=nullptr;
  idSWFHelper*& swfHelper=Field<idSWFHelper*>(0x34CC0);
  if(swfHelper!=nullptr) {
    idMem::Singleton()->Free(swfHelper,ALIGN_16);
    swfHelper=nullptr;
  }
  ClearWarnings("Wolfenstein The New Order shutdown");
  warningList.caption.Clear();
  warningList.list.ClearFree();
  warningList.hash.InternalFree();
  idLib::ShutDown();
  Sys_Quit(static_cast<UINT>(exitCode));
}

void idCommonLocal::ActivateTool(bool active) {
  com_editorActive.SetBool(active,true);
}

void idCommonLocal::WriteConfigToFile(const char* filename) {
  idFile* const file=fileSystem->OpenFileWriteRetail(filename,FSPATH_SAVE);
  if(file!=nullptr) {
    file->Printf("configVersion %d\n",1);
    idKeyInput::WriteBindings(file);
    cvarSystem->WriteFlaggedVariables(0x10000,file);
    delete file;
  } else {
    idLib::Printf("Couldn't write %s.\n",filename);
  }
}

bool idCommonLocal::IsInitialized() const { return Field<bool>(0x161); }

void idCommonLocal::Quit(int exitCode) {
  Field<bool>(0x34685)=true;
  Field<int>(0x34688)=exitCode;
}

void idCommonLocal::SetCommandLineCVars(const idCmdArgs& args,bool init) {
  for(int index=0;index<args.argc;++index) {
    const char* name=args.argv[index];
    if(name==nullptr || name[0]!='+') continue;
    ++name;
    if(name[0]=='\0' && ++index<args.argc) name=args.argv[index];
    if(idStr::Icmpn(name,"set",3)==0 && ++index<args.argc)
      name=args.argv[index];
    idCVar* const cvar=cvarSystem->Find(name);
    if(cvar!=nullptr && ((cvar->flags&0x4000)==0 || init)) {
      const char* const value=index+1<args.argc?args.argv[index+1]:"";
      cvar->SetString(value,true);
      ++index;
    }
  }
}

void idCommonLocal::CheckStartupStorageRequirements() {
  session->UpdateSignInManager();
  const std::int64_t requiredWithSaves=
      common->Game()->GetMinSaveStorageRequired(true);
  const std::int64_t requiredProfile=
      common->Game()->GetMinSaveStorageRequired(false);
  const std::int64_t requiredSaves=requiredWithSaves-requiredProfile;

  bool saveExists=true;
  bool autosaveExists=true;
  extern idCVar fs_savepath;
  idStr savePath(fs_savepath.valueString);
  idStr createPath(savePath);
  createPath.Append("\\");
  fileSystem->CreateOSPathRetail(createPath.data);
  std::int64_t freeBytes=static_cast<std::int64_t>(
      Sys_GetDriveFreeSpaceInBytes(savePath.data));
  if(freeBytes<=1 && savePath.Length()>=3 && savePath[1]==':' &&
     savePath[2]=='\\' &&
     ((savePath[0]>='A'&&savePath[0]<='Z')||
      (savePath[0]>='a'&&savePath[0]<='z'))) {
    idStr drive=savePath.Left(3);
    freeBytes=static_cast<std::int64_t>(Sys_GetDriveFreeSpaceInBytes(drive.data));
  }
  Sys_SaveGameCheck(saveExists,autosaveExists);
  const std::int64_t savesBytes=saveExists?0:requiredSaves;
  idLib::Printf("requiredSizeSavesBytes: %lld\n",savesBytes);
  const std::int64_t profileBytes=Sys_SaveGameProfileCheck()?0:requiredProfile;
  const std::int64_t requiredBytes=savesBytes+profileBytes;
  idLib::Printf("requiredSizeProfileBytes: %lld\n",profileBytes);
  idLib::Printf("requiredSizeBytes: %lld\n",requiredBytes);
  if(requiredBytes<=freeBytes) return;

  idStaticList<idSWFScriptFunction*,4> callbacks;
  idStaticList<idStrId,4> optionText;
  callbacks.Append(new StorageContinueFunction());
  optionText.Append(idStrId("#str_menu_accept"));
  const idStrId messageId("#str_dlg_startup_insufficient_storage");
  const char* const format=messageId.GetLocalizedString();
  idStr sizeText;
  const float sizeAsFloat=static_cast<float>(static_cast<int>(requiredBytes));
  if(static_cast<std::uint64_t>(requiredBytes)<=0x100000u)
    sizeText=va("%.1f KB",sizeAsFloat*0.0009765625f+0.1f);
  else
    sizeText=va("%.1f MB",sizeAsFloat*0.00000095367432f+0.1f);
  idStr message=va(format,sizeText.data);
  idStrStatic<256> overrideMessage;
  overrideMessage=message;
  common->AddDynamicDialog(GDM_INSUFFICENT_STORAGE_SPACE,callbacks,optionText,
                           true,overrideMessage,false,false,false);
}

void idCommonLocal::UnRegisterMapChangeListener(idMapChangeListener* listener) {
  auto& listeners=Field<idList<idMapChangeListener*,5>>(0x34CA8);
  for(int index=0;index<listeners.Num();++index) {
    if(listeners[index]==listener) {
      listeners.RemoveIndexFast(index);
      return;
    }
  }
}

void idCommonLocal::RegisterMapChangeListener(idMapChangeListener* listener) {
  auto& listeners=Field<idList<idMapChangeListener*,5>>(0x34CA8);
  for(int index=0;index<listeners.Num();++index)
    if(listeners[index]==listener) return;
  listeners.Append(listener);
}

bool idCommonLocal::IsMainMenuActive() {
  return Field<idGame*>(0x1D0)->Shell_IsActive();
}

bool idCommonLocal::IsGamePaused() {
  return Field<idGame*>(0x1D0)->GetGamePaused(nullptr);
}

int idCommonLocal::GetRecentInputDevice() { return Field<int>(0x34690); }
int idCommonLocal::GetFocusDeviceNum() { return Field<int>(0x34694); }
void idCommonLocal::SetFocusDeviceNum(int index) { Field<int>(0x34694)=index; }
int idCommonLocal::GetPauseMenuDeviceNum() { return Field<int>(0x34698); }
void idCommonLocal::SetPauseMenuDeviceNum(int index) { Field<int>(0x34698)=index; }
bool idCommonLocal::GetAutosaveEnabled() { return Field<bool>(0x3468C); }
void idCommonLocal::EnableAutosaves(bool enable) { Field<bool>(0x3468C)=enable; }
idRenderVideoOverlay* idCommonLocal::VideoOverlay() {
  return Field<idRenderVideoOverlay*>(0x5AE18);
}
idGame* idCommonLocal::Game() { return Field<idGame*>(0x1D0); }
idRenderWorld* idCommonLocal::RW() { return Field<idGame*>(0x1D0)->GetRenderWorld(); }
idSoundWorld* idCommonLocal::SW() { return Field<idGame*>(0x1D0)->GetSoundWorld(); }
recordState_t idCommonLocal::GetRecordState() { return Field<recordState_t>(0x1E8); }
idRegression* idCommonLocal::GetRegression() {
  return reinterpret_cast<idRegression*>(reinterpret_cast<std::byte*>(this)+0x1F0);
}
bool idCommonLocal::ShouldDelayShutdown() { return Field<bool>(0x34722); }
void idCommonLocal::SetShouldDelayShutdown(bool delay) { Field<bool>(0x34722)=delay; }
bool idCommonLocal::GetAbnormalFileRead() { return Field<bool>(0x34723); }
void idCommonLocal::SetAbnormalFileRead(bool flag) { Field<bool>(0x34723)=flag; }
bool idCommonLocal::SnapshotsAreStalled() {
  return Field<std::uint64_t>(0x750)>=Field<std::uint64_t>(0x728) &&
         Field<int>(0x34730)==Field<int>(0x34734);
}
bool idCommonLocal::IsPurgeOverlaysPending() {
  return Field<bool>(0x34720) && !Field<bool>(0x34721);
}
int idCommonLocal::GetSnapRate() {
  return static_cast<int>(Field<std::uint32_t>(0x728));
}
bool idCommonLocal::IsDialogPausing() { return Field<bool>(0x5AE0D); }
bool idCommonLocal::IsMultiplayer() const { return com_multiplayer.valueInteger!=0; }

void idCommonLocal::RecordPlayback(bool record,const char* fileName) {
  idFile*& eventFile=Field<idFile*>(0x1E0);
  delete eventFile;
  eventFile=nullptr;
  const recordState_t previous=Field<recordState_t>(0x1E8);
  Field<recordState_t>(0x1E8)=RECORD_NONE;
  com_fixedTic.SetBool(false,true);
  usercmdGen->RecordPlay(0,false,nullptr);
  auto& regression=Field<RegressionView>(0x1F0);
  regression.SetName("");
  if(record && previous==RECORD_MAKE) return;
  if(regression.GetTestInProgress() && previous==RECORD_PLAY) {
    regression.NextTest();
    return;
  }
  if(fileName==nullptr) return;

  idStr bareFileName(fileName);
  bareFileName.StripFileExtension();
  idStr cmdsFileName("regression/events/");
  cmdsFileName.Append(bareFileName);
  cmdsFileName.Append(".cmds");
  idStr eventsFileName("regression/events/");
  eventsFileName.Append(bareFileName);
  eventsFileName.Append(".events");
  eventFile=record
      ? fileSystem->OpenFileWriteRetail(eventsFileName.data,FSPATH_SAVE)
      : fileSystem->OpenFileRead(eventsFileName.data,false,false);
  if(eventFile==nullptr) {
    idLib::Warning("Common::RecordPlayback could not open file %s",
                   eventsFileName.data);
    return;
  }
  if(!usercmdGen->RecordPlay(0,record,cmdsFileName.data)) {
    idLib::Warning("Common::RecordPlayback could not open file %s",
                   cmdsFileName.data);
    delete eventFile;
    eventFile=nullptr;
    return;
  }
  Field<recordState_t>(0x1E8)=record?RECORD_MAKE:RECORD_PLAY;
  com_fixedTic.SetBool(true,true);
  regression.SetName(bareFileName.data);
  if(record) {
    const std::uint32_t magic=0xBC305E00u;
    const std::uint32_t version=0x01000000u;
    eventFile->Write(&magic,4);
    eventFile->Write(&version,4);
  } else {
    std::uint32_t magic=0;
    eventFile->Read(&magic,4);
    magic=_byteswap_ulong(magic);
    if(magic==0x005E30BCu) {
      std::uint32_t version=0;
      eventFile->Read(&version,4);
      Field<int>(0x6CC)=static_cast<int>(_byteswap_ulong(version));
    } else {
      Field<int>(0x6CC)=0;
      eventFile->Seek(0,FS_SEEK_SET);
    }
  }
}

void idCommonLocal::WriteSysEvent(const sysEvent_t& event) {
  idFile* const file=Field<idFile*>(0x1E0);
  auto writeBig=[file](std::uint32_t value) {
    const std::uint32_t swapped=_byteswap_ulong(value);
    file->Write(&swapped,4);
  };
  writeBig(static_cast<std::uint32_t>(event.evPtrLength));
  if(event.evPtrLength>0) file->Write(event.evPtr,event.evPtrLength);
  writeBig(static_cast<std::uint32_t>(event.evType));
  writeBig(static_cast<std::uint32_t>(event.evValue));
  writeBig(static_cast<std::uint32_t>(event.evValue2));
  writeBig(static_cast<std::uint32_t>(event.inputDevice));
}

bool idCommonLocal::ReadSysEvent(sysEvent_t& event) {
  idFile* const file=Field<idFile*>(0x1E0);
  std::uint64_t bytes=file->Read(&event.evPtrLength,4);
  event.evPtrLength=static_cast<int>(
      _byteswap_ulong(static_cast<std::uint32_t>(event.evPtrLength)));
  if(event.evPtr!=nullptr) {
    idMem::Singleton()->Free(event.evPtr,ALIGN_16);
    event.evPtr=nullptr;
  }
  if(event.evPtrLength>0)
    bytes+=file->Read(event.evPtr,event.evPtrLength);
  bytes+=file->Read(&event.evType,4);
  event.evType=static_cast<sysEventType_t>(
      _byteswap_ulong(static_cast<std::uint32_t>(event.evType)));
  bytes+=file->Read(&event.evValue,4);
  event.evValue=static_cast<int>(
      _byteswap_ulong(static_cast<std::uint32_t>(event.evValue)));
  if(Field<int>(0x6CC)<1 && event.evType==SE_KEY) {
    if(event.evValue==126 || event.evValue==96) event.evValue=41;
    else event.evValue=static_cast<int>(MapVirtualKeyA(
        static_cast<unsigned>(event.evValue),MAPVK_VK_TO_VSC));
  }
  bytes+=file->Read(&event.evValue2,4);
  event.evValue2=static_cast<int>(
      _byteswap_ulong(static_cast<std::uint32_t>(event.evValue2)));
  bytes+=file->Read(&event.inputDevice,4);
  event.inputDevice=static_cast<int>(
      _byteswap_ulong(static_cast<std::uint32_t>(event.inputDevice)));
  if(bytes==static_cast<std::uint64_t>(event.evPtrLength)+20u) return true;
  RecordPlayback(false,nullptr);
  return false;
}
