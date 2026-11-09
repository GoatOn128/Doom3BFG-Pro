#include "tungsten/game/ai/gamestate/aientitystate.h"

#include <cmath>

namespace {
float Distance(const idVec3& a, const idVec3& b) {
  return (a - b).Length();
}

idVec3 BoundsCenter(const aiPhysicalState_t& physical) {
  return physical.origin + (physical.bounds[0] + physical.bounds[1]) * 0.5F;
}
}

// DIA: idAILogic::idAILogic
idAILogic::idAILogic(const idSpawnId& newSpawnId,
                     const idSpawnId& newEnemyId,
                     idEncounterGroup* newEncounterGroup,
                     idFaction* newFaction, float combatTimeout,
                     float searchTimeout, bool relaxed, bool searching)
    : spawnId(newSpawnId), curEnemyId(newEnemyId),
      encounterGroup(newEncounterGroup), faction(newFaction),
      combatTimeoutInterval(combatTimeout),
      searchTimeoutInterval(searchTimeout), isRelaxed(relaxed),
      isSearching(searching) {}

// DIA: idAIEntityState::GetEntity
idEntity* idAIEntityState::GetEntity() const { return entity; }

// DIA: aiPhysicalState_t::aiPhysicalState_t
aiPhysicalState_t::aiPhysicalState_t() {
  bounds[0] = {-16.0F, -16.0F, 0.0F};
  bounds[1] = {16.0F, 16.0F, 72.0F};
}

// DIA: idAILogic::GetState
const aiLatchedState_t* idAILogic::GetState() const {
  return stateSource != nullptr ? stateSource->GetState(&spawnId) : nullptr;
}

// DIA: aiSenseState_t::aiSenseState_t
aiSenseState_t::aiSenseState_t() = default;

// DIA: aiSenseState_t::GetConfirmedPhysicalSafe
const aiPhysicalState_t* aiSenseState_t::GetConfirmedPhysicalSafe() const {
  return lastConfirmedStimTime >= 0 ? &confirmedPhysical
                                    : &unConfirmedPhysical;
}

// DIA: aiSenseState_t::GetConfirmedStimTimeSafe
int aiSenseState_t::GetConfirmedStimTimeSafe() const {
  if (lastConfirmedStimTime >= 0) return lastConfirmedStimTime;
  return std::max(0, lastUnconfirmedStimTime);
}

// DIA: aiSenseState_t::ExtrapolatePosition
void aiSenseState_t::ExtrapolatePosition(float seconds,
                                         idVec3* futurePos) const {
  if (futurePos == nullptr) return;
  const aiPhysicalState_t* physical = GetConfirmedPhysicalSafe();
  *futurePos = HasSpring() && lastConfirmedStimTime >= 0
                   ? GetSpring()->p1 : physical->origin;
  *futurePos += physical->velocity * seconds;
}

// DIA: idAISenseState_Sight::InitSpring
void idAISenseState_Sight::InitSpring(const aiLatchedState_t* state,
                                      const idAILogic*) {
  confirmedOriginSpring = {};
  if (state != nullptr) {
    confirmedOriginSpring.p0 = state->physical.origin;
    confirmedOriginSpring.p1 = state->physical.origin;
  }
  springInitialized = true;
}

// DIA: idAISenseState_Sight::Update
void idAISenseState_Sight::Update(const aiLatchedState_t* state) {
  if (state == nullptr) return;
  confirmedOriginSpring.p0 = state->physical.origin +
      state->physical.velocity * ai_originSpringExtrapolate.GetFloat();
}

// DIA: idAIEntityState::~idAIEntityState
idAIEntityState::~idAIEntityState() = default;

