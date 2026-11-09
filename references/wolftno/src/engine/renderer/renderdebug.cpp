#include "engine/models/renderworld.h"

#include "shared/idlib/containers/heaparray.h"
#include "shared/idlib/csystems/cvarsystemlocal.h"
#include "shared/idlib/math/mathlib.h"

#include <algorithm>
#include <cmath>
#include <cstring>
#include <new>
#include <type_traits>

// Owned by renderer/rendersystem.cpp in the retail image.
extern idCVar r_debugArrowStep;

namespace {

constexpr float kDegreesToRadians = 0.017453292f;
constexpr float kTwoPi = 6.2831855f;

const idVec4& ColorVector(const idColor& color) {
  return reinterpret_cast<const idVec4&>(color);
}

idVec3 NormalizeRetail(const idVec3& value) {
  const float lengthSquared = value.x * value.x + value.y * value.y +
                              value.z * value.z;
  const float inverseLength =
      1.0f / std::sqrt(std::max(lengthSquared,
                                idMath::FLT_SMALLEST_NON_DENORMAL));
  return {value.x * inverseLength, value.y * inverseLength,
          value.z * inverseLength};
}

void NormalVectorsRetail(const idVec3& direction, idVec3& left,
                         idVec3& down) {
  const float horizontalLengthSquared =
      direction.x * direction.x + direction.y * direction.y;
  if (horizontalLengthSquared == 0.0f) {
    left = {1.0f, 0.0f, 0.0f};
  } else {
    const float inverseLength =
        1.0f / std::sqrt(std::max(horizontalLengthSquared,
                                  idMath::FLT_SMALLEST_NON_DENORMAL));
    left = {-direction.y * inverseLength,
            direction.x * inverseLength, 0.0f};
  }
  down = left.Cross(direction);
}

float WrappedSin(float angle) {
  if (angle < 0.0f || angle >= kTwoPi)
    angle -= std::floor(angle * 0.15915494f) * kTwoPi;
  return std::sin(angle);
}

float WrappedCos(float angle) {
  if (angle < 0.0f || angle >= kTwoPi)
    angle -= std::floor(angle * 0.15915494f) * kTwoPi;
  return std::cos(angle);
}

idVec3 TransformPointRetail(const idVec3& point, const idVec3& origin,
                            const idMat3& axis) {
  return {point.y * axis.mat[1].x + point.x * axis.mat[0].x +
              point.z * axis.mat[2].x + origin.x,
          point.x * axis.mat[0].y + point.y * axis.mat[1].y +
              point.z * axis.mat[2].y + origin.y,
          point.x * axis.mat[0].z + point.y * axis.mat[1].z +
              point.z * axis.mat[2].z + origin.z};
}

void AddWindingPoint(idWinding& winding, const idVec3& point) {
  winding.AddPoint({point.x, point.y, point.z, 0.0f, 0.0f});
}

bool StopTimeActive() {
  return cvarSystem->GetCVarBool("g_stopTime", false) &&
         cvarSystem->GetCVarInteger("g_runFrames", 0) <= 0;
}

int DebugLifetime(int lifetime) {
  return StopTimeActive() ? -1 : lifetime;
}

void BoundsPoints(const idBounds& bounds, idVec3 points[8]) {
  points[0] = {bounds.b[0].x, bounds.b[0].y, bounds.b[0].z};
  points[1] = {bounds.b[1].x, bounds.b[0].y, bounds.b[0].z};
  points[2] = {bounds.b[1].x, bounds.b[1].y, bounds.b[0].z};
  points[3] = {bounds.b[0].x, bounds.b[1].y, bounds.b[0].z};
  points[4] = {bounds.b[0].x, bounds.b[0].y, bounds.b[1].z};
  points[5] = {bounds.b[1].x, bounds.b[0].y, bounds.b[1].z};
  points[6] = {bounds.b[1].x, bounds.b[1].y, bounds.b[1].z};
  points[7] = {bounds.b[0].x, bounds.b[1].y, bounds.b[1].z};
}

} // namespace

// This debug switch is owned by idlib in the retail image.  Keeping it as an
// external dependency preserves the idList::Append contract and the retail
// RenderDebug.obj dependency surface.
extern bool forceIdListsToReallocateEveryAppend;

