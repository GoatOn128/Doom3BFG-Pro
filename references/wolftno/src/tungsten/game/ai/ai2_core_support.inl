#include "tungsten/game/ai/ai2_core_support.h"

namespace {
float EmittedCosineDegrees(const float degrees) {
  constexpr float degreesToRadians = 0.017453292F;
  constexpr float fullCircle = 6.2831855F;
  constexpr float inverseFullCircle = 0.15915494F;
  float radians = degrees * degreesToRadians;
  if (radians < 0.0F || radians >= fullCircle) {
    radians -= std::floor(radians * inverseFullCircle) * fullCircle;
  }
  return std::cos(radians);
}
}  // namespace

#define RECOVERED_METHOD(owner, method) \
  idRecoveredValue owner::method(...) { return {}; }

void idFiniteStateMachine::Save(idFile_String* file,
                                const char* indent) const {
  lastSaveIndent = indent != nullptr ? indent : "";
  ++saveCount;
  file->Printf("%s%s", lastSaveIndent.data, saveText.data);
}

idAction_MoveToPoint::~idAction_MoveToPoint() = default;
idAIActionManager::idAIActionManager()
    : actionFSM(std::make_unique<idAIActionFSM>()) {}
idAIActionManager::~idAIActionManager() = default;
idAIActionFSM* idAIActionManager::GetActionFSM() { return actionFSM.get(); }
const idAIActionFSM* idAIActionManager::GetActionFSM() const {
  return actionFSM.get();
}
idAIDamageInfo::~idAIDamageInfo() = default;
idAIDeathInfo::idAIDeathInfo() = default;
idAIDeathInfo::~idAIDeathInfo() = default;

idAIEditable::idAIPerception::~idAIPerception() = default;
idAIEditable::idAISpawn::idAISpawn() = default;
idAIEditable::idAISpawn::~idAISpawn() = default;
idAIEditable::idInteractions::idInteractions() = default;
idAIEditable::idInteractions::~idInteractions() = default;
idAIEditable::idAIDeath::idAIDeath() = default;
idAIEditable::idAIDeath::~idAIDeath() = default;
idAIEditable::idAIActionSettings::idAICustomRobot::idAICustomRobot() = default;
idAIEditable::idAIActionSettings::idAIFireSettings::idAIFireSettings() = default;
idAIEditable::idAIActionSettings::~idAIActionSettings() = default;
RECOVERED_METHOD(idAIEditable::idAIActionSettings, Init)
idAIEditable::~idAIEditable() = default;