// DIA: idAIEntityState::Init
void idAIEntityState::Init(idAIGameState* gameState, const idAILogic* owner,
                           const idSpawnId& entId,
                           const aiLatchedState_t* suppliedState) {
  initTime = gameState != nullptr ? gameState->gameTime : 0;
  currentTime = initTime;
  spawnId = entId;
  firstSpot = -3600000;
  spotDuration = 0;
  spotLostDuration = -1;
  lastSense = AISENSE_NONE;
  active = false;
  awarenessChanged = false;
  entity = gameState != nullptr ? gameState->GetEntity(entId) : entity;
  const aiLatchedState_t* state = suppliedState;
  if (state == nullptr && gameState != nullptr) state = gameState->GetState(&entId);
  if (state == nullptr) return;
  entityType = state->entityType;
  for (aiSenseState_t* sense : senses) {
    sense->integrator.value = 0.0F;
    sense->integrator.lastUpdateMS = initTime;
    sense->integrator.triggered = false;
    sense->InitSpring(state, owner);
  }
  active = true;
}

// DIA: idAIEntityState::UpdateSprings
void idAIEntityState::UpdateSprings(const trackingParms_t* parms) {
  for (aiSenseState_t* sense : senses) sense->UpdateSpring(parms);
}

// DIA: idAIEntityState::WithinFOV
bool idAIEntityState::WithinFOV(const idAILogic* owner,
                                const aiLatchedState_t* target,
                                float* fovWeight) {
  if (fovWeight == nullptr) return false;
  *fovWeight = 0.0F;
  if (owner == nullptr || target == nullptr) return false;
  const aiLatchedState_t* observer = owner->GetState();
  if (observer == nullptr) return false;
  idVec3 direction = target->physical.eyePos - observer->physical.eyePos;
  if (direction.Normalize() == 0.0F) {
    *fovWeight = 1.0F;
    return true;
  }
  if (observer->horzFOV == 0) {
    *fovWeight = 1.0F;
    return true;
  }
  const float horizontal = idAIVisibility::ByteToFOVDot(observer->horzFOV);
  *fovWeight = observer->vertFOV != 0
      ? idAIVisibility::CheckFOV(direction, observer->physical.viewfwd,
            horizontal, idAIVisibility::ByteToFOVDot(observer->vertFOV))
      : idAIVisibility::CheckFOV(direction, observer->physical.viewfwd,
                                 horizontal);
  return *fovWeight > 0.0F;
}

// DIA: idAIEntityState::AICanSee_Submit
void idAIEntityState::AICanSee_Submit(const idAILogic* owner,
                                      const aiLatchedState_t* target,
                                      bool seeThroughBreakables,
                                      aiDefVisInfo_t* info) {
  if (info == nullptr) return;
  WithinFOV(owner, target, &info->fovWeight);
  info->eyeQuery.index = 1;
  info->boundsQuery.index = target != nullptr && target->entityType == 1 ? 2 : 0;
  info->queryReady = target != nullptr;
  info->eyeVisible = target != nullptr;
  info->boundsVisible = target != nullptr || seeThroughBreakables;
  const aiLatchedState_t* observer = owner != nullptr ? owner->GetState() : nullptr;
  info->delta = observer != nullptr && target != nullptr
                    ? target->physical.origin - observer->physical.origin
                    : idVec3{};
}

// DIA: idAIEntityState::IsDead
bool idAIEntityState::IsDead() const { return entity == nullptr || dead; }

// DIA: idAIEntityState::IsDying
bool idAIEntityState::IsDying() const { return entity != nullptr && dying; }

// DIA: idAIEntityState::DecreaseAwareness
void idAIEntityState::DecreaseAwareness(const idAILogic*, int newAwareness) {
  const int clamped = std::clamp(newAwareness, 0, 3);
  if (clamped >= curAwareness) return;
  curAwareness = clamped;
  awarenessChanged = true;
  if (curAwareness < 3 && firstSpot > 0) {
    spotDuration += std::max(0, currentTime - firstSpot);
    firstSpot = -1;
    spotLostDuration = currentTime;
  }
}

// DIA: `idAIEntityState::DecreaseAwareness'::`1'::dtor$0
// DIA: `idAIEntityState::DecreaseAwareness'::`1'::dtor$1

