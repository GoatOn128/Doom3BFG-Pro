#pragma once

#include "decl.h"
#include "decljointname.h"
#include "shared/idlib/math/vector.h"

class idCollisionModel;
class idDeclBreakable;
class idDeclEntityDef;
class idDeclFX;
class idDeclImpactParticle;
class idDeclMD6;
class idDeclParticle;
class idDeclTable;
class idMaterial;
class idSoundShader;

#ifndef SLOWMO_PARMS_T_DEFINED
#define SLOWMO_PARMS_T_DEFINED
struct slowmoParms_t {
  float easeInDuration{};
  float slowmoDuration{};
  float easeOutDuration{};
  float timeScale{};
};
#endif

class idDeclGameTime final : public idDeclTypeInfo {
public:
  idDeclInfo* GetDeclInfo() const override;
  slowmoParms_t slowmoParms;
  static idDeclInfoTemplate<idDeclGameTime> resourceList;
};

class idDeclDestructiblePieceInfo final : public idDeclTypeInfo {
public:
  enum destructiblePieceBreakBehavior_t : int { BREAK_DEFAULT = 0 };
  struct pieceInfo_t {
    float health;
    bool useOverallHealthInstead;
    destructiblePieceBreakBehavior_t behavior;
    float breakDelay;
    float density;
    const idDeclParticle* particleSystem;
    idMat3 particleAxis;
    const idSoundShader* soundToPlayWhenBroken;
    float obliterateChance;
  };
  struct obliterateInfo_t {
    const idDeclParticle* particleSystem;
    idMat3 particleAxis;
    const idSoundShader* soundToPlay;
    const idDeclEntityDef* entityToSpawn;
    int entitiesInPool;
    bool firstTimeOnly;
  };

  idDeclDestructiblePieceInfo();
  idDeclInfo* GetDeclInfo() const override;
  idList<pieceInfo_t> destructiblePieceInfo;
  idList<obliterateInfo_t> obliterateInfo;
  static idDeclInfoTemplate<idDeclDestructiblePieceInfo> resourceList;
};

class idDeclDragJoints final : public idDeclTypeInfo {
public:
  idDeclDragJoints();
  ~idDeclDragJoints() override;
  idDeclInfo* GetDeclInfo() const override;
  float ghostJointDistance;
  float maxBend;
  float downHillPitchAdjust;
  idJointName baseJoint;
  idList<idJointName> joints;
  idList<idStr> subWebs;
  idList<idStr> excludeStates;
  static idDeclInfoTemplate<idDeclDragJoints> resourceList;
};

class idDeclFreezeJoints final : public idDeclTypeInfo {
public:
  idDeclFreezeJoints();
  idDeclInfo* GetDeclInfo() const override;
  idList<idJointName> joints;
  static idDeclInfoTemplate<idDeclFreezeJoints> resourceList;
};

class idDeclPonytail final : public idDeclTypeInfo {
public:
  struct idPonytailJointInfo {
    idJointName jointName;
    float limitAngle;
    float lerpRate;
    int targetSpringIndex;
  };
  struct idPonytailSpringInfo {
    int ptJointIndex;
    float mass;
    float tightness;
    float damping;
    float maxLength;
    float maxSpeed;
    float gravityMag;
  };

  idDeclPonytail();
  ~idDeclPonytail() override;
  idDeclInfo* GetDeclInfo() const override;
  const idDeclMD6* modelDef;
  idAtomicString tagName;
  idList<idPonytailJointInfo, 30> jointInfo;
  idList<idPonytailSpringInfo, 30> springInfo;
  static idDeclInfoTemplate<idDeclPonytail> resourceList;
};

struct idSuspensionLeverDef {
  enum leverDir_t : int {
    DIR_RIGHT = 0,
    DIR_LEFT = 1,
    DIR_FORWARD = 2,
    DIR_BACKWARD = 3,
    DIR_ALIGN_TO = 4,
    LEVER_DEFAULT = DIR_RIGHT,
  };
  idAtomicString joint;
  idAtomicString alignToJoint;
  leverDir_t direction;
};

struct idWheelDef {
  idAtomicString wheelJoint;
  float wheelRadius;
  float wheelOffset;
  float suspensionUp;
  float suspensionDown;
  float suspensionCompress;
  float suspensionDamping;
  idList<idSuspensionLeverDef> suspensionLever;
  const idDeclTable* friction_longitudinal;
  const idDeclTable* friction_lateral;
  float steering;
  float wobbleScale;
  short copyFromWheel;
  unsigned char hasDrive : 1;
  unsigned char hasBrake : 1;
  unsigned char hasHandbrake : 1;
};

