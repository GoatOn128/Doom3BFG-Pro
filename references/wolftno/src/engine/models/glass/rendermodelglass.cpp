#include "engine/models/glass/rendermodelglass.h"

#include "engine/decls/decl.h"
#include "engine/models/glass/glassmodel.h"
#include "engine/models/glass/spheretree.h"
#include "engine/models/renderworld.h"
#include "engine/models/transparency/rendermodeltransparency.h"
#include "engine/renderer/autorenderparms.h"
#include "engine/renderer/jobs/render/renderocclusion.h"
#include "engine/renderer/render.h"
#include "shared/idlib/color.h"
#include "shared/idlib/csystems/cvarsystemlocal.h"

#include <algorithm>
#include <cmath>
#include <cstdint>
#include <cstring>
#include <new>
#include <utility>

idCVar r_skipGlass("r_skipGlass", "0", CVAR_BOOL, "skip glass");
idCVar r_showVisibleGlassBounds(
    "r_showVisibleGlassBounds", "0", CVAR_INTEGER,
    "Show the geometry bounds of visible glass surfaces");

void approximateLighting_t::Clear() { std::memset(this, 0, sizeof(*this)); }

template <typename T> class idSort {
public:
  virtual ~idSort() = default;
  virtual void Sort(T* base, unsigned int num) = 0;
};

template <typename T, typename Derived>
class idSort_Quick : public idSort<T> {
public:
  void Sort(T* base, unsigned int num) override;
};

class idSort_GlassSurfaces
    : public idSort_Quick<glassSurfaceSort_t, idSort_GlassSurfaces> {
public:
  ~idSort_GlassSurfaces() override;

  int Compare(const glassSurfaceSort_t& left,
              const glassSurfaceSort_t& right) const {
    if (left.depth < right.depth)
      return -1;
    if (left.depth > right.depth)
      return 1;
    return 0;
  }
};

idSort_GlassSurfaces::~idSort_GlassSurfaces() = default;

template <typename T, typename Derived>
void idSort_Quick<T, Derived>::Sort(T* base, unsigned int num) {
  if (num <= 1)
    return;
  constexpr std::int64_t maxLevels = 128;
  std::int64_t lo[maxLevels];
  std::int64_t hi[maxLevels];
  std::int64_t low = 0;
  std::int64_t high = num - 1;
  std::int64_t level = 0;
  for (;;) {
    std::int64_t i = low;
    std::int64_t j = high;
    if (j - i >= 4 && level < maxLevels - 1) {
      const std::int64_t pivotIndex = (i + j) / 2;
      std::swap(base[j], base[pivotIndex]);
      T& pivot = base[j--];
      do {
        while (static_cast<const Derived*>(this)->Compare(base[i], pivot) < 0) {
          if (++i >= j)
            break;
        }
        while (static_cast<const Derived*>(this)->Compare(base[j], pivot) > 0) {
          if (--j <= i)
            break;
        }
        if (i >= j)
          break;
        std::swap(base[i], base[j]);
      } while (++i < --j);
      while (static_cast<const Derived*>(this)->Compare(base[i], pivot) <= 0 &&
             i < high) {
        ++i;
      }
      while (static_cast<const Derived*>(this)->Compare(base[j], pivot) >= 0 &&
             low < j) {
        --j;
      }
      std::swap(pivot, base[i]);
      if (high - i <= j - low) {
        lo[level] = low;
        hi[level++] = j;
        low = i;
      } else {
        lo[level] = i;
        hi[level++] = high;
        high = j;
      }
    } else {
      for (; i < j; --j) {
        std::int64_t maximum = i;
        for (std::int64_t index = i + 1; index <= j; ++index) {
          if (static_cast<const Derived*>(this)->Compare(base[index],
                                                         base[maximum]) > 0) {
            maximum = index;
          }
        }
        std::swap(base[maximum], base[j]);
      }
      if (--level < 0)
        return;
      low = lo[level];
      high = hi[level];
    }
  }
}

idSphereTree::~idSphereTree() = default;

idRenderModelGlass::idRenderModelGlass() {
  glassSurfaces.granularity = 5;
  glassSurfaces.memTag = 5;
  sphereTree = nullptr;
  renderModelType = RENDERMODEL_GLASS;
  g.flags0 |= 0x80u;
  g.flags1 |= 0x01u;
  g.modelDepthSort = MODEL_DEPTH_SORT_AFTER_WORLD;
}

idRenderModelGlass::~idRenderModelGlass() {
  delete sphereTree;
  sphereTree = nullptr;
}

void idRenderModelGlass::DisableInstance(int index) {
  if (index >= 0 && index < glassSurfaces.num)
    glassSurfaces[index].enabled = false;
}

