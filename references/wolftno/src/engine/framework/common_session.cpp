#include "engine/framework/common_local.h"

#include "engine/framework/discswapmanager.h"
#include "engine/framework/console.h"
#include "engine/framework/loadscreen.h"
#include "engine/framework/gamesystem.h"
#include "engine/framework/resourcemanager.h"
#include "engine/framework/gamespawninfo.h"
#include "engine/framework/keyinput.h"
#include "engine/framework/playerprofile.h"
#include "engine/framework/session.h"
#include "engine/framework/rendermanager.h"
#include "engine/framework/swfhelper.h"
#include "engine/framework/versionselector.h"
#include "engine/decls/declmapinfo.h"
#include "engine/network/snapshot.h"
#include "engine/renderer/renderersystem.h"
#include "engine/renderer/rendersystem_bink.h"
#include "engine/sound/soundsystem.h"
#include "engine/sys/sys_signin.h"
#include "engine/sys/sys_lobby_types.h"
#include "engine/sys/sys_public.h"
#include "engine/sys/sys_savegame.h"
#include "shared/idlib/csystems/cvarsystemlocal.h"
#include "shared/idlib/csystems/cmdsystemlocal.h"
#include "shared/idlib/exception.h"
#include "shared/idlib/filesystem/compressor.h"
#include "shared/idlib/filesystem/filesystemlocal.h"
#include "shared/idlib/hashing/uuid.h"
#include "shared/idlib/metrics/timer.h"
#include "shared/idlib/sys/sys_time.h"
#include "shared/idlib/text/cmdargs.h"

#include <cstring>
#include <cstdlib>

extern int com_editors;
extern idCVar com_captureFrames;
extern idCVar com_captureSounds;
extern idCVar com_capturePath;
extern idCVar com_production;
extern idCVar discSwapper_verbose;
extern idCVar com_newLoadScreen;
extern int time_mapLoadMS;
extern void Sys_SoundCaptureStop();
extern void Sys_PumpEvents();
extern void Cmd_GetViewpos_f(const idCmdArgs* args);
extern void Cmd_SetViewpos_f(const idCmdArgs* args);

idCVar si_timeLimit("si_timeLimit","0",4,"Time limit",nullptr);
idCVar si_scoreLimit("si_scoreLimit","0",2,"Score limit",nullptr);
idCVar si_teamCount("si_teamCount","2",2,"How many teams, 0 for FFA",nullptr);
idCVar si_minRespawnDelay(
    "si_minRespawnDelay","5",4,
    "Minimum respawn delay after player death in MP game",nullptr);
idCVar si_swapTeams(
    "si_swapTeams","0",1,"Swap teams and do two rounds",nullptr);
idCVar si_stopWatch(
    "si_stopWatch","0",1,
    "If swap teams is enabled, do stopwatch as well",nullptr);
idCVar si_oneFlagCTF(
    "si_oneFlagCTF","0",1,
    "If swap teams is enabled and is CTF, it will do one flag",nullptr);
idCVar com_profileLevelLoad("com_profileLevelLoad","0",0,"",nullptr);
idCVar si_spawnspot(
    "si_spawnspot","player_start",0,
    "name of the spawn spot to use with the map command",nullptr);
idCVar si_nextlayeractive(
    "si_nextlayeractive","",0,
    "game play layers to use with map the map command",nullptr);
idCVar si_nextlayerdeactive(
    "si_nextlayerdeactive","",0,
    "game play layers to use with map the map command",nullptr);
idCVar si_devspawninventory(
    "si_devspawninventory","0",2,
    "if 0 dont give inventory, if 1 gets reset after first checkpoint, if 2 wont "
    "get reset and inventory will be updated on each checkpoint",nullptr);
idCVar si_jobDebugName_Error(
    "si_jobDebugName_Error","0",1,
    "if set and we don't find the requested si_jobDebugName error instead of warn",
    nullptr);
idCVar si_loadAllJobsOnBuild(
    "si_loadAllJobsOnBuild","0",1,
    "if true the jobmanager will load all possible jobs on a build game.",nullptr);
idCVar si_giveSPInventoryOnBuild(
    "si_giveSPInventoryOnBuild","0",1,
    "if true the jobmanager will give the list of possible single player items.",
    nullptr);
idCVar saveGame_allowOlderVersions(
    "saveGame_allowOlderVersions","0",2,
    "allows loading of savegames from previous versions of the game",nullptr);
idCVar com_useEntitiesFiles(
    "com_useEntitiesFiles","1",1,
    "if set use .entities files instead of .map files for production and buildgame "
    "loads",nullptr);
idCVar com_useExpandedEntityDefs(
    "com_useExpandedEntityDefs","1",1,
    "if set use entity defs with expanded inheritance for production and buildgame "
    "loads",nullptr);
idCVar si_multiplayerTestWeapons(
    "si_multiPlayerTestWeapons","0",65537,
    "when true starts with with devmap equipment",nullptr);
idSysMutex mutexHitch;

struct idVRAction {
  const char* phrases[3];
  const char* cmdCommand;
  const char* usrCmdAction;
  keyNum_t keyPress;
  std::byte padding[4];
};
static_assert(sizeof(idVRAction)==48,"idVRAction ABI drift");
extern idVRAction vrIngameActions[21];
extern idVRAction vrMenuActions[4];

void WaitOnSession(unsigned int waitingOnState,float pacifierProgress,
                   unsigned int pacifierState) {
  while(static_cast<unsigned int>(session->GetState())!=waitingOnState) {
    Sys_PumpEvents();
    if(pacifierProgress<0.0f) {
      session->UpdateSignInManager();
      session->Pump();
      session->ProcessSnapAckQueue();
    } else {
      common->UpdateLevelLoadPacifier(
          pacifierProgress,static_cast<pacifierState_t>(pacifierState),true,true);
    }
    Sys_Sleep(10);
    const int state=session->GetState();
    if(state!=6 && state!=2 && state!=3 && state!=8 &&
       static_cast<unsigned int>(state)!=waitingOnState) {
      idStr::snPrintf(idException::error,idException::MAX_ERROR_LEN,
          "WaitOnSession: Invalid state (%s) while waiting on state (%s).",
          session->GetStateString(),session->GetStateString(
              static_cast<int>(waitingOnState)));
      idNetworkLoadException exception;
      throw &exception;
    }
  }
}

void idCommonLocal::RestartPassMainMenu_f(const idCmdArgs& argsRef) {
  const idCmdArgs* args=&argsRef;
  commonLocal.Game()->RestartPassMainMenu(*args);
}

void idCommonLocal::RestartMultiplayerInvite_f(const idCmdArgs& argsRef) {
  const idCmdArgs* args=&argsRef;
  if(args->argc==2) {
    idLib::Printf("Restarting with multiplayer invite\n");
    discSwapMgr->Reset();
    if(discSwapMgr->RebuildStateAfterSwap(
           idDiscSwapManager::DSC_MULTIPLAYER_INVITE,*args) &&
       discSwapMgr->Swap(idDiscSwapManager::DSC_MULTIPLAYER_INVITE,3,true)==
           idDiscSwapManager::ERROR_NONE)
      session->JoinAfterSwap(&discSwapMgr->inviteInfo);
  } else {
    idLib::Warning("syntax: RestartMultiplayerInvite_f <swapDat_invite_stuff>");
  }
}

void idCommonLocal::RestartLoadGame_f(const idCmdArgs& argsRef) {
  const idCmdArgs* args=&argsRef;
  idLib::Printf("DISCSWAP      :  RestartLoadGame_f\n");
  if(args->argc==3) {
    idLib::PrintfIf(discSwapper_verbose.valueInteger!=0,
                    "[%s]:","idCommonLocal::RestartLoadGame_f");
    for(int index=0;index<args->argc;++index)
      idLib::PrintfIf(discSwapper_verbose.valueInteger!=0,
                      "\targs[%d] = %s\n",index,args->Argv(index));
    if(discSwapMgr->RebuildStateAfterSwap(idDiscSwapManager::DSC_LOAD,*args)) {
      commonLocal.Game()->CheckpointBlockOnLoad();
      cmdSystem->AppendCommandText(va("checkmap %d nextMapDiscSwap\n",-1));
      idLib::Printf("Loading checkpoint file %d...\n",-1);
    }
  } else {
    idLib::Warning("syntax: restart_loadGame <swapSerializedUser> <savegame>");
  }
}