// DIA: idAIEntityState::SetSpot
void idAIEntityState::SetSpot(int curTime, const idVec3& aimDirection) {
  if (curAwareness != 3 || highestAwareness != 3) return;
  if (firstSpot > 0) spotDuration += curTime - firstSpot;
  else spotDuration = 0;
  aimDir = aimDirection;
  firstSpot = curTime;
}

// DIA: aiSenseState_t::SetUnconfirmedPhysical
void aiSenseState_t::SetUnconfirmedPhysical(const aiPhysicalState_t* physical) {
  if (physical != nullptr) unConfirmedPhysical = *physical;
}

// DIA: aiSenseState_t::SetConfirmedPhysical
void aiSenseState_t::SetConfirmedPhysical(const aiPhysicalState_t* physical) {
  if (physical != nullptr) confirmedPhysical = *physical;
}

// DIA: aiLatchedState_t::aiLatchedState_t
aiLatchedState_t::aiLatchedState_t() = default;

// DIA: aiLatchedEvent_t::aiLatchedEvent_t
aiLatchedEvent_t::aiLatchedEvent_t() = default;

// DIA: idAILogic::idAILogic
idAILogic::idAILogic(const idSpawnId& newSpawnId) : spawnId(newSpawnId) {}

// DIA: idAISenseState_Sight::UpdateSpring
void idAISenseState_Sight::UpdateSpring(const trackingParms_t* parms) {
  if (!springInitialized || lastConfirmedStimTime < 0 ||
      ai_useOriginSprings.GetInteger() == 0) return;
  const trackingParms_t defaults;
  const trackingParms_t& values = parms != nullptr ? *parms : defaults;
  confirmedOriginSpring.k = values.springK;
  confirmedOriginSpring.m = std::max(0.001F, values.springMass);
  confirmedOriginSpring.c = values.springDamping;
  confirmedOriginSpring.maxSpeed = values.springMaxSpeed;
  confirmedOriginSpring.maxLength = values.springMaxLength;
  const idVec3 displacement = confirmedOriginSpring.p0 - confirmedOriginSpring.p1;
  idVec3 acceleration = displacement *
      (confirmedOriginSpring.k / confirmedOriginSpring.m);
  acceleration -= confirmedOriginSpring.velocity *
      (confirmedOriginSpring.c / confirmedOriginSpring.m);
  confirmedOriginSpring.velocity += acceleration * values.deltaSeconds;
  if (confirmedOriginSpring.maxSpeed > 0.0F &&
      confirmedOriginSpring.velocity.Length() > confirmedOriginSpring.maxSpeed) {
    confirmedOriginSpring.velocity.Normalize();
    confirmedOriginSpring.velocity *= confirmedOriginSpring.maxSpeed;
  }
  confirmedOriginSpring.p1 += confirmedOriginSpring.velocity * values.deltaSeconds;
  if (confirmedOriginSpring.maxLength > 0.0F &&
      Distance(confirmedOriginSpring.p1, confirmedOriginSpring.p0) >
          confirmedOriginSpring.maxLength) {
    idVec3 offset = confirmedOriginSpring.p1 - confirmedOriginSpring.p0;
    offset.Normalize();
    confirmedOriginSpring.p1 = confirmedOriginSpring.p0 +
        offset * confirmedOriginSpring.maxLength;
  }
}

// DIA: idAIEntityState::Refresh_Submit
void idAIEntityState::Refresh_Submit(idAIGameState* gameState,
                                     const idAIWorldState*,
                                     const idAILogic* owner,
                                     aiDefVisInfo_t* info) {
  const aiLatchedState_t* state = gameState != nullptr
      ? gameState->GetState(&spawnId) : nullptr;
  if (state == nullptr || state->spawnId.value != spawnId.value) {
    active = false;
    return;
  }
  const bool recentlySeen = currentTime - sightSense.lastConfirmedStimTime < 1000;
  AICanSee_Submit(owner, state, recentlySeen, info);
}

