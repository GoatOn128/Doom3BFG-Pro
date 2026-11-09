#define WOLFTNO_RETAIL_CVAR_INLINE_FREEDATA_ABI
#define WOLFTNO_RETAIL_IDMEM_SINGLETON_ABI
#define WOLFTNO_RETAIL_IDMATH_CONSTANT_ABI
#define WOLFTNO_RETAIL_MUTABLE_IDCOLOR_ABI
#define WOLFTNO_RETAIL_STATICLIST_THROWING_CTOR_ABI
#define WOLFTNO_RETAIL_TRIVIAL_IDINDEX_ABI
#include "engine/gamelib/aas2/aas2_pathing_abi.h"

#include "shared/idlib/color.h"
#include "shared/idlib/containers/staticlist.h"
#include "shared/idlib/csystems/cvarsystemlocal.h"
#include "shared/idlib/geometry/geometry.h"
#include "shared/idlib/lib.h"
#include "shared/idlib/math/mathlib.h"

#undef WOLFTNO_RETAIL_STATICLIST_THROWING_CTOR_ABI
#undef WOLFTNO_RETAIL_IDMATH_CONSTANT_ABI
#undef WOLFTNO_RETAIL_IDMEM_SINGLETON_ABI
#undef WOLFTNO_RETAIL_MUTABLE_IDCOLOR_ABI
#undef WOLFTNO_RETAIL_TRIVIAL_IDINDEX_ABI
#undef WOLFTNO_RETAIL_CVAR_INLINE_FREEDATA_ABI

#include <algorithm>
#include <cmath>
#include <cstring>
#include <xmmintrin.h>

class idRenderDebug {
public:
#define ID_RENDER_DEBUG_PATHING_SLOT(slot) \
  virtual int PathingRenderSlot##slot() = 0
  ID_RENDER_DEBUG_PATHING_SLOT(00);
  ID_RENDER_DEBUG_PATHING_SLOT(01);
  ID_RENDER_DEBUG_PATHING_SLOT(02);
  virtual void DebugArrow(const idVec4& color, const idVec3& start,
                          const idVec3& end, float size, int lifetime,
                          bool depthTest) = 0;
  ID_RENDER_DEBUG_PATHING_SLOT(04);
  ID_RENDER_DEBUG_PATHING_SLOT(05);
  ID_RENDER_DEBUG_PATHING_SLOT(06);
  ID_RENDER_DEBUG_PATHING_SLOT(07);
  ID_RENDER_DEBUG_PATHING_SLOT(08);
  ID_RENDER_DEBUG_PATHING_SLOT(09);
  ID_RENDER_DEBUG_PATHING_SLOT(10);
  ID_RENDER_DEBUG_PATHING_SLOT(11);
  ID_RENDER_DEBUG_PATHING_SLOT(12);
  ID_RENDER_DEBUG_PATHING_SLOT(13);
  ID_RENDER_DEBUG_PATHING_SLOT(14);
  ID_RENDER_DEBUG_PATHING_SLOT(15);
  ID_RENDER_DEBUG_PATHING_SLOT(16);
  ID_RENDER_DEBUG_PATHING_SLOT(17);
  ID_RENDER_DEBUG_PATHING_SLOT(18);
  ID_RENDER_DEBUG_PATHING_SLOT(19);
  ID_RENDER_DEBUG_PATHING_SLOT(20);
  virtual void DebugPoint(const idVec4& color, const idVec3& point,
                          int lifetime, bool depthTest) = 0;
#undef ID_RENDER_DEBUG_PATHING_SLOT
};

idRenderDebug* GetGameLibRenderDebug();

idCVar aas_debugTraversals(
    "aas_debugTraversals", "0", CVAR_INTEGER,
    "if > 0, traversal debug info will be shown for value * 1000 milliseconds",
    static_cast<argCompletionRef_t>(nullptr));
idCVar aas_simplifyPath(
    "aas_simplifyPath", "0", CVAR_INTEGER,
    "1 = simplify the AAS2 route returning it",
    static_cast<argCompletionRef_t>(nullptr));

namespace {

__forceinline idVec3 Dequantize(const idQuantizedVec3& value) {
  return {static_cast<float>(value.x), static_cast<float>(value.y),
          static_cast<float>(value.z)};
}

__forceinline float LengthSquared(const idVec3& value) {
  return ((value.x * value.x) + (value.y * value.y)) +
         (value.z * value.z);
}

__forceinline float ClampedSqrt(float value) {
  return _mm_cvtss_f32(
      _mm_sqrt_ps(_mm_max_ps(_mm_set_ss(value),
                             idMath::vector_float_smallestNonDenorm)));
}

__forceinline float RetailMaximum(float first, float second) {
  return _mm_cvtss_f32(_mm_max_ss(_mm_set_ss(first), _mm_set_ss(second)));
}

__forceinline bool IsWalkReachability(const aas2Reachability_t& reachability,
                                      int walkTravelFlags) {
  return (reachability.travelFlags & walkTravelFlags) != 0 &&
         (reachability.travelFlags & ~walkTravelFlags) == 0;
}

__forceinline void ClearTrace(aas2TraceFloor_t& trace) {
  std::memset(&trace, 0, 76);
  trace.areas = nullptr;
  trace.reachIndices = nullptr;
}

} // namespace

