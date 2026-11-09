#pragma once

#include "engine/decls/decl.h"
#include "engine/models/skeletalanimation/md6util.h"
#include "engine/models/static/staticmodel.h"
#include "engine/renderer/triangles.h"
#include "shared/idlib/filesystem/file.h"
#include "shared/idlib/geometry/tracemodel.h"

#include <cstddef>
#include <cstdint>

class idCollisionModel;
class idCVar;

extern idCVar r_discretemodel_freecpudata;
extern idCVar r_discretemodel_forcestale;

struct idDiscreteAnimationModelSurface {
  idDiscreteAnimationModelSurface();

  int jointOffset;
  int numJoints;
  idTriangles* triangles;
  const idMaterial* material;
};

class idSwap {
public:
  template <typename T> static void BigArray(T* values, int count);
};

template <>
void idSwap::BigArray<idDrawVert>(idDrawVert* values, int count);

template <>
std::size_t idFile::ReadBigArray<idDrawVert>(idDrawVert* values, int count);

class idDiscreteAnimationModelData : public idResource {
public:
  idDiscreteAnimationModelData();
  ~idDiscreteAnimationModelData() override;

  void LoadResource() override;
  bool ReloadIfStale() override;
  idResourceList* GetResourceList() const override;

  std::uint32_t version;
  std::uint32_t sourceTimestamp;
  std::uint32_t binaryTimestamp;
  float maxRadius;
  idBounds bounds;
  idList<idTraceModel, 59> traceModels;
  idList<const idMaterial*, 59> traceMaterials;
  idList<idJointMat, 59> transforms;
  idList<idDiscreteAnimationModelSurface*, 59> surfaces;
  idList<sourceSurface_t, 59> sourceSurfaces;
  idCollisionModel* baseModel;

  static idTypedResourceList<idDiscreteAnimationModelData> resourceList;

private:
  void UpdateBuffers();
  void FreeCPUData();
  static idStr GetBinaryFilename(const char* modelName);
  void FreeData();
  void RemapSourceSurfaces();
  bool LoadBinary(const char* fileName);

  friend class DiscreteAnimationModelDataValidationAccess;
};

template <>
idDiscreteAnimationModelData*
idTypedResourceList<idDiscreteAnimationModelData>::Alloc(
    const char* resourceName) const;

static_assert(sizeof(idDiscreteAnimationModelSurface) == 24,
              "idDiscreteAnimationModelSurface ABI drift");
static_assert(offsetof(idDiscreteAnimationModelSurface, triangles) == 8,
              "discrete surface triangles offset drift");
static_assert(offsetof(idDiscreteAnimationModelSurface, material) == 16,
              "discrete surface material offset drift");
static_assert(sizeof(idDiscreteAnimationModelData) == 216,
              "idDiscreteAnimationModelData ABI drift");
static_assert(offsetof(idDiscreteAnimationModelData, version) == 48,
              "discrete model version offset drift");
static_assert(offsetof(idDiscreteAnimationModelData, bounds) == 64,
              "discrete model bounds offset drift");
static_assert(offsetof(idDiscreteAnimationModelData, traceModels) == 88,
              "discrete trace-model list offset drift");
static_assert(offsetof(idDiscreteAnimationModelData, surfaces) == 160,
              "discrete render-surface list offset drift");
static_assert(offsetof(idDiscreteAnimationModelData, sourceSurfaces) == 184,
              "discrete source-surface list offset drift");
static_assert(offsetof(idDiscreteAnimationModelData, baseModel) == 208,
              "discrete base-model offset drift");
