#include "engine/renderer/rendermodelcommitted.h"

#include "engine/models/renderworld.h"
#include "engine/renderer/declmaterial.h"
#include "shared/idlib/color.h"
#include "shared/idlib/lib.h"

#include <algorithm>
#include <cmath>

void idRenderModelCommitted::LocalPointToGlobal(const idVec3& in,
                                                idVec3& out) const {
  out.x = (modelMatrix.m[1] * in.y + modelMatrix.m[0] * in.x) +
          modelMatrix.m[2] * in.z + modelMatrix.m[3];
  out.y = (modelMatrix.m[4] * in.x + modelMatrix.m[5] * in.y) +
          modelMatrix.m[6] * in.z + modelMatrix.m[7];
  out.z = (modelMatrix.m[8] * in.x + modelMatrix.m[9] * in.y) +
          modelMatrix.m[10] * in.z + modelMatrix.m[11];
}

void idRenderWorldCommitted::LinkApproximateLighting(
    idRenderModelCommitted* model) {
  if (model->nextApproxLightModel != model)
    return;

  idRenderModelCommitted* current = approxLightModels;
  if (current == nullptr || (model->r.flags3 & 0x01u) != 0u) {
    model->nextApproxLightModel = current;
    approxLightModels = model;
    return;
  }

  idRenderModelCommitted** link = &current->nextApproxLightModel;
  while (*link != nullptr) {
    if ((current->r.flags3 & 0x01u) == 0u)
      break;
    current = *link;
    link = &current->nextApproxLightModel;
  }
  model->nextApproxLightModel = current->nextApproxLightModel;
  current->nextApproxLightModel = model;
}

void idRenderModelCommitted::SetFlagsFromSurfaces() {
  committedFlags0 &= 0x03u;
  committedFlags1 &= 0xf6u;

  for (int surfaceIndex = 0; surfaceIndex < surfaces.num; ++surfaceIndex) {
    const idRenderModelSurface& surface = surfaces.list[surfaceIndex];
    const idMaterial* material = surface.material;
    if (material == nullptr)
      continue;

    if (material->coverage != MC_INVISIBLE)
      committedFlags0 |= 0x08u;
    if (material->coverage == MC_OPAQUE ||
        material->coverage == MC_PERFORATED_FOR_MODELFADE)
      committedFlags1 |= 0x01u;
    if ((material->flags0 & 0x01u) != 0u)
      committedFlags0 |= 0x10u;
    if (material->progs[MPR_INTERACTION] != nullptr)
      committedFlags0 |= 0x20u;
    if (material->pageFlavor == ID_VTF_BAKE)
      committedFlags0 |= 0x80u;
    if ((material->flags0 & 0x20u) != 0u)
      committedFlags0 |= 0x04u;
    if (material->progs[MPR_SSS] != nullptr)
      committedFlags0 |= 0x40u;
    if ((material->flags1 & 0x10u) != 0u)
      committedFlags1 |= 0x08u;
  }

  if ((r.flags0 & 0x04u) != 0u && world->isComboMap)
    committedFlags0 &= static_cast<std::uint8_t>(~0x04u);
  if ((committedFlags1 & 0x04u) != 0u)
    committedFlags0 &= static_cast<std::uint8_t>(~0x04u);
  if ((committedFlags1 & 0x02u) != 0u)
    committedFlags0 |= 0x04u;
}

