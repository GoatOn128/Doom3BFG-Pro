#include "tungsten/game/clientgame/clientgame_local.h"
#include "tungsten/game/clientgame/presentable/presentablevehiclecar.h"

#include <algorithm>
#include <cmath>
#include <cstring>
#include <random>
#include <sstream>

// DIA: StringForPresentableType
// DIA: idClientGame::SetServerGameTime
// DIA: idClientGame::GetServerGameTime
// DIA: idClientGame::GetPreviousServerGameTime
// DIA: idPlayerTimeLock::~idPlayerTimeLock
// DIA: idClientGame::GetPresentableByIndex
// DIA: idClientGame::IsPresentableIndexValid
// DIA: idClientGame::FindPlayerPresentableByIndex
// DIA: idClientGame::GetLobbyUserIdFromPlayerIndex
// DIA: idClientGame::SetLobbyUserIdForPlayerIndex
// DIA: idClientGame::GetProfileFromPlayerIndex
// DIA: idClientGame::GetLocalUserFromPlayerIndex
// DIA: idClientGame::PlayerIndexIsLocal
// DIA: idClientGame::SubmitEffectPhysicsCollisionQueries
// DIA: idClientGame::GetEffectsModel
// DIA: idClientGame::GetDecalModel
// DIA: idClientGame::GetBeamModel
// DIA: idClientGame::GetBeamDepthHackModel
// DIA: idClientGame::GetDeferredDecalMgr
// DIA: idClientGame::GetWeaponTraceMgr
// DIA: idClientGame::GetWeaponImpactMgr
// DIA: idClientGame::GetEffectsModelMgr
// DIA: idClientGame::GetLocalViewForPlayerIndex
// DIA: idClientGame::RenderLocalViewForPlayer
// DIA: `idClientGame::RenderLocalViewForPlayer'::`1'::dtor$0
// DIA: idClientGame::HandleGuiEventForPlayer
// DIA: idClientGame::IsPlayerControlInhibited
// DIA: idClientGame::HandleReliableGuiMsg
// DIA: idClientGame::HandlePlayerMetricsMsg
// DIA: idClientGame::HandleVehiclePickupMsg
// DIA: idClientGame::GetObjectList
// DIA: idClientGame::GetObjectListSize
// DIA: idClientGame::CalcHitscanDamageScale
// DIA: idClientGame::GetRandomSpreadTraceDir
// DIA: idClientGame::GetPlayerPresentableCmds
// DIA: idClientGame::FindReplicatedSound
// DIA: idClientGame::ReplicateSoundVolume
// DIA: idClientGame::ReplicateSoundPitch
// DIA: idClientGame::ReplicateSoundModify
// DIA: idClientGame::ReplicateSoundVolumeAdjustment
// DIA: idClientGame::ReplicateSoundClearVolumeAdjustment
// DIA: idClientGame::HandleDeathMessage
// DIA: `idClientGame::HandleDeathMessage'::`1'::dtor$0
// DIA: `idClientGame::HandleDeathMessage'::`1'::dtor$1
// DIA: idClientGame::HandleStatusMessageForPlayer
// DIA: idClientGame::HandleStatusMessage
// DIA: idClientGame::HandleStatusOrSystemMessage
// DIA: idClientGame::HandleSystemMessage
// DIA: idClientGame::HandlePointsMessageForPlayer
// DIA: idClientGame::HandleMeteorPointsMessageForPlayer
// DIA: idClientGame::HandleShowTutorialForPlayer
// DIA: idClientGame::GetLocalPresentablePlayer
// DIA: idClientGame::PlaySoundForAllPeers
// DIA: idClientGame::DisplayTextChat
// DIA: idClientGame::GetPresentableByEntityNum
// DIA: idClientGame::GetPlayerGameTime
// DIA: idClientGame::GetPreviousPlayerGameTime
// DIA: idClientGame::ConstructPeerMaskForPlayer
// DIA: idClientGame::GeneratePredictionKey
// DIA: idClientGame::GetTracePresentable
// DIA: idPlayerTimeLock::idPlayerTimeLock
// DIA: idClientGame::ClientDisablePresentablePushers
// DIA: idClientGame::GetVehiclePresentable
// DIA: idClientGame::GenerateInputToPlayerIndexRouting
// DIA: idClientGame::GetPlayerIndexFromInputDevice
// DIA: idList<idReplicatedSound,5>::RemoveIndex
// DIA: idSerializer::SerializeQ<60,8>
// DIA: idBitMsg::WriteQuantizedVector<idVec3,32768,11>
// DIA: idBitMsg::ReadQuantizedVector<idVec3,32768,11>
// DIA: idClientGame::ClearViewCallbacks
// DIA: idClientGame::DeleteAllPresentables
// DIA: idClientGame::ShowSynchronousPerf
// DIA: idClientGame::ShowThinks
// DIA: idClientGame::ShowPresentableInfo
// DIA: idClientGame::RenderLocalViewForGame
// DIA: `idClientGame::RenderLocalViewForGame'::`1'::dtor$0
// DIA: idClientGame::ReplicateStopSound
// DIA: idClientGame::HandleKillMessage
// DIA: idClientGame::HandlePersonalOrSystemMessage
// DIA: messagemode_f
// DIA: idClientGame::SendChatText
// DIA: idClientGame::HandleTextChat
// DIA: `idClientGame::HandleTextChat'::`1'::dtor$0
// DIA: `idClientGame::HandleTextChat'::`1'::dtor$1
// DIA: `idClientGame::HandleTextChat'::`1'::dtor$2
// DIA: idClientGame::RemoveSimulatingEffectPhysics
// DIA: idTestFireResults::operator=
// DIA: idPresentablePtr<idPresentablePlayer>::operator idPresentablePlayer * __ptr64
// DIA: idList<idRecycledTraceModel * __ptr64,13>::idList<idRecycledTraceModel * __ptr64,13>
// DIA: idBlockAlloc<idTraceModelCache::trmCache_t,64,13>::~idBlockAlloc<idTraceModelCache::trmCache_t,64,13>
// DIA: idTraceModelCache::~idTraceModelCache
// DIA: `idTraceModelCache::~idTraceModelCache'::`1'::dtor$0
// DIA: `idTraceModelCache::~idTraceModelCache'::`1'::dtor$1
// DIA: `idTraceModelCache::~idTraceModelCache'::`1'::dtor$2
// DIA: idClientGame::HandleReliableOnlineGameEvent
// DIA: `idClientGame::HandleReliableOnlineGameEvent'::`1'::dtor$0
// DIA: `idClientGame::HandleReliableOnlineGameEvent'::`1'::dtor$1
// DIA: `idClientGame::HandleReliableOnlineGameEvent'::`1'::dtor$2
// DIA: `idClientGame::HandleReliableOnlineGameEvent'::`1'::dtor$3
// DIA: `idClientGame::HandleReliableOnlineGameEvent'::`1'::dtor$4
// DIA: `idClientGame::HandleReliableOnlineGameEvent'::`1'::dtor$5
// DIA: `idClientGame::HandleReliableOnlineGameEvent'::`1'::dtor$6
// DIA: `idClientGame::HandleReliableOnlineGameEvent'::`1'::dtor$7
// DIA: `idClientGame::HandleReliableOnlineGameEvent'::`1'::dtor$8
// DIA: `idClientGame::HandleReliableOnlineGameEvent'::`1'::dtor$9
// DIA: `idClientGame::HandleReliableOnlineGameEvent'::`1'::dtor$10
// DIA: `idClientGame::HandleReliableOnlineGameEvent'::`1'::dtor$11
// DIA: `idClientGame::HandleReliableOnlineGameEvent'::`1'::dtor$12
// DIA: `idClientGame::HandleReliableOnlineGameEvent'::`1'::dtor$13
// DIA: `idClientGame::HandleReliableOnlineGameEvent'::`1'::dtor$14
// DIA: `idClientGame::HandleReliableOnlineGameEvent'::`1'::dtor$15
// DIA: `idClientGame::HandleReliableOnlineGameEvent'::`1'::dtor$16
// DIA: `idClientGame::HandleReliableOnlineGameEvent'::`1'::dtor$17
// DIA: `idClientGame::HandleReliableOnlineGameEvent'::`1'::dtor$18
// DIA: `idClientGame::HandleReliableOnlineGameEvent'::`1'::dtor$19
// DIA: `idClientGame::HandleReliableOnlineGameEvent'::`1'::dtor$20
// DIA: `idClientGame::HandleReliableOnlineGameEvent'::`1'::dtor$21
// DIA: `idClientGame::HandleReliableOnlineGameEvent'::`1'::dtor$22
// DIA: `idClientGame::HandleReliableOnlineGameEvent'::`1'::dtor$23
// DIA: `idClientGame::HandleReliableOnlineGameEvent'::`1'::dtor$24
// DIA: `idClientGame::HandleReliableOnlineGameEvent'::`1'::dtor$25
// DIA: `idClientGame::HandleReliableOnlineGameEvent'::`1'::dtor$26
// DIA: `idClientGame::HandleReliableOnlineGameEvent'::`1'::dtor$27
// DIA: `idClientGame::HandleReliableOnlineGameEvent'::`1'::dtor$28
// DIA: `idClientGame::HandleReliableOnlineGameEvent'::`1'::dtor$29
// DIA: `idClientGame::HandleReliableOnlineGameEvent'::`1'::dtor$30
// DIA: `idClientGame::HandleReliableOnlineGameEvent'::`1'::dtor$31
// DIA: `idClientGame::HandleReliableOnlineGameEvent'::`1'::dtor$32
// DIA: `idClientGame::HandleReliableOnlineGameEvent'::`1'::dtor$33
// DIA: `idClientGame::HandleReliableOnlineGameEvent'::`1'::dtor$34
// DIA: `idClientGame::HandleReliableOnlineGameEvent'::`1'::dtor$35
// DIA: `idClientGame::HandleReliableOnlineGameEvent'::`1'::dtor$36
// DIA: `idClientGame::HandleReliableOnlineGameEvent'::`1'::dtor$37
// DIA: `idClientGame::HandleReliableOnlineGameEvent'::`1'::dtor$38
// DIA: `idClientGame::HandleReliableOnlineGameEvent'::`1'::dtor$39
// DIA: `idClientGame::HandleReliableOnlineGameEvent'::`1'::dtor$40
// DIA: `idClientGame::HandleReliableOnlineGameEvent'::`1'::dtor$41
// DIA: `idClientGame::HandleReliableOnlineGameEvent'::`1'::dtor$42
// DIA: `idClientGame::HandleReliableOnlineGameEvent'::`1'::dtor$43
// DIA: `idClientGame::HandleReliableOnlineGameEvent'::`1'::dtor$44
// DIA: `idClientGame::HandleReliableOnlineGameEvent'::`1'::dtor$45
// DIA: `idClientGame::HandleReliableOnlineGameEvent'::`1'::dtor$46
// DIA: idList<aiThink_t,5>::Resize
// DIA: idList<presentableThink_t,5>::Resize
// DIA: idTraceModelRecycler::idTraceModelRecycler
// DIA: `idTraceModelRecycler::idTraceModelRecycler'::`1'::dtor$0
// DIA: `idTraceModelRecycler::idTraceModelRecycler'::`1'::dtor$1
// DIA: idTraceModelCache::idTraceModelCache
// DIA: idPush::idPush
// DIA: idReplicatedSound::Serialize
// DIA: idGameLocal::GetChallengeMode
// DIA: idDeferredFireManager::~idDeferredFireManager
// DIA: idClientGame::ShowAIThinks
// DIA: idClientGame::WriteToSnapshot
// DIA: idClientGame::ReadFromSnapshot
// DIA: idClientGame::FinishLaunchProjectile
// DIA: idList<idPresentable * __ptr64,5>::SetNum
// DIA: idList<idReplicatedSound,5>::Resize
// DIA: idClientGame::idClientGame
// DIA: `idClientGame::idClientGame'::`1'::dtor$0
// DIA: `idClientGame::idClientGame'::`1'::dtor$1
// DIA: `idClientGame::idClientGame'::`1'::dtor$2
// DIA: `idClientGame::idClientGame'::`1'::dtor$3
// DIA: `idClientGame::idClientGame'::`1'::dtor$4
// DIA: `idClientGame::idClientGame'::`1'::dtor$5
// DIA: `idClientGame::idClientGame'::`1'::dtor$6
// DIA: `idClientGame::idClientGame'::`1'::dtor$7
// DIA: `idClientGame::idClientGame'::`1'::dtor$8
// DIA: `idClientGame::idClientGame'::`1'::dtor$9
// DIA: `idClientGame::idClientGame'::`1'::dtor$10
// DIA: `idClientGame::idClientGame'::`1'::dtor$11
// DIA: `idClientGame::idClientGame'::`1'::dtor$12
// DIA: `idClientGame::idClientGame'::`1'::dtor$13
// DIA: `idClientGame::idClientGame'::`1'::dtor$14
// DIA: `idClientGame::idClientGame'::`1'::dtor$15
// DIA: `idClientGame::idClientGame'::`1'::dtor$16
// DIA: `idClientGame::idClientGame'::`1'::dtor$17
// DIA: `idClientGame::idClientGame'::`1'::dtor$18
// DIA: `idClientGame::idClientGame'::`1'::dtor$19
// DIA: `idClientGame::idClientGame'::`1'::dtor$20
// DIA: `idClientGame::idClientGame'::`1'::dtor$21
// DIA: `idClientGame::idClientGame'::`1'::dtor$22
// DIA: `idClientGame::idClientGame'::`1'::dtor$23
// DIA: idClientGame::~idClientGame
// DIA: `idClientGame::~idClientGame'::`1'::dtor$0
// DIA: `idClientGame::~idClientGame'::`1'::dtor$1
// DIA: `idClientGame::~idClientGame'::`1'::dtor$2
// DIA: `idClientGame::~idClientGame'::`1'::dtor$3
// DIA: `idClientGame::~idClientGame'::`1'::dtor$4
// DIA: `idClientGame::~idClientGame'::`1'::dtor$5
// DIA: `idClientGame::~idClientGame'::`1'::dtor$6
// DIA: `idClientGame::~idClientGame'::`1'::dtor$7
// DIA: `idClientGame::~idClientGame'::`1'::dtor$8
// DIA: `idClientGame::~idClientGame'::`1'::dtor$9
// DIA: `idClientGame::~idClientGame'::`1'::dtor$10
// DIA: `idClientGame::~idClientGame'::`1'::dtor$11
// DIA: `idClientGame::~idClientGame'::`1'::dtor$12
// DIA: `idClientGame::~idClientGame'::`1'::dtor$13
// DIA: `idClientGame::~idClientGame'::`1'::dtor$14
// DIA: `idClientGame::~idClientGame'::`1'::dtor$15
// DIA: `idClientGame::~idClientGame'::`1'::dtor$16
// DIA: `idClientGame::~idClientGame'::`1'::dtor$17
// DIA: `idClientGame::~idClientGame'::`1'::dtor$18
// DIA: `idClientGame::~idClientGame'::`1'::dtor$19
// DIA: `idClientGame::~idClientGame'::`1'::dtor$20
// DIA: `idClientGame::~idClientGame'::`1'::dtor$21
// DIA: `idClientGame::~idClientGame'::`1'::dtor$22
// DIA: `idClientGame::~idClientGame'::`1'::dtor$24
// DIA: `idClientGame::~idClientGame'::`1'::dtor$27
// DIA: `idClientGame::~idClientGame'::`1'::dtor$29
// DIA: `idClientGame::~idClientGame'::`1'::dtor$30
// DIA: `idClientGame::~idClientGame'::`1'::dtor$31
// DIA: idClientGame::AllocRenderModel
// DIA: `idClientGame::AllocRenderModel'::`1'::dtor$0
// DIA: `idClientGame::AllocRenderModel'::`1'::dtor$1
// DIA: `idClientGame::AllocRenderModel'::`1'::dtor$2
// DIA: `idClientGame::AllocRenderModel'::`1'::dtor$3
// DIA: `idClientGame::AllocRenderModel'::`1'::dtor$4
// DIA: `idClientGame::AllocRenderModel'::`1'::dtor$5
// DIA: `idClientGame::AllocRenderModel'::`1'::dtor$6
// DIA: `idClientGame::AllocRenderModel'::`1'::dtor$7
// DIA: `idClientGame::AllocRenderModel'::`1'::dtor$8
// DIA: `idClientGame::AllocRenderModel'::`1'::dtor$9
// DIA: `idClientGame::AllocRenderModel'::`1'::dtor$10
// DIA: `idClientGame::AllocRenderModel'::`1'::dtor$11
// DIA: `idClientGame::AllocRenderModel'::`1'::dtor$12
// DIA: `idClientGame::AllocRenderModel'::`1'::dtor$13
// DIA: `idClientGame::AllocRenderModel'::`1'::dtor$14
// DIA: `idClientGame::AllocRenderModel'::`1'::dtor$15
// DIA: `idClientGame::AllocRenderModel'::`1'::dtor$16
// DIA: `idClientGame::AllocRenderModel'::`1'::dtor$17
// DIA: `idClientGame::AllocRenderModel'::`1'::dtor$18
// DIA: `idClientGame::AllocRenderModel'::`1'::dtor$19
// DIA: `idClientGame::AllocRenderModel'::`1'::dtor$20
// DIA: `idClientGame::AllocRenderModel'::`1'::dtor$21
// DIA: `idClientGame::AllocRenderModel'::`1'::dtor$22
// DIA: `idClientGame::AllocRenderModel'::`1'::dtor$23
// DIA: `idClientGame::AllocRenderModel'::`1'::dtor$24
// DIA: `idClientGame::AllocRenderModel'::`1'::dtor$25
// DIA: `idClientGame::AllocRenderModel'::`1'::dtor$26
// DIA: `idClientGame::AllocRenderModel'::`1'::dtor$27
// DIA: `idClientGame::AllocRenderModel'::`1'::dtor$28
// DIA: `idClientGame::AllocRenderModel'::`1'::dtor$29
// DIA: `idClientGame::AllocRenderModel'::`1'::dtor$30
// DIA: `idClientGame::AllocRenderModel'::`1'::dtor$31
// DIA: `idClientGame::AllocRenderModel'::`1'::dtor$32
// DIA: `idClientGame::AllocRenderModel'::`1'::dtor$33
// DIA: `idClientGame::AllocRenderModel'::`1'::dtor$34
// DIA: `idClientGame::AllocRenderModel'::`1'::dtor$35
// DIA: `idClientGame::AllocRenderModel'::`1'::dtor$36
// DIA: `idClientGame::AllocRenderModel'::`1'::dtor$37
// DIA: `idClientGame::AllocRenderModel'::`1'::dtor$38
// DIA: `idClientGame::AllocRenderModel'::`1'::dtor$39
// DIA: `idClientGame::AllocRenderModel'::`1'::dtor$40
// DIA: `idClientGame::AllocRenderModel'::`1'::dtor$41
// DIA: `idClientGame::AllocRenderModel'::`1'::dtor$42
// DIA: `idClientGame::AllocRenderModel'::`1'::dtor$43
// DIA: idClientGame::FindFreePresentableIndex
// DIA: idClientGame::GetLocalViewCallbacks
// DIA: idClientGame::MarkPresentableSerialized
// DIA: idClientGame::MarkAllPresentablesAsSerialized
// DIA: idList<aiThink_t,5>::Alloc
// DIA: idList<presentableThink_t,5>::Alloc
// DIA: idList<idReplicatedSound,5>::Append
// DIA: idClientGame::AddPresentable
// DIA: idClientGame::RemovePresentable
// DIA: idClientGame::MakePresentableNonReplicated
// DIA: idClientGame::AddAIThink
// DIA: idClientGame::AddThink
// DIA: idClientGame::ReplicatePlaySound
// DIA: idClientGame::PlayShaderDirectly
// DIA: idClientGame::AddSimulatingEffectPhysics
// DIA: idClientGame::AddPresentable
// DIA: idClientGame::ImpactEffect
// DIA: idClientGame::ProjectileImpact
// DIA: idClientGame::DoTracer
// DIA: idClientGame::FinishHitScan
// DIA: `idClientGame::FinishHitScan'::`1'::dtor$0
// DIA: idClientGame::FinishFire
// DIA: `dynamic initializer for 'cg_predictProjectiles''
// DIA: `dynamic initializer for 'cg_predictProjectileExpire''
// DIA: `dynamic initializer for 'cg_projectile_clientAuthoritative''
// DIA: `dynamic initializer for 'cg_optimizeSnapSerialize''
// DIA: `dynamic initializer for 'cg_optimizeSnapSerialize_debug''
// DIA: `dynamic initializer for 'cg_showSnapSerializePerformance''
// DIA: `dynamic initializer for 'cg_projectile_clientAuthoritative_maxCatchup''
// DIA: `dynamic initializer for 'cg_predictedSpawn_disable''
// DIA: `dynamic initializer for 'cg_predictedSpawn_debug''
// DIA: `dynamic initializer for 'net_replicateSounds''
// DIA: `dynamic initializer for 'net_visCullSnapshots''
// DIA: `dynamic initializer for 'clientGame_impactDist''
// DIA: `dynamic initializer for 'mp_cg_soundDebug''
// DIA: `dynamic initializer for 'cg_showPresentableInfo''
// DIA: `dynamic initializer for 'g_showSynchronousPerf''
// DIA: `dynamic initializer for 's_snapTrace''
// DIA: `dynamic initializer for 's_tracerDistance''
// DIA: `dynamic initializer for 'cg_gunfireInfluenceForce''
// DIA: `dynamic initializer for 'cg_gunfireInfluenceExtraUp''
// DIA: `dynamic initializer for 'cg_playerImpactAttenuationFalloff''
// DIA: `dynamic initializer for 'cg_playerImpactAttenuationMax''
// DIA: `dynamic initializer for 'g_useImpactColor''
// DIA: `dynamic initializer for 'messagemode_v''
// DIA: `dynamic atexit destructor for 'cg_predictProjectiles''
// DIA: `dynamic atexit destructor for 'cg_predictProjectileExpire''
// DIA: `dynamic atexit destructor for 'cg_projectile_clientAuthoritative''
// DIA: `dynamic atexit destructor for 'cg_optimizeSnapSerialize''
// DIA: `dynamic atexit destructor for 'cg_optimizeSnapSerialize_debug''
// DIA: `dynamic atexit destructor for 'cg_showSnapSerializePerformance''
// DIA: `dynamic atexit destructor for 'cg_projectile_clientAuthoritative_maxCatchup''
// DIA: `dynamic atexit destructor for 'cg_predictedSpawn_disable''
// DIA: `dynamic atexit destructor for 'cg_predictedSpawn_debug''
// DIA: `dynamic atexit destructor for 'net_replicateSounds''
// DIA: `dynamic atexit destructor for 'net_visCullSnapshots''
// DIA: `dynamic atexit destructor for 'clientGame_impactDist''
// DIA: `dynamic atexit destructor for 'mp_cg_soundDebug''
// DIA: `dynamic atexit destructor for 'cg_showPresentableInfo''
// DIA: `dynamic atexit destructor for 'g_showSynchronousPerf''
// DIA: `dynamic atexit destructor for 's_snapTrace''
// DIA: `dynamic atexit destructor for 's_tracerDistance''
// DIA: `dynamic atexit destructor for 'cg_gunfireInfluenceForce''
// DIA: `dynamic atexit destructor for 'cg_gunfireInfluenceExtraUp''
// DIA: `dynamic atexit destructor for 'cg_playerImpactAttenuationFalloff''
// DIA: `dynamic atexit destructor for 'cg_playerImpactAttenuationMax''
// DIA: `dynamic atexit destructor for 'g_useImpactColor''

