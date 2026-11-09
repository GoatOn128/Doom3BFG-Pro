#pragma once

#include "tungsten/game/clientgame/presentable/presentableanimatedphysics.h"
#include "tungsten/game/clientgame/presentable/presentableplayer_shared.h"

#include <array>
#include <cstdint>
#include <string>
#include <unordered_map>
#include <vector>

class idPresentableVehicle;
class idClientGameMsg;

enum presentableType_t {
  PRESENTABLE_GENERIC = 0,
  PRESENTABLE_RENDERMODEL,
  PRESENTABLE_ANIMATED_ENTITY,
  PRESENTABLE_VEHICLE,
  PRESENTABLE_BREAKABLE,
  PRESENTABLE_PIECE_EMITTER,
  PRESENTABLE_DAMAGEABLE,
  PRESENTABLE_ACTOR,
  PRESENTABLE_PLAYER,
  PRESENTABLE_PROJECTILE,
  PRESENTABLE_PROJECTILE_GRENADE,
  PRESENTABLE_PROJECTILE_ROCKET,
  PRESENTABLE_PROJECTILE_HOMING,
  PRESENTABLE_AI,
  PRESENTABLE_PROP,
  PRESENTABLE_WEAPON_STATIC,
  PRESENTABLE_DOOR_ANIMATED,
  PRESENTABLE_WEAPON,
  PRESENTABLE_PARTICLE_EMITTER,
  PRESENTABLE_COLLISION_TRIGGER
};

const char* StringForPresentableType(presentableType_t type);

struct idReplicatedSound {
  int presentableIndex{-1};
  int channel{0};
  std::string shader;
  float volume{1.0F};
  float pitch{1.0F};
  float volumeAdjustment{1.0F};
  bool playing{false};
  void Serialize(idSerializer* serializer);
  bool operator==(const idReplicatedSound& other) const {
    return presentableIndex == other.presentableIndex && channel == other.channel;
  }
};

struct aiThink_t { idPresentable* presentable{nullptr}; int time{0}; };
struct presentableThink_t { idPresentable* presentable{nullptr}; int time{0}; };
struct idRecycledTraceModel { int id{0}; };

struct idTestFireResults {
  idTestFireResults& operator=(const idTestFireResults& other);
  idVec3 impact{};
  float damage{0.0F};
  int hitEntity{-1};
  bool hit{false};
};

template<typename T> class idPresentablePtr {
public:
  idPresentablePtr() = default;
  explicit idPresentablePtr(T* value) : pointer(value) {}
  operator T*() const { return pointer; }
  T* Get() const { return pointer; }
private:
  T* pointer{nullptr};
};

template<typename T, int BlockSize, int Tag> class idBlockAlloc {
public:
  ~idBlockAlloc() { blocks.clear(); }
  T* Alloc() { blocks.emplace_back(); return &blocks.back(); }
private:
  std::vector<T> blocks;
};

class idTraceModelCache {
public:
  struct trmCache_t { int id{0}; };
  idTraceModelCache();
  ~idTraceModelCache();
  int NumCached() const { return static_cast<int>(cache.size()); }
private:
  idBlockAlloc<trmCache_t, 64, 13> allocator;
  std::vector<trmCache_t*> cache;
};

class idTraceModelRecycler {
public:
  idTraceModelRecycler();
  int NumRecycled() const { return recycled.Num(); }
private:
  idList<idRecycledTraceModel*, 13> recycled;
};

class idPush { public: idPush(); int GetPushCount() const{return pushCount;} private:int pushCount{0}; };
class idDeferredFireManager { public: ~idDeferredFireManager(); };

class idClientGame;
class idPlayerTimeLock {
public:
  idPlayerTimeLock(idClientGame& game, int playerIndex, int lockedTime);
  ~idPlayerTimeLock();
private:
  idClientGame* game{nullptr};
  int player{-1};
  int previousTime{0};
};

#define WOLF_CLIENT_MESSAGE_HANDLERS(X) \
  X(HandleReliableGuiMsg) X(HandlePlayerMetricsMsg) X(HandleVehiclePickupMsg) \
  X(HandleDeathMessage) X(HandleStatusMessageForPlayer) X(HandleStatusMessage) \
  X(HandleStatusOrSystemMessage) X(HandleSystemMessage) X(HandlePointsMessageForPlayer) \
  X(HandleMeteorPointsMessageForPlayer) X(HandleShowTutorialForPlayer) X(HandleKillMessage) \
  X(HandlePersonalOrSystemMessage) X(HandleTextChat) X(HandleReliableOnlineGameEvent)

