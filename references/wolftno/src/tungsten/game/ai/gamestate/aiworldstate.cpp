#include "tungsten/game/ai/gamestate/aiworldstate.h"

#include <algorithm>
#include <cmath>
#include <limits>

// DIA: idAIWorldState::Init
void idAIWorldState::Init(float newVisibleSightTime,
                          int friendlyEffectsMask,
                          int enemyEffectsMask) {
  visibleSightTime = std::max(0.001F, newVisibleSightTime);
  friendlyVisibilityEffectsMask = friendlyEffectsMask;
  enemyVisibilityEffectsMask = enemyEffectsMask;
  Clear();
}

// DIA: idAIWorldState::VisibilityEffects
float idAIWorldState::VisibilityEffects(
    const idAILogic*, float input, float fovWeight,
    const aiLatchedState_t* state, visibleType_t visibility,
    float distance, float displacement, bool isRelaxed) const {
  if (state == nullptr) return 0.0F;
  const int effects = enemyVisibilityEffectsMask;
  float result = std::max(0.0F, input);
  if ((effects & 1) != 0)
    result *= std::min(1.0F, 500.0F / std::max(1.0F, distance));
  if ((effects & 2) != 0 && fovWeight <= 0.4F) result *= 0.25F;
  if ((effects & 4) != 0 && distance >= 100.0F &&
      (state->physical.entFlags & 2U) != 0)
    result *= isRelaxed ? 0.25F : 0.5F;
  if ((effects & 8) != 0 && visibility == VISIBLE_IN_COVER)
    result *= isRelaxed ? 0.25F : 0.5F;
  if ((effects & 0x10) != 0 && displacement > 256.0F) result *= 1.5F;
  if ((effects & 0x20) != 0 && state->physical.velocity.Length() >= 100.0F)
    result *= 2.0F;
  if ((effects & 0x40) != 0)
    result *= state->physical.stimScale == 0
                  ? 1.0F : state->physical.stimScale / 255.0F;
  if ((effects & 0x80) != 0)
    result *= state->physical.lightScale / 255.0F;
  if ((effects & 0x800) != 0) {
    const idVec3 size = state->physical.bounds[1] - state->physical.bounds[0];
    result *= ((size.x + size.y) * 0.5F * std::max(size.z, 90.0F)) /
              2880.0F;
  }
  return std::clamp(result, 0.0F, 1.0F);
}

// DIA: idAIWorldState::UpdateSeen
void idAIWorldState::UpdateSeen(idAIGameState* gameState,
                                const idAILogic* owner,
                                const trackingParms_t* trackingParms) {
  for (int active = static_cast<int>(activeIndices.size()) - 1;
       active >= 0; --active) {
    const int slot = activeIndices[static_cast<std::size_t>(active)];
    idAIEntityState& state = entityStates[static_cast<std::size_t>(slot)];
    if (!state.active || gameState == nullptr ||
        gameState->GetState(&state.spawnId) == nullptr) {
      state.active = false;
      freeIndices.push_back(slot);
      activeIndices.erase(activeIndices.begin() + active);
      continue;
    }
    if (state.lastVisible > VISIBLE_NOT_VISIBLE)
      state.UpdateSeen(gameState, owner);
    state.UpdateSprings(trackingParms);
  }
}

// DIA: idAIWorldState::StateForSpawnId
idAIEntityState* idAIWorldState::StateForSpawnId(const idSpawnId& entityId) {
  for (int slot : activeIndices) {
    idAIEntityState& state = entityStates[static_cast<std::size_t>(slot)];
    if (state.spawnId.value == entityId.value) return &state;
  }
  return nullptr;
}

const idAIEntityState* idAIWorldState::StateForSpawnId(
    const idSpawnId& entityId) const {
  for (int slot : activeIndices) {
    const idAIEntityState& state = entityStates[static_cast<std::size_t>(slot)];
    if (state.spawnId.value == entityId.value) return &state;
  }
  return nullptr;
}

// DIA: idAIWorldState::StateForEntity
idAIEntityState* idAIWorldState::StateForEntity(const idEntity* entity) {
  return entity != nullptr ? StateForSpawnId({entity->GetEntityNumber()})
                           : nullptr;
}

