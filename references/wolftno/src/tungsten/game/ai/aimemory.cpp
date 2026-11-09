#include "tungsten/game/ai/aimemory.h"

#include "tungsten/game/ai/aidefs.h"

#include <algorithm>

void idVolume_ScenePoints::GetScenePoints(
    idList<idEntityPtr<idScenePoint>, 5>* output, scenePointType_t type,
    idVolume_Restriction* restriction,
    idScenePoint_UserGroup* userGroup) const {
  (void)userGroup;
  if (output == nullptr) return;
  for (int index = 0; index < scenePoints.Num(); ++index) {
    idScenePoint* point = scenePoints[index];
    if (point == nullptr || !point->IsEnabled() ||
        point->GetScenePointType() != type) {
      continue;
    }
    if (restriction != nullptr && restriction->IsActive() &&
        point->GetPhysics() != nullptr &&
        !restriction->ContainsPoint(point->GetPhysics()->GetOrigin())) {
      continue;
    }
    bool duplicate = false;
    for (int current = 0; current < output->Num(); ++current) {
      if (static_cast<idScenePoint*>((*output)[current]) == point) {
        duplicate = true;
        break;
      }
    }
    if (!duplicate) output->Append(idEntityPtr<idScenePoint>(point));
  }
}

// DIA: idAIMemory::DebugGetEnemyTrailPos
const idAIPos* idAIMemory::DebugGetEnemyTrailPos(int index) const {
  const int wrapped = ((index + enemyTrailFinger) % 5 + 5) % 5;
  return &enemyTrailPositions[wrapped];
}

// DIA: idAIMemory::CheckSoundAlarm
bool idAIMemory::CheckSoundAlarm(int current) {
  if (timeToSoundAlarm <= 0 || current <= timeToSoundAlarm) return false;
  timeToSoundAlarm = -1;
  return true;
}

// DIA: idAIMemory::SetAIVarExpression
void idAIMemory::SetAIVarExpression(const char* name,
                                    aiVarOp_t operation,
                                    const char* value) {
  varName = name;
  varOp = operation;
  varValue = value;
}

// DIA: idAIMemory::GetScriptedAggro
float idAIMemory::GetScriptedAggro(int current) const {
  return scriptedAggroEndTime < 0 || current <= scriptedAggroEndTime
             ? scriptedAggression
             : 0.0F;
}

// DIA: idAIMemory::SetNextMeleeTime
void idAIMemory::SetNextMeleeTime(aiMelee_t meleeType, int time) {
  if (meleeType == AIMELEE_MEDIUM) nextMediumMeleeTime = time;
  else if (meleeType == AIMELEE_LONG) nextLongMeleeTime = time;
  else nextCloseMeleeTime = time;
}

// DIA: idAIMemory::GetNextMeleeTime
int idAIMemory::GetNextMeleeTime(aiMelee_t meleeType) const {
  if (meleeType == AIMELEE_MEDIUM) return nextMediumMeleeTime;
  if (meleeType == AIMELEE_LONG) return nextLongMeleeTime;
  return nextCloseMeleeTime;
}

// DIA: idAIMemory::ClearLastHeldScenePoint
void idAIMemory::ClearLastHeldScenePoint() { scenePointHistory[0] = nullptr; }

// DIA: idAIMemory::GetLastHeldScenePoint
idScenePoint* idAIMemory::GetLastHeldScenePoint() const {
  return scenePointHistory[0];
}

// DIA: idAIMemory::ActivateCurScenePoint
void idAIMemory::ActivateCurScenePoint(idAI2* user, int time) {
  if (curScenePoint == nullptr) return;
  curScenePoint->ActivateScenePoint(user, time);
  curScenePointActivated = true;
}

// DIA: idAIMemory::SetCurLookScenePoint
void idAIMemory::SetCurLookScenePoint(idAI2* user, int time,
                                      idScenePoint* scenePoint) {
  if (curLookScenePoint != nullptr && curLookScenePoint != scenePoint) {
    curLookScenePoint->ReleaseScenePoint(user, time);
  }
  curLookScenePoint = scenePoint;
  curLookScenePointActivated = false;
}

