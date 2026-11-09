#include "winding.h"

#include "shared/idlib/math/mathlib.h"

#include <algorithm>
#include <cmath>
#include <cstring>
#include <vector>

namespace {
float PlaneDistance(const idPlane& plane, const idVec5& point) {
  return plane.a * point.x + plane.b * point.y + plane.c * point.z + plane.d;
}

idVec5 Interpolate(const idVec5& first, const idVec5& second, float fraction) {
  idVec5 result{};
  for (int component = 0; component < 5; ++component) {
    (&result.x)[component] =
        (&first.x)[component] + ((&second.x)[component] - (&first.x)[component]) * fraction;
  }
  return result;
}
} // namespace

idWinding::~idWinding() {
  delete[] p;
}

void idWinding::Clear() {
  delete[] p;
  p = nullptr;
  numPoints = 0;
  allocedSize = 0;
}

bool idWinding::ReAllocate(int points, bool keep) {
  if (points <= allocedSize)
    return true;
  if (points < 0)
    return false;
  idVec5* replacement = new idVec5[static_cast<std::size_t>(points)];
  if (keep && p != nullptr) {
    std::copy_n(p, std::min(numPoints, points), replacement);
  } else {
    numPoints = 0;
  }
  delete[] p;
  p = replacement;
  allocedSize = points;
  if (numPoints > points)
    numPoints = points;
  return true;
}

bool idWinding::AddPoint(const idVec5& point) {
  if (numPoints >= allocedSize && !ReAllocate(std::max(4, allocedSize * 2), true))
    return false;
  p[numPoints++] = point;
  return true;
}

void idWinding::ReverseSelf() {
  for (int first = 0, last = numPoints - 1; first < last; ++first, --last) {
    std::swap(p[first], p[last]);
  }
}

void idFixedWinding::Clear() {
  numPoints = 0;
}

idFixedWinding::idFixedWinding() {
  p = data;
  allocedSize = 64;
}

idFixedWinding::idFixedWinding(const idFixedWinding& other) : idFixedWinding() {
  numPoints = other.numPoints;
  std::copy_n(other.p, numPoints, data);
}

idFixedWinding& idFixedWinding::operator=(const idFixedWinding& other) {
  if (this != &other) {
    numPoints = other.numPoints;
    std::copy_n(other.p, numPoints, data);
  }
  return *this;
}

idFixedWinding::~idFixedWinding() {
  p = nullptr;
  allocedSize = 0;
}

bool idFixedWinding::ReAllocate(int points, bool) {
  return points <= 64;
}

void idWinding::BaseForPlane(idVec3& normal, float distance, float radius) {
  normal.Normalize();
  const idVec3 origin = normal * distance;
  idVec3 up;
  if (std::fabs(normal.z) > 0.999f)
    up = {1.0f, 0.0f, 0.0f};
  else
    up = {0.0f, 0.0f, 1.0f};
  idVec3 right = up.Cross(normal);
  right.Normalize();
  up = normal.Cross(right);
  right *= radius;
  up *= radius;
  ReAllocate(4, false);
  numPoints = 4;
  const idVec3 corners[4]{origin - right + up, origin + right + up, origin + right - up,
                          origin - right - up};
  for (int index = 0; index < 4; ++index) {
    p[index] = {corners[index].x, corners[index].y, corners[index].z, 0.0f, 0.0f};
  }
}

void CalculateSides(const idVec5* points, unsigned numPoints, const idPlane& plane, float epsilon,
                    float* distances, unsigned char* sides, int counts[3]) {
  counts[0] = counts[1] = counts[2] = 0;
  for (unsigned index = 0; index < numPoints; ++index) {
    distances[index] = PlaneDistance(plane, points[index]);
    if (distances[index] > epsilon)
      sides[index] = PLANESIDE_FRONT;
    else if (distances[index] < -epsilon)
      sides[index] = PLANESIDE_BACK;
    else
      sides[index] = PLANESIDE_ON;
    ++counts[sides[index]];
  }
}

bool idWinding::ClipInPlace(const idPlane& plane, float epsilon, bool keepOn) {
  if (numPoints == 0)
    return false;
  std::vector<float> distances(static_cast<std::size_t>(numPoints));
  std::vector<unsigned char> sides(static_cast<std::size_t>(numPoints));
  int counts[3];
  CalculateSides(p, static_cast<unsigned>(numPoints), plane, epsilon, distances.data(),
                 sides.data(), counts);
  if (counts[PLANESIDE_FRONT] == 0) {
    if (counts[PLANESIDE_BACK] == 0 && keepOn)
      return true;
    numPoints = 0;
    return false;
  }
  if (counts[PLANESIDE_BACK] == 0)
    return true;

  std::vector<idVec5> clipped;
  clipped.reserve(static_cast<std::size_t>(numPoints + 4));
  for (int index = 0; index < numPoints; ++index) {
    const int next = (index + 1) % numPoints;
    if (sides[index] != PLANESIDE_BACK)
      clipped.push_back(p[index]);
    if ((sides[index] == PLANESIDE_FRONT && sides[next] == PLANESIDE_BACK) ||
        (sides[index] == PLANESIDE_BACK && sides[next] == PLANESIDE_FRONT)) {
      const float fraction = distances[index] / (distances[index] - distances[next]);
      clipped.push_back(Interpolate(p[index], p[next], fraction));
    }
  }
  if (!ReAllocate(static_cast<int>(clipped.size()), false))
    return false;
  numPoints = static_cast<int>(clipped.size());
  std::copy(clipped.begin(), clipped.end(), p);
  return numPoints > 0;
}