class idClientGame {
public:
  idClientGame();
  ~idClientGame();

  void SetServerGameTime(int time);
  int GetServerGameTime() const;
  int GetPreviousServerGameTime() const;
  idPresentable* GetPresentableByIndex(int index) const;
  bool IsPresentableIndexValid(int index) const;
  idPresentablePlayer* FindPlayerPresentableByIndex(int playerIndex) const;
  std::uint64_t GetLobbyUserIdFromPlayerIndex(int playerIndex) const;
  void SetLobbyUserIdForPlayerIndex(int playerIndex, std::uint64_t lobbyId);
  const std::string* GetProfileFromPlayerIndex(int playerIndex) const;
  const std::string* GetLocalUserFromPlayerIndex(int playerIndex) const;
  bool PlayerIndexIsLocal(int playerIndex) const;
  void SetPlayerLocal(int playerIndex, bool local, std::string user = {}, std::string profile = {});

  void SubmitEffectPhysicsCollisionQueries();
  const std::string& GetEffectsModel() const;
  const std::string& GetDecalModel() const;
  const std::string& GetBeamModel() const;
  const std::string& GetBeamDepthHackModel() const;
  void* GetDeferredDecalMgr();
  void* GetWeaponTraceMgr();
  void* GetWeaponImpactMgr();
  void* GetEffectsModelMgr();
  localView_t* GetLocalViewForPlayerIndex(int playerIndex);
  bool RenderLocalViewForPlayer(int playerIndex);
  bool HandleGuiEventForPlayer(int playerIndex, const std::string& event);
  bool IsPlayerControlInhibited(int playerIndex) const;

#define WOLF_DECLARE_MESSAGE(name) void name(const std::string& message = {});
  WOLF_CLIENT_MESSAGE_HANDLERS(WOLF_DECLARE_MESSAGE)
#undef WOLF_DECLARE_MESSAGE

  const std::vector<idPresentable*>& GetObjectList() const;
  int GetObjectListSize() const;
  float CalcHitscanDamageScale(float distance, float falloffDistance) const;
  idVec3 GetRandomSpreadTraceDir(const idVec3& direction, float spread, std::uint32_t seed) const;
  std::vector<idUserCmd> GetPlayerPresentableCmds(int playerIndex) const;
  idReplicatedSound* FindReplicatedSound(int presentableIndex, int channel);
  void ReplicateSoundVolume(int presentableIndex, int channel, float volume);
  void ReplicateSoundPitch(int presentableIndex, int channel, float pitch);
  void ReplicateSoundModify(int presentableIndex, int channel, float volume, float pitch);
  void ReplicateSoundVolumeAdjustment(int presentableIndex, int channel, float adjustment);
  void ReplicateSoundClearVolumeAdjustment(int presentableIndex, int channel);
  idPresentablePlayer* GetLocalPresentablePlayer() const;
  void PlaySoundForAllPeers(const std::string& shader);
  void DisplayTextChat(const std::string& text);
  idPresentable* GetPresentableByEntityNum(int entityNumber) const;
  int GetPlayerGameTime(int playerIndex) const;
  int GetPreviousPlayerGameTime(int playerIndex) const;
  std::uint8_t ConstructPeerMaskForPlayer(int playerIndex) const;
  std::uint32_t GeneratePredictionKey(int playerIndex);
  idPresentable* GetTracePresentable(int entityNumber) const;
  void ClientDisablePresentablePushers(bool disable);
  idPresentableVehicle* GetVehiclePresentable(int index) const;
  void GenerateInputToPlayerIndexRouting(const std::vector<int>& devices);
  int GetPlayerIndexFromInputDevice(int device) const;
  void ClearViewCallbacks();
  void DeleteAllPresentables();
  void ShowSynchronousPerf();
  void ShowThinks();
  void ShowPresentableInfo();
  bool RenderLocalViewForGame();
  void ReplicateStopSound(int presentableIndex, int channel);
  void SendChatText(const std::string& text);
  void RemoveSimulatingEffectPhysics(idPresentable* presentable);
  void AddSimulatingEffectPhysics(idPresentable* presentable);
  void ShowAIThinks();
  std::vector<std::uint8_t> WriteToSnapshot() const;
  void ReadFromSnapshot(const std::vector<std::uint8_t>& snapshot);
  void FinishLaunchProjectile();
  std::string AllocRenderModel(const std::string& name);
  int FindFreePresentableIndex() const;
  idViewCallbacks* GetLocalViewCallbacks(int playerIndex) const;
  void MarkPresentableSerialized(int index);
  void MarkAllPresentablesAsSerialized();
  int AddPresentable(idPresentable* presentable);
  bool AddPresentable(int index, idPresentable* presentable);
  bool RemovePresentable(idPresentable* presentable);
  void MakePresentableNonReplicated(idPresentable* presentable);
  void AddAIThink(idPresentable* presentable, int time);
  void AddThink(idPresentable* presentable, int time);
  void ReplicatePlaySound(int presentableIndex, int channel, const std::string& shader);
  void PlayShaderDirectly(const std::string& shader);
  void ImpactEffect(const idVec3& point);
  void ProjectileImpact(const idVec3& point);
  void DoTracer(const idVec3& start, const idVec3& end);
  void FinishHitScan(const idTestFireResults& result);
  void FinishFire();
  void HandleReliableGameMsg(idClientGameMsg& message);
  void ExecuteQueuedClientGameMsgs();
  void QueueReliableClientGameMsg(idClientGameMsg* message);
  void NetReceiveReliable(idClientGameMsg* message);

