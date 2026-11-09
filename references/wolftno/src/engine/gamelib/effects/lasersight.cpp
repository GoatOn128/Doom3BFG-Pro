#include "engine/gamelib/effects/lasersight.h"

#include "engine/models/rendermodelbeam.h"
#pragma warning(push)
#pragma warning(disable : 4201)
#include "engine/models/rendermodeldecal.h"
#pragma warning(pop)

#include <cstdint>

#pragma comment(                                                               \
    linker,                                                                    \
    "/alternatename:?DrawBeam@idRenderModelBeam@@QEAAXAEAUbeamNodeParms_t@@" \
    "PEBVidMaterial@@W4beamOrientType_t@@@Z="                                  \
    "?DrawBeam@idRenderModelBeam@@QEAAXPEAUbeamNodeParms_t@@"                 \
    "PEBVidMaterial@@W4beamOrientType_t@@@Z")

namespace {

using removeDecal_t = bool(__fastcall*)(idRenderModelDecal*, decalHandle_t);
using addDecalFromPoint_t = decalHandle_t*(__fastcall*)(
    idRenderModelDecal*, decalHandle_t*, const decalParams_t*, unsigned,
    const idVec3*, const idVec3*, renderJointIndex_t);

void RemoveDecalRetail(idRenderModelDecal* model, decalHandle_t handle) {
  auto** const vtable = *reinterpret_cast<void***>(model);
  reinterpret_cast<removeDecal_t>(vtable[5])(model, handle);
}

decalHandle_t AddDecalFromPointRetail(idRenderModelDecal* model,
                                      const decalParams_t* params,
                                      unsigned startTime) {
  auto** const vtable = *reinterpret_cast<void***>(model);
  const auto addDecal =
      reinterpret_cast<addDecalFromPoint_t>(vtable[4]);
  alignas(decalHandle_t) std::uint8_t resultStorage[sizeof(decalHandle_t)];
  auto* const result = reinterpret_cast<decalHandle_t*>(resultStorage);
  const decalHandle_t* const returned =
      addDecal(model, result, params, startTime, &vec3_origin, &vec3_origin,
               renderJointIndex_t{-1});
  return *returned;
}

std::uint8_t FadeByte(float fade) {
  int value = static_cast<int>(fade * 255.89999f);
  if (value < 0) {
    value = 0;
  } else if (value > 255) {
    value = 255;
  }
  return static_cast<std::uint8_t>(value);
}

} // namespace

idLaserSight::idLaserSight() {
  beamEffects = nullptr;
  decalEffects = nullptr;
  laserBeam.material = nullptr;
  laserBeam.height = 1.0f;
  laserEndPoint.material = nullptr;
  laserEndPoint.size = 1.0f;
  laserEndPoint.depth = 8.0f;
  laserEndPoint.handle = decalHandle_t{-1, -1};
  laserEndPoint.quad = false;
  initialized = false;
  hidden = false;
  fadeInDuration = 1.0f;
  fadeOutDuration = 1.0f;
}

idLaserSight::~idLaserSight() {
  initialized = false;
  if (decalEffects != nullptr && laserEndPoint.handle.decalIndex != -1) {
    RemoveDecalRetail(decalEffects, laserEndPoint.handle);
  }
  beamEffects = nullptr;
  decalEffects = nullptr;
}

void idLaserSight::Init(idRenderModelBeam* beamEffects_,
                        idRenderModelDecal* decalEffects_,
                        const idMaterial* laserBeamMaterial,
                        float laserBeamHeight,
                        const idMaterial* laserPointMaterial,
                        float laserPointSize, float laserPointDepth,
                        float fadeIn, float fadeOut, bool isQuad) {
  laserEndPoint.material = laserPointMaterial;
  beamEffects = beamEffects_;
  laserBeam.height = laserBeamHeight;
  laserEndPoint.depth = laserPointDepth;
  decalEffects = decalEffects_;
  laserBeam.material = laserBeamMaterial;
  initialized = true;
  hidden = false;
  laserEndPoint.size = laserPointSize;
  laserEndPoint.quad = isQuad;
  fadeInDuration = fadeIn;
  fadeOutDuration = fadeOut;
}

