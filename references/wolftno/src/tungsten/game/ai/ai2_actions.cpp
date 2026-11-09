#include "tungsten/game/ai/ai2_actions.h"
#include "tungsten/game/ai/aimemory.h"
#include "tungsten/game/ai/aivoicecontroller.h"
#include "tungsten/game/animation/facemgr.h"
#include "tungsten/game/clientgame/presentable/presentable.h"
#include "engine/animation/md6.h"
#include "shared/idlib/lib.h"

// DIA: idAI2::AnimEvent_StopVoiceOver
eventVoid* idAI2::AnimEvent_StopVoiceOver(eventVoid* result) {
  localVoiceController->StopVoiceOver(this);

  idTreeAnimator* treeAnimator = nullptr;
  if (presentable != nullptr &&
      presentable->GetAnimatedEntityInterface() != nullptr) {
    idPresentable* animatedPresentable =
        presentable != nullptr
            ? presentable->GetAnimatedEntityInterface()
            : nullptr;
    treeAnimator = static_cast<idTreeAnimator*>(
        animatedPresentable->GetAnimatedTreeAnimator());
  }
  faceManager->Reset(this, treeAnimator);
  return result;
}

// DIA: idAI2::Action_LeapAttack
void idAI2::Action_LeapAttack(eventVoid* result, const idVec3* leapTarget) {
  idLib::Error("FIXME: Action_LeapAttack not implemented!", result,
               leapTarget);
}

// DIA: idAI2::Action_MoveToPoint
eventVoid* idAI2::Action_MoveToPoint(
    eventVoid* result, const idVec3* destination, int stopUponArrival,
    float, aiActionBool_t align, const idVec3* alignDirection) {
  scriptProxy.blocking = true;
  scriptProxy.ProcessEvent(
      "ACTION_MoveToPointEnter",
      {idAIScriptEventArg::Entity(this), idAIScriptEventArg::Integer(33),
       idAIScriptEventArg::Vector(*destination),
       idAIScriptEventArg::Integer(stopUponArrival),
       idAIScriptEventArg::Float(0.1F),
       idAIScriptEventArg::Integer(align != AIACTION_FALSE),
       idAIScriptEventArg::Vector(*alignDirection)});
  return result;
}

// DIA: `idAI2::Action_MoveToPoint'::`1'::dtor$0
// DIA: `idAI2::Action_MoveToPoint'::`1'::dtor$1
// DIA: `idAI2::Action_MoveToPoint'::`1'::dtor$2
// DIA: `idAI2::Action_MoveToPoint'::`1'::dtor$3
// DIA: `idAI2::Action_MoveToPoint'::`1'::dtor$4
// DIA: `idAI2::Action_MoveToPoint'::`1'::dtor$5
// DIA: idAI2::Action_MoveToPointNoFail
eventVoid* idAI2::Action_MoveToPointNoFail(
    eventVoid* result, const idVec3* destination, int stopUponArrival,
    float, aiActionBool_t align, const idVec3* alignDirection,
    const idDeclVoiceOver* voiceOver) {
  scriptProxy.blocking = true;
  scriptProxy.ProcessEvent(
      "ACTION_MoveToPointNoFailEnter",
      {idAIScriptEventArg::Entity(this), idAIScriptEventArg::Integer(33),
       idAIScriptEventArg::Vector(*destination),
       idAIScriptEventArg::Integer(stopUponArrival),
       idAIScriptEventArg::Float(0.1F),
       idAIScriptEventArg::Integer(align != AIACTION_FALSE),
       idAIScriptEventArg::Vector(*alignDirection),
       idAIScriptEventArg::Decl(voiceOver)});
  return result;
}

// DIA: `idAI2::Action_MoveToPointNoFail'::`1'::dtor$0
// DIA: `idAI2::Action_MoveToPointNoFail'::`1'::dtor$1
// DIA: `idAI2::Action_MoveToPointNoFail'::`1'::dtor$2
// DIA: `idAI2::Action_MoveToPointNoFail'::`1'::dtor$3
// DIA: `idAI2::Action_MoveToPointNoFail'::`1'::dtor$4
// DIA: `idAI2::Action_MoveToPointNoFail'::`1'::dtor$5
// DIA: `idAI2::Action_MoveToPointNoFail'::`1'::dtor$6
// DIA: idAI2::Action_MoveToEntity
eventVoid* idAI2::Action_MoveToEntity(
    eventVoid* result, const idEntity* entity, int stopUponArrival,
    float reachedDistance, aiActionBool_t align,
    const idVec3* alignDirection) {
  scriptProxy.blocking = true;
  scriptProxy.ProcessEvent(
      "ACTION_MoveToEntityEnter",
      {idAIScriptEventArg::Entity(this), idAIScriptEventArg::Integer(33),
       idAIScriptEventArg::Entity(entity),
       idAIScriptEventArg::Integer(stopUponArrival),
       idAIScriptEventArg::Float(reachedDistance),
       idAIScriptEventArg::Integer(align != AIACTION_FALSE),
       idAIScriptEventArg::Vector(*alignDirection)});
  return result;
}

// DIA: idAI2::Action_MoveToEntityNoFail
eventVoid* idAI2::Action_MoveToEntityNoFail(
    eventVoid* result, const idEntity* entity, int stopUponArrival,
    float reachedDistance, aiActionBool_t align,
    const idVec3* alignDirection, const idDeclVoiceOver* voiceOver) {
  scriptProxy.blocking = true;
  scriptProxy.ProcessEvent(
      "ACTION_MoveToEntityNoFailEnter",
      {idAIScriptEventArg::Entity(this), idAIScriptEventArg::Integer(33),
       idAIScriptEventArg::Entity(entity),
       idAIScriptEventArg::Integer(stopUponArrival),
       idAIScriptEventArg::Float(reachedDistance),
       idAIScriptEventArg::Integer(align != AIACTION_FALSE),
       idAIScriptEventArg::Vector(*alignDirection),
       idAIScriptEventArg::Decl(voiceOver)});
  return result;
}

