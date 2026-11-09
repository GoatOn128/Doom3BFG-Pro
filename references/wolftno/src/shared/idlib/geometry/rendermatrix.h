#pragma once

#include "shared/idlib/math/vector.h"

class idBounds;
class idMat3;
class idPlane;

struct frustumCorners_t {
  float x[8];
  float y[8];
  float z[8];
};

struct idRenderMatrix {
  float m[16];

  static void CreateFromOriginAxis(const idVec3& origin, const idMat3& axis, idRenderMatrix& out);
  static void CreateFromOriginAxisScale(const idVec3& origin, const idMat3& axis,
                                        const idVec3& scale, idRenderMatrix& out);
  static void CreateViewMatrix(const idVec3& origin, const idMat3& axis, idRenderMatrix& out);
  static void CreateProjectionMatrix(float xMin, float xMax, float yMin, float yMax, float zNear,
                                     float zFar, idRenderMatrix& out);
  static void CreateProjectionMatrixFov(float xFovDegrees, float yFovDegrees, float zNear,
                                        float zFar, float xOffset, float yOffset,
                                        idRenderMatrix& out);
  static void OffsetScaleForBounds(const idRenderMatrix& source, const idBounds& bounds,
                                   idRenderMatrix& out);
  static void InverseOffsetScaleForBounds(const idRenderMatrix& source, const idBounds& bounds,
                                          idRenderMatrix& out);
  static void Multiply(const idRenderMatrix& left, const idRenderMatrix& right,
                       idRenderMatrix& out);
  static void TransformModelToClip(const idVec3& source,
                                   const idRenderMatrix& modelMatrix,
                                   const idRenderMatrix& projectionMatrix,
                                   idVec4& eye, idVec4& clip);
  static bool Inverse(const idRenderMatrix& source, idRenderMatrix& out);
  static void InverseByTranspose(const idRenderMatrix& source, idRenderMatrix& out);
  static void CopyMatrix(const idRenderMatrix& matrix, idVec4& row0, idVec4& row1, idVec4& row2,
                         idVec4& row3);
  static void SetMVP(const idRenderMatrix& mvp, idVec4& row0, idVec4& row1, idVec4& row2,
                     idVec4& row3, bool& negativeDeterminant);
  static void SetMVPForBounds(const idRenderMatrix& mvp, const idBounds& bounds, idVec4& row0,
                              idVec4& row1, idVec4& row2, idVec4& row3, bool& negativeDeterminant);
  static void SetMVPForInverseProject(const idRenderMatrix& mvp,
                                      const idRenderMatrix& inverseProject, idVec4& row0,
                                      idVec4& row1, idVec4& row2, idVec4& row3,
                                      bool& negativeDeterminant);
  static bool CullPointToMVPbits(const idRenderMatrix& mvp, const idVec3& point,
                                 unsigned char& outBits, bool zeroToOne);
  static bool CullBoundsToMVPbits(const idRenderMatrix& mvp, const idBounds& bounds,
                                  unsigned char& outBits, bool zeroToOne);
  static bool CullExtrudedBoundsToMVPbits(const idRenderMatrix& mvp, const idBounds& bounds,
                                          const idVec3& direction, const idPlane& clipPlane,
                                          unsigned char& outBits, bool zeroToOne);
  static void ProjectedBounds(idBounds& projected, const idRenderMatrix& mvp,
                              const idBounds& bounds, bool windowSpace);
  static void ProjectedCoords2D(idVec2& projected, const idVec3& position,
                                const idRenderMatrix& mvp, bool windowSpace);
  static void DepthBoundsForBounds(float& minimum, float& maximum, const idRenderMatrix& mvp,
                                   const idBounds& bounds, bool windowSpace);
  static void DepthBoundsForExtrudedBounds(float& minimum, float& maximum,
                                           const idRenderMatrix& mvp, const idBounds& bounds,
                                           const idVec3& direction, const idPlane& clipPlane,
                                           bool windowSpace);
  void InverseTransformPlane(const idPlane& input, idPlane& output,
                             bool normalize) const;
  static void GetFrustumCorners(frustumCorners_t& corners, const idRenderMatrix& transform,
                                const idBounds& bounds);
  static int CullFrustumCornersToPlane(const frustumCorners_t& corners, const idPlane& plane);
};

extern const idRenderMatrix renderMatrix_identity;

static_assert(sizeof(idRenderMatrix) == 64, "idRenderMatrix ABI drift");