int idAAS2Local::GetTraversalsForReachability(
    idAAS2ReachabilityIndex reachIndex,
    idList<int, 5>& traversalList) const {
  return file->GetTraversalsForReachability(reachIndex, traversalList);
}

int idAAS2Local::GetTraversalsForInteractableEntity(
    idAAS2InteractionEntIndex interactionEntIndex,
    idList<int, 5>& traversalList) const {
  return file->GetTraversalsForInteractableEntity(interactionEntIndex,
                                                   traversalList);
}

bool idAAS2Local::WalkPathIsValid(
    int startAreaNum, const idVec3& startOrigin, int goalAreaNum,
    const idVec3& goalOrigin, int travelFlags, int& endAreaNum,
    aas2EdgeCrossed_t& firstEdgeCrossed) const {
  aas2TraceFloor_t trace;
  ClearTrace(trace);
  file->TraceFloor(trace, startOrigin, startAreaNum, goalOrigin, goalAreaNum,
                   travelFlags, false, false, true);
  endAreaNum = trace.lastEdge.toAreaNum;
  if (trace.lastEdge.toAreaNum != goalAreaNum) {
    return false;
  }
  firstEdgeCrossed = trace.firstEdge;
  return true;
}

void idAAS2Local::SubSampleWalkPath(
    int startAreaNum, const idVec3& startOrigin, int pathAreaNum,
    const idVec3& pathStart, const idVec3& pathEnd, int travelFlags,
    idVec3& endPos, int& endAreaNum,
    aas2EdgeCrossed_t& firstEdgeCrossed) const {
  float step = 0.25f;
  float fraction = 0.5f;
  idVec3 pathPoint = pathStart;
  idVec3 acceptedPoint = pathPoint;
  int acceptedAreaNum = pathAreaNum;
  const float deltaX = pathEnd.x - pathStart.x;
  const float deltaY = pathEnd.y - pathStart.y;
  const float deltaZ = pathEnd.z - pathStart.z;
  const float pathLengthSquared =
      ((deltaX * deltaX) + (deltaY * deltaY)) + (deltaZ * deltaZ);

  for (int iteration = 0;
       iteration < 8 && step * pathLengthSquared > 16.0f;
       ++iteration, step *= 0.5f) {
    pathPoint.x = deltaX * fraction + pathStart.x;
    pathPoint.y = deltaY * fraction + pathStart.y;
    pathPoint.z = deltaZ * fraction + pathStart.z;

    aas2TraceFloor_t trace;
    ClearTrace(trace);
    file->TraceFloor(trace, startOrigin, startAreaNum, pathPoint, pathAreaNum,
                     travelFlags, false, false, true);
    if (trace.lastEdge.toAreaNum == pathAreaNum) {
      acceptedAreaNum = trace.lastEdge.toAreaNum;
      acceptedPoint = pathPoint;
      firstEdgeCrossed = trace.firstEdge;
      fraction += step;
    } else {
      fraction -= step;
    }
  }

  endPos = acceptedPoint;
  endAreaNum = acceptedAreaNum;
}