void idCommonLocal::RestartMapAfterSwap_f(const idCmdArgs& argsRef) {
  const idCmdArgs* args=&argsRef;
  idLib::PrintfIf(discSwapper_verbose.valueInteger!=0,
                  "[%s]:","idCommonLocal::RestartMapAfterSwap_f");
  for(int index=0;index<args->argc;++index)
    idLib::PrintfIf(discSwapper_verbose.valueInteger!=0,
                    "\targs[%d] = %s\n",index,args->Argv(index));
  if(discSwapMgr->RebuildStateAfterSwap(idDiscSwapManager::DSC_MAP,*args)) {
    commonLocal.Game()->CheckpointBlockOnLoad();
    if(discSwapMgr->Swap(idDiscSwapManager::DSC_MAP,0,true)==
       idDiscSwapManager::ERROR_NONE) {
      si_spawnspot.SetString(discSwapMgr->spawnSpot.IsEmpty()
          ? "player_start_1" : discSwapMgr->spawnSpot.c_str(),true);
      si_nextlayerdeactive.SetString("",true);
      si_nextlayeractive.SetString(discSwapMgr->layerData.IsEmpty()
          ? "" : discSwapMgr->layerData.c_str(),true);
      cmdSystem->AppendCommandText(discSwapMgr->additionalArguments.c_str());
      idLib::PrintfIf(discSwapper_verbose.valueInteger!=0,
          "DISCSWAP: restartmapafterswap command: %s\n",
          discSwapMgr->additionalArguments.c_str());
      idLib::PrintfIf(discSwapper_verbose.valueInteger!=0,
          "          layer: '%s'\n",discSwapMgr->layerData.c_str());
      idLib::PrintfIf(discSwapper_verbose.valueInteger!=0,
          "\t\t  spawnspot: '%s'\n",discSwapMgr->spawnSpot.c_str());
      cmdSystem->ExecuteCommandBuffer();
    }
  }
}

void idCommonLocal::PrepareForMapChange() {
  CloseSnapshotFiles();
  if(idLib::production==3) idLib::production=2;
  console->Close(true);
  cvarSystem->ReportModifiedCVars();
  soundSystem->SetPlayingSoundWorld(nullptr);
  for(int index=0;index<4;++index) {
    sys->Joystick()->ResetColor(index);
    sys->Joystick()->SetRumble(index,0,0,0,0);
  }
  renderManager.Clear();
  gameSystem->FreeGame(Field<idGame*>(0x1D0));
  idList<idMapChangeListener*,5>& listeners=
      Field<idList<idMapChangeListener*,5>>(0x34CA8);
  for(int index=0;index<listeners.Num();++index)
    listeners[index]->MapDisconnected();
  session->SetUserSignedOutDuringLoading(false);
}

void idCommonLocal::LeaveGame_f(const idCmdArgs&) {
  if(session->GetActingGameStateLobbyBase().IsHost()) session->EndMatch(false);
}

void idCommonLocal::LoadGame_f(const idCmdArgs& argsRef) {
  const idCmdArgs* args=&argsRef;
  idStr saveName(args->Argv(1));
  if(saveName.IsEmpty()) saveName=session->GetCurrentSaveSlot();
  si_spawnspot.SetString("",true);
  commonLocal.Game()->LoadGame(saveName.c_str());
}

void idCommonLocal::SaveGame_f(const idCmdArgs& argsRef) {
  const idCmdArgs* args=&argsRef;
  idStr saveName(args->Argv(1));
  if(saveName.IsEmpty()) saveName=session->GetCurrentSaveSlot();
  commonLocal.Game()->RequestSaveGame(saveName.c_str(),true,false);
}

idFile* idCommonLocal::DetachActiveReadSnapshotFile() {
  delete Field<idFile*>(0x6D8);
  Field<idFile*>(0x6D8)=nullptr;
  delete Field<idFile*>(0x6E0);
  Field<idFile*>(0x6E0)=nullptr;
  idFile* result=Field<idFile*>(0x6E8);
  Field<idFile*>(0x6E8)=nullptr;
  return result;
}

idFile* idCommonLocal::DetachActiveWriteSnapshotFile() {
  delete Field<idFile*>(0x700);
  Field<idFile*>(0x700)=nullptr;
  delete Field<idFile*>(0x708);
  Field<idFile*>(0x708)=nullptr;
  idFile* result=Field<idFile*>(0x710);
  Field<idFile*>(0x710)=nullptr;
  return result;
}

bool idCommonLocal::IsInGame() const {
  return !Field<bool>(0x162) && Game()!=nullptr && Game()->IsGameActive(true);
}

snapshotState_t idCommonLocal::SnapshotState() const {
  if(Field<idFile*>(0x6E8)!=nullptr) return SNAP_READING;
  if(!session->GetActingGameStateLobbyBase().IsHost() &&
     session->GetActingGameStateLobbyBase().HasActivePeers())
    return SNAP_READING;
  if(Field<idFile*>(0x710)!=nullptr) return SNAP_WRITING;
  if(session->GetActingGameStateLobbyBase().IsHost() &&
     session->GetActingGameStateLobbyBase().HasActivePeers())
    return SNAP_WRITING;
  return SNAP_NONE;
}

bool idCommonLocal::IsToolActive() const { return com_editors!=0; }

void idCommonLocal::QueueSnapshotMemFile(idFile_Memory* file,unsigned int tag) {
  Field<idFile_Memory*>(0x34678)=file;
  Field<unsigned int>(0x34680)=tag;
}

void idCommonLocal::ClearQueuedSnapshotFile() {
  Field<idFile_Memory*>(0x34678)=nullptr;
  Field<unsigned int>(0x34680)=0;
}

void idCommonLocal::PlayQueuedSnapshotFile() {
  PlaySnapshotFile(Field<idFile_Memory*>(0x34678),Field<unsigned int>(0x34680));
  ClearQueuedSnapshotFile();
}

void idCommonLocal::StopSnapshotFileRecord() {
  *reinterpret_cast<std::int64_t*>(&SnapPrevious().localTime)=-1;
  Field<std::uint64_t>(0x748)=0;
  *reinterpret_cast<std::int64_t*>(&SnapCurrent().localTime)=-1;
  delete Field<idFile*>(0x700);
  Field<idFile*>(0x700)=nullptr;
  delete Field<idFile*>(0x708);
  Field<idFile*>(0x708)=nullptr;
  delete Field<idFile*>(0x710);
  Field<idFile*>(0x710)=nullptr;
  idSnapShot* snapshot=Field<idSnapShot*>(0x718);
  if(snapshot!=nullptr) {
    snapshot->~idSnapShot();
    idMem::Singleton()->Free(snapshot,ALIGN_16);
  }
  Field<idSnapShot*>(0x718)=nullptr;
  Field<std::uint64_t>(0x720)=0;
  Field<int>(0x6D0)=Field<int>(0x6D0)==4 && Field<idFile*>(0x6E8)!=nullptr ? 4 : 0;
}

void idCommonLocal::StopSnapshotFilePlayback() {
  *reinterpret_cast<std::int64_t*>(&SnapPrevious().localTime)=-1;
  Field<std::uint64_t>(0x738)=0;
  *reinterpret_cast<std::int64_t*>(&SnapCurrent().localTime)=-1;
  delete Field<idFile*>(0x6D8);
  Field<idFile*>(0x6D8)=nullptr;
  delete Field<idFile*>(0x6E0);
  Field<idFile*>(0x6E0)=nullptr;
  delete Field<idFile*>(0x6E8);
  Field<idFile*>(0x6E8)=nullptr;
  idSnapShot* snapshot=Field<idSnapShot*>(0x6F0);
  if(snapshot!=nullptr) {
    snapshot->~idSnapShot();
    idMem::Singleton()->Free(snapshot,ALIGN_16);
  }
  Field<idSnapShot*>(0x6F0)=nullptr;
  Field<std::uint64_t>(0x6F8)=0;
  Field<int>(0x6D0)=Field<int>(0x6D0)==4 && Field<idFile*>(0x710)!=nullptr ? 4 : 0;
  if(com_captureFrames.valueInteger!=0) com_captureFrames.SetInteger(0,true);
  if(com_captureSounds.valueInteger!=0) {
    com_captureSounds.SetInteger(0,true);
    cvarSystem->SetCVarFloat("s_volume_db",0.0f,0);
    Sys_SoundCaptureStop();
  }
}

void idCommonLocal::CloseSnapshotFiles() {
  if(Field<int>(0x6D0)==0) return;
  StopSnapshotFilePlayback();
  StopSnapshotFileRecord();
  Game()->OnCloseSnapshot();
}

void idCommonLocal::StopDemo_f(const idCmdArgs&) { commonLocal.CloseSnapshotFiles(); }

idPlayerProfile* idCommonLocal::GetProfileFromMasterLocalUser() {
  idPlayerProfile* profile=session->GetProfileFromMasterLocalUser();
  if(profile==nullptr) idLib::Error("MasterLocalUser profile is NULL");
  return profile;
}

idSWFHelper* idCommonLocal::GetSWFHelper() {
  return Field<idSWFHelper*>(0x34CC0);
}

void idCommonLocal::VoiceRecognition_ClearUnprocessedCmd() {
  session->VoiceRecognition_ClearUnprocessedPhrases();
}

void idCommonLocal::VoiceRecognition_GetBareWordlist(idList<idStr,5>& outList) {
  for(const idVRAction& action : vrIngameActions) {
    for(const char* phrase : action.phrases) {
      if(phrase!=nullptr) outList.Append(idStr(phrase));
    }
  }
  for(const idVRAction& action : vrMenuActions) {
    for(const char* phrase : action.phrases) {
      if(phrase==nullptr) continue;
      const idStr value(phrase);
      int index=0;
      while(index<outList.Num() && !(outList[index]==value)) ++index;
      if(index>=outList.Num()) outList.Append(value);
    }
  }
}

