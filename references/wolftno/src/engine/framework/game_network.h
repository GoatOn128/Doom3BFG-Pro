#pragma once

#include "engine/framework/common.h"
#include "engine/framework/keyinput.h"
#include "engine/framework/usercmd.h"
#include "engine/renderer/render.h"
#include "shared/idlib/containers/staticlist.h"
#include "shared/idlib/math/vector.h"

#include <cstddef>
#include <cstdint>

class idBitMsg;
class idCmdArgs;
class idFile_SaveGame;
class idGameSpawnInfo;
class idRenderModelGui;
class idSnapShot;
class idGameTimeManager;
class idRenderWorld {
public:
  const void* const* vtable;
  idRenderDebug* GetRenderDebug() {
    using Fn=idRenderDebug*(*)(idRenderWorld*);
    return reinterpret_cast<Fn>(vtable[37])(this);
  }
  void ReadFromSnapshot(idSnapShot& snap,const idSnapShot* oldSnap) {
    using Fn=void(*)(idRenderWorld*,idSnapShot&,const idSnapShot*);
    reinterpret_cast<Fn>(vtable[35])(this,snap,oldSnap);
  }
  void WriteToSnapshot(idSnapShot& snap,const idSnapShot* oldSnap) {
    using Fn=void(*)(idRenderWorld*,idSnapShot&,const idSnapShot*);
    reinterpret_cast<Fn>(vtable[34])(this,snap,oldSnap);
  }
  void AdvanceTime() {
    using Fn=void(*)(idRenderWorld*);
    reinterpret_cast<Fn>(vtable[31])(this);
  }
  void GenerateAllInteractions() {
    using Fn=void(*)(idRenderWorld*);
    reinterpret_cast<Fn>(vtable[36])(this);
  }
  void UpdateDebugTime(unsigned milliseconds) {
    using GetFn=void*(*)(idRenderWorld*);
    void* debug=reinterpret_cast<GetFn>(vtable[37])(this);
    const auto slots=*reinterpret_cast<const void* const* const*>(debug);
    reinterpret_cast<void(*)(void*,unsigned)>(slots[1])(debug,milliseconds);
  }
};
#define WOLFTNO_ID_SOUND_WORLD_DEFINED 1
class idSoundWorld {
public:
  const void* const* vtable;
  void ReadFromSnapshot(idSnapShot& snap,const idSnapShot* oldSnap) {
    using Fn=void(*)(idSoundWorld*,idSnapShot&,const idSnapShot*);
    reinterpret_cast<Fn>(vtable[33])(this,snap,oldSnap);
  }
  void WriteToSnapshot(idSnapShot& snap,const idSnapShot* oldSnap) {
    using Fn=void(*)(idSoundWorld*,idSnapShot&,const idSnapShot*);
    reinterpret_cast<Fn>(vtable[32])(this,snap,oldSnap);
  }
  void AdvanceTime() {
    using Fn=void(*)(idSoundWorld*);
    reinterpret_cast<Fn>(vtable[38])(this);
  }
  void PlaceListener(const idVec3& origin,const idMat3& axis,
                     const idVec3& velocity,int listenerId) {
    using Fn=void(*)(idSoundWorld*,const idVec3&,const idMat3&,const idVec3&,int);
    reinterpret_cast<Fn>(vtable[17])(this,origin,axis,velocity,listenerId);
  }
  void SetNumListeners(int count) {
    reinterpret_cast<void(*)(idSoundWorld*,int)>(vtable[18])(this,count);
  }
  void Pause(int fade) {
    reinterpret_cast<void(*)(idSoundWorld*,int)>(vtable[35])(this,fade);
  }
  void UnPause() {
    reinterpret_cast<void(*)(idSoundWorld*)>(vtable[36])(this);
  }
  bool IsPaused() const {
    return reinterpret_cast<bool(*)(const idSoundWorld*)>(vtable[37])(this);
  }
};