void idLaserSight::Shutdown() {
  idRenderModelDecal* const currentDecalEffects = decalEffects;
  initialized = false;
  if (currentDecalEffects != nullptr &&
      laserEndPoint.handle.decalIndex != -1) {
    RemoveDecalRetail(currentDecalEffects, laserEndPoint.handle);
  }
}

void idLaserSight::Hide() {
  if (initialized && !hidden) {
    hidden = true;
    idRenderModelDecal* const currentDecalEffects = decalEffects;
    if (currentDecalEffects != nullptr) {
      if (laserEndPoint.handle.decalIndex != -1) {
        RemoveDecalRetail(currentDecalEffects, laserEndPoint.handle);
      }
      laserEndPoint.handle = decalHandle_t{-1, -1};
    }
  }
}

void idLaserSight::Show(unsigned startTime) {
  if (initialized && hidden) {
    hidden = false;
    idRenderModelDecal* const currentDecalEffects = decalEffects;
    if (currentDecalEffects != nullptr) {
      alignas(decalParams_t)
          std::uint8_t parameterStorage[sizeof(decalParams_t)];
      auto* const params =
          reinterpret_cast<decalParams_t*>(parameterStorage);
      params->lifeTime = 10000;
      params->fadeOutTime = 2000;
      params->fadeInTime = 0;
      params->quad = false;
      params->overlap = false;
      params->cost = 1;
      params->depth = 8.0f;
      params->size = idVec2{32.0f, 32.0f};
      params->angle = 0.0f;
      params->overlapRadius = 32.0f;
      params->decalMat = laserEndPoint.material;
      if (params->decalMat != nullptr) {
        params->quad = laserEndPoint.quad;
        params->lifeTime = -1;
        params->depth = laserEndPoint.depth;
        params->size.x = laserEndPoint.size;
        params->overlap = true;
        params->size.y = laserEndPoint.size;
        params->angle = 0.0099999998f;
        params->overlapRadius = 0.0f;
        laserEndPoint.handle =
            AddDecalFromPointRetail(currentDecalEffects, params, startTime);
      }
    }
  }
}

void idLaserBeam::Update(idRenderModelBeam* beamEffects,
                         const idVec3& startPos, const idVec3& endPos,
                         float fade) {
  if (beamEffects == nullptr) {
    return;
  }

  alignas(beamNodeParms_t)
      std::uint8_t nodeStorage[sizeof(beamNodeParms_t)];
  auto& startNode = *reinterpret_cast<beamNodeParms_t*>(nodeStorage);
  const std::uint8_t alpha = FadeByte(fade);
  startNode.startPos = startPos;
  startNode.endPos = endPos;
  startNode.orientVec = vec3_origin;
  startNode.sMinMax = idVec2{0.0f, 1.0f};
  startNode.tMinMax = idVec2{0.0f, 1.0f};
  startNode.halfWidth = height * 0.5f;
  for (int end = 0; end < 2; ++end) {
    startNode.color[end][0] = 255;
    startNode.color[end][1] = 255;
    startNode.color[end][2] = 255;
    startNode.color[end][3] = alpha;
  }
  for (int tangent = 0; tangent < 4; ++tangent) {
    startNode.tangent[tangent][0] = 128;
    startNode.tangent[tangent][1] = 128;
    startNode.tangent[tangent][2] = 128;
  }
  beamEffects->DrawBeam(startNode, material, BEAM_ORIENT_VIEWER);
}

void idLaserSight::Update(const idVec3& startPos, const idVec3& endPos,
                          const idMat3& axis, float sizeScale, float fade) {
  if (!hidden) {
    laserBeam.Update(beamEffects, startPos, endPos, fade);
    if (decalEffects != nullptr) {
      decalEffects->SetDecalAttributes(laserEndPoint.handle, endPos, axis,
                                       sizeScale * fade);
    }
  }
}
