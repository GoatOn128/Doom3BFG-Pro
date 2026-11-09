#include "rendermatrix.h"

#include "shared/idlib/bv/bounds.h"
#include "shared/idlib/math/plane.h"

#include <algorithm>
#include <cmath>
#include <cstring>
#include <limits>

const idRenderMatrix renderMatrix_identity{{
    1.0F, 0.0F, 0.0F, 0.0F,
    0.0F, 1.0F, 0.0F, 0.0F,
    0.0F, 0.0F, 1.0F, 0.0F,
    0.0F, 0.0F, 0.0F, 1.0F,
}};

namespace {
idVec4 Transform(const idRenderMatrix& matrix, const idVec3& point) {
  return {matrix.m[0] * point.x + matrix.m[1] * point.y + matrix.m[2] * point.z + matrix.m[3],
          matrix.m[4] * point.x + matrix.m[5] * point.y + matrix.m[6] * point.z + matrix.m[7],
          matrix.m[8] * point.x + matrix.m[9] * point.y + matrix.m[10] * point.z + matrix.m[11],
          matrix.m[12] * point.x + matrix.m[13] * point.y + matrix.m[14] * point.z + matrix.m[15]};
}
idVec3 Corner(const idBounds& bounds, int index) {
  return {bounds[(index >> 0) & 1].x, bounds[(index >> 1) & 1].y, bounds[(index >> 2) & 1].z};
}
float Determinant3(const idRenderMatrix& a) {
  return a.m[0] * (a.m[5] * a.m[10] - a.m[6] * a.m[9]) -
         a.m[1] * (a.m[4] * a.m[10] - a.m[6] * a.m[8]) +
         a.m[2] * (a.m[4] * a.m[9] - a.m[5] * a.m[8]);
}
unsigned char ClipBits(const idVec4& p, bool zeroToOne) {
  unsigned char bits = 0;
  const float lowerZ = zeroToOne ? 0.0f : -p.w;
  if (p.x < -p.w)
    bits |= 1;
  if (p.x > p.w)
    bits |= 2;
  if (p.y < -p.w)
    bits |= 4;
  if (p.y > p.w)
    bits |= 8;
  if (p.z < lowerZ)
    bits |= 16;
  if (p.z > p.w)
    bits |= 32;
  return bits;
}
idVec3 ExtrudeToPlane(const idVec3& point, const idVec3& direction, const idPlane& plane) {
  const float denominator = plane.a * direction.x + plane.b * direction.y + plane.c * direction.z;
  if (std::abs(denominator) < 1e-8f)
    return point;
  const float distance =
      -(plane.a * point.x + plane.b * point.y + plane.c * point.z + plane.d) / denominator;
  return {point.x + direction.x * distance, point.y + direction.y * distance,
          point.z + direction.z * distance};
}
} // namespace

