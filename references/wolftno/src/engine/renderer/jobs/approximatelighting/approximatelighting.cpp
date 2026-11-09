#include "engine/renderer/jobs/approximatelighting/approximatelighting.h"

#include "engine/renderer/jobs/render/graphicsapiwrapper.h"
#if defined(_MSC_VER)
#pragma warning(push)
#pragma warning(disable : 4324)
#endif
#include "engine/renderer/renderlightcommitted.h"
#if defined(_MSC_VER)
#pragma warning(pop)
#endif
#include "shared/idlib/math/mathlib.h"
#include "shared/idlib/parallelism/paralleljoblist.h"

#include <algorithm>
#include <cmath>
#include <cstdint>
#include <cstring>
#include <immintrin.h>

namespace {

float SafeLength(const idVec3& value) {
  return std::sqrt(std::max(value.x * value.x + value.y * value.y +
                                value.z * value.z,
                            idMath::FLT_SMALLEST_NON_DENORMAL));
}

idVec3 NormalizeRetail(const idVec3& value) {
  const float inverseLength = 1.0f / SafeLength(value);
  return {value.x * inverseLength, value.y * inverseLength,
          value.z * inverseLength};
}

void ClearContribution(idVec3& direction, idVec3& color) {
  direction = {0.0f, 0.0f, 0.0f};
  color = {0.0f, 0.0f, 0.0f};
}

bool IsPrivateLightClass(lightClass_t lightClass) {
  return lightClass == LIGHT_PRIVATE_DYNAMIC_ONLY ||
         lightClass == LIGHT_PRIVATE_PRIME ||
         lightClass == LIGHT_SLOWLIGHT_PRIVATE_ONLY;
}

bool MatchesLightGroup(const idRenderLightCommitted& light,
                       const approximateLightingParms_t& parms) {
  return (light.r.lightGroupID == -1 && parms.lightGroupID == -1) ||
         light.r.lightGroupID == parms.lightGroupID;
}

void DistributeDirection(const idVec3& direction, const idVec3& color,
                         approximateLighting_t& lighting) {
  const float manhattan = std::fabs(direction.x) + std::fabs(direction.y) +
                          std::fabs(direction.z);
  if (manhattan <= idMath::FLT_SMALLEST_NON_DENORMAL) {
    return;
  }
  const float inverse = 1.0f / manhattan;
  const int xChannel = direction.x > 0.0f ? 0 : 1;
  const int yChannel = direction.y > 0.0f ? 2 : 3;
  const int zChannel = direction.z > 0.0f ? 4 : 5;
  const float xWeight = std::fabs(direction.x) * inverse;
  const float yWeight = std::fabs(direction.y) * inverse;
  const float zWeight = std::fabs(direction.z) * inverse;
  lighting.channelLighting[xChannel].x += color.x * xWeight;
  lighting.channelLighting[xChannel].y += color.y * xWeight;
  lighting.channelLighting[xChannel].z += color.z * xWeight;
  lighting.channelLighting[yChannel].x += color.x * yWeight;
  lighting.channelLighting[yChannel].y += color.y * yWeight;
  lighting.channelLighting[yChannel].z += color.z * yWeight;
  lighting.channelLighting[zChannel].x += color.x * zWeight;
  lighting.channelLighting[zChannel].y += color.y * zWeight;
  lighting.channelLighting[zChannel].z += color.z * zWeight;
}

ambientColors_t SampleAmbient(const approximateLightingParms_t& parms) {
  ambientColors_t ambient{};
  if (!parms.noAmbient) {
    parms.ambientMap->AmbientSample(parms.samplePoint, &ambient);
  }
  return ambient;
}

void BlendAmbientChannels(const approximateLightingParms_t& parms,
                          const ambientColors_t& ambient,
                          approximateLighting_t& lighting) {
  const float ambientScale = parms.ambientChannelScale * 0.0039215689f;
  for (int direction = 0; direction < 6; ++direction) {
    lighting.channelLighting[direction].x =
        static_cast<float>(ambient.channels[direction][0]) * ambientScale +
        parms.directChannelScale * lighting.channelLighting[direction].x;
    lighting.channelLighting[direction].y =
        static_cast<float>(ambient.channels[direction][1]) * ambientScale +
        parms.directChannelScale * lighting.channelLighting[direction].y;
    lighting.channelLighting[direction].z =
        static_cast<float>(ambient.channels[direction][2]) * ambientScale +
        parms.directChannelScale * lighting.channelLighting[direction].z;
  }
}

idVec3 ScaleDynamicColor(const approximateLightingParms_t& parms,
                         const idRenderLightCommitted& light,
                         const idVec3& color) {
  return {
      color.x * parms.dynamicLightingScale *
          light.r.dynamicModelLightingScale.x,
      color.y * parms.dynamicLightingScale *
          light.r.dynamicModelLightingScale.y,
      color.z * parms.dynamicLightingScale *
          light.r.dynamicModelLightingScale.z,
  };
}

}  // namespace