template <typename T>
__declspec(noinline) void* idListArrayResize(void* oldPointer, int oldNum,
                                             int newNum, memTag_t tag,
                                             bool zeroBuffer) {
  T* oldList = static_cast<T*>(oldPointer);
  T* replacement = nullptr;

  if (newNum > 0) {
    const char* location =
        zeroBuffer
            ? "w:\\build\\tech5\\shared\\idlib\\containers/List.h(27) : tag"
            : "w:\\build\\tech5\\shared\\idlib\\containers/List.h(29) : tag";
    replacement = static_cast<T*>(idMem::Singleton()->AllocWithLocation(
        location, sizeof(T) * static_cast<std::size_t>(newNum), tag,
        zeroBuffer, ALIGN_16, HEAP_DEFAULTHEAP));
    if (replacement == nullptr)
      return nullptr;

    if constexpr (!std::is_trivially_default_constructible_v<T>) {
      for (int index = 0; index < newNum; ++index)
        new (&replacement[index]) T;
    }

    const int copyCount = (std::min)(oldNum, newNum);
    for (int index = 0; index < copyCount; ++index)
      replacement[index] = oldList[index];
  }

  if constexpr (!std::is_trivially_destructible_v<T>) {
    for (int index = 0; index < oldNum; ++index)
      oldList[index].~T();
  }
  idMem::Singleton()->Free(oldList, ALIGN_16);
  return replacement;
}

#define DEFINE_RENDER_DEBUG_TRIVIAL_RESIZE(ElementType)                       \
  template <>                                                                \
  __declspec(noinline) bool idList<ElementType, 3>::Resize(int newSize) {     \
    if (newSize <= 0) {                                                       \
      if (listStatic == 0 || listStatic == 2) {                               \
        if (list != nullptr)                                                  \
          idMem::Singleton()->Free(list, ALIGN_16);                           \
        list = nullptr;                                                       \
        size = 0;                                                             \
      }                                                                       \
      num = 0;                                                                \
      return true;                                                            \
    }                                                                         \
    if (newSize == size || listStatic != 0)                                   \
      return true;                                                            \
    list = static_cast<ElementType*>(idListArrayResize<ElementType>(          \
        list, size, newSize, static_cast<memTag_t>(memTag), false));          \
    if (list == nullptr)                                                      \
      return false;                                                           \
    size = newSize;                                                           \
    num = (std::min)(num, newSize);                                           \
    return true;                                                              \
  }

DEFINE_RENDER_DEBUG_TRIVIAL_RESIZE(debugLine_t)
DEFINE_RENDER_DEBUG_TRIVIAL_RESIZE(debugBounds_t)

#undef DEFINE_RENDER_DEBUG_TRIVIAL_RESIZE

template <>
__declspec(noinline) bool idList<debugText_t, 3>::Resize(int newSize) {
  if (newSize <= 0) {
    if (listStatic == 0 || listStatic == 2) {
      if (list != nullptr)
        idMem::Singleton()->Free(list, ALIGN_16);
      list = nullptr;
      size = 0;
    }
    num = 0;
    return true;
  }
  if (newSize == size || listStatic != 0)
    return true;

  debugText_t* oldList = list;
  debugText_t* replacement =
      static_cast<debugText_t*>(idMem::Singleton()->AllocWithLocation(
          "w:\\build\\tech5\\shared\\idlib\\containers/List.h(29) : tag",
          sizeof(debugText_t) * static_cast<std::size_t>(newSize),
          static_cast<memTag_t>(memTag), false, ALIGN_16, HEAP_DEFAULTHEAP));
  if (replacement != nullptr) {
    const int copyCount = (std::min)(size, newSize);
    for (int index = 0; index < copyCount; ++index)
      replacement[index] = oldList[index];
    idMem::Singleton()->Free(oldList, ALIGN_16);
  }
  list = replacement;
  if (list == nullptr)
    return false;
  size = newSize;
  num = (std::min)(num, newSize);
  return true;
}

template <>
__declspec(noinline) bool idList<debugPolygon_t, 3>::Resize(int newSize) {
  if (newSize <= 0) {
    if (listStatic == 0 || listStatic == 2) {
      if (list != nullptr) {
        for (int index = 0; index < size; ++index)
          list[index].~debugPolygon_t();
        idMem::Singleton()->Free(list, ALIGN_16);
      }
      list = nullptr;
      size = 0;
    }
    num = 0;
    return true;
  }
  if (newSize == size || listStatic != 0)
    return true;

  list = static_cast<debugPolygon_t*>(idListArrayResize<debugPolygon_t>(
      list, size, newSize, static_cast<memTag_t>(memTag), false));
  if (list == nullptr)
    return false;
  size = newSize;
  num = (std::min)(num, newSize);
  return true;
}