void idRenderMatrix::CreateFromOriginAxis(const idVec3& origin, const idMat3& axis,
                                          idRenderMatrix& out) {
  out = {{axis.mat[0].x, axis.mat[1].x, axis.mat[2].x, origin.x, axis.mat[0].y, axis.mat[1].y,
          axis.mat[2].y, origin.y, axis.mat[0].z, axis.mat[1].z, axis.mat[2].z, origin.z, 0, 0, 0,
          1}};
}
void idRenderMatrix::CreateFromOriginAxisScale(const idVec3& origin, const idMat3& axis,
                                               const idVec3& scale, idRenderMatrix& out) {
  out = {{axis.mat[0].x * scale.x, axis.mat[1].x * scale.y, axis.mat[2].x * scale.z, origin.x,
          axis.mat[0].y * scale.x, axis.mat[1].y * scale.y, axis.mat[2].y * scale.z, origin.y,
          axis.mat[0].z * scale.x, axis.mat[1].z * scale.y, axis.mat[2].z * scale.z, origin.z, 0, 0,
          0, 1}};
}
void idRenderMatrix::CreateViewMatrix(const idVec3& origin, const idMat3& axis,
                                      idRenderMatrix& out) {
  out = {{-axis.mat[1].x, -axis.mat[1].y, -axis.mat[1].z,
          axis.mat[1].x * origin.x + axis.mat[1].y * origin.y + axis.mat[1].z * origin.z,
          axis.mat[2].x, axis.mat[2].y, axis.mat[2].z,
          -(axis.mat[2].x * origin.x + axis.mat[2].y * origin.y + axis.mat[2].z * origin.z),
          -axis.mat[0].x, -axis.mat[0].y, -axis.mat[0].z,
          axis.mat[0].x * origin.x + axis.mat[0].y * origin.y + axis.mat[0].z * origin.z, 0, 0, 0,
          1}};
}
void idRenderMatrix::CreateProjectionMatrix(float xMin, float xMax, float yMin, float yMax,
                                            float zNear, float zFar, idRenderMatrix& out) {
  std::fill(std::begin(out.m), std::end(out.m), 0.0f);
  out.m[0] = 2 * zNear / (xMax - xMin);
  out.m[2] = (xMin + xMax) / (xMax - xMin);
  out.m[5] = 2 * zNear / (yMax - yMin);
  out.m[6] = (yMin + yMax) / (yMax - yMin);
  if (zNear < zFar) {
    out.m[10] = -(zNear + zFar) / (zFar - zNear);
    out.m[11] = -2 * zFar * zNear / (zFar - zNear);
  } else {
    out.m[10] = -1;
    out.m[11] = -2 * zNear;
  }
  out.m[14] = -1;
}
void idRenderMatrix::CreateProjectionMatrixFov(float xFov, float yFov, float zNear, float zFar,
                                               float xOffset, float yOffset, idRenderMatrix& out) {
  constexpr float degToRad = 0.01745329251994329577f;
  const float x = std::tan(xFov * degToRad * 0.5f) * zNear;
  const float y = std::tan(yFov * degToRad * 0.5f) * zNear;
  CreateProjectionMatrix(-x + xOffset, x + xOffset, -y + yOffset, y + yOffset, zNear, zFar, out);
}
void idRenderMatrix::Multiply(const idRenderMatrix& a, const idRenderMatrix& b,
                              idRenderMatrix& out) {
  idRenderMatrix result{};
  for (int row = 0; row < 4; ++row)
    for (int col = 0; col < 4; ++col)
      for (int k = 0; k < 4; ++k)
        result.m[row * 4 + col] += a.m[row * 4 + k] * b.m[k * 4 + col];
  out = result;
}
void idRenderMatrix::TransformModelToClip(
    const idVec3& source, const idRenderMatrix& modelMatrix,
    const idRenderMatrix& projectionMatrix, idVec4& eye, idVec4& clip) {
  eye = Transform(modelMatrix, source);
  clip = {
      projectionMatrix.m[0] * eye.x + projectionMatrix.m[1] * eye.y +
          projectionMatrix.m[2] * eye.z + projectionMatrix.m[3] * eye.w,
      projectionMatrix.m[4] * eye.x + projectionMatrix.m[5] * eye.y +
          projectionMatrix.m[6] * eye.z + projectionMatrix.m[7] * eye.w,
      projectionMatrix.m[8] * eye.x + projectionMatrix.m[9] * eye.y +
          projectionMatrix.m[10] * eye.z + projectionMatrix.m[11] * eye.w,
      projectionMatrix.m[12] * eye.x + projectionMatrix.m[13] * eye.y +
          projectionMatrix.m[14] * eye.z + projectionMatrix.m[15] * eye.w};
}
void idRenderMatrix::OffsetScaleForBounds(const idRenderMatrix& source, const idBounds& bounds,
                                          idRenderMatrix& out) {
  const idVec3 center = {(bounds[0].x + bounds[1].x) * 0.5f, (bounds[0].y + bounds[1].y) * 0.5f,
                         (bounds[0].z + bounds[1].z) * 0.5f};
  const idVec3 extent = {(bounds[1].x - bounds[0].x) * 0.5f, (bounds[1].y - bounds[0].y) * 0.5f,
                         (bounds[1].z - bounds[0].z) * 0.5f};
  idRenderMatrix transform = {
      {extent.x, 0, 0, center.x, 0, extent.y, 0, center.y, 0, 0, extent.z, center.z, 0, 0, 0, 1}};
  Multiply(source, transform, out);
}
void idRenderMatrix::InverseOffsetScaleForBounds(const idRenderMatrix& source,
                                                 const idBounds& bounds, idRenderMatrix& out) {
  const idVec3 center = {(bounds[0].x + bounds[1].x) * 0.5f, (bounds[0].y + bounds[1].y) * 0.5f,
                         (bounds[0].z + bounds[1].z) * 0.5f};
  const idVec3 extent = {(bounds[1].x - bounds[0].x) * 0.5f, (bounds[1].y - bounds[0].y) * 0.5f,
                         (bounds[1].z - bounds[0].z) * 0.5f};
  idRenderMatrix transform = {{1 / extent.x, 0, 0, -center.x / extent.x, 0, 1 / extent.y, 0,
                               -center.y / extent.y, 0, 0, 1 / extent.z, -center.z / extent.z, 0, 0,
                               0, 1}};
  Multiply(transform, source, out);
}
bool idRenderMatrix::Inverse(const idRenderMatrix& source, idRenderMatrix& out) {
  double augmented[4][8]{};
  for (int r = 0; r < 4; ++r)
    for (int c = 0; c < 4; ++c) {
      augmented[r][c] = source.m[r * 4 + c];
      augmented[r][c + 4] = r == c ? 1.0 : 0.0;
    }
  for (int col = 0; col < 4; ++col) {
    int pivot = col;
    for (int r = col + 1; r < 4; ++r)
      if (std::abs(augmented[r][col]) > std::abs(augmented[pivot][col]))
        pivot = r;
    if (std::abs(augmented[pivot][col]) < 1e-12)
      return false;
    if (pivot != col)
      for (int c = 0; c < 8; ++c)
        std::swap(augmented[pivot][c], augmented[col][c]);
    const double divisor = augmented[col][col];
    for (double& value : augmented[col])
      value /= divisor;
    for (int r = 0; r < 4; ++r)
      if (r != col) {
        const double factor = augmented[r][col];
        for (int c = 0; c < 8; ++c)
          augmented[r][c] -= factor * augmented[col][c];
      }
  }
  for (int r = 0; r < 4; ++r)
    for (int c = 0; c < 4; ++c)
      out.m[r * 4 + c] = static_cast<float>(augmented[r][c + 4]);
  return true;
}
void idRenderMatrix::InverseByTranspose(const idRenderMatrix& source, idRenderMatrix& out) {
  out = {{source.m[0], source.m[4], source.m[8], 0, source.m[1], source.m[5], source.m[9], 0,
          source.m[2], source.m[6], source.m[10], 0, 0, 0, 0, 1}};
  out.m[3] = -(out.m[0] * source.m[3] + out.m[1] * source.m[7] + out.m[2] * source.m[11]);
  out.m[7] = -(out.m[4] * source.m[3] + out.m[5] * source.m[7] + out.m[6] * source.m[11]);
  out.m[11] = -(out.m[8] * source.m[3] + out.m[9] * source.m[7] + out.m[10] * source.m[11]);
}
void idRenderMatrix::CopyMatrix(const idRenderMatrix& a, idVec4& r0, idVec4& r1, idVec4& r2,
                                idVec4& r3) {
  std::memcpy(&r0, &a.m[0], 16);
  std::memcpy(&r1, &a.m[4], 16);
  std::memcpy(&r2, &a.m[8], 16);
  std::memcpy(&r3, &a.m[12], 16);
}
void idRenderMatrix::SetMVP(const idRenderMatrix& a, idVec4& r0, idVec4& r1, idVec4& r2, idVec4& r3,
                            bool& negative) {
  CopyMatrix(a, r0, r1, r2, r3);
  negative = Determinant3(a) < 0;
}
void idRenderMatrix::SetMVPForBounds(const idRenderMatrix& a, const idBounds& b, idVec4& r0,
                                     idVec4& r1, idVec4& r2, idVec4& r3, bool& negative) {
  idRenderMatrix adjusted;
  OffsetScaleForBounds(a, b, adjusted);
  SetMVP(adjusted, r0, r1, r2, r3, negative);
}
void idRenderMatrix::SetMVPForInverseProject(const idRenderMatrix& a,
                                             const idRenderMatrix& inverseProject, idVec4& r0,
                                             idVec4& r1, idVec4& r2, idVec4& r3, bool& negative) {
  idRenderMatrix adjusted;
  Multiply(a, inverseProject, adjusted);
  SetMVP(adjusted, r0, r1, r2, r3, negative);
}
bool idRenderMatrix::CullPointToMVPbits(const idRenderMatrix& mvp, const idVec3& point,
                                        unsigned char& outBits, bool zeroToOne) {
  outBits = ClipBits(Transform(mvp, point), zeroToOne);
  return outBits != 0;
}
bool idRenderMatrix::CullBoundsToMVPbits(const idRenderMatrix& mvp, const idBounds& bounds,
                                         unsigned char& outBits, bool zeroToOne) {
  unsigned char common = 63;
  for (int i = 0; i < 8; ++i)
    common &= ClipBits(Transform(mvp, Corner(bounds, i)), zeroToOne);
  outBits = common;
  return common != 0;
}
bool idRenderMatrix::CullExtrudedBoundsToMVPbits(const idRenderMatrix& mvp, const idBounds& bounds,
                                                 const idVec3& direction, const idPlane& plane,
                                                 unsigned char& outBits, bool zeroToOne) {
  unsigned char common = 63;
  for (int i = 0; i < 8; ++i) {
    const idVec3 p = Corner(bounds, i);
    common &= ClipBits(Transform(mvp, p), zeroToOne);
    common &= ClipBits(Transform(mvp, ExtrudeToPlane(p, direction, plane)), zeroToOne);
  }
  outBits = common;
  return common != 0;
}
void idRenderMatrix::ProjectedCoords2D(idVec2& out, const idVec3& position,
                                       const idRenderMatrix& mvp, bool windowSpace) {
  const idVec4 p = Transform(mvp, position);
  const float inv = std::abs(p.w) > 1e-20f ? 1.0f / p.w : 0.0f;
  out = {p.x * inv, p.y * inv};
  if (windowSpace) {
    out.x = out.x * 0.5f + 0.5f;
    out.y = out.y * 0.5f + 0.5f;
  }
}
void idRenderMatrix::ProjectedBounds(idBounds& projected, const idRenderMatrix& mvp,
                                     const idBounds& bounds, bool windowSpace) {
  projected.Clear();
  for (int i = 0; i < 8; ++i) {
    const idVec4 p = Transform(mvp, Corner(bounds, i));
    const float inv = std::abs(p.w) > 1e-20f ? 1.0f / p.w : 0.0f;
    idVec3 q = {p.x * inv, p.y * inv, p.z * inv};
    if (windowSpace) {
      q.x = q.x * 0.5f + 0.5f;
      q.y = q.y * 0.5f + 0.5f;
      q.z = q.z * 0.5f + 0.5f;
    }
    projected.AddPoint(q);
  }
}
void idRenderMatrix::DepthBoundsForBounds(float& minimum, float& maximum, const idRenderMatrix& mvp,
                                          const idBounds& bounds, bool windowSpace) {
  minimum = std::numeric_limits<float>::infinity();
  maximum = -minimum;
  for (int i = 0; i < 8; ++i) {
    const idVec4 p = Transform(mvp, Corner(bounds, i));
    float z = std::abs(p.w) > 1e-20f ? p.z / p.w : 0;
    if (windowSpace)
      z = z * 0.5f + 0.5f;
    minimum = std::min(minimum, z);
    maximum = std::max(maximum, z);
  }
}
void idRenderMatrix::DepthBoundsForExtrudedBounds(float& minimum, float& maximum,
                                                  const idRenderMatrix& mvp, const idBounds& bounds,
                                                  const idVec3& direction, const idPlane& plane,
                                                  bool windowSpace) {
  minimum = std::numeric_limits<float>::infinity();
  maximum = -minimum;
  for (int i = 0; i < 8; ++i)
    for (int j = 0; j < 2; ++j) {
      const idVec3 p = j ? ExtrudeToPlane(Corner(bounds, i), direction, plane) : Corner(bounds, i);
      const idVec4 q = Transform(mvp, p);
      float z = std::abs(q.w) > 1e-20f ? q.z / q.w : 0;
      if (windowSpace)
        z = z * 0.5f + 0.5f;
      minimum = std::min(minimum, z);
      maximum = std::max(maximum, z);
    }
}
void idRenderMatrix::GetFrustumCorners(frustumCorners_t& corners, const idRenderMatrix& transform,
                                       const idBounds& bounds) {
  for (int i = 0; i < 8; ++i) {
    const idVec4 p = Transform(transform, Corner(bounds, i));
    const float inv = std::abs(p.w) > 1e-20f ? 1 / p.w : 0;
    corners.x[i] = p.x * inv;
    corners.y[i] = p.y * inv;
    corners.z[i] = p.z * inv;
  }
}
int idRenderMatrix::CullFrustumCornersToPlane(const frustumCorners_t& corners,
                                              const idPlane& plane) {
  int negative = 0;
  for (int i = 0; i < 8; ++i) {
    negative += plane.a * corners.x[i] + plane.b * corners.y[i] +
                        plane.c * corners.z[i] + plane.d <
                    0.0f
                ? 1
                : 0;
  }
  if (negative == 0)
    return 1; // FRUSTUM_CULL_FRONT
  if (negative == 8)
    return 2; // FRUSTUM_CULL_BACK
  return 3;   // FRUSTUM_CULL_CROSS
}