// DIA: idAIMemory::ActivateCurLookScenePoint
void idAIMemory::ActivateCurLookScenePoint(idAI2* user, int time) {
  if (curLookScenePoint == nullptr) return;
  curLookScenePoint->ActivateScenePoint(user, time);
  curLookScenePointActivated = true;
}

// DIA: idAIMemory::SetTakeScenePointTimeStamp
void idAIMemory::SetTakeScenePointTimeStamp(int time) {
  if (curScenePointTakeTime < 0) curScenePointTakeTime = time;
}

// DIA: idAIMemory::SetActivateScenePointTimeStamp
void idAIMemory::SetActivateScenePointTimeStamp(int time) {
  if (curScenePointActivateTime < 0) curScenePointActivateTime = time;
}

// DIA: idAIMemory::ClearScenePointTimeStamps
void idAIMemory::ClearScenePointTimeStamps() {
  curScenePointTakeTime = -1;
  curScenePointActivateTime = -1;
}

// DIA: idAIMemory::FirstThink
void idAIMemory::FirstThink(const idDeclAIBehavior* behavior) {
  spawnTime = currentTime;
  int interval = 0;
  if (behavior != nullptr) {
    interval = (behavior->minAimDodgeInterval +
                behavior->maxAimDodgeInterval) /
               2;
  }
  nextDodgeTime = static_cast<int>(GetRandomInterval(
      static_cast<unsigned int>(std::max(0, currentTime)),
      std::max(currentTime, currentTime + interval)));
  ClearAllTrailPositions(currentTime);
}

// DIA: idAIMemory::ClearOldestTrailPos
void idAIMemory::ClearOldestTrailPos() {
  for (int offset = 0; offset < 5; ++offset) {
    idAIPos& position = enemyTrailPositions[(enemyTrailFinger + offset) % 5];
    if (position.valid) {
      position.valid = false;
      return;
    }
  }
}

// DIA: idAIMemory::ClearAllTrailPositions
void idAIMemory::ClearAllTrailPositions(int time) {
  for (idAIPos& position : enemyTrailPositions) position.valid = false;
  nextEnemyTrailTime = time + enemyTrailUpdatePeriod;
  enemyTrailFinger = 0;
}

// DIA: idAIMemory::CheckVoiceVolumes
void idAIMemory::CheckVoiceVolumes(idAI2* ai, int time,
                                   const idVec3* suspectedPosition) {
  if (time < nextVoiceVolumeCheckTime) return;
  for (int index = voiceVolumes.Num() - 1; index >= 0; --index) {
    idVolume_LocationVoice* volume = voiceVolumes[index];
    if (volume != nullptr && volume->CheckActivation(ai, time,
                                                     suspectedPosition)) {
      if (volume->nextActivationTime >= 0 && volume->reuseDelay < 0.0F) {
        voiceVolumes.RemoveIndex(index);
      }
      break;
    }
  }
  nextVoiceVolumeCheckTime = time + 500;
}

// DIA: idAIMemory::ReportInjured
void idAIMemory::ReportInjured() { injuredTime = currentTime; }

// DIA: idAIMemory::TriggerPulled
void idAIMemory::TriggerPulled() {
  numShotsThisPullBlocked = 0;
  numShotsThisPullBlockedConsecutive = 0;
}

// DIA: idAIMemory::HasOverrideScriptScenePointOfType
bool idAIMemory::HasOverrideScriptScenePointOfType(
    scenePointType_t type) const {
  return overrideScenePoint != nullptr && overrideScenePoint->IsEnabled() &&
         overrideScenePoint->GetScenePointType() == type &&
         (overrideScenePoint->GetScriptFlags() & 1U) != 0;
}

// DIA: idAIMemory::HasOverrideScriptScenePointNotOfType
bool idAIMemory::HasOverrideScriptScenePointNotOfType(
    scenePointType_t type) const {
  return overrideScenePoint != nullptr && overrideScenePoint->IsEnabled() &&
         overrideScenePoint->GetScenePointType() != type &&
         (overrideScenePoint->GetScriptFlags() & 1U) != 0;
}

