#include "declentitydef.h"
#include "declaf.h"
#include "declenv.h"
#include "declholder.h"
#include "declskins.h"
#include "declswfresource.h"
#include "engine/models/particles/declparticle.h"

idDeclInfoTemplate<idDeclEntityDef> idDeclEntityDef::resourceList(
    "entityDef", "idDeclEntityDef", "def", false);

idDeclInfoTemplate<idDeclSkins> idDeclSkins::resourceList(
    "skins", "idDeclSkins", "skin", false);

idDeclInfoTemplate<idDeclSwfResource> idDeclSwfResource::resourceList(
    "swfresource", "idDeclSwfResource", "tdef", false);

idDeclInfoTemplate<idDeclGameTime> idDeclGameTime::resourceList(
    "gameTime", "idDeclGameTime", "tdef", false);
idDeclInfoTemplate<idDeclDestructiblePieceInfo> idDeclDestructiblePieceInfo::resourceList(
    "destructiblePiece", "idDeclDestructiblePieceInfo", "tdef", false);
idDeclInfoTemplate<idDeclDragJoints> idDeclDragJoints::resourceList(
    "dragJoints", "idDeclDragJoints", "dragjoints", false);
idDeclInfoTemplate<idDeclFreezeJoints> idDeclFreezeJoints::resourceList(
    "freezeJoints", "idDeclFreezeJoints", "freezejoints", false);
idDeclInfoTemplate<idDeclPonytail> idDeclPonytail::resourceList(
    "ponytail", "idDeclPonytail", "tdef", false);
idDeclInfoTemplate<idDeclVehicleProps> idDeclVehicleProps::resourceList(
    "vehicle", "idDeclVehicleProps", "vehicle", false);
idDeclInfoTemplate<idDeclEnv> idDeclEnv::resourceList(
    "env", "idDeclEnv", "env", false);
idDeclInfoTemplate<idDeclAF> idDeclAF::resourceList(
    "af", "idDeclAF", "af", false);
idDeclInfoTemplate<idDeclParticle> idDeclParticle::resourceList(
    "particle", "idDeclParticle", "prt", false);

idDeclInfo* idDeclEnv::GetDeclInfo() const { return &resourceList; }

idDeclSkins::idDeclSkins() {
  resourceListPtr = &resourceList;
}

idDeclSkins::~idDeclSkins() = default;

idDeclInfo* idDeclSkins::GetDeclInfo() const {
  return &resourceList;
}

idDeclInfo* idDeclGameTime::GetDeclInfo() const { return &resourceList; }

idDeclDestructiblePieceInfo::idDeclDestructiblePieceInfo() {
  resourceListPtr = &resourceList;
}
idDeclInfo* idDeclDestructiblePieceInfo::GetDeclInfo() const { return &resourceList; }

idDeclDragJoints::idDeclDragJoints()
    : ghostJointDistance(16.0f), maxBend(90.0f), downHillPitchAdjust(1.35f) {
  resourceListPtr = &resourceList;
}
idDeclDragJoints::~idDeclDragJoints() = default;
idDeclInfo* idDeclDragJoints::GetDeclInfo() const { return &resourceList; }

idDeclFreezeJoints::idDeclFreezeJoints() { resourceListPtr = &resourceList; }
idDeclInfo* idDeclFreezeJoints::GetDeclInfo() const { return &resourceList; }

idDeclPonytail::idDeclPonytail() : modelDef(nullptr) { resourceListPtr = &resourceList; }
idDeclPonytail::~idDeclPonytail() = default;
idDeclInfo* idDeclPonytail::GetDeclInfo() const { return &resourceList; }

idVehicleSoundDef::surfaceSoundList_t::surfaceSoundList_t()
    : metal{}, stone{}, wood{}, cardboard{}, liquid{}, glass{}, plastic{}, dirt{}, asphalt{},
      concrete{}, linoleum{}, fabric{}, rubber{}, rock{}, foliage{}, porcelain{} {}

idVehicleEffectsDef::idVehicleEffectsDef()
    : tireKickup(nullptr), tireKickup2(nullptr), tireKickupSpacing(32.0f),
      tireKickup2Spacing(32.0f), dustVelocityScale(0.25f), tireInitialKickup(nullptr),
      rolloverKickup(nullptr), tireSkidParms{}, tireTrackParms{}, particleImpact(nullptr),
      particleExplode(nullptr), particleRpmFlame(nullptr), particleShotDamage(nullptr),
      particleShotShield(nullptr), breakable(nullptr), particleAttachmentLost(nullptr),
      particleViolentRoll(nullptr), exhaustEmitterParticle_right(nullptr),
      exhaustEmitterParticle_left(nullptr), vehicleFX(nullptr) {
  tireSkidParms.halfWidth = tireTrackParms.halfWidth = 16.0f;
  tireSkidParms.halfLength = tireTrackParms.halfLength = 32.0f;
  tireSkidParms.lifeTime = tireTrackParms.lifeTime = 2000;
  tireSkidParms.fadeTime = tireTrackParms.fadeTime = 500;
}

