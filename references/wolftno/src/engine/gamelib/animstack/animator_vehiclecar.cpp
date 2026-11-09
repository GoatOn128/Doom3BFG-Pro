#define WOLFTNO_RETAIL_IDLIST_FORCEINLINE_CLEARFREE_ABI
#define WOLFTNO_RETAIL_ANIMWEB_DECLMD6_LAYOUT_ABI
#define WOLFTNO_RETAIL_IDLIST_FORCEINLINE_CTOR_DTOR_ABI
#define WOLFTNO_RETAIL_IDLIST_EMPTY_CTOR_ABI
#define WOLFTNO_RETAIL_IDLIST_OUTOFLINE_APPEND_ABI
#define WOLFTNO_RETAIL_IDLIST_OUTOFLINE_RESIZE_ABI
#define WOLFTNO_RETAIL_IDMATH_CONSTANT_ABI
#define WOLFTNO_RETAIL_IDMEM_SINGLETON_ABI
#define WOLFTNO_RETAIL_IDSTR_INLINE_FREEDATA_ABI
#define WOLFTNO_RETAIL_SYNCED_DTOR_UNWIND_ABI
#include "engine/gamelib/animstack/animator_vehiclecar.h"

#include "engine/framework/gametimemanager.h"
#include "engine/gamelib/animstack/animstack.h"
#include "engine/models/rendermodel.h"
#include "engine/models/serializer.h"
#include "engine/models/skeletalanimation/md6skel.h"
#include "shared/idlib/geometry/tracemodel.h"
#include "shared/idlib/lib.h"
#include "shared/idlib/math/mathlib.h"

#undef WOLFTNO_RETAIL_SYNCED_DTOR_UNWIND_ABI
#undef WOLFTNO_RETAIL_IDSTR_INLINE_FREEDATA_ABI
#undef WOLFTNO_RETAIL_IDMEM_SINGLETON_ABI
#undef WOLFTNO_RETAIL_IDMATH_CONSTANT_ABI
#undef WOLFTNO_RETAIL_IDLIST_OUTOFLINE_RESIZE_ABI
#undef WOLFTNO_RETAIL_IDLIST_OUTOFLINE_APPEND_ABI
#undef WOLFTNO_RETAIL_IDLIST_EMPTY_CTOR_ABI
#undef WOLFTNO_RETAIL_IDLIST_FORCEINLINE_CTOR_DTOR_ABI
#undef WOLFTNO_RETAIL_IDLIST_FORCEINLINE_CLEARFREE_ABI
#undef WOLFTNO_RETAIL_ANIMWEB_DECLMD6_LAYOUT_ABI

#include <algorithm>
#include <cmath>
#include <cstddef>
#include <cstdint>
#include <cstring>
#include <new>
#include <xmmintrin.h>

class idEntity;
class idCollisionModelManager {
public:
  bool GetStepMoveResult(trace_t* result, idCollisionQuery& query,
                         bool peek);
};
extern idCollisionModelManager* collisionModelManager;

class idScenePoint {
public:
  virtual void ActivateTargets(idEntity* target);
};

template <typename Type>
void* idListArrayResize(void* oldPointer, int oldCount, int newCount,
                        memTag_t tag, bool zeroBuffer);

template <typename Type>
void idListArrayDelete(void* pointer, int count);

template <>
void* idListArrayResize<idAnimSuspensionLever>(
    void* oldPointer, int oldCount, int newCount, memTag_t tag,
    bool zeroBuffer);
template <>
void* idListArrayResize<idAnimDialMeter>(void* oldPointer, int oldCount,
                                         int newCount, memTag_t tag,
                                         bool zeroBuffer);
template <>
void* idListArrayResize<idAnimWheel>(void* oldPointer, int oldCount,
                                     int newCount, memTag_t tag,
                                     bool zeroBuffer);
template <>
void idListArrayDelete<idAnimWheel>(void* pointer, int count);

extern bool forceIdListsToReallocateEveryAppend;