#define WOLF_DEFINE_CLIENTGAME_CVAR(name) idCVar name(#name,"0",0,"recovered client game setting");
WOLF_CLIENTGAME_LOCAL_CVARS(WOLF_DEFINE_CLIENTGAME_CVAR)
#undef WOLF_DEFINE_CLIENTGAME_CVAR

const char* StringForPresentableType(presentableType_t type) {
  static constexpr const char* names[] = {
      "PRESENTABLE_GENERIC", "PRESENTABLE_RENDERMODEL", "PRESENTABLE_ANIMATED_ENTITY",
      "PRESENTABLE_VEHICLE", "PRESENTABLE_BREAKABLE", "PRESENTABLE_PIECE_EMITTER",
      "PRESENTABLE_DAMAGEABLE", "PRESENTABLE_ACTOR", "PRESENTABLE_PLAYER",
      "PRESENTABLE_PROJECTILE", "PRESENTABLE_PROJECTILE_GRENADE",
      "PRESENTABLE_PROJECTILE_ROCKET", "PRESENTABLE_PROJECTILE_HOMING", "PRESENTABLE_AI",
      "PRESENTABLE_PROP", "PRESENTABLE_WEAPON_STATIC", "PRESENTABLE_DOOR_ANIMATED",
      "PRESENTABLE_WEAPON", "PRESENTABLE_PARTICLE_EMITTER", "PRESENTABLE_COLLISION_TRIGGER"};
  const int index = static_cast<int>(type);
  return index >= 0 && index < static_cast<int>(std::size(names)) ? names[index]
                                                                  : "Unknown presentable";
}

