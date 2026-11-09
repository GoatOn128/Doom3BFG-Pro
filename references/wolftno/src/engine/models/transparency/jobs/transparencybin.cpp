#include "engine/models/transparency/jobs/transparencybin.h"

#include <algorithm>
#include <cfloat>
#include <cmath>
#include <cstring>
#include <immintrin.h>

const __m128 vector_float_abs_mask = _mm_castsi128_ps(
    _mm_setr_epi32(0x7fffffff, 0x7fffffff, 0x7fffffff, 0x7fffffff));
const __m128 vector_float_sign_bit = _mm_castsi128_ps(
    _mm_setr_epi32(static_cast<int>(0x80000000u),
                   static_cast<int>(0x80000000u),
                   static_cast<int>(0x80000000u),
                   static_cast<int>(0x80000000u)));
const __m128 vector_float_last_sign_bit = _mm_castsi128_ps(
    _mm_setr_epi32(0, 0, 0, static_cast<int>(0x80000000u)));
const __m128 vector_float_first_sign_bit = _mm_castsi128_ps(
    _mm_setr_epi32(static_cast<int>(0x80000000u), 0, 0, 0));
const __m128 vector_float_clear_last = _mm_castsi128_ps(
    _mm_setr_epi32(-1, -1, -1, 0));
const __m128 DC_OFFSET_V128 = _mm_castsi128_ps(
    _mm_set1_epi32(0x26901d7d));

namespace {

enum polygonSide_t : std::uint8_t {
  SIDE_FRONT = 0,
  SIDE_BACK = 1,
  SIDE_ON = 2,
};

float PlaneDistance(const idPlane& plane, const idVec3& point) {
  return (plane.b * point.y + plane.a * point.x) +
         plane.c * point.z + plane.d;
}

void FixDegenerateNormal(idPlane& plane) {
  const float absoluteA = std::fabs(plane.a);
  const float absoluteB = std::fabs(plane.b);
  if (absoluteA >= FLT_MIN) {
    if (absoluteB < FLT_MIN && std::fabs(plane.c) < FLT_MIN) {
      const float oldA = plane.a;
      plane.a = oldA <= 0.0f ? -1.0f : 1.0f;
      plane.b = 0.0f;
      plane.c = 0.0f;
      return;
    }
  } else {
    if (absoluteB < FLT_MIN) {
      const float oldC = plane.c;
      plane.a = 0.0f;
      plane.b = 0.0f;
      plane.c = oldC <= 0.0f ? -1.0f : 1.0f;
      return;
    }
    if (std::fabs(plane.c) < FLT_MIN) {
      const float oldB = plane.b;
      plane.a = 0.0f;
      plane.b = oldB <= 0.0f ? -1.0f : 1.0f;
      plane.c = 0.0f;
      return;
    }
  }

  const float almostOne = 1.0f - FLT_EPSILON;
  if (absoluteA > almostOne) {
    const float oldA = plane.a;
    plane.a = oldA <= 0.0f ? -1.0f : 1.0f;
    plane.b = 0.0f;
    plane.c = 0.0f;
  } else if (absoluteB > almostOne) {
    const float oldB = plane.b;
    plane.a = 0.0f;
    plane.b = oldB <= 0.0f ? -1.0f : 1.0f;
    plane.c = 0.0f;
  } else if (std::fabs(plane.c) > almostOne) {
    const float oldC = plane.c;
    plane.a = 0.0f;
    plane.b = 0.0f;
    plane.c = oldC <= 0.0f ? -1.0f : 1.0f;
  }
}

bool PlaneFromPoints(idPlane& plane, const idVec3& p1, const idVec3& p2,
                     const idVec3& p3) {
  const float y1 = p1.y - p2.y;
  const float z1 = p1.z - p2.z;
  const float x1 = p1.x - p2.x;
  const float z2 = p3.z - p2.z;
  const float y2 = p3.y - p2.y;
  const float x2 = p3.x - p2.x;
  plane.a = y1 * z2 - z1 * y2;
  plane.c = x1 * y2 - x2 * y1;
  plane.b = x2 * z1 - x1 * z2;
  const float length =
      std::sqrt(plane.a * plane.a + plane.b * plane.b + plane.c * plane.c);
  if (length == 0.0f)
    return false;
  const float inverseLength = 1.0f / length;
  plane.a *= inverseLength;
  plane.b *= inverseLength;
  plane.c *= inverseLength;
  FixDegenerateNormal(plane);
  plane.d = -((p2.x * plane.a + p2.y * plane.b) + p2.z * plane.c);
  return true;
}

polygonSide_t Classify(float distance, float epsilon) {
  std::int32_t lowerBits = 0;
  std::int32_t upperBits = 0;
  const float lower = -epsilon - distance;
  const float upper = epsilon - distance;
  std::memcpy(&lowerBits, &lower, sizeof(lowerBits));
  std::memcpy(&upperBits, &upper, sizeof(upperBits));
  const int lowerNonNegative = lowerBits >= 0 ? 1 : 0;
  const int upperNonNegative = upperBits >= 0 ? 1 : 0;
  return static_cast<polygonSide_t>(
      lowerNonNegative |
      (2 * (lowerNonNegative ^ upperNonNegative)));
}

void CopyVertex(idTransparencyVert& destination,
                const idTransparencyVert& source) {
  destination = source;
}

} // namespace

