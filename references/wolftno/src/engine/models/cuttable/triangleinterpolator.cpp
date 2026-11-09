#include "engine/models/cuttable/triangleinterpolator.h"

#include "shared/idlib/color.h"

#include <algorithm>
#include <cmath>
#include <cstring>
#include <immintrin.h>
#include <limits>

namespace {

constexpr float kFloatEpsilon = 1.192092896e-7f;
constexpr float kByteToSignedFloat = 0.0078431377f;

float Dot2(const idVec2& left, const idVec2& right) {
  return left.x * right.x + left.y * right.y;
}

float InverseLength(float lengthSquared) {
  const float clamped = _mm_cvtss_f32(_mm_max_ss(
      _mm_set_ss(lengthSquared),
      _mm_set_ss(std::numeric_limits<float>::min())));
  return 1.0f / std::sqrt(clamped);
}

idVec2 ExtrapolateStart(const idVec2& first, const idVec2& second,
                        const idVec2& third, float inverseFirstThird,
                        float inverseSecondFirst) {
  return first - (third - first) * inverseFirstThird +
         (first - second) * inverseSecondFirst;
}

idVec2 ExtrapolateSecond(const idVec2& first, const idVec2& second,
                         const idVec2& third, float inverseSecondFirst,
                         float inverseThirdSecond) {
  return second - (first - second) * inverseSecondFirst +
         (second - third) * inverseThirdSecond;
}

idVec2 ExtrapolateThird(const idVec2& first, const idVec2& second,
                        const idVec2& third, float inverseFirstThird,
                        float inverseThirdSecond) {
  return third + (third - first) * inverseFirstThird -
         (second - third) * inverseThirdSecond;
}

float Decode(std::uint8_t value) {
  return static_cast<float>(value) * kByteToSignedFloat - 1.0f;
}

std::uint8_t Encode(float value) {
  const float scaled = (value + 1.0f) * 127.5f + 0.5f;
  const int rounded = _mm_cvtss_si32(_mm_set_ss(scaled));
  return static_cast<std::uint8_t>(std::clamp(rounded, 0, 255));
}

void FixDegenerateNormal(idVec3& normal) {
  const float smallestNormal = std::numeric_limits<float>::min();
  const float absX = std::fabs(normal.x);
  const float absY = std::fabs(normal.y);
  if (absX >= smallestNormal) {
    if (absY < smallestNormal && std::fabs(normal.z) < smallestNormal) {
      normal = {normal.x <= 0.0f ? -1.0f : 1.0f, 0.0f, 0.0f};
      return;
    }
  } else {
    if (absY < smallestNormal) {
      normal = {0.0f, 0.0f, normal.z <= 0.0f ? -1.0f : 1.0f};
      return;
    }
    if (std::fabs(normal.z) < smallestNormal) {
      normal = {0.0f, normal.y <= 0.0f ? -1.0f : 1.0f, 0.0f};
      return;
    }
  }

  const float almostOne = 1.0f - kFloatEpsilon;
  if (absX > almostOne) {
    normal = {normal.x <= 0.0f ? -1.0f : 1.0f, 0.0f, 0.0f};
  } else if (absY > almostOne) {
    normal = {0.0f, normal.y <= 0.0f ? -1.0f : 1.0f, 0.0f};
  } else if (std::fabs(normal.z) > almostOne) {
    normal = {0.0f, 0.0f, normal.z <= 0.0f ? -1.0f : 1.0f};
  }
}

bool PlaneFromPoints(idPlane& plane, const idVec3& first,
                     const idVec3& second, const idVec3& third) {
  idVec3 normal = (first - second).Cross(third - second);
  const float lengthSquared = normal.LengthSqr();
  const float inverse = InverseLength(lengthSquared);
  normal *= inverse;
  const float length = inverse * lengthSquared;
  FixDegenerateNormal(normal);
  plane.a = normal.x;
  plane.b = normal.y;
  plane.c = normal.z;
  if (length == 0.0f)
    return false;
  plane.d = -(second * normal);
  return true;
}

void NormalVectors(const idVec3& normal, idVec3& left, idVec3& down) {
  const float horizontalLengthSquared =
      normal.x * normal.x + normal.y * normal.y;
  if (horizontalLengthSquared == 0.0f) {
    left = {1.0f, 0.0f, 0.0f};
  } else {
    const float inverse = InverseLength(horizontalLengthSquared);
    left = {-inverse * normal.y, inverse * normal.x, 0.0f};
  }
  down = left.Cross(normal);
}

std::uint32_t PackedColor(const idDrawVert& vertex) {
  std::uint32_t color = 0;
  std::memcpy(&color, vertex.color, sizeof(color));
  return color;
}

} // namespace