// DIA: `idAI2::Action_MoveToEntityNoFail'::`1'::dtor$0
// DIA: `idAI2::Action_MoveToEntityNoFail'::`1'::dtor$1
// DIA: `idAI2::Action_MoveToEntityNoFail'::`1'::dtor$2
// DIA: `idAI2::Action_MoveToEntityNoFail'::`1'::dtor$3
// DIA: `idAI2::Action_MoveToEntityNoFail'::`1'::dtor$4
// DIA: `idAI2::Action_MoveToEntityNoFail'::`1'::dtor$5
// DIA: `idAI2::Action_MoveToEntityNoFail'::`1'::dtor$6
// DIA: idAI2::Action_MoveToPathPoint
eventVoid* idAI2::Action_MoveToPathPoint(eventVoid* result,
                                         const idEntity* pathPoint) {
  if (pathPoint == nullptr) {
    idLib::Error("Invalid pathPoint for Action_MovetoPathPoint on entity '%s'",
                 GetName());
  }
  scriptProxy.blocking = true;
  scriptProxy.ProcessEvent(
      "ACTION_MoveToPathPointEnter",
      {idAIScriptEventArg::Entity(this), idAIScriptEventArg::Integer(33),
       idAIScriptEventArg::Entity(pathPoint)});
  return result;
}

// DIA: `idAI2::Action_MoveToPathPoint'::`1'::dtor$0
// DIA: `idAI2::Action_MoveToPathPoint'::`1'::dtor$1
// DIA: idAI2::Action_MoveToPathPointNoFail
eventVoid* idAI2::Action_MoveToPathPointNoFail(
    eventVoid* result, const idEntity* pathPoint,
    const idDeclVoiceOver* voiceOver) {
  if (pathPoint == nullptr) {
    idLib::Error(
        "Invalid pathPoint for Action_MovetoPathPointNoFail on entity '%s'",
        GetName());
  }
  scriptProxy.blocking = true;
  scriptProxy.ProcessEvent(
      "ACTION_MoveToPathPointNoFailEnter",
      {idAIScriptEventArg::Entity(this), idAIScriptEventArg::Integer(33),
       idAIScriptEventArg::Entity(pathPoint),
       idAIScriptEventArg::Decl(voiceOver)});
  return result;
}

// DIA: `idAI2::Action_MoveToPathPointNoFail'::`1'::dtor$0
// DIA: `idAI2::Action_MoveToPathPointNoFail'::`1'::dtor$1
// DIA: `idAI2::Action_MoveToPathPointNoFail'::`1'::dtor$2
// DIA: idAI2::Action_MoveToCover
eventVoid* idAI2::Action_MoveToCover(
    eventVoid* result, const idVec3* destination, int coverAction,
    posture_t posture, const idVec3* alignDirection) {
  scriptProxy.blocking = true;
  scriptProxy.ProcessEvent(
      "ACTION_MoveToCoverEnter",
      {idAIScriptEventArg::Entity(this), idAIScriptEventArg::Integer(33),
       idAIScriptEventArg::Vector(*destination),
       idAIScriptEventArg::Integer(coverAction),
       idAIScriptEventArg::Integer(posture), idAIScriptEventArg::Integer(1),
       idAIScriptEventArg::Vector(*alignDirection)});
  return result;
}

// DIA: idAI2::Action_UseZipline
eventVoid* idAI2::Action_UseZipline(eventVoid* result,
                                    const idEntity* entity) {
  scriptProxy.blocking = true;
  scriptProxy.ProcessEvent(
      "ACTION_UseZiplineEnter",
      {idAIScriptEventArg::Entity(this), idAIScriptEventArg::Entity(entity)});
  return result;
}

// DIA: idAI2::Action_SetMovePushStatus
eventVoid* idAI2::Action_SetMovePushStatus(eventVoid* result,
                                           int pushMoveStatus) {
  scriptProxy.blocking = false;
  scriptProxy.ProcessEvent(
      "ACTION_SetMovePushStatusEnter",
      {idAIScriptEventArg::Entity(this),
       idAIScriptEventArg::Integer(pushMoveStatus)});
  return result;
}

// DIA: idAI2::Action_Idle
eventVoid* idAI2::Action_Idle(eventVoid* result, int time,
                              int idleTurnType) {
  scriptProxy.blocking = time != 0;
  scriptProxy.ProcessEvent(
      "ACTION_IdleEnter",
      {idAIScriptEventArg::Entity(this), idAIScriptEventArg::Integer(time),
       idAIScriptEventArg::Integer(idleTurnType)});
  return result;
}

// DIA: idAI2::Action_Pain
eventVoid* idAI2::Action_Pain(eventVoid* result, painType_t, aiDirection_t) {
  scriptProxy.blocking = false;
  scriptProxy.ProcessEvent("ACTION_PainEnter",
                           {idAIScriptEventArg::Entity(this)});
  return result;
}

// DIA: idAI2::Action_StandToCrouch
eventVoid* idAI2::Action_StandToCrouch(eventVoid* result) {
  scriptProxy.blocking = true;
  scriptProxy.ProcessEvent("ACTION_StandToCrouchEnter",
                           {idAIScriptEventArg::Entity(this)});
  return result;
}

// DIA: idAI2::Action_CrouchToStand
eventVoid* idAI2::Action_CrouchToStand(eventVoid* result) {
  scriptProxy.blocking = true;
  scriptProxy.ProcessEvent("ACTION_CrouchToStandEnter",
                           {idAIScriptEventArg::Entity(this)});
  return result;
}

// DIA: idAI2::Action_PullTriggerRight
eventVoid* idAI2::Action_PullTriggerRight(eventVoid* result) {
  scriptProxy.blocking = false;
  scriptProxy.ProcessEvent("ACTION_PullTriggerRightEnter",
                           {idAIScriptEventArg::Entity(this)});
  return result;
}

// DIA: idAI2::Action_ReleaseTriggerRight
eventVoid* idAI2::Action_ReleaseTriggerRight(eventVoid* result) {
  scriptProxy.blocking = false;
  scriptProxy.ProcessEvent("ACTION_ReleaseTriggerRightEnter",
                           {idAIScriptEventArg::Entity(this)});
  return result;
}

// DIA: idAI2::Action_PullTriggerLeft
eventVoid* idAI2::Action_PullTriggerLeft(eventVoid* result) {
  scriptProxy.blocking = false;
  scriptProxy.ProcessEvent("ACTION_PullTriggerLeftEnter",
                           {idAIScriptEventArg::Entity(this)});
  return result;
}

// DIA: idAI2::Action_ReleaseTriggerLeft
eventVoid* idAI2::Action_ReleaseTriggerLeft(eventVoid* result) {
  scriptProxy.blocking = false;
  scriptProxy.ProcessEvent("ACTION_ReleaseTriggerLeftEnter",
                           {idAIScriptEventArg::Entity(this)});
  return result;
}

// DIA: idAI2::Action_WaitForAnim
eventVoid* idAI2::Action_WaitForAnim(eventVoid* result,
                                     const idAnimWebPath* animWebNode) {
  scriptProxy.blocking = true;
  scriptProxy.ProcessEvent(
      "ACTION_WaitForAnimEnter",
      {idAIScriptEventArg::Entity(this),
       idAIScriptEventArg::String(animWebNode->data)});
  return result;
}