void idReplicatedSound::Serialize(idSerializer* serializer) {
  if (!serializer) return;
  idAtomicString sound{shader};
  serializer->SerializeInt(presentableIndex); serializer->SerializeInt(channel);
  serializer->SerializeString(&sound); serializer->SerializeFloat(volume);
  serializer->SerializeFloat(pitch); serializer->SerializeFloat(volumeAdjustment);
  serializer->SerializeBool(playing); shader=sound.value;
}

idTestFireResults& idTestFireResults::operator=(const idTestFireResults& other) {
  if(this!=&other){impact=other.impact;damage=other.damage;hitEntity=other.hitEntity;hit=other.hit;}return *this;
}

idTraceModelCache::idTraceModelCache(){cache.push_back(allocator.Alloc());}
idTraceModelCache::~idTraceModelCache()=default;
idTraceModelRecycler::idTraceModelRecycler(){recycled.granularity=16;}
idPush::idPush()=default;
idDeferredFireManager::~idDeferredFireManager()=default;
int idGameLocal::GetChallengeMode()const{return 0;}

idPlayerTimeLock::idPlayerTimeLock(idClientGame& owner,int playerIndex,int lockedTime)
    :game(&owner),player(playerIndex),previousTime(owner.GetPlayerGameTime(playerIndex)){
  owner.SetPlayerGameTime(playerIndex,lockedTime);
}
idPlayerTimeLock::~idPlayerTimeLock(){if(game)game->SetPlayerGameTime(player,previousTime);}