idVehicleSoundDef::idVehicleSoundDef()
    : idle{}, boostParms{}, shifting{}, revLimiter{}, windParms{}, brakingReverse{},
      surfaceList{}, crash{}, landing{}, impact{}, bumpSounds{}, tireWobble{}, engineDamage{},
      engineStop(nullptr), explode(nullptr), backfire(nullptr), smokeburp(nullptr),
      attachmentCollision(nullptr), impactSpark(nullptr), debris(nullptr), steering(nullptr),
      zooming(nullptr), rcbombTriggered(nullptr), lockOnWarningSound(nullptr),
      nonCombatMusic(nullptr), combatMusic(nullptr), combatMusic2(nullptr), enterVehicle(nullptr),
      exitVehicle(nullptr), quickReverse(nullptr), hornOnce(nullptr), hornLoop(nullptr),
      engineSmoke(nullptr), engineFire(nullptr), quickUseError(nullptr), foliage(nullptr),
      bodyToBody(nullptr), tireToTire(nullptr), projectileToBody(nullptr) {
  idle.speedMPH = 10.0f;
  idle.inTime = 4.0f;
  idle.outTime = 1.0f;
  idle.loadedUnloadedMPH = 5.0f;
  idle.loadedUnloadedInTime = 0.2f;
  idle.loadedUnloadedOutTime = 10.0f;
  idle.engineSoundFadeInTimeMilliSec = 0;
  boostParms.boostFadeOutTime = 1.5f;
  boostParms.boostOverlayDelay = 1.0f;
  boostParms.boostOverlayFadeInTime = 0.5f;
  boostParms.boostPitchModifier = 1.5f;
  boostParms.minVolume = -60.0f;
  boostParms.maxVolume = -4.0f;
  shifting.shiftUpPercent = 0.9f;
  shifting.shiftDownPercent = 0.7f;
  shifting.shiftSilenceTime = 0.05f;
  shifting.shiftRampInTime = 0.0f;
  shifting.shiftRampOutTime = 0.05f;
  shifting.playShiftDownDuringCoast = false;
  shifting.playShiftUpOnShiftDown = true;
  revLimiter.delay = 500;
  revLimiter.enabled = true;
  revLimiter.frequency = 20;
  revLimiter.squareMinVolumeMultiplier = 0.25f;
  revLimiter.throttleMin = 0.5f;
  revLimiter.pitchMultiplier = 1.5f;
  brakingReverse.maxReversePitch = 0.4f;
  brakingReverse.brakeWhineMPH = 35.0f;
  impact.maxEasyForce = 15000.0f;
  impact.maxMediumForce = 40000.0f;
  bumpSounds.delay = 100;
  bumpSounds.easyCompression = 0.1f;
}

idDeclVehicleProps::idDeclVehicleProps()
    : camera{}, camera2{}, fpsStickYawTable(nullptr), fpsStickPitchTable(nullptr),
      secondarySteeringScale(0.0f), steeringThrottleScale(0.0f), steeringWheel{},
      steerControlInfluence(0.55f), tachometer{}, speedometer{}, cgLocation{}, maxCgLocation{},
      maxSpeed(100.0f), boostMaxSpeedScalar(1.0f), additionalRaceSpeed(10.0f),
      maxReverseSpeed(40.0f), handBrakeForce(3000.0f), handbrakeFrictionScale(0.3f),
      handbrakeMinThreshold(1.0f), brakeForce(400.0f), motorTorque(nullptr),
      frictionForce(30.0f), pitchStabilization(0.7f), rollStabilization(0.7f),
      skidRoll(0.0006f), skidRollBoostMult(1.8f), hasBoost(true), boostBlurMaterial(nullptr),
      baseBoostUpgrade(1), damageSpeedScale(nullptr), jumpImpulse(3250000.0f),
      hasEngineIdle(true), removeOnExit(false), influence{}, remoteControl(false),
      raceRemoteControl(false), body{}, sounds{}, effects{}, breakableDebris(nullptr) {
  resourceListPtr = &resourceList;
  steeringWheel.rotationAxis = 1;
  steeringWheel.rotationDirection = steeringWheel_t::ROTATION_DEFAULT;
  steeringWheel.maxAngle = nullptr;
  steeringWheel.speed = 90.0f;
  tachometer.axis = speedometer.axis = static_cast<dialAxis_t>(1);
  tachometer.direction = speedometer.direction = static_cast<dialDirection_t>(0);
  tachometer.startValue = speedometer.startValue = 0;
  tachometer.endValue = speedometer.endValue = 100;
  tachometer.startAmount = speedometer.startAmount = 0;
  tachometer.endAmount = speedometer.endAmount = 90;
  tachometer.table = speedometer.table = nullptr;
  influence.offset = {0.0f, 0.0f, 0.0f};
  influence.innerRadius = 32.0f;
  influence.outerRadius = 64.0f;
  influence.angle = 0.0f;
  influence.trailDist = 32.0f;
  influence.trailFadeInTime = 0.25f;
  influence.trailFadeOutTime = 2.0f;
  body = {nullptr, nullptr, 0.00094f, 0.06f, 0.06f, 0.5f, 0.3f};
  camera = {0.0f, 5.0f, 50.0f, 0.0f, 300.0f, 500.0f, nullptr, nullptr};
  camera2 = {0.0f, 10.0f, 8.0f, 7.0f, 8.0f, 8.0f, nullptr, nullptr};
}

idDeclVehicleProps::~idDeclVehicleProps() = default;
idDeclInfo* idDeclVehicleProps::GetDeclInfo() const { return &resourceList; }
void idDeclVehicleProps::Parse(idParser& source) { idDeclTypeInfo::Parse(source); }
