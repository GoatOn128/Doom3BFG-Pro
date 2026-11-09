#pragma once

#include <cstdint>

// NEEDS_WORK(engine/framework/session): PDB-exact interface slice required by
// renderer/renderthread.cpp.  The full session implementation remains owned by
// the framework/session recovery pass.

class idSaveGameManager;
class idBitMsg;
class idRenderModelGui;
class idFile_SaveGame;
class idSaveGameDetails;
class idPlayerProfile;
class idSignInManagerBase;
template <typename T,int Capacity> class idStaticList;

class idLobbyBase {
public:
  const void* const* vtable;
  bool IsHost() {
    using Fn=bool(*)(idLobbyBase*);
    return reinterpret_cast<Fn>(vtable[1])(this);
  }
  bool IsPeer() {
    using Fn=bool(*)(idLobbyBase*);
    return reinterpret_cast<Fn>(vtable[2])(this);
  }
  bool HasActivePeers() {
    using Fn=bool(*)(idLobbyBase*);
    return reinterpret_cast<Fn>(vtable[3])(this);
  }
  int GetNumLobbyUsers() {
    using Fn=int(*)(idLobbyBase*);
    return reinterpret_cast<Fn>(vtable[4])(this);
  }
  const struct idMatchParameters& GetMatchParms() {
    using Fn=const idMatchParameters&(*)(idLobbyBase*);
    return reinterpret_cast<Fn>(vtable[36])(this);
  }
  void DrawDebugNetworkHUD_ServerSnapshotMetrics(idRenderModelGui* gui,bool draw) {
    using Fn=void(*)(idLobbyBase*,idRenderModelGui*,bool);
    reinterpret_cast<Fn>(vtable[51])(this,gui,draw);
  }
  void DrawDebugNetworkHUD(idRenderModelGui* gui) {
    using Fn=void(*)(idLobbyBase*,idRenderModelGui*);
    reinterpret_cast<Fn>(vtable[49])(this,gui);
  }
  bool EnsureAllPeersHaveBaseState() {
    using Fn=bool(*)(idLobbyBase*);
    return reinterpret_cast<Fn>(vtable[38])(this);
  }
};

class idSession {
public:
  enum sessionGoToStoreState_t : int {
    GO_TO_STORE_INVALID = 0,
    GO_TO_STORE_WAITING_FOR_ANSWER = 1,
    GO_TO_STORE_YES = 2,
    GO_TO_STORE_NO = 3,
  };

  const void* const* vtable;

  void Initialize() {
    using Fn=void(*)(idSession*);
    reinterpret_cast<Fn>(vtable[197])(this);
  }

  void InitForge() {
    using Fn=void(*)(idSession*);
    reinterpret_cast<Fn>(vtable[202])(this);
  }

  void HandleBootableInvite(std::int64_t lobbyId) {
    using Fn=void(*)(idSession*,std::int64_t);
    reinterpret_cast<Fn>(vtable[362])(this,lobbyId);
  }

  void InitializeSoundRelatedSystems() {
    using Fn=void(*)(idSession*);
    reinterpret_cast<Fn>(vtable[3])(this);
  }

  void ShutdownSoundRelatedSystems() {
    using Fn=void(*)(idSession*);
    reinterpret_cast<Fn>(vtable[4])(this);
  }

  void Shutdown() {
    using Fn=void(*)(idSession*);
    reinterpret_cast<Fn>(vtable[2])(this);
  }

  void MoveToPressStart() {
    using Fn=void(*)(idSession*);
    reinterpret_cast<Fn>(vtable[23])(this);
  }
  void QuitMatchToTitle() {
    using Fn=void(*)(idSession*);
    reinterpret_cast<Fn>(vtable[17])(this);
  }

  bool IsLosingConnectionToHost() {
    using Fn=bool(*)(idSession*);
    return reinterpret_cast<Fn>(vtable[28])(this);
  }

  void SetUserSignedOutDuringLoading(bool signedOut) {
    using Fn=void(*)(idSession*,bool);
    reinterpret_cast<Fn>(vtable[57])(this,signedOut);
  }
  bool GetUserSignedOutDuringLoading() {
    using Fn=bool(*)(idSession*);
    return reinterpret_cast<Fn>(vtable[58])(this);
  }

  void Pump() {
    using Fn=void(*)(idSession*);
    reinterpret_cast<Fn>(vtable[35])(this);
  }
  void EndMatch(bool premature) {
    using Fn=void(*)(idSession*,bool);
    reinterpret_cast<Fn>(vtable[14])(this,premature);
  }
  void JoinAfterSwap(void* inviteInfo) {
    using Fn=void(*)(idSession*,void*);
    reinterpret_cast<Fn>(vtable[72])(this,inviteInfo);
  }
  void ClearSessionOption(int option) {
    using Fn=void(*)(idSession*,int);
    reinterpret_cast<Fn>(vtable[19])(this,option);
  }
  void CreateMatch(const idMatchParameters& parameters) {
    using Fn=void(*)(idSession*,const idMatchParameters&);
    reinterpret_cast<Fn>(vtable[9])(this,parameters);
  }
  void StartMatch() {
    using Fn=void(*)(idSession*);
    reinterpret_cast<Fn>(vtable[13])(this);
  }
  bool CheckMapInstallAndShowGui(const char* mapName,bool block,
                                 bool showGui) {
    using Fn=bool(*)(idSession*,const char*,bool,bool);
    return reinterpret_cast<Fn>(vtable[137])(this,mapName,block,showGui);
  }
  bool ProcessInputEvent(const struct sysEvent_t& event) {
    using Fn=bool(*)(idSession*,const sysEvent_t&);
    return reinterpret_cast<Fn>(vtable[51])(this,event);
  }