idClientGame::idClientGame():presentables(64,nullptr),serialized(64,false){}
idClientGame::~idClientGame(){DeleteAllPresentables();}
void idClientGame::SetServerGameTime(int time){previousServerTime=serverTime;serverTime=time;}
int idClientGame::GetServerGameTime()const{return serverTime;}
int idClientGame::GetPreviousServerGameTime()const{return previousServerTime;}
idPresentable* idClientGame::GetPresentableByIndex(int index)const{return IsPresentableIndexValid(index)?presentables[static_cast<std::size_t>(index)]:nullptr;}
bool idClientGame::IsPresentableIndexValid(int index)const{return index>=0&&static_cast<std::size_t>(index)<presentables.size()&&presentables[static_cast<std::size_t>(index)]!=nullptr;}
idPresentablePlayer* idClientGame::FindPlayerPresentableByIndex(int playerIndex)const{return dynamic_cast<idPresentablePlayer*>(GetPresentableByIndex(playerIndex));}
std::uint64_t idClientGame::GetLobbyUserIdFromPlayerIndex(int playerIndex)const{return ValidPlayer(playerIndex)?lobbyIds[static_cast<std::size_t>(playerIndex)]:0;}
void idClientGame::SetLobbyUserIdForPlayerIndex(int playerIndex,std::uint64_t id){if(ValidPlayer(playerIndex))lobbyIds[static_cast<std::size_t>(playerIndex)]=id;}
const std::string* idClientGame::GetProfileFromPlayerIndex(int playerIndex)const{return ValidPlayer(playerIndex)?&profiles[static_cast<std::size_t>(playerIndex)]:nullptr;}
const std::string* idClientGame::GetLocalUserFromPlayerIndex(int playerIndex)const{return ValidPlayer(playerIndex)?&localUsers[static_cast<std::size_t>(playerIndex)]:nullptr;}
bool idClientGame::PlayerIndexIsLocal(int playerIndex)const{return ValidPlayer(playerIndex)&&localPlayers[static_cast<std::size_t>(playerIndex)];}
void idClientGame::SetPlayerLocal(int playerIndex,bool local,std::string user,std::string profile){if(!ValidPlayer(playerIndex))return;const auto i=static_cast<std::size_t>(playerIndex);localPlayers[i]=local;localUsers[i]=std::move(user);profiles[i]=std::move(profile);}

