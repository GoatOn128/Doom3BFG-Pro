#pragma once

#include <algorithm>
#include <cmath>
#include <cstdint>
#include <limits>

class idAngles;
class idQuat;
class idMat3;
#if defined(WOLFTNO_RETAIL_IDROTATION_CLASS_ABI)
class idRotation;
#else
struct idRotation;
#endif
#if defined(WOLFTNO_RETAIL_IDVEC3_RADIANS_ABI)
enum RadiansUnique_t : int;
template <typename T, typename UniqueTag> class idTypesafeNumber;
#endif

class idVec2 {
public:
  float x;
  float y;

  float& operator[](int index) { return (&x)[index]; }
  const float& operator[](int index) const { return (&x)[index]; }
  idVec2 operator+(const idVec2& rhs) const { return {x + rhs.x, y + rhs.y}; }
  idVec2 operator-(const idVec2& rhs) const { return {x - rhs.x, y - rhs.y}; }
  idVec2 operator*(float scale) const { return {x * scale, y * scale}; }
  float operator*(const idVec2& rhs) const { return x * rhs.x + y * rhs.y; }
  float LengthSqr() const { return *this * *this; }
  float Length() const { return std::sqrt(LengthSqr()); }
  const char* ToString(int precision = 2) const;
};

struct idVec2i {
  int x;
  int y;
};

class idVec3 {
public:
  float x;
  float y;
  float z;

  float& operator[](int index) { return (&x)[index]; }
  const float& operator[](int index) const { return (&x)[index]; }
  idVec3 operator-() const { return {-x, -y, -z}; }
  idVec3 operator+(const idVec3& rhs) const { return {x + rhs.x, y + rhs.y, z + rhs.z}; }
  idVec3 operator-(const idVec3& rhs) const { return {x - rhs.x, y - rhs.y, z - rhs.z}; }
  idVec3 operator*(float scale) const { return {x * scale, y * scale, z * scale}; }
  idVec3 operator/(float scale) const { return *this * (1.0f / scale); }
  bool operator==(const idVec3& rhs) const {
    return x == rhs.x && y == rhs.y && z == rhs.z;
  }
  float operator*(const idVec3& rhs) const { return x * rhs.x + y * rhs.y + z * rhs.z; }
  idVec3& operator+=(const idVec3& rhs) {
    x += rhs.x;
    y += rhs.y;
    z += rhs.z;
    return *this;
  }
  idVec3& operator-=(const idVec3& rhs) {
    x -= rhs.x;
    y -= rhs.y;
    z -= rhs.z;
    return *this;
  }
  idVec3& operator*=(float scale) {
    x *= scale;
    y *= scale;
    z *= scale;
    return *this;
  }
  idVec3 Cross(const idVec3& rhs) const {
    return {y * rhs.z - z * rhs.y, z * rhs.x - x * rhs.z, x * rhs.y - y * rhs.x};
  }
  float LengthSqr() const { return *this * *this; }
  float Length() const { return std::sqrt(LengthSqr()); }
  float DistanceFast(const idVec3& other) const {
    const float deltaX = other.x - x;
    const float deltaY = other.y - y;
    const float deltaZ = other.z - z;
    const float squaredDistance =
        deltaX * deltaX + deltaY * deltaY + deltaZ * deltaZ;
    const float inverseDistance =
        1.0F / std::sqrt(std::max(squaredDistance,
                                  std::numeric_limits<float>::min()));
    return inverseDistance * squaredDistance;
  }
  float Normalize() {
    const float length = Length();
    if (length > 0.0f) {
      *this *= 1.0f / length;
    }
    return length;
  }
  float NormalizeFast();
  idVec3& Truncate(float length);
  void ProjectOntoPlane(const idVec3& normal, float overBounce);
  bool FixDenormals();
  bool FixDegenerateNormal();

  void NormalVectors(idVec3& left, idVec3& down) const;
  float ToYaw() const;
  float ToYaw(const idVec3& up) const;
  float ToPitch() const;
  idAngles ToAngles() const;
  idMat3 ToMat3() const;
  idMat3 ToZAxis() const;
  void ProjectSelfOntoSphere(float radius);
  const idVec3& RotateSelf(const idVec3& axis, float angleDegrees);
  const idVec3& RotateSelfRadians(const idVec3& axis, float angleRadians);
#if defined(WOLFTNO_RETAIL_IDVEC3_RADIANS_ABI)
  const idVec3&
  RotateSelf(const idVec3& axis,
             idTypesafeNumber<float, RadiansUnique_t> angleRadians);
#endif
  const char* ToString(int precision = 2) const;
};

inline idVec3 operator*(float scale, const idVec3& vector) {
  return vector * scale;
}

class idVec4 {
public:
  float x;
  float y;
  float z;
  float w;

  float& operator[](int index) { return (&x)[index]; }
  const float& operator[](int index) const { return (&x)[index]; }
};

class idVec6 {
public:
  float p[6];
};

class idAngles {
public:
  float pitch;
  float yaw;
  float roll;

  float& operator[](int index) { return (&pitch)[index]; }
  const float& operator[](int index) const { return (&pitch)[index]; }
  idAngles& Normalize360();
  idAngles& Normalize180();
  void Clamp(const idAngles& minimum, const idAngles& maximum);
  void ToVectors(idVec3* forward, idVec3* right = nullptr, idVec3* up = nullptr) const;
  idVec3 ToForward() const;
  idQuat ToQuat() const;
  idRotation ToRotation() const;
  idMat3 ToMat3() const;
  idVec3 ToAngularVelocity() const;
  const char* ToString(int precision = 2) const;
};