// DIA: idAIMemory::UpdateScenePointHistory
void idAIMemory::UpdateScenePointHistory(idScenePoint* scenePoint) {
  if (scenePoint == nullptr || scenePoint->GetScenePointType() == LOOK ||
      scenePointHistory[0] == scenePoint) {
    return;
  }
  for (int index = 4; index > 0; --index) {
    scenePointHistory[index] = scenePointHistory[index - 1];
  }
  scenePointHistory[0] = scenePoint;
}

// DIA: idAIMemory::GetScenePointHistoryScore
float idAIMemory::GetScenePointHistoryScore(idScenePoint* scenePoint) const {
  if (scenePoint == nullptr || scenePoint->GetScenePointType() == LOOK) {
    return 0.0F;
  }
  for (int index = 0; index < 4; ++index) {
    if (scenePointHistory[index] == scenePoint) return index * 0.2F;
  }
  return 1.0F;
}

// DIA: idAIMemory::UpdateLookScenePointHistory
void idAIMemory::UpdateLookScenePointHistory(idScenePoint* scenePoint) {
  if (scenePoint == nullptr || scenePoint->GetScenePointType() != LOOK ||
      lookScenePointHistory[0] == scenePoint) {
    return;
  }
  for (int index = 4; index > 0; --index) {
    lookScenePointHistory[index] = lookScenePointHistory[index - 1];
  }
  lookScenePointHistory[0] = scenePoint;
}

// DIA: idAIMemory::IsLastHeldLookScenePoint
bool idAIMemory::IsLastHeldLookScenePoint(idScenePoint* scenePoint) const {
  return scenePoint != nullptr && scenePoint->GetScenePointType() == LOOK &&
         lookScenePointHistory[0] == scenePoint;
}

// DIA: idAIMemory::GetLookScenePointHistoryScore
float idAIMemory::GetLookScenePointHistoryScore(
    idScenePoint* scenePoint) const {
  if (scenePoint == nullptr || scenePoint->GetScenePointType() != LOOK) {
    return 0.0F;
  }
  for (int index = 0; index < 4; ++index) {
    if (lookScenePointHistory[index] == scenePoint) return index * 0.2F;
  }
  return 1.0F;
}

// DIA: idAIMemory::SetOverrideScenePoint
void idAIMemory::SetOverrideScenePoint(idScenePoint* scenePoint) {
  if (scenePoint != nullptr && scenePoint->GetScenePointType() == SPAWN) return;
  if (overrideScenePoint != nullptr &&
      (overrideScenePoint->GetScriptFlags() & 1U) != 0 &&
      scenePoint != nullptr && (scenePoint->GetScriptFlags() & 1U) == 0) {
    return;
  }
  if (overrideScenePoint != nullptr && overrideScenePoint != scenePoint &&
      (overrideScenePoint->GetScriptFlags() & 0x400U) != 0) {
    keptOverrideScenePoint = overrideScenePoint;
  }
  overrideScenePoint = scenePoint;
}

// DIA: idAIMemory::AddEnemyTrailPos
void idAIMemory::AddEnemyTrailPos(int time, const idVec3& enemyPosition) {
  if (enemyTrailUpdatePeriod <= 0 || time < nextEnemyTrailTime) return;
  const int previous = (enemyTrailFinger + 4) % 5;
  if (!enemyTrailPositions[previous].valid ||
      (enemyPosition - enemyTrailPositions[previous].pos).LengthSqr() >
          48.0F * 48.0F) {
    enemyTrailPositions[enemyTrailFinger] = {enemyPosition, true};
    enemyTrailFinger = (enemyTrailFinger + 1) % 5;
    nextEnemyTrailTime = time + enemyTrailUpdatePeriod;
  }
}

// DIA: idAIMemory::GetOldestTrailPos
bool idAIMemory::GetOldestTrailPos(idAIPos* trail) const {
  if (trail == nullptr) return false;
  for (int offset = 0; offset < 5; ++offset) {
    const idAIPos& position =
        enemyTrailPositions[(enemyTrailFinger + offset) % 5];
    if (position.valid) {
      *trail = position;
      return true;
    }
  }
  return false;
}

