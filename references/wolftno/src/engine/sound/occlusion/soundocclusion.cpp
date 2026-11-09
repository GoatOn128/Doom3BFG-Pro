#include "engine/sound/occlusion/soundocclusion.h"

#include <algorithm>
#include <cmath>
#include <limits>
#include <vector>

namespace {

float PlaneDistance(const idPlane& plane, const idVec3& point) {
  return plane.a * point.x + plane.b * point.y + plane.c * point.z + plane.d;
}

bool TraceNode(const flatSoundBSP_t& bsp, int nodeNumber,
               const idVec3& start, const idVec3& end, int depth) {
  if (nodeNumber == 0) {
    return true;
  }
  if (nodeNumber < 0) {
    return false;
  }
  if (depth >= 128 || nodeNumber >= bsp.numNodes) {
    return true;
  }

  const sbspNode_t& node = bsp.nodes[nodeNumber];
  if (node.planeNum >= static_cast<unsigned int>(bsp.numPlanes)) {
    return true;
  }
  const idPlane& plane = bsp.planes[node.planeNum];
  const float startDistance = PlaneDistance(plane, start);
  const float endDistance = PlaneDistance(plane, end);
  if (startDistance >= 0.0F && endDistance >= 0.0F) {
    return TraceNode(bsp, node.children[0], start, end, depth + 1);
  }
  if (startDistance < 0.0F && endDistance < 0.0F) {
    return TraceNode(bsp, node.children[1], start, end, depth + 1);
  }

  const float fraction = std::clamp(startDistance / (startDistance - endDistance),
                                    0.0F, 1.0F);
  const idVec3 middle = start + (end - start) * fraction;
  const int nearSide = startDistance >= 0.0F ? 0 : 1;
  return TraceNode(bsp, node.children[nearSide], start, middle, depth + 1) ||
         TraceNode(bsp, node.children[nearSide ^ 1], middle, end, depth + 1);
}

idVec3 ClosestPointOnSegment(const idVec3& point, const idVec3& start,
                             const idVec3& end) {
  const idVec3 edge = end - start;
  const float lengthSquared = edge.LengthSqr();
  if (lengthSquared <= std::numeric_limits<float>::min()) {
    return start;
  }
  const float fraction = std::clamp(((point - start) * edge) / lengthSquared,
                                    0.0F, 1.0F);
  return start + edge * fraction;
}

void AppendPathPoint(soundOcclusionResult_t& result, const idVec3& point) {
  if (result.pathPoints != nullptr && result.numPathPoints < result.maxPathPoints) {
    result.pathPoints[result.numPathPoints++] = point;
  }
}

}  // namespace

bool SoundTrace_i(const flatSoundBSP_t* flatBSP,
                  const idVec3* start, const idVec3* end) {
  if (flatBSP == nullptr || flatBSP->nodes == nullptr || flatBSP->planes == nullptr ||
      flatBSP->numNodes <= 1) {
    return false;
  }
  return TraceNode(*flatBSP, 1, *start, *end, 0);
}

bool ExternalSoundTrace(const flatSoundBSP_t* flatBSP,
                        const idVec3* start, const idVec3* end) {
  return SoundTrace_i(flatBSP, start, end);
}