void idAIEditable::idAIActionSettings::Clear() {
  usesTurrets = false;
  stationaryMode = false;
  noLowerBodyTurns = false;
  enemyTrailUpdatePeriod = 4.0F;
  isElite = false;
  isDualWielder = false;
  isCommander = false;
  isHeavy = false;
  isKampHund = false;
  isSuperSoldat = false;

  cover.usesCombatScenePoints = true;
  cover.usesClose = true;
  cover.usesIdle = true;
  cover.usesEmergencyExit = false;
  cover.dodgeMovingToScenepoint = false;
  cover.scenePointSelectionFlags = 3;
  cover.coverFlags = 0x36;
  cover.coverTargetMinRange = 200.0F;
  cover.coverMinRange = 50.0F;
  cover.coverMaxRange = 1000.0F;
  cover.coverRandomWeight = 0.1F;
  cover.coverHistoryWeight = 1.0F;
  cover.coverVerticalWeight = 1.0F;
  cover.coverTacticalWeight = 0.25F;
  cover.coverTargetTimeout = -1.0F;
  cover.returnToCoverDelay = 1.0F;

  melee.meleeMaxRange = 75.0F;
  melee.meleeLongMaxRange = 0.0F;
  melee.playerTakedownRange = 0.0F;
  melee.playerTakedownMaxHealth = 0.0F;
  melee.playerTakedownDecl = nullptr;
  melee.instantHitExcludeFromTrace = false;

  search.usesSearchScenePoints = true;
  search.searchMinRange = 50.0F;
  search.searchMaxRange = -1.0F;
  roam.usesRoamScenePoints = true;
  roam.roamMinRange = 50.0F;
  roam.roamMaxRange = -1.0F;
  roam.idleTalkPeriod = -1.0F;
  look.usesLookScenePoints = true;
  look.lookMinRange = 50.0F;
  look.lookMaxRange = -1.0F;
  look.minLookSPPeriod = 5.0F;
  look.combatTargetDelayLook = -1.0F;

  fire.aimAngle = 15.0F;
  fire.gangstaPoseFraction = 0.0F;
  fire.gangstaDuration = 3.0F;
  fire.maxFireRange = -1.0F;
  fire.usesGrenades = false;
  fire.fakeGrenadeMD6 = nullptr;
  fire.fakeGrenadeExplosion = nullptr;
  fire.secondaryFireRange = -1.0F;
  fire.useReloadAnimWhenMoving = false;
  fire.additiveFireAnims.ClearFree();
  fire.additiveFireAnimsCrouch.ClearFree();
  fire.additiveFireAnimBlendTime = 0.0F;
  fire.freeAmmoDistance = -1.0F;
  fire.goNutsRange = 0.0F;
  fire.safeReloadFraction = 0.0F;
  fire.aimDot = EmittedCosineDegrees(fire.aimAngle);
  fire.gangstaAimDot = EmittedCosineDegrees(fire.aimAngle + 10.0F);
  fire.aimDotUpsideDown = EmittedCosineDegrees(fire.aimAngle + 20.0F);
  fire.fireTiming.bUseFireTiming = false;
  fire.fireTiming.minBurstDuration = 1.0F;
  fire.fireTiming.maxBurstDuration = 3.0F;
  fire.fireTiming.minBurstPause = 0.2F;
  fire.fireTiming.maxBurstPause = 1.0F;
  fire.fireTiming.missTime = 0.0F;
  fire.fireTiming.missTimeMinRange = 300.0F;
  fire.fireTiming.missTrackAlongGround = false;
  fire.fireTiming.spotLostFire = 2.0F;
  fire.fireTiming.spotGainedFire = 0.0F;
  fire.fireTiming.minSpotLostRange = 75.0F;
  fire.fireTiming.predictTargetMovement = false;
  fire.fireTiming.focusCompensationTime = -1.0F;

  close.closeFlags = 1;
  close.closeMinRange = 0.0F;
  close.closeMaxRange = -1.0F;
  close.stopOnSpotRange = 500.0F;
  close.noSpotTimeBeforeWalking = 3.0F;
  close.maxRunRange = -1.0F;
  close.useEvasiveManeuvers = false;
  close.noStopOnSpotWhenBoreBlocked = false;
  close.evadeInterval = 1000;
  close.evadeMinDistance = 300;
  close.useOutOfReach = false;
  close.outOfReachGentlemanRange = 0.0F;
  close.periodicVoice.use = false;
  close.periodicVoice.minTime = 1000;
  close.periodicVoice.maxTime = 1000;

  dodge.dodgeFlags = 0xF6;
  dodge.dodgeMinRange = 100.0F;
  dodge.dodgeMaxRange = -1.0F;
  dodge.tauntMinRange = 100.0F;
  dodge.dodgePeriod = 2.0F;
  dodge.dodgeAnimPeriod = 4.0F;
  dodge.detectGrenadesInAir = false;
  taunt.tauntMinInterval = 4.0F;
  taunt.tauntMaxInterval = 7.0F;

  squad.squadLeader.value = 0x1FFF;
  squad.playerIsLeader = false;
  squad.leaderRange = 200.0F;
  squad.scenePointRange = 500.0F;
  squad.squadActive = true;
  outOfReach.distance = 32.0F;

  customRobot.usesRobot = false;
  customRobot.isBoss = true;
  customRobot.stageGroup.Set("battery");
  customRobot.stageHealth = 300.0F;
  customRobot.curStageHealth = 300.0F;
  customRobot.bossHealth = 1.0F;
  customRobot.chargeHealthFract = 0.15000001F;
  customRobot.chargeTime = 5.0F;
  customRobot.shootHealthDrain = 0.0F;
  customRobot.autoRecharge = false;
  customRobot.useRechargeAnim = false;
  customRobot.isCharging = false;
  customRobot.chargeStartTime = -1;
  customRobot.chargeDoneTime = -1;
  customRobot.numStages = 1;
  customRobot.triggerOnPowerOut.value = 0x1FFF;
  customRobot.triggerOnChargingDone.value = 0x1FFF;
  customRobot.curStage = 0;
  customRobot.noFireTime = -1;
  customRobot.immuneDoneTime = -1;
  customRobot.turnfastSound = nullptr;
  customRobot.lastBodyDir = idVec3{1.0F, 0.0F, 0.0F};
  customRobot.nextTurnfastTime = -1;
  customRobot.lastShootTime = -1;
  customRobot.nextMTSPCheck = -1;

  robotBlinding.bUsesBlindness = false;
  robotBlinding.blindedDuration = -1.0F;
  robotBlinding.reblindCooldown = 0.0F;
  robotBlinding.endBlindedTime = 0;
  robotBlinding.endReblindCooldown = 0;
  robotBlinding.blindedDamageGroup = nullptr;
  EMPStun.bUsesEMPStun = false;
  EMPStun.stunDuration = -1.0F;
  EMPStun.isEMPMagnetic = false;
  EMPStun.isNeverEMPMagnetic = false;
  EMPStun.endStunnedTime = 0;
  sleeping.bUsesSleeping = false;
  sleeping.sleepRadius = 100.0F;
  sleeping.isSleeping = false;

  robotDeath.timeToCritical = -1.0F;
  robotDeath.timeCriticalToExplode = -1.0F;
  robotDeath.explodeDamage = nullptr;
  robotDeath.stopEffectsDelay = 1.0F;
  robotDeath.criticalTimestamp = -1;
  robotDeath.explodeTimestamp = -1;
  robotDeath.deadTimeStamp = -1;
  robotDeath.deathSetup = false;

  tesla.usesTesla = false;
  tesla.useTeslaAnim = true;
  tesla.isHeavyGuardGun = false;
  tesla.teslaMeleeRange = 0.0F;
  tesla.teslaAASRange = 0.0F;
  tesla.teslaMinRange = 0.0F;
  tesla.teslaRange = 500.0F;
  tesla.teslaChargeTime = 5.0F;
  tesla.teslaCoolDownTime = 5.0F;
  tesla.teslaExplodeDelay = 0.25F;
  tesla.teslaMaxRangeDamageScale = 1.0F;
  tesla.useTeslaWhileBlinded = false;
  tesla.useTeslaBombGroundEffects = false;
  tesla.spawnTeslaBombInFOV = false;
  tesla.teslaTargetOffset = 64.0F;
  tesla.inhibitFireTime = 0.0F;
  tesla.teslaDamageDecl = nullptr;
  tesla.teslaBombDamageDecl = nullptr;
  tesla.teslaBombDangerZone = 0.0F;
  tesla.teslaBoltDecl = nullptr;
  tesla.teslaBoltFlickerInterval = 0.5F;
  tesla.teslaBlindFireDuration = 0.0F;
  tesla.teslaBombFxDecl = nullptr;
}