bool idAAS2Local::WalkPathToGoal(
    idAAS2Path& path, int startAreaNum, const idVec3& startOrigin,
    int goalAreaNum, const idVec3& goalOrigin, int travelFlags,
    int walkTravelFlags) const {
  path.type = 0;
  path.moveGoal = startOrigin;
  path.moveAreaNum = startAreaNum;
  path.reachIndex.value = -1;
  path.travelTime = 0;
  path.firstEdge.toAreaNum = startAreaNum;
  path.firstEdge.edgeNum = startAreaNum;
  path.firstEdge.edgePoint = startOrigin;
  path.traversalIndex = 0;
  path.traversalStart = vec3_origin;
  path.traversalEnd = vec3_origin;
  path.obstacleGoal = startOrigin;
  path.obstacleAreaNum = startAreaNum;
  path.obstacleRoute.endAlignDir = vec3_origin;
  path.obstacleRoute.endTurnRadius = 0.0f;
  path.obstacleRoute.numAreas = 1;
  path.obstacleRoute.areas[0].areaNum = startAreaNum;
  path.obstacleRoute.areas[0].start = startOrigin;
  path.obstacleRoute.areas[0].end = goalOrigin;
  path.pathMaxHeight = RetailMaximum(startOrigin.z, goalOrigin.z);

  if (file == nullptr) {
    path.moveGoal = goalOrigin;
    path.moveAreaNum = goalAreaNum;
    path.obstacleGoal = goalOrigin;
    path.obstacleAreaNum = goalAreaNum;
    return true;
  }
  if (startAreaNum == goalAreaNum) {
    path.moveGoal = goalOrigin;
    path.obstacleGoal = goalOrigin;
    path.travelTime = AreaTravelTime(startAreaNum, startOrigin, goalOrigin);
    return true;
  }

  idAAS2ReachabilityIndex reachabilityIndices[10];
  for (idAAS2ReachabilityIndex& index : reachabilityIndices) {
    index.value = -1;
  }
  int reachabilityCount = 0;
  idAreaTrail areaTrail = {
      {startAreaNum, startAreaNum, startAreaNum, startAreaNum}, 0};
  idVec3 currentOrigin = startOrigin;
  int currentAreaNum = startAreaNum;
  path.obstacleRoute.areas[0].areaNum = startAreaNum;
  path.obstacleRoute.areas[0].start = startOrigin;
  path.obstacleRoute.areas[0].end = startOrigin;
  path.obstacleRoute.numAreas = 0;
  int routeIndex = 0;
  idAAS2ReachabilityIndex lastReachabilityIndex;
  lastReachabilityIndex.value = -1;
  const aas2Reachability_t* lastReachability = nullptr;

  for (;;) {
    int routeTravelTime;
    if (!RouteToGoalArea(currentAreaNum, currentOrigin, goalAreaNum,
                         goalOrigin, travelFlags, routeTravelTime,
                         lastReachabilityIndex)) {
      return false;
    }
    if (lastReachabilityIndex.value < 0) {
      return false;
    }

    lastReachability =
        &file->reachabilities.list[lastReachabilityIndex.value];
    path.obstacleRoute.areas[routeIndex].end =
        Dequantize(lastReachability->start);
    path.obstacleRoute.areas[routeIndex + 1].areaNum =
        lastReachability->toAreaNum;
    path.obstacleRoute.areas[routeIndex + 1].start =
        Dequantize(lastReachability->end);
    ++path.obstacleRoute.numAreas;
    const float reachabilityMaxHeight = RetailMaximum(
        static_cast<float>(lastReachability->start.z),
        static_cast<float>(lastReachability->end.z));
    path.pathMaxHeight =
        RetailMaximum(path.pathMaxHeight, reachabilityMaxHeight);
    if (routeIndex == 0) {
      path.travelTime = routeTravelTime;
    }

    if (!IsWalkReachability(*lastReachability, walkTravelFlags)) {
      break;
    }

    reachabilityIndices[reachabilityCount++].value =
        lastReachabilityIndex.value;
    currentAreaNum = lastReachability->toAreaNum;
    if (currentAreaNum == goalAreaNum) {
      path.obstacleRoute.areas[routeIndex + 1].end = goalOrigin;
      ++path.obstacleRoute.numAreas;
      break;
    }
    currentOrigin = Dequantize(lastReachability->end);

    bool localMinimum = false;
    for (int trailIndex = 0; trailIndex < 4; ++trailIndex) {
      if (currentAreaNum == areaTrail.areaTrail[trailIndex]) {
        idLib::Warning(
            "idAAS2Local::WalkPathToGoal: local routing minimum at area %d "
            "while going from area %d to area %d",
            currentAreaNum, startAreaNum, goalAreaNum);
        localMinimum = true;
        break;
      }
    }
    if (localMinimum) {
      break;
    }

    areaTrail.areaTrail[areaTrail.trailIndex] = currentAreaNum;
    areaTrail.trailIndex = (areaTrail.trailIndex + 1) & 3;
    ++routeIndex;
    if (routeIndex >= 10) {
      break;
    }
  }

  if (!IsWalkReachability(*lastReachability, walkTravelFlags)) {
    const idVec3 reachStart = Dequantize(lastReachability->start);
    aas2TraceFloor_t trace;
    ClearTrace(trace);
    file->TraceFloor(trace, startOrigin, startAreaNum, reachStart,
                     lastReachability->fromAreaNum, walkTravelFlags, false,
                     false, true);
    if (trace.lastEdge.toAreaNum != lastReachability->fromAreaNum) {
      goto walk_fallback;
    }

    path.firstEdge = trace.firstEdge;
    path.moveGoal = reachStart;
    path.moveAreaNum = lastReachability->fromAreaNum;
    switch (lastReachability->travelFlags) {
      case 64:
        path.type = 1;
        path.reachIndex = lastReachabilityIndex;
        path.moveGoal = Dequantize(lastReachability->end);
        path.moveAreaNum = lastReachability->toAreaNum;
        break;
      case 128:
        path.type = 2;
        path.reachIndex = lastReachabilityIndex;
        path.moveGoal = Dequantize(lastReachability->end);
        path.moveAreaNum = lastReachability->toAreaNum;
        break;
      case 256:
        path.type = 3;
        path.reachIndex = lastReachabilityIndex;
        break;
      case 512:
        path.type = 4;
        path.reachIndex = lastReachabilityIndex;
        break;
      case 1024:
        path.type = 5;
        path.reachIndex = lastReachabilityIndex;
        path.moveGoal = Dequantize(lastReachability->end);
        path.moveAreaNum = lastReachability->toAreaNum;
        break;
      case 4096:
        path.type = 6;
        path.reachIndex = lastReachabilityIndex;
        break;
      case 0x2000:
        path.type = 7;
        path.reachIndex = lastReachabilityIndex;
        break;
      case 0x200000:
        path.type = 8;
        path.reachIndex = lastReachabilityIndex;
        path.moveAreaNum = lastReachability->toAreaNum;
        break;
      default:
        if ((lastReachability->travelFlags & 0x1870000) != 0) {
          path.type = 9;
          path.reachIndex = lastReachabilityIndex;
          path.traversalIndex = FindBestTraversalForReachability(
              lastReachabilityIndex, startOrigin, goalOrigin,
              path.traversalStart, path.traversalEnd, path.dependencyIndex,
              travelFlags, true);
          if (path.traversalIndex > 0) {
            path.moveGoal = path.traversalStart;
            path.obstacleRoute.areas[path.obstacleRoute.numAreas - 1].end =
                path.traversalStart;
          }
        }
        break;
    }
    path.obstacleGoal = path.moveGoal;
    path.obstacleAreaNum = path.moveAreaNum;
    return true;
  }

  if (lastReachability->toAreaNum == goalAreaNum) {
    int endAreaNum;
    if (WalkPathIsValid(startAreaNum, startOrigin,
                        lastReachability->toAreaNum, goalOrigin,
                        walkTravelFlags, endAreaNum, path.firstEdge)) {
      path.moveGoal = goalOrigin;
      path.moveAreaNum = goalAreaNum;
      path.obstacleGoal = goalOrigin;
      path.obstacleAreaNum = goalAreaNum;
      return true;
    }
  }

walk_fallback:
  path.moveGoal = path.obstacleRoute.areas[0].end;
  path.moveAreaNum = path.obstacleRoute.areas[0].areaNum;
  path.obstacleGoal = path.obstacleRoute.areas[0].end;
  path.obstacleAreaNum = path.obstacleRoute.areas[0].areaNum;

  if (aas_simplifyPath.valueInteger != 0) {
    int foundIndex = reachabilityCount - 1;
    const aas2Reachability_t* foundReachability = nullptr;
    idVec3 foundReachabilityEnd{};
    aas2TraceFloor_t trace{};
    for (; foundIndex >= 0; --foundIndex) {
      foundReachability =
          &file->reachabilities.list[reachabilityIndices[foundIndex].value];
      foundReachabilityEnd = Dequantize(foundReachability->end);
      if ((travelFlags & 0x200000) != 0) {
        foundReachabilityEnd.z =
            RetailMaximum(startOrigin.z, foundReachabilityEnd.z);
      }
      ClearTrace(trace);
      file->TraceFloor(trace, startOrigin, startAreaNum,
                       foundReachabilityEnd, foundReachability->toAreaNum,
                       walkTravelFlags, false, false, true);
      if (trace.lastEdge.toAreaNum == foundReachability->toAreaNum) {
        break;
      }
    }

    if (foundIndex >= 0) {
      path.firstEdge = trace.firstEdge;
      if (foundIndex >= reachabilityCount - 1) {
        if (!IsWalkReachability(*lastReachability, walkTravelFlags)) {
          const idVec3 nextReachStart =
              Dequantize(lastReachability->start);
          SubSampleWalkPath(
              startAreaNum, startOrigin, foundReachability->toAreaNum,
              foundReachabilityEnd, nextReachStart, walkTravelFlags,
              path.moveGoal, path.moveAreaNum, path.firstEdge);
          path.obstacleGoal = nextReachStart;
          path.moveAreaNum = foundReachability->toAreaNum;
        } else if (lastReachability->toAreaNum == goalAreaNum) {
          SubSampleWalkPath(
              startAreaNum, startOrigin, foundReachability->toAreaNum,
              foundReachabilityEnd, goalOrigin, walkTravelFlags,
              path.moveGoal, path.moveAreaNum, path.firstEdge);
          path.obstacleGoal = goalOrigin;
          path.moveAreaNum = goalAreaNum;
        } else {
          path.moveGoal = foundReachabilityEnd;
          path.moveAreaNum = foundReachability->toAreaNum;
          path.obstacleGoal = foundReachabilityEnd;
          path.obstacleAreaNum = foundReachability->toAreaNum;
        }
      } else {
        const aas2Reachability_t& nextReachability =
            file->reachabilities
                .list[reachabilityIndices[foundIndex + 1].value];
        const idVec3 nextReachStart = Dequantize(nextReachability.start);
        SubSampleWalkPath(
            startAreaNum, startOrigin, foundReachability->toAreaNum,
            foundReachabilityEnd, nextReachStart, walkTravelFlags,
            path.moveGoal, path.moveAreaNum, path.firstEdge);
        path.obstacleGoal = nextReachStart;
        path.moveAreaNum = foundReachability->toAreaNum;
      }
    }
  }

  const float radius = file->obstaclePVSRadius * 0.75f;
  const idVec3 moveDelta = {
      path.moveGoal.x - startOrigin.x, path.moveGoal.y - startOrigin.y,
      path.moveGoal.z - startOrigin.z};
  const idVec3 obstacleDelta = {
      path.obstacleGoal.x - startOrigin.x,
      path.obstacleGoal.y - startOrigin.y,
      path.obstacleGoal.z - startOrigin.z};
  const float moveDistanceSquared = LengthSquared(moveDelta);
  const float obstacleDistanceSquared = LengthSquared(obstacleDelta);
  if (moveDistanceSquared <= radius * radius) {
    if (obstacleDistanceSquared > radius * radius) {
      const float fraction =
          (radius - ClampedSqrt(moveDistanceSquared)) /
          ClampedSqrt(obstacleDistanceSquared);
      path.obstacleGoal.x =
          (path.obstacleGoal.x - path.moveGoal.x) * fraction +
          path.moveGoal.x;
      path.obstacleGoal.y =
          (path.obstacleGoal.y - path.moveGoal.y) * fraction +
          path.moveGoal.y;
      path.obstacleGoal.z =
          (path.obstacleGoal.z - path.moveGoal.z) * fraction +
          path.moveGoal.z;
    }
  } else {
    path.obstacleGoal = path.moveGoal;
    path.obstacleAreaNum = path.moveAreaNum;
  }
  return true;
}

