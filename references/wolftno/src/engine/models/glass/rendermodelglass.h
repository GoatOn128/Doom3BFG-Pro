#pragma once

#include "engine/models/rendermodel.h"
#include "shared/idlib/bv/sphere.h"

#include <cstddef>
#include <cstdint>

class idCVar;
class idSphereTree;

struct glassModelData_t {
  const idMaterial* material{};
  idTriangles* geometry{};
  idSphere boundSphere{};
  idStaticParmBlock<10> parms;
  idStaticParmBlock<10> parmBuffers[2];
  int currentParmBuffer{};
  std::uint32_t alignmentBeforeReferences{};
  idList<int, 5> references;
  bool enabled{};
  std::uint8_t recoveredPadding[15]{};
};

struct glassSurfaceSort_t {
  int index{};
  float depth{};
};

class idRenderModelGlass : public idRenderModel {
public:
  idRenderModelGlass();
  ~idRenderModelGlass() override;

  void DisableInstance(int index);
  void SetDecal(int index, int decal, const idVec3& position,
                const idVec3& direction, float radius, float alpha,
                float angle);
  int GetSphereIntersection(const idSphere& sphere, int* ids,
                            int maxIds) const;
  void SetGlassModel(const char* modelName);
  bool UpdateInView(const idRenderView* currentView,
                    const idRenderView* nextView,
                    idRenderModelUpdateTools* updateTools) override;

  bool bvhCull{};
  std::uint8_t recoveredAlignment[7]{};
  idList<glassModelData_t, 5> glassSurfaces;
  idSphereTree* sphereTree{};

private:
  void ApproximateLighting(idRenderModelUpdateTools* updateTools);
  void RenderInstance(int index, const idRenderView* view);

  friend struct RenderModelGlassValidationAccess;
};

extern idCVar r_skipGlass;
extern idCVar r_showVisibleGlassBounds;

static_assert(sizeof(idStaticParmBlock<10>) == 0x120,
              "glass parm-block ABI drift");
static_assert(sizeof(glassModelData_t) == 0x3b0,
              "glass model-data ABI drift");
static_assert(offsetof(glassModelData_t, material) == 0x00,
              "glass material offset drift");
static_assert(offsetof(glassModelData_t, geometry) == 0x08,
              "glass geometry offset drift");
static_assert(offsetof(glassModelData_t, boundSphere) == 0x10,
              "glass sphere offset drift");
static_assert(offsetof(glassModelData_t, parms) == 0x20,
              "glass parms offset drift");
static_assert(offsetof(glassModelData_t, parmBuffers) == 0x140,
              "glass parm buffers offset drift");
static_assert(offsetof(glassModelData_t, currentParmBuffer) == 0x380,
              "glass current parm-buffer offset drift");
static_assert(offsetof(glassModelData_t, references) == 0x388,
              "glass references offset drift");
static_assert(offsetof(glassModelData_t, enabled) == 0x3a0,
              "glass enabled offset drift");
static_assert(sizeof(glassSurfaceSort_t) == 8,
              "glass surface-sort ABI drift");
static_assert(offsetof(idRenderModelGlass, bvhCull) == 0x880,
              "glass BVH flag offset drift");
static_assert(offsetof(idRenderModelGlass, glassSurfaces) == 0x888,
              "glass surface-list offset drift");
static_assert(offsetof(idRenderModelGlass, sphereTree) == 0x8a0,
              "glass sphere-tree offset drift");
static_assert(sizeof(idRenderModelGlass) == 0x8a8,
              "render-model glass ABI drift");