// DIA: idAIEntityState::GetAimPoint
void idAIEntityState::GetAimPoint(aiSense_t senseIndex,
                                  aimPoint_t requestedPoint,
                                  idVec3* point) const {
  if (point == nullptr || senseIndex < AISENSE_HEARING ||
      senseIndex > AISENSE_SIGHT) return;
  const aiSenseState_t* sense = senses[static_cast<std::size_t>(senseIndex)];
  const aiPhysicalState_t* physical = sense->GetConfirmedPhysicalSafe();
  idVec3 origin = sense->HasSpring() && sense->lastConfirmedStimTime >= 0
                      ? sense->GetSpring()->p1 : physical->origin;
  aimPoint_t selected = requestedPoint;
  if (selected == AIMPOINT_BEST)
    selected = lastVisible == VISIBLE_IN_COVER ? AIMPOINT_HEAD : AIMPOINT_TORSO;
  const idVec3 center = origin + (physical->bounds[0] + physical->bounds[1]) * 0.5F;
  switch (selected) {
    case AIMPOINT_ORIGIN: *point = origin; break;
    case AIMPOINT_HEAD: *point = origin + (physical->eyePos - physical->origin); break;
    case AIMPOINT_TORSO:
      *point = (center + origin + (physical->eyePos - physical->origin)) * 0.5F;
      break;
    case AIMPOINT_CENTER: *point = center; break;
    case AIMPOINT_LEGS:
      *point = origin + idVec3{0.0F, 0.0F,
          (physical->bounds[1].z - physical->bounds[0].z) * 0.25F};
      break;
    case AIMPOINT_FEET:
      *point = physical->origin + idVec3{0.0F, 0.0F,
          (physical->bounds[1].z - physical->bounds[0].z) * 0.1F};
      break;
    case AIMPOINT_LOOKAHEAD: {
      *point = origin + (physical->eyePos - physical->origin);
      idVec3 direction = physical->velocity;
      if (direction.Normalize() > 0.0F) *point += direction * 64.0F;
      break;
    }
    default: break;
  }
}

// DIA: idAIEntityState::AICanSee_Resolve
visibleType_t idAIEntityState::AICanSee_Resolve(const idAILogic* owner,
                                                aiDefVisInfo_t* info,
                                                const aiLatchedState_t* target) {
  if (owner == nullptr || info == nullptr || target == nullptr ||
      !info->queryReady) return VISIBLE_INVALID;
  info->eyeQuery.index = 0;
  if (!info->eyeVisible)
    return info->fovWeight > 0.0F ? VISIBLE_NOT_VISIBLE
                                  : VISIBLE_OUTSIDE_FOV;
  if (info->fovWeight <= 0.0F) return VISIBLE_OUTSIDE_FOV_CLEAR;
  if (!info->boundsVisible) return VISIBLE_IN_COVER;
  info->boundsQuery.index = 0;
  return VISIBLE_VISIBLE;
}

// DIA: idAIEntityState::IsVisible
bool idAIEntityState::IsVisible() const {
  return lastVisible > VISIBLE_NOT_VISIBLE &&
         sightSense.lastUnconfirmedStimTime >= 0 &&
         currentTime - sightSense.lastConfirmedStimTime < 1000;
}

// DIA: idAIEntityState::TestSurprise
void idAIEntityState::TestSurprise(const idAIGameState* gameState,
                                   const idAILogic*,
                                   const aiLatchedState_t* state) {
  if (gameState == nullptr || state == nullptr || lastSense == AISENSE_NONE) return;
  const aiSenseState_t* sense = senses[static_cast<std::size_t>(lastSense)];
  const float displacement = Distance(
      sense->GetConfirmedPhysicalSafe()->origin, state->physical.origin);
  awarenessChanged = gameState->gameTime - sense->lastConfirmedStimTime > 3000 &&
                     displacement > 512.0F;
}