struct idVehicleBodyDef {
  const idCollisionModel* model;
  const idCollisionModel* shotModel;
  float density;
  float linearFriction;
  float angularFriction;
  float contactFriction;
  float bouncyness;
};

struct idVehicleSoundDef {
  struct gear_t {
    float maxSpeed, shiftUpPercent, shiftDownPercent;
    const idDeclTable* motorTorque;
    const idSoundShader* loaded;
    const idSoundShader* unloaded;
    float minVolumeLoaded, minPitch, maxPitch, minVolume, maxVolume;
    float compressionPitchMin, compressionPitchMax, compressionPitchMinScale, compressionPitchMaxScale;
  };
  struct engineIdle_t {
    const idSoundShader* sound;
    float speedMPH, inTime, outTime, loadedUnloadedMPH, loadedUnloadedInTime,
        loadedUnloadedOutTime;
    int engineSoundFadeInTimeMilliSec;
  };
  struct engineBoost_t {
    const idSoundShader *boost, *boostOverlay, *boostFull, *boostEmpty, *boostNotReady;
    float boostFadeOutTime, boostOverlayDelay, boostOverlayFadeInTime, boostPitchModifier,
        minVolume, maxVolume;
  };
  struct shifting_t {
    const idSoundShader *engineShiftUp, *engineShiftDown;
    float shiftUpPercent, shiftDownPercent, shiftSilenceTime, shiftRampInTime, shiftRampOutTime;
    bool playShiftDownDuringCoast, playShiftUpOnShiftDown;
  };
  struct revLimiter_t {
    int delay;
    bool enabled;
    int frequency;
    float squareMinVolumeMultiplier, throttleMin, pitchMultiplier;
  };
  struct windParms_t {
    const idSoundShader* wind;
    const idDeclTable *windVolumeTable, *windPitchTable;
  };
  struct brakingReverse_t {
    const idSoundShader* brakeWhine;
    float maxReversePitch, brakeWhineMPH;
  };
  struct surfaceSounds_t {
    const idSoundShader *roll, *skid, *scrape;
    const idDeclTable *rollVolume, *rollPitch, *skidVolume;
  };
  struct surfaceSoundList_t {
    surfaceSoundList_t();
    surfaceSounds_t metal, stone, wood, cardboard, liquid, glass, plastic, dirt, asphalt,
        concrete, linoleum, fabric, rubber, rock, foliage, porcelain;
  };
  struct crashSounds_t { const idSoundShader *human, *vehicle, *genericSnd; };
  struct landingSounds_t { const idSoundShader *easy, *medium, *hard; };
  struct impactSounds_t {
    float maxEasyForce, maxMediumForce;
    const idSoundShader *easy, *medium, *hard;
  };
  struct bumpSounds_t {
    int delay;
    float easyCompression;
    const idSoundShader* easy;
  };
  struct tireWobbleSounds_t {
    const idSoundShader* sound;
    float minPitch, maxPitch, minVolume, maxVolume;
  };
  struct engineDamageSpunds_t {
    const idSoundShader *damagedEngine1, *damagedEngine2, *damagedEngine3, *EngineExplode;
    float minPitch, maxPitch, minVolume, maxVolume;
  };

  idVehicleSoundDef();
  idList<gear_t> gears;
  engineIdle_t idle;
  engineBoost_t boostParms;
  shifting_t shifting;
  revLimiter_t revLimiter;
  windParms_t windParms;
  brakingReverse_t brakingReverse;
  surfaceSoundList_t surfaceList;
  crashSounds_t crash;
  landingSounds_t landing;
  impactSounds_t impact;
  bumpSounds_t bumpSounds;
  tireWobbleSounds_t tireWobble;
  engineDamageSpunds_t engineDamage;
  const idSoundShader *engineStop, *explode, *backfire, *smokeburp, *attachmentCollision,
      *impactSpark, *debris, *steering, *zooming, *rcbombTriggered, *lockOnWarningSound,
      *nonCombatMusic, *combatMusic, *combatMusic2, *enterVehicle, *exitVehicle, *quickReverse,
      *hornOnce, *hornLoop, *engineSmoke, *engineFire, *quickUseError, *foliage, *bodyToBody,
      *tireToTire, *projectileToBody;
};