struct idCQuat {
  float x;
  float y;
  float z;
};

class idQuat {
public:
  float x;
  float y;
  float z;
  float w;

  float& operator[](int index) { return (&x)[index]; }
  const float& operator[](int index) const { return (&x)[index]; }
#if defined(WOLFTNO_RETAIL_PONYTAIL_QUATERNION_MULTIPLY_ABI)
  idQuat operator*(const idQuat& rhs) const;
  idQuat& operator*=(const idQuat& rhs);
#else
  float operator*(const idQuat& rhs) const { return x * rhs.x + y * rhs.y + z * rhs.z + w * rhs.w; }
#endif
  idQuat operator-() const { return {-x, -y, -z, -w}; }
  idQuat operator+(const idQuat& rhs) const { return {x + rhs.x, y + rhs.y, z + rhs.z, w + rhs.w}; }
  idQuat operator-(const idQuat& rhs) const { return {x - rhs.x, y - rhs.y, z - rhs.z, w - rhs.w}; }
  idQuat operator*(float scale) const { return {x * scale, y * scale, z * scale, w * scale}; }
  idVec3 operator*(const idVec3& vector) const;
  idQuat& Normalize();
  idRotation ToRotation() const;
  idMat3 ToMat3() const;
  idVec3 ToForward() const;
  idVec3 ToAngularVelocity() const;
  idQuat& Slerp(const idQuat& from, const idQuat& to, float t);
  idQuat& Lerp(const idQuat& from, const idQuat& to, float t);
  idAngles ToAngles() const;
};

class idMat3 {
public:
  idVec3 mat[3];

  idMat3() = default;
  explicit idMat3(const idVec3& axis, float angleDegrees);
  idMat3(const idVec3& axis, float sineHalfAngle, float cosineHalfAngle);
  idVec3& operator[](int index) { return mat[index]; }
  const idVec3& operator[](int index) const { return mat[index]; }
  idVec3 operator*(const idVec3& vector) const {
    return mat[0] * vector.x + mat[1] * vector.y + mat[2] * vector.z;
  }
  idMat3 operator*(const idMat3& rhs) const;
  idMat3& operator*=(const idMat3& rhs);
  bool Compare(const idMat3& other, float epsilon) const;
  bool IsDiagonal(float epsilon) const;
  bool IsOrthoNormal(float epsilon) const;
  float Determinant() const;
  bool InverseSelf();
  idMat3& OrthoNormalizeSelf(std::uint8_t prio1, std::uint8_t prio2);
  idMat3& OrthoNormalizeSelf();
  idMat3& InertiaTranslateSelf(float mass, const idVec3& centerOfMass, const idVec3& translation);
  const idMat3& RotateSelf(const idVec3& axis, float angleDegrees);
  float ToPitch() const;
  float ToYaw() const;
  idAngles ToAngles() const;
  idQuat ToQuat() const;
  idRotation ToRotation() const;
  idVec3 ToAngularVelocity() const;
  const char* ToString(int precision = 2) const;
};

struct idMat4 {
  idVec4 mat[4];
  idVec4& operator[](int index) { return mat[index]; }
  const idVec4& operator[](int index) const { return mat[index]; }
  bool InverseSelf();
};

#if defined(WOLFTNO_RETAIL_IDROTATION_CLASS_ABI)
class idRotation {
public:
#else
struct idRotation {
#endif
  idVec3 origin;
  idVec3 vec;
  float angle;
  mutable idMat3 axis;
  mutable bool axisValid;

  idRotation();
  idRotation(const idVec3& rotationOrigin, const idVec3& rotationAxis, float angleDegrees);
  const idMat3& ToMat3() const;
  void Normalize180();
  idAngles ToAngles() const;
  idVec3 ToAngularVelocity() const;
  idVec3 operator*(const idVec3& point) const;
  void RotatePoint(idVec3& point) const;
};

extern const idVec2 vec2_origin;
extern const idVec3 vec3_origin;
extern const idVec3 vec3_unit;
extern const idVec3 vec3_infinity;
extern const idVec3 vec3_down;
extern const idVec6 vec6_infinity;
extern idAngles ang_zero;
extern const idMat3 mat3_identity;
extern const idMat4 mat4_zero;
extern const idMat4 mat4_identity;
extern idQuat quat_identity;

static_assert(sizeof(idVec2) == 8, "idVec2 ABI drift");
static_assert(sizeof(idVec2i) == 8, "idVec2i ABI drift");
static_assert(sizeof(idVec3) == 12, "idVec3 ABI drift");
static_assert(sizeof(idVec4) == 16, "idVec4 ABI drift");
static_assert(sizeof(idVec6) == 24, "idVec6 ABI drift");
static_assert(sizeof(idAngles) == 12, "idAngles ABI drift");
static_assert(sizeof(idCQuat) == 12, "idCQuat ABI drift");
static_assert(sizeof(idQuat) == 16, "idQuat ABI drift");
static_assert(sizeof(idMat3) == 36, "idMat3 ABI drift");
static_assert(sizeof(idMat4) == 64, "idMat4 ABI drift");
static_assert(sizeof(idRotation) == 68, "idRotation ABI drift");
