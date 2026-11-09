#include "engine/models/lod/rendermodellod.h"

#include "engine/decls/declsimple.h"
#include "engine/renderer/autorenderparms.h"
#include "engine/renderer/jobs/render/renderocclusion.h"
#include "engine/renderer/render.h"
#include "shared/idlib/csystems/cvarsystemlocal.h"
#include "shared/idlib/lib.h"

#include <algorithm>
#include <cmath>
#include <cstdint>
#include <cstring>
#include <new>

idCVar r_forceLODIndex(
    "r_forceLODIndex", "-1", CVAR_INTEGER,
    "Force all LODed render models to specific index");
idCVar r_keepLowestLOD("r_keepLowestLOD", "0", CVAR_BOOL,
                       "Force system to never hide lowest lod");
idCVar r_distanceScaleLOD("r_distanceScaleLOD", "1.0", CVAR_FLOAT,
                          "Scale all distance by factor");
idCVar r_fadeScaleLOD("r_fadeScaleLOD", "1.0", CVAR_FLOAT,
                      "Scale all lod fade times by factor");

namespace {

std::uint8_t QuantizeFade(float fade) {
  const int quantized = static_cast<int>(fade * 255.5f);
  if (quantized < 0)
    return 0;
  if (quantized > 255)
    return 255;
  return static_cast<std::uint8_t>(quantized);
}

float DistanceSqrToBounds(const idVec3& point, const idBounds& bounds) {
  if (point.x >= bounds.b[0].x && point.x <= bounds.b[1].x &&
      point.y >= bounds.b[0].y && point.y <= bounds.b[1].y &&
      point.z >= bounds.b[0].z && point.z <= bounds.b[1].z) {
    return 0.0f;
  }

  const float x = std::clamp(point.x, bounds.b[0].x, bounds.b[1].x);
  const float y = std::clamp(point.y, bounds.b[0].y, bounds.b[1].y);
  const float z = std::clamp(point.z, bounds.b[0].z, bounds.b[1].z);
  const float dx = point.x - x;
  const float dy = point.y - y;
  const float dz = point.z - z;
  return dx * dx + dy * dy + dz * dz;
}

idVec3 TransformCameraToModelSpace(const idRenderView& view,
                                   const idVec3& origin,
                                   const idMat3& axis) {
  const idVec3 cameraWorld{view.inverseViewMatrix.m[3],
                           view.inverseViewMatrix.m[7],
                           view.inverseViewMatrix.m[11]};
  const idVec3 delta{cameraWorld.x - origin.x, cameraWorld.y - origin.y,
                     cameraWorld.z - origin.z};
  return {
      delta.x * axis.mat[0].x + delta.y * axis.mat[0].y +
          delta.z * axis.mat[0].z,
      delta.x * axis.mat[1].x + delta.y * axis.mat[1].y +
          delta.z * axis.mat[1].z,
      delta.x * axis.mat[2].x + delta.y * axis.mat[2].y +
          delta.z * axis.mat[2].z,
  };
}

} // namespace

void idRenderModelLOD::UnlinkSubclass() {
  for (int lodIndex = 0; lodIndex < LODs.Num(); ++lodIndex) {
    lod_t& lod = LODs[lodIndex];
    for (int modelIndex = 0; modelIndex < lod.models.Num(); ++modelIndex) {
      idRenderModel* model = lod.models[modelIndex];
      if (model != nullptr && (model->unlinked & 8U) == 0U) {
        model->CommitThisFrame();
        model->UnlinkSubclass();
        model->unlinked |= 8U;
      }
    }
    lod.visible = false;
  }
}