void idClientGame::SubmitEffectPhysicsCollisionQueries(){collisionQueries+=static_cast<int>(simulatingEffects.size());}
const std::string& idClientGame::GetEffectsModel()const{static const std::string value{"_effects"};return value;}
const std::string& idClientGame::GetDecalModel()const{static const std::string value{"_decals"};return value;}
const std::string& idClientGame::GetBeamModel()const{static const std::string value{"_beams"};return value;}
const std::string& idClientGame::GetBeamDepthHackModel()const{static const std::string value{"_beams_depthhack"};return value;}
void* idClientGame::GetDeferredDecalMgr(){return &managerTokens[0];}
void* idClientGame::GetWeaponTraceMgr(){return &managerTokens[1];}
void* idClientGame::GetWeaponImpactMgr(){return &managerTokens[2];}
void* idClientGame::GetEffectsModelMgr(){return &managerTokens[3];}
localView_t* idClientGame::GetLocalViewForPlayerIndex(int playerIndex){return ValidPlayer(playerIndex)?&localViews[static_cast<std::size_t>(playerIndex)]:nullptr;}
bool idClientGame::RenderLocalViewForPlayer(int playerIndex){if(!ValidPlayer(playerIndex))return false;++renderCount;return true;}
bool idClientGame::HandleGuiEventForPlayer(int playerIndex,const std::string& event){if(!ValidPlayer(playerIndex))return false;RecordMessage("gui",event);return true;}
bool idClientGame::IsPlayerControlInhibited(int playerIndex)const{return ValidPlayer(playerIndex)&&controlInhibited[static_cast<std::size_t>(playerIndex)];}
void idClientGame::RecordMessage(const char* kind,const std::string& message){messages.emplace_back(std::string(kind)+":"+message);}
#define WOLF_DEFINE_MESSAGE(name) void idClientGame::name(const std::string& message){RecordMessage(#name,message);}
WOLF_CLIENT_MESSAGE_HANDLERS(WOLF_DEFINE_MESSAGE)
#undef WOLF_DEFINE_MESSAGE