int idTriangleInterpolator::FindTriangle(const idVec2& point) const {
  float bestSum = 1.0e30f;
  int bestIndex = -1;
  for (int index = 0; index < front.Num(); ++index) {
    const triangle_t& triangle = front[index];
    const idVec2 delta = point - triangle.xy;
    const float dot0 = Dot2(delta, triangle.v0);
    const float dot1 = Dot2(delta, triangle.v1);
    const float u =
        (dot0 * triangle.d11 - triangle.d01 * dot1) * triangle.invDenom;
    const float v =
        (dot1 * triangle.d00 - triangle.d01 * dot0) * triangle.invDenom;
    if (u < -kFloatEpsilon || v < -kFloatEpsilon)
      continue;
    const float sum = u + v;
    if (sum <= 1.0f + kFloatEpsilon)
      return index;
    if (sum < bestSum) {
      bestSum = sum;
      bestIndex = index;
    }
  }
  return bestIndex;
}

idVec3 idTriangleInterpolator::InterpolateVector(const idVec3& v0,
                                                  const idVec3& v1,
                                                  const idVec3& v2, float u,
                                                  float v) {
  const idVec3 result =
      (v0 * (1.0f - u) + v1 * u) * (1.0f - v) + v2 * v;
  return result * InverseLength(result.LengthSqr());
}

std::uint32_t idTriangleInterpolator::InterpolateColor(
    const triangle_t& triangle, float u, float v) {
  idVec4 color0{};
  idVec4 color1{};
  idVec4 color2{};
  idColor::UnpackColor(triangle.c0, color0);
  idColor::UnpackColor(triangle.c1, color1);
  idColor::UnpackColor(triangle.c2, color2);
  const float oneMinusU = 1.0f - u;
  const float oneMinusV = 1.0f - v;
  const idVec4 color{
      (color0.x * oneMinusU + color1.x * u) * oneMinusV + color2.x * v,
      (color0.y * oneMinusU + color1.y * u) * oneMinusV + color2.y * v,
      (color0.z * oneMinusU + color1.z * u) * oneMinusV + color2.z * v,
      (color0.w * oneMinusU + color1.w * u) * oneMinusV + color2.w * v};
  return idColor::PackColor(color);
}

const idVec3 idDrawVert::GetBiTangentRaw() const {
  const idVec3 decodedNormal{Decode(normal[0]), Decode(normal[1]),
                             Decode(normal[2])};
  const idVec3 decodedTangent{Decode(tangent[0]), Decode(tangent[1]),
                              Decode(tangent[2])};
  const float handedness = (tangent[3] & 0x80u) != 0 ? 1.0f : -1.0f;
  return decodedNormal.Cross(decodedTangent) * handedness;
}

void idTriangleInterpolator::Clear() {
  if (front.listStatic == 0 || front.listStatic == 2) {
    delete[] front.list;
    front.list = nullptr;
    front.size = 0;
  }
  front.num = 0;
  if (back.listStatic == 0 || back.listStatic == 2) {
    delete[] back.list;
    back.list = nullptr;
    back.size = 0;
  }
  back.num = 0;
  refVert = {};
  std::memset(refVert.color, 0xff, sizeof(refVert.color));
  refPlane = {};
  refLeft = {};
  refDown = {};
}