void idRenderModelGlass::SetDecal(int index, int decal,
                                  const idVec3& position,
                                  const idVec3& direction, float radius,
                                  float alpha, float angle) {
  if (decal < 0 || decal > 2 ||
      radius <= 0.00000011920928955078125f) {
    return;
  }

  const idDeclRenderParm* decalParms[3][3]{
      {rp_fractureDecal1Q.Get(), rp_fractureDecal1R.Get(),
       rp_fractureDecal1S.Get()},
      {rp_fractureDecal2Q.Get(), rp_fractureDecal2R.Get(),
       rp_fractureDecal2S.Get()},
      {rp_fractureDecal3Q.Get(), rp_fractureDecal3R.Get(),
       rp_fractureDecal3S.Get()}};

  idMat3 axis{};
  axis[0] = direction;
  direction.NormalVectors(axis[1], axis[2]);
  axis.RotateSelf(axis[0], angle);

  idParmBlock& parms = glassSurfaces[index].parms;
  const float inverseRadiusSquared =
      1.0f / std::sqrt((radius * radius) * (radius * radius));
  parms.SetParm(decalParms[decal][0],
                idVec4{position.x, position.y, position.z,
                       inverseRadiusSquared});
  parms.SetParm(decalParms[decal][1],
                idVec4{axis[0].x, axis[0].y, axis[0].z, 1.0f});
  parms.SetParm(decalParms[decal][2],
                idVec4{axis[1].x, axis[1].y, axis[1].z, alpha});
}

int idRenderModelGlass::GetSphereIntersection(const idSphere& sphere,
                                               int* ids,
                                               int maxIds) const {
  if (sphereTree == nullptr || maxIds <= 0)
    return 0;

  int count = 0;
  int collisionId = -1;
  if (sphereTree->FirstCollision(sphere.origin, sphere.radius,
                                 &collisionId)) {
    do {
      if (glassSurfaces[collisionId - 1].enabled) {
        ids[count++] = collisionId - 1;
        if (count >= maxIds)
          break;
      }
    } while (sphereTree->NextCollision(sphere.origin, sphere.radius,
                                       &collisionId));
  }
  return count;
}

void idRenderModelGlass::ApproximateLighting(
    idRenderModelUpdateTools* updateTools) {
  for (int index = 0; index < glassSurfaces.num; ++index) {
    glassModelData_t& surface = glassSurfaces[index];
    approximateLighting_t lighting;
    lighting.Clear();
    updateTools->ApproximateLightingForBounds(
        this, &surface.geometry->bounds, &lighting);

    idVec3 channelMaximum{};
    for (const idVec4& channel : lighting.channelLighting) {
      channelMaximum.x = std::fmax(channel.x, channelMaximum.x);
      channelMaximum.y = std::fmax(channel.y, channelMaximum.y);
      channelMaximum.z = std::fmax(channel.z, channelMaximum.z);
    }
    surface.parms.SetParm(
        rp_Color.Get(),
        idVec4{channelMaximum.x + lighting.lightColor[0].x,
               channelMaximum.y + lighting.lightColor[0].y,
               channelMaximum.z + lighting.lightColor[0].z, 1.0f});
  }
}

void idRenderModelGlass::RenderInstance(int index, const idRenderView* view) {
  glassModelData_t& instance = glassSurfaces[index];
  if (!instance.enabled)
    return;

  bool visible = instance.references.num == 0;
  if (instance.references.num > 0) {
    occlusionState_t* occlusionState = view->occlusionState;
    for (int reference = 0; reference < instance.references.num; ++reference) {
      if (occlusionState->worldAreaVisibleFrameNumber[
              instance.references[reference]] >=
          occlusionState->renderFrameNumber) {
        visible = true;
      }
    }
  }
  if (!visible)
    return;

  instance.currentParmBuffer ^= 1;
  instance.parmBuffers[instance.currentParmBuffer].CopyFrom(instance.parms);
  idRenderModelSurface* surface = &surfaces.Alloc();
  surface->geometry = instance.geometry;
  surface->material = instance.material;
  surface->geometryIsReference = true;
  surface->referenceMask = 0x21;
  surface->parms = &instance.parmBuffers[instance.currentParmBuffer];

  if (r_showVisibleGlassBounds.valueInteger != 0) {
    idRenderDebug* renderDebug = GetRenderDebug();
    renderDebug->DebugBounds(idColor::colorYellow,
                             surface->geometry->bounds, vec3_origin, 0,
                             r_showVisibleGlassBounds.valueInteger == 2);
  }
}