// DIA: idAI2::Action_WaitForAnimVia
eventVoid* idAI2::Action_WaitForAnimVia(
    eventVoid* result, const idAnimWebPath* destinationNode,
    const idAnimWebPath* viaNode) {
  scriptProxy.blocking = true;
  scriptProxy.ProcessEvent(
      "ACTION_WaitForAnimViaEnter",
      {idAIScriptEventArg::Entity(this),
       idAIScriptEventArg::String(destinationNode->data),
       idAIScriptEventArg::String(viaNode->data)});
  return result;
}

// DIA: idAI2::Action_LoopAnim
eventVoid* idAI2::Action_LoopAnim(eventVoid* result,
                                  const idAnimWebPath* aliasName,
                                  int milliseconds) {
  scriptProxy.blocking = true;
  scriptProxy.ProcessEvent(
      "ACTION_LoopAnimEnter",
      {idAIScriptEventArg::Entity(this),
       idAIScriptEventArg::String(aliasName->data),
       idAIScriptEventArg::Integer(milliseconds)});
  return result;
}

// DIA: idAI2::Action_LoopAnimExitAtEnd
eventVoid* idAI2::Action_LoopAnimExitAtEnd(
    eventVoid* result, const idAnimWebPath* animWebNode, int milliseconds,
    int framesFromEnd) {
  scriptProxy.blocking = true;
  scriptProxy.ProcessEvent(
      "ACTION_LoopAnimExitAtEndEnter",
      {idAIScriptEventArg::Entity(this),
       idAIScriptEventArg::String(animWebNode->data),
       idAIScriptEventArg::Integer(milliseconds),
       idAIScriptEventArg::Integer(framesFromEnd)});
  return result;
}

// DIA: idAI2::Action_StartAnim
eventVoid* idAI2::Action_StartAnim(eventVoid* result,
                                   const idAnimWebPath* animWebNode,
                                   aiActionBool_t finishBlend,
                                   aiActionBool_t wait) {
  scriptProxy.blocking = true;
  scriptProxy.ProcessEvent(
      "ACTION_StartAnimEnter",
      {idAIScriptEventArg::Entity(this),
       idAIScriptEventArg::String(animWebNode->data),
       idAIScriptEventArg::Integer(finishBlend != AIACTION_FALSE),
       idAIScriptEventArg::Integer(wait != AIACTION_FALSE)});
  return result;
}

// DIA: idAI2::Action_WaitForTraversalAnim
eventVoid* idAI2::Action_WaitForTraversalAnim(
    eventVoid* result, const idAnimWebPath* animWebNode,
    aiActionBool_t runWhenDone) {
  scriptProxy.blocking = true;
  scriptProxy.ProcessEvent(
      "ACTION_WaitForTraversalAnimEnter",
      {idAIScriptEventArg::Entity(this),
       idAIScriptEventArg::String(animWebNode->data),
       idAIScriptEventArg::Integer(runWhenDone != AIACTION_FALSE)});
  return result;
}

// DIA: idAI2::Action_ChangeAnimState
eventVoid* idAI2::Action_ChangeAnimState(
    eventVoid* result, int animWeb, const idAnimWebPath* destinationNode,
    aiAnimWait_t wait) {
  scriptProxy.blocking = wait != AIANIMWAIT_DONT_WAIT;
  scriptProxy.ProcessEvent(
      "ACTION_ChangeAnimStateEnter",
      {idAIScriptEventArg::Entity(this), idAIScriptEventArg::Integer(animWeb),
       idAIScriptEventArg::String(destinationNode->data),
       idAIScriptEventArg::Integer(wait)});
  return result;
}

// DIA: idAI2::Action_ChangeAnimStateVia
eventVoid* idAI2::Action_ChangeAnimStateVia(
    eventVoid* result, int animWeb, const idAnimWebPath* destinationNode,
    const idAnimWebPath* viaNode, aiAnimWait_t wait) {
  scriptProxy.blocking = wait != AIANIMWAIT_DONT_WAIT;
  scriptProxy.ProcessEvent(
      "ACTION_ChangeAnimStateViaEnter",
      {idAIScriptEventArg::Entity(this), idAIScriptEventArg::Integer(animWeb),
       idAIScriptEventArg::String(destinationNode->data),
       idAIScriptEventArg::String(viaNode->data),
       idAIScriptEventArg::Integer(wait)});
  return result;
}

// DIA: idAI2::Action_ForceAnimState
eventVoid* idAI2::Action_ForceAnimState(
    eventVoid* result, int animWeb, const idAnimWebPath* destinationNode,
    int blendDurationMilliseconds, aiAnimWait_t wait) {
  scriptProxy.blocking = wait != AIANIMWAIT_DONT_WAIT;
  scriptProxy.ProcessEvent(
      "ACTION_ForceAnimStateEnter",
      {idAIScriptEventArg::Entity(this), idAIScriptEventArg::Integer(animWeb),
       idAIScriptEventArg::String(destinationNode->data),
       idAIScriptEventArg::Integer(blendDurationMilliseconds),
       idAIScriptEventArg::Integer(wait)});
  return result;
}

// DIA: idAI2::Action_Dodge
eventVoid* idAI2::Action_Dodge(eventVoid* result, const idVec3* dodgeFrom) {
  idLib::Warning("idAI2::Action_Dodge is called, but not implemented", result,
                 dodgeFrom);
  return result;
}

// DIA: idAI2::Action_Wait
eventVoid* idAI2::Action_Wait(eventVoid* result, int milliseconds) {
  scriptProxy.blocking = true;
  scriptProxy.ProcessEvent(
      "ACTION_WaitEnter",
      {idAIScriptEventArg::Entity(this),
       idAIScriptEventArg::Integer(milliseconds)});
  return result;
}

// DIA: idAI2::Action_Melee
eventVoid* idAI2::Action_Melee(eventVoid* result, int overrideAnimation) {
  scriptProxy.blocking = true;
  scriptProxy.ProcessEvent(
      "ACTION_MeleeEnter",
      {idAIScriptEventArg::Entity(this),
       idAIScriptEventArg::Integer(overrideAnimation),
       idAIScriptEventArg::Integer(0), idAIScriptEventArg::Vector(idVec3{})});
  return result;
}

