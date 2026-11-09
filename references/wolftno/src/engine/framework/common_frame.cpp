#include "engine/framework/common_local.h"

#include "engine/framework/game_network.h"
#include "engine/framework/gamespawninfo.h"
#include "engine/framework/gametimemanager.h"
#include "engine/framework/rendermanager.h"
#include "engine/framework/playerprofile.h"
#include "engine/framework/console.h"
#include "engine/framework/debughud.h"
#include "engine/framework/session.h"
#include "engine/framework/usercmdmgr.h"
#include "engine/framework/usercmdgen.h"
#include "engine/models/serializer.h"
#include "engine/models/rendermodelgui.h"
#include "engine/renderer/imagemanager.h"
#include "engine/renderer/rendersystem_commands.h"
#include "engine/guis/swf/swf.h"
#include "engine/guis/font.h"
#include "engine/network/snapshotprocessor.h"
#include "engine/renderer/renderersystem.h"
#include "engine/sound/soundsystem.h"
#include "engine/sys/sys_public.h"
#include "engine/sys/sys_signin.h"
#include "engine/sys/sys_savegame.h"
#include "engine/sys/sys_streaming_install.h"
#include "engine/sys/sys_lobby_types.h"
#include "shared/idlib/csystems/cvarsystemlocal.h"
#include "shared/idlib/csystems/cmdsystemlocal.h"
#include "shared/idlib/filesystem/filesystemlocal.h"
#include "shared/idlib/metrics/gaugemetric.h"
#include "shared/idlib/parallelism/paralleljoblist.h"
#include "shared/idlib/lib.h"
#include "shared/idlib/exception.h"
#include "shared/idlib/color.h"
#include "shared/idlib/metrics/recoverymetrics.h"
#include "shared/idlib/sys/sys_time.h"
#include "shared/idlib/text/cmdargs.h"

#include <algorithm>
#include <cmath>
#include <cstring>

extern int com_editors;
extern idCVar net_headlessServer;
extern idCVar feature_hq;
extern idCVar com_drawSnapshots;
extern idCVar com_timescale;
extern idCVar com_fixedTic;
extern idCVar net_debug_snapShotTime;
extern idSession* session;
extern idCVar com_editorActive;
extern idCVar in_unlockMouseInMenus;
extern idCVar joy_pitchSpeed;
extern idCVar joy_yawSpeed;
extern idCVar m_sensitivity;
extern idCVar in_invertLook;
extern idCVar r_swapInterval;
extern idCVar win_pauseOnAltTab;
extern bool Sys_IsGameWindowActive();
extern void Sys_SetClipboardData(const char*);
[[noreturn]] extern void Sys_Quit(unsigned int);
extern void Com_TestStatsEnd_f(const idCmdArgs&);
extern void Sys_SoundCaptureStart(const char*);
extern idCVar com_capturePath;
extern idCVar com_captureTGA;
extern idCVar sys_lang;
class idDiscSwapManager;
extern idDiscSwapManager* discSwapMgr;
class idCompanionApp {
public:
  virtual ~idCompanionApp()=default;
  virtual void OnConnect(int)=0;
  virtual void OnDisconnect(int)=0;
  virtual bool OnRequest(int,const char*,idStr&)=0;
  virtual void Update(const idGame*)=0;
};
class idCompileStatus {
public:
  const void* const* vtable;
  void Error(const char* message) {
    reinterpret_cast<void(*)(idCompileStatus*,const char*)>(vtable[11])(
        this,message);
  }
};
struct idCompileStatusHandler { idCompileStatus* compileStatus; };

bool runOrbisCommands=false;
idCompanionApp* g_companionApp=nullptr;
idStreamingInstallManager* streamingInstallMgr=&streamingInstallMgrLocal;
idCompileStatusHandler compileStatusHandler{};

namespace {
class scopedGlobalHeap_t {
public:
  explicit scopedGlobalHeap_t(heapType_t heap) { idMem::Singleton()->PushHeap(heap); }
  ~scopedGlobalHeap_t() { idMem::Singleton()->PopHeap(); }
  scopedGlobalHeap_t(const scopedGlobalHeap_t&)=delete;
  scopedGlobalHeap_t& operator=(const scopedGlobalHeap_t&)=delete;
};
}

idPLog pLog;

int time_gameFrame=0;
int time_maxGameFrame=0;
int count_numGameFrames=0;
std::uint64_t time_frame=0;
int time_FPS=0;
float avgFrameTime=0.0f;
int time_spinning=0;
int time_renderThreadSync=0;
extern int time_renderFrame;
extern int time_GPUFrameTime;

idCVar com_forceLatestSnap("com_forceLatestSnap","0",CVAR_BOOL,"");
idCVar net_effectiveSnapRateEnable("net_effectiveSnapRateEnable","0",CVAR_BOOL,
                                  "Dynamically adjust client snaprate");
idCVar net_effectiveSnapRateDebug("net_effectiveSnapRateDebug","0",CVAR_BOOL,"Debug");
idCVar net_minBufferedSnapPCT_Static("net_minBufferedSnapPCT_Static","0.5",CVAR_FLOAT,
                                    "Min amount of snapshot buffer time we want need to buffer");
idCVar net_maxBufferedSnapMS("net_maxBufferedSnapMS","336",CVAR_INTEGER,
                            "Max time to allow for interpolation cushion");
idCVar net_minBufferedSnapWinPCT_Static("net_minBufferedSnapWinPCT_Static","1.0",CVAR_FLOAT,
                                       "Min amount of snapshot buffer time we want need to buffer");
idCVar net_interpolationCatchupRate("net_interpolationCatchupRate","1.3",CVAR_FLOAT,
                                   "Scale interpolationg rate when we fall behind");
idCVar net_interpolationFallbackRate("net_interpolationFallbackRate","0.95",CVAR_FLOAT,
                                    "Scale interpolationg rate when we fall behind");
idCVar net_interpolationBaseRate("net_interpolationBaseRate","1.0",CVAR_FLOAT,
                                "Scale interpolationg rate when we fall behind");
idCVar net_optimalDynamic("net_optimalDynamic","1",CVAR_BOOL,
                         "How fast to add to our optimal time buffer");
idCVar net_optimalSnapWindow("net_optimalSnapWindow","112",CVAR_FLOAT,"");
idCVar net_optimalSnapTime("net_optimalSnapTime","112",CVAR_FLOAT,
                          "How fast to add to our optimal time buffer");
idCVar net_maxExtrapolationInMS("net_maxExtrapolationInMS","0",CVAR_INTEGER,
                               "Max time in MS that extrapolation is allowed to occur.");
idCVar com_useSampleClock("com_useSampleClock","1",CVAR_BOOL,"");
idCVar com_syncToTime("com_syncToTime","1",CVAR_BOOL,"");
idCVar com_skipSignInManager("com_skipSignInManager","0",CVAR_BOOL|CVAR_INIT,"");
idCVar com_skipDiscSwapStateManager("com_skipDiscSwapStateManager","0",CVAR_BOOL|CVAR_INIT,"");
idCVar com_skipInputRouting("com_skipInputRouting","0",CVAR_BOOL|CVAR_INIT,"");
idCVar com_skipGameRenderView("com_skipGameRenderView","0",CVAR_BOOL|CVAR_INIT,
                             "skip generating the GUIs");
idCVar com_threadSpeedScale("com_threadSpeedScale","10.0",20,
                           "Time to screen scale");
idCVar com_statsFileName("com_statsFile","stats.txt",0,"logfile name");
idCVar com_sleepGame("com_sleepGame","0",CVAR_INTEGER,
                    "sleep for this many milliseconds between frames to simulate long game frames.");
idCVar com_disableGameScripts("com_disableGameScripts","0",CVAR_BOOL,
                             "disable script loading");