bool idAAS2Local::HopPathIsValid(
    int startAreaNum, const idVec3& startOrigin, int goalAreaNum,
    const idVec3& goalOrigin, const idAAS2HopPathParms& parms) const {
  if ((file->areas.list[goalAreaNum].flags & 4) == 0) {
    return false;
  }

  const idAAS2TreeIndex tree = file->GetTreeForArea(startAreaNum);
  const idVec3& floorNormal = file->trees.list[tree.value].floorNormal;
  const idVec3 delta = {
      goalOrigin.x - startOrigin.x, goalOrigin.y - startOrigin.y,
      goalOrigin.z - startOrigin.z};
  const float height = ((floorNormal.y * delta.y) +
                        (floorNormal.x * delta.x)) +
                       (floorNormal.z * delta.z);
  const idVec3 planarDelta = {
      delta.x - floorNormal.x * height,
      delta.y - floorNormal.y * height,
      delta.z - floorNormal.z * height,
  };
  if (LengthSquared(planarDelta) > parms.maxDistance * parms.maxDistance) {
    return false;
  }

  idVec3 points[512];
  aas2TraceHeight_t trace;
  trace.maxPoints = 512;
  trace.points = points;
  file->TraceHeight(tree, trace, startOrigin, goalOrigin);
  if (trace.numPoints >= 512) {
    return false;
  }
  if (trace.numPoints <= 1) {
    return true;
  }

  const float startHeight =
      ((startOrigin.y * floorNormal.y) +
       (floorNormal.x * startOrigin.x)) +
      (startOrigin.z * floorNormal.z);
  for (int pointIndex = 1; pointIndex < trace.numPoints; ++pointIndex) {
    const idVec3 pointDelta = {
        trace.points[pointIndex].x - startOrigin.x,
        trace.points[pointIndex].y - startOrigin.y,
        trace.points[pointIndex].z - startOrigin.z};
    const float pointHeight =
        ((floorNormal.y * pointDelta.y) +
         (floorNormal.x * pointDelta.x)) +
        (floorNormal.z * pointDelta.z);
    const idVec3 planar = {
        pointDelta.x - floorNormal.x * pointHeight,
        pointDelta.y - floorNormal.y * pointHeight,
        pointDelta.z - floorNormal.z * pointHeight,
    };
    float maxHeight = parms.maxSlope * ClampedSqrt(LengthSquared(planar));
    if (parms.minHeight <= maxHeight) {
      if (maxHeight > parms.maxHeight) {
        maxHeight = parms.maxHeight;
      }
    } else {
      maxHeight = parms.minHeight;
    }
    const float absoluteHeight =
        ((trace.points[pointIndex].y * floorNormal.y) +
         (trace.points[pointIndex].x * floorNormal.x)) +
        (trace.points[pointIndex].z * floorNormal.z);
    if (absoluteHeight - startHeight > maxHeight) {
      return false;
    }
  }
  return true;
}