// DIA: idAI2::Action_TurnToPoint
eventVoid* idAI2::Action_TurnToPoint(eventVoid* result,
                                     const idVec3* point) {
  scriptProxy.blocking = true;
  scriptProxy.ProcessEvent(
      "ACTION_TurnToPointEnter",
      {idAIScriptEventArg::Entity(this), idAIScriptEventArg::Vector(*point)});
  return result;
}

// DIA: idAI2::Action_TurnToEntity
eventVoid* idAI2::Action_TurnToEntity(eventVoid* result,
                                      const idEntity* entity) {
  scriptProxy.blocking = true;
  scriptProxy.ProcessEvent(
      "ACTION_TurnToEntityEnter",
      {idAIScriptEventArg::Entity(this), idAIScriptEventArg::Entity(entity)});
  return result;
}

// DIA: idAI2::Action_TurnToEntityWithOffset
eventVoid* idAI2::Action_TurnToEntityWithOffset(
    eventVoid* result, const idEntity* entity, float yawOffset) {
  scriptProxy.blocking = true;
  scriptProxy.ProcessEvent(
      "ACTION_TurnToEntityWithOffsetEnter",
      {idAIScriptEventArg::Entity(this), idAIScriptEventArg::Entity(entity),
       idAIScriptEventArg::Float(yawOffset)});
  return result;
}

// DIA: idAI2::Action_Dive
eventVoid* idAI2::Action_Dive(eventVoid* result, float diveYaw,
                              float faceYaw) {
  scriptProxy.blocking = false;
  scriptProxy.ProcessEvent(
      "ACTION_DiveEnter",
      {idAIScriptEventArg::Entity(this), idAIScriptEventArg::Integer(33),
       idAIScriptEventArg::Float(diveYaw),
       idAIScriptEventArg::Float(faceYaw)});
  return result;
}

// DIA: idAI2::Action_WaitForEntity
eventVoid* idAI2::Action_WaitForEntity(eventVoid* result,
                                       const idEntity* entity, float range) {
  scriptProxy.blocking = true;
  scriptProxy.ProcessEvent(
      "ACTION_WaitForEntityEnter",
      {idAIScriptEventArg::Entity(this), idAIScriptEventArg::Entity(entity),
       idAIScriptEventArg::Float(range)});
  return result;
}

// DIA: idAI2::Action_PlayVoiceOver
eventVoid* idAI2::Action_PlayVoiceOver(
    eventVoid* result, const idDeclVoiceOver* voiceOver,
    aiActionBool_t waitUntilFinished) {
  if (voiceOver != nullptr && voiceOver->resourceValid) {
    scriptProxy.blocking = true;
    scriptProxy.ProcessEvent(
        "ACTION_PlayVoiceOverEnter",
        {idAIScriptEventArg::Entity(this),
         idAIScriptEventArg::Decl(voiceOver),
         idAIScriptEventArg::Integer(waitUntilFinished != AIACTION_FALSE)});
  } else {
    if (voiceOver != nullptr) {
      idLib::Warning(
          "Incorrect decl type passed to event %s. Expected a %s but got a %s.",
          "idAI2::Action_PlayVoiceOver", "voiceover",
          voiceOver->declTypeName);
    } else {
      idLib::Warning("NULL decl passed from script.");
    }
  }
  return result;
}

// DIA: idAI2::Action_PlayInteractionVoiceOver
eventVoid* idAI2::Action_PlayInteractionVoiceOver(
    eventVoid* result, const idDeclVoiceOver* voiceOver,
    const idEntity* trackEntity, float abortDistance, bool abortScript) {
  if (voiceOver != nullptr && voiceOver->resourceValid) {
    scriptProxy.blocking = true;
    scriptProxy.ProcessEvent(
        "ACTION_PlayInteractionVoiceOverEnter",
        {idAIScriptEventArg::Entity(this),
         idAIScriptEventArg::Decl(voiceOver),
         idAIScriptEventArg::Entity(trackEntity),
         idAIScriptEventArg::Float(abortDistance),
         idAIScriptEventArg::Integer(abortScript)});
  } else {
    if (voiceOver != nullptr) {
      idLib::Warning(
          "Incorrect decl type passed to event %s. Expected a %s but got a %s.",
          "idAI2::Action_PlayInteractionVoiceOver", "voiceover",
          voiceOver->declTypeName);
    } else {
      idLib::Warning("NULL decl passed from script.");
    }
  }
  return result;
}

// DIA: `idAI2::Action_PlayInteractionVoiceOver'::`1'::dtor$0
// DIA: `idAI2::Action_PlayInteractionVoiceOver'::`1'::dtor$1
// DIA: `idAI2::Action_PlayInteractionVoiceOver'::`1'::dtor$2
// DIA: `idAI2::Action_PlayInteractionVoiceOver'::`1'::dtor$3
// DIA: idAI2::Action_PlayOverrideAnim
eventVoid* idAI2::Action_PlayOverrideAnim(eventVoid* result, int overrideAnim,
                                          aiActionBool_t cycle) {
  (void)cycle;
  scriptProxy.blocking = true;
  scriptProxy.ProcessEvent(
      "ACTION_PlayOverrideAnimEnter",
      {idAIScriptEventArg::Entity(this),
       idAIScriptEventArg::Integer(overrideAnim),
       idAIScriptEventArg::Integer(0)});
  return result;
}

// DIA: idAI2::Action_PlayOverrideAnimInterrupt
eventVoid* idAI2::Action_PlayOverrideAnimInterrupt(eventVoid* result,
                                                   int overrideAnim,
                                                   aiActionBool_t cycle) {
  (void)cycle;
  scriptProxy.blocking = true;
  scriptProxy.ProcessEvent(
      "ACTION_PlayOverrideAnimInterruptEnter",
      {idAIScriptEventArg::Entity(this),
       idAIScriptEventArg::Integer(overrideAnim),
       idAIScriptEventArg::Integer(1), idAIScriptEventArg::Integer(0)});
  return result;
}

// DIA: idAI2::Action_HolsterWeapon
eventVoid* idAI2::Action_HolsterWeapon(eventVoid* result) {
  scriptProxy.blocking = true;
  scriptProxy.ProcessEvent("ACTION_HolsterWeaponEnter",
                           {idAIScriptEventArg::Entity(this)});
  return result;
}

// DIA: idAI2::Action_DrawWeapon
eventVoid* idAI2::Action_DrawWeapon(eventVoid* result) {
  scriptProxy.blocking = true;
  scriptProxy.ProcessEvent("ACTION_DrawWeaponEnter",
                           {idAIScriptEventArg::Entity(this)});
  return result;
}

