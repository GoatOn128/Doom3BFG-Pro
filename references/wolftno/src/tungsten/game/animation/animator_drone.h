#pragma once

#include "tungsten/game/game_runtime.h"

#include <array>
#include <string>

class idAI2;

template <typename T>
class idSpring {
public:
  void Update(float dt);
  void SetConstants(float springConstant, float damping);

  float k{1.0F};
  float m{1.0F};
  float c{2.0F};
  T p0{};
  T p1{};
  T vel{};
  float restLength{0.0F};
  float maxSpeed{0.0F};
  T pMin{};
  bool hasPMin{false};
  T pMax{};
  bool hasPMax{false};
  bool fixDenormals{true};
};

class idAnimStack {};

struct idAnimatorParms_Base {
  const idAnimStack* animStack{nullptr};
  std::string weightGroup;
};

struct idMD6Branch {
  float currentAlpha{0.0F};
  float targetAlpha{0.0F};
  bool hasRightLeaf{false};
};

class idAnimator_Drone {
public:
  struct thruster_t {
    thruster_t();
    std::string jointNamePitch;
    std::string jointNameRoll;
    std::string jointNameProp;
    idVec2 rotationMod{};
    idVec2 minAngles{};
    idVec2 maxAngles{};
    idSpring<idVec2> angles;
    idSpring<idVec2> propSpeed;
    float propAngle{0.0F};
  };

  struct feeler_t {
    feeler_t();
    std::string jointName;
    float randomScale{0.0F};
    idVec2 minAngles{};
    idVec2 maxAngles{};
    idSpring<idVec2> angles;
  };

  idAnimator_Drone();
  virtual ~idAnimator_Drone() = default;

  void SetLastDamageImpact(float damage, float damageFraction,
                           const idVec3& damagePosition,
                           const idVec3& damageDirection);
  bool ShouldHoldFire(idAI2* ai, const idEntity* enemy,
                      aiHoldFireReason_t* holdFireReason);
  void InternalPreBlendTree(const idAnimStack* stack, int curTime,
                            int ticksPerSec);
  void Update(idAI2* ai, const idMat3* axis, const idVec3* velocity);
  bool InternalInit(const idAnimatorParms_Base* parms);
  const idMD6Branch* InternalGetMergeBranch() const;

  void SetPresentOrigin(const idVec3& value) { presentOrigin = value; }
  void SetGoalRange(float value) { goalRange = value; }
  void SetNoFireMoving(bool value) { noFireMoving = value; }
  void SetLaunchDoneTime(int value) { launchDoneTime = value; }
  void SetFireWindup(int value) { curFireWindupTick = value; }
  void SetKeepFiringStop(int value) { keepFiringStopTick = value; }
  float GetLastDamageFraction() const { return lastDamageFraction; }
  float GetLastDamagePush() const { return lastDamagePush; }
  int GetLastDamageTick() const { return lastDamageTick; }
  const idVec3& GetLastDamageDirection() const { return lastDamageDir; }
  float GetSpeed() const { return currentSpeed; }
  int GetPreBlendUpdates() const { return preBlendUpdates; }
  const std::array<feeler_t, 4>& GetFeelers() const { return feelers; }
  const std::array<thruster_t, 2>& GetThrusters() const { return thrusters; }

private:
  std::array<feeler_t, 4> feelers;
  std::array<thruster_t, 2> thrusters;
  idMD6Branch mergeBranch;
  idVec3 presentOrigin{};
  idVec3 lastDamagePos{};
  idVec3 lastDamageDir{};
  float collisionRadius{25.0F};
  float lastDamageFraction{1.0F};
  float lastDamagePush{-1.0F};
  float goalRange{0.0F};
  float currentSpeed{0.0F};
  int lastDamageTick{-1};
  int launchDoneTime{0};
  int curFireWindupTick{0};
  int keepFiringStopTick{0};
  int nextFireWindupTick{0};
  int preBlendUpdates{0};
  bool noFireMoving{false};
  bool initialized{false};
};

extern idCVar droneAnimator_angleSpringK;
extern idCVar droneAnimator_MaxAcc;
extern idCVar droneAnimator_thrusterAngleSpringK;
extern idCVar droneAnimator_thrusterPropSpeedSpringK;
extern idCVar droneAnimator_thrusterAngleTurnscale;
extern idCVar droneAnimator_thrusterAngleMovescale;
