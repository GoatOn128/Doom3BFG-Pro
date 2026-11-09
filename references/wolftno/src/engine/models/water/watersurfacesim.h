#pragma once

#include "engine/models/water/jobs/watergen.h"
#include "shared/idlib/bv/bounds.h"
#include "shared/idlib/containers/list.h"
#include "shared/idlib/math/random.h"

#include <cstddef>
#include <cstdint>

class idRenderModel;
class idRenderModelWaterSurface;

class idWaterSurfaceSim {
public:
  idWaterSurfaceSim() = default;
  idWaterSurfaceSim(const idBounds& bounds, float spacing, float damping,
                    float speed, float timeStep);
  ~idWaterSurfaceSim();

  void AddRipple(const idVec3& pos, float magnitude, float radius,
                 const idRenderModel* renderModel);
  void AddFancyRipple(const idVec3& pos, const idRenderModel* renderModel,
                      float amplitude, float frequency, float fade1,
                      float fade2, float currentDistance);
  void AddDrop(unsigned int x, unsigned int y, float radius, float magnitude);
  void AddWave(const idVec2& direction, float frequency, float phase,
               float amplitude, float waveExponent);
  void ApplyRadiusEffect(const idVec3& pos, const idRenderModel* renderModel,
                         float radius, float fade);
  void ClearRadiusEffects();
  void Swap();

  unsigned int GetWidth() const { return width; }
  unsigned int GetHeight() const { return height; }
  float GetSpacing() const { return spacing; }
  float GetSpeed() const { return speed; }
  float GetDamping() const { return damping; }
  const idVec3& GetOrigin() const { return origin; }
  unsigned int GetNumDisturbances() const {
    return static_cast<unsigned int>(numDisturbances);
  }
  const waterDisturbanceParms_t* GetDisturbances() const {
    return waterDisturbances[currentDeferred].list;
  }
  int GetCurrentIdx() const { return currentDeferred; }
  float* GetHeightMap(int index) { return waveHeight[index].list; }
  const float* GetHeightMap(int index) const { return waveHeight[index].list; }
  float* GetDummyBlock() { return dummyBlock.list; }
  unsigned int* GetSimulationParmsMap() { return simulateParms.list; }
  unsigned char* GetVertGradientMap() { return vertGradient.list; }
  unsigned char GetVertGradient(unsigned int x, unsigned int y) const {
    int blockLocation = 0;
    GridPosToBlockLoc(static_cast<int>(x), static_cast<int>(y), numBlocksX,
                      numBlocksY, &blockLocation);
    return vertGradient.list[blockLocation];
  }
  float GetTimeStep();
  int GetAllocWidth() const { return allocWidth; }
  int GetAllocHeight() const { return allocHeight; }
  int GetNumBlocksX() const { return numBlocksX; }
  int GetNumBlocksY() const { return numBlocksY; }
  void SetVisible(bool value) { visible = value; }
  void SetSimulationParms(unsigned int x, unsigned int y,
                          const idVec4& parms);
  void SetVertGradient(unsigned int x, unsigned int y, unsigned char value) {
    int blockLocation = 0;
    GridPosToBlockLoc(static_cast<int>(x), static_cast<int>(y), numBlocksX,
                      numBlocksY, &blockLocation);
    vertGradient.list[blockLocation] = value;
  }
  bool GetSurfacePositionAtPoint(idVec3& pos,
                                 idRenderModelWaterSurface* waterSurfaceModel,
                                 float& waveHeight, idVec3& normal);

  unsigned int width = 0;
  unsigned int height = 0;
  float spacing = 0.0f;
  float damping = 0.0f;
  float speed = 0.0f;
  float timeStep = 0.0f;
  bool visible = false;
  std::uint8_t recoveredPadding19[3]{};
  idVec3 origin{};
  idRandom2 rnd{};
  int currentDeferred = 0;
  int numDisturbances = 0;
  int allocWidth = 0;
  int allocHeight = 0;
  int numBlocksX = 0;
  int numBlocksY = 0;
  std::uint32_t recoveredPadding44 = 0;
  idList<unsigned int, 54> simulateParms;
  idList<unsigned char, 54> vertGradient;
  idList<float, 54> waveHeight[2];
  idList<float, 54> dummyBlock;
  idList<waterDisturbanceParms_t, 54> waterDisturbances[2];

private:
  void GlobalToLocalPos(const idVec3& globalPos, idVec3& localPos,
                        const idRenderModel* renderModel);
};

static_assert(sizeof(idWaterSurfaceSim) == 0xF0,
              "idWaterSurfaceSim ABI drift");
static_assert(offsetof(idWaterSurfaceSim, spacing) == 0x08,
              "water spacing offset drift");
static_assert(offsetof(idWaterSurfaceSim, origin) == 0x1C,
              "water origin offset drift");
static_assert(offsetof(idWaterSurfaceSim, currentDeferred) == 0x2C,
              "water deferred index offset drift");
static_assert(offsetof(idWaterSurfaceSim, simulateParms) == 0x48,
              "water simulation map offset drift");
static_assert(offsetof(idWaterSurfaceSim, vertGradient) == 0x60,
              "water gradient map offset drift");
static_assert(offsetof(idWaterSurfaceSim, waveHeight) == 0x78,
              "water height maps offset drift");
static_assert(offsetof(idWaterSurfaceSim, dummyBlock) == 0xA8,
              "water dummy block offset drift");
static_assert(offsetof(idWaterSurfaceSim, waterDisturbances) == 0xC0,
              "water disturbances offset drift");