namespace {

constexpr const char* kWheelTraceName =
    "w:\\build\\tech5\\engine\\gamelib\\animstack\\Animator_VehicleCar."
    "cpp(676) : Translation";

float RetailSqrt(float value) {
  return _mm_cvtss_f32(_mm_sqrt_ps(_mm_max_ps(
      _mm_set_ss(value), idMath::vector_float_smallestNonDenorm)));
}

float RetailLength(const idVec3& value) {
  return RetailSqrt(((value.x * value.x) + (value.y * value.y)) +
                    (value.z * value.z));
}

idVec3 TransformVector(const idMat3& axis, const idVec3& value) {
  return {
      ((value.y * axis.mat[1].x) + (value.x * axis.mat[0].x)) +
          (value.z * axis.mat[2].x),
      ((value.x * axis.mat[0].y) + (value.y * axis.mat[1].y)) +
          (value.z * axis.mat[2].y),
      ((value.x * axis.mat[0].z) + (value.y * axis.mat[1].z)) +
          (value.z * axis.mat[2].z),
  };
}

idMat3 MultiplyMatrix(const idMat3& left, const idMat3& right) {
  idMat3 result;
  for (int row = 0; row < 3; ++row) {
    for (int column = 0; column < 3; ++column) {
      result.mat[row][column] =
          ((left.mat[row].x * right.mat[0][column]) +
           (left.mat[row].y * right.mat[1][column])) +
          (left.mat[row].z * right.mat[2][column]);
    }
  }
  return result;
}

idMat3 TransposeMatrix(const idMat3& value) {
  idMat3 result;
  for (int row = 0; row < 3; ++row) {
    for (int column = 0; column < 3; ++column) {
      result.mat[row][column] = value.mat[column][row];
    }
  }
  return result;
}

idQuat MultiplyQuat(const idQuat& left, const idQuat& right) {
  return {
      ((right.w * left.x + right.x * left.w) + right.y * left.z) -
          right.z * left.y,
      ((right.w * left.y + right.y * left.w) + right.z * left.x) -
          right.x * left.z,
      ((right.w * left.z + right.z * left.w) + right.x * left.y) -
          right.y * left.x,
      ((right.w * left.w - right.x * left.x) - right.y * left.y) -
          right.z * left.z,
  };
}

idQuat NormalizeQuat(const idQuat& value) {
  const float inverseLength =
      1.0f /
      RetailSqrt(((value.x * value.x) + (value.y * value.y)) +
                 ((value.z * value.z) + (value.w * value.w)));
  return {value.x * inverseLength, value.y * inverseLength,
          value.z * inverseLength, value.w * inverseLength};
}

idQuat AxisAngleQuat(const idVec3& axis, float angle) {
  const float halfAngle = angle * 0.5f;
  const float sine = std::sinf(halfAngle);
  return NormalizeQuat(
      {axis.x * sine, axis.y * sine, axis.z * sine, std::cosf(halfAngle)});
}

idQuat ShortestArcQuat(const idVec3& from, const idVec3& to) {
  const idVec3 cross = from.Cross(to);
  return NormalizeQuat({cross.x, cross.y, cross.z, 1.0f + (from * to)});
}

idMat3 Mat3FromMat3x4(const idMat3x4& value) {
  idMat3 result;
  result.mat[0] = {value.mat[0], value.mat[1], value.mat[2]};
  result.mat[1] = {value.mat[4], value.mat[5], value.mat[6]};
  result.mat[2] = {value.mat[8], value.mat[9], value.mat[10]};
  return result;
}

idMat3x4 Mat3x4FromQuat(const idQuat& quaternion,
                        const idVec3& translation) {
  const idMat3 axis = quaternion.ToMat3();
  idMat3x4 result;
  result.mat[0] = axis.mat[0].x;
  result.mat[1] = axis.mat[0].y;
  result.mat[2] = axis.mat[0].z;
  result.mat[3] = translation.x;
  result.mat[4] = axis.mat[1].x;
  result.mat[5] = axis.mat[1].y;
  result.mat[6] = axis.mat[1].z;
  result.mat[7] = translation.y;
  result.mat[8] = axis.mat[2].x;
  result.mat[9] = axis.mat[2].y;
  result.mat[10] = axis.mat[2].z;
  result.mat[11] = translation.z;
  return result;
}

idVec3 WheelLocalOrigin(const idAnimWheel& wheel) {
  return {wheel.localJointOrigin.x, wheel.localJointOrigin.y,
          wheel.localJointOrigin.z};
}

template <typename Type>
bool ResizeVehicleList(idList<Type, 5>& values, int newSize) {
  if (newSize <= 0) {
    if (values.listStatic == 0 || values.listStatic == 2) {
      if (values.list != nullptr) {
        if constexpr (std::is_same_v<Type, idAnimWheel>) {
          idListArrayDelete<idAnimWheel>(values.list, values.size);
        } else {
          idMem::Singleton().Free(values.list, ALIGN_16);
        }
      }
      values.list = nullptr;
      values.size = 0;
    }
    values.num = 0;
    return true;
  }

  if (newSize == values.size || values.listStatic != 0) {
    return true;
  }
  if (values.size > 0 || newSize == values.granularity) {
    reinterpret_cast<idScenePoint*>(&values)->idScenePoint::ActivateTargets(
        reinterpret_cast<idEntity*>(static_cast<std::intptr_t>(newSize)));
  }
  values.list = static_cast<Type*>(idListArrayResize<Type>(
      values.list, values.size, newSize,
      static_cast<memTag_t>(values.memTag), false));
  if (values.list == nullptr) {
    return false;
  }
  values.size = newSize;
  if (values.num > newSize) {
    values.num = newSize;
  }
  return true;
}

idJointIndex_t FindJoint(const idJointAnimator* animator,
                         const char* jointName) {
  if (animator != nullptr && animator->decl != nullptr &&
      animator->decl->model != nullptr &&
      animator->decl->model->skeleton != nullptr) {
    return animator->decl->model->skeleton->GetJointIndex(jointName);
  }
  return idJointIndex_t(static_cast<std::int16_t>(-1));
}

}  // namespace

