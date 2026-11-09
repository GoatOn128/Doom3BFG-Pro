#include "tungsten/game/ai/ai2_actions.h"
#include "tungsten/game/ai/aimemory.h"

#include <algorithm>

eventVoid* idAI2::RecordGameEvent(const char* eventName, eventVoid* result) {
  aiEventState.lastEvent = eventName != nullptr ? eventName : "";
  ++aiEventState.sequence;
  if (result != nullptr) {
    result->accepted = true;
    result->completed = true;
  }
  return result;
}

#define PORT_AI_GAME_EVENT(methodName)                                     \
  eventVoid* idAI2::methodName(eventVoid* result, ...) {                  \
    return RecordGameEvent(#methodName, result);                          \
  }

// DIA: idActor::AnimEvent_DropWeapons
eventVoid* idActor::AnimEvent_DropWeapons(eventVoid* result) {
  weaponsDropped = true;
  if (result != nullptr) {
    result->accepted = true;
    result->completed = true;
  }
  return result;
}

// DIA: idAI2::Event_MoveToPathPoint
eventVoid* idAI2::Event_MoveToPathPoint(eventVoid* result,
                                        const idEntity* pathPoint) {
  aiEventState.path = const_cast<idEntity*>(pathPoint);
  return RecordGameEvent("Event_MoveToPathPoint", result);
}

// DIA: idAI2::Event_SetAlertCycle
eventVoid* idAI2::Event_SetAlertCycle(eventVoid* result, int alertCycle) {
  aiEventState.alertCycle = alertCycle;
  return RecordGameEvent("Event_SetAlertCycle", result);
}

// DIA: idAI2::Event_SetCallback
eventInt* idAI2::Event_SetCallback(eventInt* result) {
  ++aiEventState.callback;
  if (result != nullptr) result->value = aiEventState.callback;
  return result;
}

// DIA: idAI2::Event_GetCurrentAIEventClass
eventInt* idAI2::Event_GetCurrentAIEventClass(eventInt* result) {
  if (result != nullptr) result->value = aiEventState.currentEventClass;
  return result;
}

// DIA: idAI2::Event_GetLastAIEventClass
eventInt* idAI2::Event_GetLastAIEventClass(eventInt* result) {
  if (result != nullptr) result->value = aiEventState.lastEventClass;
  return result;
}

// DIA: idAI2::Event_GetLastAIEventOrigin
eventVector* idAI2::Event_GetLastAIEventOrigin(eventVector* result) {
  if (result != nullptr) result->value = aiEventState.lastEventOrigin;
  return result;
}

// DIA: idAI2::Event_TeslaIsActive
eventBool* idAI2::Event_TeslaIsActive(eventBool* result) {
  if (result != nullptr) result->value = aiEventState.teslaActive;
  return result;
}

// DIA: idTreeAnimator::GetModelDefName
const char* idTreeAnimator::GetModelDefName() {
  return modelDefName.data;
}

// DIA: idFaction::GetName
const char* idFaction::GetName() {
  return name.data;
}

// DIA: idEntity::GetEntityDefName
const char* idEntity::GetEntityDefName() {
  return entityDefName.empty() ? name.c_str() : entityDefName.c_str();
}

// DIA: idAI2::Event_UpdateGroupAmbush
PORT_AI_GAME_EVENT(Event_UpdateGroupAmbush)

// DIA: idAI2::Event_SetGlobalAggro
eventVoid* idAI2::Event_SetGlobalAggro(eventVoid* result, float aggression) {
  aiEventState.globalAggro = aggression;
  return RecordGameEvent("Event_SetGlobalAggro", result);
}

// DIA: idAI2::Event_KillAI
eventVoid* idAI2::Event_KillAI(eventVoid* result) {
  animEventState.health = 0;
  animEventState.dead = true;
  return RecordGameEvent("Event_KillAI", result);
}

// DIA: idAI2::Event_AnimWeb_Wait
PORT_AI_GAME_EVENT(Event_AnimWeb_Wait)
// DIA: idAI2::Event_AnimWeb_BeginAnim
PORT_AI_GAME_EVENT(Event_AnimWeb_BeginAnim)
// DIA: idAI2::Event_AnimWeb_ReloadRightFinished
PORT_AI_GAME_EVENT(Event_AnimWeb_ReloadRightFinished)
// DIA: idAI2::Event_AnimWeb_EnableFireFromCover
PORT_AI_GAME_EVENT(Event_AnimWeb_EnableFireFromCover)

// DIA: idAI2::Event_RelinquishControl
eventVoid* idAI2::Event_RelinquishControl(eventVoid* result) {
  aiEventState.controlRelinquished = true;
  return RecordGameEvent("Event_RelinquishControl", result);
}

// DIA: idAI2::Event_TurnOffLaserSight
eventVoid* idAI2::Event_TurnOffLaserSight(eventVoid* result) {
  aiEventState.laserSight = false;
  return RecordGameEvent("Event_TurnOffLaserSight", result);
}

// DIA: idAI2::Event_TurnOnLaserSight
eventVoid* idAI2::Event_TurnOnLaserSight(eventVoid* result) {
  aiEventState.laserSight = true;
  return RecordGameEvent("Event_TurnOnLaserSight", result);
}

// DIA: idAI2::Event_Path
eventVoid* idAI2::Event_Path(eventVoid* result, idEntity* newPath) {
  aiEventState.path = newPath;
  return RecordGameEvent("Event_Path", result);
}

// DIA: idAI2::Event_SetOverrideScenePoint
eventVoid* idAI2::Event_SetOverrideScenePoint(eventVoid* result,
                                              idEntity* scenePoint) {
  aiEventState.overrideScenePoint = scenePoint;
  return RecordGameEvent("Event_SetOverrideScenePoint", result);
}

// DIA: idAI2::Event_GetBodyOrientation
eventVector* idAI2::Event_GetBodyOrientation(eventVector* result) {
  if (result != nullptr) result->value = aiEventState.bodyOrientation;
  return result;
}

// DIA: idAI2::Event_StartVoiceOver
eventBool* idAI2::Event_StartVoiceOver(eventBool* result,
                                       const idDeclVoiceOver* voiceOver, ...) {
  aiEventState.playingVoiceOver = voiceOver;
  voiceOverPlaying = voiceOver != nullptr;
  if (result != nullptr) result->value = voiceOverPlaying;
  return result;
}

// DIA: idAI2::Event_IsVoiceOverPlaying
eventBool* idAI2::Event_IsVoiceOverPlaying(eventBool* result,
                                           const idDeclVoiceOver* voiceOver) {
  if (result != nullptr) {
    result->value = voiceOverPlaying &&
                    (voiceOver == nullptr ||
                     voiceOver == aiEventState.playingVoiceOver);
  }
  return result;
}

// DIA: idAI2::Event_IsPlayingVoiceOver
eventBool* idAI2::Event_IsPlayingVoiceOver(eventBool* result) {
  if (result != nullptr) result->value = voiceOverPlaying;
  return result;
}

// DIA: idAI2::Event_GetPlayingVoiceOver
eventDecl* idAI2::Event_GetPlayingVoiceOver(eventDecl* result) {
  if (result != nullptr) result->value = aiEventState.playingVoiceOver;
  return result;
}

// DIA: idAI2::Event_SetFocus
PORT_AI_GAME_EVENT(Event_SetFocus)
// DIA: idAI2::Event_ClearLookFocus
eventVoid* idAI2::Event_ClearLookFocus(eventVoid* result) {
  return RecordGameEvent("Event_ClearLookFocus", result);
}
// DIA: idAI2::Event_ClearAimFocus
eventVoid* idAI2::Event_ClearAimFocus(eventVoid* result) {
  return RecordGameEvent("Event_ClearAimFocus", result);
}
// DIA: idAI2::Event_DisableAutoFocus
PORT_AI_GAME_EVENT(Event_DisableAutoFocus)

// DIA: idAI2::Event_SetRemoveHealthWhenDamaged
eventVoid* idAI2::Event_SetRemoveHealthWhenDamaged(eventVoid* result,
                                                   bool value) {
  aiEventState.removeHealthWhenDamaged = value;
  return RecordGameEvent("Event_SetRemoveHealthWhenDamaged", result);
}

// DIA: idAI2::Event_SetEventOverloadEnableFlag
eventVoid* idAI2::Event_SetEventOverloadEnableFlag(eventVoid* result,
                                                   int flags) {
  aiEventState.overloadEnableFlags |= flags;
  return RecordGameEvent("Event_SetEventOverloadEnableFlag", result);
}

// DIA: idAI2::Event_ClearEventOverloadEnableFlag
eventVoid* idAI2::Event_ClearEventOverloadEnableFlag(eventVoid* result,
                                                     int flags) {
  aiEventState.overloadEnableFlags &= ~flags;
  return RecordGameEvent("Event_ClearEventOverloadEnableFlag", result);
}

// DIA: idAI2::Event_SetEventOverloadDisableFlag
eventVoid* idAI2::Event_SetEventOverloadDisableFlag(eventVoid* result,
                                                    int flags) {
  aiEventState.overloadDisableFlags |= flags;
  return RecordGameEvent("Event_SetEventOverloadDisableFlag", result);
}

// DIA: idAI2::Event_ClearEventOverloadDisableFlag
eventVoid* idAI2::Event_ClearEventOverloadDisableFlag(eventVoid* result,
                                                      int flags) {
  aiEventState.overloadDisableFlags &= ~flags;
  return RecordGameEvent("Event_ClearEventOverloadDisableFlag", result);
}

// DIA: idAI2::Event_SetBehaviorDecl
PORT_AI_GAME_EVENT(Event_SetBehaviorDecl)

// DIA: idAI2::Event_ShouldTurnTo
eventInt* idAI2::Event_ShouldTurnTo(eventInt* result, ...) {
  if (result != nullptr) result->value = 1;
  return result;
}

// DIA: idAI2::Event_IsEntityVisible
eventBool* idAI2::Event_IsEntityVisible(eventBool* result,
                                        const idEntity* entity) {
  if (result != nullptr) {
    result->value = entity != nullptr && !entity->IsHidden() &&
                    !entity->IsRemoved();
  }
  return result;
}

// DIA: idAI2::Event_TriggerGore
PORT_AI_GAME_EVENT(Event_TriggerGore)

// DIA: idAI2::Event_SetCloseMeleeInterval
eventVoid* idAI2::Event_SetCloseMeleeInterval(eventVoid* result,
                                              float interval) {
  aiEventState.closeMeleeInterval = std::max(0.0F, interval);
  return RecordGameEvent("Event_SetCloseMeleeInterval", result);
}

// DIA: idAI2::Event_SetMediumMeleeInterval
eventVoid* idAI2::Event_SetMediumMeleeInterval(eventVoid* result,
                                               float interval) {
  aiEventState.mediumMeleeInterval = std::max(0.0F, interval);
  return RecordGameEvent("Event_SetMediumMeleeInterval", result);
}

// DIA: idAI2::Event_SetLongMeleeInterval
eventVoid* idAI2::Event_SetLongMeleeInterval(eventVoid* result,
                                             float interval) {
  aiEventState.longMeleeInterval = std::max(0.0F, interval);
  return RecordGameEvent("Event_SetLongMeleeInterval", result);
}

// DIA: idAI2::Event_EnableTaunts
eventVoid* idAI2::Event_EnableTaunts(eventVoid* result, bool enable) {
  aiEventState.tauntsEnabled = enable;
  return RecordGameEvent("Event_EnableTaunts", result);
}

// DIA: idAI2::Event_Push
PORT_AI_GAME_EVENT(Event_Push)

// DIA: idAI2::Event_StopAllEffects
eventVoid* idAI2::Event_StopAllEffects(eventVoid* result) {
  aiEventState.effectsStopped = true;
  return RecordGameEvent("Event_StopAllEffects", result);
}

// DIA: idAI2::Event_FakeSenseStimulus
PORT_AI_GAME_EVENT(Event_FakeSenseStimulus)

// DIA: idAI2::Event_SetTeslaCooldown
eventVoid* idAI2::Event_SetTeslaCooldown(eventVoid* result, float duration) {
  aiEventState.teslaCooldown = std::max(0.0F, duration);
  aiEventState.teslaActive = duration > 0.0F;
  return RecordGameEvent("Event_SetTeslaCooldown", result);
}

// DIA: idAI2::Event_SetFOV
eventVoid* idAI2::Event_SetFOV(eventVoid* result, float fov) {
  SetFOV(idTypesafeNumber<float, DegreesUnique_t>(
      std::clamp(fov, 0.0F, 360.0F)));
  return RecordGameEvent("Event_SetFOV", result);
}

// DIA: idAI2::Event_SetEMPStunDuration
eventVoid* idAI2::Event_SetEMPStunDuration(eventVoid* result,
                                           float duration) {
  aiEventState.empStunDuration = std::max(0.0F, duration);
  return RecordGameEvent("Event_SetEMPStunDuration", result);
}

// DIA: idList<idVec3,5>::RemoveFast
template bool idList<idVec3, 5>::RemoveFast(const idVec3& value);

// DIA: idAI2::LocationCallingHelper
void idAI2::LocationCallingHelper(idEntity* entity) {
  aiEventState.lastEventOriginator = entity;
  if (entity != nullptr && entity->GetPhysics() != nullptr) {
    aiEventState.lastEventOrigin = entity->GetPhysics()->GetOrigin();
  }
}

// DIA: idAI2::Event_SpawnLootBox
eventVoid* idAI2::Event_SpawnLootBox(eventVoid* result) {
  aiEventState.lootBoxSpawned = true;
  return RecordGameEvent("Event_SpawnLootBox", result);
}

// DIA: idAI2::Event_RemoveBody
eventVoid* idAI2::Event_RemoveBody(eventVoid* result) {
  aiEventState.bodyRemoved = true;
  Remove();
  return RecordGameEvent("Event_RemoveBody", result);
}

// DIA: idAI2::Event_DisintigrateHide
eventVoid* idAI2::Event_DisintigrateHide(eventVoid* result) {
  OnHide(true);
  return RecordGameEvent("Event_DisintigrateHide", result);
}

// DIA: idAI2::Event_SetAnimWebScriptBlend
eventVoid* idAI2::Event_SetAnimWebScriptBlend(eventVoid* result,
                                              float blendValue) {
  aiEventState.animWebScriptBlend = std::clamp(blendValue, 0.0F, 1.0F);
  return RecordGameEvent("Event_SetAnimWebScriptBlend", result);
}

// DIA: idAI2::Event_RemoveTeslaBlindFirePosition
eventVoid* idAI2::Event_RemoveTeslaBlindFirePosition(
    eventVoid* result, const idVec3* position) {
  if (position != nullptr) {
    aiEventState.teslaBlindFirePositions.RemoveFast(*position);
  }
  return RecordGameEvent("Event_RemoveTeslaBlindFirePosition", result);
}

// DIA: idAI2::Event_ClearTeslaBlindFirePositions
eventVoid* idAI2::Event_ClearTeslaBlindFirePositions(eventVoid* result) {
  aiEventState.teslaBlindFirePositions.Clear();
  return RecordGameEvent("Event_ClearTeslaBlindFirePositions", result);
}

// DIA: idEncounterGroup::ResetSearchPoints
void idEncounterGroup::ResetSearchPoints() {
  ++resetCount;
}

// DIA: idAI2::Event_AwarenessLost
PORT_AI_GAME_EVENT(Event_AwarenessLost)

// DIA: idAI2::Event_ResetSearchPoints
eventVoid* idAI2::Event_ResetSearchPoints(eventVoid* result) {
  return RecordGameEvent("Event_ResetSearchPoints", result);
}

// DIA: idAI2::EnemySightedHelper
void idAI2::EnemySightedHelper(idEntity* entity, bool firstSight) {
  aiEventState.lastEventOriginator = entity;
  aiEventState.lastEventClass = aiEventState.currentEventClass;
  aiEventState.currentEventClass = firstSight ? 1 : 2;
}

// DIA: idAI2::Event_AwarenessRegained
PORT_AI_GAME_EVENT(Event_AwarenessRegained)

// DIA: idAI2::Event_FlyBack
eventVoid* idAI2::Event_FlyBack(eventVoid* result) {
  return RecordGameEvent("Event_FlyBack", result);
}

// DIA: idAI2::Event_GetLastAIEventOriginator
eventEntity* idAI2::Event_GetLastAIEventOriginator(eventEntity* result) {
  if (result != nullptr) result->value = aiEventState.lastEventOriginator;
  return result;
}

// DIA: idAI2::Event_GetLastAIEventInstigator
eventEntity* idAI2::Event_GetLastAIEventInstigator(eventEntity* result) {
  if (result != nullptr) result->value = aiEventState.lastEventInstigator;
  return result;
}

// DIA: idAI2::Event_SetTakedownRestrictionVolume
eventVoid* idAI2::Event_SetTakedownRestrictionVolume(eventVoid* result,
                                                     idEntity* volume) {
  aiEventState.takedownRestrictionVolume = volume;
  return RecordGameEvent("Event_SetTakedownRestrictionVolume", result);
}

// DIA: idAI2::Event_SetScenePointUserGroup
eventVoid* idAI2::Event_SetScenePointUserGroup(eventVoid* result,
                                               idEntity* userGroup) {
  aiEventState.scenePointUserGroup = userGroup;
  return RecordGameEvent("Event_SetScenePointUserGroup", result);
}

// DIA: idAI2::Event_GetAvoidEntity
eventEntity* idAI2::Event_GetAvoidEntity(eventEntity* result) {
  if (result != nullptr) result->value = aiEventState.avoidEntity;
  return result;
}

// DIA: idPlayer::GetCurFirstPersonEvent
idFirstPersonEvent* idPlayer::GetCurFirstPersonEvent() {
  return currentFirstPersonEvent;
}

// DIA: idAIMemory::SetScriptedAggro
void idAIMemory::SetScriptedAggro(const idEntity* entity, float aggression,
                                  int endTime) {
  scriptedAggroEntity = entity;
  scriptedAggression = aggression;
  scriptedAggroEndTime = endTime;
}

// DIA: idAI2::Event_AwarenessGained
PORT_AI_GAME_EVENT(Event_AwarenessGained)
// DIA: idAI2::Event_Surprised
PORT_AI_GAME_EVENT(Event_Surprised)
// DIA: idAI2::Event_AddAggression
PORT_AI_GAME_EVENT(Event_AddAggression)
// DIA: idAI2::Event_AnimWeb_StartUseTurret
PORT_AI_GAME_EVENT(Event_AnimWeb_StartUseTurret)
// DIA: idAI2::Event_AnimWeb_BeginSyncedAnim
PORT_AI_GAME_EVENT(Event_AnimWeb_BeginSyncedAnim)

// DIA: idAI2::Event_ActionNode
eventVoid* idAI2::Event_ActionNode(eventVoid* result, idEntity* actionNode) {
  aiEventState.actionNode = actionNode;
  return RecordGameEvent("Event_ActionNode", result);
}

// DIA: idAI2::Event_Touch
PORT_AI_GAME_EVENT(Event_Touch)

// DIA: idAI2::Event_AddTeslaBlindFirePosition
eventVoid* idAI2::Event_AddTeslaBlindFirePosition(
    eventVoid* result, const idVec3* position) {
  if (position != nullptr) {
    aiEventState.teslaBlindFirePositions.Append(*position);
  }
  return RecordGameEvent("Event_AddTeslaBlindFirePosition", result);
}

// DIA: idList<idAI2::idAIVolatile::idAIRenderParamBlend,5>::Resize
template bool idList<idAI2::idAIVolatile::idAIRenderParamBlend, 5>::Resize(
    int newSize);

// DIA: idList<idAI2::idAIVolatile::idAIRenderParamBlend,5>::Append
template int idList<idAI2::idAIVolatile::idAIRenderParamBlend, 5>::Append(
    const idAI2::idAIVolatile::idAIRenderParamBlend& value);

// DIA: idAI2::Event_BlendRenderParameter
eventVoid* idAI2::Event_BlendRenderParameter(eventVoid* result, int parameter,
                                             float amount, float blendTime) {
  idAIVolatile::idAIRenderParamBlend* blend = nullptr;
  for (int index = 0; index < renderParameterBlends.Num(); ++index) {
    if (renderParameterBlends[index].parameter == parameter) {
      blend = &renderParameterBlends[index];
      break;
    }
  }
  if (blend == nullptr) {
    idAIVolatile::idAIRenderParamBlend newBlend;
    newBlend.parameter = parameter;
    const int index = renderParameterBlends.Append(newBlend);
    blend = &renderParameterBlends[index];
  }
  blend->currentValue = blend->targetValue;
  blend->targetValue = amount;
  blend->duration = std::max(0.0F, blendTime);
  return RecordGameEvent("Event_BlendRenderParameter", result);
}

// DIA: idAI2::Event_DropAttachedProjectiles
eventVoid* idAI2::Event_DropAttachedProjectiles(eventVoid* result) {
  return RecordGameEvent("Event_DropAttachedProjectiles", result);
}

#undef PORT_AI_GAME_EVENT