// DIA: aiSenseState_t::aiSenseState_t
aiSenseState_t::aiSenseState_t(const aiSenseState_t& other)
    : unConfirmedPhysical(other.unConfirmedPhysical),
      lastUnconfirmedStimTime(other.lastUnconfirmedStimTime),
      confirmedPhysical(other.confirmedPhysical),
      lastConfirmedStimTime(other.lastConfirmedStimTime),
      integrator(other.integrator), aiOrigin(other.aiOrigin),
      senseType(other.senseType), stimulusType(other.stimulusType),
      senseFlags(other.senseFlags) {}

// DIA: idAIWorldState::Clear
void idAIWorldState::Clear() {
  targetCache.Clear();
  activeIndices.clear();
  freeIndices.clear();
  for (int slot = MAX_ENTITY_STATES - 1; slot >= 0; --slot) {
    entityStates[static_cast<std::size_t>(slot)].active = false;
    freeIndices.push_back(slot);
  }
  deferredVisibility.Clear();
  deferredRefresh.Clear();
  fakeStates.clear();
}

// DIA: idAIWorldState::UpdateSprings
void idAIWorldState::UpdateSprings(const trackingParms_t* trackingParms) {
  for (int slot : activeIndices)
    entityStates[static_cast<std::size_t>(slot)].UpdateSprings(trackingParms);
}

// DIA: idAIWorldState::GetCurrentRefreshInterval
int idAIWorldState::GetCurrentRefreshInterval() const {
  const int users = gameLocal != nullptr ? std::max(1, gameLocal->NumAAS()) : 1;
  const int millisecondsPerFrame = gameLocal != nullptr
      ? gameLocal->GetGameMsPerFrame() : 16;
  return std::max(3, users) * millisecondsPerFrame;
}

// DIA: idAIWorldState::RemoveLeastRelevant
void idAIWorldState::RemoveLeastRelevant(idAIGameState* gameState,
                                         const idAILogic* owner) {
  int selected = -1;
  int oldestStimulus = std::numeric_limits<int>::max();
  for (int active = 0; active < static_cast<int>(activeIndices.size()); ++active) {
    idAIEntityState& state =
        entityStates[static_cast<std::size_t>(activeIndices[active])];
    if (!state.active || gameState == nullptr ||
        gameState->GetState(&state.spawnId) == nullptr) {
      selected = active;
      break;
    }
    if (owner != nullptr && state.spawnId.value == owner->curEnemyId.value)
      continue;
    int stimulusTime = -1;
    if (state.lastSense != AISENSE_NONE)
      stimulusTime = state.senses[static_cast<std::size_t>(state.lastSense)]
                         ->lastUnconfirmedStimTime;
    if (stimulusTime < oldestStimulus) {
      oldestStimulus = stimulusTime;
      selected = active;
    }
  }
  if (selected < 0) return;
  const int slot = activeIndices[static_cast<std::size_t>(selected)];
  entityStates[static_cast<std::size_t>(slot)].active = false;
  activeIndices.erase(activeIndices.begin() + selected);
  freeIndices.push_back(slot);
}

// DIA: idAIWorldState::GetStimulusForVisibility
float idAIWorldState::GetStimulusForVisibility(int visibility,
                                               int* interval) const {
  if (interval == nullptr || visibility <= VISIBLE_NOT_VISIBLE) return 0.0F;
  return (*interval * 0.001F) / visibleSightTime;
}

// DIA: idAIWorldState::InternalAddEntityState
idAIEntityState* idAIWorldState::InternalAddEntityState(
    idAIGameState* gameState, const idAILogic* owner,
    const idSpawnId& entityId, aiStimulus_t stimulusType,
    float stimulusAmount, const idSpawnId& sourceId,
    int sourceEventHandle, const aiLatchedState_t* suppliedState) {
  if (gameState == nullptr || owner == nullptr || !entityId.IsValid() ||
      entityId.value == owner->spawnId.value) return nullptr;
  const aiLatchedState_t* state = suppliedState != nullptr
      ? suppliedState : gameState->GetState(&entityId);
  if (state == nullptr) return nullptr;
  idAIEntityState* result = StateForSpawnId(entityId);
  if (result == nullptr) {
    if (freeIndices.empty()) RemoveLeastRelevant(gameState, owner);
    if (freeIndices.empty()) return nullptr;
    const int slot = freeIndices.back();
    freeIndices.pop_back();
    activeIndices.push_back(slot);
    result = &entityStates[static_cast<std::size_t>(slot)];
    result->Init(gameState, owner, entityId, state);
  }
  lastAddedStimulus = stimulusType;
  result->AddStimulus(gameState, owner, stimulusType, stimulusAmount,
                      sourceId, sourceEventHandle, state);
  return result;
}