// DIA: idAI2::Action_PerformCoverAction
eventVoid* idAI2::Action_PerformCoverAction(eventVoid* result,
                                            int coverAction, int duration) {
  scriptProxy.blocking = true;
  scriptProxy.ProcessEvent(
      "ACTION_PerformCoverActionEnter",
      {idAIScriptEventArg::Entity(this),
       idAIScriptEventArg::Integer(coverAction),
       idAIScriptEventArg::Integer(duration)});
  return result;
}

// DIA: idAI2::Action_WaitForPlayerInteraction
eventVoid* idAI2::Action_WaitForPlayerInteraction(eventVoid* result) {
  scriptProxy.blocking = true;
  scriptProxy.ProcessEvent(
      "ACTION_WaitForPlayerInteractionEnter",
      {idAIScriptEventArg::Entity(this), idAIScriptEventArg::Float(0.0F)});
  return result;
}

// DIA: idAI2::Action_WaitForPlayerInteractionDist
eventVoid* idAI2::Action_WaitForPlayerInteractionDist(eventVoid* result,
                                                      float maxDistance) {
  scriptProxy.blocking = true;
  scriptProxy.ProcessEvent(
      "ACTION_WaitForPlayerInteractionEnter",
      {idAIScriptEventArg::Entity(this),
       idAIScriptEventArg::Float(maxDistance)});
  return result;
}

// DIA: idAI2::Action_WaitForAIVar
eventVoid* idAI2::Action_WaitForAIVar(eventVoid* result, const char* name,
                                      int op, const char* value) {
  scriptProxy.blocking = true;
  scriptProxy.ProcessEvent(
      "ACTION_WaitForAIVarEnter",
      {idAIScriptEventArg::Entity(this), idAIScriptEventArg::String(name),
       idAIScriptEventArg::Integer(op), idAIScriptEventArg::String(value)});
  return result;
}

// DIA: idAI2::Action_SetAlertCycle
eventVoid* idAI2::Action_SetAlertCycle(eventVoid* result, int alertCycle) {
  scriptProxy.blocking = false;
  scriptProxy.ProcessEvent(
      "ACTION_SetAlertCycleEnter",
      {idAIScriptEventArg::Entity(this),
       idAIScriptEventArg::Integer(alertCycle)});
  return result;
}

// DIA: idAI2::Action_SetSubWeb
eventVoid* idAI2::Action_SetSubWeb(eventVoid* result, int subWeb) {
  scriptProxy.blocking = false;
  scriptProxy.ProcessEvent(
      "ACTION_SetSubWebEnter",
      {idAIScriptEventArg::Entity(this), idAIScriptEventArg::Integer(subWeb)});
  return result;
}

// DIA: idAI2::Action_SetFocus
eventVoid* idAI2::Action_SetFocus(eventVoid* result, const idEntity* ent,
                                  aiActionBool_t aiming,
                                  aiActionBool_t keepFocusInView,
                                  aiActionBool_t trackOnlyVisible,
                                  aimPoint_t aimPoint) {
  scriptProxy.blocking = false;
  scriptProxy.ProcessEvent(
      "ACTION_SetFocusEnter",
      {idAIScriptEventArg::Entity(this), idAIScriptEventArg::Entity(ent),
       idAIScriptEventArg::Integer(aiming != AIACTION_FALSE),
       idAIScriptEventArg::Integer(keepFocusInView != AIACTION_FALSE),
       idAIScriptEventArg::Integer(trackOnlyVisible != AIACTION_FALSE),
       idAIScriptEventArg::Integer(aimPoint)});
  return result;
}

// DIA: `idAI2::Action_SetFocus'::`1'::dtor$0
// DIA: `idAI2::Action_SetFocus'::`1'::dtor$1
// DIA: `idAI2::Action_SetFocus'::`1'::dtor$2
// DIA: `idAI2::Action_SetFocus'::`1'::dtor$3
// DIA: `idAI2::Action_SetFocus'::`1'::dtor$4
// DIA: idAI2::Action_ClearLookFocus
eventVoid* idAI2::Action_ClearLookFocus(eventVoid* result) {
  scriptProxy.blocking = false;
  scriptProxy.ProcessEvent("ACTION_ClearLookFocusEnter",
                           {idAIScriptEventArg::Entity(this)});
  return result;
}

// DIA: idAI2::Action_ClearAimFocus
eventVoid* idAI2::Action_ClearAimFocus(eventVoid* result) {
  scriptProxy.blocking = false;
  scriptProxy.ProcessEvent("ACTION_ClearAimFocusEnter",
                           {idAIScriptEventArg::Entity(this)});
  return result;
}

// DIA: idAI2::Action_SetAimPoint
eventVoid* idAI2::Action_SetAimPoint(eventVoid* result, int aimPoint) {
  scriptProxy.blocking = false;
  scriptProxy.ProcessEvent(
      "ACTION_SetAimPointEnter",
      {idAIScriptEventArg::Entity(this),
       idAIScriptEventArg::Integer(aimPoint)});
  return result;
}

// DIA: idAI2::Action_SetFireMode
eventVoid* idAI2::Action_SetFireMode(eventVoid* result, int fireMode) {
  scriptProxy.blocking = false;
  scriptProxy.ProcessEvent(
      "ACTION_SetFireModeEnter",
      {idAIScriptEventArg::Entity(this),
       idAIScriptEventArg::Integer(fireMode)});
  return result;
}

// DIA: idAI2::Action_SetScriptAbort
eventVoid* idAI2::Action_SetScriptAbort(eventVoid* result,
                                        aiActionBool_t abortOnCombat,
                                        aiActionBool_t abortOnSearch) {
  scriptProxy.blocking = false;
  scriptProxy.ProcessEvent(
      "ACTION_SetScriptAbortEnter",
      {idAIScriptEventArg::Entity(this),
       idAIScriptEventArg::Integer(abortOnCombat == AIACTION_TRUE),
       idAIScriptEventArg::Integer(abortOnSearch == AIACTION_TRUE)});
  return result;
}

// DIA: idAI2::Action_SetPosture
eventVoid* idAI2::Action_SetPosture(eventVoid* result, int state) {
  scriptProxy.blocking = false;
  scriptProxy.ProcessEvent(
      "ACTION_SetPostureEnter",
      {idAIScriptEventArg::Entity(this), idAIScriptEventArg::Integer(state)});
  return result;
}

// DIA: idAI2::Action_SetWalkState
eventVoid* idAI2::Action_SetWalkState(eventVoid* result, int state) {
  scriptProxy.blocking = false;
  scriptProxy.ProcessEvent(
      "ACTION_SetWalkStateEnter",
      {idAIScriptEventArg::Entity(this), idAIScriptEventArg::Integer(state)});
  return result;
}

