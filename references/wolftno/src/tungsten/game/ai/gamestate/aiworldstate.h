#pragma once

#include "tungsten/game/ai/gamestate/aigamestate.h"
#include "tungsten/game/ai/targetting/targetcache.h"
#include "shared/idlib/containers/list.h"

#include <array>
#include <memory>
#include <vector>

class idAIWorldState {
public:
  static constexpr int MAX_ENTITY_STATES = 8;
  static constexpr float AI_CLOSE_ENOUGH_FOR_VIS_SQR = 4096.0F;

  idAIWorldState();
  virtual ~idAIWorldState();
  void Init(float visibleSightTime, int friendlyVisibilityEffectsMask,
            int enemyVisibilityEffectsMask);
  float VisibilityEffects(const idAILogic* owner, float input,
                          float fovWeight, const aiLatchedState_t* state,
                          visibleType_t visibility, float distance,
                          float displacement, bool isRelaxed) const;
  void UpdateSeen(idAIGameState* gameState, const idAILogic* owner,
                  const trackingParms_t* trackingParms);
  idAIEntityState* StateForSpawnId(const idSpawnId& entityId);
  const idAIEntityState* StateForSpawnId(const idSpawnId& entityId) const;
  idAIEntityState* StateForEntity(const idEntity* entity);
  void Clear();
  void UpdateSprings(const trackingParms_t* trackingParms);
  int GetCurrentRefreshInterval() const;
  void RemoveLeastRelevant(idAIGameState* gameState, const idAILogic* owner);
  float GetStimulusForVisibility(int visibility, int* interval) const;
  idAIEntityState* InternalAddEntityState(
      idAIGameState* gameState, const idAILogic* owner,
      const idSpawnId& entityId, aiStimulus_t stimulusType,
      float stimulusAmount, const idSpawnId& sourceId,
      int sourceEventHandle, const aiLatchedState_t* state);
  const idAIEntityState* AddEntityStateFromEvent(
      idAIGameState* gameState, idAILogic* owner,
      const aiLatchedEvent_t* event, aiStimulus_t stimulusType,
      float stimulusAmount, const idSpawnId& sourceId);
  const idAIEntityState* AddEntityState(
      idAIGameState* gameState, idAILogic* owner,
      const idSpawnId& entityId, aiStimulus_t stimulusType,
      float stimulusAmount, const idSpawnId& sourceId);
  const aiLatchedState_t* AddEntityStateFakePosition(
      idAIGameState* gameState, idAILogic* owner,
      const idSpawnId& entityId, aiStimulus_t stimulusType,
      float stimulusAmount, const idSpawnId& sourceId,
      const idVec3& fakePosition);
  idAIEntityState* AddEntityState(
      idAIGameState* gameState, idAILogic* owner,
      const idSpawnId& entityId, aiStimulus_t stimulusType,
      float stimulusAmount, const idSpawnId& sourceId,
      const aiSenseState_t* sourceSense);
  void ForceAwarenessByDistance(idEntity* owner, float radius,
                                bool includePlayers, bool includeAI);
  void ResolveDeferredQueries(idAIGameState* gameState, idAILogic* owner);
  void UpdateInternal(idAIGameState* gameState, idAILogic* owner,
                      trackingParms_t* trackingParms,
                      const aiLatchedState_t* ownerState);
  void Update(idAIGameState* gameState, idAILogic* owner,
              trackingParms_t* trackingParms);

  int NumEntityStates() const { return static_cast<int>(activeIndices.size()); }
  idAIEntityState* AddEntityStateForTest(const idSpawnId& entityId) {
    if (idAIEntityState* existing = StateForSpawnId(entityId)) return existing;
    if (freeIndices.empty()) return nullptr;
    const int slot = freeIndices.back();
    freeIndices.pop_back();
    activeIndices.push_back(slot);
    idAIEntityState& state =
        entityStates[static_cast<std::size_t>(slot)];
    state.spawnId = entityId;
    state.active = true;
    return &state;
  }
  const idAIEntityState* GetEntityStateAt(int index) const {
    return index >= 0 && index < static_cast<int>(activeIndices.size())
        ? &entityStates[static_cast<std::size_t>(activeIndices[index])]
        : nullptr;
  }
  idTargetCache& GetTargetCache() { return targetCache; }
  const idTargetCache& GetTargetCache() const { return targetCache; }
  void SetRadius(float value) { radius = value; }
  float GetRadiusForTest() const { return radius; }
  aiStimulus_t GetLastAddedStimulusForTest() const {
    return lastAddedStimulus;
  }
  void SetOwnerInCover(bool value) { ownerInCover = value; }
  bool IsOwnerInCoverForTest() const { return ownerInCover; }
  int GetUpdateCountForTest() const { return updateCount; }
  const trackingParms_t& GetLastTrackingParmsForTest() const {
    return lastTrackingParms;
  }

private:
  idTargetCache targetCache;
  std::array<idAIEntityState, MAX_ENTITY_STATES> entityStates{};
  std::vector<int> activeIndices;
  std::vector<int> freeIndices;
  idList<idAIEntityState::aiDefVisInfo_t, 5> deferredVisibility;
  idList<idAIEntityState::aiDefVisInfo_t, 5> deferredRefresh;
  std::vector<std::unique_ptr<aiLatchedState_t>> fakeStates;
  float radius{2048.0F};
  float visibleSightTime{0.3F};
  int friendlyVisibilityEffectsMask{3903};
  int enemyVisibilityEffectsMask{3967};
  int initialUpdateCount{2};
  int lastUpdateFrameNum{-1};
  idVec3 lastUpdatePos{-65535.0F, -65535.0F, -65535.0F};
  bool updateFriendly{true};
  bool refreshFriendly{true};
  bool ownerInCover{false};
  aiStimulus_t lastAddedStimulus{AISTIM_NONE};
  idAIGameState* lastGameState{nullptr};
  idAILogic* lastOwner{nullptr};
  trackingParms_t lastTrackingParms{};
  int updateCount{0};
};

idAIEntityState::aiDefVisInfo_t* AllocQuery(
    idList<idAIEntityState::aiDefVisInfo_t, 5>* queryList);

extern idCVar ai_visTraceOutsideFOV;
extern idCVar ai_perceptionRadiusOverride;
extern idCVar ai_debugDeferredWorldState;
