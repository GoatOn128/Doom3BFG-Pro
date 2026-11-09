#define WOLFTNO_RETAIL_IDMATH_CONSTANT_ABI
#include "engine/gamelib/effects/trailbeam.h"

#pragma warning(push)
#pragma warning(disable : 4201 4324 4984)
#include "engine/decls/decltable.h"
#include "engine/models/particles/rendermodeleffects.h"
#include "engine/models/rendermodelbeam.h"
#pragma warning(pop)
#include "shared/idlib/geometry/geometry.h"
#include "shared/idlib/math/mathlib.h"
#undef WOLFTNO_RETAIL_IDMATH_CONSTANT_ABI

#include <cmath>
#include <cstdint>
#include <cstring>
#include <xmmintrin.h>

const idVec2 vec2_origin{0.0f, 0.0f};
const float idMath::FLOAT_EPSILON = 1.192092896e-07f;
const __m128 idMath::vector_float_smallestNonDenorm = {
    idMath::FLT_SMALLEST_NON_DENORMAL,
    idMath::FLT_SMALLEST_NON_DENORMAL,
    idMath::FLT_SMALLEST_NON_DENORMAL,
    idMath::FLT_SMALLEST_NON_DENORMAL};

namespace {

constexpr float kMillisecondsToSeconds = 0.001f;
constexpr float kParticleMinimumSeparation = 0.1f;
constexpr float kRandomToUnit = 0.000030518509f;

float Clamp01(float value) {
  if (value < 0.0f) {
    return 0.0f;
  }
  if (value > 1.0f) {
    return 1.0f;
  }
  return value;
}

float SseMax(float lhs, float rhs) {
  return _mm_cvtss_f32(_mm_max_ss(_mm_set_ss(lhs), _mm_set_ss(rhs)));
}

float SseMin(float lhs, float rhs) {
  return _mm_cvtss_f32(_mm_min_ss(_mm_set_ss(lhs), _mm_set_ss(rhs)));
}

float ReciprocalSqrtClamped(float value) {
  const __m128 input = _mm_set1_ps(value);
  const __m128 bounded =
      _mm_max_ps(input, idMath::vector_float_smallestNonDenorm);
  return _mm_cvtss_f32(
      _mm_div_ps(_mm_set1_ps(1.0f), _mm_sqrt_ps(bounded)));
}

std::uint8_t ColorByte(int value) {
  if (value < 0) {
    return 0;
  }
  if (value > 255) {
    return 255;
  }
  return static_cast<std::uint8_t>(value);
}

std::uint8_t ColorByte(float value) {
  return ColorByte(static_cast<int>(value * 255.0f));
}

void SetColor(std::uint8_t (&color)[4], std::uint8_t red,
              std::uint8_t green, std::uint8_t blue,
              std::uint8_t alpha) {
  color[0] = red;
  color[1] = green;
  color[2] = blue;
  color[3] = alpha;
}

void InitializeBeamNode(beamNodeParms_t& node, float sStart,
                        float sEnd, float halfWidth,
                        std::uint8_t red, std::uint8_t green,
                        std::uint8_t blue, std::uint8_t alpha) {
  node.startPos = vec3_origin;
  node.endPos = vec3_origin;
  node.startPos2 = vec3_origin;
  node.endPos2 = vec3_origin;
  node.orientVec = vec3_origin;
  node.sMinMax = {sStart, sEnd};
  node.tMinMax = {0.0f, 1.0f};
  node.halfWidth = halfWidth;
  SetColor(node.color[0], red, green, blue, alpha);
  SetColor(node.color[1], red, green, blue, alpha);
  for (int vertex = 0; vertex < 4; ++vertex) {
    node.tangent[vertex][0] = 128;
    node.tangent[vertex][1] = 128;
    node.tangent[vertex][2] = 128;
  }
}

float TextureCoordinate(float distance, float beamLength,
                        float sStart, float sEnd) {
  return (Clamp01(distance / beamLength) * (sEnd - sStart)) + sStart;
}

std::uint32_t ParticleRandomBits(int timestamp, float distance) {
  const std::uint32_t timestampSeed =
      static_cast<std::uint32_t>(timestamp) << 8u;
  const std::uint32_t distanceInteger =
      static_cast<std::uint32_t>(static_cast<std::int64_t>(distance));
  return (1664525u * timestampSeed) - (8175616u * distanceInteger) +
         7271263u;
}

float ParticleDiversity(int timestamp, float distance) {
  return static_cast<float>((ParticleRandomBits(timestamp, distance) >> 10u) &
                            0x7fffu) *
         kRandomToUnit;
}

int ParticleStartTime(int timestamp, float distance, float inverseSpeed) {
  return timestamp -
         static_cast<int>((distance * inverseSpeed) * -1000.0f);
}

}  // namespace