idCVar com_forceAllMapLayersToLoad("com_forceAllMapLayersToLoad","0",0,"");
idCVar com_forceSound("com_forceSound","0",CVAR_BOOL,
                     "Force sound to play even if not the focused app");
idCVar in_joystickRumble("in_joystickRumble","1",CVAR_BOOL|CVAR_ARCHIVE,
                        "enables joystick rumble");
idCVar m_menu_sensitivity("m_menu_sensitivity","0.35",CVAR_FLOAT,
                         "mouse menu sensitivity used because using the same unscaled mouse view sensitivity is way to high");
idCVar com_captureSounds("com_captureSounds","0",CVAR_INTEGER,
                        "set to 1 to start capture, 2 while capturing, 0 when disabled");
idCVar com_captureFrames("com_captureFrames","0",CVAR_INTEGER,
                        "1 to capture each frame and output it");
idCVar com_demoPlaybackRate("com_demoPlaybackRate","0",CVAR_INTEGER,
                           "Rate to playback demos, 0 for realtime");
idCVar com_demoPlaybackScale("com_demoPlaybackScale","1",CVAR_FLOAT,
                            "Scales playback time of demos");
idCVar com_demoStats("com_demoStats","0",CVAR_BOOL,
                    "Print stats on demo compression rates");
idCVar net_drawDebugHud("net_drawDebugHud","0",CVAR_INTEGER|CVAR_ARCHIVE,
                       "Shows various network debug information");
idCVar com_debugSnapShotInterpolation("com_debugSnapShotInterpolation","0",CVAR_INTEGER,"");
idCVar plog_mode("plog_mode","0",CVAR_INTEGER,
                "global performance log mode, 0 = list, 1 = call graph");
idCVar plog_threshold("plog_threshold","0",CVAR_FLOAT,
                     "only shows the global log if the total time exceeds this threshold");
idCVar net_interpolationSlowdownStart("net_interpolationSlowdownStart","0.5",CVAR_FLOAT,
                                    "Scale interpolation rate when we fall behind");
idCVar com_sendSnapshots("com_sendSnapshots","1",CVAR_BOOL,
                        "For debugging, will not send snapshots when false.");
idCVar net_snapRate("net_snapRate","0",CVAR_INTEGER,"");
idCVar net_ForceWriteSnap("net_ForceWriteSnap","0",CVAR_BOOL,
                         "If true, server will create snapshots even if there are no peers (for debugging)");
idCVar net_ForceWriteSnapAckMS("net_ForceWriteSnapAckMS","0",CVAR_INTEGER,
                              "Ack net_ForceWriteSnap snapshots every x MS");
idCVar net_ForceWriteSnapAckSEQ("net_ForceWriteSnapAckSEQ","0",CVAR_INTEGER,
                               "Ack net_ForceWriteSnap snapshots this many seqs behind latest");
idCVar com_hitchThresholdMS("com_hitchThresholdMS","33",CVAR_INTEGER|CVAR_INIT,
    "sets com_drawThreadSpeeds to 2 if a frame takes longer than this time in milliseconds");
idCVar com_drawThreadSpeeds("com_drawThreadSpeeds","0",CVAR_INTEGER|CVAR_INIT,
    "1 = dynamic draw, 2 = static draw, 3 = dynamic draw until stall",0.0f,3.0f);
idCVar forge_logPerformanceMetrics("forge_logPerformanceMetrics","0",CVAR_BOOL,
    "Enable logging of performance metrics (renderer, frame timing, network etc.) to Forge.");
extern idCVar com_speeds;

idGaugeMetric framesPerSec(idStr("framesPerSec"),idStr("frames per second"));
idGaugeMetric timeSpinning(idStr("timeSpinning"),
    idStr("main thread time spent waiting, when com_syncToTime is set (PC only), in microseconds"));
idGaugeMetric timeCPUFrameTime(idStr("timeCPUFrameTime"),
    idStr("CPU time spent this frame, in microseconds"));
idGaugeMetric timeGameFrame(idStr("timeGameFrame"),
    idStr("time spent advancing game frames, in microseconds"));
idGaugeMetric timeRenderThreadSync(idStr("timeRenderThreadSync"),
    idStr("time spent waiting for the renderer thread, in microseconds"));
idGaugeMetric timeRenderFrame(idStr("timeRenderFrame"),
    idStr("time spent in the renderer thread generating command buffers"));
idGaugeMetric timeGPUFrameTime(idStr("timeGPUFrameTime"),
    idStr("time spent by the GPU rendering the scene"));
idGaugeMetric countNumGameFrames(idStr("countNumGameFrames"),
    idStr("number of game frames executed this frame"));

gameFrameParms_t::gameFrameParms_t()
    : renderWidth(1280),renderHeight(720),pixelAspect(1.0f),isServer(true),
      isClient(true),isHeadlessServer(false),isDemoPlaying(false),
      isDemoRecording(false),isToolActive(false),advanceTime(true),
      editorRequestsStopTime(false),recordState(RECORD_NONE),
      masterLocalUser(nullptr),regression(nullptr),userCmdMgr(nullptr),
      currentEditorReferenceMapID() {}

gameFrameParms_t::gameFrameParms_t(const gameFrameParms_t& other)
    : renderWidth(other.renderWidth),renderHeight(other.renderHeight),
      pixelAspect(other.pixelAspect),isServer(other.isServer),
      isClient(other.isClient),isHeadlessServer(other.isHeadlessServer),
      isDemoPlaying(other.isDemoPlaying),
      isDemoRecording(other.isDemoRecording),isToolActive(other.isToolActive),
      advanceTime(other.advanceTime),
      editorRequestsStopTime(other.editorRequestsStopTime),
      recordState(other.recordState),masterLocalUser(other.masterLocalUser),
      regression(other.regression),userCmdMgr(other.userCmdMgr),
      currentEditorReferenceMapID(other.currentEditorReferenceMapID) {}

gameFrameParms_t::~gameFrameParms_t()=default;

void idCommonLocal::PurgeOverlays() {
  if(!Field<bool>(0x34721)) Field<bool>(0x34720)=true;
}

void idCommonLocal::ShowJobListTimeRanges(std::uint64_t displayStartTime) {
  if(com_drawThreadSpeeds.valueInteger==0) return;
  const int numLists=parallelJobManager->GetNumJobLists();
  if(numLists==0) return;
  const int numUnits=parallelJobManager->GetNumProcessingUnits();
  for(int unit=0;unit<numUnits && unit<=31;++unit) {
    std::uint64_t start=displayStartTime;
    std::uint64_t end=displayStartTime;
    std::uint64_t wasted=0;
    for(int listIndex=0;listIndex<numLists;++listIndex) {
      idParallelJobList* list=parallelJobManager->GetJobList(listIndex);
      end+=list->GetUnitProcessingTimeMicroSec(unit);
      end+=list->GetUnitWastedTimeMicroSec(unit);
      console->AddTimeRange(static_cast<rangeId_t>(unit+16),*list->color,start,end);
      start=end;
      wasted+=list->GetUnitWastedTimeMicroSec(unit);
    }
    const std::uint64_t duration=end-displayStartTime;
    const std::uint64_t percent=duration!=0 ? 100*wasted/duration : 0;
    console->SetTimeRangeLabel(static_cast<rangeId_t>(unit+16),
        va("%1.1f, %lld %%",static_cast<float>(wasted)/1000.0f,percent));
  }
  for(int range=numUnits+16;range<=31;++range)
    console->SetTimeRangeLabel(static_cast<rangeId_t>(range),"");
}