bool idCommonLocal::CutScenePress(sysEvent_t event,bool pressAlready) {
  if(event.evType!=SE_KEY || (event.evValue2&1)!=0) return false;
  if(!pressAlready) {
    if(idKeyInput::KeyIsBoundToMulti(event.inputDevice,BIND_DEFAULT,
           static_cast<keyNum_t>(event.evValue),0,"_use","_useandreload",
           "_crouch",nullptr) || event.evValue==28 || event.evValue==57)
      renderSystem->SetRenderFlag(RENDERFLAG_PRESSEDDURINGCUTSCENE);
    return false;
  }
  if(idKeyInput::KeyIsBoundToMulti(event.inputDevice,BIND_DEFAULT,
         static_cast<keyNum_t>(event.evValue),0,"_use","_useandreload",
         nullptr,nullptr) || event.evValue==28 || event.evValue==57) {
    renderSystem->RemoveRenderFlag(7);
    return true;
  }
  return false;
}

int idCommonLocal::GetMasterLocalUserInputDevice() const {
  idSignInManagerBase& signIn=session->GetSignInManager();
  if(signIn.GetMasterLocalUser()==nullptr) return -1;
  return signIn.GetMasterLocalUser()->GetInputDevice();
}

int idCommonLocal::GetMasterLocalUserPlayerId() const {
  const int player=Game()->GetPlayerIndexFromInputDevice(
      GetMasterLocalUserInputDevice());
  return player==-1 ? 0 : player;
}

idLocalUser* idCommonLocal::GetMasterLocalUser() const {
  return session->GetSignInManager().GetMasterLocalUser();
}

void idUserCmdMgr::ResetPlayer(int playerIndex) {
  for(int frame=0;frame<128;++frame)
    std::memset(&cmdBuffer[frame][playerIndex],0,sizeof(usercmd_t));
  writeFrame[playerIndex]=0;
  readFrame[playerIndex]=-1;
}

const idStrStatic<1024>& idGameSpawnInfo::GetSpawnSpot() const {
  return spawnSpotOverride.IsEmpty() ? spawnSpot : spawnSpotOverride;
}

void idCommonLocal::ResetNetworkingState() {
  Field<std::uint64_t>(0x728)=100000;
  Field<float>(0x34C38)=0.5f;
  Field<float>(0x34C3C)=0.0f;
  Field<std::uint64_t>(0x738)=0;
  Field<std::uint64_t>(0x748)=0;
  Field<std::uint64_t>(0x750)=0;
  Field<std::uint64_t>(0x34660)=0;
  Field<std::uint64_t>(0x34668)=0;
  Field<int>(0x34670)=0;
  Field<std::uint64_t>(0x34730)=0;
  Field<std::uint64_t>(0x34C40)=0;
  for(int i=0;i<16;++i) ReceivedSnaps()[i].Clear();
  std::memset(UserCmdMgr().cmdBuffer,0,sizeof(UserCmdMgr().cmdBuffer));
  std::memset(UserCmdMgr().writeFrame,0,sizeof(UserCmdMgr().writeFrame));
  std::memset(UserCmdMgr().readFrame,-1,sizeof(UserCmdMgr().readFrame));
  *reinterpret_cast<std::int64_t*>(&SnapCurrent().localTime)=-1;
  *reinterpret_cast<std::int64_t*>(&SnapPrevious().localTime)=-1;
  OldSnapshot().Clear();
}

void idCommonLocal::ResetPlayerInput(int playerIndex) {
  UserCmdMgr().ResetPlayer(playerIndex);
}

void idCommonLocal::UpdateBotUserCmds(int botPlayerIndex,const usercmd_t& command) {
  UserCmdMgr().PutUserCmdForPlayer(botPlayerIndex,command);
}

idUUID::idUUID(const idUUID& other)
    : version(other.version),hash(other.hash),generator(other.generator),
      data(other.data) {}

void idMatchParameters::SetMapName(const char* name) {
  const idStrStatic<128> value(name);
  mapName=value;
}

idMatchParameters::idMatchParameters(int gameMode_,int searchMode_,
    int gameMap_,int gameType_,int numSlots_,std::uint8_t matchFlags_,
    const idStr& mapName_,const idStr& layersActive_,
    const idStr& layersDeactive_,const idStr& spawnSpot_,
    const idStr& loadingScreen_,float minRespawnDelay_,float timeLimit_,
    int scoreLimit_,int teamCount_,bool swapTeams_,bool stopWatch_,
    bool comboMap_,bool devmap_,bool transitional_,bool forceHeapReset_,
    int checkpointSlot,bool playVideo_)
    : matchID(),gameMode(gameMode_),searchMode(searchMode_),gameMap(gameMap_),
      gameType(gameType_),numSlots(numSlots_),startCheckSlot(checkpointSlot),
      matchFlags(matchFlags_),mapName(mapName_),layersActive(layersActive_),
      layersDeactive(layersDeactive_),spawnSpot(spawnSpot_),
      loadingScreen(loadingScreen_),timeLimit(static_cast<int>(timeLimit_)),
      scoreLimit(scoreLimit_),numBots(-1),botSkill(1),
      minRespawnDelay(minRespawnDelay_),teamCount(teamCount_),
      swapTeams(swapTeams_),stopWatch(stopWatch_),comboMap(comboMap_),
      devmap(devmap_),isTransitionalMapRequest(transitional_),
      forceHeapReset(forceHeapReset_),playVideo(playVideo_),
      nightmareCompletionStat(-1),completionStats(),collectionStat(-1),
      specialCollectionStat(-1),challengeLeaderboardId(-1),skillLevel(0),
      nextMapDiscSwap(false) {}

idMatchParameters::idMatchParameters(const idMatchParameters& other)
    : matchID(other.matchID),gameMode(other.gameMode),
      searchMode(other.searchMode),gameMap(other.gameMap),
      gameType(other.gameType),numSlots(other.numSlots),
      startCheckSlot(other.startCheckSlot),matchFlags(other.matchFlags),
      mapName(other.mapName),layersActive(other.layersActive),
      layersDeactive(other.layersDeactive),spawnSpot(other.spawnSpot),
      loadingScreen(other.loadingScreen),timeLimit(other.timeLimit),
      scoreLimit(other.scoreLimit),numBots(other.numBots),
      botSkill(other.botSkill),minRespawnDelay(other.minRespawnDelay),
      teamCount(other.teamCount),swapTeams(other.swapTeams),
      stopWatch(other.stopWatch),comboMap(other.comboMap),devmap(other.devmap),
      isTransitionalMapRequest(other.isTransitionalMapRequest),
      forceHeapReset(other.forceHeapReset),playVideo(other.playVideo),
      nightmareCompletionStat(other.nightmareCompletionStat),
      completionStats(other.completionStats),collectionStat(other.collectionStat),
      specialCollectionStat(other.specialCollectionStat),
      challengeLeaderboardId(other.challengeLeaderboardId),
      skillLevel(other.skillLevel),nextMapDiscSwap(other.nextMapDiscSwap) {}

idMatchParameters::~idMatchParameters()=default;

idGameSpawnInfo::idGameSpawnInfo(const idGameSpawnInfo& other)
    : gameFlags(other.gameFlags),mapName(other.mapName),
      spawnSpot(other.spawnSpot),spawnSpotOverride(other.spawnSpotOverride),
      checkpointName(other.checkpointName),loadingScreen(other.loadingScreen),
      activeLayers(other.activeLayers),deactiveLayers(other.deactiveLayers),
      comboMap(other.comboMap),devmap(other.devmap),
      forceHeapReset(other.forceHeapReset),gameSpawnFlags(other.gameSpawnFlags),
      numPlayers(other.numPlayers),
      currentCheckpointSlot(other.currentCheckpointSlot) {}

idGameSpawnInfo::idGameSpawnInfo(const char* mapName_,int gameFlags_)
    : gameFlags(gameFlags_),mapName(mapName_),spawnSpot(),spawnSpotOverride(),
      checkpointName(),loadingScreen(),activeLayers(),deactiveLayers(),
      comboMap(false),devmap(false),forceHeapReset(false),gameSpawnFlags(1),
      numPlayers(1),currentCheckpointSlot(-1) {}

void idGameSpawnInfo::SetSpawnSpot(const char* value) {
  const idStrStatic<1024> text(value);
  spawnSpot=text;
}

void idGameSpawnInfo::SetSpawnSpotOverride(const char* value) {
  const idStrStatic<1024> text(value);
  spawnSpotOverride=text;
}

