#pragma once

#include "engine/renderer/jobs/approximatelighting/ambientmapcpu.h"
#include "engine/renderer/jobs/approximatelighting/shadowmapcpu.h"
#include "shared/idlib/math/vector.h"

#include <cstddef>
#include <cstdint>

struct idRenderLightCommitted;
struct idRenderLightParms;
template <typename T> class idODSObject;

enum lightingModel_t : std::int32_t {
  LM_PRIME_AND_CHANNEL = 0,
  LM_LOCAL_LIGHTS = 1,
};

struct approximateLighting_t {
  void Clear();

  idVec4 lightingModel;
  idVec4 primeLightDir;
  idVec4 lightOrigin[3];
  idVec4 lightColor[3];
  idVec4 channelLighting[6];
};

struct lightContribution_t {
  const idRenderLightCommitted* light;
  idVec3 color;
};

struct approximateLightingParms_t {
  approximateLighting_t* lighting;
  lightContribution_t* contributions;
  const ambientMapCPU_t* ambientMap;
  const shadowMapCPU_t* shadowMap;
  const idRenderLightCommitted** lights;
  idVec3 samplePoint;
  float sampleRadius;
  int lightGroupID;
  lightingModel_t lightingModel;
  std::uint32_t numLights : 16;
  std::uint32_t noAmbient : 1;
  std::uint32_t noApproximateLighting : 1;
  std::uint32_t privateLightOnly : 1;
  std::uint32_t noSelfShadow : 1;
  std::uint32_t lowQualityDimShadows : 1;
  std::uint32_t unusedPaddingBits : 11;
  float ambientChannelScale;
  float directChannelScale;
  float primeLightScale;
  float dynamicLightingScale;
  float dimShadowHeightTweak;
};

struct contributingLight_t {
  idVec3 dir;
  idVec3 color;
  float attenuation;
  float falloff;
  int index;
};

void AmbientOnlyLighting(const approximateLightingParms_t* parms,
                         approximateLighting_t* lighting);
float CalcSpotShadowFactor(const idRenderLightParms& lightParms,
                           const idVec3& dir);
bool CalculateLightContribution(
    const idODSObject<idRenderLightCommitted>& light,
    const shadowMapCPU_t* shadowMap, const idVec3& samplePoint,
    idVec3& lightDir, idVec3& lightColor);
void ApproximateLighting(const approximateLightingParms_t* parms,
                         approximateLighting_t* lighting);
void LocalLighting(const approximateLightingParms_t* parms,
                   approximateLighting_t* lighting);
void ApproximateLightingJob(const approximateLightingParms_t* parms);

static_assert(sizeof(approximateLighting_t) == 0xE0,
              "approximate-lighting ABI drift");
static_assert(offsetof(approximateLighting_t, primeLightDir) == 0x10,
              "approximate-lighting prime-light offset drift");
static_assert(offsetof(approximateLighting_t, lightColor) == 0x50,
              "approximate-lighting light-color offset drift");
static_assert(offsetof(approximateLighting_t, channelLighting) == 0x80,
              "approximate-lighting channel offset drift");
static_assert(sizeof(lightContribution_t) == 0x18,
              "light-contribution ABI drift");
static_assert(offsetof(approximateLightingParms_t, samplePoint) == 0x28,
              "approximate-lighting sample-point offset drift");
static_assert(offsetof(approximateLightingParms_t, ambientChannelScale) ==
                  0x44,
              "approximate-lighting scale offset drift");
static_assert(sizeof(approximateLightingParms_t) == 0x58,
              "approximate-lighting parameters ABI drift");
static_assert(sizeof(contributingLight_t) == 0x24,
              "contributing-light ABI drift");