void idCommonLocal::CalcPerfTimings() {
  static std::uint64_t prevTime=Sys_Microseconds();
  const std::uint64_t now=Sys_Microseconds();
  time_frame=now-prevTime;
  prevTime=now;
  avgFrameTime+=(static_cast<float>(time_frame)-avgFrameTime)*0.2f;
  time_FPS=static_cast<int>(1000000.0f/avgFrameTime+0.5f);
  if(com_drawThreadSpeeds.valueInteger==3 &&
     time_frame>static_cast<std::uint64_t>(1000*com_hitchThresholdMS.valueInteger))
    com_drawThreadSpeeds.SetInteger(2,true);
  int speeds=com_speeds.valueInteger;
  if(speeds==1) {
    idLib::Printf("%i: all:%4.1f spin:%4.1f game:%4.1f/%i sync:%4.1f rfr:%4.1f GPU:%4.1f\n",
      idLib::frameNumber,static_cast<float>(time_frame)*0.001f,
      static_cast<float>(time_spinning)*0.001f,
      static_cast<float>(time_gameFrame)*0.001f,count_numGameFrames,
      static_cast<float>(time_renderThreadSync)*0.001f,
      static_cast<float>(time_renderFrame)*0.001f,
      static_cast<float>(time_GPUFrameTime)*0.001f);
    speeds=com_speeds.valueInteger;
  }
  if(speeds!=0 || forge_logPerformanceMetrics.valueInteger!=0) {
    countNumGameFrames.Log(static_cast<float>(count_numGameFrames),false);
    timeSpinning.Log(static_cast<float>(time_spinning),false);
    timeCPUFrameTime.Log(static_cast<float>(time_frame-time_spinning),false);
    timeGameFrame.Log(static_cast<float>(time_gameFrame),false);
    timeRenderThreadSync.Log(static_cast<float>(time_renderThreadSync),false);
    timeRenderFrame.Log(static_cast<float>(time_renderFrame),false);
    timeGPUFrameTime.Log(static_cast<float>(time_GPUFrameTime),false);
    framesPerSec.Log(static_cast<float>(time_FPS),false);
  }
}

void idUserCmdMgr::PutUserCmdForPlayer(int playerIndex,
                                       const usercmd_t& command) {
  std::memmove(&cmdBuffer[writeFrame[playerIndex]%128][playerIndex],
               &command,sizeof(command));
  if(writeFrame[playerIndex]-readFrame[playerIndex]+1>128) {
    readFrame[playerIndex]=writeFrame[playerIndex]-64;
    idLib::Printf("PutUserCmdForPlayer: buffer overflow.\n");
  }
  ++writeFrame[playerIndex];
}

idLocalUser* idSignInManagerBase::GetMasterLocalUser() {
  return GetNumLocalUsers()>0 ? GetLocalUserByIndex(0) : nullptr;
}

int idCommonLocal::CalcSnapTimeBuffered(int& totalBufferedTime,
                                        int& totalRecvTime) {
  totalBufferedTime=static_cast<int>(Field<std::uint32_t>(0x728));
  totalRecvTime=Field<int>(0x740);
  int readIndex=Field<int>(0x34730);
  int time=Field<idSnapShot>(0x34C58).GetTime();
  int recvTime=Field<idSnapShot>(0x34C58).GetRecvTime();
  while(readIndex<Field<int>(0x34734)) {
    const idSnapShot& snap=Field<idSnapShot>(
        0x34738+(readIndex%16)*sizeof(idSnapShot));
    ++readIndex;
    totalBufferedTime+=snap.GetTime()-time;
    time=snap.GetTime();
    totalRecvTime+=snap.GetRecvTime()-recvTime;
    recvTime=snap.GetRecvTime();
  }
  if(totalRecvTime<1) totalRecvTime=1;
  totalRecvTime=static_cast<int>(
      static_cast<float>(Game()->GetGameMsPerRealSec())*
      (static_cast<float>(totalRecvTime)/1000.0f));
  int currentTime=static_cast<int>(Field<std::uint64_t>(0x750));
  const int snapRate=static_cast<int>(Field<std::uint32_t>(0x728));
  if(snapRate<currentTime) currentTime=snapRate;
  return totalBufferedTime-currentTime;
}

gameFrameParms_t idCommonLocal::GameFrameParms(int gameFrameFlags) {
  gameFrameParms_t result;
  result.renderWidth=renderSystem->GetWidth();
  result.renderHeight=renderSystem->GetHeight();
  result.pixelAspect=renderSystem->GetPixelAspect();
  result.isServer=common->IsServer();
  result.isClient=common->IsClient();
  result.isHeadlessServer=net_headlessServer.valueInteger!=0;
  result.isDemoPlaying=common->IsDemoPlaying();
  result.isDemoRecording=common->IsDemoRecording();
  result.isToolActive=common->IsToolActive();
  result.editorRequestsStopTime=false;
  result.advanceTime=(gameFrameFlags&1)!=0;
  result.recordState=common->GetRecordState();
  result.masterLocalUser=common->GetMasterLocalUser();
  result.regression=common->GetRegression();
  result.userCmdMgr=&Field<idUserCmdMgr>(0x34D68);
  result.currentEditorReferenceMapID="";
  GetToolReferenceMap(static_cast<toolFlag_t>(com_editors),
                      result.currentEditorReferenceMapID);
  return result;
}

void idGameSpawnInfo::MakeNamesCanonical() {
  mapName.MakeNameCanonical();
  mapName.StripFileExtension();
  for(int i=0;i<activeLayers.Num();++i) activeLayers[i].MakeNameCanonical();
  for(int i=0;i<deactiveLayers.Num();++i) deactiveLayers[i].MakeNameCanonical();
}

void idGameSpawnInfo::FixupFeatureLayers() {
  if(mapName.IsEmpty() || static_cast<signed char>(gameFlags)<0) return;
  for(int i=0;i<activeLayers.Num();++i) {
    if(activeLayers[i].Icmp("layer/tungsten/nextgen")==0 ||
       activeLayers[i].Icmp("layer/tungsten/oldgen")==0) {
      activeLayers.RemoveIndex(i--);
    }
  }
  idStrStatic<128> layer(feature_hq.valueInteger!=0
                             ? "layer/tungsten/nextgen"
                             : "layer/tungsten/oldgen");
  activeLayers.Append(layer);
}

void idGameSpawnInfo::ReadLayersFromCommaSeparatedStrings(
    const char* activeString,const char* deactiveString) {
  auto read=[](idStaticList<idStrStatic<128>,256>& layers,const char* text) {
    layers.ClearFree();
    if(idStr::Length(text)<=0) return;
    idStr copy(text);
    int start=0;
    for(int i=0;copy[i]!='\0';++i) {
      if(copy[i]==',') {
        if(start!=i) {
          copy[i]='\0';
          layers.Append(idStrStatic<128>(&copy[start]));
        }
        start=i+1;
      }
    }
    if(copy[start]!='\0') layers.Append(idStrStatic<128>(&copy[start]));
  };
  read(activeLayers,activeString);
  read(deactiveLayers,deactiveString);
  for(int i=0;i<activeLayers.Num();++i) activeLayers[i].Strip(' ');
  for(int i=0;i<deactiveLayers.Num();++i) deactiveLayers[i].Strip(' ');
  FixupFeatureLayers();
}

idGameSpawnInfo::idGameSpawnInfo(const idMatchParameters* matchParms,
    int gameFlags_,const char* spawnSpotOverride_,const char* checkpointName_,
    int numPlayers_,int gameSpawnFlags_)
    : gameFlags(gameFlags_),mapName(matchParms->mapName),
      spawnSpot(matchParms->spawnSpot),spawnSpotOverride(spawnSpotOverride_),
      checkpointName(checkpointName_),loadingScreen(),activeLayers(),
      deactiveLayers(),comboMap(matchParms->comboMap),devmap(matchParms->devmap),
      forceHeapReset(matchParms->forceHeapReset),
      gameSpawnFlags(gameSpawnFlags_),numPlayers(numPlayers_),
      currentCheckpointSlot(matchParms->startCheckSlot) {
  ReadLayersFromCommaSeparatedStrings(matchParms->layersActive.c_str(),
                                       matchParms->layersDeactive.c_str());
  MakeNamesCanonical();
}

