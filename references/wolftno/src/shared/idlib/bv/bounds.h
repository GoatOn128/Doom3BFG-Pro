#pragma once

#include "shared/idlib/geometry/rendermatrix.h"
#include "shared/idlib/math/plane.h"
#include "shared/idlib/math/vector.h"

class idSphere;

enum planeSide_t {
  PLANESIDE_FRONT = 0,
  PLANESIDE_BACK = 1,
  PLANESIDE_ON = 2,
  PLANESIDE_CROSS = 3,
};

class idBounds {
public:
  idVec3 b[2];

  idVec3& operator[](int index) { return b[index]; }
  const idVec3& operator[](int index) const { return b[index]; }
  bool operator==(const idBounds& other) const;
  bool operator!=(const idBounds& other) const;
  void Clear();
  bool IsZero() const {
    return b[0].x == 0.0f && b[0].y == 0.0f && b[0].z == 0.0f &&
           b[1].x == 0.0f && b[1].y == 0.0f && b[1].z == 0.0f;
  }
  bool AddPoint(const idVec3& point);
  void AddBounds(const idBounds& bounds);
  float GetRadius() const;
  idVec3 GetMaxExtents() const;
  float GetMaxDistFromOrigin() const;
  float GetInternalRadiusXY() const;
  float GetRadiusXY() const;
  float GetRadius(const idVec3& center) const;
  planeSide_t PlaneSide(const idPlane& plane, float epsilon = 0.0f) const;
  bool LineIntersection(const idVec3& start, const idVec3& end) const;
  bool RayIntersection(const idVec3& rayStart, const idVec3& rayDirection, float& scale1,
                       float& scale2) const;
  void FromTransformedBounds(const idBounds& bounds, const idVec3& origin, const idMat3& axis);
  void FromTransformedBounds(const idBounds& bounds, const idVec3& origin, const idMat3& axis,
                             const idVec3& scale);
  void FromModelMatrixBounds(const idBounds& bounds, const idRenderMatrix& modelMatrix);
  void FromBoundsTranslation(const idBounds& bounds, const idVec3& boundsOrigin,
                             const idMat3& boundsAxis, const idVec3& translation);
  void FromPointRotation180(const idVec3& point, const idRotation& rotation);
  void FromPointRotation(const idVec3& point, const idRotation& rotation);
  void FromBoundsRotation180(const idBounds& bounds, const idVec3& boundsOrigin,
                             const idMat3& boundsAxis, const idRotation& rotation);
  void FromBoundsRotation(const idBounds& bounds, const idVec3& boundsOrigin,
                          const idMat3& boundsAxis, const idRotation& rotation);
  bool SphereIntersection(const idSphere& sphere) const;
  std::intptr_t Compare(...) const;
};

static_assert(sizeof(idBounds) == 24, "idBounds ABI drift");

extern const idBounds bounds_zero;
extern const idBounds bounds_zeroOneCube;
extern const idBounds bounds_unitCube;