template <>
__declspec(noinline) bool
idList<idAnimSuspensionLever, 5>::Resize(int newSize) {
  return ResizeVehicleList(*this, newSize);
}

template <>
__declspec(noinline) bool idList<idAnimDialMeter, 5>::Resize(int newSize) {
  return ResizeVehicleList(*this, newSize);
}

template <>
__declspec(noinline) bool idList<idAnimWheel, 5>::Resize(int newSize) {
  return ResizeVehicleList(*this, newSize);
}

template <>
__declspec(noinline) bool idList<idAnimSuspensionLever, 5>::SetNum(
    int newNum, const idAnimSuspensionLever& initialValue) {
  if (newNum > size && !Resize(newNum)) {
    return false;
  }
  const int finalNum = newNum < size ? newNum : size;
  for (int index = num; index < finalNum; ++index) {
    list[index] = initialValue;
  }
  num = finalNum;
  return true;
}

template <>
__declspec(noinline) int idList<idAnimDialMeter, 5>::Append(
    const idAnimDialMeter& value) {
  if (list == nullptr) {
    const int initialSize = granularity > 0 ? granularity : 1;
    if (!Resize(initialSize)) {
      return -1;
    }
  }
  if (num == size) {
    int newSize;
    if (forceIdListsToReallocateEveryAppend) {
      newSize = size + 1;
    } else if (granularity != 0) {
      newSize = size + granularity - (size + granularity) % granularity;
    } else if (size <= 0x3FFFFFFF) {
      newSize = size * 2;
      if (newSize == 0) {
        newSize = 1;
      }
    } else {
      newSize = 0x7FFFFFFF;
    }
    if (!Resize(newSize)) {
      return -1;
    }
  }
  if (num >= size) {
    return num - 1;
  }
  list[num] = value;
  return num++;
}

template <>
idList<idAnimWheel, 5>::~idList() noexcept(false) {
  if (listStatic == 0 || listStatic == 2) {
    if (list != nullptr) {
      idListArrayDelete<idAnimWheel>(list, size);
    }
    list = nullptr;
    size = 0;
  }
  num = 0;
}

float idMath::ACos16(float value) {
  if (value >= 0.0f) {
    if (value < 1.0f) {
      return ((((0.074261002f - value * 0.018729299f) * value -
                 0.21211439f) *
                    value +
                1.5707288f) *
              RetailSqrt(1.0f - value));
    }
    return 0.0f;
  }
  if (value > -1.0f) {
    const float absolute = std::fabs(value);
    return 3.1415927f -
           ((((0.074261002f - absolute * 0.018729299f) * absolute -
               0.21211439f) *
                  absolute +
              1.5707288f) *
            RetailSqrt(1.0f - absolute));
  }
  return 3.1415927f;
}

idAnimator_VehicleCar::idAnimator_VehicleCar()
    : vehicleProps(nullptr), animWheels(), clip(nullptr), entityNum(0x1FFF),
      renderModel(nullptr), dialMeters(), speedometer(-1), tachometer(-1),
      clipMask(107553), accelerating(false), handBraking(false),
      isReversing(false), playerOccupant(false), rotateSteeringWheel(true),
      steeringWheelJoint(static_cast<std::int16_t>(-1)),
      steeringWheelAxis{1.0f, 0.0f, 0.0f},
      steeringWheelJointQuat(quat_identity), steerAngle(0.0f),
      lastSteerAngle(0.0f), newSteerAngle(0.0f), updateWheelAngle(true),
      isOnElevator(false) {
  idMem::Singleton();
  animWheels.list = nullptr;
  animWheels.granularity = 0;
  animWheels.memTag = 5;
  animWheels.listStatic = 0;
  animWheels.num = 0;
  animWheels.size = 0;
  idMem::Singleton();
  dialMeters.list = nullptr;
  dialMeters.granularity = 0;
  dialMeters.memTag = 5;
  dialMeters.listStatic = 0;
  dialMeters.num = 0;
  dialMeters.size = 0;
}

idAnimator_VehicleCar::~idAnimator_VehicleCar() {
  DeleteAnimWheels();
  DeleteDialMeters();
  clip = nullptr;
  vehicleProps = nullptr;
}

idAnimator_Base::priority_t idAnimator_VehicleCar::GetStackPriority() const {
  return PRIORITY_IK;
}

void idAnimator_VehicleCar::SetPlayerOccupant(bool value) {
  playerOccupant = value;
}

void idAnimator_VehicleCar::SetTireWobble(int wheelIndex,
                                          float wobbleFraction) {
  if (wheelIndex >= 0 && wheelIndex < animWheels.num) {
    animWheels.list[wheelIndex].wobbleFrac = wobbleFraction;
  }
}

idJointIndex_t idAnimator_VehicleCar::InternalGetDialMeterJointIndex(
    int dialIndex) const {
  idJointIndex_t result(static_cast<std::int16_t>(-1));
  if (dialIndex >= 0 || dialIndex < dialMeters.num) {
    result.value = dialMeters.list[dialIndex].joint.value;
  }
  return result;
}