// DIA: idAIEntityState::UpdateSense
bool idAIEntityState::UpdateSense(idAIGameState* gameState,
                                  const idAILogic* owner,
                                  const idSpawnId& entId,
                                  const idSpawnId& sourceEntId,
                                  int, aiSenseState_t* sense,
                                  const aiLatchedState_t* state) {
  if (gameState == nullptr || owner == nullptr || sense == nullptr ||
      owner->pausePerception || state == nullptr || entId.value != spawnId.value) {
    if (state == nullptr) active = false;
    return false;
  }
  sense->SetUnconfirmedPhysical(&state->physical);
  bool confirmed = false;
  if (sense->integrator.value >= 1.0F) {
    if (sense->lastConfirmedStimTime >= 0 &&
        state->spawnId.value == owner->curEnemyId.value &&
        (sense->senseFlags & 1U) != 0)
      TestSurprise(gameState, owner, state);
    sense->SetConfirmedPhysical(&state->physical);
    sense->lastConfirmedStimTime = gameState->gameTime;
    sense->Update(state);
    confirmed = true;
  }
  const aiLatchedState_t* observer = owner->GetState();
  if (observer != nullptr) sense->aiOrigin = observer->physical.origin;
  sense->lastUnconfirmedStimTime = gameState->gameTime;
  sense->senseFlags &= static_cast<std::uint8_t>(~4U);
  if (sourceEntId.value == owner->spawnId.value) sense->senseFlags |= 1U;
  return confirmed;
}

// DIA: idAIEntityState::SetSenseState
void idAIEntityState::SetSenseState(idAIGameState* gameState,
                                    const idAILogic* owner,
                                    aiSense_t senseIndex,
                                    const aiPhysicalState_t* physical,
                                    aiStimulus_t newStimulusType,
                                    const idSpawnId& sourceId) {
  if (gameState == nullptr || owner == nullptr || physical == nullptr ||
      senseIndex < AISENSE_HEARING || senseIndex > AISENSE_SIGHT) return;
  aiSenseState_t* sense = senses[static_cast<std::size_t>(senseIndex)];
  sense->SetUnconfirmedPhysical(physical);
  sense->SetConfirmedPhysical(physical);
  sense->lastUnconfirmedStimTime = gameState->gameTime;
  sense->lastConfirmedStimTime = gameState->gameTime;
  const aiLatchedState_t* observer = owner->GetState();
  if (observer != nullptr) sense->aiOrigin = observer->physical.origin;
  if (sourceId.value == owner->spawnId.value) sense->senseFlags |= 1U;
  sense->stimulusType = newStimulusType;
  lastSense = senseIndex;
  currentTime = gameState->gameTime;
}

// DIA: idAILogic::idAILogic
idAILogic::idAILogic(idAI2*) {}

// DIA: idAIEntityState::idAIEntityState
idAIEntityState::idAIEntityState() = default;

// DIA: idAIEntityState::`scalar deleting destructor'

// DIA: idAIEntityState::UpdateSeen
void idAIEntityState::UpdateSeen(idAIGameState* gameState,
                                 const idAILogic* owner) {
  if (gameState == nullptr) return;
  const int lastTime = sightSense.lastUnconfirmedStimTime;
  const idVec3 lastOrigin = sightSense.unConfirmedPhysical.origin;
  const idVec3 lastAIOrigin = sightSense.aiOrigin;
  const aiLatchedState_t* state = gameState->GetState(&spawnId);
  const idSpawnId source = owner != nullptr ? owner->spawnId : idSpawnId{};
  UpdateSense(gameState, owner, spawnId, source, -1, &sightSense, state);
  sightSense.lastUnconfirmedStimTime = lastTime;
  sightSense.unConfirmedPhysical.origin = lastOrigin;
  sightSense.aiOrigin = lastAIOrigin;
}

// DIA: idAIEntityState::IncreaseAwareness
void idAIEntityState::IncreaseAwareness(const idAILogic*, int newAwareness) {
  if (dead) return;
  const int clamped = std::clamp(newAwareness, 0, 3);
  if (clamped <= curAwareness) return;
  curAwareness = clamped;
  highestAwareness = std::max(highestAwareness, clamped);
  awarenessChanged = true;
}

