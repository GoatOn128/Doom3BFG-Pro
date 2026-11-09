#include "engine/models/static/rendermodelstatic.h"

#include "engine/models/transparency/rendermodeltransparency.h"
#include "engine/renderer/declmaterial.h"
#include "shared/idlib/csystems/cvarsystemlocal.h"
#include "shared/idlib/parallelism/paralleljoblist.h"
#include "shared/idlib/sys/sys_alloc.h"

#include <new>

idCVar r_staticTransparencyFadeStartDist(
    "r_staticTransparencyFadeStartDist", "1024", CVAR_FLOAT,
    "start fading out at this range");
idCVar r_staticTransparencyFadeMaxDist(
    "r_staticTransparencyFadeMaxDist", "2048", CVAR_FLOAT,
    "completely faded out at this range");
idCVar r_useStaticTransparencyGenJobs(
    "r_useStaticTransparencyGenJobs", "1", CVAR_INTEGER,
    "0 = off, 1 = threaded", 0.0f, 1.0f);

namespace {

constexpr memTag_t kRenderModelTag = static_cast<memTag_t>(85);
constexpr char kTransparencyParmsLocation[] =
    "w:\\build\\tech5\\engine\\models\\static\\RenderModelStatic.cpp(128) : TAG_RENDERMODEL";

bool IsTransparencySorted(const idMaterial* material) {
  return material != nullptr && material->materialIsTransparencySorted != 0;
}

} // namespace

const idList<sourceSurface_t, 5>*
idRenderModelStatic::GetSourceSurfaces() const {
  return staticModel != nullptr ? &staticModel->sourceSurfaces : nullptr;
}

bool idRenderModelStatic::UpdateInView(
    const idRenderView* currentView, const idRenderView*,
    idRenderModelUpdateTools* updateTools) {
  if (staticModel == nullptr || !staticModel->usesTransparencySort)
    return false;

  const int useJobs = r_useStaticTransparencyGenJobs.valueInteger;
  int transparencyIndex = 0;
  for (int surfaceIndex = 0; surfaceIndex < staticModel->surfaces.num;
       ++surfaceIndex) {
    const idStaticModelSurface& surface = staticModel->surfaces[surfaceIndex];
    if (!IsTransparencySorted(surface.material) ||
        surface.geometry == nullptr || surface.geometry->verts == nullptr) {
      continue;
    }

    staticTransparencyGenParms_t* parms =
        transparencyGenParms[transparencyIndex++];
    parms->inputVerts = surface.geometry->verts;
    parms->numInputVerts = surface.geometry->numVerts;
    parms->fadeStartDist = r_staticTransparencyFadeStartDist.valueFloat;
    parms->fadeMaxDist = r_staticTransparencyFadeMaxDist.valueFloat;
    const bool deferred = (useDeferredPosition & 1U) != 0U;
    parms->origin = deferred ? deferredOrigin : g.origin;
    parms->axis = deferred ? deferredAxis : g.axis;
    parms->viewOrg = currentView->r.vieworg;
    parms->viewDir = currentView->r.viewaxis[0];
    parms->transSortScaleBias = surface.material->atlasScaleBias;
    parms->lighting = *GetApproximateLighting();
    parms->verts = nullptr;
    parms->quadDepth = nullptr;
    parms->quadsUsed = nullptr;
    idRenderModelTransparency::AllocateQuadSegment(
        surface.geometry->numVerts / 4, parms->verts, parms->quadDepth,
        parms->quadsUsed, false);
    if (useJobs == 1) {
      updateTools->renderModelJobList->AddJob(
          reinterpret_cast<jobRun_t>(StaticTransparencyGenJob), parms);
    } else {
      StaticTransparencyGenJob(parms);
    }
  }
  return true;
}

idRenderModelStatic::~idRenderModelStatic() {
  for (int index = 0; index < transparencyGenParms.num; ++index)
    idMem::Singleton()->Free(transparencyGenParms[index], ALIGN_16);
  transparencyGenParms.ClearFree();
}

bool idRenderModelStatic::UpdateSurfaces() {
  const bool changed = reloadCount != staticModel->reloadCount;
  if (!changed)
    return false;

  FreeSurfaces();
  int transparencySurfaceCount = 0;
  for (int index = 0; index < staticModel->surfaces.num; ++index) {
    const idStaticModelSurface& source = staticModel->surfaces[index];
    idStaticModelSurface surface;
    static_cast<idRenderModelSurface&>(surface) =
        static_cast<const idRenderModelSurface&>(source);
    surface.skinRemaps = source.skinRemaps;
    if (IsTransparencySorted(surface.material)) {
      ++transparencySurfaceCount;
    } else {
      surface.geometryIsReference = true;
      AddSurface(static_cast<const idRenderModelSurface&>(surface));
    }
  }
  FinishSurfaces();

  if (!staticModel->modelIsAutosprite)
    referenceBounds = staticModel->referenceBounds;
  if (staticModel->usesTransparencySort) {
    g.flags2 |= 0x20U;
    SetForceApproximateLightingCalculation();
  }
  reloadCount = staticModel->reloadCount;

  while (transparencyGenParms.num < transparencySurfaceCount) {
    auto* parms = static_cast<staticTransparencyGenParms_t*>(
        idMem::Singleton()->AllocWithLocation(
            kTransparencyParmsLocation, sizeof(staticTransparencyGenParms_t),
            kRenderModelTag, false, ALIGN_16, HEAP_DEFAULTHEAP));
    transparencyGenParms.Append(parms);
  }
  return true;
}

bool idRenderModelStatic::CommitSubclass() {
  if (staticModel == nullptr)
    return false;

  for (int index = decalData.num - 1; index >= 0; --index) {
    const bool deferred = (useDeferredPosition & 1U) != 0U;
    const idVec3& origin = deferred ? deferredOrigin : g.origin;
    const idMat3& axis = deferred ? deferredAxis : g.axis;
    const idVec3 currentPosition = axis * decalData[index].relativePos + origin;
    const idMat3 currentAxis = decalData[index].relativeAxis * axis;
    if (!SetDecalPosition(decalData[index].handle, currentPosition,
                          currentAxis)) {
      decalData.RemoveIndexFast(index);
    }
  }

  const bool changed = UpdateSurfaces();
  const int customSkin = g.customSkin;
  if (skin != customSkin || changed) {
    skin = customSkin;
    if (skin >= 0) {
      for (int index = 0; index < surfaces.num; ++index) {
        const skinRemap_t& remap =
            staticModel->surfaces[index].skinRemaps[skin];
        surfaces[index].material = remap.mtr;
        surfaces[index].skinOffsets = {remap.texOffset.x, remap.texOffset.y,
                                       remap.texOffset.x, remap.texOffset.y};
      }
    }
  }
  return changed;
}

idRenderModelStatic::idRenderModelStatic(const idStaticModel* smod)
    : staticModel(smod), skin(-1), reloadCount(-1), transparencyGenParms() {
  transparencyGenParms.ClearFree();
  renderModelType = RENDERMODEL_STATIC;
  SetName(smod->name.str);
  referenceBounds = smod->referenceBounds;
  if (smod->usesTransparencySort)
    g.flags2 |= 0x20U;
  UpdateSurfaces();
}
