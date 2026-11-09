#include "tungsten/game/ai/ai2_actions.h"

#include <string_view>

idCVar ai_cookGrenades(
    "ai_cookGrenades", "0", 2, "1 = cook grenades when throwing them");

eventVoid* idAI2::RecordAnimEvent(const char* eventName, eventVoid* result) {
  const std::string_view eventKey = eventName != nullptr ? eventName : "";
  animEventState.lastEvent.assign(eventKey.data(), eventKey.size());
  ++animEventState.sequence;

  if (eventKey == "Event_EnableAutoFocus") animEventState.autoFocus = true;
  if (eventKey == "AnimEvent_ShowFakeGrenade") animEventState.fakeGrenadeVisible = true;
  if (eventKey == "AnimEvent_HideFakeGrenade" ||
      eventKey == "AnimEvent_DetonateFakeGrenade") {
    animEventState.fakeGrenadeVisible = false;
  }
  if (eventKey == "AnimEvent_EnableHeadTracking") animEventState.headTracking = true;
  if (eventKey == "AnimEvent_DisableHeadTracking") animEventState.headTracking = false;
  if (eventKey == "AnimEvent_EnableEyeTracking") animEventState.eyeTracking = true;
  if (eventKey == "AnimEvent_DisableEyeTracking") animEventState.eyeTracking = false;
  if (eventKey == "AnimEvent_EnableWorldCollision") animEventState.worldCollision = true;
  if (eventKey == "AnimEvent_DisableWorldCollision") animEventState.worldCollision = false;
  if (eventKey == "AnimEvent_EnableSolidCollision") animEventState.solidCollision = true;
  if (eventKey == "AnimEvent_DisableSolidCollision") animEventState.solidCollision = false;
  if (eventKey == "AnimEvent_EnableDamage") animEventState.damageEnabled = true;
  if (eventKey == "AnimEvent_DisableDamage") animEventState.damageEnabled = false;
  if (eventKey == "AnimEvent_EnableExtendedClipModel") animEventState.extendedClipModel = true;
  if (eventKey == "AnimEvent_DisableExtendedClipModel") animEventState.extendedClipModel = false;
  if (eventKey == "AnimEvent_EnableIK" || eventKey == "AnimEvent_EnableTwoLegIK" ||
      eventKey == "AnimEvent_EnableFourLegIK" || eventKey == "AnimEvent_EnableLegIK") {
    animEventState.ikEnabled = true;
  }
  if (eventKey == "AnimEvent_DisableIK" || eventKey == "AnimEvent_DisableTwoLegIK" ||
      eventKey == "AnimEvent_DisableFourLegIK" || eventKey == "AnimEvent_DisableLegIK" ||
      eventKey == "AnimEvent_DisableIKAnimDuration") {
    animEventState.ikEnabled = false;
  }
  if (eventKey == "AnimEvent_EnableBlinking") animEventState.blinking = true;
  if (eventKey == "AnimEvent_DisableBlinking") animEventState.blinking = false;
  if (eventKey == "AnimEvent_EnableLidDeformation") animEventState.lidDeformation = true;
  if (eventKey == "AnimEvent_DisableLidDeformation") animEventState.lidDeformation = false;
  if (eventKey == "AnimEvent_EnableStaggeringPain") animEventState.staggeringPain = true;
  if (eventKey == "AnimEvent_DisableStaggeringPain") animEventState.staggeringPain = false;
  if (eventKey == "AnimEvent_EnableDeathAnimations") animEventState.deathAnimations = true;
  if (eventKey == "AnimEvent_DisableDeathAnimations") animEventState.deathAnimations = false;
  if (eventKey == "AnimEvent_EnableCanRagdoll") animEventState.canRagdoll = true;
  if (eventKey == "AnimEvent_DisableCanRagdoll") animEventState.canRagdoll = false;
  if (eventKey == "AnimEvent_StartRagdoll") animEventState.ragdollActive = true;
  if (eventKey == "AnimEvent_StartMeleeTrace" || eventKey == "AnimEvent_StartMeleeTrace2" ||
      eventKey == "AnimEvent_StartMeleeTraceTag" || eventKey == "AnimEvent_StartMeleeTraceTag2") {
    animEventState.meleeTraceActive = true;
  }
  if (eventKey == "AnimEvent_EndMeleeTrace") animEventState.meleeTraceActive = false;
  if (eventKey == "AnimEvent_StartJetPackFX") animEventState.jetPackFxActive = true;
  if (eventKey == "AnimEvent_StopJetPackFX" || eventKey == "AnimEvent_DropJetPack") {
    animEventState.jetPackFxActive = false;
  }
  if (eventKey == "AnimEvent_StartWeaponFX") animEventState.weaponFxActive = true;
  if (eventKey == "AnimEvent_StopWeaponFX") animEventState.weaponFxActive = false;
  if (eventKey == "AnimEvent_StartAutoLipsync") animEventState.autoLipsync = true;
  if (eventKey == "AnimEvent_StopAutoLipsync") animEventState.autoLipsync = false;
  if (eventKey == "AnimEvent_VoiceOver" || eventKey == "AnimEvent_VoiceOverPriority" ||
      eventKey == "AnimEvent_PlayVoice") {
    voiceOverPlaying = true;
  }
  if (eventKey == "AnimEvent_ForceAllowLooting") animEventState.allowLooting = true;
  if (eventKey == "AnimEvent_Kill" || eventKey == "AnimEvent_Death") {
    animEventState.health = 0;
    animEventState.dead = true;
  }

  if (result != nullptr) {
    result->accepted = true;
    result->completed = true;
  }
  return result;
}