idTriangleInterpolator::triangle_t idTriangleInterpolator::CreateTriangle(
    const idVec3& left, const idVec3& down, const idDrawVert* verts,
    const idVec2* uv, const std::uint16_t* indices, int triIndex) {
  triangle_t result{};
  const std::uint16_t* triangleIndices = indices + triIndex * 3;
  const idDrawVert& first = verts[triangleIndices[0]];
  const idDrawVert& second = verts[triangleIndices[1]];
  const idDrawVert& third = verts[triangleIndices[2]];

  const idVec2 firstPoint{first.xyz * left, first.xyz * down};
  const idVec2 secondPoint{second.xyz * left, second.xyz * down};
  const idVec2 thirdPoint{third.xyz * left, third.xyz * down};

  const idVec2 firstToThird = thirdPoint - firstPoint;
  const float firstThirdLengthSquared = firstToThird.LengthSqr();
  const float firstThirdInverseLength =
      InverseLength(firstThirdLengthSquared);
  const idVec2 firstThirdDirection = firstToThird * firstThirdInverseLength;
  const idVec2 extendedFirst = firstPoint - firstThirdDirection;
  const idVec2 extendedThird = thirdPoint + firstThirdDirection;
  const float inverseFirstThird =
      1.0f / (firstThirdInverseLength * firstThirdLengthSquared);

  const idVec2 secondToFirst = extendedFirst - secondPoint;
  const float secondFirstLengthSquared = secondToFirst.LengthSqr();
  const float secondFirstInverseLength =
      InverseLength(secondFirstLengthSquared);
  const idVec2 secondFirstDirection = secondToFirst * secondFirstInverseLength;
  const idVec2 expandedFirst = extendedFirst + secondFirstDirection;
  const idVec2 extendedSecond = secondPoint - secondFirstDirection;
  const float inverseSecondFirst =
      1.0f / (secondFirstInverseLength * secondFirstLengthSquared);

  const idVec2 thirdToSecond = extendedSecond - extendedThird;
  const float thirdSecondLengthSquared = thirdToSecond.LengthSqr();
  const float thirdSecondInverseLength =
      InverseLength(thirdSecondLengthSquared);
  const idVec2 thirdSecondDirection = thirdToSecond * thirdSecondInverseLength;
  const idVec2 expandedSecond = extendedSecond + thirdSecondDirection;
  const idVec2 expandedThird = extendedThird - thirdSecondDirection;
  const float inverseThirdSecond =
      1.0f / (thirdSecondInverseLength * thirdSecondLengthSquared);

  result.xy = expandedFirst;
  result.v0 = expandedThird - expandedFirst;
  result.v1 = expandedSecond - expandedFirst;
  result.d00 = Dot2(result.v0, result.v0);
  result.d01 = Dot2(result.v0, result.v1);
  result.d11 = Dot2(result.v1, result.v1);
  result.invDenom =
      1.0f / (result.d11 * result.d00 - result.d01 * result.d01);
  result.invArea =
      1.0f / (result.v0.y * result.v1.x - result.v0.x * result.v1.y);

  const idVec2 primary[3] = {first.st, second.st, third.st};
  const idVec2 secondary[3] = {uv[triangleIndices[0]], uv[triangleIndices[1]],
                               uv[triangleIndices[2]]};
  for (int channel = 0; channel < 2; ++channel) {
    const idVec2* coordinates = channel == 0 ? primary : secondary;
    const idVec2 expandedCoordinateFirst =
        ExtrapolateStart(coordinates[0], coordinates[1], coordinates[2],
                         inverseFirstThird, inverseSecondFirst);
    const idVec2 expandedCoordinateSecond =
        ExtrapolateSecond(coordinates[0], coordinates[1], coordinates[2],
                          inverseSecondFirst, inverseThirdSecond);
    const idVec2 expandedCoordinateThird =
        ExtrapolateThird(coordinates[0], coordinates[1], coordinates[2],
                         inverseFirstThird, inverseThirdSecond);
    result.st[channel] = expandedCoordinateFirst;
    result.st0[channel] =
        expandedCoordinateThird - expandedCoordinateFirst;
    result.st1[channel] =
        expandedCoordinateSecond - expandedCoordinateFirst;
  }

  result.c0 = PackedColor(first);
  result.c1 = PackedColor(second);
  result.c2 = PackedColor(third);
  const idDrawVert* source[3] = {&first, &second, &third};
  for (int vertex = 0; vertex < 3; ++vertex) {
    std::memcpy(result.tangent[vertex], source[vertex]->tangent,
                sizeof(result.tangent[vertex]));
    std::memcpy(result.normal[vertex], source[vertex]->normal,
                sizeof(result.normal[vertex]));
  }
  return result;
}