const std::vector<idPresentable*>& idClientGame::GetObjectList()const{return presentables;}
int idClientGame::GetObjectListSize()const{return static_cast<int>(std::count_if(presentables.begin(),presentables.end(),[](const auto* p){return p!=nullptr;}));}
float idClientGame::CalcHitscanDamageScale(float distance,float falloffDistance)const{return falloffDistance<=0.0F?1.0F:std::clamp(1.0F-distance/falloffDistance,0.0F,1.0F);}
idVec3 idClientGame::GetRandomSpreadTraceDir(const idVec3& direction,float spread,std::uint32_t seed)const{std::mt19937 rng(seed);std::uniform_real_distribution<float>d(-spread,spread);idVec3 v{direction.x+d(rng),direction.y+d(rng),direction.z+d(rng)};const float len=std::sqrt(v.x*v.x+v.y*v.y+v.z*v.z);return len>0.0F?idVec3{v.x/len,v.y/len,v.z/len}:direction;}
std::vector<idUserCmd> idClientGame::GetPlayerPresentableCmds(int playerIndex)const{auto* player=FindPlayerPresentableByIndex(playerIndex);return player?player->GetUserCmdsForNetSend():std::vector<idUserCmd>{};}
idReplicatedSound* idClientGame::FindReplicatedSound(int index,int channel){for(int i=0;i<replicatedSounds.Num();++i)if(replicatedSounds[i].presentableIndex==index&&replicatedSounds[i].channel==channel)return &replicatedSounds[i];return nullptr;}
void idClientGame::ReplicatePlaySound(int index,int channel,const std::string& shader){auto* sound=FindReplicatedSound(index,channel);if(!sound){idReplicatedSound value;value.presentableIndex=index;value.channel=channel;replicatedSounds.Append(value);sound=&replicatedSounds[replicatedSounds.Num()-1];}sound->shader=shader;sound->playing=true;}
void idClientGame::ReplicateSoundVolume(int i,int c,float v){if(auto*s=FindReplicatedSound(i,c))s->volume=v;}
void idClientGame::ReplicateSoundPitch(int i,int c,float v){if(auto*s=FindReplicatedSound(i,c))s->pitch=v;}
void idClientGame::ReplicateSoundModify(int i,int c,float v,float p){ReplicateSoundVolume(i,c,v);ReplicateSoundPitch(i,c,p);}
void idClientGame::ReplicateSoundVolumeAdjustment(int i,int c,float v){if(auto*s=FindReplicatedSound(i,c))s->volumeAdjustment=v;}
void idClientGame::ReplicateSoundClearVolumeAdjustment(int i,int c){ReplicateSoundVolumeAdjustment(i,c,1.0F);}
idPresentablePlayer* idClientGame::GetLocalPresentablePlayer()const{for(int i=0;i<4;++i)if(localPlayers[static_cast<std::size_t>(i)])if(auto*p=FindPlayerPresentableByIndex(i))return p;return nullptr;}
void idClientGame::PlaySoundForAllPeers(const std::string& shader){RecordMessage("sound",shader);}
void idClientGame::DisplayTextChat(const std::string& text){RecordMessage("chat",text);}
idPresentable* idClientGame::GetPresentableByEntityNum(int entityNumber)const{auto found=entities.find(entityNumber);return found==entities.end()?nullptr:found->second;}
int idClientGame::GetPlayerGameTime(int playerIndex)const{return ValidPlayer(playerIndex)?playerTimes[static_cast<std::size_t>(playerIndex)]:0;}
int idClientGame::GetPreviousPlayerGameTime(int playerIndex)const{return ValidPlayer(playerIndex)?previousPlayerTimes[static_cast<std::size_t>(playerIndex)]:0;}
std::uint8_t idClientGame::ConstructPeerMaskForPlayer(int playerIndex)const{return ValidPlayer(playerIndex)?static_cast<std::uint8_t>(1U<<playerIndex):0;}
std::uint32_t idClientGame::GeneratePredictionKey(int playerIndex){return (++predictionCounter<<2)|static_cast<std::uint32_t>(std::max(playerIndex,0)&3);}
idPresentable* idClientGame::GetTracePresentable(int entityNumber)const{return GetPresentableByEntityNum(entityNumber);}
void idClientGame::ClientDisablePresentablePushers(bool disable){pushersDisabled=disable;}
idPresentableVehicle* idClientGame::GetVehiclePresentable(int index)const{return dynamic_cast<idPresentableVehicle*>(GetPresentableByIndex(index));}
void idClientGame::GenerateInputToPlayerIndexRouting(const std::vector<int>& devices){inputRouting.fill(-1);for(std::size_t i=0;i<devices.size()&&i<inputRouting.size();++i)inputRouting[i]=devices[i];}
int idClientGame::GetPlayerIndexFromInputDevice(int device)const{for(std::size_t i=0;i<inputRouting.size();++i)if(inputRouting[i]==device)return static_cast<int>(i);return -1;}
void idClientGame::ClearViewCallbacks(){viewCallbacks.fill(nullptr);}
void idClientGame::DeleteAllPresentables(){presentables.assign(presentables.size(),nullptr);entities.clear();serialized.assign(serialized.size(),false);}
void idClientGame::ShowSynchronousPerf(){RecordMessage("debug","synchronous_perf");}
void idClientGame::ShowThinks(){RecordMessage("debug","thinks:"+std::to_string(thinks.Num()));}
void idClientGame::ShowPresentableInfo(){RecordMessage("debug","presentables:"+std::to_string(GetObjectListSize()));}
bool idClientGame::RenderLocalViewForGame(){bool rendered=false;for(int i=0;i<4;++i)if(localPlayers[static_cast<std::size_t>(i)])rendered=RenderLocalViewForPlayer(i)||rendered;return rendered;}
void idClientGame::ReplicateStopSound(int i,int c){if(auto*s=FindReplicatedSound(i,c))s->playing=false;}
void idClientGame::SendChatText(const std::string& text){DisplayTextChat(text);}
void idClientGame::RemoveSimulatingEffectPhysics(idPresentable* p){simulatingEffects.erase(std::remove(simulatingEffects.begin(),simulatingEffects.end(),p),simulatingEffects.end());}
void idClientGame::AddSimulatingEffectPhysics(idPresentable* p){if(p&&std::find(simulatingEffects.begin(),simulatingEffects.end(),p)==simulatingEffects.end())simulatingEffects.push_back(p);}
void idClientGame::ShowAIThinks(){RecordMessage("debug","ai_thinks:"+std::to_string(aiThinks.Num()));}

