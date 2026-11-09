#pragma once

#include "engine/models/discreteanimation/discreteanimationmodeldata.h"

class idStaticModel;

struct idDiscreteGenSurface {
  idDiscreteGenSurface();
  ~idDiscreteGenSurface();

  void FreeVisibleData();

  int materialNum{};
  idList<idTriangles*, 5> visibleGeometry;
  idList<const idMaterial*, 5> visibleMappingMaterial;
  idList<const idMaterial*, 5> visibleGenericMaterial;
  const idMaterial* surfTypeMaterial{};
};

struct idDiscreteBuildSurface {
  idDiscreteBuildSurface();
  ~idDiscreteBuildSurface();

  const idMaterial* material{};
  idList<const idMaterial*, 5> sourceMaterial;
  idList<idTriangles*, 5> geometry;
  idList<int, 5> jointIndex;
  int jointOffset{};
  int maxJoint{};
};

class idDiscreteAnimationModelDataGenerator {
public:
  idDiscreteAnimationModelDataGenerator();
  ~idDiscreteAnimationModelDataGenerator();

  bool Generate(const char* modelName);
  bool WriteBinary(const char* fileName);

  void FreeRenderSurfaces();
  void FreeData();
  void GenerateRenderSurfaces();
  void WriteBaseModel(const char* fileName);

  std::uint32_t timestamp;
  idBounds referenceBounds;
  float maxRadius;
  idList<sourceSurface_t, 5> sourceSurfaces;
  idList<idDiscreteGenSurface, 5> surfaces;
  idList<idCollisionModel*, 5> collisionModels;
  idList<idTraceModel, 5> traceModels;
  idList<const idMaterial*, 5> traceMaterials;
  idList<idJointMat, 5> transforms;
  idList<idDiscreteAnimationModelSurface, 5> renderSurfaces;
  idList<idTriangles*, 5> baseGeometry;
  idList<const idMaterial*, 5> baseMaterial;
  idStr errorString;
  idStaticModel* errorModel;
};

static_assert(sizeof(idDiscreteAnimationModelDataGenerator) == 296,
              "idDiscreteAnimationModelDataGenerator ABI drift");
static_assert(sizeof(idDiscreteGenSurface) == 88,
              "idDiscreteGenSurface ABI drift");
static_assert(offsetof(idDiscreteGenSurface, visibleGeometry) == 8,
              "idDiscreteGenSurface geometry offset drift");
static_assert(offsetof(idDiscreteGenSurface, surfTypeMaterial) == 80,
              "idDiscreteGenSurface material offset drift");
static_assert(sizeof(idDiscreteBuildSurface) == 88,
              "idDiscreteBuildSurface ABI drift");
static_assert(offsetof(idDiscreteBuildSurface, sourceMaterial) == 8,
              "idDiscreteBuildSurface source-material offset drift");
static_assert(offsetof(idDiscreteBuildSurface, geometry) == 32,
              "idDiscreteBuildSurface geometry offset drift");
static_assert(offsetof(idDiscreteBuildSurface, jointIndex) == 56,
              "idDiscreteBuildSurface joint-index offset drift");
static_assert(offsetof(idDiscreteBuildSurface, jointOffset) == 80,
              "idDiscreteBuildSurface joint offset drift");
static_assert(offsetof(idDiscreteAnimationModelDataGenerator, sourceSurfaces) ==
                  32,
              "discrete generator source-surface offset drift");
static_assert(offsetof(idDiscreteAnimationModelDataGenerator, surfaces) == 56,
              "discrete generator surface offset drift");
static_assert(offsetof(idDiscreteAnimationModelDataGenerator, renderSurfaces) ==
                  176,
              "discrete generator render-surface offset drift");
static_assert(offsetof(idDiscreteAnimationModelDataGenerator, errorString) ==
                  248,
              "discrete generator error-string offset drift");
static_assert(offsetof(idDiscreteAnimationModelDataGenerator, errorModel) ==
                  288,
              "discrete generator error-model offset drift");

template <>
std::size_t idFile::WriteBigArray<idDrawVert>(const idDrawVert* values,
                                              int count);