void idTriangleInterpolator::InterpolateTBN(const triangle_t& triangle,
                                            float u, float v, idVec3& tangent,
                                            idVec3& bitangent,
                                            idVec3& normal) {
  idDrawVert vertices[3]{};
  for (int index = 0; index < 3; ++index) {
    std::memcpy(vertices[index].tangent, triangle.tangent[index],
                sizeof(vertices[index].tangent));
    std::memcpy(vertices[index].normal, triangle.normal[index],
                sizeof(vertices[index].normal));
  }

  const idVec3 normals[3] = {
      {Decode(vertices[0].normal[0]), Decode(vertices[0].normal[1]),
       Decode(vertices[0].normal[2])},
      {Decode(vertices[1].normal[0]), Decode(vertices[1].normal[1]),
       Decode(vertices[1].normal[2])},
      {Decode(vertices[2].normal[0]), Decode(vertices[2].normal[1]),
       Decode(vertices[2].normal[2])}};
  const idVec3 tangents[3] = {
      {Decode(vertices[0].tangent[0]), Decode(vertices[0].tangent[1]),
       Decode(vertices[0].tangent[2])},
      {Decode(vertices[1].tangent[0]), Decode(vertices[1].tangent[1]),
       Decode(vertices[1].tangent[2])},
      {Decode(vertices[2].tangent[0]), Decode(vertices[2].tangent[1]),
       Decode(vertices[2].tangent[2])}};
  normal = InterpolateVector(normals[0], normals[1], normals[2], u, v);
  tangent = InterpolateVector(tangents[0], tangents[1], tangents[2], u, v);

  const idVec3 bitangents[3] = {vertices[0].GetBiTangentRaw(),
                                vertices[1].GetBiTangentRaw(),
                                vertices[2].GetBiTangentRaw()};
  bitangent =
      InterpolateVector(bitangents[0], bitangents[1], bitangents[2], u, v);
}

void idTriangleInterpolator::Interpolate(const idVec2& point,
                                         const triangle_t& triangle,
                                         idDrawVert& vert, idVec2& st) const {
  const idVec2 delta = point - triangle.xy;
  const float dot0 = Dot2(delta, triangle.v0);
  const float dot1 = Dot2(delta, triangle.v1);
  const float u =
      (dot0 * triangle.d11 - triangle.d01 * dot1) * triangle.invDenom;
  const float v =
      (dot1 * triangle.d00 - triangle.d01 * dot0) * triangle.invDenom;
  const idVec2 primary =
      triangle.st[0] + triangle.st0[0] * u + triangle.st1[0] * v;
  st = triangle.st[1] + triangle.st0[1] * u + triangle.st1[1] * v;

  idVec3 tangent{};
  idVec3 bitangent{};
  idVec3 normal{};
  InterpolateTBN(triangle, u, v, tangent, bitangent, normal);
  const std::uint32_t color = InterpolateColor(triangle, u, v);
  const float handedness =
      normal.Cross(tangent) * bitangent >= 0.0f ? 1.0f : -1.0f;

  vert = refVert;
  const idVec3 projected = refLeft * point.x + refDown * point.y;
  const idVec3 planeNormal{refPlane.a, refPlane.b, refPlane.c};
  const float planeDistance = planeNormal * projected + refPlane.d;
  vert.xyz = projected - planeNormal * planeDistance;
  vert.st = primary;
  std::memcpy(vert.color, &color, sizeof(color));
  vert.normal[0] = Encode(normal.x);
  vert.normal[1] = Encode(normal.y);
  vert.normal[2] = Encode(normal.z);
  vert.tangent[0] = Encode(tangent.x);
  vert.tangent[1] = Encode(tangent.y);
  vert.tangent[2] = Encode(tangent.z);
  vert.tangent[3] &= ~0x80u;
  if (handedness >= 0.0f)
    vert.tangent[3] |= 0x80u;
}

bool idTriangleInterpolator::Interpolate(const idVec2& point,
                                         idDrawVert* verts, idVec2* st) const {
  const int triangleIndex = FindTriangle(point);
  if (triangleIndex < 0)
    return false;
  Interpolate(point, front[triangleIndex], verts[0], st[0]);
  Interpolate(point, back[triangleIndex], verts[1], st[1]);
  return true;
}

void idTriangleInterpolator::AddTriangles(const idDrawVert* verts,
                                          const idVec2* st,
                                          const std::uint16_t* indices,
                                          int numFrontIndices) {
  const int triangleCount = numFrontIndices / 3;
  if (triangleCount <= 0)
    return;

  const idDrawVert& first = verts[indices[0]];
  PlaneFromPoints(refPlane, first.xyz, verts[indices[1]].xyz,
                  verts[indices[2]].xyz);
  NormalVectors({refPlane.a, refPlane.b, refPlane.c}, refLeft, refDown);
  refVert = first;

  front.SetNum(triangleCount);
  back.SetNum(triangleCount);
  for (int triangle = 0; triangle < triangleCount; ++triangle) {
    front[triangle] =
        CreateTriangle(refLeft, refDown, verts, st, indices, triangle);
    back[triangle] = CreateTriangle(refLeft, refDown, verts, st, indices,
                                    triangle + triangleCount);
  }
}