std::vector<std::uint8_t> idClientGame::WriteToSnapshot()const{std::vector<std::uint8_t> data(sizeof(serverTime)+sizeof(previousServerTime)+sizeof(playerTimes));std::size_t o=0;std::memcpy(data.data()+o,&serverTime,sizeof(serverTime));o+=sizeof(serverTime);std::memcpy(data.data()+o,&previousServerTime,sizeof(previousServerTime));o+=sizeof(previousServerTime);std::memcpy(data.data()+o,playerTimes.data(),sizeof(playerTimes));return data;}
void idClientGame::ReadFromSnapshot(const std::vector<std::uint8_t>& data){const std::size_t required=sizeof(serverTime)+sizeof(previousServerTime)+sizeof(playerTimes);if(data.size()<required)return;std::size_t o=0;std::memcpy(&serverTime,data.data()+o,sizeof(serverTime));o+=sizeof(serverTime);std::memcpy(&previousServerTime,data.data()+o,sizeof(previousServerTime));o+=sizeof(previousServerTime);std::memcpy(playerTimes.data(),data.data()+o,sizeof(playerTimes));}
void idClientGame::FinishLaunchProjectile(){RecordMessage("fire","projectile_launched");}
std::string idClientGame::AllocRenderModel(const std::string& name){renderModels.push_back(name);return renderModels.back();}
int idClientGame::FindFreePresentableIndex()const{for(std::size_t i=0;i<presentables.size();++i)if(!presentables[i])return static_cast<int>(i);return static_cast<int>(presentables.size());}
idViewCallbacks* idClientGame::GetLocalViewCallbacks(int playerIndex)const{return ValidPlayer(playerIndex)?viewCallbacks[static_cast<std::size_t>(playerIndex)]:nullptr;}
void idClientGame::MarkPresentableSerialized(int index){if(index>=0){if(static_cast<std::size_t>(index)>=serialized.size())serialized.resize(static_cast<std::size_t>(index)+1);serialized[static_cast<std::size_t>(index)]=true;}}
void idClientGame::MarkAllPresentablesAsSerialized(){serialized.assign(presentables.size(),true);}
int idClientGame::AddPresentable(idPresentable* p){const int index=FindFreePresentableIndex();return AddPresentable(index,p)?index:-1;}
bool idClientGame::AddPresentable(int index,idPresentable* p){if(index<0||!p)return false;if(static_cast<std::size_t>(index)>=presentables.size()){presentables.resize(static_cast<std::size_t>(index)+1);serialized.resize(presentables.size());}if(presentables[static_cast<std::size_t>(index)]&&presentables[static_cast<std::size_t>(index)]!=p)return false;presentables[static_cast<std::size_t>(index)]=p;p->SetEntityNumber(index);entities[index]=p;return true;}
bool idClientGame::RemovePresentable(idPresentable* p){auto found=std::find(presentables.begin(),presentables.end(),p);if(found==presentables.end())return false;const int entity=p->GetEntityNumber();presentables[static_cast<std::size_t>(found-presentables.begin())]=nullptr;entities.erase(entity);return true;}
void idClientGame::MakePresentableNonReplicated(idPresentable* p){if(p)MarkPresentableSerialized(p->GetEntityNumber());}
void idClientGame::AddAIThink(idPresentable* p,int time){if(p)aiThinks.Append({p,time});}
void idClientGame::AddThink(idPresentable* p,int time){if(p)thinks.Append({p,time});}
void idClientGame::PlayShaderDirectly(const std::string& shader){RecordMessage("shader",shader);}
void idClientGame::ImpactEffect(const idVec3& point){(void)point;++impactCount;}
void idClientGame::ProjectileImpact(const idVec3& point){ImpactEffect(point);RecordMessage("impact","projectile");}
void idClientGame::DoTracer(const idVec3& start,const idVec3& end){(void)start;(void)end;++tracerCount;}
void idClientGame::FinishHitScan(const idTestFireResults& result){if(result.hit)ImpactEffect(result.impact);}
void idClientGame::FinishFire(){++finishedFireCount;}
void idClientGame::SetPlayerGameTime(int playerIndex,int time){if(!ValidPlayer(playerIndex))return;const auto i=static_cast<std::size_t>(playerIndex);previousPlayerTimes[i]=playerTimes[i];playerTimes[i]=time;}
void idClientGame::SetPlayerControlInhibited(int playerIndex,bool value){if(ValidPlayer(playerIndex))controlInhibited[static_cast<std::size_t>(playerIndex)]=value;}

