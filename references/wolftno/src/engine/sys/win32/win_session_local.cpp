#include "engine/sys/win32/win_session_local.h"

#include "shared/idlib/csystems/cvarsystemlocal.h"
#include "shared/idlib/lib.h"
#include "shared/idlib/networking/networking.h"

#include <algorithm>

namespace { winSessionHooks_t sessionHooks{}; }
idCVar si_map("si_map", "", CVAR_ARCHIVE, "map to use when starting a server");
idCVar si_layer("si_layer", "", CVAR_ARCHIVE, "active map layers");
idCVar si_layerDeactive("si_layerDeactive", "", CVAR_ARCHIVE, "inactive map layers");
idSessionLocalWin sessionLocalWin;

idLocalUserWin::idLocalUserWin() = default;
idLocalUserWin::~idLocalUserWin() = default;
unsigned idLocalUserWin::GetOnlineCaps() { return IsPersistent() && IsOnline() ? 5U : 0U; }
int idLocalUserWin::GetInputDevice() { return inputDevice; }

idSignInManagerWin::idSignInManagerWin() {
  localUsers.reserve(4);
  Sys_SetActiveSignInManager(this);
}
idSignInManagerWin::~idSignInManagerWin() {
  Shutdown();
  if (Sys_GetActiveSignInManager() == this) Sys_SetActiveSignInManager(nullptr);
}
idLocalUser* idSignInManagerWin::GetLocalUserByIndex(int index) {
  return index >= 0 && index < GetNumLocalUsers() ? localUsers[index].get() : nullptr;
}
const idLocalUser* idSignInManagerWin::GetLocalUserByIndex(int index) const {
  return index >= 0 && index < GetNumLocalUsers() ? localUsers[index].get() : nullptr;
}
idVoiceChatMgrSteam::idVoiceChatMgrSteam() = default;

void Win_SetSessionHooks(const winSessionHooks_t& hooks) { sessionHooks = hooks; }
void SteamAPIDebugTextHook(int severity,const char* text) {
  if(severity>=1)idLib::Warning("Steam: %s",text!=nullptr?text:"");
  else idLib::Printf("Steam: %s",text!=nullptr?text:"");
}

idLobbyBackend* idLobbyToSessionCBLocal::GetLobbyBackend(int type) {
  return sessionLocalWin!=nullptr?sessionLocalWin->GetLobbyBackend(type):nullptr;
}
bool idLobbyToSessionCBLocal::CanJoinLocalHost(const idMatchParameters&,int) {
  return sessionLocalWin!=nullptr&&sessionLocalWin->CanJoinLocalHost();
}

idSessionLocalWin::idSessionLocalWin() { lobbyToSessionCB=std::make_unique<idLobbyToSessionCBLocal>(this); }
idSessionLocalWin::~idSessionLocalWin(){Shutdown();}
void idSessionLocalWin::InitializeSoundRelatedSystems(){if(voiceChat==nullptr){voiceChat=std::make_unique<idVoiceChatMgrSteam>();voiceChat->Init();}}
void idSessionLocalWin::ShutdownSoundRelatedSystems(){if(voiceChat!=nullptr)voiceChat->Shutdown();voiceChat.reset();}
void idSessionLocalWin::ShowServerGamerCardUI(int index){if(steamLobbySearch!=nullptr)steamLobbySearch->ShowServerGamerCardUI(index);}
bool idSessionLocalWin::ProcessInputEvent(const sysEvent_t* event){return signInManager!=nullptr&&signInManager->ProcessInputEvent(event);}
void idSessionLocalWin::SetSystemUIShowing(bool show){systemUIShowing=show;}
void idSessionLocalWin::LoadInvite(std::uint64_t lobbyId){HandleBootableInvite(lobbyId);}
void idSessionLocalWin::ClearBootableInvite(){pendingBootGameInviteInfo={};}
void idSessionLocalWin::HandleBootableInvite(std::uint64_t lobbyId){pendingBootGameInviteInfo.pending=lobbyId!=0;pendingBootGameInviteInfo.steamId=lobbyId;}
bool idSessionLocalWin::HasPendingBootableInvite()const{return pendingBootGameInviteInfo.pending;}
void idSessionLocalWin::LeaderboardDownloadAttachment(int,const leaderboardDefinition_t*,std::int64_t attachmentId){
  if(steamLeaderboards!=nullptr){std::vector<std::uint8_t> ignored;steamLeaderboards->DownloadAttachment(static_cast<std::uint64_t>(attachmentId),ignored);}}