void idAnimator_VehicleCar::DeleteDialMeters() {
  if (dialMeters.listStatic == 0 || dialMeters.listStatic == 2) {
    if (dialMeters.list != nullptr) {
      idMem::Singleton().Free(dialMeters.list, ALIGN_16);
    }
    dialMeters.list = nullptr;
    dialMeters.size = 0;
  }
  dialMeters.num = 0;
}

void idAnimator_VehicleCar::SetSpeedometer(float value) {
  if (speedometer >= 0 && speedometer < dialMeters.num) {
    dialMeters.list[speedometer].value = value;
  }
}

void idAnimator_VehicleCar::SetTachometer(float value) {
  if (tachometer >= 0 && tachometer < dialMeters.num) {
    dialMeters.list[tachometer].value = value;
  }
}

void idAnimator_VehicleCar::InternalPreBlendTree(const idAnimStack* stack,
                                                  int, int) {
  Evaluate(stack);
}

void idAnimator_VehicleCar::SerializeSteerAngle(idSerializer& serializer) {
  const float currentSteerAngle = steerAngle;
  lastSteerAngle = currentSteerAngle;
  if (serializer.writing) {
    int quantized = static_cast<int>(currentSteerAngle);
    if (quantized < -127) {
      quantized = -127;
    } else if (quantized > 127) {
      quantized = 127;
    }
    serializer.msg->WriteBits(quantized, -8);
    steerAngle = currentSteerAngle;
    newSteerAngle = currentSteerAngle;
  } else {
    const float deserialized =
        static_cast<float>(serializer.msg->ReadBits(-8)) * 0.70866144f;
    steerAngle = deserialized;
    newSteerAngle = deserialized;
  }
}

void idAnimator_VehicleCar::PreBlendSnapshot(idAnimStack* stack, int, int,
                                               float fraction) {
  steerAngle = (newSteerAngle - lastSteerAngle) * fraction + lastSteerAngle;
  Evaluate(stack);
}

void idAnimator_VehicleCar::DeleteAnimWheels() {
  for (int index = 0; index < animWheels.num; ++index) {
    if (animWheels.list[index].clipModel != nullptr) {
      animWheels.list[index].clipModel->Delete();
    }
  }
  if (animWheels.listStatic == 0 || animWheels.listStatic == 2) {
    if (animWheels.list != nullptr) {
      idListArrayDelete<idAnimWheel>(animWheels.list, animWheels.size);
    }
    animWheels.list = nullptr;
    animWheels.size = 0;
  }
  animWheels.num = 0;
}

int idAnimator_VehicleCar::InternalInitDialMeter(
    const idAnimStack* stack, const dialMeter_t& properties) {
  idJointAnimator* const jointAnimator = stack->jointAnimator;
  const idJointIndex_t joint = FindJoint(jointAnimator,
                                         properties.jointName.c_str());
  if (joint.value < 0) {
    return -1;
  }

  float direction = 0.0f;
  if (properties.direction == DIAL_ROTATE_LEFT ||
      properties.direction == DIAL_MOVE_NEGATIVE) {
    direction = -1.0f;
  } else if (properties.direction > DIAL_ROTATE_LEFT &&
             properties.direction <= DIAL_MOVE_POSITIVE) {
    direction = 1.0f;
  }

  int axisIndex = 0;
  if (properties.axis == DIAL_AXIS_Y) {
    axisIndex = 1;
  } else if (properties.axis == DIAL_AXIS_Z) {
    axisIndex = 2;
  }

  idVec3 origin;
  idMat3 orientation;
  jointAnimator->GetModelSpaceJointTransform(DRIVER_MODEL, joint, origin,
                                               orientation);
  idAnimDialMeter dial;
  dial.joint = joint;
  dial.axis = mat3_identity.mat[axisIndex] * direction;
  dial.pos = origin;
  dial.rot = orientation.ToQuat();
  dial.value = 0.0f;
  return dialMeters.Append(dial);
}