idVec3* SoundFaceIntersection(idVec3* result, const flatSoundBSP_t* flatBSP,
                              const sbspFace_t* face, const idVec3* lineStart,
                              const idVec3* lineEnd) {
  if (face == nullptr || face->planeNum >= flatBSP->numPlanes || face->numVerts == 0) {
    *result = *lineStart;
    return result;
  }

  const idPlane& plane = flatBSP->planes[face->planeNum];
  const float startDistance = PlaneDistance(plane, *lineStart);
  const float endDistance = PlaneDistance(plane, *lineEnd);
  const float denominator = startDistance - endDistance;
  float fraction = 0.0F;
  if (std::fabs(denominator) > std::numeric_limits<float>::min()) {
    fraction = std::clamp(startDistance / denominator, 0.0F, 1.0F);
  }
  idVec3 point = *lineStart + (*lineEnd - *lineStart) * fraction;

  std::vector<const idVec3*> vertices;
  vertices.reserve(face->numVerts);
  for (unsigned int index = 0; index < face->numVerts; ++index) {
    const int vertexIndex = flatBSP->vertIndex[face->firstVert + index];
    vertices.push_back(&flatBSP->vertices[std::abs(vertexIndex)]);
  }

  bool inside = true;
  float closestDistance = std::numeric_limits<float>::max();
  idVec3 closest = point;
  const idVec3 normal{plane.a, plane.b, plane.c};
  for (std::size_t index = 0; index < vertices.size(); ++index) {
    const idVec3& edgeStart = *vertices[index];
    const idVec3& edgeEnd = *vertices[(index + 1) % vertices.size()];
    const idVec3 edgeNormal = (edgeEnd - edgeStart).Cross(normal);
    if ((point - edgeStart) * edgeNormal < -0.1F) {
      inside = false;
    }
    const idVec3 candidate = ClosestPointOnSegment(point, edgeStart, edgeEnd);
    const float distance = (candidate - point).LengthSqr();
    if (distance < closestDistance) {
      closestDistance = distance;
      closest = candidate;
    }
  }
  *result = inside ? point : closest;
  return result;
}

void SoundOcclusionJob(soundOcclusionParms_t* parms) {
  soundOcclusionResult_t& result = parms->result;
  result.virtualSoundOrigin = parms->soundOrigin;
  result.occludedDistance = 0.0F;
  result.numPathPoints = 0;
  result.valid = 1;

  if (parms->soundAreaNum <= 0 || parms->listenerAreaNum <= 0 ||
      parms->soundAreaNum >= parms->numAreas ||
      parms->listenerAreaNum >= parms->numAreas) {
    result.occludedDistance = parms->maxDistance * 2.0F;
    return;
  }

  if (parms->soundAreaNum != parms->listenerAreaNum &&
      parms->listenerRoute[parms->soundAreaNum] == 0) {
    result.occludedDistance = parms->maxDistance * 2.0F;
    return;
  }

  float doorDistance = 0.0F;
  int routeArea = parms->soundAreaNum;
  int previousDoor = -1;
  for (int steps = 0; routeArea != parms->listenerAreaNum && steps < parms->numAreas;
       ++steps) {
    const sbspArea_t& area = parms->flatBSP.areas[routeArea];
    if (area.door >= 0 && area.door < parms->numDoors && area.door != previousDoor) {
      doorDistance += parms->doors[area.door];
      previousDoor = area.door;
    }
    const int signedFace = parms->listenerRoute[routeArea];
    if (signedFace == 0) {
      result.occludedDistance = parms->maxDistance * 2.0F;
      return;
    }
    const sbspFace_t& face = parms->flatBSP.faces[std::abs(signedFace)];
    routeArea = std::abs(face.faceAreas[signedFace >= 0 ? 1 : 0]);
  }

  const float straightDistance = (parms->listenerOrigin - parms->soundOrigin).Length();
  if (straightDistance * straightDistance + doorDistance * doorDistance >
      parms->maxDistance * parms->maxDistance) {
    result.occludedDistance = parms->maxDistance * 2.0F;
    return;
  }
  if (!SoundTrace_i(&parms->flatBSP, &parms->soundOrigin, &parms->listenerOrigin)) {
    result.occludedDistance = doorDistance;
    return;
  }

  idVec3 current = parms->soundOrigin;
  float pathDistance = doorDistance;
  routeArea = parms->soundAreaNum;
  for (int steps = 0; routeArea != parms->listenerAreaNum && steps < parms->numAreas;
       ++steps) {
    const int signedFace = parms->listenerRoute[routeArea];
    if (signedFace == 0) {
      break;
    }
    const sbspFace_t& face = parms->flatBSP.faces[std::abs(signedFace)];
    idVec3 portalPoint{};
    SoundFaceIntersection(&portalPoint, &parms->flatBSP, &face, &current,
                          &parms->listenerOrigin);
    pathDistance += (portalPoint - current).Length();
    current = portalPoint;
    AppendPathPoint(result, current);
    routeArea = std::abs(face.faceAreas[signedFace >= 0 ? 1 : 0]);
  }
  pathDistance += (parms->listenerOrigin - current).Length();
  result.virtualSoundOrigin = current;
  result.occludedDistance = pathDistance;
}