bool idRenderModelLOD::UpdateInView(
    const idRenderView* currentView, const idRenderView* nextView,
    idRenderModelUpdateTools* updateTools) {
  (void)updateTools;

  if (currentLOD != -1 &&
      (currentLOD < 0 || currentLOD >= LODs.Num() ||
       LODs[currentLOD].models.Num() <= 0)) {
    return false;
  }

  idRenderModel* visibilityModel =
      currentLOD == -1 ? static_cast<idRenderModel*>(this)
                       : LODs[currentLOD].models[0];
  const int modelIndex = visibilityModel->GetIndex();
  const occlusionState_t* occlusionState = currentView->occlusionState;
  const int visibleFrameCount =
      occlusionState->modelVisibleFrameNumber[modelIndex] <
              occlusionState->renderFrameNumber
          ? 0
          : occlusionState->modelVisibleFrameCount[modelIndex];
  const bool visible =
      visibleFrameCount >= visibilityModel->g.firstVisibleFrameCount ||
      forceVis > 0;
  if (forceVis > 0)
    --forceVis;

  const float inverseFadeTime = 1.0f / static_cast<float>(fadeTime);
  const int renderTime = currentView->r.renderTime;
  int retainedLOD = -1;

  if (visible) {
    const bool deferred = (useDeferredPosition & 1U) != 0U;
    const idVec3& origin = deferred ? deferredOrigin : g.origin;
    const idMat3& axis = deferred ? deferredAxis : g.axis;
    const idVec3 cameraMS =
        TransformCameraToModelSpace(*nextView, origin, axis);
    const float distanceSqr = DistanceSqrToBounds(cameraMS, referenceBounds);

    const int numLODs = LODs.Num();
    newLOD = -1;
    for (int lodIndex = 0; lodIndex < numLODs; ++lodIndex) {
      if (r_distanceScaleLOD.GetFloat() * LODs[lodIndex].maxDistanceSqr >
          distanceSqr) {
        newLOD = lodIndex;
        break;
      }
    }
    if (r_keepLowestLOD.GetInteger() != 0 && newLOD == -1 && numLODs > 0)
      newLOD = numLODs - 1;
    if (r_forceLODIndex.GetInteger() != -1) {
      const int forcedLOD = r_forceLODIndex.GetInteger();
      newLOD = forcedLOD >= 0 && forcedLOD < numLODs ? forcedLOD : -1;
    }

    const std::int16_t traceGroup = g.traceGroup;
    const idVec3 propagatedOrigin = origin;
    const idMat3 propagatedAxis = axis;
    const idVec3 propagatedScale = g.scale;
    std::uint8_t currentFade = 255;
    if (currentLOD != -1) {
      currentFade = QuantizeFade(
          static_cast<float>(renderTime - fadeTimestamp) * inverseFadeTime +
          LODs[currentLOD].fade);
    }

    for (int lodIndex = 0; lodIndex < numLODs; ++lodIndex) {
      lod_t& lod = LODs[lodIndex];
      if (lodIndex == currentLOD) {
        lod.fade = static_cast<float>(
            QuantizeFade(static_cast<float>(currentFade) * 255.5f));
      } else {
        const float fadeDelta =
            static_cast<float>(renderTime - fadeTimestamp) * inverseFadeTime;
        if (lodIndex == keepLOD && currentFade != 255)
          lod.fade = std::min(1.0f, lod.fade + fadeDelta);
        else
          lod.fade = std::max(0.0f, lod.fade - fadeDelta);
      }

      const std::uint8_t quantizedFade = QuantizeFade(lod.fade);
      lod.visible = quantizedFade != 0;
      for (int modelNumber = 0; modelNumber < lod.models.Num(); ++modelNumber) {
        idRenderModel* model = lod.models[modelNumber];
        if (!lod.visible) {
          if ((model->unlinked & 8U) == 0U) {
            model->CommitThisFrame();
            model->UnlinkSubclass();
            model->unlinked |= 8U;
          }
          continue;
        }

        if ((model->useDeferredPosition &
             (model->deferredPositionInitialized >> 1U) & 1U) == 0U) {
          model->g.origin = propagatedOrigin;
          model->g.axis = propagatedAxis;
        }
        model->deferredOrigin = propagatedOrigin;
        model->deferredAxis = propagatedAxis;
        model->g.scale = propagatedScale;

        const float modelFade = std::clamp(
            static_cast<float>(quantizedFade) * 0.0039220001f, 0.0f, 1.0f);
        if ((model->g.flags4 & 4U) == 0U) {
          model->g.modelFade = modelFade;
          const idDeclRenderParm* fadeParm = rp_modelFade.Get();
          if (modelFade == 1.0f) {
            model->g.flags2 &= static_cast<std::uint8_t>(~0x20U);
            if (fadeParm != nullptr)
              model->gameParmBlock.ClearParm(fadeParm);
          } else {
            model->g.flags2 |= 0x20U;
            if (fadeParm != nullptr) {
              model->gameParmBlock.SetParm(
                  fadeParm, idVec4{modelFade, 1.0f, 0.0f, 0.0f});
            }
          }
        }

        model->g.traceGroup = traceGroup;
        if ((g.flags1 & 0x10U) != 0U &&
            (model->g.flags1 & 0x10U) != 0U) {
          model->CommitThisFrame();
          continue;
        }
        if ((g.flags1 & 0x10U) == 0U)
          model->g.flags1 &= static_cast<std::uint8_t>(~0x10U);
        if ((g.flags1 & 0x02U) != 0U || (g.flags0 & 0x80U) != 0U)
          model->g.castDimShadows = DIMSHADOW_OFF;
        else
          model->g.castDimShadows = lod.dimShadows;
        model->CommitThisFrame();
      }
    }

    prevParm.origin = propagatedOrigin;
    prevParm.scale = propagatedScale;
    prevParm.axis = propagatedAxis;
  }

  if (newLOD != currentLOD) {
    if (newLOD == -1) {
      g.flags1 &= static_cast<std::uint8_t>(~0x20U);
      g.flags2 |= 0x60U;
    } else {
      g.flags1 |= 0x20U;
      g.flags2 &= static_cast<std::uint8_t>(~0x60U);
    }
    fadeTimestamp = renderTime;
    if (newLOD != -1)
      retainedLOD = currentLOD;
    currentLOD = newLOD;
    forceVis = forceVisFrameCount;
    keepLOD = retainedLOD;
  }
  return true;
}