idSpawnFocusWithRequirement::idSpawnFocusWithRequirement() = default;

idAI2::idAnimWebDefaults::~idAnimWebDefaults() = default;
idAI2::idAIVolatile::idAIDeath::idAIDeath() = default;
idAI2::idAIVolatile::idAIDeath::~idAIDeath() = default;
idAI2::idAIVolatile::idAIWorldStateInfo::~idAIWorldStateInfo() = default;
idAI2::idAIVolatile::idPlayerAIControl::~idPlayerAIControl() = default;
idAI2::idAIVolatile::idAIDebugging::~idAIDebugging() = default;
idAI2::idAIVolatile::idAIAnimationInfo::~idAIAnimationInfo() = default;
idAI2::idAIVolatile::idAIScriptInfo::~idAIScriptInfo() = default;
idAI2::idAIConstant::~idAIConstant() = default;
idAI2::idAIVolatile::idAIPhysics::~idAIPhysics() = default;
idAI2::idAIVolatile::idAIPain::~idAIPain() = default;
idAI2::idAIVolatile::idAIFSMInfo::~idAIFSMInfo() = default;
idAI2::idAIVolatile::~idAIVolatile() = default;

idAI2::idAIVolatile::idAIAnimationInfo::idAIAnimationInfo() {
  subWeb = aiSubWeb_t::relaxed;
  turretAnimator.lastBlendTriangle = 0;
  turretAnimator.vertexAliasHandles.fill(0xFFFFU);
  viewTagData.trans = {};
  viewTagData.rot = {};
  viewTagData.parentJoint = 0xFFFFU;
  syncedAnim.targetSpawnId = 0x1FFF;
  ikEnableTime = -1;
}