bool idRenderModelGlass::UpdateInView(
    const idRenderView* currentView, const idRenderView* nextView,
    idRenderModelUpdateTools* updateTools) {
  if (r_skipGlass.valueInteger != 0) {
    for (int index = 0; index < surfaces.num; ++index) {
      surfaces[index].geometry->numVerts = 0;
      surfaces[index].geometry->numIndexes = 0;
    }
    return false;
  }

  if ((g.flags1 & 0x20u) != 0) {
    const int modelIndex = GetIndex();
    occlusionState_t* occlusionState = currentView->occlusionState;
    if (occlusionState->modelVisibleFrameNumber[modelIndex] >=
            occlusionState->renderFrameNumber &&
        occlusionState->modelVisibleFrameCount[modelIndex] == 2) {
      ApproximateLighting(updateTools);
    }
  }

  int ids[1024];
  int numIds = 0;
  if (bvhCull && sphereTree != nullptr) {
    numIds = sphereTree->CollectSpheresFromView(nextView, ids, 1024);
  } else {
    numIds = std::min(glassSurfaces.num, 1024);
    for (int index = 0; index < numIds; ++index)
      ids[index] = index + 1;
  }

  idStaticList<glassSurfaceSort_t, 1024> sortSurfaces;
  for (int candidate = 0; candidate < numIds; ++candidate) {
    const int index = ids[candidate] - 1;
    glassModelData_t& instance = glassSurfaces[index];
    idTriangles* geometry = instance.geometry;
    if (!instance.enabled || instance.material == nullptr ||
        geometry == nullptr || geometry->numVerts == 0 ||
        geometry->numIndexes < 3) {
      continue;
    }

    glassSurfaceSort_t* sorted = sortSurfaces.Alloc();
    sorted->index = index;
    const idVec3 center{
        (geometry->bounds[0].x + geometry->bounds[1].x) * 0.5f,
        (geometry->bounds[0].y + geometry->bounds[1].y) * 0.5f,
        (geometry->bounds[0].z + geometry->bounds[1].z) * 0.5f};
    const idVec3 delta = currentView->r.vieworg - center;
    sorted->depth = delta.LengthSqr();
  }

  idSort_GlassSurfaces sorter;
  if (sortSurfaces.list != nullptr)
    sorter.Sort(sortSurfaces.list,
                static_cast<unsigned int>(sortSurfaces.num));

  surfaces.Clear();
  for (int index = 0; index < sortSurfaces.num; ++index)
    RenderInstance(sortSurfaces[index].index, currentView);
  CommitThisFrame();
  return true;
}

void idRenderModelGlass::SetGlassModel(const char* modelName) {
  auto* glassModel = static_cast<idGlassModel*>(
      idGlassModel::resourceList.Load(modelName, false, false));
  bvhCull = idStr::CheckExtension(modelName, "gmodel");

  // The retail linker folded idGlassModel's surface-count accessor into
  // idDecl::GetTextLength: both return the int at object offset 0x38.
  const int numSurfaces =
      reinterpret_cast<const idDecl*>(glassModel)->GetTextLength();
  if (numSurfaces <= glassSurfaces.size ||
      glassSurfaces.ResizeForPort(numSurfaces)) {
    glassSurfaces.num = std::min(numSurfaces, glassSurfaces.size);
  }

  for (int index = 0;
       index < reinterpret_cast<const idDecl*>(glassModel)->GetTextLength();
       ++index) {
    const idGlassModelSurface& source = glassModel->ConstSurface(index);
    glassModelData_t& destination = glassSurfaces[index];
    destination.geometry = source.geometry;
    destination.material = source.material;
    destination.currentParmBuffer = 0;
    destination.boundSphere = source.boundSphere;
    destination.enabled = true;

    int areas[128];
    int numAreas = 0;
    BoundsInAreas(destination.geometry->bounds, areas, &numAreas, 128);
    if (numAreas > destination.references.size)
      destination.references.ResizeForPort(numAreas);
    for (int area = 0; area < numAreas; ++area)
      destination.references.Append(areas[area]);
  }

  if (bvhCull) {
    SetMaxSurfaces(1024);
    g.flags1 |= 0x20u;
    sphereTree = new (std::nothrow) idSphereTree;
    sphereTree->Alloc((3 * glassSurfaces.num) >> 1, 1024.0f, 256.0f,
                      64.0f);
    for (int index = 0; index < glassSurfaces.num; ++index) {
      sphereTree->AddSphere(glassSurfaces[index].boundSphere.origin,
                            glassSurfaces[index].boundSphere.radius,
                            SPHERE_NODE_NONE, index + 1);
    }
    sphereTree->Update();
  }
}

template idRenderModelSurface& idList<idRenderModelSurface, 85>::Alloc();
template void idSort_Quick<glassSurfaceSort_t, idSort_GlassSurfaces>::Sort(
    glassSurfaceSort_t*, unsigned int);