idGameSpawnInfo::~idGameSpawnInfo()=default;

gameReturn_t& gameReturn_t::operator=(const gameReturn_t& other) {
  localTime=other.localTime;
  serverTime=other.serverTime;
  forceOneView=other.forceOneView;
  initTools=other.initTools;
  updateTools=other.updateTools;
  for(int i=0;i<16;++i) players[i]=other.players[i];
  static_cast<idList<idRenderModelGui*,5>&>(debugGuis)=
      static_cast<const idList<idRenderModelGui*,5>&>(other.debugGuis);
  std::memcpy(debugGuis.staticList,other.debugGuis.staticList,
              sizeof(debugGuis.staticList));
  return *this;
}

void idCommonLocal::PrepareSnapshot(idSnapShot& snapshot,
                                    gameReturn_t& gameReturn,
                                    idSnapShot* compareSnap) {
  snapshot.SetTime(gameReturn.serverTime);
  console->ClearSnapshots();
  for(int i=0;i<16;++i) {
    gameReturnPlayer_t& player=gameReturn.players[i];
    if(!player.valid) continue;
    std::uint8_t buffer[2048];
    idBitMsg msg;
    msg.InitWrite(buffer,sizeof(buffer));
    idSerializer serializer;
    serializer.msg=&msg;
    serializer.writing=true;
    SerializePlayer(serializer,player);
    snapshot.S_AddObject(i,player.visMask,msg);
    if(com_drawSnapshots.valueInteger==3) {
      console->AddSnapObject("players",msg.GetSize(),
          snapshot.CompareObject(compareSnap,i,0,0,0),0.0f);
    }
  }
  Game()->WriteToSnapshot(snapshot,compareSnap);
  Game()->GetRenderWorld()->WriteToSnapshot(snapshot,compareSnap);
  Game()->GetSoundWorld()->WriteToSnapshot(snapshot,compareSnap);
}

void idCommonLocal::RunNetworkSnapshotFrame(gameReturn_t& gameReturn) {
  static int clientTimeResidual=0;
  static std::int64_t lastTime=Sys_Milliseconds();
  auto invalidFloat=[](float value) {
    std::uint32_t bits;
    std::memcpy(&bits,&value,sizeof(bits));
    return value!=value || ((bits&0x7F800000u)==0 && (bits&0x7FFFFFu)!=0);
  };

  const std::uint64_t frameStart=Sys_Microseconds();
  time_gameFrame=static_cast<int>(frameStart);
  time_maxGameFrame=0;
  count_numGameFrames=0;
  Game()->GetRenderWorld()->UpdateDebugTime(
      static_cast<unsigned>(Game()->GetGameMsPerFrame(0)));
  if(SnapPrevious().serverTime<0) {
    gameReturn=SnapCurrent();
  } else {
    const int clientStep=static_cast<int>(
        static_cast<float>(Game()->GetGameMsPerFrame(0))/com_timescale.valueFloat)+1;
    const int now=static_cast<int>(Sys_Milliseconds());
    const int rawDelta=now-static_cast<int>(lastTime);
    const int scaledDelta=std::max(1,std::min(rawDelta,33));
    clientTimeResidual+=std::max(0,std::min(rawDelta,50));
    lastTime=now;
    if(com_fixedTic.valueInteger!=0) clientTimeResidual=0;
    do {
      while((Field<std::uint64_t>(0x750)>=Field<std::uint64_t>(0x728) ||
             com_forceLatestSnap.valueInteger!=0) &&
            Field<int>(0x34730)<Field<int>(0x34734)) {
        Field<std::uint64_t>(0x750)-=Field<std::uint64_t>(0x728);
        ProcessNextSnapshot();
      }
      Game()->GetRenderWorld()->AdvanceTime();
      const int rate=static_cast<int>(Field<std::uint64_t>(0x728));
      int current=static_cast<int>(Field<std::uint64_t>(0x750));
      current=std::max(0,std::min(current,rate+std::min(rate,
                                      net_maxExtrapolationInMS.valueInteger)));
      Field<std::uint64_t>(0x750)=static_cast<std::uint64_t>(current);
      if(Field<std::uint64_t>(0x728)==0) {
        idLib::Warning("snapRate <= 0. Resetting to 100");
        Field<std::uint64_t>(0x728)=100;
      }
      float fraction=static_cast<float>(Field<std::uint64_t>(0x750))/
                     static_cast<float>(Field<std::uint64_t>(0x728));
      if(invalidFloat(fraction)) {
        idLib::Warning("Interpolation Fraction invalid: snapCurrentTime %d / snapRate %d",
                       static_cast<int>(Field<std::uint64_t>(0x750)),
                       static_cast<int>(Field<std::uint64_t>(0x728)));
        fraction=0.0f;
      }
      InterpolateSnapshot(gameReturn,SnapPrevious(),SnapCurrent(),fraction,false);
      float interpolationRate=net_interpolationBaseRate.valueFloat;
      Field<float>(0x34664)=static_cast<float>(CalcSnapTimeBuffered(
          Field<int>(0x3466C),Field<int>(0x34670)));
      Field<float>(0x34668)=static_cast<float>(Field<int>(0x3466C))/
                           static_cast<float>(Field<int>(0x34670));
      if(net_minBufferedSnapPCT_Static.valueFloat>0.0f)
        Field<float>(0x34C38)=session->GetTitleStorageFloat(
            "net_minBufferedSnapPCT_Static",
            net_minBufferedSnapPCT_Static.valueFloat);
      if(net_optimalDynamic.valueInteger!=0) {
        int optimal=static_cast<int>(static_cast<float>(Field<std::uint64_t>(0x728))*
                                     Field<float>(0x34C38));
        optimal=std::max(0,std::min(optimal,net_maxBufferedSnapMS.valueInteger));
        Field<float>(0x34C3C)=static_cast<float>(optimal);
        Field<float>(0x34C40)=static_cast<float>(Field<std::uint64_t>(0x728))*
                              net_minBufferedSnapWinPCT_Static.valueFloat;
      } else {
        Field<float>(0x34C3C)=net_optimalSnapTime.valueFloat;
        Field<float>(0x34C40)=net_optimalSnapWindow.valueFloat;
      }
      if(Field<float>(0x34664)>Field<float>(0x34C3C)) {
        if(Field<float>(0x34664)>Field<float>(0x34C3C)+Field<float>(0x34C40))
          interpolationRate=net_interpolationCatchupRate.valueFloat;
      } else if(1.1754944e-38f<Field<float>(0x34664)) {
        interpolationRate=net_interpolationFallbackRate.valueFloat;
      } else {
        interpolationRate=0.0f;
      }
      float advance=static_cast<float>(Game()->GetGameMsPerFrame(0))*interpolationRate;
      if(net_effectiveSnapRateEnable.valueInteger!=0) {
        const float realAdvance=static_cast<float>(Game()->GetGameMsPerRealSec())*
                                (static_cast<float>(scaledDelta)/1000.0f);
        advance=realAdvance*interpolationRate*Field<float>(0x34668)+
                Field<float>(0x34660);
        if(invalidFloat(advance)) advance=0.0f;
        Field<float>(0x34660)=advance-std::floor(advance);
        if(invalidFloat(Field<float>(0x34660))) Field<float>(0x34660)=0.0f;
        if(net_effectiveSnapRateDebug.valueInteger!=0)
          idLib::Printf("%d/%.2f snapRateScale: %.2f effectiveSR: %.2f d.interp: %.2f snapTimeBuffered: %.2f res: %.2f\n",
              scaledDelta,realAdvance,interpolationRate,Field<float>(0x34668),
              advance,Field<float>(0x34664),Field<float>(0x34660));
      }
      Field<std::uint64_t>(0x750)+=static_cast<int>(advance);
      clientTimeResidual-=clientStep;
    } while(clientTimeResidual>=clientStep);
    if(clientTimeResidual<0) clientTimeResidual=0;
  }
  time_gameFrame=static_cast<int>(Sys_Microseconds()-frameStart);
}