// DIA: idAI2::Action_SetSitState
eventVoid* idAI2::Action_SetSitState(eventVoid* result, int state) {
  scriptProxy.blocking = false;
  scriptProxy.ProcessEvent(
      "ACTION_SetSitStateEnter",
      {idAIScriptEventArg::Entity(this), idAIScriptEventArg::Integer(state)});
  return result;
}

// DIA: idAI2::Action_SetStandState
eventVoid* idAI2::Action_SetStandState(eventVoid* result, int state) {
  scriptProxy.blocking = false;
  scriptProxy.ProcessEvent(
      "ACTION_SetStandStateEnter",
      {idAIScriptEventArg::Entity(this), idAIScriptEventArg::Integer(state)});
  return result;
}

// DIA: idAI2::Action_SetIdleState
eventVoid* idAI2::Action_SetIdleState(eventVoid* result, int indexType) {
  scriptProxy.blocking = false;
  scriptProxy.ProcessEvent(
      "ACTION_SetIdleStateEnter",
      {idAIScriptEventArg::Entity(this),
       idAIScriptEventArg::Integer(indexType)});
  return result;
}

// DIA: idAI2::Action_IgnorePlayerApproach
eventVoid* idAI2::Action_IgnorePlayerApproach(eventVoid* result,
                                              aiActionBool_t permanent) {
  scriptProxy.blocking = false;
  scriptProxy.ProcessEvent(
      "ACTION_IgnorePlayerApproachEnter",
      {idAIScriptEventArg::Entity(this),
       idAIScriptEventArg::Integer(permanent == AIACTION_TRUE)});
  return result;
}

// DIA: idAI2::Action_NoticePlayerApproach
eventVoid* idAI2::Action_NoticePlayerApproach(eventVoid* result,
                                              aiActionBool_t permanent) {
  scriptProxy.blocking = false;
  scriptProxy.ProcessEvent(
      "ACTION_NoticePlayerApproachEnter",
      {idAIScriptEventArg::Entity(this),
       idAIScriptEventArg::Integer(permanent == AIACTION_TRUE)});
  return result;
}

// DIA: idAI2::Action_GiveItem
eventVoid* idAI2::Action_GiveItem(eventVoid* result, const idEntity* giveTo,
                                  const idDeclInventory* itemDecl) {
  if (itemDecl != nullptr && itemDecl->resourceValid) {
    scriptProxy.blocking = false;
    scriptProxy.ProcessEvent(
        "ACTION_GiveItemEnter",
        {idAIScriptEventArg::Entity(this), idAIScriptEventArg::Entity(giveTo),
         idAIScriptEventArg::Decl(itemDecl)});
  } else {
    if (itemDecl != nullptr) {
      idLib::Warning(
          "Incorrect decl type passed to event %s. Expected a %s but got a %s.",
          "idAI2::Action_GiveItem", "inventoryItem",
          itemDecl->declTypeName);
    } else {
      idLib::Warning("NULL decl passed from script.");
    }
  }
  return result;
}

// DIA: idAI2::Action_TakeItem
eventVoid* idAI2::Action_TakeItem(eventVoid* result, const idEntity* takeFrom,
                                  const idDeclInventory* itemDecl) {
  if (itemDecl != nullptr && itemDecl->resourceValid) {
    scriptProxy.blocking = false;
    scriptProxy.ProcessEvent(
        "ACTION_TakeItemEnter",
        {idAIScriptEventArg::Entity(this),
         idAIScriptEventArg::Entity(takeFrom),
         idAIScriptEventArg::Decl(itemDecl)});
  } else {
    if (itemDecl != nullptr) {
      idLib::Warning(
          "Incorrect decl type passed to event %s. Expected a %s but got a %s.",
          "idAI2::Action_TakeItem", "inventoryItem",
          itemDecl->declTypeName);
    } else {
      idLib::Warning("NULL decl passed from script.");
    }
  }
  return result;
}

// DIA: idAI2::GetPlayerTarget
idPlayer* idAI2::GetPlayerTarget(aiPlayer_t player) {
  const idVec3& origin = GetPhysics()->GetOrigin();
  idPlayer* target = nullptr;

  if (player == AIPLAYER_CLOSEST) {
    float bestDistance = std::numeric_limits<float>::max();
    for (int index = 0; index < 16; ++index) {
      idPlayer* candidate = gameLocal->GetPlayer(index);
      if (candidate != nullptr && !candidate->IsSpectating()) {
        const float distance =
            (candidate->GetPhysics()->GetOrigin() - origin).LengthSqr();
        if (bestDistance > distance) {
          target = candidate;
          bestDistance = distance;
        }
      }
    }
  } else if (player == AIPLAYER_FARTHEST) {
    float bestDistance = 0.0F;
    for (int index = 0; index < 16; ++index) {
      idPlayer* candidate = gameLocal->GetPlayer(index);
      if (candidate != nullptr && !candidate->IsSpectating()) {
        const float distance =
            (candidate->GetPhysics()->GetOrigin() - origin).LengthSqr();
        if (distance > bestDistance) {
          target = candidate;
          bestDistance = distance;
        }
      }
    }
  } else if (player == AIPLAYER_LOWEST_HEALTH) {
    float bestHealth = std::numeric_limits<float>::max();
    for (int index = 0; index < 16; ++index) {
      idPlayer* candidate = gameLocal->GetPlayer(index);
      if (candidate != nullptr && !candidate->IsSpectating() &&
          bestHealth > candidate->GetTotalCurHealth()) {
        target = candidate;
        bestHealth = candidate->GetTotalCurHealth();
      }
    }
  } else if (player == AIPLAYER_HIGHEST_HEALTH) {
    float bestHealth = 0.0F;
    for (int index = 0; index < 16; ++index) {
      idPlayer* candidate = gameLocal->GetPlayer(index);
      if (candidate != nullptr && !candidate->IsSpectating() &&
          candidate->GetTotalCurHealth() > bestHealth) {
        target = candidate;
        bestHealth = candidate->GetTotalCurHealth();
      }
    }
  }

  return target;
}

// DIA: idAI2::Action_SetPlayerEnemy
eventVoid* idAI2::Action_SetPlayerEnemy(eventVoid* result, int player) {
  scriptProxy.blocking = false;
  scriptProxy.ProcessEvent(
      "ACTION_SetPlayerEnemyEnter",
      {idAIScriptEventArg::Entity(this), idAIScriptEventArg::Integer(player)});
  return result;
}