#define DEFINE_RENDER_DEBUG_APPEND(ElementType)                               \
  template <>                                                                \
  __declspec(noinline) int idList<ElementType, 3>::Append(                    \
      const ElementType& value) {                                             \
    if (list == nullptr) {                                                    \
      const int initialSize = granularity > 0 ? granularity : 1;              \
      if (!Resize(initialSize))                                               \
        return -1;                                                            \
    }                                                                         \
    if (num == size) {                                                        \
      int newSize;                                                            \
      if (forceIdListsToReallocateEveryAppend) {                              \
        newSize = size + 1;                                                   \
      } else if (granularity != 0) {                                          \
        const int candidate = size + granularity;                             \
        newSize = candidate - candidate % granularity;                        \
      } else if (size <= 0x3fffffff) {                                        \
        newSize = size * 2;                                                   \
        if (newSize == 0)                                                     \
          newSize = 1;                                                        \
      } else {                                                                \
        newSize = 0x7fffffff;                                                 \
      }                                                                       \
      if (!Resize(newSize))                                                   \
        return -1;                                                            \
    }                                                                         \
    if (num >= size)                                                          \
      return num - 1;                                                         \
    list[num] = value;                                                        \
    return num++;                                                             \
  }

DEFINE_RENDER_DEBUG_APPEND(debugLine_t)
DEFINE_RENDER_DEBUG_APPEND(debugBounds_t)
DEFINE_RENDER_DEBUG_APPEND(debugText_t)
DEFINE_RENDER_DEBUG_APPEND(debugPolygon_t)

#undef DEFINE_RENDER_DEBUG_APPEND

template void* idListArrayResize<debugLine_t>(void*, int, int, memTag_t,
                                               bool);
template void* idListArrayResize<debugBounds_t>(void*, int, int, memTag_t,
                                                 bool);
template void* idListArrayResize<debugPolygon_t>(void*, int, int, memTag_t,
                                                  bool);

float idMath::Cos16(float angle) {
  if (angle < 0.0f || angle >= kTwoPi)
    angle -= std::floor(angle * 0.15915494f) * kTwoPi;

  float sign = 1.0f;
  if (angle >= idMath::PI) {
    if (angle > 4.712389f) {
      angle -= kTwoPi;
    } else {
      sign = -1.0f;
      angle = idMath::PI - angle;
    }
  } else if (angle > 1.5707964f) {
    sign = -1.0f;
    angle = idMath::PI - angle;
  }

  const float square = angle * angle;
  return (((((0.0000247609f - square * 0.0000002605f) * square -
              0.0013888397f) *
                 square +
             0.041666642f) *
                square -
            0.5f) *
               square +
           1.0f) *
          sign;
}

void idRenderDebugLocal::DebugArrow3(const idVec4& color,
                                     const idVec3& start,
                                     const idVec3& end, float size,
                                     int lifetime, bool depthTest) {
  const idVec3 forward = NormalizeRetail(end - start);
  idVec3 right{};
  const float horizontalLengthSquared =
      forward.x * forward.x + forward.y * forward.y;
  if (horizontalLengthSquared == 0.0f) {
    right = {1.0f, 0.0f, 0.0f};
  } else {
    const float inverseLength =
        1.0f / std::sqrt(std::max(horizontalLengthSquared,
                                  idMath::FLT_SMALLEST_NON_DENORMAL));
    right = {-forward.y * inverseLength, forward.x * inverseLength, 0.0f};
  }

  DebugLine(color, start, end, lifetime, depthTest);
  const idVec3 base = end - forward * size;
  // Retail hard-codes the pyramid portion to persistent/no-depth even though
  // the shaft uses the caller's lifetime and depth flag.
  DebugPyramid(color, base, forward, right, size, size, 0, false);
}

void idRenderDebugLocal::DebugPyramid(const idVec4& color,
                                      const idVec3& base,
                                      const idVec3& up,
                                      const idVec3& forward, float height,
                                      float width, int lifetime,
                                      bool depthTest) {
  const idVec3 peak = base + up * height;
  const idVec3 side = forward.Cross(up) * width;
  const idVec3 longSide = forward * width;
  const idVec3 corner1 = base + side * 0.5f + longSide * 0.5f;
  const idVec3 corner2 = corner1 - longSide;
  const idVec3 corner3 = corner2 - side;
  const idVec3 corner4 = corner3 + longSide;

  DebugLine(color, corner1, corner2, lifetime, depthTest);
  DebugLine(color, corner2, corner3, lifetime, depthTest);
  DebugLine(color, corner3, corner4, lifetime, depthTest);
  DebugLine(color, corner4, corner1, lifetime, depthTest);
  DebugLine(color, corner1, peak, lifetime, depthTest);
  DebugLine(color, corner2, peak, lifetime, depthTest);
  DebugLine(color, corner3, peak, lifetime, depthTest);
  DebugLine(color, corner4, peak, lifetime, depthTest);
}