idAI2::idAIVolatile::idAIScriptInfo::idAIScriptInfo() {
  threads.granularity = 1;
  threads.memTag = 5;
  threads.listStatic = 1;
  threads.num = 0;
  threads.size = 4;
  threads.list = threads.staticList;
  std::fill(std::begin(threads.staticList), std::end(threads.staticList), 0);
  fibers.granularity = 1;
  fibers.memTag = 5;
  fibers.listStatic = 1;
  fibers.num = 0;
  fibers.size = 4;
  fibers.list = fibers.staticList;
  std::fill(std::begin(fibers.staticList), std::end(fibers.staticList), 0);
  lastAIEventOriginator.value = 0x1FFF;
  lastAIEventInstigator.value = 0x1FFF;
}

idAI2::idAIConstant::idAIConstant() {
  physics.contents.bytes.fill(0);
  physics.clipMask.bytes.fill(0);
  physics.playerClipMask.bytes.fill(0);
  physics.extendedClipMask.bytes.fill(0);
  movement.moveSpeed = {0.0F, 0.0F};
  movement.bodyTurnRate = {0.0F, 0.0F};
  movement.moveTurnRate = {0.0F, 0.0F};
  movement.influence.offset = {};
  movement.influence.innerRadius = 32.0F;
  movement.influence.outerRadius = 64.0F;
  movement.influence.angle = 0.0F;
  movement.influence.trailDist = 32.0F;
  movement.influence.trailFadeInTime = 0.25F;
  movement.influence.trailFadeOutTime = 2.0F;
  animation.animWebs[0] = nullptr;
  animation.useMoods = false;
  animation.useLipsync = false;
  animation.useGestures = false;
  animation.useEyeTracking = false;
  animation.useFaceSetupEyeTracking = false;
  animation.useBlinking = false;
  animation.useLidDeformation = false;
  animation.useFaceManager = false;
  animation.useFacialAnims = false;
  animation.useCombatWebOnly = false;
  animation.useRelaxedWebOnly = false;
  animation.useHandsRelaxed = false;
  animation.canUseTurret = false;
  animation.useAimOffsetAnims = false;
  animation.focusTrackingYawOnlyOverride = 0.0F;
  animation.useMoodsEnabled = false;
  animation.useLipsyncEnabled = false;
  animation.useGesturesEnabled = false;
  animation.useFacialAnimsEnabled = false;
  animation.useFaceSetupEyeTrackingEnabled = false;
}