void idAnimator_VehicleCar::InitVehicleAnimator(
    idGameTimeManager* gameTimeManager,
    const idDeclVehicleProps* vehicleProperties, idAnimStack* animStack,
    idRenderModel* model, int newEntityNum, idClip* newClip) {
  if (vehicleProperties == nullptr) {
    idLib::Warning("idAnimator_VehicleCar: vehicleProps == NULL for '%s'",
                   "<unknown>");
    return;
  }

  idAnimatorParms_Base parameters(
      animStack, "vehicleAnimator", idMD6Blend::BOP_LERP,
      idMD6Blend::ORIGINBLEND_BRANCH, MD6_WEIGHTGROUP_ALL,
      MD6_WEIGHTGROUP_ALL, 1.0f);
  Init(gameTimeManager, parameters);
  vehicleProps = vehicleProperties;
  clip = newClip;
  entityNum = newEntityNum;
  renderModel = model;

  idJointAnimator* const jointAnimator = animStack->jointAnimator;
  steeringWheelJoint =
      FindJoint(jointAnimator, vehicleProps->steeringWheel.jointName.c_str());
  if (steeringWheelJoint.value >= 0) {
    idVec3 steeringOrigin;
    idMat3 steeringAxis;
    jointAnimator->GetModelSpaceJointTransform(
        DRIVER_MODEL, steeringWheelJoint, steeringOrigin, steeringAxis);
    const float direction =
        vehicleProps->steeringWheel.rotationDirection ==
                steeringWheel_t::ROTATE_LEFT
            ? -1.0f
            : 1.0f;
    steeringWheelAxis =
        steeringAxis.mat[vehicleProps->steeringWheel.rotationAxis] * direction;
    steeringWheelJointQuat = TransposeMatrix(steeringAxis).ToQuat();
  }

  DeleteDialMeters();
  speedometer = InternalInitDialMeter(animStack, vehicleProps->speedometer);
  tachometer = InternalInitDialMeter(animStack, vehicleProps->tachometer);

  idAnimWheel defaultWheel;
  DeleteAnimWheels();
  const int wheelCount = vehicleProps->wheels.num;
  if (wheelCount <= animWheels.size ||
      ResizeVehicleList(animWheels, wheelCount)) {
    const int finalCount = wheelCount < animWheels.size ? wheelCount
                                                        : animWheels.size;
    for (int index = animWheels.num; index < finalCount; ++index) {
      animWheels.list[index] = defaultWheel;
    }
    animWheels.num = finalCount;
  }

  for (int wheelIndex = 0; wheelIndex < animWheels.num; ++wheelIndex) {
    const idWheelDef& wheelDefinition = vehicleProps->wheels.list[wheelIndex];
    idAnimWheel& wheel = animWheels.list[wheelIndex];
    wheel.wheelJointIndex =
        FindJoint(jointAnimator, wheelDefinition.wheelJoint.str);
    if (wheel.wheelJointIndex.value < 0) {
      idLib::Error("idAnimator_VehicleCar '%s' can't find wheel joint '%s'",
                   "<unknown>", wheelDefinition.wheelJoint.str);
      return;
    }

    idVec3 wheelOrigin;
    idMat3 wheelAxis;
    jointAnimator->GetModelSpaceJointTransform(
        DRIVER_MODEL, wheel.wheelJointIndex, wheelOrigin, wheelAxis);
    wheel.localJointQuat = TransposeMatrix(wheelAxis).ToQuat();

    idAnimSuspensionLever defaultLever;
    if (wheel.suspensionLever.listStatic == 0 ||
        wheel.suspensionLever.listStatic == 2) {
      if (wheel.suspensionLever.list != nullptr) {
        idMem::Singleton().Free(wheel.suspensionLever.list, ALIGN_16);
      }
      wheel.suspensionLever.list = nullptr;
      wheel.suspensionLever.size = 0;
    }
    wheel.suspensionLever.num = 0;
    wheel.suspensionLever.SetNum(wheelDefinition.suspensionLever.num,
                                 defaultLever);
    for (int leverIndex = 0;
         leverIndex < wheelDefinition.suspensionLever.num; ++leverIndex) {
      const idSuspensionLeverDef& leverDefinition =
          wheelDefinition.suspensionLever.list[leverIndex];
      idAnimSuspensionLever& lever = wheel.suspensionLever.list[leverIndex];
      if (*leverDefinition.joint.str == '\0') {
        return;
      }
      lever.jointIndex = FindJoint(jointAnimator, leverDefinition.joint.str);
      if (lever.jointIndex.value < 0) {
        idLib::Error(
            "idAnimator_VehicleCar '%s' can't find first lever joint '%s'",
            "<unknown>", leverDefinition.joint.str);
        return;
      }

      idVec3 leverOrigin;
      idMat3 leverAxis;
      jointAnimator->GetModelSpaceJointTransform(
          DRIVER_MODEL, lever.jointIndex, leverOrigin, leverAxis);
      switch (leverDefinition.direction) {
        case idSuspensionLeverDef::DIR_LEFT:
          lever.rotationAxis = {-1.0f, 0.0f, 0.0f};
          break;
        case idSuspensionLeverDef::DIR_FORWARD:
          lever.rotationAxis = {0.0f, 1.0f, 0.0f};
          break;
        case idSuspensionLeverDef::DIR_BACKWARD:
          lever.rotationAxis = {0.0f, -1.0f, 0.0f};
          break;
        case idSuspensionLeverDef::DIR_ALIGN_TO:
          lever.alignToJointIndex =
              FindJoint(jointAnimator, leverDefinition.alignToJoint.str);
          if (lever.jointIndex.value < 0) {
            idLib::Error(
                "idAnimator_VehicleCar '%s' can't find alignTo joint '%s'",
                "<unknown>", leverDefinition.joint.str);
          }
          lever.rotationAxis = leverAxis.mat[1];
          break;
        case idSuspensionLeverDef::DIR_RIGHT:
        default:
          lever.rotationAxis = {1.0f, 0.0f, 0.0f};
          break;
      }
      lever.alignAxis = leverAxis.mat[2];
      const idVec3 wheelDelta = wheelOrigin - leverOrigin;
      const float alongAxis = wheelDelta * lever.rotationAxis;
      const idVec3 perpendicular =
          wheelDelta - lever.rotationAxis * alongAxis;
      lever.invLength = -1.0f / RetailLength(perpendicular);
      lever.localJointQuat = TransposeMatrix(leverAxis).ToQuat();
    }

    wheel.localJointOrigin.x = wheelOrigin.x;
    wheel.localJointOrigin.y = wheelOrigin.y + wheelDefinition.wheelOffset;
    wheel.localJointOrigin.z = wheelOrigin.z;

    idTraceModel traceModel;
    traceModel.SetupCylinder(5.0f, wheelDefinition.wheelRadius * 2.0f, 8);
    traceModel.Rotate(
        idQuat{0.70710677f, 0.0f, 0.0f, 0.70710677f}.ToMat3());
    traceModel.Scale(renderModel->g.scale);
    idMem& memory = idMem::Singleton();
    void* allocation = memory.AllocWithLocation(
        "w:\\build\\tech5\\shared\\idlib\\Heap.h(95) : tag",
        sizeof(idClipModel), TAG_CLIPMODEL, false, ALIGN_16,
        HEAP_DEFAULTHEAP);
    try {
      wheel.clipModel =
          allocation != nullptr
              ? ::new (allocation) idClipModel(newClip, &traceModel, 1, nullptr)
              : nullptr;
    } catch (...) {
      memory.Free(allocation, ALIGN_16);
      throw;
    }
    wheel.clipModel->SetContents(197120);
    wheel.dustTime = 0;
    wheel.lastDustPrtDropPos = {wheel.localJointOrigin.x,
                                wheel.localJointOrigin.y,
                                wheel.localJointOrigin.z};
    wheel.lastDustPrtDropPos2 = wheel.lastDustPrtDropPos;
  }
}