  void SetPlayerGameTime(int playerIndex, int time);
  void SetPlayerControlInhibited(int playerIndex, bool inhibited);
  const std::vector<std::string>& GetMessages() const{return messages;}
  int GetCollisionQueryCount() const{return collisionQueries;}
  int GetRenderCount() const{return renderCount;}
  int GetImpactCount() const{return impactCount;}
  int GetFinishedFireCount() const{return finishedFireCount;}
  int GetQueuedReliableCount()const{return static_cast<int>(reliableQueue.size());}
  const idList<idReplicatedSound,5>& GetReplicatedSounds() const{return replicatedSounds;}

private:
  friend class idPlayerTimeLock;
  bool ValidPlayer(int playerIndex) const{return playerIndex>=0&&playerIndex<4;}
  void RecordMessage(const char* kind, const std::string& message);
  int serverTime{0};
  int previousServerTime{0};
  std::vector<idPresentable*> presentables;
  std::unordered_map<int,idPresentable*> entities;
  std::array<std::uint64_t,4> lobbyIds{};
  std::array<std::string,4> profiles{};
  std::array<std::string,4> localUsers{};
  std::array<bool,4> localPlayers{};
  std::array<bool,4> controlInhibited{};
  std::array<int,4> playerTimes{};
  std::array<int,4> previousPlayerTimes{};
  std::array<int,4> inputRouting{{-1,-1,-1,-1}};
  std::array<localView_t,4> localViews{};
  std::array<idViewCallbacks*,4> viewCallbacks{};
  std::vector<std::string> messages;
  std::vector<std::string> renderModels;
  std::vector<idPresentable*> simulatingEffects;
  std::vector<idClientGameMsg*> reliableQueue;
  idList<idReplicatedSound,5> replicatedSounds;
  idList<aiThink_t,5> aiThinks;
  idList<presentableThink_t,5> thinks;
  std::vector<bool> serialized;
  std::uint32_t predictionCounter{0};
  int collisionQueries{0};
  int renderCount{0};
  int impactCount{0};
  int tracerCount{0};
  int finishedFireCount{0};
  int managerTokens[4]{};
  bool pushersDisabled{false};
};

void messagemode_f(idClientGame& game, const std::string& text);

#define WOLF_CLIENTGAME_LOCAL_CVARS(X) \
  X(cg_predictProjectiles) X(cg_predictProjectileExpire) X(cg_projectile_clientAuthoritative) \
  X(cg_optimizeSnapSerialize) X(cg_optimizeSnapSerialize_debug) X(cg_showSnapSerializePerformance) \
  X(cg_projectile_clientAuthoritative_maxCatchup) X(cg_predictedSpawn_disable) X(cg_predictedSpawn_debug) \
  X(net_replicateSounds) X(net_visCullSnapshots) X(clientGame_impactDist) X(mp_cg_soundDebug) \
  X(cg_showPresentableInfo) X(g_showSynchronousPerf) X(s_snapTrace) X(s_tracerDistance) \
  X(cg_gunfireInfluenceForce) X(cg_gunfireInfluenceExtraUp) X(cg_playerImpactAttenuationFalloff) \
  X(cg_playerImpactAttenuationMax) X(g_useImpactColor) X(messagemode_v)
#define WOLF_DECLARE_CLIENTGAME_CVAR(name) extern idCVar name;
WOLF_CLIENTGAME_LOCAL_CVARS(WOLF_DECLARE_CLIENTGAME_CVAR)
#undef WOLF_DECLARE_CLIENTGAME_CVAR