void AmbientOnlyLighting(const approximateLightingParms_t* parms,
                         approximateLighting_t* lighting) {
  lighting->lightingModel.x = 0.0f;
  const ambientColors_t ambient = SampleAmbient(*parms);
  lighting->primeLightDir = {0.0f, 0.0f, 0.0f, 0.0f};
  for (idVec4& color : lighting->lightColor) {
    color = {0.0f, 0.0f, 0.0f, 0.0f};
  }
  const float scale = parms->ambientChannelScale * 0.0039215689f;
  for (int direction = 0; direction < 6; ++direction) {
    lighting->channelLighting[direction].x =
        static_cast<float>(ambient.channels[direction][0]) * scale;
    lighting->channelLighting[direction].y =
        static_cast<float>(ambient.channels[direction][1]) * scale;
    lighting->channelLighting[direction].z =
        static_cast<float>(ambient.channels[direction][2]) * scale;
  }
}

float CalcSpotShadowFactor(const idRenderLightParms& lightParms,
                           const idVec3& dir) {
  const idVec3 target = lightParms.axis * lightParms.target;
  const idVec3 transformedUp = lightParms.axis * lightParms.up;
  const idVec3 transformedRight = lightParms.axis * lightParms.right;
  const float targetLength = SafeLength(target);
  const idVec3 targetNormal = target * (1.0f / targetLength);
  const float rayScale = targetLength / (dir * targetNormal);
  const idVec3 projected = dir * rayScale;
  const float upLength = SafeLength(transformedUp);
  const float rightLength = SafeLength(transformedRight);
  const float upCoordinate =
      (transformedUp * (1.0f / upLength) * projected) / upLength;
  const float rightCoordinate =
      (transformedRight * (1.0f / rightLength) * projected) / rightLength;
  const float radius =
      std::sqrt(std::max(rightCoordinate * rightCoordinate +
                             upCoordinate * upCoordinate,
                         idMath::FLT_SMALLEST_NON_DENORMAL));
  return std::fmin(1.0f, std::fmax(0.0f, 1.0f - radius * 4.0f));
}

bool CalculateLightContribution(
    const idODSObject<idRenderLightCommitted>& lightObject,
    const shadowMapCPU_t* shadowMap, const idVec3& samplePoint,
    idVec3& lightDir, idVec3& lightColor) {
  const idRenderLightCommitted& light = *lightObject.objectPtr;
  const idRenderMatrix& projection = light.baseLightProject;
  const float inverseW =
      1.0f /
      ((samplePoint.x * projection.m[12] +
        samplePoint.y * projection.m[13]) +
       samplePoint.z * projection.m[14] + projection.m[15]);
  const float projectedX =
      ((samplePoint.x * projection.m[0] +
        samplePoint.y * projection.m[1]) +
       samplePoint.z * projection.m[2] + projection.m[3]) *
      inverseW;
  const float projectedY =
      ((samplePoint.x * projection.m[4] +
        samplePoint.y * projection.m[5]) +
       samplePoint.z * projection.m[6] + projection.m[7]) *
      inverseW;
  const float projectedZ =
      ((samplePoint.x * projection.m[8] +
        samplePoint.y * projection.m[9]) +
       samplePoint.z * projection.m[10] + projection.m[11]) *
      inverseW;

  float factor = 1.0f;
  if (light.r.lightType == LIGHT_PARALLEL) {
    if (projectedX < 0.0f || projectedX > 1.0f || projectedY < 0.0f ||
        projectedY > 1.0f || projectedZ < 0.0f || projectedZ > 1.0f) {
      ClearContribution(lightDir, lightColor);
      return false;
    }
  } else {
    const float radial =
        ((projectedY - 0.5f) * (projectedY - 0.5f) +
         (projectedX - 0.5f) * (projectedX - 0.5f)) *
        4.0f;
    if (radial >= 1.0f) {
      ClearContribution(lightDir, lightColor);
      return false;
    }
    if (radial >= 0.25f) {
      factor = 1.0f - (std::sqrt(std::max(
                           radial, idMath::FLT_SMALLEST_NON_DENORMAL)) -
                       0.5f) *
                          2.0f;
    }
    if (light.r.lightType == LIGHT_SPOT) {
      if (projectedZ < 0.0f || projectedZ > 1.0f) {
        ClearContribution(lightDir, lightColor);
        return false;
      }
    } else if (light.r.lightType == LIGHT_POINT) {
      const float depth = std::fabs((projectedZ - 0.5f) * 2.0f);
      if (depth > 1.0f) {
        ClearContribution(lightDir, lightColor);
        return false;
      }
      if (depth >= 0.5f) {
        factor *= 1.0f - (depth - 0.5f) * 2.0f;
      }
    }
    if (factor <= 0.0f) {
      ClearContribution(lightDir, lightColor);
      return false;
    }
  }

  if (shadowMap != nullptr) {
    factor *= shadowMap->ShadowSample(samplePoint);
    if (factor == 0.0f) {
      ClearContribution(lightDir, lightColor);
      return false;
    }
  }

  if (light.r.lightType == LIGHT_PARALLEL) {
    lightDir = NormalizeRetail(light.r.lightCenter);
  } else {
    lightDir = NormalizeRetail(light.globalLightOrigin - samplePoint);
  }
  lightColor = light.currentColor * factor;
  return true;
}

