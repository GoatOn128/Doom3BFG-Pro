#include "vector.h"

#include "mathlib.h"
#include "shared/idlib/text/str.h"

#include <algorithm>
#include <cmath>

const idMat3 mat3_identity = [] {
  idMat3 result;
  result.mat[0] = {1.0f, 0.0f, 0.0f};
  result.mat[1] = {0.0f, 1.0f, 0.0f};
  result.mat[2] = {0.0f, 0.0f, 1.0f};
  return result;
}();
const idMat4 mat4_zero{{{0.0f, 0.0f, 0.0f, 0.0f},
                        {0.0f, 0.0f, 0.0f, 0.0f},
                        {0.0f, 0.0f, 0.0f, 0.0f},
                        {0.0f, 0.0f, 0.0f, 0.0f}}};
const idMat4 mat4_identity{{{1.0f, 0.0f, 0.0f, 0.0f},
                            {0.0f, 1.0f, 0.0f, 0.0f},
                            {0.0f, 0.0f, 1.0f, 0.0f},
                            {0.0f, 0.0f, 0.0f, 1.0f}}};

idMat3::idMat3(const idVec3& rotationAxis, float angleRadians) {
  const float halfAngle = angleRadians * 0.5f;
  *this = idMat3(rotationAxis, std::sin(halfAngle), std::cos(halfAngle));
}

idMat3::idMat3(const idVec3& rotationAxis, float sineHalfAngle, float cosineHalfAngle) {
  *this = idQuat{rotationAxis.x * sineHalfAngle, rotationAxis.y * sineHalfAngle,
                 rotationAxis.z * sineHalfAngle, cosineHalfAngle}
              .ToMat3();
}

idMat3 idMat3::operator*(const idMat3& rhs) const {
  idMat3 result{};
  for (int row = 0; row < 3; ++row) {
    for (int column = 0; column < 3; ++column) {
      result.mat[row][column] = mat[row].x * rhs.mat[0][column] + mat[row].y * rhs.mat[1][column] +
                                mat[row].z * rhs.mat[2][column];
    }
  }
  return result;
}

float idMat3::Determinant() const {
  return mat[0].x * (mat[1].y * mat[2].z - mat[1].z * mat[2].y) -
         mat[0].y * (mat[1].x * mat[2].z - mat[1].z * mat[2].x) +
         mat[0].z * (mat[1].x * mat[2].y - mat[1].y * mat[2].x);
}

bool idMat3::InverseSelf() {
  const float determinant = Determinant();
  if (std::fabs(determinant) < 1.0e-14f)
    return false;
  const float inverseDeterminant = 1.0f / determinant;
  idMat3 inverse;
  inverse.mat[0] = {(mat[1].y * mat[2].z - mat[1].z * mat[2].y) * inverseDeterminant,
                    (mat[0].z * mat[2].y - mat[0].y * mat[2].z) * inverseDeterminant,
                    (mat[0].y * mat[1].z - mat[0].z * mat[1].y) * inverseDeterminant};
  inverse.mat[1] = {(mat[1].z * mat[2].x - mat[1].x * mat[2].z) * inverseDeterminant,
                    (mat[0].x * mat[2].z - mat[0].z * mat[2].x) * inverseDeterminant,
                    (mat[0].z * mat[1].x - mat[0].x * mat[1].z) * inverseDeterminant};
  inverse.mat[2] = {(mat[1].x * mat[2].y - mat[1].y * mat[2].x) * inverseDeterminant,
                    (mat[0].y * mat[2].x - mat[0].x * mat[2].y) * inverseDeterminant,
                    (mat[0].x * mat[1].y - mat[0].y * mat[1].x) * inverseDeterminant};
  *this = inverse;
  return true;
}

idMat3& idMat3::InertiaTranslateSelf(float mass, const idVec3& centerOfMass,
                                     const idVec3& translation) {
  const idVec3 translated = centerOfMass + translation;
  mat[0].x += mass * ((centerOfMass.y * centerOfMass.y + centerOfMass.z * centerOfMass.z) -
                      (translated.y * translated.y + translated.z * translated.z));
  mat[1].y += mass * ((centerOfMass.x * centerOfMass.x + centerOfMass.z * centerOfMass.z) -
                      (translated.x * translated.x + translated.z * translated.z));
  mat[2].z += mass * ((centerOfMass.x * centerOfMass.x + centerOfMass.y * centerOfMass.y) -
                      (translated.x * translated.x + translated.y * translated.y));
  const float xy = mass * (translated.x * translated.y - centerOfMass.x * centerOfMass.y);
  const float xz = mass * (translated.x * translated.z - centerOfMass.x * centerOfMass.z);
  const float yz = mass * (translated.y * translated.z - centerOfMass.y * centerOfMass.z);
  mat[0].y += xy;
  mat[1].x += xy;
  mat[0].z += xz;
  mat[2].x += xz;
  mat[1].z += yz;
  mat[2].y += yz;
  return *this;
}

