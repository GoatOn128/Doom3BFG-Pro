#include "engine/models/cable/jobs/cablegen.h"

#include "shared/idlib/parallelism/paralleljoblist.h"

#include <algorithm>
#include <cmath>
#include <cstring>
#include <limits>

namespace {

constexpr int kMaximumCablePoints = 512;

idVec3 Position(const idVec4& point) {
  return {point.x, point.y, point.z};
}

float SplineLength(const idVec3& vector) {
  return std::max(
      std::sqrt(std::max(vector.LengthSqr(),
                         std::numeric_limits<float>::min())),
      std::numeric_limits<float>::epsilon());
}

idVec3 Normalize(const idVec3& vector) {
  const float inverseLength =
      1.0f / std::sqrt(std::max(vector.LengthSqr(),
                                std::numeric_limits<float>::min()));
  return vector * inverseLength;
}

idVec3 Hermite(const idVec3& start, const idVec3& end,
               const idVec3& startTangent, const idVec3& endTangent,
               float fraction) {
  const float fractionSquared = fraction * fraction;
  const float fractionCubed = fractionSquared * fraction;
  const float startWeight = 2.0f * fractionCubed - 3.0f * fractionSquared + 1.0f;
  const float endWeight = 3.0f * fractionSquared - 2.0f * fractionCubed;
  const float startTangentWeight =
      fractionCubed - 2.0f * fractionSquared + fraction;
  const float endTangentWeight = fractionCubed - fractionSquared;
  return start * startWeight + end * endWeight +
         startTangent * startTangentWeight + endTangent * endTangentWeight;
}

std::uint8_t EncodeDirection(float value) {
  const float scaled = (value + 1.0f) * 127.5f + 0.5f;
  const long rounded = std::lrintf(scaled);
  return static_cast<std::uint8_t>(std::clamp(rounded, 0L, 255L));
}

void StoreDirection(std::uint8_t destination[4], const idVec3& direction) {
  destination[0] = EncodeDirection(direction.x);
  destination[1] = EncodeDirection(direction.y);
  destination[2] = EncodeDirection(direction.z);
}

} // namespace

void GenCable(const cableGenParms_t* parms) {
  if (parms->numPoints < 2) {
    *parms->numVertices = 0;
    *parms->numIndices = 0;
    *parms->done = 1;
    return;
  }

  int segmentCount = parms->numPoints - 1;
  segmentCount = std::min(segmentCount, parms->lengthMaxSubdivision / 16 - 1);

  idVec3 cablePoints[kMaximumCablePoints];
  int generatedPointCount = 0;
  for (int segment = 0; segment < segmentCount; ++segment) {
    const idVec3 start = Position(parms->points[segment]);
    const idVec3 previous = Position(parms->points[std::max(segment - 1, 0)]);
    const idVec3 end = Position(parms->points[std::min(segment + 1, segmentCount)]);
    const idVec3 following =
        Position(parms->points[std::min(segment + 2, segmentCount)]);
    const float previousLength = SplineLength(start - previous);
    const float segmentLength = SplineLength(end - start);
    const float followingLength = SplineLength(following - end);

    cablePoints[generatedPointCount++] = start;
    if (segmentLength > 1.0f) {
      const float controlScale = parms->points[segment].w;
      const idVec3 startTangent =
          (end - previous) *
          (controlScale * 2.0f * segmentLength /
           (segmentLength + previousLength));
      const idVec3 endTangent =
          (following - start) *
          (controlScale * 2.0f * segmentLength /
           (followingLength + segmentLength));
      for (int subdivision = 1; subdivision < 16; ++subdivision) {
        const float fraction = static_cast<float>(subdivision) * (1.0f / 16.0f);
        cablePoints[generatedPointCount++] =
            Hermite(start, end, startTangent, endTangent, fraction);
      }
    }
  }
  cablePoints[generatedPointCount++] = Position(parms->points[segmentCount]);

  idMat3 axis = mat3_identity;
  const float inverseCircumference =
      1.0f / (3.1415927f * parms->diameter);
  float distanceAlongCable = 0.0f;
  for (int pointIndex = 0; pointIndex < generatedPointCount; ++pointIndex) {
    idVec3 direction;
    if (pointIndex == generatedPointCount - 1)
      direction = cablePoints[pointIndex] - cablePoints[pointIndex - 1];
    else
      direction = cablePoints[pointIndex + 1] - cablePoints[pointIndex];
    const float directionLengthSquared = direction.LengthSqr();
    const float directionLength = std::sqrt(std::max(
        directionLengthSquared, std::numeric_limits<float>::min()));
    direction *= 1.0f / directionLength;

    idQuat rotation{direction.Cross(axis[2]).x, direction.Cross(axis[2]).y,
                    direction.Cross(axis[2]).z, direction * axis[2] + 1.0f};
    const float inverseQuatLength = 1.0f / std::sqrt(std::max(
        rotation * rotation, std::numeric_limits<float>::min()));
    rotation = rotation * inverseQuatLength;
    axis = axis * rotation.ToMat3();
    axis[2] = direction;
    axis[1] = Normalize(direction.Cross(axis[0]));
    axis[0] = Normalize(axis[1].Cross(direction));

    for (int radial = 0; radial < parms->diameterSubdivision; ++radial) {
      const subdivPoint_t& subdivision = parms->subdivPoints[radial];
      idDrawVert vertex{};
      vertex.xyz = cablePoints[pointIndex] + axis[0] * subdivision.position.x +
                   axis[1] * subdivision.position.y +
                   axis[2] * subdivision.position.z;
      vertex.st.x = subdivision.texcoord;
      vertex.st.y = distanceAlongCable * inverseCircumference;
      vertex.color[0] = 255;
      vertex.color[1] = 255;
      vertex.color[2] = 255;
      vertex.color[3] = 255;
      const idVec3 tangent = axis[0] * subdivision.tangent.x +
                             axis[1] * subdivision.tangent.y +
                             axis[2] * subdivision.tangent.z;
      StoreDirection(vertex.tangent, tangent);
      StoreDirection(vertex.normal, direction.Cross(tangent));
      parms->vertices[pointIndex * parms->diameterSubdivision + radial] = vertex;
    }
    distanceAlongCable += directionLength;
  }

  std::uint16_t* indices = parms->indices;
  for (int ring = 0; ring < generatedPointCount - 1; ++ring) {
    const int firstRing = ring * parms->diameterSubdivision;
    const int secondRing = ((ring + 1) % generatedPointCount) *
                           parms->diameterSubdivision;
    for (int radial = 0; radial < parms->diameterSubdivision; ++radial) {
      const int nextRadial = (radial + 1) % parms->diameterSubdivision;
      const std::uint16_t first = static_cast<std::uint16_t>(firstRing + radial);
      const std::uint16_t firstNext =
          static_cast<std::uint16_t>(firstRing + nextRadial);
      const std::uint16_t second =
          static_cast<std::uint16_t>(secondRing + radial);
      const std::uint16_t secondNext =
          static_cast<std::uint16_t>(secondRing + nextRadial);
      *indices++ = first;
      *indices++ = firstNext;
      *indices++ = secondNext;
      *indices++ = secondNext;
      *indices++ = second;
      *indices++ = first;
    }
  }

  *parms->numVertices = generatedPointCount * parms->diameterSubdivision;
  *parms->numIndices =
      6 * (generatedPointCount - 1) * parms->diameterSubdivision;
  *parms->done = 1;
}

idParallelJobRegistration register_GenCable(
    reinterpret_cast<jobRun_t>(GenCable), "GenCable");