void messagemode_f(idClientGame& game,const std::string& text){game.SendChatText(text);messagemode_v.valueString=text;}

template<> void idSerializer::SerializeQ<60,8>(float& value){value=std::clamp(value,-60.0F,60.0F);SerializeFloat(value);}
template<> idVec3 idBitMsg::ReadQuantizedVector<idVec3,32768,11>(){return ReadQuantizedVector<idVec3,32768,17>();}
template<> void idBitMsg::WriteQuantizedVector<idVec3,32768,11>(const idVec3& value){WriteQuantizedVector<idVec3,32768,17>(value);}
template class idPresentablePtr<idPresentablePlayer>;
template class idList<idRecycledTraceModel*,13>;
template class idBlockAlloc<idTraceModelCache::trmCache_t,64,13>;
template bool idList<aiThink_t,5>::Resize(int);
template aiThink_t* idList<aiThink_t,5>::Alloc();
template bool idList<presentableThink_t,5>::Resize(int);
template presentableThink_t* idList<presentableThink_t,5>::Alloc();
template void idList<idPresentable*,5>::SetNum(int);
template bool idList<idReplicatedSound,5>::Resize(int);
template bool idList<idReplicatedSound,5>::RemoveIndex(int);
template int idList<idReplicatedSound,5>::Append(const idReplicatedSound&);