// DIA: `idAIWorldState::InternalAddEntityState'::`1'::dtor$0

// DIA: idAIWorldState::AddEntityStateFromEvent
const idAIEntityState* idAIWorldState::AddEntityStateFromEvent(
    idAIGameState* gameState, idAILogic* owner,
    const aiLatchedEvent_t* event, aiStimulus_t stimulusType,
    float stimulusAmount, const idSpawnId& sourceId) {
  if (event == nullptr || !event->relevantId.IsValid()) return nullptr;
  return InternalAddEntityState(gameState, owner, event->relevantId,
                                stimulusType, stimulusAmount, sourceId,
                                event->eventId,
                                gameState->GetState(&event->relevantId));
}

// DIA: idAIWorldState::~idAIWorldState
idAIWorldState::~idAIWorldState() = default;

// DIA: `idAIWorldState::~idAIWorldState'::`1'::dtor$0
// DIA: `idAIWorldState::~idAIWorldState'::`1'::dtor$1
// DIA: `idAIWorldState::~idAIWorldState'::`1'::dtor$2
// DIA: `idAIWorldState::~idAIWorldState'::`1'::dtor$3

// DIA: idAIWorldState::AddEntityState
const idAIEntityState* idAIWorldState::AddEntityState(
    idAIGameState* gameState, idAILogic* owner,
    const idSpawnId& entityId, aiStimulus_t stimulusType,
    float stimulusAmount, const idSpawnId& sourceId) {
  return InternalAddEntityState(gameState, owner, entityId, stimulusType,
                                stimulusAmount, sourceId, -1,
                                gameState != nullptr
                                    ? gameState->GetState(&entityId) : nullptr);
}

// DIA: idAIWorldState::AddEntityStateFakePosition
const aiLatchedState_t* idAIWorldState::AddEntityStateFakePosition(
    idAIGameState* gameState, idAILogic* owner,
    const idSpawnId& entityId, aiStimulus_t stimulusType,
    float stimulusAmount, const idSpawnId& sourceId,
    const idVec3& fakePosition) {
  if (gameState == nullptr) return nullptr;
  const aiLatchedState_t* original = gameState->GetState(&entityId);
  if (original == nullptr) return nullptr;
  auto fake = std::make_unique<aiLatchedState_t>(*original);
  const idVec3 eyeOffset = fake->physical.eyePos - fake->physical.origin;
  fake->physical.origin = fakePosition;
  fake->physical.eyePos = fakePosition + eyeOffset;
  aiLatchedState_t* result = fake.get();
  fakeStates.push_back(std::move(fake));
  InternalAddEntityState(gameState, owner, entityId, stimulusType,
                         stimulusAmount, sourceId, -1, result);
  return result;
}

// DIA: idAIWorldState::AddEntityState
idAIEntityState* idAIWorldState::AddEntityState(
    idAIGameState* gameState, idAILogic* owner,
    const idSpawnId& entityId, aiStimulus_t stimulusType,
    float stimulusAmount, const idSpawnId& sourceId,
    const aiSenseState_t* sourceSense) {
  if (gameState == nullptr || sourceSense == nullptr) return nullptr;
  const aiLatchedState_t* original = gameState->GetState(&entityId);
  aiLatchedState_t temporary;
  if (original != nullptr) temporary = *original;
  temporary.spawnId = entityId;
  temporary.physical = *sourceSense->GetConfirmedPhysicalSafe();
  idAIEntityState* result = InternalAddEntityState(
      gameState, owner, entityId, stimulusType, stimulusAmount,
      sourceId, -1, &temporary);
  if (result != nullptr) {
    aiSenseState_t* destination =
        stimulusType == AISTIM_SIGHT ? result->senses[AISENSE_SIGHT]
                                     : result->senses[AISENSE_HEARING];
    const aiPhysicalState_t* sharedPhysical =
        sourceSense->GetConfirmedPhysicalSafe();
    destination->SetUnconfirmedPhysical(sharedPhysical);
    destination->SetConfirmedPhysical(sharedPhysical);
    destination->lastUnconfirmedStimTime = sourceSense->lastUnconfirmedStimTime;
    destination->lastConfirmedStimTime = sourceSense->lastConfirmedStimTime;
    destination->senseFlags = sourceSense->senseFlags;
  }
  return result;
}

