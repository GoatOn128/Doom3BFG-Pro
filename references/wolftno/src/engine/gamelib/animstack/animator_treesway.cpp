#define WOLFTNO_RETAIL_IDMEM_SINGLETON_ABI
#include "engine/gamelib/animstack/animator_treesway.h"
#undef WOLFTNO_RETAIL_IDMEM_SINGLETON_ABI

#include "engine/gamelib/animstack/animator_treesway_abi.h"
#include "shared/idlib/math/mathlib.h"

#include <cmath>
#include <cstdint>
#include <xmmintrin.h>

namespace {

constexpr float kRandToUnit = 0.000030518509f;

const idMD6Model* GetDeclModel(const idDeclMD6* decl) {
  return *reinterpret_cast<idMD6Model* const*>(
      reinterpret_cast<const std::uint8_t*>(decl) + 0x60);
}

float SqrtClamped(float value) {
  const __m128 smallestNonDenormal = _mm_set1_ps(1.175494351e-38f);
  return _mm_cvtss_f32(
      _mm_sqrt_ps(_mm_max_ps(_mm_set_ss(value), smallestNonDenormal)));
}

idQuat Mat3ToQuat(const idMat3& matrix) {
  idQuat result;
  const float diagonal0 = matrix.mat[0].x;
  const float diagonal1 = matrix.mat[1].y;
  const float diagonal2 = matrix.mat[2].z;
  const float trace = (diagonal0 + diagonal1) + diagonal2;

  if (trace > 0.0f) {
    const float tracePlusOne = trace + 1.0f;
    const float inverseRoot = 1.0f / SqrtClamped(tracePlusOne);
    const float scale = inverseRoot * 0.5f;
    result.x = (matrix.mat[2].y - matrix.mat[1].z) * scale;
    result.w = scale * tracePlusOne;
    result.z = (matrix.mat[1].x - matrix.mat[0].y) * scale;
    result.y = (matrix.mat[0].z - matrix.mat[2].x) * scale;
    return result;
  }

  static constexpr int next[3] = {1, 2, 0};
  int index = diagonal1 > diagonal0 ? 1 : 0;
  if (diagonal2 > matrix.mat[index][index]) {
    index = 2;
  }
  const int nextIndex = next[index];
  const int finalIndex = next[nextIndex];
  const float rootValue =
      (matrix.mat[index][index] -
       (matrix.mat[finalIndex][finalIndex] +
        matrix.mat[nextIndex][nextIndex])) +
      1.0f;
  float scale = (1.0f / SqrtClamped(rootValue)) * 0.5f;
  const float difference = matrix.mat[finalIndex][nextIndex] -
                           matrix.mat[nextIndex][finalIndex];
  if (difference < 0.0f) {
    scale = -scale;
  }
  result[index] = scale * rootValue;
  result.w = difference * scale;
  result[nextIndex] =
      (matrix.mat[index][nextIndex] + matrix.mat[nextIndex][index]) * scale;
  result[finalIndex] =
      (matrix.mat[index][finalIndex] + matrix.mat[finalIndex][index]) * scale;
  return result;
}

idQuat MultiplyQuat(const idQuat& left, const idQuat& right) {
  idQuat result;
  result.x = (((left.x * right.w) + (right.x * left.w)) +
              (left.y * right.z)) -
             (left.z * right.y);
  result.y = (((left.y * right.w) + (right.y * left.w)) +
              (left.z * right.x)) -
             (right.z * left.x);
  result.w = (((right.w * left.w) - (left.x * right.x)) -
              (right.y * left.y)) -
             (left.z * right.z);
  result.z = (((left.z * right.w) + (right.z * left.w)) +
              (right.y * left.x)) -
             (left.y * right.x);
  return result;
}

idMat3 QuatToMat3(const idQuat& quaternion) {
  idMat3 result;
  const float z = quaternion.z;
  const float twiceX = quaternion.x + quaternion.x;
  const float xz = quaternion.x * (z + z);
  const float y = quaternion.y;
  const float xx = quaternion.x * twiceX;
  const float wx = quaternion.w * twiceX;
  const float twiceY = y + y;
  const float yz = y * (z + z);
  const float yy = y * (y + y);
  const float xy = quaternion.x * twiceY;
  const float wy = quaternion.w * twiceY;
  const float wz = quaternion.w * (z + z);
  const float zz = z * (z + z);

  result.mat[0].x = 1.0f - (zz + yy);
  result.mat[0].y = xy - wz;
  result.mat[0].z = wy + xz;
  result.mat[2].x = xz - wy;
  result.mat[1].y = 1.0f - (zz + xx);
  result.mat[1].x = wz + xy;
  result.mat[1].z = yz - wx;
  result.mat[2].y = wx + yz;
  result.mat[2].z = 1.0f - (yy + xx);
  return result;
}

std::uint32_t NextRandom(std::uint32_t value) {
  return 1664525u * value + 1013904223u;
}

float Random15(std::uint32_t value) {
  return static_cast<float>((value >> 10) & 0x7fffu);
}

}  // namespace

