#pragma once

#include "engine/models/rendermodel.h"
#include "engine/models/water/jobs/watergen.h"
#include "engine/models/water/watersurfacesim.h"
#include "engine/renderer/bufferobject.h"

#include <cstddef>
#include <cstdint>

class idMaterial;
struct RenderModelWaterSurfaceValidationAccess;

class idRenderModelWaterSurface : public idRenderModel {
public:
  static constexpr int NUM_VERTEX_BUFFERS = 3;

  struct waterSimParms_t {
    waterSimParms_t() = default;
    float spacing{};
    float damping{};
    float speed{};
    float timeStep{};
    idBounds waterBounds{};
  };

  explicit idRenderModelWaterSurface(const char* modelName);

  bool CheckValid(const idWaterSurfaceSim* simulation,
                  const idBounds& waterSimBounds) const;
  void BuildWaterModel(const idMaterial* material, const idBounds& waterBounds,
                       float spacing, float damping, float speed,
                       float timeStep, const char* imageName);
  static void BuildStaticModel(idRenderModel* model, const idMaterial* material,
                               const idBounds& waterBounds,
                               const char* imageName);
  idWaterSurfaceSim* GetWaterSim() { return waterSim; }

protected:
  ~idRenderModelWaterSurface() override;

private:
  friend struct RenderModelWaterSurfaceValidationAccess;

  void Save(idFile* file) const override;
  bool Load(idFile* file) override;
  bool LoadModel(const char* modelName);
  bool LoadBinary(const char* modelName);
  bool WriteBinary(const char* modelName) const;
  void FreeData();
  void CreateGeometry(const idMaterial* material, const char* imageName,
                      const idVec3& origin, int width, int height,
                      float spacing, int numBlocksX, int numBlocksY);
  static void CreateGeometryInternal(
      const idMaterial* material, const char* imageName, const idVec3& origin,
      int width, int height, float spacing, int numBlocksX, int numBlocksY,
      int numVerts, int numIndices, idDrawVert* verts,
      std::uint16_t* indices, idBounds& bounds);
  bool UpdateInView(const idRenderView* currentView,
                    const idRenderView* nextView,
                    idRenderModelUpdateTools* updateTools) override;

public:
  idWaterSurfaceSim* waterSim{};
  idVertexBuffer vertexBuffer[NUM_VERTEX_BUFFERS];
  idIndexBuffer indexBuffer;
  idDrawVert* deferredVerts{};
  int currentIndex{};
  std::uint8_t recoveredPadding8F4[4]{};
  deferredWaterBlockGenParms_t* waterGenBlockParms{};
  waterSimParms_t waterSimParms{};
  const idMaterial* material{};
};

void WaterGenJob(const deferredWaterBlockGenParms_t* parms);

static_assert(sizeof(idRenderModelWaterSurface::waterSimParms_t) == 0x28,
              "water-surface simulation parms ABI drift");
static_assert(sizeof(idRenderModelWaterSurface) == 0x930,
              "water-surface model ABI drift");
static_assert(offsetof(idRenderModelWaterSurface, waterSim) == 0x880,
              "water-surface simulation offset drift");
static_assert(offsetof(idRenderModelWaterSurface, vertexBuffer) == 0x888,
              "water-surface vertex-buffer offset drift");
static_assert(offsetof(idRenderModelWaterSurface, indexBuffer) == 0x8D0,
              "water-surface index-buffer offset drift");
static_assert(offsetof(idRenderModelWaterSurface, waterGenBlockParms) == 0x8F8,
              "water-surface job-parms offset drift");
static_assert(offsetof(idRenderModelWaterSurface, waterSimParms) == 0x900,
              "water-surface saved-parms offset drift");
static_assert(offsetof(idRenderModelWaterSurface, material) == 0x928,
              "water-surface material offset drift");