// DIA: idList<idAIEntityState::aiDefVisInfo_t,5>::Resize
template bool idList<idAIEntityState::aiDefVisInfo_t, 5>::Resize(int);

// DIA: idAIWorldState::idAIWorldState
idAIWorldState::idAIWorldState() {
  freeIndices.reserve(MAX_ENTITY_STATES);
  activeIndices.reserve(MAX_ENTITY_STATES);
  Clear();
}

// DIA: `idAIWorldState::idAIWorldState'::`1'::dtor$0
// DIA: `idAIWorldState::idAIWorldState'::`1'::dtor$1
// DIA: `idAIWorldState::idAIWorldState'::`1'::dtor$2
// DIA: `idAIWorldState::idAIWorldState'::`1'::dtor$3
// DIA: idAIWorldState::`scalar deleting destructor'

// DIA: idAIWorldState::ForceAwarenessByDistance
void idAIWorldState::ForceAwarenessByDistance(idEntity* owner, float forceRadius,
                                               bool includePlayers,
                                               bool includeAI) {
  if (owner == nullptr || lastGameState == nullptr || lastOwner == nullptr ||
      (!includePlayers && !includeAI)) return;
  const idPhysics* ownerPhysics = owner->GetPhysics();
  if (ownerPhysics == nullptr) return;
  const float radiusSquared = forceRadius * forceRadius;
  for (int entityNumber = 0; entityNumber < 0x1fff; ++entityNumber) {
    const aiLatchedState_t* state =
        lastGameState->GetStateForEntityNum(entityNumber);
    if (state == nullptr || state->spawnId.value == lastOwner->spawnId.value)
      continue;
    if ((state->physical.origin - ownerPhysics->GetOrigin()).LengthSqr() <=
        radiusSquared)
      InternalAddEntityState(lastGameState, lastOwner, state->spawnId,
                             AISTIM_DIRECT, 1.0F, lastOwner->spawnId,
                             -1, state);
  }
}

// DIA: idAIWorldState::ResolveDeferredQueries
void idAIWorldState::ResolveDeferredQueries(idAIGameState* gameState,
                                            idAILogic* owner) {
  if (gameState == nullptr || owner == nullptr) {
    deferredVisibility.Clear();
    deferredRefresh.Clear();
    return;
  }
  for (int index = 0; index < deferredRefresh.Num(); ++index) {
    auto& query = deferredRefresh[index];
    const aiLatchedState_t* state =
        gameState->GetStateForEntityNum(query.entityNum);
    if (state == nullptr) continue;
    idAIEntityState* entityState = StateForSpawnId(state->spawnId);
    if (entityState != nullptr)
      entityState->Refresh_Resolve(gameState, this, owner, &query);
  }
  deferredRefresh.Clear();
  for (int index = 0; index < deferredVisibility.Num(); ++index) {
    auto& query = deferredVisibility[index];
    const aiLatchedState_t* state =
        gameState->GetStateForEntityNum(query.entityNum);
    if (state == nullptr) continue;
    visibleType_t visibility = query.closeEnough ? VISIBLE_VISIBLE
        : idAIEntityState::AICanSee_Resolve(owner, &query, state);
    if (visibility <= VISIBLE_NOT_VISIBLE || visibility == VISIBLE_INVALID)
      continue;
    int interval = GetCurrentRefreshInterval();
    const float baseStimulus = GetStimulusForVisibility(visibility, &interval);
    const float distance = query.delta.Length();
    const float stimulus = VisibilityEffects(
        owner, baseStimulus, query.fovWeight, state, visibility,
        distance, 0.0F, owner->isRelaxed);
    InternalAddEntityState(gameState, owner, state->spawnId, AISTIM_SIGHT,
                           stimulus, owner->spawnId, -1, state);
  }
  deferredVisibility.Clear();
}

// DIA: idList<idAIEntityState::aiDefVisInfo_t,5>::Alloc
template idAIEntityState::aiDefVisInfo_t*
idList<idAIEntityState::aiDefVisInfo_t, 5>::Alloc();

