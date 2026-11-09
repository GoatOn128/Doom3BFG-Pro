#pragma once

#include "shared/idlib/math/vector.h"
#include "shared/idlib/geometry/screenrect.h"

#include <cstddef>
#include <cstdint>

enum modelDepthSort_t : int {
  MODEL_DEPTH_SORT_FIRST = -6,
  MODEL_DEPTH_SORT_PORTAL_INSIDE = -5,
  MODEL_DEPTH_SORT_PORTAL_PLANE = -4,
  MODEL_DEPTH_SORT_VIEW_HEAD = -3,
  MODEL_DEPTH_SORT_VIEW_BODY = -2,
  MODEL_DEPTH_SORT_SCENE_MODEL = -1,
  MODEL_DEPTH_SORT_NORMAL = 0,
  MODEL_DEPTH_SORT_AFTER_WORLD = 1,
  MODEL_DEPTH_SORT_SKY = 8,
  MODEL_DEPTH_SORT_TRANSPARENT = 9,
  MODEL_DEPTH_SORT_LAST = 10,
};

enum dimShadow_t : int {
  DIMSHADOW_OFF = 0,
  DIMSHADOW_ON = 1,
  DIMSHADOW_HIGH_QUALITY = 2,
  DIM_SHADOW_DEFAULT = DIMSHADOW_OFF,
};

enum highlightColor_t : int {
  HIGHLIGHT_COLOR_NONE = 0,
  HIGHLIGHT_COLOR_CONSTANT = 1,
  HIGHLIGHT_COLOR_USE = 2,
  HIGHLIGHT_COLOR_ITEM = 3,
  HIGHLIGHT_COLOR_LOOT = 4,
  HIGHLIGHT_COLOR_CORPSE = 5,
  HIGHLIGHT_COLOR_EXPLODE = 6,
  HIGHLIGHT_COLOR_MAXED_HEALTH = 7,
  HIGHLIGHT_COLOR_MAXED_AMMO = 8,
  HIGHLIGHT_COLOR_MAX = 9,
};

// PDB-exact renderer model parameters. Flag bytes retain the packed retail
// representation because renderer jobs test those bits directly.
struct idRenderModelParms {
  idRenderModelParms();
  idRenderModelParms& operator=(const idRenderModelParms& other);

  idVec3 origin;
  idMat3 axis;
  idVec3 scale;
  std::uint8_t alignmentPadding[4];
  const void* customMaterial;
  int customSkin;
  int groupMasterIndex;
  int lightGroupID;
  int allowSurfaceOnlyInViewID;
  int suppressSurfaceInViewID;
  int suppressShadowInViewID;
  int suppressShadowInLightIndex;
  int firstVisibleFrameCount;
  float fovScale;
  float modelDepthHack;
  dimShadow_t castDimShadows;
  std::uint8_t flags0;
  std::uint8_t flags1;
  std::uint8_t flags2;
  std::uint8_t flags3;
  std::uint8_t flags4;
  std::uint8_t flagPadding[3];
  std::int16_t traceGroup;
  std::uint8_t alignmentBeforeRadiosity[2];
  float radiosityScale;
  float staticReflectionStrength;
  highlightColor_t highlightColor;
  float dimShadowClipZ;
  float maxTexelDensity;
  float modelFade;
  float maxVisibleRange;
  float fadeVisibilityOver;
  idVec3 localLightPositionOverride;
  modelDepthSort_t modelDepthSort;
  const void* referenceModel;
  void* imposterParent;
  void* imposterNext;
  idScreenRect viewport;

  bool MvpIsIdentity() const { return (flags1 & 0x80u) != 0; }
  bool MvpIsOrthographic() const { return (flags2 & 0x01u) != 0; }
};

static_assert(offsetof(idRenderModelParms, fovScale) == 0x68,
              "render-model fov scale offset drift");
static_assert(offsetof(idRenderModelParms, flags1) == 0x75,
              "render-model identity flags offset drift");
static_assert(offsetof(idRenderModelParms, modelFade) == 0x94,
              "render-model fade offset drift");
static_assert(offsetof(idRenderModelParms, modelDepthSort) == 0xAC,
              "render-model depth-sort offset drift");
static_assert(offsetof(idRenderModelParms, imposterParent) == 0xB8,
              "render-model imposter-parent offset drift");
static_assert(sizeof(idRenderModelParms) == 0xD8,
              "render-model parameters ABI drift");