void idGameSpawnInfo::WriteLayersToCommaSeparatedStrings(
    idStr& activeString,idStr& deactiveString) const {
  activeString="";
  for(int index=0;index<activeLayers.Num()-1;++index) {
    activeString+=activeLayers[index];
    activeString+=",";
  }
  if(activeLayers.Num()>0) activeString+=activeLayers[activeLayers.Num()-1];

  deactiveString="";
  for(int index=0;index<deactiveLayers.Num()-1;++index) {
    deactiveString+=deactiveLayers[index];
    deactiveString+=",";
  }
  if(deactiveLayers.Num()>0)
    deactiveString+=deactiveLayers[deactiveLayers.Num()-1];
}

void idCommonLocal::Hitch_f(const idCmdArgs& argsRef) {
  const idCmdArgs* args=&argsRef;
  idSoundWorld* playingWorld=soundSystem->GetPlayingSoundWorld();
  if(playingWorld!=nullptr) {
    soundSystem->SetMute(true);
    Sys_MutexLock(&mutexHitch.handle,true);
  }
  const unsigned long milliseconds=args->argc==2
      ? static_cast<unsigned long>(std::atol(args->Argv(1))) : 100UL;
  Sys_Sleep(milliseconds);
  if(playingWorld!=nullptr) {
    Sys_MutexUnlock(&mutexHitch.handle);
    soundSystem->SetMute(false);
  }
}

void idCommonLocal::UpdateConsoleDisplay(bool) {
  if(idLib::production!=1 && com_production.valueInteger<=0 &&
     console->IsInitialized() && !Field<bool>(0x163)) {
    Field<bool>(0x163)=true;
    sys->Mouse()->GrabCursor(false);
    sys->GenerateEvents();
    renderManager.UpdateConsole();
    soundSystem->Render(false);
    Field<bool>(0x163)=false;
  }
}

void idCommonLocal::CompressFile_f(const idCmdArgs& argsRef) {
  const idCmdArgs* args=&argsRef;
  idStr name;
  idTimer timer;
  timer.Start();
  idFile* input=fileSystem->OpenFileReadRetail(args->Argv(1),true,false);
  if(input==nullptr) return;
  name=args->Argv(1);
  name.SetFileExtension("zid");
  idFile* output=fileSystem->OpenFileWriteRetail(name.c_str(),FSPATH_SAVE);
  if(output==nullptr) return;

  idCompressor* arithmetic=idCompressor::AllocArithmetic();
  arithmetic->Init(output,true,8);
  idCompressor* lzss=idCompressor::AllocLZSS_ByteAligned();
  lzss->Init(arithmetic,true,8);
  const int length=static_cast<int>(input->Length());
  lzss->Write(&length,sizeof(length));
  unsigned char buffer[65536];
  int offset=0;
  while(offset<length-65536) {
    input->Read(buffer,sizeof(buffer));
    lzss->Write(buffer,sizeof(buffer));
    offset+=static_cast<int>(sizeof(buffer));
  }
  input->Read(buffer,length-offset);
  lzss->Write(buffer,length-offset);
  delete lzss;
  delete arithmetic;
  delete output;
  delete input;
  timer.Stop();
  idLib::Printf("%1.0f milliseconds to compress the file\n",
                static_cast<float>(timer.Milliseconds()));
}

void idCommonLocal::DecompressFile_f(const idCmdArgs& argsRef) {
  const idCmdArgs* args=&argsRef;
  idStr name(args->Argv(1));
  idTimer timer;
  timer.Start();
  name.SetFileExtension("zid");
  idFile* input=fileSystem->OpenFileReadRetail(name.c_str(),true,false);
  if(input==nullptr) return;
  idFile* output=fileSystem->OpenFileWriteRetail(args->Argv(1),FSPATH_SAVE);
  if(output==nullptr) return;

  idCompressor* arithmetic=idCompressor::AllocArithmetic();
  arithmetic->Init(input,false,8);
  idCompressor* lzss=idCompressor::AllocLZSS_ByteAligned();
  lzss->Init(arithmetic,false,8);
  int length;
  lzss->Read(&length,sizeof(length));
  unsigned char buffer[65536];
  int offset=0;
  while(offset<length-65536) {
    input->Read(buffer,sizeof(buffer));
    lzss->Write(buffer,sizeof(buffer));
    offset+=static_cast<int>(sizeof(buffer));
  }
  input->Read(buffer,length-offset);
  lzss->Write(buffer,length-offset);
  delete lzss;
  delete arithmetic;
  delete output;
  delete input;
  timer.Stop();
  idLib::Printf("%1.0f milliseconds to decompress the file\n",
                static_cast<float>(timer.Milliseconds()));
}

template<>
std::size_t idFile::ReadBig<std::uint64_t>(std::uint64_t& value) {
  const std::size_t result=Read(&value,sizeof(value));
  value=((value&0x00000000000000FFULL)<<56) |
        ((value&0x000000000000FF00ULL)<<40) |
        ((value&0x0000000000FF0000ULL)<<24) |
        ((value&0x00000000FF000000ULL)<<8) |
        ((value&0x000000FF00000000ULL)>>8) |
        ((value&0x0000FF0000000000ULL)>>24) |
        ((value&0x00FF000000000000ULL)>>40) |
        ((value&0xFF00000000000000ULL)>>56);
  return result;
}

namespace {
std::uint32_t Big32(std::uint32_t value) {
  return ((value&0x000000FFU)<<24) | ((value&0x0000FF00U)<<8) |
         ((value&0x00FF0000U)>>8) | ((value&0xFF000000U)>>24);
}
std::uint64_t Big64(std::uint64_t value) {
  return ((value&0x00000000000000FFULL)<<56) |
         ((value&0x000000000000FF00ULL)<<40) |
         ((value&0x0000000000FF0000ULL)<<24) |
         ((value&0x00000000FF000000ULL)<<8) |
         ((value&0x000000FF00000000ULL)>>8) |
         ((value&0x0000FF0000000000ULL)>>24) |
         ((value&0x00FF000000000000ULL)>>40) |
         ((value&0xFF00000000000000ULL)>>56);
}
}

void idCommonLocal::RecordSnapshotFile(idFile* file,unsigned int tag) {
  if(file==nullptr) idLib::Error("Snapshot record failed because of NULL file.");
  idLib::Printf("Recording %s\n",file->GetName());
  Game()->GetRenderWorld()->GenerateAllInteractions();
  if(tag==0x44454D4FU) {
    Field<int>(0x6D0)=session->GetActingGameStateLobbyBase().IsHost() ||
        !session->GetActingGameStateLobbyBase().HasActivePeers() ? 1 : 2;
  } else {
    Field<int>(0x6D0)=4;
  }

  void* memory=idMem::Singleton()->AllocWithLocation(
      __FILE__,sizeof(idSnapShot),static_cast<memTag_t>(57),false,ALIGN_16,
      HEAP_DEFAULTHEAP);
  Field<idSnapShot*>(0x718)=memory!=nullptr ? new(memory) idSnapShot() : nullptr;
  Field<std::uint32_t>(0x720)=0;
  Field<idFile*>(0x710)=file;
  Field<idCompressor*>(0x708)=idCompressor::AllocLZW();
  Field<idCompressor*>(0x708)->Init(Field<idFile*>(0x710),true,8);
  Field<idCompressor*>(0x700)=idCompressor::AllocRunLength_ZeroBased();
  Field<idCompressor*>(0x700)->Init(Field<idCompressor*>(0x708),true,8);
  idCompressor* compressor=Field<idCompressor*>(0x700);

  std::uint32_t word=Big32(tag);
  compressor->Write(&word,sizeof(word));
  word=Big32(3);
  compressor->Write(&word,sizeof(word));

  const idGameSpawnInfo* spawnInfo=Game()->GetGameSpawnInfo();
  compressor->WriteString(spawnInfo->mapName.c_str());
  std::uint8_t flag=spawnInfo->comboMap;
  compressor->Write(&flag,sizeof(flag));
  flag=spawnInfo->devmap;
  compressor->Write(&flag,sizeof(flag));
  word=Big32(static_cast<std::uint32_t>(spawnInfo->numPlayers));
  compressor->Write(&word,sizeof(word));

  word=Big32(static_cast<std::uint32_t>(spawnInfo->activeLayers.Num()));
  compressor->Write(&word,sizeof(word));
  for(int index=0;index<spawnInfo->activeLayers.Num();++index) {
    const idStr& layer=spawnInfo->activeLayers[index];
    const int length=layer.Length();
    compressor->Write(&length,sizeof(length));
    compressor->Write(layer.c_str(),length);
  }
  word=Big32(static_cast<std::uint32_t>(spawnInfo->deactiveLayers.Num()));
  compressor->Write(&word,sizeof(word));
  for(int index=0;index<spawnInfo->deactiveLayers.Num();++index) {
    const idStr& layer=spawnInfo->deactiveLayers[index];
    const int length=layer.Length();
    compressor->Write(&length,sizeof(length));
    compressor->Write(layer.c_str(),length);
  }
  word=Big32(static_cast<std::uint32_t>(spawnInfo->gameSpawnFlags));
  compressor->Write(&word,sizeof(word));
  word=Big32(static_cast<std::uint32_t>(spawnInfo->currentCheckpointSlot));
  compressor->Write(&word,sizeof(word));

  std::uint64_t wide=Big64(Field<std::uint64_t>(0x728));
  compressor->Write(&wide,sizeof(wide));
  const idMatchParameters& match=
      session->GetActingGameStateLobbyBase().GetMatchParms();
  word=Big32(static_cast<std::uint32_t>(match.gameMap));
  compressor->Write(&word,sizeof(word));
  word=Big32(static_cast<std::uint32_t>(match.gameMode));
  compressor->Write(&word,sizeof(word));
  Field<std::uint64_t>(0x750)=0;
  Field<float>(0x34660)=0.0f;
  Field<std::uint64_t>(0x738)=0;
}