// DIA: AllocQuery
idAIEntityState::aiDefVisInfo_t* AllocQuery(
    idList<idAIEntityState::aiDefVisInfo_t, 5>* queryList) {
  if (queryList == nullptr) return nullptr;
  auto* query = queryList->Alloc();
  *query = {};
  query->entityNum = -1;
  return query;
}

// DIA: idAIWorldState::UpdateInternal
void idAIWorldState::UpdateInternal(idAIGameState* gameState,
                                    idAILogic* owner,
                                    trackingParms_t* trackingParms,
                                    const aiLatchedState_t* ownerState) {
  ResolveDeferredQueries(gameState, owner);
  if (gameState == nullptr || owner == nullptr || ownerState == nullptr) return;
  lastGameState = gameState;
  lastOwner = owner;
  lastUpdateFrameNum = gameState->gameFrame;
  lastUpdatePos = ownerState->physical.origin;
  const float useRadius = ai_perceptionRadiusOverride.GetFloat() > 0.0F
                              ? ai_perceptionRadiusOverride.GetFloat() : radius;
  const float radiusSquared = useRadius * useRadius;
  for (int entityNumber = 0; entityNumber < 0x1fff; ++entityNumber) {
    const aiLatchedState_t* state = gameState->GetStateForEntityNum(entityNumber);
    if (state == nullptr || state->spawnId.value == owner->spawnId.value)
      continue;
    const idVec3 delta = state->physical.origin - ownerState->physical.origin;
    if (delta.LengthSqr() > radiusSquared) continue;
    float fovWeight = 0.0F;
    const bool withinFov = idAIEntityState::WithinFOV(owner, state, &fovWeight);
    const bool closeEnough = delta.LengthSqr() <= AI_CLOSE_ENOUGH_FOR_VIS_SQR;
    if (withinFov || closeEnough || ai_visTraceOutsideFOV.GetInteger() != 0) {
      auto* query = AllocQuery(&deferredVisibility);
      query->entityNum = entityNumber;
      query->delta = delta;
      query->fovWeight = fovWeight;
      query->closeEnough = closeEnough;
      idAIEntityState::AICanSee_Submit(owner, state, false, query);
    }
    idAIEntityState* existing = StateForSpawnId(state->spawnId);
    if (existing != nullptr) {
      auto* refresh = AllocQuery(&deferredRefresh);
      refresh->entityNum = entityNumber;
      refresh->delta = delta;
      refresh->fovWeight = fovWeight;
      refresh->closeEnough = closeEnough;
      existing->Refresh_Submit(gameState, this, owner, refresh);
    }
  }
  if (initialUpdateCount > 0) --initialUpdateCount;
  UpdateSeen(gameState, owner, trackingParms);
}

// DIA: idAIWorldState::Update
void idAIWorldState::Update(idAIGameState* gameState, idAILogic* owner,
                            trackingParms_t* trackingParms) {
  if (owner == nullptr) return;
  ++updateCount;
  lastTrackingParms = trackingParms != nullptr ? *trackingParms
                                               : trackingParms_t{};
  const aiLatchedState_t* ownerState = owner->GetState();
  if (ownerState == nullptr) return;
  UpdateInternal(gameState, owner, trackingParms, ownerState);
  UpdateSprings(trackingParms);
  targetCache.Init(ownerState, this);
}

// DIA: `dynamic initializer for 'ai_visTraceOutsideFOV''
// DIA: `dynamic atexit destructor for 'ai_visTraceOutsideFOV''
idCVar ai_visTraceOutsideFOV("ai_visTraceOutsideFOV", "0", 1,
                             "trace entities outside the field of view");
// DIA: `dynamic initializer for 'ai_perceptionRadiusOverride''
// DIA: `dynamic atexit destructor for 'ai_perceptionRadiusOverride''
idCVar ai_perceptionRadiusOverride("ai_perceptionRadiusOverride", "-1", 4,
                                   "override AI perception radius");
// DIA: `dynamic initializer for 'ai_debugDeferredWorldState''
// DIA: `dynamic atexit destructor for 'ai_debugDeferredWorldState''
idCVar ai_debugDeferredWorldState("ai_debugDeferredWorldState", "0", 1,
                                  "draw deferred AI perception state");