// DIA: idAI2::Action_SetEnemy
eventVoid* idAI2::Action_SetEnemy(eventVoid* result,
                                  const idEntity* entity) {
  scriptProxy.blocking = false;
  scriptProxy.ProcessEvent(
      "ACTION_SetEnemyEnter",
      {idAIScriptEventArg::Entity(this), idAIScriptEventArg::Entity(entity)});
  return result;
}

// DIA: idAI2::Action_SearchToTarget
eventVoid* idAI2::Action_SearchToTarget(eventVoid* result,
                                        const idEntity* target) {
  scriptProxy.blocking = false;
  scriptProxy.ProcessEvent(
      "ACTION_SearchToTargetEnter",
      {idAIScriptEventArg::Entity(this), idAIScriptEventArg::Entity(target)});
  return result;
}

// DIA: idAI2::Action_SetPlayerFocus
eventVoid* idAI2::Action_SetPlayerFocus(eventVoid* result, int player,
                                        aiActionBool_t aiming,
                                        aiActionBool_t keepFocusInView) {
  scriptProxy.blocking = false;
  scriptProxy.ProcessEvent(
      "ACTION_SetPlayerFocusEnter",
      {idAIScriptEventArg::Entity(this), idAIScriptEventArg::Integer(player),
       idAIScriptEventArg::Integer(aiming != AIACTION_FALSE),
       idAIScriptEventArg::Integer(keepFocusInView != AIACTION_FALSE),
       idAIScriptEventArg::Integer(1), idAIScriptEventArg::Integer(1)});
  return result;
}

// DIA: `idAI2::Action_SetPlayerFocus'::`1'::dtor$0
// DIA: `idAI2::Action_SetPlayerFocus'::`1'::dtor$1
// DIA: `idAI2::Action_SetPlayerFocus'::`1'::dtor$2
// DIA: `idAI2::Action_SetPlayerFocus'::`1'::dtor$3
// DIA: `idAI2::Action_SetPlayerFocus'::`1'::dtor$4
// DIA: idAI2::Action_SetActionNodeGroup
eventVoid* idAI2::Action_SetActionNodeGroup(eventVoid* result,
                                            const char* group) {
  scriptProxy.blocking = false;
  scriptProxy.ProcessEvent(
      "ACTION_SetActionNodeGroupEnter",
      {idAIScriptEventArg::Entity(this), idAIScriptEventArg::String(group)});
  return result;
}

// DIA: idAI2::Action_Trigger
eventVoid* idAI2::Action_Trigger(eventVoid* result, idEntity* entity) {
  scriptProxy.blocking = false;
  scriptProxy.ProcessEvent(
      "ACTION_TriggerEnter",
      {idAIScriptEventArg::Entity(this), idAIScriptEventArg::Entity(entity)});
  return result;
}

// DIA: idAI2::Action_EnableDamage
eventVoid* idAI2::Action_EnableDamage(eventVoid* result,
                                      aiActionBool_t enabled,
                                      aiActionBool_t permanent) {
  scriptProxy.blocking = false;
  scriptProxy.ProcessEvent(
      "ACTION_EnableDamageEnter",
      {idAIScriptEventArg::Entity(this),
       idAIScriptEventArg::Integer(enabled != AIACTION_FALSE),
       idAIScriptEventArg::Integer(permanent != AIACTION_FALSE)});
  return result;
}

// DIA: idAI2::Action_EnableAutoFocus
eventVoid* idAI2::Action_EnableAutoFocus(eventVoid* result,
                                         aiActionBool_t enabled,
                                         aiActionBool_t permanent) {
  scriptProxy.blocking = false;
  scriptProxy.ProcessEvent(
      "ACTION_EnableAutoFocusEnter",
      {idAIScriptEventArg::Entity(this),
       idAIScriptEventArg::Integer(enabled != AIACTION_FALSE),
       idAIScriptEventArg::Integer(permanent != AIACTION_FALSE)});
  return result;
}

// DIA: idAI2::Action_EnableBodyRotation
eventVoid* idAI2::Action_EnableBodyRotation(eventVoid* result,
                                            aiActionBool_t enabled) {
  scriptProxy.blocking = false;
  scriptProxy.ProcessEvent(
      "ACTION_EnableBodyRotationEnter",
      {idAIScriptEventArg::Entity(this),
       idAIScriptEventArg::Integer(enabled != AIACTION_FALSE)});
  return result;
}

// DIA: idAI2::Action_EnableWalkIK
eventVoid* idAI2::Action_EnableWalkIK(eventVoid* result,
                                      aiActionBool_t enabled) {
  scriptProxy.blocking = false;
  scriptProxy.ProcessEvent(
      "ACTION_EnableWalkIKEnter",
      {idAIScriptEventArg::Entity(this),
       idAIScriptEventArg::Integer(enabled != AIACTION_FALSE)});
  return result;
}

// DIA: idAI2::Action_EnableHeadTracking
eventVoid* idAI2::Action_EnableHeadTracking(eventVoid* result,
                                            aiActionBool_t enabled) {
  scriptProxy.blocking = false;
  scriptProxy.ProcessEvent(
      "ACTION_EnableHeadTrackingEnter",
      {idAIScriptEventArg::Entity(this),
       idAIScriptEventArg::Integer(enabled != AIACTION_FALSE)});
  return result;
}

// DIA: idAI2::Action_ForceAwarenessByDistance
eventVoid* idAI2::Action_ForceAwarenessByDistance(eventVoid* result,
                                                  float radius,
                                                  bool playerFlag,
                                                  bool aiFlag) {
  scriptProxy.blocking = false;
  scriptProxy.ProcessEvent(
      "ACTION_ForceAwarenessByDistanceEnter",
      {idAIScriptEventArg::Entity(this), idAIScriptEventArg::Float(radius),
       idAIScriptEventArg::Integer(playerFlag),
       idAIScriptEventArg::Integer(aiFlag)});
  return result;
}

// DIA: idAI2::Action_SetAIVar
eventVoid* idAI2::Action_SetAIVar(eventVoid* result, const char* name,
                                  int type, const char* value) {
  scriptProxy.blocking = false;
  scriptProxy.ProcessEvent(
      "ACTION_SetAIVarEnter",
      {idAIScriptEventArg::Entity(this), idAIScriptEventArg::String(name),
       idAIScriptEventArg::Integer(type), idAIScriptEventArg::String(value)});
  return result;
}

// DIA: idAI2::Action_SetScriptFlag
eventVoid* idAI2::Action_SetScriptFlag(eventVoid* result, int flag) {
  scriptProxy.blocking = false;
  scriptProxy.ProcessEvent(
      "ACTION_SetScriptFlagEnter",
      {idAIScriptEventArg::Entity(this), idAIScriptEventArg::Integer(flag)});
  return result;
}