void idCommonLocal::RecordDemo_f(const idCmdArgs& argsRef) {
  const idCmdArgs* args=&argsRef;
  idStr demoName(args->Argv(1));
  if(demoName.IsEmpty()) {
    int lastNumber=0;
    fileSystem->GetNumberedFilenameRetail(
        lastNumber,"demos/demo","demo",demoName,true);
  } else {
    demoName="demos/"+demoName;
  }
  demoName.SetFileExtension("demo");
  if(commonLocal.Field<int>(0x6D0)!=0) {
    commonLocal.StopSnapshotFilePlayback();
    commonLocal.StopSnapshotFileRecord();
    commonLocal.Game()->OnCloseSnapshot();
  }
  idFile* file=fileSystem->OpenFileWriteRetail(demoName.c_str(),FSPATH_SAVE);
  commonLocal.RecordSnapshotFile(file,0x44454D4FU);
  if(session->GetActingGameStateLobbyBase().IsHost() ||
     !session->GetActingGameStateLobbyBase().HasActivePeers())
    commonLocal.Field<int>(0x6D0)=1;
  else
    commonLocal.Field<int>(0x6D0)=2;
}

void idCommonLocal::DeleteGenerated_f(const idCmdArgs&) {
  idFileList* files=fileSystem->ListFilesTreeRetail("generated","*.*",false);
  idLib::Printf("Removing generated files...\n");
  for(int index=0;index<files->list.Num();++index) {
    idLib::Printf("Deleting %s\n",files->list[index].c_str());
    fileSystem->RemoveFileRetail(files->list[index].c_str(),FSPATH_BASE);
  }
  idLib::Printf("done.\n");
  fileSystem->FreeFileListRetail(files);
}

void idCommonLocal::Map_f(const idCmdArgs& argsRef) {
  const idCmdArgs* args=&argsRef;
  bool comboMap=false;
  bool devmap=false;
  bool forceHeapReset=false;
  bool checkpointLoad=false;
  bool nextMapDiscSwap=false;
  int checkpointSlot=-1;
  idGame* game=commonLocal.Game();
  bool playVideo=false;

  idSignInManagerBase& signIn=session->GetSignInManager();
  if(signIn.GetNumLocalUsers()<=0 || signIn.GetLocalUserByIndex(0)==nullptr) {
    signIn.SetDesiredLocalUsers(1,1);
    session->GetSignInManager().Pump();
  }
  for(int index=2;index<args->argc;++index) {
    const char* option=args->Argv(index);
    if(idStr::Icmp(option,"combo")==0) comboMap=true;
    else if(idStr::Icmp(option,"dev")==0) devmap=true;
    else if(idStr::Icmp(option,"reset")==0) forceHeapReset=true;
    else if(idStr::Icmp(option,"check")==0) {
      checkpointLoad=true;
      comboMap=true;
    } else if(idStr::Icmp(option,"nextMapDiscSwap")==0) {
      nextMapDiscSwap=true;
    }
  }

  idStr mapExt("map");
  idStr map(args->Argv(1));
  map.ToLower();
  if(map.Right(8)=="entities") mapExt="entities";
  else map.StripFileExtension();

  if(game!=nullptr && !checkpointLoad) {
    game->CheckpointCapFileData(-1);
  } else if(checkpointLoad) {
    checkpointSlot=std::atoi(args->Argv(1));
    if(checkpointSlot==-1)
      checkpointSlot=game->CheckpointGetActiveSlots()-1;
    playVideo=std::atoi(args->Argv(2))!=0;
    idGameSpawnInfo checkpointInfo("",0);
    if(game->CheckpointGetSpawninfoFromSlot(checkpointSlot,checkpointInfo)) {
      si_spawnspot.SetString(checkpointInfo.GetSpawnSpot().c_str(),true);
      idStr active;
      idStr deactive;
      checkpointInfo.WriteLayersToCommaSeparatedStrings(active,deactive);
      si_nextlayeractive.SetString(active.c_str(),true);
      si_nextlayerdeactive.SetString(deactive.c_str(),true);
      map=checkpointInfo.mapName;
      map.ToLower();
      map.StripFileExtension();
    } else {
      idLib::Printf("idCommonLocal::Map_f map doesnt have valid data");
    }
  }

  discSwapMgr->Reset();
  discSwapMgr->map=map;
  discSwapMgr->additionalArguments=args->Args(0,-1,false,false);
  discSwapMgr->layerData=si_nextlayeractive.valueString;
  discSwapMgr->spawnSpot=si_spawnspot.valueString;
  if(discSwapMgr->Swap(idDiscSwapManager::DSC_MAP,0,true)!=
     idDiscSwapManager::ERROR_NONE)
    return;

  if(!session->CheckMapInstallAndShowGui(map.c_str(),true,true)) {
    session->MoveToPressStart();
    return;
  }

  if(idLib::production==0) {
    idStr path(va("maps/%s",map.c_str()));
    path.SetFileExtension(mapExt.c_str());
    if(!fileSystem->FileExistsRetail(path.c_str(),false)) {
      idLib::Printf("Can't find map '%s'\n",path.c_str());
      if(mapExt=="entities")
        idLib::Printf("Use the writeEntitiesFile command to convert the map to an entities file.\n");
      return;
    }
    if(comboMap) {
      idStr regionMap(map);
      regionMap.StripFileExtension();
      regionMap.StripPath();
      path.Format("maps/%s/_combo/%s.proc",map.c_str(),regionMap.c_str());
      if(!fileSystem->FileExistsRetail(path.c_str(),false))
        idLib::Error("You need to build the combo map for: %s",map.c_str());
    } else {
      idStr regionMap(va("maps/%s",map.c_str()));
      regionMap.StripFileExtension();
      regionMap+="_region.map";
      if(fileSystem->FileExistsRetail(regionMap.c_str(),false)) {
        idLib::Printf("Loading region map: %s\n",map.c_str());
        map=va("%s_region",map.c_str());
      }
    }
  }

  const idStr loadingScreen("");
  const idStr spawnSpot(si_spawnspot.valueString);
  const idStr deactive(si_nextlayerdeactive.valueString);
  const idStr active(si_nextlayeractive.valueString);
  idMatchParameters matchParms(
      -1,-1,-1,0,16,0,map,active,deactive,spawnSpot,loadingScreen,
      si_minRespawnDelay.valueFloat,si_timeLimit.valueFloat,
      si_scoreLimit.valueInteger,si_teamCount.valueInteger,
      si_swapTeams.valueInteger!=0,si_stopWatch.valueInteger!=0,
      comboMap,devmap,false,forceHeapReset,checkpointSlot,playVideo);
  matchParms.nextMapDiscSwap=nextMapDiscSwap;
  session->ClearSessionOption(-1);
  session->CreateMatch(matchParms);
  WaitOnSession(5,-1.0f,0);
  session->StartMatch();
  WaitOnSession(6,-1.0f,0);
  si_nextlayeractive.SetString("",true);
  si_nextlayerdeactive.SetString("",true);
  if(si_spawnspot.valueString.IsEmpty())
    si_spawnspot.SetString("player_start",true);
  fileSystem->ClearZipCacheRetail();
}

void idCommonLocal::DevMap_f(const idCmdArgs& argsRef) {
  const idCmdArgs* args=&argsRef;
  si_spawnspot.SetString("player_start_1",true);
  idCmdArgs copy(*args);
  copy.AppendArg("dev");
  Map_f(copy);
}

void idCommonLocal::ComboMap_f(const idCmdArgs& argsRef) {
  const idCmdArgs* args=&argsRef;
  idCmdArgs copy(*args);
  copy.AppendArg("combo");
  Map_f(copy);
}

void idCommonLocal::DevComboMap_f(const idCmdArgs& argsRef) {
  const idCmdArgs* args=&argsRef;
  idCmdArgs copy(*args);
  copy.AppendArg("dev");
  copy.AppendArg("combo");
  Map_f(copy);
}

void idCommonLocal::CheckLoad_f(const idCmdArgs& argsRef) {
  const idCmdArgs* args=&argsRef;
  idCmdArgs copy(*args);
  copy.AppendArg("check");
  Map_f(copy);
}