#define PORT_AI_ANIM_EVENT(methodName)                                      \
  eventVoid* idAI2::methodName(eventVoid* result, ...) {                   \
    return RecordAnimEvent(#methodName, result);                           \
  }

// DIA: idAI2::Event_EnableAutoFocus
eventVoid* idAI2::Event_EnableAutoFocus(eventVoid* result,
                                        const idAIAnimNodePlayback* anim) {
  (void)anim;
  return RecordAnimEvent("Event_EnableAutoFocus", result);
}

// DIA: idAI2::SetupMeleeTraceBounds
std::int64_t idAI2::SetupMeleeTraceBounds(float radius) {
  if (radius == 8.0F) animEventState.meleeTraceBounds = 1;
  else if (radius == 16.0F) animEventState.meleeTraceBounds = 2;
  else if (radius == 24.0F) animEventState.meleeTraceBounds = 3;
  else if (radius == 32.0F) animEventState.meleeTraceBounds = 4;
  else if (radius == 48.0F) animEventState.meleeTraceBounds = 5;
  else if (radius == 96.0F) animEventState.meleeTraceBounds = 6;
  else animEventState.meleeTraceBounds = 8;
  return animEventState.meleeTraceBounds;
}

// DIA: idAI2::AnimEvent_TriggerGore
PORT_AI_ANIM_EVENT(AnimEvent_TriggerGore)
// DIA: idAI2::AnimEvent_BlendRenderParameter
PORT_AI_ANIM_EVENT(AnimEvent_BlendRenderParameter)
// DIA: idAI2::AnimEvent_DropAttachedProjectiles
PORT_AI_ANIM_EVENT(AnimEvent_DropAttachedProjectiles)
// DIA: idAI2::AnimEvent_HideFakeGrenade
PORT_AI_ANIM_EVENT(AnimEvent_HideFakeGrenade)
// DIA: idAI2::AnimEvent_ShowFakeGrenade
PORT_AI_ANIM_EVENT(AnimEvent_ShowFakeGrenade)
// DIA: idAI2::AnimEvent_DeathAnimImpulse
PORT_AI_ANIM_EVENT(AnimEvent_DeathAnimImpulse)
// DIA: idAI2::AnimEvent_DeathTrace
PORT_AI_ANIM_EVENT(AnimEvent_DeathTrace)
// DIA: idAI2::AnimEvent_DeathPhys
PORT_AI_ANIM_EVENT(AnimEvent_DeathPhys)
// DIA: idAI2::AnimEvent_SmoothAnimSetDone
PORT_AI_ANIM_EVENT(AnimEvent_SmoothAnimSetDone)
// DIA: idAI2::AnimEvent_SmoothAnimDestinationLerpStop
PORT_AI_ANIM_EVENT(AnimEvent_SmoothAnimDestinationLerpStop)
// DIA: idAI2::AnimEvent_UseAdditivePainAnims
PORT_AI_ANIM_EVENT(AnimEvent_UseAdditivePainAnims)
// DIA: idAI2::AnimEvent_AllowExecutions
PORT_AI_ANIM_EVENT(AnimEvent_AllowExecutions)
// DIA: idAI2::AnimEvent_AIReloadWeapon
PORT_AI_ANIM_EVENT(AnimEvent_AIReloadWeapon)
// DIA: idAI2::AnimEvent_AIFireWeapon
PORT_AI_ANIM_EVENT(AnimEvent_AIFireWeapon)
// DIA: idAI2::AnimEvent_AISetFireMode
PORT_AI_ANIM_EVENT(AnimEvent_AISetFireMode)
// DIA: idAI2::AnimEvent_AIInhibitFire
PORT_AI_ANIM_EVENT(AnimEvent_AIInhibitFire)
// DIA: idAI2::AnimEvent_Kill
PORT_AI_ANIM_EVENT(AnimEvent_Kill)
// DIA: idAI2::AnimEvent_EquipItem
PORT_AI_ANIM_EVENT(AnimEvent_EquipItem)
// DIA: idAI2::AnimEvent_HolsterItem
PORT_AI_ANIM_EVENT(AnimEvent_HolsterItem)
// DIA: idAI2::AnimEvent_DropAttachment
PORT_AI_ANIM_EVENT(AnimEvent_DropAttachment)
// DIA: idAI2::AnimEvent_MeleeCustom
PORT_AI_ANIM_EVENT(AnimEvent_MeleeCustom)
// DIA: idAI2::AnimEvent_MeleeCustomForceDirection
PORT_AI_ANIM_EVENT(AnimEvent_MeleeCustomForceDirection)
// DIA: idAI2::AnimEvent_MeleePunch
PORT_AI_ANIM_EVENT(AnimEvent_MeleePunch)
// DIA: idAI2::AnimEvent_MeleeKick
PORT_AI_ANIM_EVENT(AnimEvent_MeleeKick)
// DIA: idAI2::AnimEvent_ThrowGrenade
PORT_AI_ANIM_EVENT(AnimEvent_ThrowGrenade)
// DIA: idAI2::AnimEvent_EnableHeadTracking
PORT_AI_ANIM_EVENT(AnimEvent_EnableHeadTracking)
// DIA: idAI2::AnimEvent_DisableHeadTracking
PORT_AI_ANIM_EVENT(AnimEvent_DisableHeadTracking)
// DIA: idAI2::AnimEvent_SetLookFocusHeadTrackingGroup
PORT_AI_ANIM_EVENT(AnimEvent_SetLookFocusHeadTrackingGroup)
// DIA: idAI2::AnimEvent_EnableEyeTracking
PORT_AI_ANIM_EVENT(AnimEvent_EnableEyeTracking)
// DIA: idAI2::AnimEvent_DisableEyeTracking
PORT_AI_ANIM_EVENT(AnimEvent_DisableEyeTracking)
// DIA: idAI2::AnimEvent_OverrideAnimOrientationWithFocus
PORT_AI_ANIM_EVENT(AnimEvent_OverrideAnimOrientationWithFocus)
// DIA: idAI2::AnimEvent_VoiceOver
PORT_AI_ANIM_EVENT(AnimEvent_VoiceOver)
// DIA: idAI2::AnimEvent_VoiceOverPriority
PORT_AI_ANIM_EVENT(AnimEvent_VoiceOverPriority)
// DIA: idAI2::AnimEvent_StopAutoLipsync
PORT_AI_ANIM_EVENT(AnimEvent_StopAutoLipsync)
// DIA: idAI2::AnimEvent_StartAutoLipsync
PORT_AI_ANIM_EVENT(AnimEvent_StartAutoLipsync)
// DIA: idAI2::AnimEvent_SetMovePushStatus
PORT_AI_ANIM_EVENT(AnimEvent_SetMovePushStatus)
// DIA: idAI2::AnimEvent_EnableWorldCollision
PORT_AI_ANIM_EVENT(AnimEvent_EnableWorldCollision)
// DIA: idAI2::AnimEvent_DisableWorldCollision
PORT_AI_ANIM_EVENT(AnimEvent_DisableWorldCollision)
// DIA: idAI2::AnimEvent_EnableSolidCollision
PORT_AI_ANIM_EVENT(AnimEvent_EnableSolidCollision)
// DIA: idAI2::AnimEvent_DisableSolidCollision
PORT_AI_ANIM_EVENT(AnimEvent_DisableSolidCollision)
// DIA: idAI2::AnimEvent_EnableDamage
PORT_AI_ANIM_EVENT(AnimEvent_EnableDamage)
// DIA: idAI2::AnimEvent_DisableDamage
PORT_AI_ANIM_EVENT(AnimEvent_DisableDamage)
// DIA: idAI2::AnimEvent_ClearReferenceJointOverride
PORT_AI_ANIM_EVENT(AnimEvent_ClearReferenceJointOverride)
// DIA: idAI2::AnimEvent_DisableExtendedClipModel
PORT_AI_ANIM_EVENT(AnimEvent_DisableExtendedClipModel)
// DIA: idAI2::AnimEvent_EnableExtendedClipModel
PORT_AI_ANIM_EVENT(AnimEvent_EnableExtendedClipModel)
// DIA: idAI2::AnimEvent_EnableIK
PORT_AI_ANIM_EVENT(AnimEvent_EnableIK)
// DIA: idAI2::AnimEvent_DisableIK
PORT_AI_ANIM_EVENT(AnimEvent_DisableIK)
// DIA: idAI2::AnimEvent_DisableIKAnimDuration
PORT_AI_ANIM_EVENT(AnimEvent_DisableIKAnimDuration)
// DIA: idAI2::AnimEvent_EnableLegIK
PORT_AI_ANIM_EVENT(AnimEvent_EnableLegIK)
// DIA: idAI2::AnimEvent_DisableLegIK
PORT_AI_ANIM_EVENT(AnimEvent_DisableLegIK)
// DIA: idAI2::AnimEvent_DisableTwoLegIK
PORT_AI_ANIM_EVENT(AnimEvent_DisableTwoLegIK)
// DIA: idAI2::AnimEvent_EnableTwoLegIK
PORT_AI_ANIM_EVENT(AnimEvent_EnableTwoLegIK)
// DIA: idAI2::AnimEvent_DisableFourLegIK
PORT_AI_ANIM_EVENT(AnimEvent_DisableFourLegIK)
// DIA: idAI2::AnimEvent_EnableFourLegIK
PORT_AI_ANIM_EVENT(AnimEvent_EnableFourLegIK)
// DIA: idAI2::AnimEvent_EndMeleeTrace
PORT_AI_ANIM_EVENT(AnimEvent_EndMeleeTrace)
// DIA: idAI2::AnimEvent_StartJetPackFX
PORT_AI_ANIM_EVENT(AnimEvent_StartJetPackFX)
// DIA: idAI2::AnimEvent_StopJetPackFX
PORT_AI_ANIM_EVENT(AnimEvent_StopJetPackFX)
// DIA: idAI2::AnimEvent_DropJetPack
PORT_AI_ANIM_EVENT(AnimEvent_DropJetPack)
// DIA: idAI2::AnimEvent_EnableBlinking
PORT_AI_ANIM_EVENT(AnimEvent_EnableBlinking)
// DIA: idAI2::AnimEvent_DisableBlinking
PORT_AI_ANIM_EVENT(AnimEvent_DisableBlinking)
// DIA: idAI2::AnimEvent_EnableLidDeformation
PORT_AI_ANIM_EVENT(AnimEvent_EnableLidDeformation)
// DIA: idAI2::AnimEvent_DisableLidDeformation
PORT_AI_ANIM_EVENT(AnimEvent_DisableLidDeformation)
// DIA: idAI2::AnimEvent_SetLookFocusOffset
PORT_AI_ANIM_EVENT(AnimEvent_SetLookFocusOffset)
// DIA: idAI2::AnimEvent_SetAimFocusOffset
PORT_AI_ANIM_EVENT(AnimEvent_SetAimFocusOffset)
// DIA: idAI2::AnimEvent_SetLookFocusRotationOffset
PORT_AI_ANIM_EVENT(AnimEvent_SetLookFocusRotationOffset)
// DIA: idAI2::AnimEvent_SetAimFocusRotationOffset
PORT_AI_ANIM_EVENT(AnimEvent_SetAimFocusRotationOffset)
// DIA: idAI2::AnimEvent_LockTracking
PORT_AI_ANIM_EVENT(AnimEvent_LockTracking)
// DIA: idAI2::AnimEvent_DisableStaggeringPain
PORT_AI_ANIM_EVENT(AnimEvent_DisableStaggeringPain)
// DIA: idAI2::AnimEvent_EnableStaggeringPain
PORT_AI_ANIM_EVENT(AnimEvent_EnableStaggeringPain)
// DIA: idAI2::AnimEvent_DisableDeathAnimations
PORT_AI_ANIM_EVENT(AnimEvent_DisableDeathAnimations)
// DIA: idAI2::AnimEvent_EnableDeathAnimations
PORT_AI_ANIM_EVENT(AnimEvent_EnableDeathAnimations)
// DIA: idAI2::AnimEvent_EnableCanRagdoll
PORT_AI_ANIM_EVENT(AnimEvent_EnableCanRagdoll)
// DIA: idAI2::AnimEvent_DisableCanRagdoll
PORT_AI_ANIM_EVENT(AnimEvent_DisableCanRagdoll)
// DIA: idAI2::AnimEvent_StartWeaponFX
PORT_AI_ANIM_EVENT(AnimEvent_StartWeaponFX)
// DIA: idAI2::AnimEvent_StopWeaponFX
PORT_AI_ANIM_EVENT(AnimEvent_StopWeaponFX)
// DIA: idAI2::AnimEvent_DeltaScaleToTarget
PORT_AI_ANIM_EVENT(AnimEvent_DeltaScaleToTarget)
// DIA: idAI2::AnimEvent_SetDeltaScale
PORT_AI_ANIM_EVENT(AnimEvent_SetDeltaScale)
// DIA: idAI2::AnimEvent_SetScriptDeltaScale
PORT_AI_ANIM_EVENT(AnimEvent_SetScriptDeltaScale)
// DIA: idAI2::AnimEvent_PopOffArmor
PORT_AI_ANIM_EVENT(AnimEvent_PopOffArmor)
// DIA: idAI2::AnimEvent_SetDefaultMoveMode
PORT_AI_ANIM_EVENT(AnimEvent_SetDefaultMoveMode)
// DIA: idAI2::AnimEvent_SetFullDeltaGravityMovementMode
PORT_AI_ANIM_EVENT(AnimEvent_SetFullDeltaGravityMovementMode)
// DIA: idAI2::AnimEvent_Death
PORT_AI_ANIM_EVENT(AnimEvent_Death)
// DIA: idAI2::AnimEvent_ForceAllowLooting
PORT_AI_ANIM_EVENT(AnimEvent_ForceAllowLooting)
// DIA: idAI2::AnimEvent_BeginDrop
PORT_AI_ANIM_EVENT(AnimEvent_BeginDrop)
// DIA: idAI2::AnimEvent_ParticleAtRottrack
PORT_AI_ANIM_EVENT(AnimEvent_ParticleAtRottrack)
// DIA: idAI2::AnimEvent_PlayVoice
PORT_AI_ANIM_EVENT(AnimEvent_PlayVoice)
// DIA: idAI2::AnimEvent_SetHealth
eventVoid* idAI2::AnimEvent_SetHealth(eventVoid* result, const idAIAnimNodePlayback* anim,
                                      int value) {
  (void)anim;
  animEventState.health = value;
  animEventState.dead = value <= 0;
  return RecordAnimEvent("AnimEvent_SetHealth", result);
}
// DIA: idAI2::AnimEvent_AnimEndEnterAnimation
PORT_AI_ANIM_EVENT(AnimEvent_AnimEndEnterAnimation)
// DIA: idAI2::AnimEvent_AnimReleaseWaitHandle
PORT_AI_ANIM_EVENT(AnimEvent_AnimReleaseWaitHandle)
// DIA: idAI2::AnimEvent_SetTakedownWindowState
PORT_AI_ANIM_EVENT(AnimEvent_SetTakedownWindowState)
// DIA: idAI2::AnimEvent_SoundHurt
PORT_AI_ANIM_EVENT(AnimEvent_SoundHurt)
// DIA: idAI2::AnimEvent_SoundDeath
PORT_AI_ANIM_EVENT(AnimEvent_SoundDeath)
// DIA: idAI2::AnimEvent_CustomDeathSound
PORT_AI_ANIM_EVENT(AnimEvent_CustomDeathSound)
// DIA: idAI2::AnimEvent_AnimationDone
PORT_AI_ANIM_EVENT(AnimEvent_AnimationDone)
// DIA: idAI2::AnimEvent_SetFade
eventVoid* idAI2::AnimEvent_SetFade(eventVoid* result, const idAIAnimNodePlayback* anim,
                                    float value) {
  (void)anim;
  animEventState.fade = value < 0.0F ? 0.0F : (value > 1.0F ? 1.0F : value);
  return RecordAnimEvent("AnimEvent_SetFade", result);
}
// DIA: idAI2::AnimEvent_StartRagdoll
PORT_AI_ANIM_EVENT(AnimEvent_StartRagdoll)

// DIA: idWeapon::GetCurrentDamageDecl
const idDeclDamage* idWeapon::GetCurrentDamageDecl() {
  return currentDamageDecl;
}

// DIA: idEntity::GetPresentableAxis
idMat3* idEntity::GetPresentableAxis() {
  return &presentableAxis;
}

// DIA: idAI2::AnimEvent_KillSyncedTarget
PORT_AI_ANIM_EVENT(AnimEvent_KillSyncedTarget)
// DIA: idAI2::AnimEvent_LaunchItem
PORT_AI_ANIM_EVENT(AnimEvent_LaunchItem)
// DIA: idAI2::AnimEvent_LaunchItemFromWeapon
PORT_AI_ANIM_EVENT(AnimEvent_LaunchItemFromWeapon)
// DIA: idAI2::AnimEvent_SetReferenceJointOverride
PORT_AI_ANIM_EVENT(AnimEvent_SetReferenceJointOverride)

// DIA: idAI2::GetMeleeTraceDamage
const idDeclDamage* idAI2::GetMeleeTraceDamage(
    const idDeclDamage* damageDecl) {
  if (damageDecl != nullptr) return damageDecl;
  return meleeWeapon != nullptr ? meleeWeapon->GetCurrentDamageDecl() : nullptr;
}

// DIA: idAI2::GetMeleeTraceImpactEffects
const idDeclProjectileImpactEffect* idAI2::GetMeleeTraceImpactEffects(
    bool useWeaponEffects) {
  (void)useWeaponEffects;
  return meleeImpactEffects;
}

// DIA: idAI2::AnimEvent_StartMeleeTrace
PORT_AI_ANIM_EVENT(AnimEvent_StartMeleeTrace)
// DIA: idAI2::AnimEvent_StartMeleeTrace2
PORT_AI_ANIM_EVENT(AnimEvent_StartMeleeTrace2)
// DIA: idAI2::AnimEvent_StartMeleeTraceTag
PORT_AI_ANIM_EVENT(AnimEvent_StartMeleeTraceTag)
// DIA: idAI2::AnimEvent_StartMeleeTraceTag2
PORT_AI_ANIM_EVENT(AnimEvent_StartMeleeTraceTag2)
// DIA: idAI2::AnimEvent_RandomRunCycleIndex
PORT_AI_ANIM_EVENT(AnimEvent_RandomRunCycleIndex)
// DIA: idAI2::AnimEvent_RandomRunCycleNormal
PORT_AI_ANIM_EVENT(AnimEvent_RandomRunCycleNormal)
// DIA: idAI2::AnimEvent_StartDropToGround
PORT_AI_ANIM_EVENT(AnimEvent_StartDropToGround)

// DIA: idEntityPtr<idProp_Moveable>::operator idProp_Moveable * __ptr64
template <>
idEntityPtr<idProp_Moveable>::operator idProp_Moveable*() const {
  return entity;
}

// DIA: idScenePoint_Combat::GetDualPickup
idProp_Moveable* idScenePoint_Combat::GetDualPickup() {
  return dualWieldPickup;
}

// DIA: idAI2::AnimEvent_DetonateFakeGrenade
PORT_AI_ANIM_EVENT(AnimEvent_DetonateFakeGrenade)
// DIA: idAI2::AnimEvent_MeleePistolWhip
PORT_AI_ANIM_EVENT(AnimEvent_MeleePistolWhip)
// DIA: idAI2::AnimEvent_TraversalAttackChoice
PORT_AI_ANIM_EVENT(AnimEvent_TraversalAttackChoice)
// DIA: idAI2::AnimEvent_Explode
PORT_AI_ANIM_EVENT(AnimEvent_Explode)
// DIA: idAI2::AnimEvent_SetAimFocusToEnemy
PORT_AI_ANIM_EVENT(AnimEvent_SetAimFocusToEnemy)

// DIA: idScenePoint_Combat::HasDualPickup
bool idScenePoint_Combat::HasDualPickup() {
  return GetDualPickup() != nullptr;
}

// DIA: idAIMoveParms::GetGoalScenePoint
const idScenePoint* idAIMoveParms::GetGoalScenePoint() {
  return goalScenePoint;
}

// DIA: idAI2::AnimEvent_SmoothAnimDestinationLerpStart
PORT_AI_ANIM_EVENT(AnimEvent_SmoothAnimDestinationLerpStart)
// DIA: `idAI2::AnimEvent_SmoothAnimDestinationLerpStart'::`1'::dtor$0
// DIA: `idAI2::AnimEvent_SmoothAnimDestinationLerpStart'::`1'::dtor$1
// DIA: idAI2::AnimEvent_TraversalLerpStart
PORT_AI_ANIM_EVENT(AnimEvent_TraversalLerpStart)
// DIA: `idAI2::AnimEvent_TraversalLerpStart'::`1'::dtor$0
// DIA: `idAI2::AnimEvent_TraversalLerpStart'::`1'::dtor$1
// DIA: idAI2::AnimEvent_GiveItem
PORT_AI_ANIM_EVENT(AnimEvent_GiveItem)
// DIA: idAI2::AnimEvent_PushEntity
PORT_AI_ANIM_EVENT(AnimEvent_PushEntity)

// DIA: `dynamic initializer for 'ai_cookGrenades''
// DIA: `dynamic atexit destructor for 'ai_cookGrenades''

#undef PORT_AI_ANIM_EVENT