void ApproximateLighting(const approximateLightingParms_t* parms,
                         approximateLighting_t* lighting) {
  lighting->lightingModel.x = 0.0f;
  idVec3 lightDirections[16];
  idVec3 lightColors[16];
  int directCount = 0;
  bool hasPrimeLight = false;

  for (std::uint32_t lightIndex = 0; lightIndex < parms->numLights;
       ++lightIndex) {
    const idRenderLightCommitted& light = *parms->lights[lightIndex];
    if (IsPrivateLightClass(light.r.lightClass) !=
            static_cast<bool>(parms->privateLightOnly) ||
        light.r.lightClass == LIGHT_PRELIGHT_ONLY ||
        light.r.lightFalloff == LIGHT_FALLOFF_RADIAL ||
        !MatchesLightGroup(light, *parms)) {
      continue;
    }

    idODSObject<idRenderLightCommitted> object{&light};
    idVec3 direction;
    idVec3 color;
    const shadowMapCPU_t* shadowMap =
        light.r.lightType == LIGHT_PARALLEL ? parms->shadowMap : nullptr;
    if (!CalculateLightContribution(object, shadowMap, parms->samplePoint,
                                    direction, color)) {
      continue;
    }
    color = ScaleDynamicColor(*parms, light, color);
    if (directCount >= 16) {
      continue;
    }
    if (parms->contributions != nullptr) {
      parms->contributions[directCount].light = &light;
      parms->contributions[directCount].color = color;
    }

    if (light.r.lightClass == LIGHT_PRIVATE_PRIME) {
      if (!hasPrimeLight || !light.r.noShadows) {
        lighting->primeLightDir.x = direction.x;
        lighting->primeLightDir.y = direction.y;
        lighting->primeLightDir.z = direction.z;
        lighting->lightColor[0].x = color.x;
        lighting->lightColor[0].y = color.y;
        lighting->lightColor[0].z = color.z;
        float shadowFactor = 1.0f;
        if (light.r.lightType == LIGHT_SPOT) {
          shadowFactor = CalcSpotShadowFactor(light.r, direction);
        }
        if (light.r.noShadows) {
          shadowFactor = 0.0f;
        }
        lighting->lightColor[0].w = 1.0f - shadowFactor;
        hasPrimeLight = true;
      }
      continue;
    }

    lightDirections[directCount] = direction;
    lightColors[directCount] = color;
    ++directCount;
    if (!light.r.noShadows && !hasPrimeLight) {
      const float stretch = SafeLength(color) * light.r.dimShadowStretch;
      lighting->primeLightDir.x += direction.x * stretch;
      lighting->primeLightDir.y += direction.y * stretch;
      lighting->primeLightDir.z += direction.z * stretch;
    }
  }

  if (hasPrimeLight) {
    for (int index = 0; index < directCount; ++index) {
      DistributeDirection(lightDirections[index], lightColors[index],
                          *lighting);
    }
  } else {
    if (lighting->primeLightDir.z < 0.0f) {
      lighting->primeLightDir.z = 0.0f;
    }
    lighting->primeLightDir.z += parms->dimShadowHeightTweak;
    const float inverseLength =
        1.0f /
        std::sqrt(std::max(
            lighting->primeLightDir.x * lighting->primeLightDir.x +
                lighting->primeLightDir.y * lighting->primeLightDir.y +
                lighting->primeLightDir.z * lighting->primeLightDir.z +
                lighting->primeLightDir.w * lighting->primeLightDir.w,
            idMath::FLT_SMALLEST_NON_DENORMAL));
    lighting->primeLightDir.x *= inverseLength;
    lighting->primeLightDir.y *= inverseLength;
    lighting->primeLightDir.z *= inverseLength;
    lighting->primeLightDir.w *= inverseLength;

    for (int index = 0; index < directCount; ++index) {
      const float primeFraction =
          lightDirections[index].x * lighting->primeLightDir.x +
          lightDirections[index].y * lighting->primeLightDir.y +
          lightDirections[index].z * lighting->primeLightDir.z;
      idVec3 channelColor = lightColors[index];
      if (primeFraction > 0.0f) {
        lighting->lightColor[0].x += lightColors[index].x * primeFraction;
        lighting->lightColor[0].y += lightColors[index].y * primeFraction;
        lighting->lightColor[0].z += lightColors[index].z * primeFraction;
        channelColor *= 1.0f - primeFraction;
      }
      DistributeDirection(lightDirections[index], channelColor, *lighting);
    }
  }

  lighting->lightColor[0].x *= parms->primeLightScale;
  lighting->lightColor[0].y *= parms->primeLightScale;
  lighting->lightColor[0].z *= parms->primeLightScale;
  lighting->lightColor[0].w *= parms->primeLightScale;
  const ambientColors_t ambient = SampleAmbient(*parms);
  BlendAmbientChannels(*parms, ambient, *lighting);
}