void idCommonLocal::NextMap_f(const idCmdArgs& argsRef) {
  const idCmdArgs* args=&argsRef;
  if(commonLocal.Game()->IsGameActive(true)) {
    if(args->argc==7) {
      idGameSpawnInfo gameSpawnInfo("",0);
      if(commonLocal.Game()->NextMap(*args,gameSpawnInfo))
        commonLocal.ExecuteMapChange(gameSpawnInfo,nullptr);
    } else {
      idLib::Printf("syntax: nextMap <shouldWeSave - 0, 1> <mapnameTarget> <spawnspotTarget> <loadingScreen> <layersActive> <layersDeactive>\n");
    }
  } else {
    idLib::Printf("nextMap only works if a map is running!");
  }
}

void idCommonLocal::LoadMainMenu() {
  ResetMapHeap(false);
  gameFrameParms_t parms=GameFrameParms(1);
  idGameSpawnInfo spawnInfo("",1);
  gameSystem->AllocGame(Field<idGame*>(0x1D0),spawnInfo,parms,nullptr,nullptr);
  if(!idLib::headless || com_production.valueInteger==1)
    GetProfileFromMasterLocalUser();
  renderManager.RenderFrame();
  renderSystem->BeginAutomaticBackgroundSwaps(
      renderSystem->GetGameWindow(),false,false);
  Game()->CheckpointBlockOnLoad();
  renderSystem->EndAutomaticBackgroundSwaps();
  Game()->Shell_CreateMenu(IsInGame());
  renderManager.BuildGuiFrame(Game()->Shell_Render(),false);
  renderManager.RenderFrame();
  if(session->GetUserSignedOutDuringLoading()) {
    ClearDialog(static_cast<gameDialogMessages_t>(72),nullptr,0);
    AddDialog(static_cast<gameDialogMessages_t>(173),DIALOG_CONTINUE,
              nullptr,nullptr,false,nullptr,0,false,false,false,0);
  }
}

void idCommonLocal::LeaveGame(bool cancelProcessors) {
  CloseSnapshotFiles();
  Field<bool>(0x163)=false;
  renderManager.Clear();
  SyncAllBackgroundOperations(false);
  if(cancelProcessors)
    session->GetSaveGameManager()->CancelAllProcessors(true);
  for(int index=0;index<4;++index) {
    sys->Joystick()->ResetColor(index);
    sys->Joystick()->SetRumble(index,0,0,0,0);
  }
  StopLevelLoad(false);
  gameSystem->FreeGame(Field<idGame*>(0x1D0));
  idList<idMapChangeListener*,5>& listeners=
      Field<idList<idMapChangeListener*,5>>(0x34CA8);
  for(int index=0;index<listeners.Num();++index)
    listeners[index]->MapDisconnected();
  ResetNetworkingState();
  LoadMainMenu();
}

void idCommonLocal::CanceledFinalizedMapChangeCleanUp() {
  if(renderSystem->BinkVideoIsLoaded()) renderSystem->FreeBinkVideo();
  renderSystem->EndAutomaticBackgroundSwaps();
  session->GetSaveGameManager()->CancelAllProcessors(true);
  StopLevelLoad(false);
  if(Game()!=nullptr) gameSystem->FreeGame(Field<idGame*>(0x1D0));
  session->QuitMatchToTitle();
  if(!IsDialogActive()) LoadMainMenu();
}

void idCommonLocal::Disconnect(bool cancelAllProcessors) {
  const bool consoleActive=console->IsActive();
  session->MoveToPressStart();
  LeaveGame(cancelAllProcessors);
  if(consoleActive) cmdSystem->ExecuteCommandText("activateConsole 0.5");
}

void idCommonLocal::Disconnect_f(const idCmdArgs&) {
  const bool consoleActive=console->IsActive();
  session->MoveToPressStart();
  commonLocal.LeaveGame(true);
  if(consoleActive) cmdSystem->ExecuteCommandText("activateConsole 0.5");
}

void idCommonLocal::ExecuteMapChangeForSnapshotFile(
    const idGameSpawnInfo& gameSpawnInfo,idFile* saveGameFile) {
  PrepareForMapChange();
  ClearWarnings(gameSpawnInfo.mapName.c_str());
  if(session->GetState()!=6) {
    std::uint32_t encoded=0xFFFFFFFFU;
    Field<idCompressor*>(0x6D8)->Read(&encoded,sizeof(encoded));
    const int gameMap=static_cast<int>(Big32(encoded));
    encoded=0xFFFFFFFFU;
    Field<idCompressor*>(0x6D8)->Read(&encoded,sizeof(encoded));
    const int gameMode=static_cast<int>(Big32(encoded));
    idMatchParameters matchParms(
        session->GetActingGameStateLobbyBase().GetMatchParms());
    matchParms.gameMap=gameMap;
    matchParms.gameMode=gameMode;
    matchParms.numSlots=gameSpawnInfo.numPlayers;
    matchParms.SetMapName(gameSpawnInfo.mapName.c_str());
    session->ClearSessionOption(-1);
    session->CreateMatch(matchParms);
    WaitOnSession(5,-1.0f,0);
    session->StartMatch();
    WaitOnSession(6,-1.0f,0);
  }
  FinalizeMapChange(gameSpawnInfo,saveGameFile);
}

void idCommonLocal::PlaySnapshotFile(idFile* file,unsigned int tag) {
  Field<idFile*>(0x6E8)=file;
  if(file==nullptr) {
    idLib::Printf("PlaySnapshotFile: invalid file\n");
    return;
  }
  idLib::Printf("Playing %s\n",file->GetName());
  Field<idCompressor*>(0x6E0)=idCompressor::AllocLZW();
  Field<idCompressor*>(0x6E0)->Init(Field<idFile*>(0x6E8),false,8);
  Field<idCompressor*>(0x6D8)=idCompressor::AllocRunLength_ZeroBased();
  Field<idCompressor*>(0x6D8)->Init(Field<idCompressor*>(0x6E0),false,8);
  idCompressor* compressor=Field<idCompressor*>(0x6D8);

  std::uint32_t encoded=0;
  compressor->Read(&encoded,sizeof(encoded));
  const unsigned int fileTag=Big32(encoded);
  if(fileTag!=tag) {
    StopSnapshotFilePlayback();
    StopSnapshotFileRecord();
    Field<unsigned int>(0x34680)=0;
    idLib::Error("invalid demo TAG %i",tag);
  }
  compressor->Read(&encoded,sizeof(encoded));
  const unsigned int version=Big32(encoded);
  if(version!=3) {
    StopSnapshotFilePlayback();
    StopSnapshotFileRecord();
    Field<unsigned int>(0x34680)=0;
    idLib::Error("incompatible demo version: %d, should be %d",version,3);
  }

  const int gameFlags=tag==0x44454D4FU ? 64 : 28;
  idStrStatic<1024> recordedMapName;
  compressor->ReadString(recordedMapName);
  bool comboMap=false;
  bool devmap=false;
  compressor->Read(&comboMap,sizeof(comboMap));
  compressor->Read(&devmap,sizeof(devmap));
  compressor->Read(&encoded,sizeof(encoded));
  const int numPlayers=static_cast<int>(Big32(encoded));
  compressor->Read(&encoded,sizeof(encoded));
  const int gameSpawnFlags=static_cast<int>(Big32(encoded));
  compressor->Read(&encoded,sizeof(encoded));
  const int currentCheckpointSlot=static_cast<int>(Big32(encoded));
  compressor->ReadBig(Field<std::uint64_t>(0x728));

  idGameSpawnInfo spawnInfo("",gameFlags);
  spawnInfo.comboMap=comboMap;
  spawnInfo.devmap=devmap;
  spawnInfo.forceHeapReset=false;
  spawnInfo.gameSpawnFlags=gameSpawnFlags;
  spawnInfo.numPlayers=numPlayers;
  spawnInfo.currentCheckpointSlot=currentCheckpointSlot;
  spawnInfo.FixupFeatureLayers();
  spawnInfo.MakeNamesCanonical();

  int num=0;
  compressor->Read(&encoded,sizeof(encoded));
  num=static_cast<int>(Big32(encoded));
  spawnInfo.activeLayers.SetNum(num);
  for(int index=0;index<num;++index)
    compressor->ReadString(spawnInfo.activeLayers[index]);
  spawnInfo.deactiveLayers.SetNum(num);
  for(int index=0;index<num;++index)
    compressor->ReadString(spawnInfo.deactiveLayers[index]);

  ExecuteMapChangeForSnapshotFile(spawnInfo,nullptr);
  if(tag==0x44454D4FU) Field<int>(0x6D0)=3;
  else if(tag==0x5454524CU) Field<int>(0x6D0)=4;

  void* memory=idMem::Singleton()->AllocWithLocation(
      __FILE__,sizeof(idSnapShot),static_cast<memTag_t>(57),false,ALIGN_16,
      HEAP_DEFAULTHEAP);
  Field<idSnapShot*>(0x6F0)=memory!=nullptr ? new(memory) idSnapShot() : nullptr;
  Field<std::uint32_t>(0x6F8)=0;
  Field<std::uint64_t>(0x730)=Field<std::uint64_t>(0x728);
  Field<std::uint64_t>(0x738)=0;
  Field<std::uint64_t>(0x750)=Field<std::uint64_t>(0x728)+1;

  if(com_captureSounds.valueInteger!=0) {
    idStr outPath("screenshots/");
    outPath+=com_capturePath.valueString;
    idStr work(file->GetName());
    work.StripPath();
    outPath.AppendPath(work.c_str());
    outPath.StripFileExtension();
    outPath.SetFileExtension("wav");
    char osPath[256];
    fileSystem->RelativePathToOSPathRetail(
        outPath.c_str(),osPath,sizeof(osPath),FSPATH_SAVE);
    fileSystem->CreateOSPathRetail(osPath);
    com_capturePath.SetString(osPath,true);
  }
}