// DIA: idEntityPtr<idScenePoint>::operator idScenePoint * __ptr64
template <>
idEntityPtr<idScenePoint>::operator idScenePoint*() const {
  return entity;
}

// DIA: idAIMemory::idScenePointList::Clear
void idAIMemory::idScenePointList::Clear() {
  scenePointList.ClearFree();
  hasSearchedForScenePoints = false;
}

// DIA: idAIMemory::~idAIMemory
idAIMemory::~idAIMemory() = default;

// DIA: `idAIMemory::~idAIMemory'::`1'::dtor$0
// DIA: `idAIMemory::~idAIMemory'::`1'::dtor$1
// DIA: `idAIMemory::~idAIMemory'::`1'::dtor$2
// DIA: `idAIMemory::~idAIMemory'::`1'::dtor$3
// DIA: `idAIMemory::~idAIMemory'::`1'::dtor$4
// DIA: `idAIMemory::~idAIMemory'::`1'::dtor$5
// DIA: `idAIMemory::~idAIMemory'::`1'::dtor$6
// DIA: `idAIMemory::~idAIMemory'::`1'::dtor$7
// DIA: `idAIMemory::~idAIMemory'::`1'::dtor$8
// DIA: `idAIMemory::~idAIMemory'::`1'::dtor$9
// DIA: `idAIMemory::~idAIMemory'::`1'::dtor$10

// DIA: idAIMemory::SetLastDamage
void idAIMemory::SetLastDamage(idEntity* attacker, idEntity* inflictor) {
  lastDamageTime = currentTime;
  lastAttacker = attacker;
  lastInflictor = inflictor;
}

// DIA: idAIMemory::SetCurrentPath
void idAIMemory::SetCurrentPath(const idInfoPath* path) {
  currentPath = path;
  nextPath = path != nullptr ? path->GetNextPathPoint() : nullptr;
}

// DIA: idAIMemory::ClearScenePointLists
void idAIMemory::ClearScenePointLists() {
  combatScenePoints.Clear();
  searchScenePoints.Clear();
  roamScenePoints.Clear();
  lookScenePoints.Clear();
}

// DIA: idAIMemory::SetRestrictionVolume
void idAIMemory::SetRestrictionVolume(idVolume_Restriction* volume) {
  if (restrictionVolume == volume) return;
  restrictionVolume = volume;
  restrictionVolumeActive = volume != nullptr && volume->IsActive();
  restrictionPos.valid = false;
  ClearScenePointLists();
}

// DIA: idAIMemory::HasRestrictionVolume
bool idAIMemory::HasRestrictionVolume() {
  if (restrictionVolume == nullptr) return false;
  const bool active = restrictionVolume->IsActive();
  if (active != restrictionVolumeActive) {
    restrictionVolumeActive = active;
    ClearScenePointLists();
  }
  return restrictionVolumeActive;
}

// DIA: idAIMemory::SetRestrictionPos
void idAIMemory::SetRestrictionPos(const idVec3& position) {
  if (HasRestrictionVolume()) restrictionPos = {position, true};
  else restrictionPos.valid = false;
}

// DIA: idAIMemory::idAIMemory
idAIMemory::idAIMemory() = default;

// DIA: `idAIMemory::idAIMemory'::`1'::dtor$0
// DIA: `idAIMemory::idAIMemory'::`1'::dtor$1
// DIA: `idAIMemory::idAIMemory'::`1'::dtor$2
// DIA: `idAIMemory::idAIMemory'::`1'::dtor$3
// DIA: `idAIMemory::idAIMemory'::`1'::dtor$4
// DIA: `idAIMemory::idAIMemory'::`1'::dtor$5
// DIA: `idAIMemory::idAIMemory'::`1'::dtor$6
// DIA: `idAIMemory::idAIMemory'::`1'::dtor$7
// DIA: `idAIMemory::idAIMemory'::`1'::dtor$8
// DIA: `idAIMemory::idAIMemory'::`1'::dtor$9
// DIA: `idAIMemory::idAIMemory'::`1'::dtor$10
// DIA: `idAIMemory::idAIMemory'::`1'::dtor$11

