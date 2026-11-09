#include "box.h"

#include "shared/idlib/math/mathlib.h"
#include "shared/idlib/math/vecx.h"

#include <algorithm>
#include <cmath>

namespace {
float ProjectionRadius(const idBox& box, const idVec3& direction) {
  return std::fabs(direction * box.axis.mat[0]) * box.extents.x +
         std::fabs(direction * box.axis.mat[1]) * box.extents.y +
         std::fabs(direction * box.axis.mat[2]) * box.extents.z;
}

bool Separates(const idBox& first, const idBox& second, const idVec3& direction) {
  const float lengthSqr = direction.LengthSqr();
  if (lengthSqr <= 1.0e-12f)
    return false;
  const float centerDistance = std::fabs(direction * (second.center - first.center));
  return centerDistance > ProjectionRadius(first, direction) + ProjectionRadius(second, direction);
}

void SolveSymmetricEigenvectors(const float covariance[3][3], idMat3& axes) {
  float matrix[3][3];
  float vectors[3][3]{{1.0f, 0.0f, 0.0f}, {0.0f, 1.0f, 0.0f}, {0.0f, 0.0f, 1.0f}};
  for (int row = 0; row < 3; ++row)
    for (int column = 0; column < 3; ++column)
      matrix[row][column] = covariance[row][column];

  for (int iteration = 0; iteration < 24; ++iteration) {
    int p = 0, q = 1;
    float largest = std::fabs(matrix[p][q]);
    for (int row = 0; row < 3; ++row) {
      for (int column = row + 1; column < 3; ++column) {
        if (std::fabs(matrix[row][column]) > largest) {
          largest = std::fabs(matrix[row][column]);
          p = row;
          q = column;
        }
      }
    }
    if (largest < 1.0e-8f)
      break;
    const float angle = 0.5f * std::atan2(2.0f * matrix[p][q], matrix[q][q] - matrix[p][p]);
    const float sine = std::sin(angle);
    const float cosine = std::cos(angle);
    for (int index = 0; index < 3; ++index) {
      const float mip = matrix[index][p];
      const float miq = matrix[index][q];
      matrix[index][p] = cosine * mip - sine * miq;
      matrix[index][q] = sine * mip + cosine * miq;
    }
    for (int index = 0; index < 3; ++index) {
      const float mpi = matrix[p][index];
      const float mqi = matrix[q][index];
      matrix[p][index] = cosine * mpi - sine * mqi;
      matrix[q][index] = sine * mpi + cosine * mqi;
      const float vip = vectors[index][p];
      const float viq = vectors[index][q];
      vectors[index][p] = cosine * vip - sine * viq;
      vectors[index][q] = sine * vip + cosine * viq;
    }
  }
  axes.mat[0] = {vectors[0][0], vectors[1][0], vectors[2][0]};
  axes.mat[1] = {vectors[0][1], vectors[1][1], vectors[2][1]};
  axes.mat[0].Normalize();
  axes.mat[1] -= axes.mat[0] * (axes.mat[0] * axes.mat[1]);
  if (axes.mat[1].Normalize() == 0.0f)
    axes.mat[1] = axes.mat[0].ToMat3().mat[1];
  axes.mat[2] = axes.mat[0].Cross(axes.mat[1]);
  axes.mat[2].Normalize();
}
} // namespace

void idBox::AxisProjection(const idMat3& projectionAxis, idBounds& bounds) const {
  for (int component = 0; component < 3; ++component) {
    const idVec3& direction = projectionAxis.mat[component];
    const float projectedCenter = center * direction;
    const float radius = ProjectionRadius(*this, direction);
    bounds.b[0][component] = projectedCenter - radius;
    bounds.b[1][component] = projectedCenter + radius;
  }
}