int SplitPolygon(const polygon_t& polygon, const idPlane& plane,
                 float epsilon, idTransparencyVert* binVerts,
                 int& numBinVerts, polygon_t& front, polygon_t& back) {
  float distances[16]{};
  polygonSide_t sides[16]{};
  int counts[3]{};
  for (int index = 0; index < polygon.numPoints; ++index) {
    distances[index] = PlaneDistance(plane, binVerts[polygon.points[index]].xyz);
    sides[index] = Classify(distances[index], epsilon);
    ++counts[sides[index]];
  }
  sides[polygon.numPoints] = sides[0];
  distances[polygon.numPoints] = distances[0];

  if (counts[SIDE_FRONT] == 0) {
    if (counts[SIDE_BACK] == 0)
      return SIDE_ON;
    back = polygon;
    return SIDE_BACK;
  }
  if (counts[SIDE_BACK] == 0) {
    front = polygon;
    return SIDE_FRONT;
  }

  front.numPoints = 0;
  back.numPoints = 0;
  if (polygon.numPoints == 0)
    return 3;
  for (int index = 0; index < polygon.numPoints; ++index) {
    const polygonSide_t side = sides[index];
    const std::uint16_t point = polygon.points[index];
    if (side == SIDE_ON) {
      front.points[front.numPoints++] = point;
      back.points[back.numPoints++] = point;
      continue;
    } else if (side == SIDE_FRONT) {
      front.points[front.numPoints++] = point;
    } else {
      back.points[back.numPoints++] = point;
    }

    const polygonSide_t nextSide = sides[index + 1];
    if (nextSide == SIDE_ON || nextSide == side)
      continue;

    const int nextIndex = (index + 1) % polygon.numPoints;
    const idTransparencyVert* start;
    const idTransparencyVert* end;
    float fraction;
    if (side == SIDE_FRONT) {
      start = &binVerts[point];
      end = &binVerts[polygon.points[nextIndex]];
      fraction = distances[index] /
                 (distances[index] - distances[index + 1]);
    } else {
      start = &binVerts[polygon.points[nextIndex]];
      end = &binVerts[point];
      fraction = distances[index + 1] /
                 (distances[index + 1] - distances[index]);
    }

    idTransparencyVert& split = binVerts[numBinVerts++];
    split.xyz.x = (end->xyz.x - start->xyz.x) * fraction + start->xyz.x;
    split.xyz.y = (end->xyz.y - start->xyz.y) * fraction + start->xyz.y;
    split.xyz.z = (end->xyz.z - start->xyz.z) * fraction + start->xyz.z;
    split.st[0] = static_cast<std::int16_t>(
        static_cast<int>((end->st[0] - start->st[0]) * fraction +
                         start->st[0]));
    split.st[1] = static_cast<std::int16_t>(
        static_cast<int>((end->st[1] - start->st[1]) * fraction +
                         start->st[1]));
    split.st1[0] = static_cast<std::int16_t>(
        static_cast<int>((end->st1[0] - start->st1[0]) * fraction +
                         start->st1[0]));
    split.st1[1] = static_cast<std::int16_t>(
        static_cast<int>((end->st1[1] - start->st1[1]) * fraction +
                         start->st1[1]));
    std::memcpy(split.normal, start->normal, sizeof(split.normal));
    std::memcpy(split.tangent, start->tangent, sizeof(split.tangent));
    std::memcpy(split.color, start->color, sizeof(split.color));
    const std::uint16_t splitIndex =
        static_cast<std::uint16_t>(numBinVerts - 1);
    front.points[front.numPoints++] = splitIndex;
    back.points[back.numPoints++] = splitIndex;
  }
  return 3;
}