void idCommonLocal::RunNormalFrame(gameReturn_t& gameReturn) {
  static std::int64_t normalGameTimeResidual=0;
  static std::int64_t sampleGameTimeResidual=0;
  static std::int64_t lastSampleTime=0;
  static int lastGameFrame=0;
  static std::int64_t lastNormalTime=0;
  static bool normalTimeInitialized=false;
  static alignas(16) std::uint8_t objectMemory[0x20000];
  static lzwCompressionData_t lzwData;
  static int lastAckMS=0;

  const int gameHz=Game()->GetGameHz(1);
  const std::int64_t sampleClock=soundSystem->GetSampleClock();
  const bool useSampleClock=com_useSampleClock.valueInteger!=0 && sampleClock>0;
  std::int64_t* residual=&normalGameTimeResidual;
  std::int64_t quantum;
  int elapsed;
  if(useSampleClock) {
    residual=&sampleGameTimeResidual;
    quantum=soundSystem->GetSampleRate();
    const std::int64_t product=sampleClock*gameHz;
    if(Game()->GetGameFrame()<lastGameFrame) lastSampleTime=product;
    lastGameFrame=Game()->GetGameFrame();
    elapsed=static_cast<int>(product-lastSampleTime);
    elapsed=std::max(0,std::min(elapsed,static_cast<int>(3*quantum)));
    lastSampleTime=product;
  } else {
    quantum=static_cast<int>(1000.0f/com_timescale.valueFloat);
    const std::int64_t now=static_cast<std::int32_t>(
        static_cast<std::uint32_t>(gameHz)*
        static_cast<std::uint32_t>(Sys_Milliseconds()));
    if(!normalTimeInitialized) { lastNormalTime=now;normalTimeInitialized=true; }
    elapsed=static_cast<int>(std::max<std::int64_t>(0,
                    std::min<std::int64_t>(now-lastNormalTime,3000)));
    lastNormalTime=now;
  }
  *residual+=elapsed;
  if(com_fixedTic.valueInteger!=0) *residual=0;
  time_gameFrame=0;
  time_maxGameFrame=0;
  count_numGameFrames=0;
  do {
    const std::uint64_t start=Sys_Microseconds();
    Game()->GetRenderWorld()->AdvanceTime();
    bool soundPaused=false;
    if(!Game()->GetGamePaused(&soundPaused)) Game()->GetSoundWorld()->AdvanceTime();
    Game()->RunFrame(common->GameFrameParms(1),&gameReturn);
    if(gameReturn.initTools!=0) {
      std::uint32_t bit=1;
      for(int i=0;i<32;++i,bit=(bit<<1)|(bit>>31))
        if((bit&gameReturn.initTools)!=0) common->InitTool(static_cast<toolFlag_t>(bit));
    }
    if(gameReturn.updateTools!=0) {
      std::uint32_t bit=1;
      for(int i=0;i<32;++i,bit=(bit<<1)|(bit>>31))
        if((bit&gameReturn.updateTools)!=0) common->UpdateTool(static_cast<toolFlag_t>(bit));
    }
    if(com_sleepGame.valueInteger>0)
      Sys_Sleep(static_cast<unsigned long>(com_sleepGame.valueInteger));
    const int duration=static_cast<int>(Sys_Microseconds()-start);
    time_gameFrame+=duration;
    time_maxGameFrame=std::max(time_maxGameFrame,duration);
    ++count_numGameFrames;
    *residual-=quantum;
  } while(*residual>=quantum);

  const int demoMode=Field<int>(0x6D0);
  const bool recording=demoMode==1 || (demoMode==4 && Field<void*>(0x710)!=nullptr);
  if(!recording && (net_ForceWriteSnap.valueInteger==0 || session->GetState()!=7)) {
    if(!session->GetActingGameStateLobbyBase().IsHost() || session->GetState()!=7 ||
       !session->GetActingGameStateLobbyBase().HasActivePeers()) return;
  }
  idSnapShot* compare=nullptr;
  if(recording) {
    compare=Field<idSnapShot*>(0x718);
  } else if((session->GetActingGameStateLobbyBase().IsHost() &&
             session->GetActingGameStateLobbyBase().HasActivePeers()) ||
            net_ForceWriteSnap.valueInteger!=0) {
    compare=&OldSnapshot();
  }
  const std::int64_t currentTime=static_cast<std::int32_t>(
      static_cast<std::uint32_t>(gameReturn.serverTime)*1000u);
  if(Field<std::int64_t>(0x748)==0) Field<std::int64_t>(0x748)=currentTime;
  std::int64_t delta=currentTime-Field<std::int64_t>(0x748);
  if(net_snapRate.valueInteger>0)
    Field<std::int64_t>(0x728)=net_snapRate.valueInteger;
  delta=std::min(delta,2*Field<std::int64_t>(0x728));
  if(delta>=Field<std::int64_t>(0x728)) {
    if(net_debug_snapShotTime.valueInteger!=0)
      idLib::Printf("^5Creating snapshot. Delta real time: %d  Delta ss time: %d. SnapTime: %d\n",
          static_cast<int>(delta/1000),gameReturn.serverTime-compare->GetTime(),
          gameReturn.serverTime);
    Field<std::int64_t>(0x748)=currentTime;
    idSnapShot snapshot;
    PrepareSnapshot(snapshot,gameReturn,compare);
    if(session->GetActingGameStateLobbyBase().IsHost()) {
      if(com_sendSnapshots.valueInteger!=0) session->SendSnapshot(snapshot);
      if(net_ForceWriteSnap.valueInteger!=0 &&
         !session->GetActingGameStateLobbyBase().HasActivePeers()) {
        std::uint8_t buffer[8000];
        idSnapshotProcessor* snapProcStorage;
        {
          scopedGlobalHeap_t heap(HEAP_SYSTEMHEAP);
          static idSnapshotProcessor processor;
          snapProcStorage=&processor;
        }
        idSnapshotProcessor& snapProc=*snapProcStorage;
        snapProc.TrySetPendingSnapshot(snapshot);
        if(snapProc.HasPendingSnap())
          snapProc.SubmitPendingSnap(1,objectMemory,sizeof(objectMemory),
                                     &lzwData,nullptr);
        const unsigned size=snapProc.GetPendingSnapDelta(buffer,7872);
        const int sequence=snapProc.LastDeltaSequence();
        snapshotMetrics.LogSnapshotDelta(snapshot,&OldSnapshot(),sequence,
            snapProc.BaseSequence(),0,static_cast<int>(size),
            !snapProc.HasPendingSnap());
        const int now=static_cast<int>(Sys_Milliseconds());
        if(now-lastAckMS>=net_ForceWriteSnapAckMS.valueInteger &&
           net_ForceWriteSnapAckMS.valueInteger>=0) {
          lastAckMS=now;
          const int ackSequence=std::max(snapProc.BaseSequence(),
              sequence-net_ForceWriteSnapAckSEQ.valueInteger);
          snapProc.ApplySnapshotDelta(1,ackSequence);
        }
      }
    }
    OldSnapshot()=snapshot;
  }
}