struct idVehicleEffectsDef {
  struct effectsMaterial_t {
    const idMaterial *metal, *stone, *wood, *cardboard, *liquid, *glass, *plastic, *dirt,
        *asphalt, *concrete, *linoleum, *fabric, *rubber, *rock, *foliage, *porcelain;
  };
  struct tireTrailParms_t {
    float halfWidth, halfLength;
    int lifeTime, fadeTime;
    effectsMaterial_t material;
  };
  idVehicleEffectsDef();
  const idDeclImpactParticle *tireKickup, *tireKickup2;
  float tireKickupSpacing, tireKickup2Spacing, dustVelocityScale;
  const idDeclImpactParticle *tireInitialKickup, *rolloverKickup;
  tireTrailParms_t tireSkidParms, tireTrackParms;
  const idDeclImpactParticle* particleImpact;
  const idDeclParticle *particleExplode, *particleRpmFlame, *particleShotDamage,
      *particleShotShield;
  const idDeclEntityDef* breakable;
  const idDeclParticle *particleAttachmentLost, *particleViolentRoll,
      *exhaustEmitterParticle_right, *exhaustEmitterParticle_left;
  const idDeclFX* vehicleFX;
};

struct steeringWheel_t {
  enum rotationDirection_t : int {
    ROTATE_LEFT = 0,
    ROTATE_RIGHT = 1,
    ROTATION_DEFAULT = ROTATE_LEFT,
  };
  idJointName jointName;
  int rotationAxis;
  rotationDirection_t rotationDirection;
  const idDeclTable* maxAngle;
  float speed;
};

enum dialAxis_t : int {
  DIAL_AXIS_X = 0,
  DIAL_AXIS_Y = 1,
  DIAL_AXIS_Z = 2,
  DIAL_AXIS_DEFAULT = DIAL_AXIS_X,
};
enum dialDirection_t : int {
  DIAL_ROTATE_LEFT = 0,
  DIAL_ROTATE_RIGHT = 1,
  DIAL_MOVE_POSITIVE = 2,
  DIAL_MOVE_NEGATIVE = 3,
  DIAL_DIRECTION_DEFAULT = DIAL_ROTATE_LEFT,
};
struct dialMeter_t {
  idJointName jointName;
  dialAxis_t axis;
  dialDirection_t direction;
  int startValue, endValue, startAmount, endAmount;
  const idDeclTable* table;
};

struct idEntityInfluenceTrail {
  idVec3 offset;
  float innerRadius, outerRadius, angle, trailDist, trailFadeInTime, trailFadeOutTime;
};

class idDeclVehicleProps final : public idDeclTypeInfo {
public:
  struct cameraPlacement_t {
    float yaw, pitch, height, lateral, minRange, maxRange;
    const idDeclTable *recovery, *rotationOffset;
  };
  idDeclVehicleProps();
  ~idDeclVehicleProps() override;
  idDeclInfo* GetDeclInfo() const override;
  void Parse(idParser& source) override;

  cameraPlacement_t camera, camera2;
  const idDeclTable *fpsStickYawTable, *fpsStickPitchTable;
  float secondarySteeringScale, steeringThrottleScale;
  steeringWheel_t steeringWheel;
  float steerControlInfluence;
  dialMeter_t tachometer, speedometer;
  idVec3 cgLocation, maxCgLocation;
  float maxSpeed, boostMaxSpeedScalar, additionalRaceSpeed, maxReverseSpeed, handBrakeForce,
      handbrakeFrictionScale, handbrakeMinThreshold, brakeForce;
  const idDeclTable* motorTorque;
  float frictionForce, pitchStabilization, rollStabilization, skidRoll, skidRollBoostMult;
  bool hasBoost;
  const idMaterial* boostBlurMaterial;
  int baseBoostUpgrade;
  idStr sheildModel;
  const idDeclTable* damageSpeedScale;
  float jumpImpulse;
  bool hasEngineIdle, removeOnExit;
  idEntityInfluenceTrail influence;
  bool remoteControl, raceRemoteControl;
  idList<idWheelDef> wheels;
  idVehicleBodyDef body;
  idVehicleSoundDef sounds;
  idVehicleEffectsDef effects;
  const idDeclBreakable* breakableDebris;
  static idDeclInfoTemplate<idDeclVehicleProps> resourceList;
};

static_assert(sizeof(idDeclDragJoints) == 224, "idDeclDragJoints ABI drift");
static_assert(sizeof(idDeclFreezeJoints) == 120, "idDeclFreezeJoints ABI drift");
static_assert(sizeof(idDeclPonytail) == 160, "idDeclPonytail ABI drift");
static_assert(sizeof(idDeclVehicleProps) == 2488, "idDeclVehicleProps ABI drift");