void idAnimator_VehicleCar::Evaluate(const idAnimStack* stack) {
  if (clip == nullptr || renderModel == nullptr || vehicleProps == nullptr ||
      (animWheels.list->clipModel->contents & 0x1000) != 0) {
    return;
  }

  const idMat3& startAxis = renderModel->useDeferredPosition
                                ? renderModel->deferredAxis
                                : renderModel->g.axis;
  const idVec3& startOrigin = renderModel->useDeferredPosition
                                  ? renderModel->deferredOrigin
                                  : renderModel->g.origin;
  const float inverseFrameSeconds =
      1.0f / (gametimeManager.gameTimeManager->GetRealMsPerFrame(
                  GAMETIME_SCALED) *
              0.001f);

  for (int wheelIndex = 0; wheelIndex < animWheels.num; ++wheelIndex) {
    idAnimWheel& wheel = animWheels.list[wheelIndex];
    const idWheelDef& definition = vehicleProps->wheels.list[wheelIndex];
    if (definition.copyFromWheel != -1) {
      const idAnimWheel& source = animWheels.list[definition.copyFromWheel];
      wheel.wheelAngle = source.wheelAngle;
      wheel.wheelHeight = source.wheelHeight;
      continue;
    }

    const idVec3 localOrigin = WheelLocalOrigin(wheel);
    const idVec3 worldOrigin = startOrigin + TransformVector(startAxis, localOrigin);
    const float suspensionRange =
        definition.suspensionUp + definition.suspensionDown;
    const float suspensionQuarter = suspensionRange * 0.25f;
    const idVec3 traceStart =
        worldOrigin +
        startAxis.mat[2] * (definition.suspensionUp + suspensionQuarter);
    const idVec3 traceEnd =
        worldOrigin - startAxis.mat[2] * definition.suspensionDown;
    const idMat3 wheelSteering = Mat3FromMat3x4(wheel.wheelSteeringAxis);
    const idMat3 collisionAxis = MultiplyMatrix(startAxis, wheelSteering);

    bool onGround = false;
    if (clip->QueryIsValid(wheel.collisionQuery)) {
      wheel.prevTraceFraction = wheel.traceFraction;
      idCollisionQuery& query =
          clip->collisionQueries[wheel.collisionQuery.index & 0xFFF].query;
      wheel.collisionQuery.index = 0;
      trace_t trace;
      collisionModelManager->GetStepMoveResult(&trace, query, false);
      wheel.traceFraction = trace.fraction;
      wheel.traceSurfaceType = trace.c.surfaceType;
      wheel.traceSurfaceColor[0] = trace.c.surfaceColor[0];
      wheel.traceSurfaceColor[1] = trace.c.surfaceColor[1];
      wheel.traceSurfaceColor[2] = trace.c.surfaceColor[2];
      onGround = trace.fraction < 1.0f;
      const float expandedRange = suspensionQuarter + suspensionRange;
      if (onGround && std::fabs(trace.c.normal * collisionAxis.mat[2]) <
                          0.2f) {
        wheel.traceFraction =
            ((definition.suspensionUp - wheel.wheelHeight) +
             suspensionQuarter) /
            expandedRange;
      }
      wheel.wheelHeight =
          definition.suspensionUp -
          (expandedRange * wheel.traceFraction - suspensionQuarter);
    }

    wheel.collisionQuery =
        clip->Translation(nullptr, traceStart, traceEnd, wheel.clipModel,
                          startAxis, clipMask, entityNum, false,
                          kWheelTraceName);

    float contactWeight = 0.0f;
    if (onGround) {
      contactWeight = 1.0f;
      if (definition.steering < 1.0f &&
          -wheel.wheelHeight > definition.suspensionDown * 0.5f) {
        contactWeight =
            (1.0f - -(wheel.wheelHeight / definition.suspensionDown)) *
            2.0f;
      }
    }

    idVec3 worldVelocity =
        (worldOrigin - wheel.lastWorldOrigin) * inverseFrameSeconds;
    if (isOnElevator) {
      worldVelocity.z = 0.0f;
    }
    wheel.lastWorldOrigin = worldOrigin;
    const float forwardVelocity = worldVelocity * collisionAxis.mat[0];

    float targetVelocity = 0.0f;
    float accelerationRate = 0.0f;
    if (onGround || accelerating || isReversing || handBraking) {
      if (handBraking && definition.hasHandbrake) {
        targetVelocity = 0.0f;
      } else {
        targetVelocity = forwardVelocity * contactWeight;
      }
      if (accelerating) {
        targetVelocity += (1.0f - contactWeight) * 20.0f * 60.0f;
      } else if (isReversing) {
        targetVelocity -= (1.0f - contactWeight) * 10.0f * 60.0f;
      }
      accelerationRate = 1000.0f;
    }

    wheel.wheelVelocity *= 0.99000001f;
    const float velocityStep =
        gametimeManager.gameTimeManager->GetRealMsPerFrame(GAMETIME_SCALED) *
        0.001f * accelerationRate;
    if (targetVelocity <= wheel.wheelVelocity) {
      wheel.wheelVelocity =
          std::fmax(targetVelocity, wheel.wheelVelocity - velocityStep);
    } else {
      wheel.wheelVelocity =
          std::fmin(targetVelocity, wheel.wheelVelocity + velocityStep);
    }

    if (updateWheelAngle) {
      wheel.wheelAngle +=
          wheel.wheelVelocity /
          (inverseFrameSeconds * definition.wheelRadius);
      if (wheel.wheelAngle >= idMath::TWO_PI || wheel.wheelAngle < 0.0f) {
        wheel.wheelAngle -=
            std::floor((1.0f / idMath::TWO_PI) * wheel.wheelAngle) *
            idMath::TWO_PI;
      }
    }

    const float inverseSpeed = 1.0f / RetailLength(worldVelocity);
    idVec3 velocityDirection = worldVelocity * inverseSpeed;
    if ((velocityDirection * startAxis.mat[0]) < 0.0f) {
      velocityDirection = -velocityDirection;
    }
    float lateralSlip = 0.0f;
    if (std::fabs(forwardVelocity) >= 1.0f) {
      lateralSlip =
          idMath::ACos16(velocityDirection * collisionAxis.mat[0]) *
          (2.0f / 3.1415927f);
      if (lateralSlip > 1.0f) {
        lateralSlip = 1.0f;
      }
    }
    const float longitudinalSlip = std::fmax(0.0f, 0.0f);
    wheel.wheelSlip = RetailSqrt(lateralSlip * lateralSlip +
                                 longitudinalSlip * longitudinalSlip);
  }

  idJointAnimator* const jointAnimator = stack->jointAnimator;
  for (int wheelIndex = 0; wheelIndex < animWheels.num; ++wheelIndex) {
    idAnimWheel& wheel = animWheels.list[wheelIndex];
    const idWheelDef& definition = vehicleProps->wheels.list[wheelIndex];
    for (int leverIndex = 0; leverIndex < wheel.suspensionLever.num;
         ++leverIndex) {
      idAnimSuspensionLever& lever =
          wheel.suspensionLever.list[leverIndex];
      if (lever.jointIndex.value == -1) {
        continue;
      }

      idQuat rotation;
      if (definition.suspensionLever.list[leverIndex].direction ==
          idSuspensionLeverDef::DIR_ALIGN_TO) {
        idVec3 alignOrigin;
        idMat3 alignOrientation;
        idVec3 leverOrigin;
        idMat3 leverOrientation;
        jointAnimator->GetModelSpaceJointTransform(
            DRIVER_MODEL, lever.alignToJointIndex, alignOrigin,
            alignOrientation);
        jointAnimator->GetModelSpaceJointTransform(
            DRIVER_MODEL, lever.jointIndex, leverOrigin, leverOrientation);
        const idVec3 delta = alignOrigin - leverOrigin;
        idVec3 direction =
            delta - lever.rotationAxis * (delta * lever.rotationAxis);
        direction *= 1.0f / RetailLength(direction);
        rotation = ShortestArcQuat(lever.alignAxis, direction);
      } else {
        const float sine = wheel.wheelHeight * lever.invLength;
        const float cosine =
            RetailSqrt(std::fabs(1.0f - sine * sine)) + 1.0f;
        rotation = NormalizeQuat(
            {lever.rotationAxis.x * sine, lever.rotationAxis.y * sine,
             lever.rotationAxis.z * sine, cosine});
      }
      const idQuat finalRotation =
          MultiplyQuat(lever.localJointQuat, rotation);
      const idMat3x4 transform =
          Mat3x4FromQuat(finalRotation, vec3_origin);
      jointAnimator->SetJointMat3x4(ANIMATION_POSE_CURRENT,
                                    lever.jointIndex,
                                    JOINTMOD_MODEL_OVERRIDE, transform,
                                    false);
    }

    const float wobbleAngle =
        wheel.wobbleFrac * wheel.wobbleFrac * definition.wobbleScale *
        idMath::M_DEG2RAD;
    const float steeringAngle =
        -steerAngle * definition.steering * idMath::M_DEG2RAD;
    const idQuat wobbleRotation =
        AxisAngleQuat({0.0f, 0.0f, 1.0f}, wobbleAngle);
    const idQuat wheelRotation =
        AxisAngleQuat({0.0f, 1.0f, 0.0f}, wheel.wheelAngle);
    const idQuat steeringRotation =
        AxisAngleQuat({0.0f, 0.0f, 1.0f}, steeringAngle);
    idQuat finalRotation =
        MultiplyQuat(wheel.localJointQuat, wobbleRotation);
    finalRotation = MultiplyQuat(finalRotation, wheelRotation);
    finalRotation = MultiplyQuat(finalRotation, steeringRotation);
    finalRotation = NormalizeQuat(finalRotation);
    const idVec3 localOrigin = WheelLocalOrigin(wheel);
    const idMat3x4 wheelTransform =
        Mat3x4FromQuat(finalRotation, localOrigin);
    wheel.wheelSteeringAxis =
        Mat3x4FromQuat(steeringRotation, localOrigin);
    jointAnimator->SetJointMat3x4(
        ANIMATION_POSE_CURRENT, wheel.wheelJointIndex,
        JOINTMOD_MODEL_OVERRIDE, wheelTransform,
        wheel.suspensionLever.num == 0);
  }

  if (rotateSteeringWheel && steeringWheelJoint.value >= 0) {
    const idQuat steeringRotation = AxisAngleQuat(
        steeringWheelAxis, steerAngle * idMath::M_DEG2RAD);
    const idQuat finalRotation = NormalizeQuat(
        MultiplyQuat(steeringWheelJointQuat, steeringRotation));
    const idMat3x4 transform =
        Mat3x4FromQuat(finalRotation, vec3_origin);
    jointAnimator->SetJointMat3x4(DRIVER_MODEL, steeringWheelJoint,
                                  JOINTMOD_MODEL_OVERRIDE, transform,
                                  false);
  }

  for (int dialIndex = 0; dialIndex < dialMeters.num; ++dialIndex) {
    idAnimDialMeter& dial = dialMeters.list[dialIndex];
    const float halfAngle = dial.value * 0.5f;
    const float sine = std::sinf(halfAngle);
    const idQuat dialRotation{dial.axis.x * sine, dial.axis.y * sine,
                              dial.axis.z * sine, std::cosf(halfAngle)};
    const idQuat finalRotation = MultiplyQuat(dial.rot, dialRotation);
    const idMat3 finalAxis = finalRotation.ToMat3();
    jointAnimator->SetJointPosAxisScale(
        DRIVER_MODEL, dial.joint, JOINTMOD_MODEL_OVERRIDE, dial.pos,
        finalAxis, vec3_unit);
  }
}