void idRenderModelCommitted::PostCommit() {
  if ((committedFlags0 & 0x01u) == 0u) {
    committedFlags0 |= 0x01u;
    localLightingPoint = (referenceBounds.b[0] + referenceBounds.b[1]) * 0.5f;
    if ((r.flags3 & 0x10u) != 0u) {
      localLightingPoint.z +=
          (referenceBounds.b[1].z - referenceBounds.b[0].z) * 0.25f;
    }
    localLightingRadius = referenceBounds.GetRadius(localLightingPoint);
  }

  if ((r.flags1 & 0x20u) != 0u) {
    world->UnlinkModelFromTree(this);

    if (nextDimShadowModel != this) {
      idRenderModelCommitted** link = &world->dimShadowModels;
      while (*link != nullptr && *link != this)
        link = &(*link)->nextDimShadowModel;
      if (*link == this)
        *link = nextDimShadowModel;
      nextDimShadowModel = this;
    }

    if (nextAddAlwaysModel == this) {
      nextAddAlwaysModel = world->addAlwaysModels;
      world->addAlwaysModels = this;
    }
    return;
  }

  if (nextAddAlwaysModel != this) {
    idRenderModelCommitted** link = &world->addAlwaysModels;
    while (*link != nullptr && *link != this)
      link = &(*link)->nextAddAlwaysModel;
    if (*link == this)
      *link = nextAddAlwaysModel;
    nextAddAlwaysModel = this;
  }

  if ((committedFlags0 & 0x02u) != 0u) {
    world->UnlinkModelFromTree(this);
    if (referenceBounds.IsZero() ||
        referenceBounds.b[0].x > referenceBounds.b[1].x) {
      globalReferenceBounds.b[0] = r.origin;
      globalReferenceBounds.b[1] = r.origin;

      if (nextDimShadowModel != this) {
        idRenderModelCommitted** link = &world->dimShadowModels;
        while (*link != nullptr && *link != this)
          link = &(*link)->nextDimShadowModel;
        if (*link == this)
          *link = nextDimShadowModel;
        nextDimShadowModel = this;
      }
    } else {
      idRenderMatrix inverseBaseModelProject;
      idRenderMatrix::OffsetScaleForBounds(modelMatrix, referenceBounds,
                                           inverseBaseModelProject);
      idRenderMatrix::ProjectedBounds(globalReferenceBounds,
                                      inverseBaseModelProject,
                                      bounds_unitCube, false);
      world->LinkModelIntoTree(this, inverseBaseModelProject,
                               bounds_unitCube);

      if ((r.flags0 & 0x04u) == 0u &&
          (committedFlags0 & 0x10u) != 0u &&
          (r.flags0 & 0x80u) == 0u &&
          r.castDimShadows != DIMSHADOW_OFF) {
        if (nextDimShadowModel == this) {
          nextDimShadowModel = world->dimShadowModels;
          world->dimShadowModels = this;
        }
      } else {
        world->UnlinkDimShadow(this);
      }

      if (r.maxVisibleRange > 0.0f) {
        const idVec3 center =
            (globalReferenceBounds.b[0] + globalReferenceBounds.b[1]) * 0.5f;
        float radius = 0.0f;
        for (int bound = 0; bound < 2; ++bound) {
          for (int component = 0; component < 3; ++component) {
            radius = std::fmax(
                radius,
                globalReferenceBounds.b[bound][component] - center[component]);
          }
        }
        maxVisibleRange = {center.x, center.y, center.z,
                           radius + r.maxVisibleRange};
        committedFlags0 &= static_cast<std::uint8_t>(~0x02u);
        if ((committedFlags0 & 0x04u) != 0u) {
          world->LinkApproximateLighting(this);
        } else if (nextApproxLightModel != this) {
          idRenderModelCommitted** link = &world->approxLightModels;
          while (*link != nullptr && *link != this)
            link = &(*link)->nextApproxLightModel;
          if (*link == this)
            *link = nextApproxLightModel;
          nextApproxLightModel = this;
        }
        return;
      }
    }

    maxVisibleRange = {0.0f, 0.0f, 0.0f, 262144.0f};
    committedFlags0 &= static_cast<std::uint8_t>(~0x02u);
  }

  if ((committedFlags0 & 0x04u) != 0u) {
    world->LinkApproximateLighting(this);
  } else if (nextApproxLightModel != this) {
    idRenderModelCommitted** link = &world->approxLightModels;
    while (*link != nullptr && *link != this)
      link = &(*link)->nextApproxLightModel;
    if (*link == this)
      *link = nextApproxLightModel;
    nextApproxLightModel = this;
  }
}