void idRenderDebugLocal::DebugWinding(const idVec4& color,
                                      const idWinding& winding,
                                      const idVec3& origin,
                                      const idMat3& axis, int lifetime,
                                      bool depthTest) {
  if (winding.GetNumPoints() < 2)
    return;

  idVec3 last = TransformPointRetail(
      winding[winding.GetNumPoints() - 1].ToVec3(), origin, axis);
  for (int index = 0; index < winding.GetNumPoints(); ++index) {
    const idVec3 point =
        TransformPointRetail(winding[index].ToVec3(), origin, axis);
    DebugLine(color, last, point, lifetime, depthTest);
    last = point;
  }
}

void idRenderDebugLocal::DebugCircle(const idVec4& color,
                                     const idVec3& origin,
                                     const idVec3& direction, float radius,
                                     int numSteps, int lifetime,
                                     bool depthTest) {
  idVec3 firstAxis{};
  idVec3 secondAxis{};
  NormalVectorsRetail(direction, firstAxis, secondAxis);
  firstAxis *= radius;
  secondAxis *= radius;

  idVec3 last = origin + firstAxis;
  for (int step = 1; step <= numSteps; ++step) {
    const float angle = static_cast<float>(step) * kTwoPi /
                        static_cast<float>(numSteps);
    const idVec3 point = origin + firstAxis * WrappedCos(angle) +
                         secondAxis * WrappedSin(angle);
    DebugLine(color, last, point, lifetime, depthTest);
    last = point;
  }
}

void idRenderDebugLocal::DebugCylinder(const idVec4& color,
                                       const idCylinder& cylinder,
                                       const idVec3& direction, int numSteps,
                                       int lifetime, bool depthTest) {
  idVec3 firstAxis{};
  idVec3 secondAxis{};
  NormalVectorsRetail(direction, firstAxis, secondAxis);
  firstAxis *= cylinder.radius;
  secondAxis *= cylinder.radius;

  idVec3 previous = cylinder.origin + firstAxis;
  for (int step = 1; step <= numSteps; ++step) {
    const float angle = static_cast<float>(step) * kTwoPi /
                        static_cast<float>(numSteps);
    const idVec3 current = cylinder.origin +
                           firstAxis * WrappedCos(angle) +
                           secondAxis * WrappedSin(angle);
    const idVec3 previousTop =
        previous + direction * cylinder.halfHeight;
    const idVec3 currentTop = current + direction * cylinder.halfHeight;
    const idVec3 previousBottom =
        previous - direction * cylinder.halfHeight;
    const idVec3 currentBottom = current - direction * cylinder.halfHeight;
    DebugLine(color, previousTop, currentTop, lifetime, depthTest);
    DebugLine(color, previousBottom, currentBottom, lifetime, depthTest);
    DebugLine(color, currentTop, currentBottom, lifetime, depthTest);
    previous = current;
  }
}

void idRenderDebugLocal::DebugArc(const idVec4& color,
                                  const idVec3& origin,
                                  const idVec3& direction, float radius,
                                  float startAngle, float endAngle,
                                  float anglesPerSegment, int lifetime,
                                  bool depthTest, bool drawSectorEdges) {
  idVec3 radial{};
  idVec3 down{};
  NormalVectorsRetail(direction, radial, down);
  // Retail reconstructs the input direction as -(radial x down), retaining
  // the exact NormalVectors operand order before applying the radius.
  const idVec3 forward = -(radial.Cross(down)) * radius;
  radial *= radius;

  const float angleRange = endAngle - startAngle;
  int segments = static_cast<int>(std::fabs(angleRange / anglesPerSegment));
  if (angleRange > static_cast<float>(segments) * anglesPerSegment)
    ++segments;

  const float startRadians = startAngle * kDegreesToRadians;
  const float rangeRadians = angleRange * kDegreesToRadians;
  auto pointForAngle = [&](float angle) {
    return origin + forward * WrappedCos(angle) -
           radial * WrappedSin(angle);
  };

  idVec3 last = pointForAngle(startRadians);
  if (drawSectorEdges)
    DebugLine(color, origin, last, lifetime, depthTest);

  for (int segment = 1; segment < segments; ++segment) {
    const float angle = startRadians +
                        static_cast<float>(segment) * rangeRadians /
                            static_cast<float>(segments);
    const idVec3 point = pointForAngle(angle);
    DebugLine(color, last, point, lifetime, depthTest);
    last = point;
  }

  const idVec3 endPoint = pointForAngle(endAngle * kDegreesToRadians);
  DebugLine(color, last, endPoint, lifetime, depthTest);
  if (drawSectorEdges)
    DebugLine(color, origin, endPoint, lifetime, depthTest);
}