void BinPolygon_r(deferredTransparencyBinParms_t* parms, int nodeNum,
                  const polygon_t& polygon) {
  if (nodeNum >= 0) {
    node_t& node = parms->nodes[nodeNum];
    polygon_t front{};
    polygon_t back{};
    switch (SplitPolygon(polygon, node.plane, 0.1f, parms->tempBinVerts,
                         parms->numBinVerts, front, back)) {
      case 3:
        BinPolygon_r(parms, node.children[0], front);
        BinPolygon_r(parms, node.children[1], back);
        return;
      case SIDE_FRONT:
        BinPolygon_r(parms, node.children[0], front);
        return;
      case SIDE_BACK:
        BinPolygon_r(parms, node.children[1], back);
        return;
      default:
        return;
    }
  }
  if (nodeNum == -1)
    return;
  bin_t& bin = parms->bins[-2 - nodeNum];
  for (int point = 2; point < polygon.numPoints; ++point) {
    const int writeIndex = bin.numIndices % 126;
    bin.indices[writeIndex] = polygon.points[0];
    bin.indices[writeIndex + 1] = polygon.points[point - 1];
    bin.indices[writeIndex + 2] = polygon.points[point];
    bin.numIndices += 3;
  }
}

int BuildBinTree_r(deferredTransparencyBinParms_t* parms,
                   const idVec3* screenVerts, int offsetX, int partitionsX,
                   int offsetY, int partitionsY) {
  if (partitionsX <= 1) {
    if (partitionsY <= 1) {
      const int binIndex = offsetX / 32 + 1280 * (offsetY / 16) / 32;
      parms->bins[binIndex].numIndices = 0;
      return -2 - binIndex;
    }

    const float x = screenVerts[0].x;
    const float y = screenVerts[0].y;
    const float z = screenVerts[0].z;
    const int nodeIndex = parms->numNodes;
    node_t& node = parms->nodes[nodeIndex];
    const int splitY = offsetY + 16 * (partitionsY / 2);
    const float fraction = static_cast<float>(splitY) / 720.0f;
    idVec3 p0;
    p0.x = x + (screenVerts[2].x - x) * fraction;
    p0.y = y + (screenVerts[2].y - y) * fraction;
    p0.z = z + (screenVerts[2].z - z) * fraction;
    idVec3 p1;
    p1.x = (screenVerts[1].x - x) + p0.x;
    p1.y = (screenVerts[1].y - y) + p0.y;
    p1.z = (screenVerts[1].z - z) + p0.z;
    parms->numNodes = nodeIndex + 1;
    PlaneFromPoints(node.plane, parms->viewOrigin, p1, p0);
    node.children[0] = static_cast<std::int16_t>(BuildBinTree_r(
        parms, screenVerts, offsetX, partitionsX, splitY,
        partitionsY - partitionsY / 2));
    node.children[1] = static_cast<std::int16_t>(BuildBinTree_r(
        parms, screenVerts, offsetX, partitionsX, offsetY,
        partitionsY / 2));
    return nodeIndex;
  }

  const float x = screenVerts[0].x;
  const float y = screenVerts[0].y;
  const float z = screenVerts[0].z;
  const int nodeIndex = parms->numNodes;
  node_t& node = parms->nodes[nodeIndex];
  const int splitX = offsetX + 32 * (partitionsX / 2);
  const float fraction = static_cast<float>(splitX) / 1280.0f;
  idVec3 p0;
  p0.x = x + (screenVerts[1].x - x) * fraction;
  p0.y = y + (screenVerts[1].y - y) * fraction;
  p0.z = z + (screenVerts[1].z - z) * fraction;
  idVec3 p1;
  p1.x = (screenVerts[2].x - x) + p0.x;
  p1.y = (screenVerts[2].y - y) + p0.y;
  p1.z = (screenVerts[2].z - z) + p0.z;
  parms->numNodes = nodeIndex + 1;
  PlaneFromPoints(node.plane, parms->viewOrigin, p0, p1);
  node.children[0] = static_cast<std::int16_t>(BuildBinTree_r(
      parms, screenVerts, splitX, partitionsX - partitionsX / 2, offsetY,
      partitionsY));
  node.children[1] = static_cast<std::int16_t>(BuildBinTree_r(
      parms, screenVerts, offsetX, partitionsX / 2, offsetY, partitionsY));
  return nodeIndex;
}