void idRenderModelCommitted::CheckBounds() const {
  idBounds activeBounds;
  activeBounds.Clear();

  for (int surfaceIndex = 0; surfaceIndex < surfaces.num; ++surfaceIndex) {
    const idRenderModelSurface& surface = surfaces.list[surfaceIndex];
    const idMaterial* material = surface.material;
    idTriangles* geometry = surface.geometry;
    if (material == nullptr || geometry == nullptr ||
        material->coverage == MC_INVISIBLE) {
      continue;
    }

    for (int component = 0; component < 3; ++component) {
      activeBounds.b[0][component] = std::fmin(
          activeBounds.b[0][component], geometry->bounds.b[0][component]);
      activeBounds.b[1][component] = std::fmax(
          activeBounds.b[1][component], geometry->bounds.b[1][component]);
    }

    const idDrawVert* vertices = geometry->verts;
    bool mapped = false;
    if (vertices == nullptr) {
      vertices = static_cast<const idDrawVert*>(
          geometry->vertexBuffer.MapBuffer(BM_READ));
      mapped = vertices != nullptr;
    }

    if (vertices != nullptr) {
      for (int vertexIndex = 0; vertexIndex < geometry->numVerts;
           ++vertexIndex) {
        int component = 0;
        for (; component < 3; ++component) {
          const float value = vertices[vertexIndex].xyz[component];
          if (value > geometry->bounds.b[1][component] + 1.0f ||
              value < geometry->bounds.b[0][component] - 1.0f) {
            break;
          }
        }
        if (component != 3) {
          idLib::Printf("bad tri->bounds on %s:%s\n", name,
                        material->name.str);
          break;
        }
      }
    }

    if (mapped)
      geometry->vertexBuffer.UnmapBuffer();
  }

  for (int component = 0; component < 3; ++component) {
    if (activeBounds.b[0][component] <
            referenceBounds.b[0][component] - 1.0f ||
        activeBounds.b[1][component] >
            referenceBounds.b[1][component] + 1.0f) {
      idLib::Printf("activeBounds exceeds referenceBounds on %s\n", name);
      return;
    }
  }
}

idVec3 idRenderModelCommitted::GetLightingPoint() const {
  idVec3 result;
  if ((r.flags4 & 0x20u) != 0u) {
    LocalPointToGlobal(r.localLightPositionOverride, result);
    return result;
  }

  if ((r.flags3 & 0x08u) != 0u) {
    idVec3 point = (referenceBounds.b[0] + referenceBounds.b[1]) * 0.5f;
    if ((r.flags3 & 0x10u) != 0u) {
      point.z +=
          (referenceBounds.b[1].z - referenceBounds.b[0].z) * 0.25f;
    }
    LocalPointToGlobal(point, result);
    return result;
  }

  LocalPointToGlobal(localLightingPoint, result);
  return result;
}

float idRenderModelCommitted::GetLightingRadius() const {
  if ((r.flags3 & 0x08u) == 0u)
    return localLightingRadius;

  idVec3 lightingPoint =
      (referenceBounds.b[0] + referenceBounds.b[1]) * 0.5f;
  if ((r.flags3 & 0x10u) != 0u) {
    lightingPoint.z +=
        (referenceBounds.b[1].z - referenceBounds.b[0].z) * 0.25f;
  }
  return referenceBounds.GetRadius(lightingPoint);
}

idRenderModelCommitted::idRenderModelCommitted()
    : name("<unnamed>"), world(nullptr), index{static_cast<unsigned>(-1)},
      referenceBounds(bounds_zero), r(), renderParmBlock(), surfaces(),
      references(), nextAddAlwaysModel(this), nextDimShadowModel(this),
      nextApproxLightModel(this), referenceModel(nullptr), guiFrameCount(0),
      maxVisibleRange{0.0f, 0.0f, 0.0f, 262144.0f},
      globalReferenceBounds(bounds_zero), localLightingPoint{},
      localLightingRadius(0.0f), modelMatrix{}, inverseModelMatrix{},
      approximateLighting{}, approximateState(0), committedFlags0(0x20u),
      mayContributeToPreZ(false), forceApproximateLightingCalculation(false),
      preventApproximateLightingCalculation(false),
      forceNoGPUocclusionTest(false), mvpMatrix{},
      highlightColor(static_cast<const idVec4&>(idColor::colorBlack)) {}

idRenderModelCommitted::~idRenderModelCommitted() noexcept(false) {
  if (world == nullptr)
    return;

  world->UnlinkModelFromTree(this);

  if (nextAddAlwaysModel != this) {
    idRenderModelCommitted** link = &world->addAlwaysModels;
    while (*link != nullptr && *link != this)
      link = &(*link)->nextAddAlwaysModel;
    if (*link == this)
      *link = nextAddAlwaysModel;
    nextAddAlwaysModel = this;
  }

  if (nextDimShadowModel != this) {
    idRenderModelCommitted** link = &world->dimShadowModels;
    while (*link != nullptr && *link != this)
      link = &(*link)->nextDimShadowModel;
    if (*link == this)
      *link = nextDimShadowModel;
    nextDimShadowModel = this;
  }

  if (nextApproxLightModel != this) {
    idRenderModelCommitted** link = &world->approxLightModels;
    while (*link != nullptr && *link != this)
      link = &(*link)->nextApproxLightModel;
    if (*link == this)
      *link = nextApproxLightModel;
    nextApproxLightModel = this;
  }
}