void idRenderDebugLocal::DebugBounds(const idVec4& color,
                                     const idBounds& bounds,
                                     const idVec3& origin, int lifetime,
                                     bool depthTest) {
  if (bounds.b[0].x > bounds.b[1].x)
    return;

  idVec3 points[8];
  BoundsPoints(bounds, points);
  for (idVec3& point : points)
    point += origin;

  for (int corner = 0; corner < 4; ++corner) {
    DebugLine(color, points[corner], points[(corner + 1) & 3], lifetime,
              depthTest);
    DebugLine(color, points[corner + 4],
              points[((corner + 1) & 3) + 4], lifetime, depthTest);
    DebugLine(color, points[corner], points[corner + 4], lifetime,
              depthTest);
  }
}

void idRenderDebugLocal::DebugOrientedBounds(
    const idVec4& color, const idBounds& bounds, const idVec3& origin,
    const idMat3& axis, int lifetime, bool depthTest) {
  if (bounds.b[0].x > bounds.b[1].x)
    return;

  idVec3 points[8];
  BoundsPoints(bounds, points);
  for (idVec3& point : points)
    point = TransformPointRetail(point, origin, axis);

  for (int corner = 0; corner < 4; ++corner) {
    DebugLine(color, points[corner], points[(corner + 1) & 3], lifetime,
              depthTest);
    DebugLine(color, points[corner + 4],
              points[((corner + 1) & 3) + 4], lifetime, depthTest);
    DebugLine(color, points[corner], points[corner + 4], lifetime,
              depthTest);
  }
}

void idRenderDebugLocal::DebugBox(const idVec4& color, const idBox& box,
                                  int lifetime, bool depthTest) {
  idVec3 points[8];
  box.ToPoints(points);
  for (int corner = 0; corner < 4; ++corner) {
    DebugLine(color, points[corner], points[(corner + 1) & 3], lifetime,
              depthTest);
    DebugLine(color, points[corner + 4],
              points[((corner + 1) & 3) + 4], lifetime, depthTest);
    DebugLine(color, points[corner], points[corner + 4], lifetime,
              depthTest);
  }
}

void idRenderDebugLocal::DebugCone(const idVec4& color,
                                   const idVec3& apex,
                                   const idVec3& direction, float radius1,
                                   float radius2, int lifetime,
                                   bool depthTest) {
  const idVec3 normalizedDirection = NormalizeRetail(direction);
  idVec3 firstAxis{};
  idVec3 secondAxis{};
  NormalVectorsRetail(normalizedDirection, firstAxis, secondAxis);
  const idVec3 secondCenter = apex + direction;

  idVec3 previousSecond = secondCenter - secondAxis * radius2;
  if (radius1 == 0.0f) {
    for (int degrees = 20; degrees <= 360; degrees += 20) {
      const float angle = static_cast<float>(degrees) * kDegreesToRadians;
      const idVec3 radial = -secondAxis * idMath::Cos16(angle) +
                            firstAxis * idMath::Sin16(angle);
      const idVec3 second = secondCenter + radial * radius2;
      DebugLine(color, previousSecond, second, lifetime, depthTest);
      DebugLine(color, second, apex, lifetime, depthTest);
      previousSecond = second;
    }
    return;
  }

  idVec3 previousFirst = apex - secondAxis * radius1;
  for (int degrees = 20; degrees <= 360; degrees += 20) {
    const float angle = static_cast<float>(degrees) * kDegreesToRadians;
    const idVec3 radial = -secondAxis * idMath::Cos16(angle) +
                          firstAxis * idMath::Sin16(angle);
    const idVec3 first = apex + radial * radius1;
    const idVec3 second = secondCenter + radial * radius2;
    DebugLine(color, previousFirst, first, lifetime, depthTest);
    DebugLine(color, previousSecond, second, lifetime, depthTest);
    DebugLine(color, first, second, lifetime, depthTest);
    previousFirst = first;
    previousSecond = second;
  }
}