struct inputSettings_t {
  bindSet_t bindset;
  float mouseSensScale;
  float joySensScale;
  float joyPitchSpeed;
  float joyYawSpeed;
  float mouseSens;
  bool absoluteAngles;
  bool lookInvert;
  std::byte padding[2];
  idAngles aimAssistAngles;
};

struct gameReturnPlayer_t {
  gameReturnPlayer_t();
  ~gameReturnPlayer_t();
  bool valid;
  std::byte padding[15];
  renderView_t view;
  void* gui;
  void* inventoryModel;
  void* hudModel;
  inputSettings_t inputSettings;
  int vibrationLow;
  int vibrationHigh;
  int vibrationLeft;
  int vibrationRight;
  std::uint32_t visMask;
  idVec3 velocity;
};

struct alignas(16) gameReturn_t {
  gameReturn_t();
  ~gameReturn_t();
  gameReturn_t& operator=(const gameReturn_t&);
  int localTime;
  int serverTime;
  bool forceOneView;
  std::byte padding0[3];
  std::uint32_t initTools;
  std::uint32_t updateTools;
  std::byte padding1[12];
  gameReturnPlayer_t players[16];
  idStaticList<idRenderModelGui*,8> debugGuis;
};

// idGame is supplied by the game DLL.  Calls remain explicit vtable dispatches
// so this framework recovery does not invent the rest of its 193-slot ABI.
class idGame {
public:
  const void* const* vtable;

