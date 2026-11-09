#pragma once

#include "engine/cm/collisionmodel.h"
#include "engine/gamelib/idspawnid_abi.h"
#include "engine/gamelib/physics/clip.h"
#include "tungsten/game/game_runtime.h"

#include <cstdint>
#include <unordered_map>

struct aiLatchedState_t;
struct aiLatchedEvent_t;

class idAIGameState {
public:
  idAIGameState();
  ~idAIGameState();
  idAIGameState(const idAIGameState&) = delete;
  idAIGameState& operator=(const idAIGameState&) = delete;

  void SetMaster(idSpawnId entity, idSpawnId master) {
    masterIds[entity.value] = master;
  }
  idSpawnId GetMaster(idSpawnId entity) const {
    const auto found = masterIds.find(entity.value);
    return found != masterIds.end() ? found->second : idSpawnId{};
  }
  const aiLatchedState_t* GetState(const idSpawnId* entity) const;
  const aiLatchedState_t* GetStateForEntityNum(int entityNumber) const;
  const aiLatchedEvent_t* GetEvent(int eventId) const;
  static int GetEntityType(const idEntity* entity);
  bool GetPhysicalState(const idGameLocal* game, const idEntity* entity,
                        struct aiPhysicalState_t* physicalState) const;
  void LatchEntityState(const idGameLocal* game, idEntity* entity, bool force);
  void UpdateFromGame(const idGameLocal* game);
  void UpdateForcedLatches(const idGameLocal* game);
  void ForceLatch(idSpawnId spawnId);
  void LatchEvent(const aiLatchedEvent_t& event);
  void Shutdown();
  void RegisterEntity(idSpawnId spawnId, idEntity* entity) {
    entities[spawnId.value] = entity;
  }
  idEntity* GetEntity(idSpawnId spawnId) const {
    const auto found = entities.find(spawnId.value);
    return found != entities.end() ? found->second : nullptr;
  }

  int gameTime{0};
  int gameFrame{-1};

private:
  struct storage_t;
  std::unordered_map<int, idSpawnId> masterIds;
  std::unordered_map<int, idEntity*> entities;
  storage_t* storage{nullptr};
};

class idAIVisibilityWorld {
public:
  virtual ~idAIVisibilityWorld() = default;
  virtual void Trace(const idVec3& fromPoint, const idVec3& toPoint,
                     int clipMask, int passEntityNumber,
                     trace_t& trace) = 0;
  virtual std::uint64_t SubmitTrace(const idVec3& fromPoint,
                                    const idVec3& toPoint, int clipMask,
                                    int passEntityNumber) = 0;
  virtual bool ResolveTrace(std::uint64_t queryIndex, trace_t& trace) = 0;
};

class idAIVisibility {
public:
  static void SetWorld(idAIVisibilityWorld* value) { world = value; }
  static void TraceVisible_Submit(const idVec3& fromPoint,
                                  const idVec3& toPoint, idSpawnId ignoreId,
                                  bool seeThroughBreakables,
                                  idClipQuery* clipQuery);
  static float ByteToFOVDot(std::uint8_t fov);
  static float CheckFOV(const idVec3& direction, const idVec3& eyeForward,
                        float cosineFov);
  static float CheckFOV(const idVec3& direction, const idVec3& eyeForward,
                        float cosineHorizontalFov,
                        float cosineVerticalFov);
  static bool TraceVisible(idAIGameState* gameState,
                           const idVec3& fromPoint, const idVec3& toPoint,
                           idSpawnId ignoreId, idSpawnId destinationId,
                           trace_t* trace, bool seeThroughBreakables);
  static std::uint8_t FOVDotToByte(float fov);
  static bool TraceVisible_Resolve(idAIGameState* gameState,
                                   idSpawnId destinationId,
                                   idClipQuery* clipQuery, trace_t* trace);

private:
  static bool IsVisibleHit(const idAIGameState* gameState,
                           idSpawnId destinationId, const trace_t& trace);
  inline static idAIVisibilityWorld* world{nullptr};
};

extern idCVar ai_showVisTrace;
