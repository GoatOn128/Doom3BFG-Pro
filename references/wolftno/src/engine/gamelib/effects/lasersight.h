#pragma once

#include "engine/models/rendermodel.h"
#include "shared/idlib/math/vector.h"

#include <cstddef>
#include <cstdint>

class idMaterial;
class idRenderModelBeam;
class idRenderModelDecal;

class idLaserBeam {
public:
  __declspec(noinline) void Update(idRenderModelBeam* beamEffects,
                                   const idVec3& startPos,
                                   const idVec3& endPos, float fade);

  const idMaterial* material;
  float height;
  std::uint32_t recoveredPadding;
};

class idLaserEndPoint {
public:
  const idMaterial* material;
  float size;
  float depth;
  decalHandle_t handle;
  bool quad;
  std::uint8_t recoveredPadding[7];
};

class idLaserSight {
public:
  idLaserSight();
  ~idLaserSight();

  void Init(idRenderModelBeam* beamEffects,
            idRenderModelDecal* decalEffects,
            const idMaterial* laserBeamMaterial, float laserBeamHeight,
            const idMaterial* laserPointMaterial, float laserPointSize,
            float laserPointDepth, float fadeIn, float fadeOut, bool isQuad);
  void Shutdown();
  void Hide();
  void Show(unsigned startTime);
  void Update(const idVec3& startPos, const idVec3& endPos,
              const idMat3& axis, float sizeScale, float fade);

  idRenderModelBeam* beamEffects;
  idRenderModelDecal* decalEffects;
  idLaserBeam laserBeam;
  idLaserEndPoint laserEndPoint;
  float fadeInDuration;
  float fadeOutDuration;
  bool initialized;
  bool hidden;
  std::uint8_t recoveredPadding[6];
};

static_assert(sizeof(idLaserBeam) == 0x10, "idLaserBeam ABI drift");
static_assert(offsetof(idLaserBeam, material) == 0x00,
              "laser-beam material offset drift");
static_assert(offsetof(idLaserBeam, height) == 0x08,
              "laser-beam height offset drift");
static_assert(sizeof(idLaserEndPoint) == 0x20,
              "idLaserEndPoint ABI drift");
static_assert(offsetof(idLaserEndPoint, size) == 0x08,
              "laser endpoint size offset drift");
static_assert(offsetof(idLaserEndPoint, handle) == 0x10,
              "laser endpoint handle offset drift");
static_assert(offsetof(idLaserEndPoint, quad) == 0x18,
              "laser endpoint quad offset drift");
static_assert(sizeof(idLaserSight) == 0x50, "idLaserSight ABI drift");
static_assert(offsetof(idLaserSight, laserBeam) == 0x10,
              "laser sight beam offset drift");
static_assert(offsetof(idLaserSight, laserEndPoint) == 0x20,
              "laser sight endpoint offset drift");
static_assert(offsetof(idLaserSight, fadeInDuration) == 0x40,
              "laser sight fade-in offset drift");
static_assert(offsetof(idLaserSight, initialized) == 0x48,
              "laser sight initialized offset drift");
static_assert(offsetof(idLaserSight, hidden) == 0x49,
              "laser sight hidden offset drift");