// DIA: `idAIEntityState::IncreaseAwareness'::`1'::dtor$0
// DIA: `idAIEntityState::IncreaseAwareness'::`1'::dtor$1
// DIA: `idAIEntityState::IncreaseAwareness'::`1'::dtor$4
// DIA: `idAIEntityState::IncreaseAwareness'::`1'::dtor$5
// DIA: `idAIEntityState::IncreaseAwareness'::`1'::dtor$8
// DIA: `idAIEntityState::IncreaseAwareness'::`1'::dtor$9
// DIA: `idAIEntityState::IncreaseAwareness'::`1'::dtor$12
// DIA: `idAIEntityState::IncreaseAwareness'::`1'::dtor$13
// DIA: `idAIEntityState::IncreaseAwareness'::`1'::dtor$16
// DIA: `idAIEntityState::IncreaseAwareness'::`1'::dtor$17

// DIA: idLeakyIntegrator<0,1>::AddStimulus
template <int Minimum, int Maximum>
bool idLeakyIntegrator<Minimum, Maximum>::AddStimulus(int curTimeMS,
                                                       float stimulus) {
  Update(curTimeMS);
  value = std::clamp(value + stimulus, static_cast<float>(Minimum),
                     static_cast<float>(Maximum));
  if (!triggered) triggered = value >= static_cast<float>(Maximum);
  return triggered;
}
template bool idLeakyIntegrator<0, 1>::AddStimulus(int, float);

// DIA: idAIEntityState::AddStimulus
void idAIEntityState::AddStimulus(idAIGameState* gameState,
                                  const idAILogic* owner,
                                  aiStimulus_t newStimulusType, float amount,
                                  const idSpawnId& sourceEntityId,
                                  int sourceEventId,
                                  const aiLatchedState_t* suppliedState) {
  if (!active || gameState == nullptr || owner == nullptr) return;
  const aiLatchedState_t* state = suppliedState != nullptr
      ? suppliedState : gameState->GetState(&spawnId);
  if (state == nullptr || state->spawnId.value != spawnId.value) return;
  const bool sight = newStimulusType == AISTIM_SIGHT ||
                     newStimulusType == AISTIM_DIRECT ||
                     newStimulusType == AISTIM_DIRECT_VISIBLE;
  aiSenseState_t* sense = sight ? static_cast<aiSenseState_t*>(&sightSense)
                                : static_cast<aiSenseState_t*>(&hearingSense);
  sense->stimulusType = newStimulusType;
  const bool trigger = newStimulusType == AISTIM_DAMAGE ||
                       newStimulusType == AISTIM_DIRECT ||
                       newStimulusType == AISTIM_DIRECT_VISIBLE;
  bool reachedThreshold = false;
  if (trigger) {
    sense->integrator.value = 1.0F;
    sense->integrator.triggered = true;
    reachedThreshold = true;
  } else {
    reachedThreshold = sense->integrator.AddStimulus(gameState->gameTime, amount);
  }
  if (reachedThreshold) IncreaseAwareness(owner, sight ? 3 : 2);
  if (sight) lastVisualStimulus = amount;
  if (newStimulusType == AISTIM_SHARED) {
    sense->senseFlags &= static_cast<std::uint8_t>(~1U);
    sense->senseFlags |= 2U;
  }
  if (UpdateSense(gameState, owner, spawnId, sourceEntityId, sourceEventId,
                  sense, state) || lastSense == AISENSE_NONE)
    lastSense = sight ? AISENSE_SIGHT : AISENSE_HEARING;
  if (reachedThreshold) sense->integrator.triggered = false;
}

