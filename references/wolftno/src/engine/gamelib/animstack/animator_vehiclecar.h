#pragma once

#include "engine/decls/declholder.h"
#include "engine/gamelib/animstack/animator_base.h"
#include "engine/gamelib/physics/clip.h"
#include "engine/models/skeletalanimation/jointanimator.h"

#include <cstddef>
#include <cstdint>

class idRenderModel;

struct wheelSlipInfo_t {
  wheelSlipInfo_t() : enabled(false), velocity(0.0f), slip(0.0f) {}

  bool enabled;
  std::uint8_t padding[3];
  float velocity;
  float slip;
};

class idAnimSuspensionLever {
public:
  idAnimSuspensionLever()
      : jointIndex(static_cast<std::int16_t>(-1)),
        alignToJointIndex(static_cast<std::int16_t>(-1)), invLength(1.0f),
        rotationAxis{}, alignAxis{}, localJointQuat(quat_identity) {}

  idJointIndex_t jointIndex;
  idJointIndex_t alignToJointIndex;
  float invLength;
  idVec3 rotationAxis;
  idVec3 alignAxis;
  idQuat localJointQuat;
};

class idAnimDialMeter {
public:
  idAnimDialMeter()
      : joint(static_cast<std::int16_t>(-1)), axis{}, rot(quat_identity),
        pos{}, value(0.0f) {}

  idJointIndex_t joint;
  std::uint8_t jointPadding[2]{};
  idVec3 axis;
  idQuat rot;
  idVec3 pos;
  float value;
};

class idAnimWheel {
public:
  enum skidMode_t {
    SKIDMARK_NONE = 0,
    SKIDMARK_START = 1,
    SKIDMARK_MIDDLE = 2,
    SKIDMARK_END = 3,
  };

  struct tireMark_t {
    tireMark_t();

    skidMode_t markMode;
    skidMode_t lastMarkMode;
    idVec3 lastOrigin;
    idVec3 corners[4];
    idVec3 left;
    float yBias;
    std::uint8_t padding[4];
    const idMaterial* material;
  };

  idAnimWheel();
  ~idAnimWheel();
  idAnimWheel& operator=(const idAnimWheel& other);

  idList<idAnimSuspensionLever, 5> suspensionLever;
  idJointIndex_t wheelJointIndex;
  std::uint8_t wheelJointPadding[6];
  idClipModel* clipModel;
  idQuat localJointQuat;
  idVec4 localJointOrigin;
  tireMark_t skid;
  tireMark_t track;
  int ongroundTime;
  float hitgroundSpeed;
  int dustTime;
  idVec3 lastDustPrtDropPos;
  idVec3 lastDustPrtDropPos2;
  float wobbleFrac;
  idMat3x4 wheelSteeringAxis;
  idClipQuery collisionQuery;
  float traceFraction;
  float prevTraceFraction;
  int traceSurfaceType;
  std::uint8_t traceSurfaceColor[3];
  std::uint8_t traceSurfacePadding;
  idVec3 lastWorldOrigin;
  float wheelAngle;
  float wheelVelocity;
  float wheelHeight;
  float wheelSlip;
  wheelSlipInfo_t wheelSlipInfo;
};

class idAnimator_VehicleCar : public idAnimator_Base {
public:
  idAnimator_VehicleCar();
  ~idAnimator_VehicleCar() override;

  priority_t GetStackPriority() const override;
  serializeType_t GetSerializeType() override { return STYPE_VEHICLECAR; }
  void SerializeSnapshot(idSerializer& serializer) override;
  void PreBlendSnapshot(idAnimStack* stack, int curTime, int ticksPerSec,
                        float fraction) override;

  void InitVehicleAnimator(idGameTimeManager* gameTimeManager,
                           const idDeclVehicleProps* vehicleProps,
                           idAnimStack* animStack, idRenderModel* renderModel,
                           int entityNum, idClip* clip);