bool idAAS2Local::ExtendHopPathToGoal(
    idAAS2Path& path, int startAreaNum, const idVec3& startOrigin,
    int goalAreaNum, const idVec3& goalOrigin, int travelFlags,
    int /*walkTravelFlags*/, const idAAS2HopPathParms& parms) const {
  if ((file->areas.list[startAreaNum].flags & 4) == 0) {
    return false;
  }

  if (path.moveGoal.x == goalOrigin.x &&
      path.moveGoal.y == goalOrigin.y &&
      path.moveGoal.z == goalOrigin.z) {
    return HopPathIsValid(startAreaNum, startOrigin, path.moveAreaNum,
                          path.moveGoal, parms);
  }
  if ((file->areas.list[goalAreaNum].flags & 4) != 0 &&
      HopPathIsValid(startAreaNum, startOrigin, goalAreaNum, goalOrigin,
                     parms)) {
    path.moveGoal = goalOrigin;
    path.moveAreaNum = goalAreaNum;
    return true;
  }

  idAreaTrail areaTrail;
  areaTrail.areaTrail[0] = path.moveAreaNum;
  areaTrail.areaTrail[1] = path.moveAreaNum;
  areaTrail.areaTrail[2] = path.moveAreaNum;
  areaTrail.areaTrail[3] = path.moveAreaNum;
  areaTrail.trailIndex = 0;
  bool isValid = HopPathIsValid(startAreaNum, startOrigin, path.moveAreaNum,
                                path.moveGoal, parms);
  int routeCount = 0;
  idAAS2ReachabilityIndex reachIndex;
  reachIndex.value = -1;

  for (;;) {
    int travelTime;
    if (!RouteToGoalArea(path.moveAreaNum, path.moveGoal, goalAreaNum,
                         goalOrigin, travelFlags, travelTime, reachIndex) ||
        reachIndex.value < 0) {
      return false;
    }

    const aas2Reachability_t& reachability =
        file->reachabilities.list[reachIndex.value];
    const idVec3 reachEnd = Dequantize(reachability.end);
    if (!HopPathIsValid(startAreaNum, startOrigin, reachability.toAreaNum,
                        reachEnd, parms)) {
      return isValid;
    }
    isValid = true;
    path.moveGoal = reachEnd;
    path.moveAreaNum = reachability.toAreaNum;
    if (reachability.toAreaNum == goalAreaNum) {
      return true;
    }

    for (int trailIndex = 0; trailIndex < 4; ++trailIndex) {
      if (reachability.toAreaNum == areaTrail.areaTrail[trailIndex]) {
        idLib::Warning(
            "idAAS2Local::HopPathToGoal: local routing minimum at area %d "
            "while going from area %d to area %d",
            reachability.toAreaNum, startAreaNum, goalAreaNum);
        return true;
      }
    }
    ++routeCount;
    areaTrail.areaTrail[areaTrail.trailIndex] = reachability.toAreaNum;
    areaTrail.trailIndex = (areaTrail.trailIndex + 1) & 3;
    if (routeCount >= 11) {
      return isValid;
    }
  }
}