void idRenderDebugLocal::DebugCone(const idVec4& color,
                                   const idVec3& apex,
                                   const idVec3& direction, float angle,
                                   int lifetime, bool depthTest) {
  const float directionLength =
      std::sqrt(std::max(direction.LengthSqr(),
                         idMath::FLT_SMALLEST_NON_DENORMAL));
  const idVec3 normalizedDirection = NormalizeRetail(direction);
  idVec3 firstAxis{};
  idVec3 secondAxis{};
  NormalVectorsRetail(normalizedDirection, firstAxis, secondAxis);

  const float radius = std::sin(angle * 0.5f) * directionLength;
  const idVec3 center = apex + direction * std::cos(angle * 0.5f);
  idVec3 previous = center - secondAxis * radius;
  for (int degrees = 20; degrees <= 360; degrees += 20) {
    const float rotation = static_cast<float>(degrees) * kDegreesToRadians;
    const idVec3 radial = -secondAxis * idMath::Cos16(rotation) +
                          firstAxis * idMath::Sin16(rotation);
    const idVec3 point = center + radial * radius;
    DebugLine(color, previous, point, lifetime, depthTest);
    DebugLine(color, point, apex, lifetime, depthTest);
    previous = point;
  }
}

void idRenderDebugLocal::DebugAxis(const idVec3& origin,
                                   const idMat3& axis, int lifetime,
                                   bool depthTest) {
  DebugArrow(ColorVector(idColor::colorRed), origin,
             origin + axis.mat[0] * 16.0f, 2.0f, lifetime, depthTest);
  DebugArrow(ColorVector(idColor::colorGreen), origin,
             origin + axis.mat[1] * 16.0f, 2.0f, lifetime, depthTest);
  DebugArrow(ColorVector(idColor::colorBlue), origin,
             origin + axis.mat[2] * 16.0f, 2.0f, lifetime, depthTest);
}

void idRenderDebugLocal::DebugAxis(const idVec4& color,
                                   const idVec3& origin,
                                   const idMat3& axis, int lifetime,
                                   bool depthTest) {
  DebugArrow(color, origin, origin + axis.mat[0] * 16.0f, 2.0f, lifetime,
             depthTest);
  DebugArrow(color, origin, origin + axis.mat[1] * 16.0f, 2.0f, lifetime,
             depthTest);
  DebugArrow(color, origin, origin + axis.mat[2] * 16.0f, 2.0f, lifetime,
             depthTest);
}

void idRenderDebugLocal::DebugAxisScaled(const idVec3& origin,
                                         const idMat3& axis, float scale,
                                         int lifetime, bool depthTest) {
  DebugLine(ColorVector(idColor::colorRed), origin,
            origin + axis.mat[0] * scale, lifetime, depthTest);
  DebugLine(ColorVector(idColor::colorGreen), origin,
            origin + axis.mat[1] * scale, lifetime, depthTest);
  DebugLine(ColorVector(idColor::colorBlue), origin,
            origin + axis.mat[2] * scale, lifetime, depthTest);
}

void idRenderDebugLocal::DebugPoint(const idVec4& color,
                                    const idVec3& origin, int lifetime,
                                    bool depthTest) {
  DebugLine(color, {origin.x - 8.0f, origin.y, origin.z},
            {origin.x + 8.0f, origin.y, origin.z}, lifetime, depthTest);
  DebugLine(color, {origin.x, origin.y - 8.0f, origin.z},
            {origin.x, origin.y + 8.0f, origin.z}, lifetime, depthTest);
  DebugLine(color, {origin.x, origin.y, origin.z - 8.0f},
            {origin.x, origin.y, origin.z + 8.0f}, lifetime, depthTest);
}

void idRenderDebugLocal::DebugArrow(const idVec4& color,
                                    const idVec3& start,
                                    const idVec3& end, float size,
                                    int lifetime, bool depthTest) {
  DebugLine(color, start, end, lifetime, depthTest);
  if (r_debugArrowStep.valueInteger <= 10)
    return;

  const idVec3 forward = NormalizeRetail(end - start);
  idVec3 right{};
  const float horizontalLengthSquared =
      forward.x * forward.x + forward.y * forward.y;
  if (horizontalLengthSquared == 0.0f) {
    right = {1.0f, 0.0f, 0.0f};
  } else {
    const float inverseLength =
        1.0f / std::sqrt(std::max(horizontalLengthSquared,
                                  idMath::FLT_SMALLEST_NON_DENORMAL));
    right = {-forward.y * inverseLength, forward.x * inverseLength, 0.0f};
  }

  idWinding winding;
  AddWindingPoint(winding, end - forward * size + right * (size * 0.5f));
  AddWindingPoint(winding, end);
  AddWindingPoint(winding, end - forward * size - right * (size * 0.5f));
  DebugFilledPolygon(color, winding, lifetime, depthTest);
}