  template <typename Fn> Fn Slot(int index) const {
    return reinterpret_cast<Fn>(vtable[index]);
  }
  void RunFrame(gameFrameParms_t parms,gameReturn_t* result) {
    Slot<void(*)(idGame*,gameFrameParms_t,gameReturn_t*)>(1)(this,parms,result);
  }
  bool IsGameActive(bool allowLoading) {
    return Slot<bool(*)(idGame*,bool)>(6)(this,allowLoading);
  }
  void NetReceiveReliable(int type,idBitMsg& msg,int peer) {
    Slot<void(*)(idGame*,int,idBitMsg&,int)>(2)(this,type,msg,peer);
  }
  bool HandleGuiEvent(const struct sysEvent_t& event,int player) {
    return Slot<bool(*)(idGame*,const sysEvent_t&,int)>(3)(this,event,player);
  }
  idRenderWorld* GetRenderWorld() {
    return Slot<idRenderWorld*(*)(idGame*)>(8)(this);
  }
  idSoundWorld* GetSoundWorld() {
    return Slot<idSoundWorld*(*)(idGame*)>(9)(this);
  }
  idSoundWorld* GetMenuSoundWorld() {
    return Slot<idSoundWorld*(*)(idGame*)>(10)(this);
  }
  bool IsUsingClientSnapshots() {
    return Slot<bool(*)(idGame*)>(21)(this);
  }
  void ClientReadSnapshot() {
    Slot<void(*)(idGame*)>(22)(this);
  }
  void ClientApplySnapshot() {
    Slot<void(*)(idGame*)>(23)(this);
  }
  bool Shell_IsActive() {
    return Slot<bool(*)(idGame*)>(61)(this);
  }
  void Shell_CreateMenu(bool inGame) {
    Slot<void(*)(idGame*,bool)>(60)(this,inGame);
  }
  bool Shell_ShouldUnlockMouse() {
    return Slot<bool(*)(idGame*)>(67)(this);
  }
  void Shell_Show(bool show) {
    Slot<void(*)(idGame*,bool)>(59)(this,show);
  }
  bool GetGamePaused(bool* menuPaused) {
    return Slot<bool(*)(idGame*,bool*)>(75)(this,menuPaused);
  }
  std::int64_t GetMinSaveStorageRequired(bool includeSaves) {
    return Slot<std::int64_t(*)(idGame*,bool)>(188)(this,includeSaves);
  }
  const idGameSpawnInfo* GetGameSpawnInfo() {
    return Slot<const idGameSpawnInfo*(*)(idGame*)>(12)(this);
  }
  idFile_SaveGame* GetMapStartInfoFile() {
    return Slot<idFile_SaveGame*(*)(idGame*)>(29)(this);
  }
  idFile_SaveGame* CheckpointGetSaveFile() {
    return Slot<idFile_SaveGame*(*)(idGame*)>(37)(this);
  }
  int CheckpointGetActiveSlots() {
    return Slot<int(*)(idGame*)>(34)(this);
  }
  bool CheckpointGetSpawninfoFromSlot(int slot,idGameSpawnInfo& info) {
    return Slot<bool(*)(idGame*,int,idGameSpawnInfo&)>(35)(this,slot,info);
  }
  void CheckpointCapFileData(int slot) {
    Slot<void(*)(idGame*,int)>(36)(this,slot);
  }
  bool CheckpointGetPlayerData(int player,bool& savePersistent,int& persistentSize,
                               idStr& checkpointName) {
    return Slot<bool(*)(idGame*,int,bool&,int&,idStr&)>(31)(
        this,player,savePersistent,persistentSize,checkpointName);
  }
  void CheckpointPlayerPostLoad(int player,bool savePersistent,int persistentSize,
                                const idStr& checkpointName) {
    Slot<void(*)(idGame*,int,bool,int,const idStr&)>(32)(
        this,player,savePersistent,persistentSize,checkpointName);
  }
  bool CheckpointProcessRead(idFile_SaveGame* file,int slot,const char* mapName) {
    return Slot<bool(*)(idGame*,idFile_SaveGame*,int,const char*)>(38)(
        this,file,slot,mapName);
  }
  bool LoadGame(const char* name) {
    return Slot<bool(*)(idGame*,const char*)>(17)(this,name);
  }
  bool NextMap(const idCmdArgs& args,idGameSpawnInfo& info) {
    return Slot<bool(*)(idGame*,const idCmdArgs&,idGameSpawnInfo&)>(18)(
        this,args,info);
  }
  bool RequestSaveGame(const char* name,bool autosave,bool checkpoint) {
    return Slot<bool(*)(idGame*,const char*,bool,bool)>(20)(
        this,name,autosave,checkpoint);
  }
  void RestartPassMainMenu(const idCmdArgs& args) {
    Slot<void(*)(idGame*,const idCmdArgs&)>(42)(this,args);
  }
  void CheckpointBlockOnLoad() {
    Slot<void(*)(idGame*)>(41)(this);
  }
  int GetGameMs(int timeType) {
    return Slot<int(*)(idGame*,int)>(47)(this,timeType);
  }
  class idGameTimeManager* GetGameTimeManager() {
    return Slot<idGameTimeManager*(*)(idGame*)>(43)(this);
  }
  int GetGameFrame() {
    return Slot<int(*)(idGame*)>(45)(this);
  }
  int GetGameMsPerFrame(int timeType) {
    return Slot<int(*)(idGame*,int)>(49)(this,timeType);
  }
  float GetRealMsPerFrame(int timeType) {
    return Slot<float(*)(idGame*,int)>(51)(this,timeType);
  }
  int GetGameHz(int timeType) {
    return Slot<int(*)(idGame*,int)>(44)(this,timeType);
  }
  int GetGameMsPerRealSec() {
    return Slot<int(*)(idGame*)>(53)(this);
  }
  void SetServerGameTimeMs(int value) {
    Slot<void(*)(idGame*,int)>(54)(this,value);
  }
  int GetServerGameTimeMs() {
    return Slot<int(*)(idGame*)>(55)(this);
  }
  void SetSSTimes(int start,int end) {
    Slot<void(*)(idGame*,int,int)>(56)(this,start,end);
  }
  int GetSSEndTime() {
    return Slot<int(*)(idGame*)>(57)(this);
  }
  void Shell_ShowNewContentAvailable() {
    Slot<void(*)(idGame*)>(68)(this);
  }
  idRenderModelGui* Shell_Render() {
    return Slot<idRenderModelGui*(*)(idGame*)>(63)(this);
  }
  idMapFile* GetLevelMap() {
    return Slot<idMapFile*(*)(idGame*)>(179)(this);
  }
  bool Shell_HandleGuiEvent(const sysEvent_t& event) {
    return Slot<bool(*)(idGame*,const sysEvent_t&)>(62)(this,event);
  }
  bool Shell_IgnoreSplitScreenEvent(const sysEvent_t& event) {
    return Slot<bool(*)(idGame*,const sysEvent_t&)>(74)(this,event);
  }
  void UpdateFrameState() {
    Slot<void(*)(idGame*)>(76)(this);
  }
  void OnCloseSnapshot() {
    Slot<void(*)(idGame*)>(82)(this);
  }
  void ReadFromSnapshot(idSnapShot& snap,const idSnapShot* oldSnap) {
    Slot<void(*)(idGame*,idSnapShot&,const idSnapShot*)>(78)(this,snap,oldSnap);
  }
  void WriteToSnapshot(idSnapShot& snap,const idSnapShot* oldSnap) {
    Slot<void(*)(idGame*,idSnapShot&,const idSnapShot*)>(79)(this,snap,oldSnap);
  }
  void SetInterpolation(float fraction,int serverTime,int previousTime,int nextTime) {
    Slot<void(*)(idGame*,float,int,int,int)>(80)(this,fraction,serverTime,previousTime,nextTime);
  }
  idRenderModelGui* RenderLocalViewForPlayer(int playerIndex,int localTime,
                                             renderView_t* view,
                                             idRenderWorld* world) {
    return Slot<idRenderModelGui*(*)(idGame*,int,int,renderView_t*,idRenderWorld*)>(83)(
        this,playerIndex,localTime,view,world);
  }
  idRenderModelGui* RenderLocalViewForGame(int localTime,idRenderWorld* world) {
    return Slot<idRenderModelGui*(*)(idGame*,int,idRenderWorld*)>(84)(
        this,localTime,world);
  }
  int GetPlayerIndexFromInputDevice(int device) {
    return Slot<int(*)(idGame*,int)>(87)(this,device);
  }
  int GenerateInputToPlayerIndexRouting() {
    return Slot<int(*)(idGame*)>(86)(this);
  }
  bool IsPlayerControlInhibited(int player) {
    return Slot<bool(*)(idGame*,int)>(85)(this,player);
  }
  void GetPlayerPredictionData(int player,usercmd_t** commands,int count,bool& smoothing) {
    Slot<void(*)(idGame*,int,usercmd_t**,int,bool&)>(89)(this,player,commands,count,smoothing);
  }
  void SmoothIncomingUserCmds(usercmd_t& base,usercmd_t* commands,int count) {
    Slot<void(*)(idGame*,usercmd_t&,usercmd_t*,int)>(90)(this,base,commands,count);
  }
};

class idGameLock {
public:
  explicit idGameLock(const idGame* game);
  ~idGameLock();
private:
  const idGame* previousGame;
};

static_assert(sizeof(idGameLock)==8,"game lock ABI drift");

static_assert(sizeof(renderView_t)==0x1980,"renderView_t ABI drift");
static_assert(offsetof(renderView_t,viewID)==4,"renderView_t::viewID ABI drift");
static_assert(offsetof(renderView_t,fov_x)==0x10,"renderView_t::fov_x ABI drift");
static_assert(offsetof(renderView_t,vieworg)==0x60,"renderView_t::vieworg ABI drift");
static_assert(offsetof(renderView_t,viewaxis)==0x6C,"renderView_t::viewaxis ABI drift");
static_assert(sizeof(inputSettings_t)==40,"inputSettings_t ABI drift");
static_assert(sizeof(gameReturnPlayer_t)==6640,"gameReturnPlayer_t ABI drift");
static_assert(offsetof(gameReturnPlayer_t,inputSettings)==0x19A8,"player input ABI drift");
static_assert(sizeof(gameReturn_t)==106368,"gameReturn_t ABI drift");
static_assert(offsetof(gameReturn_t,players)==0x20,"gameReturn_t players ABI drift");
