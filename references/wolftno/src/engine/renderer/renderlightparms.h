#pragma once

#include "shared/idlib/geometry/rendermatrix.h"
#include "shared/idlib/math/vector.h"

#include <cstddef>

class idMaterial;

enum lightType_t {
  LIGHT_POINT,
  LIGHT_SPOT,
  LIGHT_PARALLEL,
  LIGHT_TYPE_MAX
};

enum lightClass_t {
  LIGHT_NORMAL,
  LIGHT_PRELIGHT_ONLY,
  LIGHT_SLOWLIGHT_DYNAMIC_ONLY,
  LIGHT_SLOWLIGHT_PRIVATE_ONLY,
  LIGHT_DYNAMIC_ONLY,
  LIGHT_PRIVATE_DYNAMIC_ONLY,
  LIGHT_PRIVATE_PRIME,
  LIGHT_BLENDED_ONLY
};

inline constexpr lightClass_t LIGHT_PRELIGHT = LIGHT_PRELIGHT_ONLY;
inline constexpr lightClass_t LIGHT_PRIVATE_PRIME_ONLY = LIGHT_PRIVATE_PRIME;

enum lightFallOff_t {
  LIGHT_FALLOFF_TEXTURE,
  LIGHT_FALLOFF_LINEAR,
  LIGHT_FALLOFF_INVERSE_SQUARE,
  LIGHT_FALLOFF_RADIAL
};

struct idRenderLightParms {
  idRenderLightParms();
  idRenderLightParms& operator=(const idRenderLightParms& other);

  idMat3 axis;
  idVec3 origin;
  int lightGroupID;
  int allowLightOnlyInViewID;
  int suppressLightInViewID;
  lightType_t lightType;
  lightClass_t lightClass;
  lightFallOff_t lightFalloff;
  bool noShadows;
  bool slowLight;
  const idMaterial* shader;
  idVec3 lightRadius;
  idVec3 lightCenter;
  idVec3 target;
  idVec3 right;
  idVec3 up;
  idVec3 start;
  idVec3 end;
  idVec3 dynamicModelLightingScale;
  float dimShadowStretch;
  float maxVisibleRange;
  float fadeVisibilityOver;
  float diffuseScale;
  float specularScale;
  float environmentScale;
  float eyeDiffuseScale;
  float eyeSpecularScale;
  float eyeEnvironmentScale;
  float staticSpecularScale;
  idVec3 staticSpecularVector;
  bool bakedParallelShadows;
  float areaLightSize;
  float shadowSampleScale;
  int slowMultiPass;
  bool forceTwoSidedShadows;

  const idMaterial* DefaultMaterial() const;
  void ComputeSpotLightProjectionMatrix(idRenderMatrix& localProject) const;
  void ComputeWorldToLightMatrix(idRenderMatrix& baseLightProject,
                                 idRenderMatrix& inverseBaseLightProject) const;
  void ComputeLightGlobalOrigin(idVec3& globalOrigin) const;
};

static_assert(offsetof(idRenderLightParms, lightType) == 0x3C,
              "idRenderLightParms lightType offset drift");
static_assert(offsetof(idRenderLightParms, shader) == 0x50,
              "idRenderLightParms shader offset drift");
static_assert(offsetof(idRenderLightParms, lightRadius) == 0x58,
              "idRenderLightParms lightRadius offset drift");
static_assert(offsetof(idRenderLightParms, target) == 0x70,
              "idRenderLightParms target offset drift");
static_assert(offsetof(idRenderLightParms, end) == 0xA0,
              "idRenderLightParms end offset drift");
static_assert(sizeof(idRenderLightParms) == 0x100,
              "idRenderLightParms ABI drift");