bool idAAS2Local::GetPath(const idVec3& startPos, const idVec3& goalPos,
                          int startAreaNum, int goalAreaNum, int travelFlags,
                          idAAS2PathPoint* pointList, int maxPoints,
                          int& numPoints) const {
  if (pointList == nullptr || maxPoints <= 0) {
    return false;
  }

  pointList[0].areaNum = startAreaNum;
  pointList[0].nextReachIndex.value = -1;
  pointList[0].travelTime = 0;
  pointList[0].origin = startPos;
  if (file == nullptr || startAreaNum <= 0 || goalAreaNum <= 0) {
    return false;
  }

  int accumulatedTravelTime = 0;
  numPoints = 1;
  while (numPoints < maxPoints) {
    idAAS2PathPoint& current = pointList[numPoints - 1];
    int travelTime = 0;
    if (!RouteToGoalArea(current.areaNum, current.origin, goalAreaNum,
                         goalPos, travelFlags, travelTime,
                         current.nextReachIndex) ||
        current.nextReachIndex.value < 0) {
      break;
    }

    accumulatedTravelTime += travelTime;
    const aas2Reachability_t& reachability =
        file->reachabilities.list[current.nextReachIndex.value];
    idAAS2PathPoint& next = pointList[numPoints++];
    next.areaNum = reachability.toAreaNum;
    next.origin = Dequantize(reachability.start);
    next.nextReachIndex.value = -1;
    next.travelTime = accumulatedTravelTime;
    if (next.areaNum == goalAreaNum) {
      if (numPoints < maxPoints) {
        idAAS2PathPoint& goal = pointList[numPoints++];
        goal.areaNum = goalAreaNum;
        goal.origin = goalPos;
        goal.nextReachIndex.value = -1;
        goal.travelTime = accumulatedTravelTime;
      }
      return true;
    }
  }
  return false;
}

bool idAAS2Local::GetOptimizedWalkPath(
    int startAreaNum, const idVec3& startOrigin, int goalAreaNum,
    const idVec3& goalOrigin, int travelFlags, int walkTravelFlags,
    idList<idVec3, 5>& path) const {
  if (file == nullptr) {
    return false;
  }

  int currentAreaNum = startAreaNum;
  idVec3 currentOrigin = startOrigin;
  if (currentAreaNum == goalAreaNum) {
    path.Append(goalOrigin);
    return true;
  }

  for (;;) {
    idAAS2Path walkPath;
    std::memset(&walkPath, 0, sizeof(walkPath));
    walkPath.type = 10;
    walkPath.reachIndex.value = -1;
    walkPath.travelTime = -1;
    walkPath.traversalIndex = -1;
    walkPath.dependencyIndex.value = -1;
    if (!WalkPathToGoal(walkPath, currentAreaNum, currentOrigin, goalAreaNum,
                        goalOrigin, travelFlags, walkTravelFlags)) {
      return false;
    }
    path.Append(currentOrigin);

    if (walkPath.reachIndex.value >= 0) {
      const aas2Reachability_t& reachability =
          file->reachabilities.list[walkPath.reachIndex.value];
      currentOrigin = Dequantize(reachability.end);
      currentAreaNum = reachability.toAreaNum;
    } else if (walkPath.firstEdge.edgeNum != 0 &&
               LengthSquared(
                   {walkPath.firstEdge.edgePoint.x - currentOrigin.x,
                    walkPath.firstEdge.edgePoint.y - currentOrigin.y,
                    walkPath.firstEdge.edgePoint.z - currentOrigin.z}) >
                   0.010000001f) {
      currentAreaNum = walkPath.firstEdge.toAreaNum;
      idVec3 nextOrigin = walkPath.firstEdge.edgePoint;
      file->PushPointIntoAreaNum(currentAreaNum, nextOrigin);
      currentOrigin = nextOrigin;
    } else {
      currentOrigin = walkPath.moveGoal;
      currentAreaNum = walkPath.moveAreaNum;
    }

    if (currentAreaNum == goalAreaNum) {
      path.Append(goalOrigin);
      return true;
    }
  }
}