  void ProcessSnapAckQueue() {
    using Fn=void(*)(idSession*);
    reinterpret_cast<Fn>(vtable[36])(this);
  }
  int GetLoadingID() {
    using Fn=int(*)(idSession*);
    return reinterpret_cast<Fn>(vtable[55])(this);
  }
  void LoadingFinished() {
    using Fn=void(*)(idSession*);
    reinterpret_cast<Fn>(vtable[26])(this);
  }

  void UpdateSignInManager() {
    using Fn=void(*)(idSession*);
    reinterpret_cast<Fn>(vtable[144])(this);
  }
  void PumpSystemServiceStatus() {
    using Fn=void(*)(idSession*);
    reinterpret_cast<Fn>(vtable[173])(this);
  }

  idLobbyBase& GetActingGameStateLobbyBase() {
    using Fn=idLobbyBase&(*)(idSession*);
    return reinterpret_cast<Fn>(vtable[45])(this);
  }
  idLobbyBase& GetGameLobbyBase() {
    using Fn=idLobbyBase&(*)(idSession*);
    return reinterpret_cast<Fn>(vtable[42])(this);
  }
  idLobbyBase& GetGameStateLobbyBase() {
    using Fn=idLobbyBase&(*)(idSession*);
    return reinterpret_cast<Fn>(vtable[43])(this);
  }

  idPlayerProfile* GetProfileFromInputDevice(int inputDevice) {
    using Fn=idPlayerProfile*(*)(idSession*,int);
    return reinterpret_cast<Fn>(vtable[50])(this,inputDevice);
  }

  int GetState() {
    using Fn=int(*)(idSession*);
    return reinterpret_cast<Fn>(vtable[60])(this);
  }
  const char* GetStateString() {
    using Fn=const char*(*)(idSession*);
    return reinterpret_cast<Fn>(vtable[61])(this);
  }
  const char* GetStateString(int state) {
    using Fn=const char*(*)(idSession*,int);
    return reinterpret_cast<Fn>(vtable[62])(this,state);
  }

  float GetTitleStorageFloat(const char* name,float defaultValue=0.0f) const {
    using Fn=float(*)(const idSession*,const char*,float);
    return reinterpret_cast<Fn>(vtable[88])(this,name,defaultValue);
  }

  void SendSnapshot(idSnapShot& snapshot) {
    using Fn=void(*)(idSession*,idSnapShot&);
    reinterpret_cast<Fn>(vtable[142])(this,snapshot);
  }
  idPlayerProfile* GetProfileFromMasterLocalUser() {
    using Fn=idPlayerProfile*(*)(idSession*);
    return reinterpret_cast<Fn>(vtable[143])(this);
  }
  idSignInManagerBase& GetSignInManager() {
    using Fn=idSignInManagerBase&(*)(idSession*);
    return reinterpret_cast<Fn>(vtable[145])(this);
  }
  bool IsSystemUIShowing() {
    using Fn=bool(*)(idSession*);
    return reinterpret_cast<Fn>(vtable[149])(this);
  }

  int GetNumContentPackages() {
    using Fn = int (*)(idSession*);
    return reinterpret_cast<Fn>(vtable[75])(this);
  }

  int GetContentPackageID(int index) {
    using Fn = int (*)(idSession*, int);
    return reinterpret_cast<Fn>(vtable[76])(this, index);
  }

  const char* GetContentPackagePath(int index) {
    using Fn = const char* (*)(idSession*, int);
    return reinterpret_cast<Fn>(vtable[77])(this, index);
  }

  void SetGoToStoreState(sessionGoToStoreState_t state) {
    using Fn=void(*)(idSession*,sessionGoToStoreState_t);
    reinterpret_cast<Fn>(vtable[86])(this,state);
  }

  int SaveGameSync(const char* folder,
                   const idStaticList<idFile_SaveGame*,10>& files,
                   const idSaveGameDetails& details,bool skipErrorDialog,
                   bool waitForAlreadyInProgress) {
    using Fn=int(*)(idSession*,const char*,
                    const idStaticList<idFile_SaveGame*,10>&,
                    const idSaveGameDetails&,bool,bool);
    return reinterpret_cast<Fn>(vtable[107])(
        this,folder,files,details,skipErrorDialog,waitForAlreadyInProgress);
  }

  idSaveGameManager* GetSaveGameManager() {
    using Fn = idSaveGameManager* (*)(idSession*);
    return reinterpret_cast<Fn>(vtable[146])(this);
  }
  const char* GetCurrentSaveSlot() {
    using Fn=const char*(*)(idSession*);
    return reinterpret_cast<Fn>(vtable[130])(this);
  }
  void VoiceRecognition_ClearUnprocessedPhrases() {
    using Fn=void(*)(idSession*);
    reinterpret_cast<Fn>(vtable[161])(this);
  }

  int GetTitleStorageInt(const char* name,int fallback) {
    using Fn=int(*)(idSession*,const char*,int);
    return reinterpret_cast<Fn>(vtable[90])(this,name,fallback);
  }

  void SendUsercmds(idBitMsg& msg) {
    using Fn=void(*)(idSession*,idBitMsg&);
    reinterpret_cast<Fn>(vtable[141])(this,msg);
  }
};

extern idSession* session;

static_assert(sizeof(idSession) == 8, "session ABI drift");
static_assert(sizeof(idLobbyBase) == 8, "lobby ABI drift");