// DIA: idAIMemory::RemoveScenePointVolume
void idAIMemory::RemoveScenePointVolume(idVolume_ScenePoints* volume) {
  for (int index = 0; index < scenePointVolumes.Num(); ++index) {
    if (scenePointVolumes[index] == volume) {
      scenePointVolumes.RemoveIndex(index);
      break;
    }
  }
  ClearScenePointLists();
}

// DIA: idAIMemory::SetCurScenePoint
void idAIMemory::SetCurScenePoint(idAI2* user, int time,
                                  idScenePoint* scenePoint) {
  if (curScenePoint != nullptr && curScenePoint != scenePoint) {
    curScenePoint->ReleaseScenePoint(user, time);
  }
  curScenePoint = scenePoint;
  curScenePointActivated = false;
}

// DIA: idAIMemory::SetScenePointUserGroup
void idAIMemory::SetScenePointUserGroup(idScenePoint_UserGroup* group) {
  scenePointUserGroup = group;
  ClearScenePointLists();
}

// DIA: idList<idEntityPtr<idScenePoint>,5>::operator=
template idList<idEntityPtr<idScenePoint>, 5>&
idList<idEntityPtr<idScenePoint>, 5>::operator=(
    const idList<idEntityPtr<idScenePoint>, 5>&);

// DIA: idAIMemory::CheckIfDirtyScenePoint
bool idAIMemory::CheckIfDirtyScenePoint(idScenePoint* dirtyScenePoint) {
  if (!curScenePointDirty) return false;
  curScenePointDirty = false;
  if (dirtyScenePoint == nullptr ||
      dirtyScenePoint->GetScenePointType() == LOOK ||
      dirtyScenePoint != curScenePoint) {
    return true;
  }
  idList<idEntityPtr<idScenePoint>, 5> refreshed;
  ListForType(dirtyScenePoint->GetScenePointType()).Clear();
  GetScenePoints(&refreshed, dirtyScenePoint->GetScenePointType());
  return !Contains(refreshed, dirtyScenePoint);
}

// DIA: idAIMemory::CheckIfDirtyLookScenePoint
bool idAIMemory::CheckIfDirtyLookScenePoint(
    idScenePoint* dirtyLookScenePoint) {
  if (curLookScenePoint == nullptr || !curLookScenePointDirty) return false;
  curLookScenePointDirty = false;
  if (dirtyLookScenePoint == nullptr ||
      dirtyLookScenePoint->GetScenePointType() != LOOK ||
      dirtyLookScenePoint != curLookScenePoint) {
    return false;
  }
  idList<idEntityPtr<idScenePoint>, 5> refreshed;
  lookScenePoints.Clear();
  GetScenePoints(&refreshed, LOOK);
  return !Contains(refreshed, dirtyLookScenePoint);
}

// DIA: idAIMemory::GetScenePoints
void idAIMemory::GetScenePoints(
    idList<idEntityPtr<idScenePoint>, 5>* output,
    scenePointType_t type) {
  if (output == nullptr) return;
  idScenePointList& cached = ListForType(type);
  if (cached.hasSearchedForScenePoints) {
    *output = cached.scenePointList;
    return;
  }
  output->Clear();
  for (int index = 0; index < scenePointVolumes.Num(); ++index) {
    if (scenePointVolumes[index] != nullptr) {
      scenePointVolumes[index]->GetScenePoints(
          output, type, HasRestrictionVolume() ? restrictionVolume : nullptr,
          scenePointUserGroup);
    }
  }
  cached.scenePointList = *output;
  cached.hasSearchedForScenePoints = true;
}

// DIA: idAIMemory::AddScenePointVolume
void idAIMemory::AddScenePointVolume(idVolume_ScenePoints* volume) {
  if (volume == nullptr) return;
  for (int index = 0; index < scenePointVolumes.Num(); ++index) {
    if (scenePointVolumes[index] == volume) return;
  }
  scenePointVolumes.Append(volume);
  ClearScenePointLists();
}