void idRenderModelLOD::DeleteSubclass() {
  for (int lodIndex = 0; lodIndex < LODs.Num(); ++lodIndex) {
    lod_t& lod = LODs[lodIndex];
    for (int modelIndex = 0; modelIndex < lod.models.Num(); ++modelIndex) {
      idRenderModel* model = lod.models[modelIndex];
      if (model == nullptr)
        continue;
      model->CommitThisFrame();
      if ((model->unlinked & 8U) == 0U) {
        model->CommitThisFrame();
        model->UnlinkSubclass();
        model->unlinked |= 8U;
      }
      model->DeleteSubclass();
      model->deleteOnSync |= 4U;
    }
    lod.models.ClearFree();
  }
  LODs.ClearFree();
}

idRenderModelLOD::idRenderModelLOD(const idDeclModelLOD* decl_,
                                   const idList<idRenderModel*>& models)
    : decl(decl_) {
  renderModelType = RENDERMODEL_LOD;
  if (decl == nullptr)
    idLib::Error("Invalid model LOD decl <NULL>\n");
  if (decl->LODs.Num() <= 0)
    idLib::Error("Zero LODs in model LOD decl: %s\n", decl->name.str);

  SetName(decl->name.str);
  fadeTime = static_cast<int>(std::max(
      1.0f, static_cast<float>(static_cast<int>(
                r_fadeScaleLOD.GetFloat() * decl->fade * 1000.0f))));
  forceVis = g.firstVisibleFrameCount;
  LODs.SetNum(decl->LODs.Num());

  idBounds bounds;
  bounds.Clear();
  int sourceModel = 0;
  for (int lodIndex = 0; lodIndex < LODs.Num(); ++lodIndex) {
    const modelLODLevel_t& declLOD = decl->LODs[lodIndex];
    lod_t& lod = LODs[lodIndex];
    lod.models.SetNum(declLOD.models.Num());
    for (int modelIndex = 0; modelIndex < lod.models.Num(); ++modelIndex) {
      idRenderModel* model = models[sourceModel++];
      lod.models[modelIndex] = model;
      if (declLOD.models[modelIndex].megatexturedModel) {
        model->g.flags1 |= 0x10U;
        model->g.castDimShadows = DIMSHADOW_OFF;
      } else {
        model->g.castDimShadows =
            static_cast<dimShadow_t>(declLOD.dimShadows);
      }
      bounds.AddBounds(model->referenceBounds);
      if ((model->unlinked & 8U) == 0U) {
        model->CommitThisFrame();
        model->UnlinkSubclass();
        model->unlinked |= 8U;
      }
      forceVisFrameCount =
          std::max(forceVisFrameCount, model->g.firstVisibleFrameCount);
    }
    lod.maxDistanceSqr = declLOD.distance * declLOD.distance;
    lod.dimShadows = static_cast<dimShadow_t>(declLOD.dimShadows);
  }

  currentLOD = -1;
  keepLOD = -1;
  newLOD = LODs.Num() - 1;
  LODs[LODs.Num() - 1].fade = 1.0f;
  forceVis = forceVisFrameCount;
  fadeTimestamp = 0;

  auto* geometry = new (std::nothrow) idTriangles{};
  if (geometry == nullptr)
    idLib::Error("Unable to allocate model LOD bounds geometry\n");
  geometry->bounds = bounds;
  idRenderModelSurface dummySurface{};
  dummySurface.geometry = geometry;
  dummySurface.material = nullptr;
  AddSurface(dummySurface);
  FinishSurfaces();
  referenceBounds = bounds;
  g.flags2 |= 0x60U;
  g.flags1 |= 0x21U;
  std::memset(&prevParm, 0, sizeof(prevParm));
}

idRenderModelLOD::~idRenderModelLOD() = default;