void BinQuads(deferredTransparencyBinParms_t* parms) {
  const float originY = parms->viewOrigin.y;
  const float rightZ = parms->viewAxis.mat[1].z;
  const float originX = parms->viewOrigin.x;
  const float scaledFovY = parms->fovY * 128.0f;
  const float scaledFovX = parms->fovX * 128.0f;
  const float forwardZ = parms->viewAxis.mat[0].z * 128.0f;
  const float forwardX = parms->viewAxis.mat[0].x * 128.0f;
  const float forwardY = parms->viewAxis.mat[0].y * 128.0f;
  const float originZ = parms->viewOrigin.z;
  const float upZ = parms->viewAxis.mat[2].z * scaledFovY;
  const float upX = parms->viewAxis.mat[2].x * scaledFovY;
  const float upY = parms->viewAxis.mat[2].y * scaledFovY;
  const float rightY = parms->viewAxis.mat[1].y * scaledFovX;
  const float rightX = parms->viewAxis.mat[1].x * scaledFovX;
  idVec3 screenVerts[3];
  screenVerts[0].x = ((originX + forwardX) - rightX) - upX;
  screenVerts[0].y = ((originY + forwardY) - rightY) - upY;
  screenVerts[0].z = ((originZ + forwardZ) - rightZ * scaledFovX) - upZ;
  screenVerts[1].x = ((originX + forwardX) + rightX) - upX;
  screenVerts[1].y = ((forwardY + originY) + rightY) - upY;
  screenVerts[1].z = ((forwardZ + originZ) + rightZ * scaledFovX) - upZ;
  screenVerts[2].x = ((originX + forwardX) - rightX) + upX;
  screenVerts[2].y = ((forwardY + originY) - rightY) + upY;
  screenVerts[2].z = ((forwardZ + originZ) - rightZ * scaledFovX) + upZ;
  const idVec3& lowerLeft = screenVerts[0];
  const idVec3& lowerRight = screenVerts[1];
  const idVec3& upperLeft = screenVerts[2];
  idVec3 upperRight;
  upperRight.x = (lowerRight.x + upperLeft.x) - lowerLeft.x;
  upperRight.y = (upperLeft.y + lowerRight.y) - lowerLeft.y;
  upperRight.z = (upperLeft.z + lowerRight.z) - lowerLeft.z;

  node_t& first = parms->nodes[parms->numNodes++];
  PlaneFromPoints(first.plane, parms->viewOrigin, lowerRight, lowerLeft);
  first.children[1] = -1;
  first.children[0] = static_cast<std::int16_t>(parms->numNodes);
  node_t& second = parms->nodes[parms->numNodes++];
  PlaneFromPoints(second.plane, parms->viewOrigin, lowerLeft, upperLeft);
  second.children[0] = static_cast<std::int16_t>(parms->numNodes);
  second.children[1] = -1;
  node_t& third = parms->nodes[parms->numNodes++];
  PlaneFromPoints(third.plane, parms->viewOrigin, upperLeft, upperRight);
  third.children[1] = -1;
  third.children[0] = static_cast<std::int16_t>(parms->numNodes);
  node_t& fourth = parms->nodes[parms->numNodes++];
  PlaneFromPoints(fourth.plane, parms->viewOrigin, upperRight, lowerRight);
  fourth.children[0] = static_cast<std::int16_t>(parms->numNodes);
  fourth.children[1] = -1;
  BuildBinTree_r(parms, screenVerts, 0, 40, 0, 45);
  parms->numBins = 1800;

  for (int indexOffset = 0; indexOffset < parms->numIndices;
       indexOffset += 6) {
    const int firstVertex = parms->indices[indexOffset];
    polygon_t polygon{};
    polygon.numPoints = 4;
    const int start = parms->numBinVerts;
    CopyVertex(parms->tempBinVerts[parms->numBinVerts++],
               parms->verts[firstVertex + 2]);
    CopyVertex(parms->tempBinVerts[parms->numBinVerts++],
               parms->verts[firstVertex + 3]);
    CopyVertex(parms->tempBinVerts[parms->numBinVerts++],
               parms->verts[firstVertex + 1]);
    CopyVertex(parms->tempBinVerts[parms->numBinVerts++],
               parms->verts[firstVertex]);
    polygon.points[0] = static_cast<std::uint16_t>(start);
    polygon.points[1] = static_cast<std::uint16_t>(start + 1);
    polygon.points[2] = static_cast<std::uint16_t>(start + 2);
    polygon.points[3] = static_cast<std::uint16_t>(start + 3);
    BinPolygon_r(parms, 0, polygon);
    if (parms->numBinVerts >= parms->maxBinVerts - 1724)
      break;
  }

  std::memset(parms->vertexRemap, 0xff,
              sizeof(int) * static_cast<std::size_t>(parms->numBinVerts));
  int compactVertexCount = 0;
  for (int binIndex = 0; binIndex < parms->numBins; ++binIndex) {
    const bin_t& bin = parms->bins[binIndex];
    const int oldest = std::max(bin.numIndices - 126, 0);
    for (int source = bin.numIndices - 3; source >= oldest; source -= 3) {
      for (int corner = 0; corner < 3; ++corner) {
        const std::uint16_t sourceVertex =
            bin.indices[(source % 126) + corner];
        int& remap = parms->vertexRemap[sourceVertex];
        if (remap == -1)
          remap = compactVertexCount++;
        parms->binIndices[parms->numBinIndices + corner] =
            static_cast<std::uint16_t>(remap);
      }
      parms->numBinIndices += 3;
      if (parms->numBinIndices >= parms->maxBinIndices)
        break;
    }
    if (parms->numBinIndices >= parms->maxBinIndices)
      break;
  }

  for (int source = 0; source < parms->numBinVerts; ++source) {
    const int destination = parms->vertexRemap[source];
    if (destination != -1)
      CopyVertex(parms->binVerts[destination], parms->tempBinVerts[source]);
  }
}