void idMath::SinCos16(float a, float& sine, float& cosine) {
  float value = a;
  if (a < 0.0f || a >= TWO_PI) {
    value = a - (::floorf(a * 0.15915494f) * TWO_PI);
  }

  float cosineSign;
  if (!(value < PI)) {
    if (value > 4.712389f) {
      value = value - TWO_PI;
      cosineSign = 1.0f;
    } else {
      cosineSign = -1.0f;
      value = PI - value;
    }
  } else if (value > 1.5707964f) {
    cosineSign = -1.0f;
    value = PI - value;
  } else {
    cosineSign = 1.0f;
  }

  const float square = value * value;
  float sinePolynomial =
      0.0000027526f - (square * 0.0000000239f);
  sinePolynomial = (sinePolynomial * square) - 0.000198409f;
  sinePolynomial = (sinePolynomial * square) + 0.0083333319f;
  sinePolynomial = (sinePolynomial * square) - 0.16666667f;
  sine = ((sinePolynomial * square) + 1.0f) * value;

  float cosinePolynomial =
      0.0000247609f - (square * 0.0000002605f);
  cosinePolynomial = (cosinePolynomial * square) - 0.0013888397f;
  cosinePolynomial = (cosinePolynomial * square) + 0.041666642f;
  cosinePolynomial = (cosinePolynomial * square) - 0.5f;
  cosine = ((cosinePolynomial * square) + 1.0f) * cosineSign;
}

idAnimator_TreeSway::idAnimator_TreeSway()
    : timeDiff(0.2f),
      timeScale(1.0f),
      rotationOffset(quat_identity),
      rotationScale{0.005f, 0.005f, 0.005f},
      translateScale{1.0f, 1.0f, 1.0f} {}

idAnimator_TreeSway::~idAnimator_TreeSway() = default;