void idCommonLocal::PlayDemo_f(const idCmdArgs& argsRef) {
  const idCmdArgs* args=&argsRef;
  idMem::Singleton()->PushHeap(HEAP_SYSTEMHEAP);
  idStr demoName(args->Argv(1));
  idMem::Singleton()->PopHeap();
  if(demoName.IsEmpty()) {
    idLib::Printf("Syntax: playDemo <demoName>\n");
    return;
  }
  demoName.DefaultFileExtension("demo");
  if(commonLocal.Field<int>(0x6D0)!=0) {
    commonLocal.StopSnapshotFilePlayback();
    commonLocal.StopSnapshotFileRecord();
    commonLocal.Game()->OnCloseSnapshot();
  }
  idMem::Singleton()->PushHeap(HEAP_SYSTEMHEAP);
  idFile* file=fileSystem->OpenFileReadRetail(demoName.c_str(),true,false);
  idMem::Singleton()->PopHeap();
  commonLocal.PlaySnapshotFile(file,0x44454D4FU);
}

void idCommonLocal::CaptureDemoSounds_f(const idCmdArgs& argsRef) {
  const idCmdArgs* args=&argsRef;
  if(args->argc<3) {
    idLib::Printf("Syntax: captureDemoSounds demo outname");
    return;
  }
  idStr demoName(args->Argv(1));
  idStr outName(args->Argv(2));
  demoName.DefaultFileExtension("demo");
  cvarSystem->SetCVarInteger("r_windowWidth",320,0);
  cvarSystem->SetCVarInteger("r_windowHeight",240,0);
  cvarSystem->SetCVarBool("r_fullscreen",false,0);
  cmdSystem->ExecuteCommandText("vid_restart");
  com_captureFrames.SetInteger(0,true);
  com_captureSounds.SetInteger(1,true);
  cvarSystem->SetCVarString("com_capturePath",(outName+"/sound/").c_str(),0);
  cvarSystem->SetCVarInteger("ai_debugLevel",0,0);
  cvarSystem->SetCVarBool("con_noPrint",true,0);
  cvarSystem->SetCVarFloat("s_volume_db",0.0f,0);
  if(commonLocal.Field<int>(0x6D0)!=0) {
    commonLocal.StopSnapshotFilePlayback();
    commonLocal.StopSnapshotFileRecord();
    commonLocal.Game()->OnCloseSnapshot();
  }
  idFile* file=fileSystem->OpenFileReadRetail(demoName.c_str(),true,false);
  commonLocal.PlaySnapshotFile(file,0x44454D4FU);
}

void idCommonLocal::CaptureDemoFrames_f(const idCmdArgs& argsRef) {
  const idCmdArgs* args=&argsRef;
  if(args->argc<3) {
    idLib::Printf("Syntax: captureDemoFrames demo outpath [fast]");
    return;
  }
  idStr demoName(args->Argv(1));
  idStr outName(args->Argv(2));
  idStr work(args->Argv(3));
  const bool fast=work.Icmp("fast")==0;
  outName.StripFileExtension();
  demoName.DefaultFileExtension("demo");
  cvarSystem->SetCVarInteger("r_windowWidth",fast ? 424 : 1280,0);
  cvarSystem->SetCVarInteger("r_windowHeight",fast ? 240 : 720,0);
  cvarSystem->SetCVarBool("r_fullscreen",false,0);
  cmdSystem->ExecuteCommandText("vid_restart");
  com_captureFrames.SetInteger(1,true);
  com_captureSounds.SetInteger(0,true);
  cvarSystem->SetCVarBool("com_captureTGA",!fast,0);
  cvarSystem->SetCVarString(
      "com_capturePath",(outName+(fast ? "/fast/" : "/frames/")).c_str(),0);
  cvarSystem->SetCVarInteger("ai_debugLevel",0,0);
  cvarSystem->SetCVarBool("con_noPrint",true,0);
  if(commonLocal.Field<int>(0x6D0)!=0) {
    commonLocal.StopSnapshotFilePlayback();
    commonLocal.StopSnapshotFileRecord();
    commonLocal.Game()->OnCloseSnapshot();
  }
  idFile* file=fileSystem->OpenFileReadRetail(demoName.c_str(),true,false);
  commonLocal.PlaySnapshotFile(file,0x44454D4FU);
}

void idCommonLocal::ExecuteMapChange(const idGameSpawnInfo& gameSpawnInfo,
                                     idFile* saveGameFile) {
  idStr active;
  idStr deactive;
  gameSpawnInfo.WriteLayersToCommaSeparatedStrings(active,deactive);
  idLib::Printf("ExecuteMapChange: %s a: %s d: %s\n",
                gameSpawnInfo.mapName.c_str(),active.c_str(),deactive.c_str());
  const bool activePeers=session->GetGameLobbyBase().HasActivePeers() ||
      session->GetGameStateLobbyBase().HasActivePeers();
  if(session->GetState()==7 && activePeers) {
    session->EndMatch(false);
    WaitOnSession(5,-1.0f,0);
    session->StartMatch();
    WaitOnSession(6,-1.0f,0);
    return;
  }

  idResourceList::AllowStaleChecks();
  PrepareForMapChange();
  ClearWarnings(gameSpawnInfo.mapName.c_str());
  if(session->GetState()!=6) {
    gameSpawnInfo.WriteLayersToCommaSeparatedStrings(active,deactive);
    idMatchParameters matchParms(
        -1,-1,-1,0,gameSpawnInfo.numPlayers,0,gameSpawnInfo.mapName,
        active,deactive,gameSpawnInfo.GetSpawnSpot(),gameSpawnInfo.loadingScreen,
        si_minRespawnDelay.valueFloat,si_timeLimit.valueFloat,
        si_scoreLimit.valueInteger,si_teamCount.valueInteger,
        si_swapTeams.valueInteger!=0,si_stopWatch.valueInteger!=0,
        gameSpawnInfo.comboMap,gameSpawnInfo.devmap,
        (gameSpawnInfo.gameSpawnFlags&0x10)!=0,gameSpawnInfo.forceHeapReset,
        gameSpawnInfo.currentCheckpointSlot,false);
    session->ClearSessionOption(-1);
    session->CreateMatch(matchParms);
    WaitOnSession(5,-1.0f,0);
    session->StartMatch();
    WaitOnSession(6,-1.0f,0);
  }
  FinalizeMapChange(gameSpawnInfo,saveGameFile);
}