const idMat3& idMat3::RotateSelf(const idVec3& rotationAxis, float angleRadians) {
  *this = *this * idRotation(vec3_origin, rotationAxis, -angleRadians * idMath::M_RAD2DEG).ToMat3();
  return *this;
}

float idMat3::ToPitch() const {
  return mat[0].ToPitch();
}
float idMat3::ToYaw() const {
  return mat[0].ToYaw();
}

idAngles idMat3::ToAngles() const {
  const float sinePitch = std::clamp(mat[0].z, -1.0f, 1.0f);
  idAngles result{};
  const float theta = -std::asin(sinePitch);
  const float cosinePitch = std::cos(theta);
  if (cosinePitch > 1.0e-6f) {
    result.pitch = theta * idMath::M_RAD2DEG;
    result.yaw = std::atan2(mat[0].y, mat[0].x) * idMath::M_RAD2DEG;
    result.roll = std::atan2(mat[1].z, mat[2].z) * idMath::M_RAD2DEG;
  } else {
    result.pitch = sinePitch > 0.0f ? -90.0f : 90.0f;
    result.yaw = std::atan2(-mat[1].x, mat[1].y) * idMath::M_RAD2DEG;
    result.roll = 0.0f;
  }
  return result;
}

idQuat idMat3::ToQuat() const {
  idQuat result{};
  const float trace = mat[0].x + mat[1].y + mat[2].z;
  if (trace > 0.0f) {
    const float scale = std::sqrt(trace + 1.0f);
    result.w = scale * 0.5f;
    const float inverse = 0.5f / scale;
    result.x = (mat[2].y - mat[1].z) * inverse;
    result.y = (mat[0].z - mat[2].x) * inverse;
    result.z = (mat[1].x - mat[0].y) * inverse;
  } else {
    int i = 0;
    if (mat[1].y > mat[0].x)
      i = 1;
    if (mat[2].z > mat[i][i])
      i = 2;
    const int j = (i + 1) % 3;
    const int k = (j + 1) % 3;
    const float scale = std::sqrt(std::max(0.0f, mat[i][i] - mat[j][j] - mat[k][k] + 1.0f));
    result[i] = scale * 0.5f;
    const float inverse = scale > 0.0f ? 0.5f / scale : 0.0f;
    result.w = (mat[k][j] - mat[j][k]) * inverse;
    result[j] = (mat[j][i] + mat[i][j]) * inverse;
    result[k] = (mat[k][i] + mat[i][k]) * inverse;
  }
  return result;
}

idRotation idMat3::ToRotation() const {
  return ToQuat().ToRotation();
}
idVec3 idMat3::ToAngularVelocity() const {
  return ToRotation().ToAngularVelocity();
}

const char* idMat3::ToString(int precision) const {
  return idStr::FloatArrayToString(&mat[0].x, 9, precision);
}

bool idMat4::InverseSelf() {
  float augmented[4][8]{};
  for (int row = 0; row < 4; ++row) {
    for (int column = 0; column < 4; ++column)
      augmented[row][column] = mat[row][column];
    augmented[row][row + 4] = 1.0f;
  }
  for (int column = 0; column < 4; ++column) {
    int pivot = column;
    for (int row = column + 1; row < 4; ++row) {
      if (std::fabs(augmented[row][column]) > std::fabs(augmented[pivot][column]))
        pivot = row;
    }
    if (std::fabs(augmented[pivot][column]) < 1.0e-14f)
      return false;
    if (pivot != column) {
      for (int index = 0; index < 8; ++index)
        std::swap(augmented[pivot][index], augmented[column][index]);
    }
    const float inversePivot = 1.0f / augmented[column][column];
    for (float& value : augmented[column])
      value *= inversePivot;
    for (int row = 0; row < 4; ++row) {
      if (row == column)
        continue;
      const float factor = augmented[row][column];
      for (int index = 0; index < 8; ++index)
        augmented[row][index] -= factor * augmented[column][index];
    }
  }
  for (int row = 0; row < 4; ++row) {
    for (int column = 0; column < 4; ++column)
      mat[row][column] = augmented[row][column + 4];
  }
  return true;
}