  void SetSteerAngle(float value) { steerAngle = value; }
  void SetAccelerating(bool value) { accelerating = value; }
  void SetHandBraking(bool value) { handBraking = value; }
  void SetIsReversing(bool value) { isReversing = value; }
  void SetTireWobble(int wheelIndex, float wobbleFrac);
  void SetPlayerOccupant(bool value);
  void SetRotateSteeringWheel(bool value) { rotateSteeringWheel = value; }
  void SetSpeedometer(float value);
  void SetTachometer(float value);
  bool HasSpeedometer() const { return speedometer >= 0; }
  bool HasTachometer() const { return tachometer >= 0; }
  idJointIndex_t GetSteeringWheelJointIndex() const {
    return steeringWheelJoint;
  }
  idJointIndex_t GetSpeedometerJointIndex() const {
    return InternalGetDialMeterJointIndex(speedometer);
  }
  idJointIndex_t GetTachometerJointIndex() const {
    return InternalGetDialMeterJointIndex(tachometer);
  }
  int NumAnimWheel() const { return animWheels.num; }
  idAnimWheel& GetAnimWheel(int index) { return animWheels.list[index]; }
  void SetClipMask(int value) { clipMask = value; }
  void SetEntityNumber(int value) { entityNum = value; }
  void SetUpdateWheelAngle(bool value) { updateWheelAngle = value; }
  void SetIsOnElevatorFlag(bool value) { isOnElevator = value; }

protected:
  bool InternalInit(const idAnimatorParms_Base&) override { return true; }
  void InternalShutdown(idAnimStack*) override {}
  void InternalPreBlendTree(const idAnimStack* stack, int curTime,
                            int ticksPerSec) override;
  void InternalPostBlendTree(const idAnimStack*, int) override {}
  void InternalStart(const idAnimStack*, int,
                     idTypesafeNumber<int, gameTimeUnique_t>) override {}
  void InternalEnd(const idAnimStack*, int,
                   idTypesafeNumber<int, gameTimeUnique_t>) override {}
  void InternalBlend(const idAnimStack*, int, float,
                     idTypesafeNumber<int, gameTimeUnique_t>) override {}
  bool InternalIsContributing() const override { return false; }
  const idMD6Branch* InternalGetMergeBranch() const override {
    return nullptr;
  }
  idMD6Branch* InternalGetMergeBranch() override { return nullptr; }

private:
  void Evaluate(const idAnimStack* stack);
  void SerializeSteerAngle(idSerializer& serializer);
  int InternalInitDialMeter(const idAnimStack* stack,
                            const dialMeter_t& properties);
  void InternalSetDialMeter(int dialIndex, float value) {
    if (dialIndex >= 0 && dialIndex < dialMeters.num) {
      dialMeters.list[dialIndex].value = value;
    }
  }
  idJointIndex_t InternalGetDialMeterJointIndex(int dialIndex) const;
  void DeleteAnimWheels();
  void DeleteDialMeters();

  const idDeclVehicleProps* vehicleProps;
  idList<idAnimWheel, 5> animWheels;
  idClip* clip;
  int entityNum;
  std::uint8_t renderModelPadding[4];
  idRenderModel* renderModel;
  idList<idAnimDialMeter, 5> dialMeters;
  int speedometer;
  int tachometer;
  int clipMask;
  bool accelerating;
  bool handBraking;
  bool isReversing;
  bool playerOccupant;
  bool rotateSteeringWheel;
  std::uint8_t steeringWheelPadding;
  idJointIndex_t steeringWheelJoint;
  idVec3 steeringWheelAxis;
  idQuat steeringWheelJointQuat;
  float steerAngle;
  float lastSteerAngle;
  float newSteerAngle;
  bool updateWheelAngle;
  bool isOnElevator;
  std::uint8_t tailPadding[2];

  friend class AnimatorVehicleCarValidationAccess;
};

static_assert(sizeof(wheelSlipInfo_t) == 0x0C,
              "wheelSlipInfo_t ABI drift");
static_assert(sizeof(idAnimSuspensionLever) == 0x30,
              "idAnimSuspensionLever ABI drift");
static_assert(sizeof(idAnimDialMeter) == 0x30,
              "idAnimDialMeter ABI drift");
static_assert(sizeof(idAnimWheel::tireMark_t) == 0x60,
              "idAnimWheel::tireMark_t ABI drift");
static_assert(sizeof(idAnimWheel) == 0x1A0, "idAnimWheel ABI drift");
static_assert(offsetof(idAnimWheel, wheelJointIndex) == 0x18,
              "idAnimWheel joint offset drift");
static_assert(offsetof(idAnimWheel, clipModel) == 0x20,
              "idAnimWheel clip-model offset drift");
static_assert(offsetof(idAnimWheel, wheelSteeringAxis) == 0x130,
              "idAnimWheel steering-axis offset drift");
static_assert(offsetof(idAnimWheel, collisionQuery) == 0x160,
              "idAnimWheel collision-query offset drift");
static_assert(offsetof(idAnimWheel, lastWorldOrigin) == 0x178,
              "idAnimWheel last-origin offset drift");
static_assert(sizeof(idAnimator_VehicleCar) == 0xE0,
              "idAnimator_VehicleCar ABI drift");