// DIA: idAIEntityState::Refresh_Resolve
void idAIEntityState::Refresh_Resolve(idAIGameState* gameState,
                                      idAIWorldState*,
                                      const idAILogic* owner,
                                      aiDefVisInfo_t* info) {
  if (gameState == nullptr || owner == nullptr || info == nullptr) return;
  currentTime = gameState->gameTime;
  const aiLatchedState_t* state = gameState->GetState(&spawnId);
  if (state == nullptr || state->spawnId.value != spawnId.value) {
    active = false;
    return;
  }
  visibleType_t classification;
  if (info->fovWeight <= 0.0F && !info->closeEnough)
    classification = VISIBLE_OUTSIDE_FOV;
  else if (info->closeEnough) {
    classification = VISIBLE_VISIBLE;
    info->fovWeight = std::clamp(1.0F - info->delta.Length() / 128.0F,
                                 0.0F, 1.0F);
  } else {
    classification = AICanSee_Resolve(owner, info, state);
  }
  if (classification == VISIBLE_INVALID) return;
  if (classification > VISIBLE_NOT_VISIBLE) {
    const float stimulus = classification == VISIBLE_VISIBLE
                               ? std::max(0.25F, info->fovWeight)
                               : std::max(0.1F, info->fovWeight * 0.5F);
    AddStimulus(gameState, owner, AISTIM_SIGHT, stimulus,
                owner->spawnId, -1, state);
  } else {
    sightSense.integrator.value = std::min(0.5F, sightSense.integrator.value);
  }
  const bool wasVisible = lastVisible > VISIBLE_NOT_VISIBLE;
  const bool nowVisible = classification > VISIBLE_NOT_VISIBLE;
  if (classification != lastVisible) {
    lastVisible = classification;
    if (wasVisible != nowVisible) visChangeTime = gameState->gameTime;
  }
  if (lastSense != AISENSE_NONE) {
    const int lastStimulus = senses[static_cast<std::size_t>(lastSense)]
                                 ->GetConfirmedStimTimeSafe();
    const float elapsed = (gameState->gameTime - lastStimulus) * 0.001F;
    if (curAwareness == 3 && owner->combatTimeoutInterval >= 0.0F &&
        elapsed >= owner->combatTimeoutInterval)
      DecreaseAwareness(owner, 2);
    else if (curAwareness == 2 && owner->searchTimeoutInterval >= 0.0F &&
             elapsed >= owner->combatTimeoutInterval +
                            owner->searchTimeoutInterval)
      DecreaseAwareness(owner, 1);
    else if (curAwareness == 1 && owner->searchTimeoutInterval >= 0.0F &&
             elapsed >= owner->combatTimeoutInterval +
                            owner->searchTimeoutInterval)
      DecreaseAwareness(owner, 0);
  }
}

// DIA: `dynamic initializer for 'ai_useOriginSprings''
// DIA: `dynamic atexit destructor for 'ai_useOriginSprings''
idCVar ai_useOriginSprings("ai_useOriginSprings", "1", 1,
                           "smooth confirmed target origins");
// DIA: `dynamic initializer for 'ai_originSpringExtrapolate''
// DIA: `dynamic atexit destructor for 'ai_originSpringExtrapolate''
idCVar ai_originSpringExtrapolate("ai_originSpringExtrapolate", "0.1", 1,
                                  "target velocity extrapolation in seconds");
// DIA: `dynamic initializer for 'ai_debugOriginSprings''
// DIA: `dynamic atexit destructor for 'ai_debugOriginSprings''
idCVar ai_debugOriginSprings("ai_debugOriginSprings", "0", 1,
                             "draw AI target origin springs");
// DIA: `dynamic initializer for 'ai_debugFOV''
// DIA: `dynamic atexit destructor for 'ai_debugFOV''
idCVar ai_debugFOV("ai_debugFOV", "0", 1, "draw AI field of view tests");
// DIA: `dynamic initializer for 'debugDeferredWorldRefresh''
// DIA: `dynamic atexit destructor for 'debugDeferredWorldRefresh''
idCVar debugDeferredWorldRefresh("debugDeferredWorldRefresh", "0", 1,
                                 "draw deferred world refresh queries");