void idAnimator_VehicleCar::SerializeSnapshot(idSerializer& serializer) {
  const idDeclVehicleProps* const oldProperties = vehicleProps;
  if (serializer.writing) {
    idDeclVehicleProps::resourceList.WriteResourceToMsg(vehicleProps,
                                                        *serializer.msg);
  } else {
    vehicleProps = static_cast<const idDeclVehicleProps*>(
        idDeclVehicleProps::resourceList.ReadResourceFromMsg(
            *serializer.msg));
  }
  if (oldProperties != vehicleProps) {
    InitVehicleAnimator(
        gametimeManager.gameTimeManager, vehicleProps,
        serializeProps.animStack,
        reinterpret_cast<idRenderModel*>(serializeProps.treeAnimator), -1,
        serializeProps.clip);
  }
  SerializeSteerAngle(serializer);
  for (int wheelIndex = 0; wheelIndex < animWheels.num; ++wheelIndex) {
    idAnimWheel& wheel = animWheels.list[wheelIndex];
    if (wheel.wobbleFrac > 1.0f) {
      wheel.wobbleFrac = 1.0f;
    }
    if (serializer.writing) {
      int quantized = static_cast<int>(wheel.wobbleFrac * 15.0f);
      if (quantized < 0) {
        quantized = 0;
      } else if (quantized > 15) {
        quantized = 15;
      }
      serializer.msg->WriteBits(quantized, 4);
    } else {
      wheel.wobbleFrac =
          static_cast<float>(serializer.msg->ReadBits(4)) * 0.06666667f;
    }
  }
}