bool idBox::AddPoint(const idVec3& point) {
  if (extents.x < 0.0f) {
    center = point;
    extents = {0.0f, 0.0f, 0.0f};
    axis = mat3_identity;
    return true;
  }
  const idVec3 oldCenterProjection{center * axis.mat[0], center * axis.mat[1],
                                   center * axis.mat[2]};
  const idVec3 pointProjection{point * axis.mat[0], point * axis.mat[1], point * axis.mat[2]};
  idVec3 minimum = oldCenterProjection - extents;
  idVec3 maximum = oldCenterProjection + extents;
  bool expanded = false;
  for (int component = 0; component < 3; ++component) {
    if (pointProjection[component] < minimum[component]) {
      minimum[component] = pointProjection[component];
      expanded = true;
    }
    if (pointProjection[component] > maximum[component]) {
      maximum[component] = pointProjection[component];
      expanded = true;
    }
  }
  if (!expanded)
    return false;
  const idVec3 localCenter = (minimum + maximum) * 0.5f;
  extents = maximum - localCenter;
  center = axis.mat[0] * localCenter.x + axis.mat[1] * localCenter.y + axis.mat[2] * localCenter.z;
  return true;
}

bool idBox::IntersectsBox(const idBox& other) const {
  for (int index = 0; index < 3; ++index) {
    if (Separates(*this, other, axis.mat[index]) || Separates(*this, other, other.axis.mat[index]))
      return false;
  }
  for (int firstAxis = 0; firstAxis < 3; ++firstAxis) {
    for (int secondAxis = 0; secondAxis < 3; ++secondAxis) {
      if (Separates(*this, other, axis.mat[firstAxis].Cross(other.axis.mat[secondAxis])))
        return false;
    }
  }
  return true;
}

bool idBox::LineIntersection(const idVec3& start, const idVec3& end) const {
  const idVec3 startDelta = start - center;
  const idVec3 endDelta = end - center;
  const idVec3 localStart{startDelta * axis.mat[0], startDelta * axis.mat[1],
                          startDelta * axis.mat[2]};
  const idVec3 localEnd{endDelta * axis.mat[0], endDelta * axis.mat[1], endDelta * axis.mat[2]};
  const idBounds localBounds{{-extents, extents}};
  return localBounds.LineIntersection(localStart, localEnd);
}

void idBox::ToPoints(idVec3 points[8]) const {
  const idVec3 x = axis.mat[0] * extents.x;
  const idVec3 y = axis.mat[1] * extents.y;
  const idVec3 z = axis.mat[2] * extents.z;
  points[0] = center - x - y - z;
  points[1] = center + x - y - z;
  points[2] = center + x + y - z;
  points[3] = center - x + y - z;
  points[4] = center - x - y + z;
  points[5] = center + x - y + z;
  points[6] = center + x + y + z;
  points[7] = center - x + y + z;
}

void idMatX::SetData(int rows, int columns, float* data) {
  numRows = rows;
  numColumns = columns;
  alloced = -1;
  mat = data;
}

void idBox::FromPoints(const idVec3* points, int numPoints) {
  if (points == nullptr || numPoints <= 0) {
    center = vec3_origin;
    extents = vec3_origin;
    axis = mat3_identity;
    return;
  }
  idVec3 mean{};
  for (int index = 0; index < numPoints; ++index)
    mean += points[index];
  mean *= 1.0f / static_cast<float>(numPoints);
  float covariance[3][3]{};
  for (int index = 0; index < numPoints; ++index) {
    const idVec3 delta = points[index] - mean;
    for (int row = 0; row < 3; ++row)
      for (int column = 0; column < 3; ++column)
        covariance[row][column] += delta[row] * delta[column];
  }
  SolveSymmetricEigenvectors(covariance, axis);
  idVec3 minimum{1.0e30f, 1.0e30f, 1.0e30f};
  idVec3 maximum{-1.0e30f, -1.0e30f, -1.0e30f};
  for (int index = 0; index < numPoints; ++index) {
    for (int component = 0; component < 3; ++component) {
      const float projection = points[index] * axis.mat[component];
      minimum[component] = std::min(minimum[component], projection);
      maximum[component] = std::max(maximum[component], projection);
    }
  }
  const idVec3 localCenter = (minimum + maximum) * 0.5f;
  extents = maximum - localCenter;
  center = axis.mat[0] * localCenter.x + axis.mat[1] * localCenter.y + axis.mat[2] * localCenter.z;
}