idAIFSMCallback::idAIFSMCallback() : log(64) {}
idAIFSMCallback::~idAIFSMCallback() = default;
idAIMoveState::~idAIMoveState() = default;
RECOVERED_METHOD(idAIMoveState, GetNextPoint)
idAnimator_Drone::~idAnimator_Drone() = default;
idAnimatorParms_AnimWeb::idAnimatorParms_AnimWeb(
    idAnimStack* animStack_, const char* name_,
    const idAI2CoreMD6Blend::blendOp_t blendOp_,
    const idAI2CoreMD6Blend::originBlend_t originBlend_,
    const md6WeightGroup_t weightGroup_,
    const md6WeightGroup_t filterGroup_, const float alpha_,
    const idDeclAnimWeb* webDecl_, const char* initialSubWebName_,
    const char* initialStateName_, const int curTime_)
    : animStack(animStack_),
      name(name_),
      blendOp(blendOp_),
      originBlend(originBlend_),
      weightGroup(weightGroup_),
      filterGroup(filterGroup_),
      alpha(alpha_),
      webDecl(webDecl_),
      initialStateName(initialStateName_),
      initialSubWebName(initialSubWebName_),
      curTime(curTime_) {}
idAnimatorParms_AnimWeb::~idAnimatorParms_AnimWeb() = default;
idFSMCallback::~idFSMCallback() = default;
idFSMManager::idFSMStack::idFSMStack() = default;
idFSMManager::~idFSMManager() = default;
idLegsFSM::~idLegsFSM() = default;
void idPresentable::SetModelFade(float value) {
  modelFade = value;
  ++modelFadeSetCount;
}
idScriptFunction::~idScriptFunction() = default;
std::intptr_t idBounds::Compare(...) const { return 0; }
std::intptr_t idStr::GetFileName(...) const { return 0; }
std::intptr_t idGameLocal::GetDebugEntity(...) { return 0; }
std::intptr_t idGameLocal::GetTensionManager(...) { return 0; }

template <int JointCount>
idAnimator_TurretNxN<JointCount>::~idAnimator_TurretNxN() = default;
template <int JointCount>
idRecoveredValue idAnimator_TurretNxN<JointCount>::SetParameters(...) { return {}; }
template class idAnimator_TurretNxN<3>;

template <typename Key, typename Value>
idHashTableT<Key, Value>::~idHashTableT() = default;
template <typename Key, typename Value>
idRecoveredValue idHashTableT<Key, Value>::Clear(...) { return {}; }
template class idHashTableT<int, idAIAction*>;

template class idHealthT<aiHealthComponent_t, 2, 0>;

template class idShieldHealthT<aiHealthComponent_t, 2, 0, 1>;

template class idInterpolate<int>;

template class idRange<float>;

using idAIEventHandle = idHandle<int, invalidAIEvent_t, -1>;
template int idList<eboltControlNode_t, 5>::Append(const eboltControlNode_t&);
template bool idList<idCrosshairCustomIcon, 5>::Resize(int);
template bool idList<idCrosshairCustomIcon, 5>::SetNum(int);
template int idList<idAIEventHandle, 5>::Append(const idAIEventHandle&);
template int idList<idAIEventHandle, 5>::Insert(const idAIEventHandle&, int);
template bool idList<idAIEventHandle, 5>::RemoveIndex(int);
template idList<idPonytail, 5>::~idList();
template bool idList<idVec3, 5>::RemoveIndexFast(int);

template <>
idEntitySpriteAnimator* idEntityPtr<idEntitySpriteAnimator>::GetEntity() const { return entity; }
template <>
idEntityPtr<idProxyMove>::operator idProxyMove*() const { return entity; }
template <>
idTarget_AlarmSystem* idEntityPtr<idTarget_AlarmSystem>::GetEntity() const { return entity; }
template <>
idEntityPtr<idTarget_FakeEnemy>::operator idTarget_FakeEnemy*() const { return entity; }
template <>
idVolume_CustomDeath* idEntityPtr<idVolume_CustomDeath>::operator->() const { return entity; }

const char* aiWalkModifierNames[7] = {
    "none", "walk", "run", "crouch", "sprint", "injured", "scripted"};

template <const char** Values, int Count>
idRecoveredValue idCmdSystem::ArgCompletion_String(...) {
  return {Count + (Values != nullptr ? 1 : 0)};
}
template idRecoveredValue idCmdSystem::ArgCompletion_String<aiWalkModifierNames, 7>(...);

#undef RECOVERED_METHOD