void LocalLighting(const approximateLightingParms_t* parms,
                   approximateLighting_t* lighting) {
  lighting->lightingModel.x = 1.0f;
  lighting->lightingModel.y = parms->noSelfShadow ? 0.0f : 1.0f;
  lighting->lightingModel.z =
      parms->noSelfShadow && !parms->lowQualityDimShadows ? 1.0f : 0.0f;

  contributingLight_t storage[16];
  contributingLight_t* contributing[16];
  int contributingCount = 0;
  for (std::uint32_t lightIndex = 0; lightIndex < parms->numLights;
       ++lightIndex) {
    const idRenderLightCommitted& light = *parms->lights[lightIndex];
    if (light.r.lightFalloff != LIGHT_FALLOFF_RADIAL ||
        !MatchesLightGroup(light, *parms)) {
      continue;
    }
    if (contributingCount >= 16) {
      break;
    }

    const idVec3 delta = parms->samplePoint - light.globalLightOrigin;
    const float distance = SafeLength(delta);
    if (distance - parms->sampleRadius > light.r.lightRadius.x) {
      continue;
    }
    contributingLight_t& entry = storage[contributingCount];
    entry.dir = delta * (1.0f / distance);
    entry.color = ScaleDynamicColor(*parms, light, light.currentColor);
    entry.falloff =
        distance /
        (parms->sampleRadius * 0.5f + light.r.lightRadius.x);
    entry.attenuation =
        ((entry.color.x + entry.color.y + entry.color.z) * 0.33333299f) *
        std::fmax(0.0f, 1.0f - entry.falloff);
    entry.index = static_cast<int>(lightIndex);
    contributing[contributingCount] = &entry;
    ++contributingCount;
  }

  for (int index = 1; index < contributingCount; ++index) {
    contributingLight_t* value = contributing[index];
    int position = index;
    while (position > 0 &&
           value->attenuation > contributing[position - 1]->attenuation) {
      contributing[position] = contributing[position - 1];
      --position;
    }
    contributing[position] = value;
  }

  const int directCount = std::min(contributingCount, 3);
  for (int index = 0; index < directCount; ++index) {
    const contributingLight_t& entry = *contributing[index];
    const idRenderLightCommitted& light = *parms->lights[entry.index];
    lighting->lightOrigin[index] = {
        light.globalLightOrigin.x, light.globalLightOrigin.y,
        light.globalLightOrigin.z, light.castsShadows ? 0.0f : 1.0f};
    lighting->lightColor[index] = {entry.color.x, entry.color.y, entry.color.z,
                                   light.r.lightRadius.x};
    if (parms->contributions != nullptr) {
      parms->contributions[index].light = &light;
      parms->contributions[index].color = entry.color;
    }
  }

  bool foundPrime = false;
  for (int index = 0; index < contributingCount; ++index) {
    const contributingLight_t& entry = *contributing[index];
    const idRenderLightCommitted& light = *parms->lights[entry.index];
    if (!light.r.noShadows && !foundPrime) {
      const float weight = std::fmax(0.0f, 1.0f - entry.falloff);
      const idVec3 direction = entry.dir * -weight;
      if (light.r.lightClass == LIGHT_PRIVATE_PRIME) {
        lighting->primeLightDir.x = direction.x;
        lighting->primeLightDir.y = direction.y;
        lighting->primeLightDir.z = direction.z;
        foundPrime = true;
      } else {
        lighting->primeLightDir.x += direction.x;
        lighting->primeLightDir.y += direction.y;
        lighting->primeLightDir.z += direction.z;
      }
    }
  }

  for (int index = 3; index < contributingCount; ++index) {
    const contributingLight_t& entry = *contributing[index];
    const idVec3 direction = -entry.dir;
    const float manhattan = std::fabs(direction.x) + std::fabs(direction.y) +
                            std::fabs(direction.z);
    if (manhattan <= idMath::FLT_SMALLEST_NON_DENORMAL) {
      continue;
    }
    float similarity = -1.0f;
    for (int top = 0; top < 3; ++top) {
      similarity = std::fmax(similarity, (-contributing[top]->dir) * direction);
    }
    const float falloff = std::fmax(0.0f, 1.0f - entry.falloff);
    idVec3 channelColor = entry.color * (falloff * falloff);
    if (similarity > 0.0f) {
      channelColor *= 1.0f - similarity;
    }
    DistributeDirection(direction, channelColor, *lighting);
  }

  for (int index = directCount; index < 3; ++index) {
    lighting->lightOrigin[index] = {0.0f, 0.0f, 0.0f, 1.0f};
    lighting->lightColor[index] = {0.0f, 0.0f, 0.0f, 1.0f};
  }

  const float inversePrimeLength =
      1.0f /
      std::sqrt(std::max(
          lighting->primeLightDir.x * lighting->primeLightDir.x +
              lighting->primeLightDir.y * lighting->primeLightDir.y +
              lighting->primeLightDir.z * lighting->primeLightDir.z,
          idMath::FLT_SMALLEST_NON_DENORMAL));
  lighting->primeLightDir.x *= inversePrimeLength;
  lighting->primeLightDir.y *= inversePrimeLength;
  lighting->primeLightDir.z *= inversePrimeLength;

  for (int index = 0; index < directCount; ++index) {
    lighting->lightColor[index].x *= parms->primeLightScale;
    lighting->lightColor[index].y *= parms->primeLightScale;
    lighting->lightColor[index].z *= parms->primeLightScale;
  }
  const ambientColors_t ambient = SampleAmbient(*parms);
  BlendAmbientChannels(*parms, ambient, *lighting);
  if (parms->noSelfShadow && directCount > 0) {
    for (int index = 0; index < directCount; ++index) {
      lighting->lightOrigin[index].w = 1.0f;
    }
  }
}

void ApproximateLightingJob(const approximateLightingParms_t* parms) {
  if (parms->contributions != nullptr) {
    for (int index = 0; index < 16; ++index) {
      parms->contributions[index].light = nullptr;
    }
  }
  approximateLighting_t lighting{};
  if (parms->noApproximateLighting) {
    AmbientOnlyLighting(parms, &lighting);
  } else if (parms->lightingModel == LM_LOCAL_LIGHTS) {
    LocalLighting(parms, &lighting);
  } else {
    ApproximateLighting(parms, &lighting);
  }
  std::memmove(parms->lighting, &lighting, sizeof(lighting));
}

const __m128 vector_float_abs_mask = _mm_castsi128_ps(
    _mm_setr_epi32(0x7FFFFFFF, 0x7FFFFFFF, 0x7FFFFFFF, 0x7FFFFFFF));
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

idParallelJobRegistration register_ApproximateLightingJob(
    reinterpret_cast<jobRun_t>(ApproximateLightingJob),
    "ApproximateLightingJob");