int idAAS2Local::FindBestTraversalForReachability(
    idAAS2ReachabilityIndex reachIndex, const idVec3& curPoint,
    const idVec3& goalPoint, idVec3& outTraversalStart,
    idVec3& outTraversalEnd, idAAS2DependencyIndex& dependencyIndex,
    int travelFlags, bool useClosestPointOnFail) const {
  idStaticList<int, 16> traversalList;
  const int numTraversals =
      file->GetTraversalsForReachability(reachIndex, traversalList);
  int bestTraversalIndex = 0;
  const aas2Traversal_t* bestTraversal = nullptr;
  outTraversalStart = vec3_origin;
  outTraversalEnd = vec3_origin;
  if (numTraversals <= 0) {
    return 0;
  }

  float bestDistance = 0.0f;
  for (int listIndex = 0; listIndex < numTraversals; ++listIndex) {
    const int traversalIndex = traversalList[listIndex];
    const aas2Traversal_t& traversal =
        file->traversalPoints.list[traversalIndex];
    const idVec3& up = GetFloorNormalForArea(traversal.startAreaNum);

    if (aas_debugTraversals.valueInteger != 0) {
      const int lifetime = 1000 * aas_debugTraversals.valueInteger - 1000;
      idVec3 extrusionStart;
      traversal.CalcExtrusionPoint(extrusionStart, up);
      const idVec3 extrusionEnd = {
          (extrusionStart.x + traversal.endPoint.x) - traversal.startPoint.x,
          (extrusionStart.y + traversal.endPoint.y) - traversal.startPoint.y,
          (extrusionStart.z + traversal.endPoint.z) - traversal.startPoint.z,
      };
      const idVec4& grey =
          reinterpret_cast<const idVec4&>(idColor::colorMdGrey);
      GetGameLibRenderDebug()->DebugPoint(
          grey, traversal.startPoint, lifetime, false);
      GetGameLibRenderDebug()->DebugPoint(
          grey, traversal.endPoint, lifetime, false);
      GetGameLibRenderDebug()->DebugPoint(
          grey, extrusionStart, lifetime, false);
      GetGameLibRenderDebug()->DebugPoint(
          grey, extrusionEnd, lifetime, false);
      GetGameLibRenderDebug()->DebugArrow(
          grey, traversal.startPoint, traversal.endPoint, 4.0f, lifetime,
          false);
      GetGameLibRenderDebug()->DebugArrow(
          grey, traversal.startPoint, extrusionStart, 4.0f, lifetime, false);
      GetGameLibRenderDebug()->DebugArrow(
          grey, extrusionStart, extrusionEnd, 4.0f, lifetime, false);
      GetGameLibRenderDebug()->DebugArrow(
          grey, traversal.endPoint, extrusionEnd, 4.0f, lifetime, false);
    }

    const unsigned int flags = static_cast<unsigned int>(traversal.flags);
    const bool matchesTravelType =
        ((flags & 0x02000000u) != 0 &&
         (travelFlags & 0x00010000) != 0) ||
        ((flags & 0x04000000u) != 0 &&
         (travelFlags & 0x00020000) != 0) ||
        ((flags & 0x08000000u) != 0 &&
         (travelFlags & 0x00040000) != 0) ||
        ((flags & 0x10000000u) != 0 &&
         (travelFlags & 0x00800000) != 0) ||
        ((flags & 0x20000000u) != 0 &&
         (travelFlags & 0x01000000) != 0);
    if ((flags & 1u) == 0 || !matchesTravelType) {
      continue;
    }

    idVec3 traversalStart = traversal.startPoint;
    if (traversal.extrusionDistance > 0) {
      idVec3 extrusionPoint;
      traversal.CalcExtrusionPoint(extrusionPoint, up);
      const idVec3& floorNormal =
          GetFloorNormalForArea(traversal.startAreaNum);
      const idMat3 toZAxis = floorNormal.ToZAxis();

      const idVec2 currentProjection = {
          ((curPoint.x * toZAxis.mat[0].x) +
           (curPoint.y * toZAxis.mat[0].y)) +
              (curPoint.z * toZAxis.mat[0].z),
          ((curPoint.x * toZAxis.mat[1].x) +
           (curPoint.y * toZAxis.mat[1].y)) +
              (curPoint.z * toZAxis.mat[1].z),
      };
      const idVec2 goalProjection = {
          ((goalPoint.x * toZAxis.mat[0].x) +
           (goalPoint.y * toZAxis.mat[0].y)) +
              (goalPoint.z * toZAxis.mat[0].z),
          ((goalPoint.x * toZAxis.mat[1].x) +
           (goalPoint.y * toZAxis.mat[1].y)) +
              (goalPoint.z * toZAxis.mat[1].z),
      };
      const float rayX = goalProjection.x - currentProjection.x;
      const float rayY = goalProjection.y - currentProjection.y;
      const float inverseRayLength =
          1.0f / ClampedSqrt((rayX * rayX) + (rayY * rayY));
      const idVec2 rayDirection = {inverseRayLength * rayX,
                                   inverseRayLength * rayY};
      const idVec2 segmentStart = {
          ((traversal.startPoint.x * toZAxis.mat[0].x) +
           (traversal.startPoint.y * toZAxis.mat[0].y)) +
              (traversal.startPoint.z * toZAxis.mat[0].z),
          ((traversal.startPoint.x * toZAxis.mat[1].x) +
           (traversal.startPoint.y * toZAxis.mat[1].y)) +
              (traversal.startPoint.z * toZAxis.mat[1].z),
      };
      const idVec2 segmentEnd = {
          ((extrusionPoint.x * toZAxis.mat[0].x) +
           (extrusionPoint.y * toZAxis.mat[0].y)) +
              (extrusionPoint.z * toZAxis.mat[0].z),
          ((extrusionPoint.x * toZAxis.mat[1].x) +
           (extrusionPoint.y * toZAxis.mat[1].y)) +
              (extrusionPoint.z * toZAxis.mat[1].z),
      };

      float fraction;
      if (idGeometry::RayIntersectsLineSegment2D(
              currentProjection, rayDirection, segmentStart, segmentEnd,
              fraction)) {
        const float projectedX =
            rayDirection.x * fraction + currentProjection.x;
        const float projectedY =
            currentProjection.y + rayDirection.y * fraction;
        const float floorHeight =
            ((floorNormal.x * traversal.startPoint.x) +
             (floorNormal.y * traversal.startPoint.y)) +
            (floorNormal.z * traversal.startPoint.z);
        traversalStart.x =
            (((toZAxis.mat[0].x * projectedX) +
              (toZAxis.mat[1].x * projectedY)) +
             (toZAxis.mat[2].x * 0.0f)) +
            (floorNormal.x * floorHeight);
        traversalStart.y =
            (((toZAxis.mat[0].y * projectedX) +
              (toZAxis.mat[1].y * projectedY)) +
             (toZAxis.mat[2].y * 0.0f)) +
            (floorNormal.y * floorHeight);
        traversalStart.z =
            (((toZAxis.mat[0].z * projectedX) +
              (toZAxis.mat[1].z * projectedY)) +
             (toZAxis.mat[2].z * 0.0f)) +
            (floorNormal.z * floorHeight);
      } else if (useClosestPointOnFail) {
        const float closest = idGeometry::ClosestPositionOnLineSegment(
            curPoint, traversal.startPoint, extrusionPoint);
        traversalStart.x =
            (extrusionPoint.x - traversal.startPoint.x) * closest +
            traversal.startPoint.x;
        traversalStart.y =
            traversal.startPoint.y +
            (extrusionPoint.y - traversal.startPoint.y) * closest;
        traversalStart.z =
            traversal.startPoint.z +
            (extrusionPoint.z - traversal.startPoint.z) * closest;
      }
    }

    const float deltaX = traversalStart.x - curPoint.x;
    const float deltaY = traversalStart.y - curPoint.y;
    const float deltaZ = traversalStart.z - curPoint.z;
    const float distance = ClampedSqrt(
        ((deltaX * deltaX) + (deltaY * deltaY)) + (deltaZ * deltaZ));
    if (bestTraversalIndex <= 0 || bestDistance > distance) {
      bestDistance = distance;
      bestTraversalIndex = traversalIndex;
      bestTraversal = &traversal;
      outTraversalStart = traversalStart;
      outTraversalEnd.x =
          (traversalStart.x + traversal.endPoint.x) -
          traversal.startPoint.x;
      outTraversalEnd.y =
          (traversalStart.y + traversal.endPoint.y) -
          traversal.startPoint.y;
      outTraversalEnd.z =
          (traversalStart.z + traversal.endPoint.z) -
          traversal.startPoint.z;
      dependencyIndex = traversal.dependencyIndex;
    }
  }

  if (aas_debugTraversals.valueInteger != 0 && bestTraversal != nullptr) {
    const idVec4& green =
        reinterpret_cast<const idVec4&>(idColor::colorGreen);
    GetGameLibRenderDebug()->DebugPoint(
        green, bestTraversal->startPoint, 0, false);
    GetGameLibRenderDebug()->DebugPoint(
        green, bestTraversal->endPoint, 0, false);
    GetGameLibRenderDebug()->DebugArrow(
        green, bestTraversal->startPoint, bestTraversal->endPoint, 2.0f, 0,
        false);
    const idVec4& purple =
        reinterpret_cast<const idVec4&>(idColor::colorPurple);
    GetGameLibRenderDebug()->DebugPoint(
        purple, outTraversalStart, 0, false);
    GetGameLibRenderDebug()->DebugPoint(
        purple, outTraversalEnd, 0, false);
    GetGameLibRenderDebug()->DebugArrow(
        purple, outTraversalStart, outTraversalEnd, 2.0f, 0, false);
  }
  return bestTraversalIndex;
}