void idAnimator_TreeSway::InternalPreBlendTree(const idAnimStack* stack,
                                               int curTime,
                                               int ticksPerSec) {
  (void)ticksPerSec;

  idJointAnimator* const jointAnimator = stack->jointAnimator;
  if (jointAnimator == nullptr) {
    return;
  }

  const idDeclMD6* const decl = jointAnimator->decl;
  if (decl == nullptr) {
    return;
  }

  const idMD6Model* model = GetDeclModel(decl);
  if (model == nullptr) {
    return;
  }

  const idMD6Skel* const skeleton = model->skeleton;
  if (skeleton == nullptr) {
    return;
  }

  const float translateX = translateScale.x;
  const float translateY = translateScale.y;
  const float translateZ = translateScale.z;
  const float rotateX = idMath::PI * rotationScale.x;
  const float rotateZ = idMath::PI * rotationScale.z;
  const float rotateY = idMath::PI * rotationScale.y;
  std::uint32_t random =
      NextRandom(static_cast<std::uint32_t>(
          reinterpret_cast<std::uintptr_t>(stack)));
  int jointNumber = 0;
  idMD6SkelData* const data = skeleton->data;
  const auto* const jointHandles =
      reinterpret_cast<const idJointHandle_t*>(
          reinterpret_cast<const std::uint8_t*>(data) +
          data->jointHandleTblOffset);
  const float baseTime =
      ((Random15(random) * kRandToUnit) * (idMath::TWO_PI - 0.0f)) +
      (static_cast<float>(curTime) * 0.001f);

  if (data->numJoints == 0) {
    return;
  }

  do {
    idJointIndex_t jointIndex(static_cast<std::int16_t>(-1));
    model = GetDeclModel(decl);
    if (model != nullptr && model->skeleton != nullptr) {
      jointIndex = model->skeleton->GetJointIndex(jointHandles[jointNumber]);
    }

    if (jointIndex.value > 0) {
      idVec3 origin;
      idMat3 jointAxis;
      if (jointAnimator->GetModelSpaceJointTransform(
              ANIMATION_POSE_CURRENT, jointIndex, origin, jointAxis)) {
        const float difference = timeDiff;
        const float scale = timeScale;
        const float minimumSpeed = 1.0f - difference;
        const float maximumSpeed = difference + 1.0f;
        const float speedRange = maximumSpeed - minimumSpeed;

        const std::uint32_t random1 = NextRandom(random);
        const std::uint32_t random2 = NextRandom(random1);
        const std::uint32_t random3 = NextRandom(random2);
        const std::uint32_t random4 = NextRandom(random3);
        const std::uint32_t random5 = NextRandom(random4);
        const std::uint32_t random6 = NextRandom(random5);
        random = random6;

        const float xPhase =
            ((Random15(random2) * kRandToUnit) *
             (idMath::TWO_PI - 0.0f)) +
            0.0f;
        const float xSpeed =
            ((Random15(random1) * kRandToUnit) * speedRange) +
            minimumSpeed;
        const float xTime =
            xPhase + ((xSpeed * baseTime) * scale);

        const float yPhase =
            ((Random15(random4) * kRandToUnit) *
             (idMath::TWO_PI - 0.0f)) +
            0.0f;
        const float ySpeed =
            ((Random15(random3) * kRandToUnit) * speedRange) +
            minimumSpeed;
        const float yTime =
            yPhase + ((ySpeed * baseTime) * scale);

        const float zSpeed =
            ((Random15(random5) * kRandToUnit) * speedRange) +
            minimumSpeed;
        const float zPhase =
            ((Random15(random6) * kRandToUnit) *
             (idMath::TWO_PI - 0.0f)) +
            0.0f;
        const float zTime =
            zPhase + ((zSpeed * baseTime) * scale);

        float sineX;
        float cosineX;
        float sineY;
        float cosineY;
        float sineZ;
        float cosineZ;
        idMath::SinCos16(xTime, sineX, cosineX);
        idMath::SinCos16(yTime, sineY, cosineY);
        idMath::SinCos16(zTime, sineZ, cosineZ);

        origin.x = origin.x + (translateX * sineX);
        origin.y = origin.y + (translateY * sineY);
        origin.z = origin.z + (translateZ * sineZ);

        const float xHalfAngle = (rotateX * cosineX) * 0.5f;
        const float xSine = ::sinf(xHalfAngle);
        const float xCosine = ::cosf(xHalfAngle);
        const idQuat xQuaternion{xSine, xSine * 0.0f,
                                 xSine * 0.0f, xCosine};

        const float yHalfAngle = (rotateY * cosineY) * 0.5f;
        const float ySine = ::sinf(yHalfAngle);
        const float yCosine = ::cosf(yHalfAngle);
        const idQuat yQuaternion{ySine * 0.0f, ySine,
                                 ySine * 0.0f, yCosine};

        const float zHalfAngle = (rotateZ * cosineZ) * 0.5f;
        const float zSine = ::sinf(zHalfAngle);
        const float zCosine = ::cosf(zHalfAngle);
        const idQuat zQuaternion{zSine * 0.0f, zSine * 0.0f,
                                 zSine, zCosine};

        idQuat result = Mat3ToQuat(jointAxis);
        result = MultiplyQuat(result, rotationOffset);
        result = MultiplyQuat(result, xQuaternion);
        result = MultiplyQuat(result, yQuaternion);
        result = MultiplyQuat(result, zQuaternion);
        const idMat3 resultAxis = QuatToMat3(result);

        idMat3x4 output;
        output.mat[0] = resultAxis.mat[0].x;
        output.mat[1] = resultAxis.mat[1].x;
        output.mat[2] = resultAxis.mat[2].x;
        output.mat[3] = origin.x;
        output.mat[4] = resultAxis.mat[0].y;
        output.mat[5] = resultAxis.mat[1].y;
        output.mat[6] = resultAxis.mat[2].y;
        output.mat[7] = origin.y;
        output.mat[8] = resultAxis.mat[0].z;
        output.mat[9] = resultAxis.mat[1].z;
        output.mat[10] = resultAxis.mat[2].z;
        output.mat[11] = origin.z;
        jointAnimator->SetJointMat3x4(DRIVER_MODEL, jointIndex,
                                      JOINTMOD_MODEL_OVERRIDE, output, true);
      }
    }

    ++jointNumber;
  } while (jointNumber < skeleton->data->numJoints);
}