idTrailBeam::idTrailBeam() {
  beamModel = nullptr;
  effectModel = nullptr;
  decl = nullptr;
  start = vec3_origin;
  end = vec3_origin;
  timestamp = -1;
  *reinterpret_cast<std::uint64_t*>(&updateStartLength) = 0;
}

void idTrailBeam::Init(idRenderModelBeam* beamModel_,
                       idRenderModelEffects* effectModel_,
                       const idDeclTrailBeam* decl_) {
  beamModel = beamModel_;
  effectModel = effectModel_;
  decl = decl_;
}

void idTrailBeam::StartTrail(int startTime, const idVec3& start_,
                             const idVec3& end_) {
  if ((beamModel != nullptr || effectModel != nullptr) && decl != nullptr) {
    timestamp = startTime;
    start = start_;
    end = end_;
    *reinterpret_cast<std::uint64_t*>(&updateStartLength) = 0;
  }
}

void idTrailBeam::StopTrail() {
  start = vec3_origin;
  end = vec3_origin;
  timestamp = -1;
  *reinterpret_cast<std::uint64_t*>(&updateStartLength) = 0;
}

void idTrailBeam::Update(int currentTime, int gameMsPerFrame) {
  if (timestamp == -1) {
    return;
  }

  const int elapsedMilliseconds = currentTime - timestamp;
  const float duration = decl->trailProperties.duration;
  if (elapsedMilliseconds >= static_cast<int>(duration * 1000.0f)) {
    timestamp = -1;
    return;
  }

  const float elapsedSeconds =
      static_cast<float>(elapsedMilliseconds) * kMillisecondsToSeconds;
  const float timeFraction = Clamp01(elapsedSeconds / duration);

  float fadeIn = 1.0f;
  if (decl->trailProperties.fadeInDuration > 0.0f) {
    fadeIn = Clamp01(elapsedSeconds /
                     decl->trailProperties.fadeInDuration);
  }

  float fadeOut = 1.0f;
  if (decl->trailProperties.fadeOutDuration > 0.0f) {
    const float fadeOutStart =
        duration - decl->trailProperties.fadeOutDuration;
    const float fadeTime = elapsedSeconds - fadeOutStart;
    if (fadeTime >= 0.0f) {
      fadeOut =
          1.0f - Clamp01(fadeTime /
                         decl->trailProperties.fadeOutDuration);
    }
  }
  const float trailAlpha = fadeOut * fadeIn;

  const float deltaX = end.x - start.x;
  const float deltaY = end.y - start.y;
  const float deltaZ = end.z - start.z;
  const float lengthSquared =
      ((deltaX * deltaX) + (deltaY * deltaY)) + (deltaZ * deltaZ);
  const float inverseLength = ReciprocalSqrtClamped(lengthSquared);
  const idVec3 direction{deltaX * inverseLength, deltaY * inverseLength,
                         deltaZ * inverseLength};
  const float fullLength = inverseLength * lengthSquared;
  const float traveledLength = elapsedSeconds * decl->trailProperties.speed;
  const float beamLength = SseMin(fullLength, traveledLength);
  const float beamOffsetX = direction.x * beamLength;
  const float beamOffsetY = direction.y * beamLength;
  const float beamOffsetZ = direction.z * beamLength;

  beamNodeParms_t nodes[3];
  int numNodes = 0;

  if (trailAlpha > idMath::FLOAT_EPSILON) {
    const float scroll =
        decl->trailProperties.textureScrollSpeed.GetValue(timeFraction);
    const float textureRepeat = decl->trailProperties.textureRepeat;
    const float repeatDenominator =
        SseMax(idMath::FLOAT_EPSILON, textureRepeat);
    const float sStart =
        ::fmodf(scroll * elapsedSeconds, textureRepeat) /
        repeatDenominator;
    const float sEnd =
        (beamLength / repeatDenominator) + sStart;
    const float halfWidth =
        decl->trailProperties.width.GetValue(timeFraction) * 0.5f;

    const std::uint8_t red = ColorByte(static_cast<int>(
        decl->trailProperties.color.r * 255.0f));
    const std::uint8_t green = ColorByte(static_cast<int>(
        decl->trailProperties.color.g * 255.0f));
    const std::uint8_t blue = ColorByte(static_cast<int>(
        decl->trailProperties.color.b * 255.0f));
    const std::uint8_t alpha = ColorByte(static_cast<int>(
        (trailAlpha * decl->trailProperties.color.a) * 255.0f));

    for (beamNodeParms_t& node : nodes) {
      InitializeBeamNode(node, sStart, sEnd, halfWidth,
                         red, green, blue, alpha);
    }

    const float fadeInLength = decl->trailProperties.fadeInLength;
    const float fadeOutLength = decl->trailProperties.fadeOutLength;
    const float totalFadeLength = fadeInLength + fadeOutLength;

    if (!(totalFadeLength > beamLength)) {
      numNodes = 3;
      const float fadeOutPosition = beamLength - fadeOutLength;
      const idVec3 fadeInPoint{
          (fadeInLength * direction.x) + start.x,
          (fadeInLength * direction.y) + start.y,
          (fadeInLength * direction.z) + start.z};
      const idVec3 fadeOutPoint{
          (fadeOutPosition * direction.x) + start.x,
          (fadeOutPosition * direction.y) + start.y,
          (fadeOutPosition * direction.z) + start.z};
      const idVec3 beamEnd{beamOffsetX + start.x, beamOffsetY + start.y,
                           beamOffsetZ + start.z};

      nodes[0].startPos = start;
      nodes[0].endPos = fadeInPoint;
      nodes[1].startPos = fadeInPoint;
      nodes[1].endPos = fadeOutPoint;
      nodes[2].startPos = fadeOutPoint;
      nodes[2].endPos = beamEnd;

      nodes[0].color[0][3] = 0;
      nodes[2].color[1][3] = 0;
      nodes[0].sMinMax.x = sStart;
      nodes[0].sMinMax.y =
          TextureCoordinate(fadeInLength, beamLength, sStart, sEnd);
      nodes[1].sMinMax.x = nodes[0].sMinMax.y;
      nodes[1].sMinMax.y =
          TextureCoordinate(fadeOutPosition, beamLength, sStart, sEnd);
      nodes[2].sMinMax.x = nodes[1].sMinMax.y;
      nodes[2].sMinMax.y = sEnd;
    } else if (fadeInLength > 0.0f && fadeOutLength > 0.0f) {
      const float rayInverseLength =
          ReciprocalSqrtClamped((fadeInLength * fadeInLength) + 1.0f);
      const idVec2 rayDirection{fadeInLength * rayInverseLength,
                                rayInverseLength};
      const idVec2 segmentStart{beamLength, 0.0f};
      const idVec2 segmentEnd{beamLength - fadeOutLength, 1.0f};
      float middleDistance = 0.0f;
      if (!idGeometry::RayIntersectsLineSegment2D(
              vec2_origin, rayDirection, segmentStart, segmentEnd,
              middleDistance)) {
        middleDistance = beamLength * 0.5f;
      }

      float middleAlpha = 0.0f;
      if (!(idMath::FLOAT_EPSILON >= middleDistance)) {
        middleAlpha =
            Clamp01((beamLength - middleDistance) / fadeOutLength) *
            Clamp01(middleDistance / fadeInLength);
      }

      if (!(idMath::FLOAT_EPSILON >= middleAlpha)) {
        numNodes = 2;
        const idVec3 middle{
            (direction.x * middleDistance) + start.x,
            (middleDistance * direction.y) + start.y,
            (middleDistance * direction.z) + start.z};
        nodes[0].startPos = start;
        nodes[0].endPos = middle;
        nodes[1].startPos = middle;
        nodes[1].endPos = {beamOffsetX + start.x,
                           beamOffsetY + start.y,
                           beamOffsetZ + start.z};
        nodes[0].color[0][3] = 0;
        SetColor(nodes[0].color[1], 255, 255, 255,
                 ColorByte(middleAlpha * trailAlpha));
        nodes[1].color[0][0] = 255;
        nodes[1].color[0][1] = 255;
        nodes[1].color[0][2] = 255;
        nodes[1].color[0][3] = nodes[0].color[1][3];
        nodes[1].color[1][3] = 0;
        nodes[0].sMinMax.x = sStart;
        nodes[0].sMinMax.y =
            TextureCoordinate(middleDistance, beamLength, sStart, sEnd);
        nodes[1].sMinMax.x = nodes[0].sMinMax.y;
        nodes[1].sMinMax.y = sEnd;
      }
    } else if (fadeInLength > 0.0f) {
      numNodes = 1;
      const float endAlpha = Clamp01(beamLength / fadeInLength) * trailAlpha;
      nodes[0].startPos = start;
      nodes[0].endPos = {beamOffsetX + start.x, beamOffsetY + start.y,
                         beamOffsetZ + start.z};
      nodes[0].color[0][3] = 0;
      SetColor(nodes[0].color[1], 255, 255, 255,
               ColorByte(endAlpha));
      nodes[0].sMinMax = {sStart, sEnd};
    } else {
      numNodes = 1;
      const float startAlpha =
          Clamp01(beamLength / fadeOutLength) * trailAlpha;
      nodes[0].startPos = start;
      nodes[0].endPos = {beamOffsetX + start.x, beamOffsetY + start.y,
                         beamOffsetZ + start.z};
      SetColor(nodes[0].color[0], 255, 255, 255,
               ColorByte(startAlpha));
      nodes[0].color[1][3] = 0;
      nodes[0].sMinMax = {sStart, sEnd};
    }
  }

  if (beamLength > 0.0f && decl->quadProperties.particle != nullptr &&
      effectModel != nullptr && effectModel->gameTime >= timestamp) {
    const float inverseSpeed = 1.0f / decl->trailProperties.speed;
    const float separation =
        SseMax(kParticleMinimumSeparation,
               decl->quadProperties.separation);
    const idMat3 particleAxis = direction.ToZAxis();
    const float speed = decl->trailProperties.speed;
    const float velocityScale = decl->quadProperties.velocityScale;
    const idVec3 particleVelocity{
        (direction.x * speed) * velocityScale,
        (direction.y * speed) * velocityScale,
        (direction.z * speed) * velocityScale};
    const std::uint32_t particleColor = 0xffffffffu;

    float startCursor = updateStartLength;
    if (updateEndLength > startCursor) {
      bool particleAdded = false;
      do {
        const idVec3 particleOrigin{
            (direction.x * startCursor) + start.x,
            (startCursor * direction.y) + start.y,
            (startCursor * direction.z) + start.z};
        particleAdded |= effectModel->AddParticles(
            decl->quadProperties.particle,
            ParticleStartTime(timestamp, startCursor, inverseSpeed),
            gameMsPerFrame, ParticleDiversity(timestamp, startCursor),
            particleOrigin, particleAxis, particleVelocity, particleColor,
            false);
        startCursor = startCursor + separation;
        if (!particleAdded) {
          updateStartLength = startCursor;
        }
      } while (updateEndLength > startCursor);
    }

    float endCursor = updateEndLength;
    if (beamLength > endCursor) {
      do {
        const idVec3 particleOrigin{
            (direction.x * endCursor) + start.x,
            (endCursor * direction.y) + start.y,
            (endCursor * direction.z) + start.z};
        effectModel->AddParticles(
            decl->quadProperties.particle,
            ParticleStartTime(timestamp, endCursor, inverseSpeed),
            gameMsPerFrame, ParticleDiversity(timestamp, endCursor),
            particleOrigin, particleAxis, particleVelocity, particleColor,
            true);
        endCursor = endCursor + separation;
      } while (beamLength > endCursor);
      updateEndLength = endCursor;
    }
  }

  if (numNodes > 0) {
    beamModel->DrawBeam(nodes, numNodes, decl->trailProperties.material,
                        BEAM_ORIENT_VIEWER);
  }
}