void idCommonLocal::Frame() try {
  static float msecResidual=0.0f;
  static int lastTime=static_cast<int>(Sys_Milliseconds());
  static const idColor colorDarkRed{0.5f,0.0f,0.0f,1.0f};

  console->BeginTimeRange(RANGE_MAIN,idColor::colorBlue);
  console->BeginTimeRange(RANGE_MAIN,colorDarkRed);
  soundSystem->WaitForSoundThread();
  console->BeginTimeRange(RANGE_MAIN,idColor::colorPurple);
  renderSystem->AllowBackgroundSwaps(false);
  streamingInstallMgr->Pump();
  console->BeginTimeRange(RANGE_MAIN,idColor::colorWhite);
  pLog.Clear();

  if(com_timescale.valueFloat<1.0f ||
     (com_syncToTime.valueInteger!=0 && r_swapInterval.valueInteger==0)) {
    const std::uint64_t start=Sys_Microseconds();
    const float frameTime=Game()->GetRealMsPerFrame(0)/com_timescale.valueFloat;
    int waitTime=static_cast<int>(frameTime);
    msecResidual+=frameTime-static_cast<float>(waitTime);
    if(msecResidual>=1.0f) { ++waitTime;msecResidual-=1.0f; }
    int now=static_cast<int>(Sys_Milliseconds());
    while(now-lastTime<waitTime) {
      session->Pump();
      Sys_Yield();
      now=static_cast<int>(Sys_Milliseconds());
    }
    lastTime=now;
    session->Pump();
    time_spinning=static_cast<int>(Sys_Microseconds()-start);
  } else {
    time_spinning=0;
  }

  if(session->GetState()==6) {
    renderSystem->AllowBackgroundSwaps(true);
    const idMatchParameters& parms=session->GetActingGameStateLobbyBase().GetMatchParms();
    const bool checkpointReload=parms.startCheckSlot>=0 && !parms.nextMapDiscSwap;
    idStr previousCheckpoint[16];
    bool checkpointDataValid[16]{};
    bool savePersistent[16]{};
    int persistentSize[16]{};
    if(checkpointReload) {
      Field<bool>(0x160)=true;
      {
        scopedGlobalHeap_t heap(HEAP_SYSTEMHEAP);
        for(int i=0;i<16;++i)
          checkpointDataValid[i]=Game()->CheckpointGetPlayerData(
              i,savePersistent[i],persistentSize[i],previousCheckpoint[i]);
      }
    }
    idGameSpawnInfo* spawnInfo=nullptr;
    {
      scopedGlobalHeap_t heap(HEAP_SYSTEMHEAP);
      const int gameFlags=(com_disableGameScripts.valueInteger!=0 ? 0 : 8) |
                          (com_forceAllMapLayersToLoad.valueInteger!=0 ? 0x80 : 0) |
                          0x11C;
      const bool noLoadingScreen=parms.loadingScreen.IsEmpty() &&
                                 (parms.startCheckSlot<0 || parms.playVideo);
      int layerFlag=8;
      if(!parms.layersActive.IsEmpty() || !parms.layersDeactive.IsEmpty()) layerFlag=0;
      void* memory=idMem::Singleton()->AllocWithLocation(
          "w:\\build\\tech5\\shared\\idlib\\Heap.h(95) : tag",
          sizeof(idGameSpawnInfo),static_cast<memTag_t>(77),false,
          ALIGN_16,HEAP_DEFAULTHEAP);
      if(memory!=nullptr) {
        try {
          spawnInfo=new(memory) idGameSpawnInfo(
              &parms,gameFlags,"","",
              session->GetActingGameStateLobbyBase().GetNumLobbyUsers(),
              layerFlag+(noLoadingScreen ? 1 : 2)+
                  (parms.isTransitionalMapRequest ? 0x10 : 0));
        } catch(...) {
          idMem::Singleton()->Free(memory,ALIGN_16);
          throw;
        }
      }
    }
    if(spawnInfo!=nullptr) {
      spawnInfo->ReadLayersFromCommaSeparatedStrings(parms.layersActive.c_str(),
                                                     parms.layersDeactive.c_str());
      try {
        ExecuteMapChange(*spawnInfo,nullptr);
      } catch(idNetworkLoadException*) {
        idLib::Printf("idNetworkLoadException: %s\n",idException::error);
        renderSystem->EndAutomaticBackgroundSwaps();
      }
    }
    if(Game()==nullptr) {
      console->BeginTimeRange(RANGE_MAIN,idColor::colorBlack);
      return;
    }
    if(checkpointReload) {
      for(int i=0;i<16;++i)
        if(checkpointDataValid[i]) Game()->CheckpointPlayerPostLoad(
            i,savePersistent[i],persistentSize[i],previousCheckpoint[i]);
      Field<bool>(0x160)=false;
    }
    if(spawnInfo!=nullptr) {
      spawnInfo->~idGameSpawnInfo();
      idMem::Singleton()->Free(spawnInfo,ALIGN_16);
    }
    renderSystem->AllowBackgroundSwaps(false);
  } else if(session->GetState()!=7 && Game()->IsGameActive(true)) {
    renderSystem->AllowBackgroundSwaps(true);
    if(session->GetState()==1) LeaveGame(true); else Disconnect(true);
    renderSystem->AllowBackgroundSwaps(false);
  }

  {
  idGameLock gameLock(Game());
  ShowJobListTimeRanges(Sys_Microseconds());
  CalcPerfTimings();
  const bool releaseMouse=console->IsActive() || com_editorActive.valueInteger!=0 ||
      (in_unlockMouseInMenus.valueInteger!=0 && Game()->Shell_ShouldUnlockMouse());
  if(releaseMouse) sys->Mouse()->GrabCursor(false);
  else if((com_editors&2)==0) sys->Mouse()->GrabCursor(true);

  const int routedPlayers=Game()->GenerateInputToPlayerIndexRouting();
  for(int device=0;device<4;++device) {
    const int player=Game()->GetPlayerIndexFromInputDevice(device);
    if(player<0) {
      if(session->GetState()<7) {
        inputSettings_t settings{};
        settings.mouseSensScale=1.0f;
        settings.joySensScale=1.0f;
        settings.joyPitchSpeed=joy_pitchSpeed.valueFloat;
        settings.joyYawSpeed=joy_yawSpeed.valueFloat;
        settings.mouseSens=m_sensitivity.valueFloat;
        settings.lookInvert=in_invertLook.valueInteger!=0;
        usercmdGen->GetUsercmd(device,settings,false,-1);
      }
      continue;
    }
    usercmd_t command=usercmdGen->GetUsercmd(device,
        Field<inputSettings_t>(0x34CC8+device*sizeof(inputSettings_t)),
        releaseMouse || Game()->IsPlayerControlInhibited(player),player);
    command.clientGameFrame=Game()->GetGameTimeManager()->GetGameFrame();
    command.serverGameTime=IsClient()
        ? std::min(Game()->GetSSEndTime(),Game()->GetServerGameTimeMs())
        : Game()->GetServerGameTimeMs();
    if(releaseMouse) command.buttons=0;
    UserCmdMgr().readFrame[player]=UserCmdMgr().writeFrame[player]-1;
    UserCmdMgr().PutUserCmdForPlayer(player,command);
  }
  if(routedPlayers==0 && session->GetState()==7 && !Game()->Shell_IsActive() &&
     net_headlessServer.valueInteger==0) Game()->Shell_Show(true);

  const bool systemUI=session->IsSystemUIShowing();
  const bool discSwapping=discSwapMgr!=nullptr &&
      *reinterpret_cast<const bool*>(
          reinterpret_cast<const std::byte*>(discSwapMgr)+0x870);
  if(!systemUI || Game()->Shell_IsActive() || !Game()->IsGameActive(true) ||
     discSwapping) {
    if(systemUI) idKeyInput::ClearStates();
  } else {
    idKeyInput::ClearStates();
    Game()->Shell_Show(true);
  }

  Sys_Microseconds();
  console->BeginTimeRange(RANGE_MAIN,idColor::colorYellow);
  Game()->UpdateFrameState();
  {
  gameReturn_t gameReturn;
  Game()->GetGameTimeManager()->UpdateTimeScale();
  static std::int64_t editorTimeResidual=0;
  static std::int64_t lastEditorTime=static_cast<std::int32_t>(
      static_cast<std::uint32_t>(Game()->GetGameHz(1))*
      static_cast<std::uint32_t>(Sys_Milliseconds()));
  const std::int64_t editorNow=static_cast<std::int32_t>(
      static_cast<std::uint32_t>(Game()->GetGameHz(1))*
      static_cast<std::uint32_t>(Sys_Milliseconds()));
  editorTimeResidual+=editorNow-lastEditorTime;
  lastEditorTime=editorNow;
  while(editorTimeResidual>=1000) {
    idSoundWorld* menuWorld=Game()->GetMenuSoundWorld();
    idSoundWorld* globalWorld=soundSystem->GetGlobalSoundWorld();
    if(globalWorld!=nullptr && !globalWorld->IsPaused()) globalWorld->AdvanceTime();
    if(menuWorld!=nullptr && !menuWorld->IsPaused()) menuWorld->AdvanceTime();
    editorTimeResidual-=1000;
  }
  const bool editorActive=com_editorActive.valueInteger!=0 && (com_editors&0x5004)!=0;
  if(session->GetActingGameStateLobbyBase().IsPeer())
    RunNetworkSnapshotFrame(gameReturn);
  else if(!editorActive && session->GetActingGameStateLobbyBase().IsHost())
    RunNormalFrame(gameReturn);
  if(g_companionApp!=nullptr) g_companionApp->Update(Game());
  for(int device=0;device<4;++device) {
    const int player=Game()->GetPlayerIndexFromInputDevice(device);
    if(player>=0)
      Field<inputSettings_t>(0x34CC8+device*sizeof(inputSettings_t))=
          gameReturn.players[player].inputSettings;
  }

  int activePlayers[4];
  int numActivePlayers=0;
  for(int device=0;device<4;++device) {
    const int player=Game()->GetPlayerIndexFromInputDevice(device);
    if(player>=0 && gameReturn.players[player].valid)
      activePlayers[numActivePlayers++]=player;
  }
  console->BeginTimeRange(RANGE_MAIN,idColor::colorOrange);
  if(Game()->Shell_IsActive()) {
    idRenderModelGui* gui=Game()->Shell_Render();
    if(renderSystem->BinkVideoIsPlaying()) gui=nullptr;
    renderManager.BuildGuiFrame(gui,false);
  } else {
    renderManager.BuildGameFrame(gameReturn,*Game(),renderSystem->FrameNumber());
  }
  renderSystem->AllowBackgroundSwaps(true);

  console->BeginTimeRange(RANGE_MAIN,idColor::colorPurple);
  const std::uint64_t soundStart=Sys_Microseconds();
  if(Sys_IsGameWindowActive() ||
     (!Sys_IsGameWindowActive() && win_pauseOnAltTab.valueInteger!=0) ||
     com_forceSound.valueInteger!=0 || (com_editors&0x10020080)!=0) {
    idSoundWorld* globalWorld=soundSystem->GetGlobalSoundWorld();
    idSoundWorld* gameWorld=Game()->GetSoundWorld();
    soundSystem->SetMute(false);
    bool pauseSound=true;
    const bool gamePaused=Game()->GetGamePaused(&pauseSound);
    const bool shellInactive=!Game()->Shell_IsActive();
    Sys_IsGameWindowActive();
    if(globalWorld!=nullptr) {
      if(shellInactive) globalWorld->UnPause(); else globalWorld->Pause(0);
    }
    if(numActivePlayers<=0 || gamePaused || Game()->GetGameMs(1)<250) {
      if(pauseSound) {
        gameWorld->Pause(0);
        idSoundWorld* menuWorld=Game()->GetMenuSoundWorld();
        menuWorld->SetNumListeners(1);
        menuWorld->PlaceListener(vec3_origin,mat3_identity,vec3_origin,-1);
        soundSystem->SetPlayingSoundWorld(menuWorld);
      } else {
        gameWorld->Pause(-50);
        soundSystem->SetPlayingSoundWorld(gameWorld);
      }
    } else {
      gameWorld->SetNumListeners(numActivePlayers);
      for(int i=0;i<numActivePlayers;++i) {
        const gameReturnPlayer_t& player=gameReturn.players[activePlayers[i]];
        gameWorld->PlaceListener(player.view.vieworg,player.view.viewaxis,
                                 player.velocity,player.view.viewID);
      }
      gameWorld->UnPause();
      soundSystem->SetPlayingSoundWorld(gameWorld);
    }
  } else {
    soundSystem->SetMute(true);
  }
  time_gameFrame+=static_cast<int>(Sys_Microseconds()-soundStart);

  console->BeginTimeRange(RANGE_MAIN,idColor::colorCyan);
  for(int device=0;device<4;++device) {
    int low=0,high=0,left=0,right=0;
    if(in_joystickRumble.valueInteger!=0 && !Game()->Shell_IsActive() &&
       !Game()->GetGamePaused(nullptr) && Game()->IsGameActive(false)) {
      const int player=Game()->GetPlayerIndexFromInputDevice(device);
      if(player>=0) {
        low=gameReturn.players[player].vibrationLow;
        high=gameReturn.players[player].vibrationHigh;
        left=gameReturn.players[player].vibrationLeft;
        right=gameReturn.players[player].vibrationRight;
      }
    }
    sys->Joystick()->SetRumble(device,low,high,left,right);
  }
  }

  StopLevelLoad(true);
  const int demoMode=Field<int>(0x6D0);
  if((demoMode!=4 || (Field<void*>(0x6E8)==nullptr && Field<void*>(0x710)==nullptr)) &&
     demoMode!=3) NetSendUsercmds();
  ++idLib::frameNumber;
  if(!sys->FPU_StackIsEmpty()) {
    idLib::Printf(sys->FPU_GetState());
    idLib::FatalError(
        "idCommon::Frame: the FPU stack is not empty at the end of the frame\n");
  }
  if(com_captureSounds.valueInteger==2) {
    com_captureSounds.SetInteger(3,true);
    Sys_SoundCaptureStart(com_capturePath.GetString());
    idLib::Printf("starting sound cap\n");
  }

  console->BeginTimeRange(RANGE_MAIN,idColor::colorRed);
  const std::uint64_t syncStart=Sys_Microseconds();
  renderSystem->SyncRenderThread(false);
  time_renderThreadSync=static_cast<int>((Sys_Microseconds()-syncStart)/1000);
  static const idColor colorBrown{0.4f,0.35f,0.08f,1.0f};
  console->BeginTimeRange(RANGE_MAIN,colorBrown);
  session->Pump();
  session->ProcessSnapAckQueue();
  common->SetRefreshOnPrint(false);
  renderSystem->InhibitEndFrameRendering(static_cast<int>(0x80000000u));

  idPlayerProfile* profile=common!=nullptr
      ? common->GetProfileFromMasterLocalUser() : nullptr;
  bool wroteConfig=false;
  if(profile!=nullptr && common->GetMasterLocalUser()!=nullptr &&
     (cvarSystem->GetModifiedFlags()&CVAR_ARCHIVE)!=0) {
    cvarSystem->ClearModifiedFlags(CVAR_ARCHIVE);
    WriteConfigToFile("wolfConfig.cfg");
    wroteConfig=true;
  }
  bool displayModeChanged=false;
  if(profile!=nullptr && profile->UpdateDisplayModeFromCvars()) {
    displayModeChanged=true;
    profile->CommitDisplayChanges();
  }
  if(profile!=nullptr && (displayModeChanged || wroteConfig)) {
    profile->SaveSettings(true);
  }
  {
    sys->GenerateEvents();
    if(runOrbisCommands) {
      cmdSystem->AppendCommandText("exec orbis.cfg\n");
      runOrbisCommands=false;
    }
    Field<bool>(0x162)=true;
    for(;;) {
      cmdSystem->ExecuteCommandBuffer();
      sysEvent_t event=sys->GetEvent();
      idPlayerProfile* eventProfile=nullptr;
      if(session->GetProfileFromMasterLocalUser()!=nullptr)
        eventProfile=common->GetProfileFromMasterLocalUser();
      if(eventProfile!=nullptr && event.evType==SE_MOUSE &&
         event.evValue!=294 && event.evValue!=295) {
        const float x=static_cast<float>(event.evValue)*
                          eventProfile->GetMouseSensitivity()*
                          m_menu_sensitivity.valueFloat+eventProfile->mouseXFrac;
        const float y=static_cast<float>(event.evValue2)*
                          eventProfile->GetMouseSensitivity()*
                          m_menu_sensitivity.valueFloat+eventProfile->mouseXFrac;
        event.evValue=static_cast<int>(std::floor(x));
        event.evValue2=static_cast<int>(std::floor(y));
        eventProfile->mouseXFrac=x-static_cast<float>(event.evValue);
        eventProfile->mouseYFrac=y-static_cast<float>(event.evValue2);
      }
      if(Field<void*>(0x1E0)!=nullptr) {
        if(Field<int>(0x1E8)==RECORD_MAKE) WriteSysEvent(event);
        else if(event.evType==SE_KEY && event.evValue==88)
          RecordPlayback(false,nullptr);
        else ReadSysEvent(event);
      }
      if(event.evType==SE_NONE) {
        if((sys_lang.flags&CVAR_MODIFIED)!=0) {
          sys_lang.flags&=~CVAR_MODIFIED;
          InitLanguageDict();
          idFont::resourceList.ReloadStaleResources();
        }
        if(com_skipSignInManager.valueInteger==0) session->UpdateSignInManager();
        if(Game()!=nullptr && Game()->IsUsingClientSnapshots()) {
          Game()->ClientReadSnapshot();
          Game()->ClientApplySnapshot();
        }
        session->GetSaveGameManager()->Pump();
        Field<bool>(0x162)=false;
        debugHUD->Frame();
        console->BeginTimeRange(RANGE_MAIN,idColor::colorPurple);
        soundSystem->Render(true);
        static const idColor colorMdGrey{0.5f,0.5f,0.5f,1.0f};
        console->BeginTimeRange(RANGE_MAIN,colorMdGrey);
        if(Field<int>(0x5ADE8)>0 || (com_editors&0x400000)!=0)
          idRenderModelGui::StartFrame();
        else
          renderManager.RenderFrame();
        if(com_captureFrames.valueInteger==2) {
          idCmdArgs args;
          args.AppendArg("screenshot");
          if(com_captureTGA.valueInteger==1) {
            args.AppendArg("tga");
            args.AppendArg("loadtextures");
          } else if(com_captureTGA.valueInteger==2) {
            args.AppendArg("png");
            args.AppendArg("loadtextures");
          }
          int captures=static_cast<int>(1.0f/com_timescale.valueFloat);
          if(captures==0) captures=1;
          for(;;) {
            const int previous=captures--;
            if(previous==0) break;
            R_ScreenShot_f(args);
          }
        }
        if(Field<bool>(0x34720) && !IsDialogActive()) {
          Field<bool>(0x34720)=false;
          globalImages->PurgeAllOverlays();
        }
        if(!IsDialogActive() &&
           (!Game()->GetGamePaused(nullptr) || !Game()->IsGameActive(true)))
          idSWF::PurgeUnusedImages();
        if(pLog.GroupMask()!=0) {
          if(plog_mode.valueInteger==0)
            pLog.ShowUniqueEntries(plog_threshold.valueFloat,0);
          else if(plog_mode.valueInteger==1)
            pLog.ShowCallGraph(plog_threshold.valueFloat,0);
        }
        if(Field<bool>(0x34685)) Shutdown(Field<int>(0x34688));
        break;
      }
      if(event.evType==SE_KEY)
        idKeyInput::PreliminaryKeyEvent(event.inputDevice,
            static_cast<keyNum_t>(event.evValue),(event.evValue2&1)!=0);
      if(event.evType==SE_CONSOLE) {
        cmdSystem->AppendCommandText(static_cast<const char*>(event.evPtr));
        cmdSystem->AppendCommandText("\n");
      } else {
        bool handled=console->ProcessEvent(&event,false);
        if(!handled && !IsDialogActive()) handled=session->ProcessInputEvent(event);
        if(!handled) {
          idSignInManagerBase& signIn=session->GetSignInManager();
          idLocalUser* master=signIn.GetNumLocalUsers()>0
              ? signIn.GetLocalUserByIndex(0) : nullptr;
          idLocalUser* eventUser=signIn.GetLocalUserByInputDevice(event.inputDevice);
          if(eventUser==nullptr)
            eventUser=signIn.GetRegisteringUserByInputDevice(event.inputDevice);
          const bool authorized=master!=nullptr ? eventUser!=nullptr
              : (eventUser!=nullptr || signIn.GetRegisteringUser()==nullptr);
          if(authorized && !Game()->Shell_IgnoreSplitScreenEvent(event)) {
            Field<int>(0x34690)=event.inputDevice;
            handled=session->ProcessInputEvent(event);
            if(!handled) handled=HandleDialogEvent(&event);
            if(!handled) handled=Game()->Shell_HandleGuiEvent(event);
            if(!handled) {
              const int player=Game()->GetPlayerIndexFromInputDevice(event.inputDevice);
              if(player<0 || !Game()->HandleGuiEvent(event,player)) {
                if(event.evType==SE_KEY && (event.evValue2&1)!=0 &&
                   !idKeyInput::ExecKeyBinding(event.inputDevice,
                      Field<inputSettings_t>(0x34CC8+
                          event.inputDevice*sizeof(inputSettings_t)).bindset,
                      static_cast<keyNum_t>(event.evValue),
                      static_cast<std::uint8_t>(event.evValue2>>1))) {
                  idKeyInput::ExecKeyBinding(event.inputDevice,BIND_DEFAULT,
                      static_cast<keyNum_t>(event.evValue),
                      static_cast<std::uint8_t>(event.evValue2>>1));
                }
              }
            }
          }
        }
      }
      if(event.evPtr!=nullptr) idMem::Singleton()->Free(event.evPtr,ALIGN_16);
    }
  }
  }
  console->BeginTimeRange(RANGE_MAIN,idColor::colorBlack);
} catch(idFatalException&) {
  throw;
} catch(idException&) {
  fileSystem->SetFilePreCallbackRetail(nullptr);
  const bool wasInLobby=IsMultiplayer() && session!=nullptr &&
      session->GetState()>=6;
  renderSystem->EndAutomaticBackgroundSwaps();
  SetRefreshOnPrint(false);
  if(idLib::headless || idLib::production==PROD_BUILDING) throw;
  if(idRegression* regression=GetRegression();regression!=nullptr) {
    const auto slots=*reinterpret_cast<const void* const* const*>(regression);
    if(reinterpret_cast<bool(*)(idRegression*)>(slots[13])(regression)) {
      idCmdArgs args;
      Com_TestStatsEnd_f(args);
      Sys_Quit(static_cast<unsigned int>(-1));
    }
  }
  Sys_SetClipboardData(idException::error);
  Disconnect(true);
  idLib::Printf("^1********************^7 ERROR ^1********************\n");
  idLib::Printf("^3%s\n",idException::error);
  idLib::Printf("^1***********************************************\n");
  idStr errorText;
  errorText.Format("ERROR: %s\n",idException::error);
  if(compileStatusHandler.compileStatus!=nullptr)
    compileStatusHandler.compileStatus->Error(errorText.c_str());
  if(idMem::Singleton()->OutOfMemory()) {
    AddDialog(GDM_OUT_OF_MEMORY,DIALOG_CONTINUE,nullptr,nullptr,true,"",0,
              false,false,false,0);
  } else if(wasInLobby) {
    AddDialog(GDM_DISCONNECTED,DIALOG_ACCEPT,nullptr,nullptr,false,"",0,
              true,true,false,0);
  }
  console->BeginTimeRange(RANGE_MAIN,idColor::colorBlack);
}