void idRenderDebugLocal::DebugArrow2(const idVec4& color,
                                     const idVec3& start,
                                     const idVec3& end, float size,
                                     int lifetime, bool depthTest,
                                     const idVec3& cameraPosition) {
  DebugLine(color, start, end, lifetime, depthTest);
  if (r_debugArrowStep.valueInteger <= 10)
    return;

  const idVec3 forward = NormalizeRetail(end - start);
  const idVec3 perpendicular =
      NormalizeRetail((end - cameraPosition).Cross(forward));
  const idVec3 base = end - forward * size;
  idWinding winding;
  AddWindingPoint(winding, base + perpendicular * (size * 0.5f));
  AddWindingPoint(winding, base - perpendicular * (size * 0.5f));
  AddWindingPoint(winding, end);
  DebugFilledPolygon(color, winding, lifetime, depthTest);
}

void idRenderDebugLocal::DebugShadedArc(
    const idVec4& color, const idVec3& origin, const idVec3& direction,
    const idVec3& up, float radius, float startAngle, float endAngle,
    float anglesPerSegment, int lifetime, bool depthTest) {
  const idVec3 projected = direction - up * (direction * up);
  const idVec3 normalizedDirection = NormalizeRetail(projected);
  const idVec3 forward = normalizedDirection * radius;
  const idVec3 side = up.Cross(normalizedDirection) * radius;

  const float angleRange = endAngle - startAngle;
  int segments = static_cast<int>(std::fabs(angleRange / anglesPerSegment));
  if (angleRange > static_cast<float>(segments) * anglesPerSegment)
    ++segments;
  const float startRadians = startAngle * kDegreesToRadians;
  const float rangeRadians = angleRange * kDegreesToRadians;
  auto pointForAngle = [&](float angle) {
    return origin + forward * WrappedCos(angle) - side * WrappedSin(angle);
  };

  idWinding winding;
  AddWindingPoint(winding, origin);
  AddWindingPoint(winding, pointForAngle(startRadians));
  for (int segment = 1; segment < segments; ++segment) {
    const float angle = startRadians +
                        static_cast<float>(segment) * rangeRadians /
                            static_cast<float>(segments);
    AddWindingPoint(winding, pointForAngle(angle));
  }
  AddWindingPoint(winding, pointForAngle(endAngle * kDegreesToRadians));
  AddWindingPoint(winding, origin);
  DebugFilledPolygon(color, winding, lifetime, depthTest);

  // Executable byte at RVA 0x15A0378 is initialized to one in the retail
  // image, causing the reverse-facing copy to be recorded as well.
  winding.ReverseSelf();
  DebugFilledPolygon(color, winding, lifetime, depthTest);
}

void idRenderDebugLocal::DebugSphere(const idVec4& color,
                                     const idSphere& sphere, int sectors,
                                     int lifetime, bool depthTest) {
  const int angleStep = 360 / sectors;
  idHeapArray<idVec3> lastArray;
  lastArray.Alloc(static_cast<unsigned int>(sectors));
  const idVec3 northPole{sphere.origin.x, sphere.origin.y,
                         sphere.origin.z + sphere.radius};
  for (int index = 0; index < sectors; ++index) {
    lastArray.buffer[index] = northPole;
  }

  for (int latitude = angleStep; latitude <= 360; latitude += angleStep) {
    const float latitudeRadians =
        static_cast<float>(latitude) * kDegreesToRadians;
    const float latitudeSin = idMath::Sin16(latitudeRadians);
    const float latitudeCos = idMath::Cos16(latitudeRadians);
    idVec3 last{sphere.origin.x,
                sphere.origin.y + sphere.radius * latitudeSin,
                sphere.origin.z + sphere.radius * latitudeCos};

    int index = 0;
    for (int longitude = angleStep; longitude <= 360;
         longitude += angleStep, ++index) {
      const float longitudeRadians =
          static_cast<float>(longitude) * kDegreesToRadians;
      const idVec3 point{
          sphere.origin.x +
              idMath::Sin16(longitudeRadians) * sphere.radius * latitudeSin,
          sphere.origin.y +
              idMath::Cos16(longitudeRadians) * sphere.radius * latitudeSin,
          last.z};
      DebugLine(color, last, point, lifetime, depthTest);
      DebugLine(color, last, lastArray.buffer[index],
                lifetime, depthTest);
      lastArray.buffer[index] = last;
      last = point;
    }
  }
}

void idRenderDebugLocal::DebugSpline(const idVec4& color,
                                     idCurve_Spline<idVec3>* spline,
                                     float step, bool, int lifetime,
                                     bool depthTest) {
  if (spline == nullptr)
    return;

  if (spline->GetNumValues() > 0) {
    const float lastTime = spline->GetTime(spline->GetNumValues() - 1);
    const float totalLength = spline->GetLengthForTime(lastTime);
    idVec3 previous = spline->GetCurrentValue(0.0f);
    for (float length = 0.0f; length <= totalLength; length += step) {
      const float time = spline->GetTimeForLength(length, 0.1f);
      const idVec3 current = spline->GetCurrentValue(time);
      DebugLine(color, previous, current, lifetime, depthTest);
      previous = current;
    }

    for (int index = 0; index < spline->GetNumValues(); ++index) {
      DebugPoint(ColorVector(idColor::colorCyan), spline->GetValue(index),
                 lifetime, depthTest);
    }
  }
}