void idCommonLocal::FinalizeMapChange(const idGameSpawnInfo& gameSpawnInfo,
                                      idFile* saveGameFile) {
  const int loadingID=session->GetLoadingID();
  ResetNetworkingState();
  resourceManager->SetFileHook();
  if(!fileSystem->FileExistsRetail(
         va("maps/%s/_combo/world.bcm",gameSpawnInfo.mapName.c_str()),true) &&
     !fileSystem->FileExistsRetail(
         va("%s/_combo/world.bcm",gameSpawnInfo.mapName.c_str()),true)) {
    idLib::Printf("Map '%s' does not exist\n",gameSpawnInfo.mapName.c_str());
    resourceManager->ReleaseFileHook();
    Disconnect(true);
    return;
  }
  resourceManager->ReleaseFileHook();

  float loadProgress=-1.0f;
  try {
    const int loadStart=static_cast<int>(Sys_Milliseconds());
    idVideoDesc videoDesc;
    videoDesc.fadeFromColor.r=-1.0f;
    videoDesc.fadeFromColor.g=-1.0f;
    videoDesc.fadeFromColor.b=-1.0f;
    videoDesc.fadeFromColor.a=-1.0f;
    idStrStatic<1024> mapName(gameSpawnInfo.mapName);
    char remappedName[512];
    const char* name=idDeclMapInfo::resourceList.RemapResourceName(
        remappedName,sizeof(remappedName),mapName.c_str());
    idDeclMapInfo* mapInfo=static_cast<idDeclMapInfo*>(
        idDeclMapInfo::resourceList.Load(name,false,false));
    if(mapInfo!=nullptr) {
      for(int index=0;index<mapInfo->loadingVideos.Num();++index) {
        const idDeclMapInfo::loadingVideos_t& loading=
            mapInfo->loadingVideos[index];
        const char* layers=loading.layerMask.str;
        bool allLayersActive=true;
        while(layers!=nullptr && *layers!='\0') {
          const int comma=idStr::Find(layers,',',0,-1);
          char layer[256];
          if(comma==-1) {
            std::strncpy(layer,layers,sizeof(layer));
            layer[sizeof(layer)-1]='\0';
            layers=nullptr;
          } else {
            std::strncpy(layer,layers,static_cast<std::size_t>(comma));
            layer[comma]='\0';
            layers+=comma+1;
          }
          bool active=false;
          for(int activeIndex=0;
              activeIndex<gameSpawnInfo.activeLayers.Num();++activeIndex) {
            if(idStr::Icmp(gameSpawnInfo.activeLayers[activeIndex].c_str(),
                           layer)==0) {
              active=true;
              break;
            }
          }
          if(!active) allLayersActive=false;
        }
        if(allLayersActive) {
          videoDesc.videoFile=va("bink/%s",loading.videoFile.str);
          videoDesc.fadeFromColor=loading.fadeFromColor;
          break;
        }
      }
    }
    videoDesc.videoFile=idVersionSelector::RemapVideoName(
        remappedName,sizeof(remappedName),videoDesc.videoFile.c_str());
    if((gameSpawnInfo.gameSpawnFlags&1)==0) videoDesc.videoFile.Empty();

    if(!IsDialogActive()) {
      SetShouldDelayShutdown(true);
      SetAbnormalFileRead(false);
      const idVideoDesc* selectedVideo=videoDesc.videoFile.IsEmpty()
          ? nullptr : &videoDesc;
      if(!resourceManager->AllocGameWithResourceFile(
             Field<idGame*>(0x1D0),gameSpawnInfo,saveGameFile,selectedVideo)) {
        StopLevelLoad(false);
        gameSystem->MinimalGameCleanup(Field<idGame*>(0x1D0));
        if(!IsDialogActive()) idLib::Error("Unable to allocate game");
      }
      SetShouldDelayShutdown(false);
    }

    if(Game()==nullptr || IsDialogActive()) {
      CanceledFinalizedMapChangeCleanUp();
      if(IsDialogActive()) return;
    } else {
      Game()->Shell_CreateMenu(true);
      if(idStr::Icmp(gameSpawnInfo.mapName.c_str(),"outro")==0) {
        const bool consoleActive=console->IsActive();
        session->MoveToPressStart();
        LeaveGame(false);
        if(consoleActive) cmdSystem->ExecuteCommandText("activateConsole 0.5");
        return;
      }
      time_mapLoadMS=static_cast<int>(Sys_Milliseconds())-loadStart;
      idLib::Printf("%.1f seconds to load %s\n",
                    static_cast<float>(time_mapLoadMS)*0.001f,
                    gameSpawnInfo.mapName.c_str());
    }
  } catch(idException* exception) {
    idLib::Printf("Caught an exception in FinalizeMapChange\n");
    SetShouldDelayShutdown(false);
    CanceledFinalizedMapChangeCleanUp();
    idException copy=*exception;
    throw &copy;
  }

  Game()->GenerateInputToPlayerIndexRouting();
  if(session->GetState()!=6 || session->GetLoadingID()!=loadingID ||
     session->GetUserSignedOutDuringLoading()) {
    StopLevelLoad(false);
    gameSystem->FreeGame(Field<idGame*>(0x1D0));
    if(com_newLoadScreen.valueInteger!=0) loadScreen.LevelLoadCompleted();
    LoadMainMenu();
    if(session->GetUserSignedOutDuringLoading()) {
      std::strncpy(idException::error,"Loading aborted due to sign-out",
                   idException::MAX_ERROR_LEN);
      idNetworkLoadException exception;
      throw &exception;
    }
    if(session->GetLoadingID()!=loadingID) {
      std::strncpy(idException::error,
          "FinalizeMapChange: Started loading a new map from within an existing map load.",
          idException::MAX_ERROR_LEN);
      idNetworkLoadException exception;
      throw &exception;
    }
    idStr::snPrintf(idException::error,idException::MAX_ERROR_LEN,
        "FinalizeMapChange: State was %s but expected LOADING.",
        session->GetStateString());
    idNetworkLoadException exception;
    throw &exception;
  }

  session->LoadingFinished();
  try {
    if(session->GetActingGameStateLobbyBase().HasActivePeers())
      loadProgress=1.0f;
    WaitOnSession(7,loadProgress,18);
  } catch(idNetworkLoadException* exception) {
    renderSystem->EndAutomaticBackgroundSwaps();
    session->GetSaveGameManager()->CancelAllProcessors(true);
    StopLevelLoad(false);
    gameSystem->FreeGame(Field<idGame*>(0x1D0));
    if(com_newLoadScreen.valueInteger!=0) loadScreen.LevelLoadCompleted();
    LoadMainMenu();
    idNetworkLoadException copy=*exception;
    throw &copy;
  }

  if(session->GetActingGameStateLobbyBase().IsHost() &&
     session->GetActingGameStateLobbyBase().HasActivePeers()) {
    gameReturn_t gameReturn;
    idSnapShot snapshot;
    Game()->RunFrame(GameFrameParms(1),&gameReturn);
    PrepareSnapshot(snapshot,gameReturn,nullptr);
    session->SendSnapshot(snapshot);
    while(!session->GetActingGameStateLobbyBase().EnsureAllPeersHaveBaseState()) {
      UpdateLevelLoadPacifier(1.0f,static_cast<pacifierState_t>(18),true,true);
      Sys_Sleep(10);
    }
  }
  if(session->GetActingGameStateLobbyBase().HasActivePeers())
    UpdateLevelLoadPacifier(1.0f,PACIFIER_STATE_0,true,false);

  if(com_newLoadScreen.valueInteger!=0) {
    gameReturn_t gameReturn;
    Game()->RunFrame(GameFrameParms(1),&gameReturn);
    loadScreen.LevelLoadCompleted();
  }
  std::memset(&UserCmdMgr(),0,0x26000);
  std::memset(UserCmdMgr().writeFrame,0,sizeof(UserCmdMgr().writeFrame));
  std::memset(UserCmdMgr().readFrame,-1,sizeof(UserCmdMgr().readFrame));
  console->ClearNotifyLines();
  sys->ClearEvents();
  fileSystem->ClearZipCacheRetail();
  if((gameSpawnInfo.gameSpawnFlags&0x10)==0)
    Game()->RequestSaveGame(SAVEGAME_AUTOSAVE_FOLDER,true,false);
  idList<idMapChangeListener*,5>& listeners=
      Field<idList<idMapChangeListener*,5>>(0x34CA8);
  for(int index=0;index<listeners.Num();++index)
    listeners[index]->MapLoaded(Game()->GetLevelMap());
}

void idCommonLocal::RestartMap_f(const idCmdArgs&) {
  if(commonLocal.Game()->IsGameActive(true)) {
    cvarSystem->SetCVarBool("g_stopTime",false,0);
    idGameSpawnInfo gameSpawnInfo(*commonLocal.Game()->GetGameSpawnInfo());
    gameSpawnInfo.spawnSpot="";
    if(commonLocal.Game()->GetMapStartInfoFile()!=nullptr)
      commonLocal.Game()->GetMapStartInfoFile()->MakeReadOnly();
    if(commonLocal.Game()->CheckpointGetSpawninfoFromSlot(0,gameSpawnInfo)) {
      gameSpawnInfo.currentCheckpointSlot=0;
      commonLocal.Game()->CheckpointCapFileData(0);
    }
    gameSpawnInfo.gameSpawnFlags|=0x10;
    commonLocal.ExecuteMapChange(
        gameSpawnInfo,commonLocal.Game()->GetMapStartInfoFile());
  } else {
    idLib::Printf("restartMap only works if a map is running!");
  }
}

void idCommonLocal::RestartMapHere_f(const idCmdArgs&) {
  if(commonLocal.Game()->IsGameActive(true)) {
    cvarSystem->SetCVarBool("g_stopTime",false,0);
    idCmdArgs viewArgs;
    viewArgs.argc=0;
    Cmd_GetViewpos_f(&viewArgs);
    idGameSpawnInfo gameSpawnInfo(*commonLocal.Game()->GetGameSpawnInfo());
    gameSpawnInfo.currentCheckpointSlot=-1;
    gameSpawnInfo.spawnSpotOverride="";
    if(commonLocal.Game()->GetMapStartInfoFile()!=nullptr)
      commonLocal.Game()->GetMapStartInfoFile()->MakeReadOnly();
    gameSpawnInfo.gameSpawnFlags|=0x10;
    commonLocal.ExecuteMapChange(
        gameSpawnInfo,commonLocal.Game()->GetMapStartInfoFile());
    idGameLock gameLock(commonLocal.Game());
    viewArgs.AppendArg("setviewpos");
    viewArgs.AppendArg("last");
    Cmd_SetViewpos_f(&viewArgs);
  } else {
    idLib::Printf("restartMapHere only works if a map is running!");
  }
}