// DIA: idAI2::Action_ClearScriptFlag
eventVoid* idAI2::Action_ClearScriptFlag(eventVoid* result, int flag) {
  scriptProxy.blocking = false;
  scriptProxy.ProcessEvent(
      "ACTION_ClearScriptFlagEnter",
      {idAIScriptEventArg::Entity(this), idAIScriptEventArg::Integer(flag)});
  return result;
}

// DIA: idAI2::Action_SetPerceptionFlag
eventVoid* idAI2::Action_SetPerceptionFlag(eventVoid* result, int flag) {
  scriptProxy.blocking = false;
  scriptProxy.ProcessEvent(
      "ACTION_SetPerceptionFlagEnter",
      {idAIScriptEventArg::Entity(this), idAIScriptEventArg::Integer(flag)});
  return result;
}

// DIA: idAI2::Action_ClearPerceptionFlag
eventVoid* idAI2::Action_ClearPerceptionFlag(eventVoid* result, int flag) {
  scriptProxy.blocking = false;
  scriptProxy.ProcessEvent(
      "ACTION_ClearPerceptionFlagEnter",
      {idAIScriptEventArg::Entity(this), idAIScriptEventArg::Integer(flag)});
  return result;
}

// DIA: idAI2::Action_ClearWorldState
eventVoid* idAI2::Action_ClearWorldState(eventVoid* result) {
  scriptProxy.blocking = false;
  scriptProxy.ProcessEvent("ACTION_ClearWorldStateEnter",
                           {idAIScriptEventArg::Entity(this)});
  return result;
}

// DIA: idAI2::Action_ShowAttachment
eventVoid* idAI2::Action_ShowAttachment(eventVoid* result,
                                        const char* slotName) {
  scriptProxy.blocking = false;
  scriptProxy.ProcessEvent(
      "ACTION_ShowAttachmentEnter",
      {idAIScriptEventArg::Entity(this),
       idAIScriptEventArg::String(slotName)});
  return result;
}

// DIA: idAI2::Action_HideAttachment
eventVoid* idAI2::Action_HideAttachment(eventVoid* result,
                                        const char* slotName) {
  scriptProxy.blocking = false;
  scriptProxy.ProcessEvent(
      "ACTION_HideAttachmentEnter",
      {idAIScriptEventArg::Entity(this),
       idAIScriptEventArg::String(slotName)});
  return result;
}

// DIA: idAI2::Action_DropAttachment
eventVoid* idAI2::Action_DropAttachment(eventVoid* result,
                                        const char* slotName) {
  scriptProxy.blocking = false;
  scriptProxy.ProcessEvent(
      "ACTION_DropAttachmentEnter",
      {idAIScriptEventArg::Entity(this),
       idAIScriptEventArg::String(slotName)});
  return result;
}

// DIA: idAI2::Action_SetMoveMode
eventVoid* idAI2::Action_SetMoveMode(eventVoid* result, int moveMode) {
  scriptProxy.blocking = false;
  scriptProxy.ProcessEvent(
      "ACTION_SetMoveModeEnter",
      {idAIScriptEventArg::Entity(this),
       idAIScriptEventArg::Integer(moveMode)});
  return result;
}

// DIA: idAI2::Action_SetAccuracy
eventVoid* idAI2::Action_SetAccuracy(eventVoid* result, int accuracy) {
  scriptProxy.blocking = false;
  scriptProxy.ProcessEvent(
      "ACTION_SetAccuracyEnter",
      {idAIScriptEventArg::Entity(this),
       idAIScriptEventArg::Integer(accuracy)});
  return result;
}

// DIA: idAI2::Action_ScriptedAnimWeb
eventVoid* idAI2::Action_ScriptedAnimWeb(eventVoid* result,
                                         const idEntity* scriptedAnimWeb,
                                         int loopInterval) {
  scriptProxy.blocking = false;
  scriptProxy.ProcessEvent(
      "ACTION_ScriptedAnimWebEnter",
      {idAIScriptEventArg::Entity(this),
       idAIScriptEventArg::Entity(scriptedAnimWeb),
       idAIScriptEventArg::Integer(loopInterval)});
  return result;
}

// DIA: idAI2::Action_ReloadWeapon
eventVoid* idAI2::Action_ReloadWeapon(eventVoid* result, unsigned int slot,
                                      const char* ammoDeclName) {
  idWeapon* equippedWeapon = GetEquippedWeapon(slot);
  if (equippedWeapon != nullptr) {
    const idDeclAmmo* ammo =
        idDeclAmmo::FindWithInheritance(ammoDeclName);
    if (ammoDeclName != nullptr && ammo != nullptr) {
      if (equippedWeapon->CanReload(GetInventoryCollection(), ammo, false)) {
        scriptProxy.blocking = true;
        scriptProxy.ProcessEvent(
            "ACTION_ReloadWeaponEnter",
            {idAIScriptEventArg::Entity(this),
             idAIScriptEventArg::Integer(static_cast<int>(slot)),
             idAIScriptEventArg::Decl(ammo)});
      }
    } else {
      idLib::Warning("idAI2::Action_ReloadWeapon - unknown ammo decl '%s'",
                     ammoDeclName);
    }
  }
  return result;
}

// DIA: idAI2::Action_ReloadWeaponTorso
eventVoid* idAI2::Action_ReloadWeaponTorso(eventVoid* result,
                                           unsigned int slot,
                                           const char* ammoDeclName) {
  idLib::Warning("idAI2::Action_ReloadWeaponTorso is called");
  idWeapon* equippedWeapon = GetEquippedWeapon(slot);
  if (equippedWeapon != nullptr) {
    const idDeclAmmo* ammo =
        idDeclAmmo::FindWithInheritance(ammoDeclName);
    if (ammoDeclName != nullptr && ammo != nullptr) {
      if (equippedWeapon->CanReload(GetInventoryCollection(), ammo, false)) {
        scriptProxy.blocking = true;
        scriptProxy.ProcessEvent(
            "ACTION_ReloadWeaponTorsoEnter",
            {idAIScriptEventArg::Entity(this),
             idAIScriptEventArg::Integer(static_cast<int>(slot)),
             idAIScriptEventArg::Decl(ammo)});
      }
    } else {
      idLib::Warning(
          "idAI2::Action_ReloadWeaponTorso - unknown ammo decl '%s'",
          ammoDeclName);
    }
  }
  return result;
}