void idRenderDebugLocal::DebugLine(const idVec4& color,
                                   const idVec3& start,
                                   const idVec3& end, int lifetime,
                                   bool depthTest) {
  debugLine_t line{};
  line.rgb = color;
  line.start = start;
  line.end = end;
  line.depthTest = depthTest;
  line.lifeTime = DebugLifetime(lifetime);
  debugLines.Append(line);
}

void idRenderDebugLocal::DebugFilledBounds(const idVec4& color,
                                           const idBounds& bounds,
                                           const idVec3& origin,
                                           int lifetime, bool depthTest) {
  debugBounds_t record{};
  record.rgb = color;
  record.bounds = bounds;
  record.origin = origin;
  record.axis = mat3_identity;
  record.depthTest = depthTest;
  record.lifeTime = DebugLifetime(lifetime);
  debugBounds.Append(record);
}

void idRenderDebugLocal::DebugText(const char* text,
                                   const idVec3& origin, float scale,
                                   const idVec4& color,
                                   const idMat3& viewAxis, int align,
                                   int lifetime, bool depthTest, bool bold) {
  debugText_t record{};
  std::strncpy(record.text, text, sizeof(record.text) - 1);
  record.text[sizeof(record.text) - 1] = '\0';
  record.origin = origin;
  record.scale = scale;
  record.color = color;
  record.viewAxis = viewAxis;
  record.align = static_cast<std::uint8_t>(align);
  record.lineWidth = bold ? 4 : 1;
  record.orientToRenderViewAxis = false;
  record.depthTest = depthTest;
  record.lifeTime = DebugLifetime(lifetime);
  debugText.Append(record);
}

void idRenderDebugLocal::DebugText(const char* text,
                                   const idVec3& origin, float scale,
                                   const idVec4& color, int align,
                                   int lifetime, bool depthTest, bool bold) {
  debugText_t record{};
  std::strncpy(record.text, text, sizeof(record.text) - 1);
  record.text[sizeof(record.text) - 1] = '\0';
  record.origin = origin;
  record.scale = scale;
  record.color = color;
  record.viewAxis = mat3_identity;
  record.align = static_cast<std::uint8_t>(align);
  record.lineWidth = bold ? 4 : 1;
  record.orientToRenderViewAxis = true;
  record.depthTest = depthTest;
  record.lifeTime = DebugLifetime(lifetime);
  debugText.Append(record);
}

void idRenderDebugLocal::DebugClear(int deltaTime) {
  int kept = 0;
  for (int index = 0; index < debugLines.num; ++index) {
    debugLines.list[index].lifeTime -= deltaTime;
    if (debugLines.list[index].lifeTime >= 0) {
      if (kept != index)
        debugLines.list[kept] = debugLines.list[index];
      ++kept;
    }
  }
  debugLines.num = kept;

  kept = 0;
  for (int index = 0; index < debugPolygons.num; ++index) {
    debugPolygons.list[index].lifeTime -= deltaTime;
    if (debugPolygons.list[index].lifeTime >= 0) {
      if (kept != index)
        debugPolygons.list[kept] = debugPolygons.list[index];
      ++kept;
    }
  }
  debugPolygons.num = kept;

  kept = 0;
  for (int index = 0; index < debugBounds.num; ++index) {
    debugBounds.list[index].lifeTime -= deltaTime;
    if (debugBounds.list[index].lifeTime >= 0) {
      if (kept != index)
        debugBounds.list[kept] = debugBounds.list[index];
      ++kept;
    }
  }
  debugBounds.num = kept;

  kept = 0;
  for (int index = 0; index < debugText.num; ++index) {
    debugText.list[index].lifeTime -= deltaTime;
    if (debugText.list[index].lifeTime >= 0) {
      if (kept != index)
        debugText.list[kept] = debugText.list[index];
      ++kept;
    }
  }
  debugText.num = kept;
}

void idRenderDebugLocal::DebugFilledPolygon(const idVec4& color,
                                            const idWinding& winding,
                                            int lifetime, bool depthTest) {
  debugPolygon_t polygon{};
  polygon.rgb = color;
  polygon.winding = winding;
  polygon.depthTest = depthTest;
  polygon.lifeTime = DebugLifetime(lifetime);
  debugPolygons.Append(polygon);
}