idAIMemory::idScenePointList& idAIMemory::ListForType(
    scenePointType_t type) {
  if (type == COMBAT) return combatScenePoints;
  if (type == SEARCH) return searchScenePoints;
  if (type == LOOK) return lookScenePoints;
  return roamScenePoints;
}

const idAIMemory::idScenePointList& idAIMemory::ListForType(
    scenePointType_t type) const {
  if (type == COMBAT) return combatScenePoints;
  if (type == SEARCH) return searchScenePoints;
  if (type == LOOK) return lookScenePoints;
  return roamScenePoints;
}

bool idAIMemory::Contains(
    const idList<idEntityPtr<idScenePoint>, 5>& list,
    idScenePoint* point) {
  for (int index = 0; index < list.Num(); ++index) {
    if (static_cast<idScenePoint*>(list[index]) == point) return true;
  }
  return false;
}

// Header-emitted memory accessors used by AI2.
const idEntity* idAIMemory::GetEnemy() const { return currentEnemy; }

const idEntity* idAIMemory::GetFriendly() const { return currentFriendly; }

const idEntity* idAIMemory::GetFriendlyBlocker() const {
  return friendlyBlocker;
}

idEntity* idAIMemory::GetLastAttacker() const { return lastAttacker; }

bool idAIMemory::ShouldAvoid(const int currentTimeValue) {
  return avoidEntity != nullptr && currentTimeValue < avoidFinishedTime &&
         avoidRadius != 0.0F;
}

void idAIMemory::SetAvoidEntity(const int currentTimeValue,
                                const idEntity* entity,
                                const float radius,
                                const float durationSeconds) {
  avoidEntity = entity;
  avoidRadius = radius;
  avoidFinishedTime =
      currentTimeValue - static_cast<int>(durationSeconds * -1000.0F);
}

void idAIMemory::SetSpottedEnemyTrigger(const idEntity* entity) {
  spottedEnemyTrigger = entity;
  spottedEnemyTriggerNumber =
      entity != nullptr ? entity->GetEntityNumber() : -1;
}

const idEntity* idAIMemory::GetSpottedEnemyTrigger() const {
  if (spottedEnemyTrigger == nullptr) return nullptr;
  if (gameLocal == nullptr || spottedEnemyTriggerNumber < 0) {
    return spottedEnemyTrigger;
  }
  return gameLocal->GetEntityByNumber(spottedEnemyTriggerNumber) ==
                 spottedEnemyTrigger
             ? spottedEnemyTrigger
             : nullptr;
}

void idAIMemory::SetDetectedEnemyTrigger(const idEntity* entity) {
  detectedEnemyTrigger = entity;
  detectedEnemyTriggerNumber =
      entity != nullptr ? entity->GetEntityNumber() : -1;
}

const idEntity* idAIMemory::GetDetectedEnemyTrigger() const {
  if (detectedEnemyTrigger == nullptr) return nullptr;
  if (gameLocal == nullptr || detectedEnemyTriggerNumber < 0) {
    return detectedEnemyTrigger;
  }
  return gameLocal->GetEntityByNumber(detectedEnemyTriggerNumber) ==
                 detectedEnemyTrigger
             ? detectedEnemyTrigger
             : nullptr;
}

void idAIMemory::SetTriggeredInteraction(idPlayer* entity) {
  triggeredInteraction = entity;
  triggeredInteractionNumber =
      entity != nullptr ? entity->GetEntityNumber() : -1;
}

idPlayer* idAIMemory::GetTriggeredInteractionForTest() const {
  if (triggeredInteraction == nullptr) return nullptr;
  if (gameLocal == nullptr || triggeredInteractionNumber < 0) {
    return triggeredInteraction;
  }
  return gameLocal->GetEntityByNumber(triggeredInteractionNumber) ==
                 triggeredInteraction
             ? triggeredInteraction
             : nullptr;
}