idVec3 idWinding::GetCenter() const {
  idVec3 center{};
  if (numPoints <= 0)
    return center;
  for (int index = 0; index < numPoints; ++index)
    center += p[index].ToVec3();
  return center * (1.0f / static_cast<float>(numPoints));
}

void idWinding::GetBounds(idBounds& bounds) const {
  bounds.Clear();
  for (int index = 0; index < numPoints; ++index)
    bounds.AddPoint(p[index].ToVec3());
}

void idWinding::AddToConvexHull(const idVec3& point, const idVec3& normal, float epsilon) {
  if (numPoints == 0) {
    AddPoint({point.x, point.y, point.z, 0.0f, 0.0f});
    return;
  }
  for (int index = 0; index < numPoints; ++index) {
    if ((p[index].ToVec3() - point).LengthSqr() <= epsilon * epsilon)
      return;
  }
  std::vector<idVec3> points;
  points.reserve(static_cast<std::size_t>(numPoints + 1));
  for (int index = 0; index < numPoints; ++index)
    points.push_back(p[index].ToVec3());
  points.push_back(point);
  const idVec3 center = [&] {
    idVec3 value{};
    for (const idVec3& item : points)
      value += item;
    return value / static_cast<float>(points.size());
  }();
  idVec3 basisX = std::fabs(normal.z) < 0.9f ? idVec3{0.0f, 0.0f, 1.0f}.Cross(normal)
                                             : idVec3{1.0f, 0.0f, 0.0f}.Cross(normal);
  basisX.Normalize();
  const idVec3 basisY = normal.Cross(basisX);
  std::sort(points.begin(), points.end(), [&](const idVec3& left, const idVec3& right) {
    const idVec3 dl = left - center;
    const idVec3 dr = right - center;
    return std::atan2(dl * basisY, dl * basisX) < std::atan2(dr * basisY, dr * basisX);
  });
  ReAllocate(static_cast<int>(points.size()), false);
  numPoints = static_cast<int>(points.size());
  for (int index = 0; index < numPoints; ++index)
    p[index] = {points[index].x, points[index].y, points[index].z, 0.0f, 0.0f};
}

bool idWinding::IsHuge(float radius) const {
  for (int index = 0; index < numPoints; ++index) {
    const idVec3 point = p[index].ToVec3();
    if (std::fabs(point.x) > radius || std::fabs(point.y) > radius || std::fabs(point.z) > radius)
      return true;
  }
  return false;
}

bool idWinding::PointInside(const idVec3& normal, const idVec3& point, float epsilon) const {
  for (int index = 0; index < numPoints; ++index) {
    const idVec3 edge = p[(index + 1) % numPoints].ToVec3() - p[index].ToVec3();
    const idVec3 edgeNormal = normal.Cross(edge);
    if (edgeNormal * (point - p[index].ToVec3()) > epsilon)
      return false;
  }
  return true;
}

void idWinding::GetPlane(idPlane& plane) const {
  if (numPoints < 3) {
    plane = {0.0f, 0.0f, 0.0f, 0.0f};
    return;
  }
  idVec3 normal = (p[1].ToVec3() - p[0].ToVec3()).Cross(p[2].ToVec3() - p[0].ToVec3());
  normal.Normalize();
  plane = {normal.x, normal.y, normal.z, -(normal * p[0].ToVec3())};
}

planeSide_t idFixedWinding::SplitInPlace(const idPlane& plane, float epsilon,
                                         idFixedWinding& back) {
  if (numPoints == 0) {
    back.Clear();
    return PLANESIDE_ON;
  }
  float distances[64];
  unsigned char sides[64];
  int counts[3];
  CalculateSides(p, static_cast<unsigned>(numPoints), plane, epsilon, distances, sides, counts);
  if (counts[PLANESIDE_FRONT] == 0) {
    back = *this;
    Clear();
    return counts[PLANESIDE_BACK] == 0 ? PLANESIDE_ON : PLANESIDE_BACK;
  }
  if (counts[PLANESIDE_BACK] == 0) {
    back.Clear();
    return PLANESIDE_FRONT;
  }
  idFixedWinding front;
  back.Clear();
  for (int index = 0; index < numPoints; ++index) {
    const int next = (index + 1) % numPoints;
    if (sides[index] != PLANESIDE_BACK)
      front.AddPoint(p[index]);
    if (sides[index] != PLANESIDE_FRONT)
      back.AddPoint(p[index]);
    if ((sides[index] == PLANESIDE_FRONT && sides[next] == PLANESIDE_BACK) ||
        (sides[index] == PLANESIDE_BACK && sides[next] == PLANESIDE_FRONT)) {
      const float fraction = distances[index] / (distances[index] - distances[next]);
      const idVec5 intersection = Interpolate(p[index], p[next], fraction);
      front.AddPoint(intersection);
      back.AddPoint(intersection);
    }
  }
  *this = front;
  return PLANESIDE_CROSS;
}