void idSessionLocalWin::Initialize(){if(initialized)return;InitSteam();ConstructSteamObjects();InitializeSoundRelatedSystems();EnumerateDownloadableContent();initialized=true;}
void idSessionLocalWin::InviteFriends(){if(sessionHooks.showInviteUI!=nullptr)sessionHooks.showInviteUI(pendingBootGameInviteInfo.steamId);}
void idSessionLocalWin::ShowSystemMarketplaceUI(){if(sessionHooks.showMarketplace!=nullptr)sessionHooks.showMarketplace();}
void idSessionLocalWin::DestroySteamObjects(){steamLobbySearch.reset();steamLeaderboards.reset();steamStats.reset();steamStatsInitialized=false;}
void idSessionLocalWin::OnP2PSessionConnectFail(P2PSessionConnectFail_t* info){if(info!=nullptr)deferredCloseP2PWithUser.push_back(info->remoteId);}
void idSessionLocalWin::OnStatsReceived(UserStatsReceived_t* result){if(steamStats!=nullptr)steamStats->OnStatsReceived(result);steamStatsInitialized=result!=nullptr&&result->result==1;}
void idSessionLocalWin::PumpSteam(){if(!steamInitialized)return;if(sessionHooks.steamPump!=nullptr)sessionHooks.steamPump();if(steamStats!=nullptr)steamStats->Pump();if(steamLeaderboards!=nullptr)steamLeaderboards->Pump();}
void idSessionLocalWin::ListServers(const idCallback* callback){if(steamLobbySearch==nullptr)steamLobbySearch=std::make_unique<idSteamLobbySearch>();steamLobbySearch->BrowserSearch(callback,1,0);}
void idSessionLocalWin::CancelListServers(){if(steamLobbySearch!=nullptr)steamLobbySearch->Clear();}
int idSessionLocalWin::NumServers()const{return steamLobbySearch!=nullptr?steamLobbySearch->NumServers():0;}
const serverInfo_t* idSessionLocalWin::ServerInfo(int index)const{return steamLobbySearch!=nullptr?steamLobbySearch->DescribeServerAtIndex(index):nullptr;}
void idSessionLocalWin::ConnectToServer(int index){if(steamLobbySearch==nullptr)return;lobbyConnectInfo_t info;info.usingBackendPlatform=true;info.platformId=steamLobbySearch->ServerAtIndex(index);if(info.platformId!=0&&sessionHooks.connectToLobby!=nullptr)sessionHooks.connectToLobby(info);}
void idSessionLocalWin::ShowLobbyUserGamerCardUI(lobbyUserID_t userId){if(sessionHooks.showUserProfile!=nullptr)sessionHooks.showUserProfile(userId.localUserHandle.handle);}
void idSessionLocalWin::OnGameLobbyJoinRequested(GameLobbyJoinRequested_t* request){if(request!=nullptr)HandleBootableInvite(request->lobbyId);}
void idSessionLocalWin::HandleServerQueryRequest(lobbyAddress_t* remote,idBitMsg*,int){if(remote!=nullptr)idLib::Printf("HandleServerQueryRequest from %s\n",remote->ToString());}
void idSessionLocalWin::HandleServerQueryAck(lobbyAddress_t* remote,idBitMsg*){if(remote!=nullptr)idLib::Printf("HandleServerQueryAck from %s\n",remote->ToString());}
void idSessionLocalWin::StatsStore(){if(steamStats!=nullptr&&steamStats->Store())flushedStats=true;}
void idSessionLocalWin::StatsUpload(lobbyUserID_t,const std::int64_t* stats,const std::int64_t* values,int count){if(steamStats==nullptr||stats==nullptr||values==nullptr)return;for(int i=0;i<count;++i)steamStats->SetInt(static_cast<int>(stats[i]),static_cast<unsigned>(values[i]));}
void idSessionLocalWin::LeaderboardUpload(lobbyUserID_t,const leaderboardDefinition_t* def,const column_t* stats,const idFile_Memory*){if(steamLeaderboards!=nullptr&&steamLeaderboards->QueueUpload(0,def,stats,nullptr,0))flushedStats=true;}
void idSessionLocalWin::EnsurePort(){if(!port.initialized){port.initialized=true;port.usingBackendPlatform=steamInitialized;}if(!dedicatedPort.initialized){dedicatedPort.initialized=true;dedicatedPort.usingBackendPlatform=steamInitialized;}}
idLobbyPort* idSessionLocalWin::GetPort(bool dedicated){EnsurePort();return dedicated?&dedicatedPort:&port;}
void idSessionLocalWin::DestroyLobbyBackend(idLobbyBackend* backend){for(auto& item:lobbyBackends)if(item.get()==backend){item.reset();break;}}
void idSessionLocalWin::PumpLobbies(){for(auto& item:lobbyBackends)if(item!=nullptr)item->Pump();}
idLobbyBackendWin* idSessionLocalWin::CreateLobbyInternal(idLobbyBackend::lobbyBackendType_t type){if(type<0||type>=3)return nullptr;EnsurePort();lobbyBackends[type]=std::make_unique<idLobbyBackendWin>(lobbyToSessionCB.get());return lobbyBackends[type].get();}
void idSessionLocalWin::JoinAfterSwap(const std::uint64_t* inviteInfo){if(inviteInfo!=nullptr)HandleBootableInvite(*inviteInfo);}
void idSessionLocalWin::Shutdown(){if(!initialized&&!steamInitialized)return;for(auto& item:lobbyBackends)item.reset();ShutdownSoundRelatedSystems();DestroySteamObjects();if(signInManager!=nullptr)signInManager->Shutdown();signInManager.reset();achievementSystem.reset();if(steamInitialized&&sessionHooks.steamShutdown!=nullptr)sessionHooks.steamShutdown();steamInitialized=false;initialized=false;}
void idSessionLocalWin::PlatformPump(){PumpSteam();PumpLobbies();if(voiceChat!=nullptr)voiceChat->Pump();for(std::uint64_t id:deferredCloseP2PWithUser)if(sessionHooks.closeP2PSession!=nullptr)sessionHooks.closeP2PSession(id);deferredCloseP2PWithUser.clear();if(HasPendingBootableInvite()&&sessionHooks.connectToLobby!=nullptr){lobbyConnectInfo_t info;info.usingBackendPlatform=true;info.platformId=pendingBootGameInviteInfo.steamId;sessionHooks.connectToLobby(info);ClearBootableInvite();}}
void idSessionLocalWin::ConstructSteamObjects(){if(!steamInitialized)return;achievementSystem=std::make_unique<idAchievementSystemWin>();signInManager=std::make_unique<idSignInManagerWin>();steamLeaderboards=std::make_unique<idSteamLeaderboards>();steamLobbySearch=std::make_unique<idSteamLobbySearch>();steamStats=std::make_unique<idSteamStats>();}
void idSessionLocalWin::OnP2PSessionRequest(P2PSessionRequest_t* request){if(request!=nullptr&&sessionHooks.acceptP2PSession!=nullptr)sessionHooks.acceptP2PSession(request->remoteId);}
void idSessionLocalWin::Connect_f(const idCmdArgs* args) {
  if (args == nullptr || args->argc < 2 || args->argv[1] == nullptr) {
    idLib::Printf("Usage: connect <address>. Use with net_port.\n");
    return;
  }
  if (signInManager == nullptr) signInManager = std::make_unique<idSignInManagerWin>();
  if (signInManager->GetNumLocalUsers() == 0) signInManager->RegisterLocalUser(0);
  lobbyConnectInfo_t info;
  info.usingBackendPlatform = false;
  if (!Sys_StringToNetAdr(args->argv[1], info.netAddr, true)) {
    idLib::Warning("Unable to resolve server address '%s'", args->argv[1]);
    return;
  }
  if (sessionHooks.connectToLobby != nullptr) sessionHooks.connectToLobby(info);
}
void idSessionLocalWin::LeaderboardDownload(int,const leaderboardDefinition_t* def,int start,int rows,const idLeaderboardCallback* callback){if(steamLeaderboards!=nullptr)steamLeaderboards->BeginDownload(0,def,start,rows,callback);}
idLobbyBackendWin* idSessionLocalWin::CreateLobbyBackend(const idMatchParameters* p,float skill,idLobbyBackend::lobbyBackendType_t type){auto* backend=CreateLobbyInternal(type);if(backend!=nullptr&&p!=nullptr)backend->StartHosting(*p,skill,type);return backend;}
idLobbyBackendWin* idSessionLocalWin::FindLobbyBackend(const idMatchParameters* p,unsigned users,float skill,idLobbyBackend::lobbyBackendType_t type){auto* backend=CreateLobbyInternal(type);if(backend!=nullptr&&p!=nullptr)backend->StartFinding(*p,users,skill);return backend;}
idLobbyBackendWin* idSessionLocalWin::JoinFromConnectInfo(const lobbyConnectInfo_t* info,idLobbyBackend::lobbyBackendType_t type){auto* backend=CreateLobbyInternal(type);if(backend!=nullptr&&info!=nullptr)backend->JoinFromConnectInfo(*info);return backend;}
void idSessionLocalWin::InitSteam(){steamInitialized=sessionHooks.steamInitialize==nullptr||sessionHooks.steamInitialize();steamFailed=!steamInitialized;Sys_SetSteamInitialized(steamInitialized);}
bool idSessionLocalWin::IsSteamInitialized() const { return steamInitialized; }
bool idSessionLocalWin::IsSteamStatsInitialized() const { return steamStatsInitialized; }
void idSessionLocalWin::StartServer_f(const idCmdArgs* args) {
  idMatchParameters parameters;
  parameters.gameMode = 0;
  parameters.searchMode = -1;
  parameters.gameMap = -1;
  parameters.gameType = 0;
  parameters.numSlots = 16;
  parameters.startCheckSlot = -1;
  parameters.matchFlags = 2;
  parameters.mapName = si_map.GetString();
  parameters.layersActive = si_layer.GetString();
  parameters.layersDeactive = si_layerDeactive.GetString();
  parameters.spawnSpot = "player_start";
  parameters.numBots = -1;
  parameters.botSkill = 1;
  parameters.skillLevel = 0;
  if (sessionHooks.createMatch != nullptr) sessionHooks.createMatch(parameters);
  if (args != nullptr && sessionHooks.startServer != nullptr) sessionHooks.startServer(*args);
}
void idSessionLocalWin::EnumerateDownloadableContent(){contentPackages=sessionHooks.enumerateContent!=nullptr?sessionHooks.enumerateContent():std::vector<winContentPackage_t>{};}
idLobbyBackend* idSessionLocalWin::GetLobbyBackend(int type)const{return type>=0&&type<3?lobbyBackends[type].get():nullptr;}
void Connect_f(const idCmdArgs* args){sessionLocalWin.Connect_f(args);}
void StartServer_f(const idCmdArgs* args){sessionLocalWin.StartServer_f(args);}
