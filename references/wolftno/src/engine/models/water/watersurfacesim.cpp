#include "engine/models/water/watersurfacesim.h"

#include "engine/models/water/jobs/watergen.h"
#include "engine/models/water/watersurfacesimmodelview.h"
#include "shared/idlib/lib.h"

#include <algorithm>
#include <cmath>
#include <cstring>
#include <limits>

namespace {

constexpr int kMaximumDisturbances = 100;
constexpr int kWaterBlockSize = 16;
constexpr int kDummyBlockElements = kWaterBlockSize * kWaterBlockSize;
constexpr float kRayStartOffset = 256.0f;
constexpr float kByteScale = 255.0f;
constexpr idVec3 kUp{0.0f, 0.0f, 1.0f};

unsigned char FloatToByte(float value) {
  const int integer = static_cast<int>(value * kByteScale);
  return static_cast<unsigned char>(std::clamp(integer, 0, 255));
}

const idVec3& ModelOrigin(const idWaterSurfaceRenderModelView& model) {
  return (model.useDeferredPosition & 1U) != 0U ? model.deferredOrigin
                                                : model.g.origin;
}

const idMat3& ModelAxis(const idWaterSurfaceRenderModelView& model) {
  return (model.useDeferredPosition & 1U) != 0U ? model.deferredAxis
                                                : model.g.axis;
}

idVec3 ModelToGlobal(const idWaterSurfaceRenderModelView& model,
                     const idVec3& local) {
  const idMat3& axis = ModelAxis(model);
  const idVec3& modelOrigin = ModelOrigin(model);
  const idVec3 scaled{local.x * model.g.scale.x, local.y * model.g.scale.y,
                      local.z * model.g.scale.z};
  return {
      scaled.x * axis[0].x + scaled.y * axis[1].x +
          scaled.z * axis[2].x + modelOrigin.x,
      scaled.x * axis[0].y + scaled.y * axis[1].y +
          scaled.z * axis[2].y + modelOrigin.y,
      scaled.x * axis[0].z + scaled.y * axis[1].z +
          scaled.z * axis[2].z + modelOrigin.z,
  };
}

} // namespace

float idWaterSurfaceSim::GetTimeStep() { return timeStep; }

void idWaterSurfaceSim::Swap() {
  currentDeferred ^= 1;
  numDisturbances = 0;
}

void idWaterSurfaceSim::GlobalToLocalPos(const idVec3& globalPos,
                                         idVec3& localPos,
                                         const idRenderModel* renderModel) {
  const idWaterSurfaceRenderModelView& model =
      idWaterSurfaceRenderModelView::From(renderModel);
  const idVec3& modelOrigin = ModelOrigin(model);
  const idMat3& axis = ModelAxis(model);
  const idVec3 delta = globalPos - modelOrigin;
  localPos.x = (delta * axis[0]) / model.g.scale.x - origin.x;
  localPos.y = (delta * axis[1]) / model.g.scale.y - origin.y;
  localPos.z = (delta * axis[2]) / model.g.scale.z - origin.z;
}

void idWaterSurfaceSim::AddRipple(const idVec3& pos, float magnitude,
                                  float radius,
                                  const idRenderModel* renderModel) {
  if (!visible || numDisturbances >= kMaximumDisturbances)
    return;

  idVec3 localPos{};
  GlobalToLocalPos(pos, localPos, renderModel);
  waterDisturbanceParms_t& disturbance =
      waterDisturbances[currentDeferred].list[numDisturbances++];
  disturbance.gridX =
      static_cast<int>(std::floor(localPos.x / spacing + 0.5f));
  disturbance.gridY =
      static_cast<int>(std::floor(localPos.y / spacing + 0.5f));
  disturbance.loc = {localPos.x, localPos.y};
  disturbance.parms.x = magnitude;
  disturbance.radius = radius;
  disturbance.type = WATER_DISTURBANCE_DROP;
}

void idWaterSurfaceSim::AddFancyRipple(const idVec3& pos,
                                       const idRenderModel* renderModel,
                                       float amplitude, float frequency,
                                       float fade1, float fade2,
                                       float currentDistance) {
  if (!visible || numDisturbances >= kMaximumDisturbances)
    return;

  idVec3 localPos{};
  GlobalToLocalPos(pos, localPos, renderModel);
  waterDisturbanceParms_t& disturbance =
      waterDisturbances[currentDeferred].list[numDisturbances++];
  disturbance.loc = {localPos.x, localPos.y};
  disturbance.parms.x = amplitude;
  disturbance.parms.y = frequency;
  disturbance.parms.z = fade1;
  disturbance.parms.w = fade2;
  disturbance.radius = currentDistance;
  disturbance.type = WATER_DISTURBANCE_WAVE;
}

void idWaterSurfaceSim::AddDrop(unsigned int x, unsigned int y, float radius,
                                float magnitude) {
  if (!visible || numDisturbances >= kMaximumDisturbances)
    return;

  waterDisturbanceParms_t& disturbance =
      waterDisturbances[currentDeferred].list[numDisturbances++];
  disturbance.gridX = static_cast<int>(x);
  disturbance.gridY = static_cast<int>(y);
  disturbance.parms.x = magnitude;
  disturbance.radius = radius;
  disturbance.type = WATER_DISTURBANCE_GRID_DROP;
}

void idWaterSurfaceSim::AddWave(const idVec2& direction, float frequency,
                                float phase, float amplitude,
                                float waveExponent) {
  if (!visible || numDisturbances >= kMaximumDisturbances)
    return;

  waterDisturbanceParms_t& disturbance =
      waterDisturbances[currentDeferred].list[numDisturbances++];
  disturbance.loc = direction;
  disturbance.parms.x = amplitude;
  disturbance.parms.y = frequency;
  disturbance.parms.z = phase;
  disturbance.parms.w = waveExponent;
  disturbance.type = WATER_DISTURBANCE_CONSTANT;
}

void idWaterSurfaceSim::SetSimulationParms(unsigned int x, unsigned int y,
                                           const idVec4& parms) {
  int blockLocation = 0;
  GridPosToBlockLoc(static_cast<int>(x), static_cast<int>(y), numBlocksX,
                    numBlocksY, &blockLocation);
  const std::uint32_t z = FloatToByte(parms.z);
  const std::uint32_t px = FloatToByte(parms.x);
  const std::uint32_t py = FloatToByte(parms.y);
  const std::uint32_t w = FloatToByte(parms.w);
  simulateParms.list[blockLocation] = z | (px << 8U) | (py << 16U) | (w << 24U);
}

void idWaterSurfaceSim::ApplyRadiusEffect(const idVec3& pos,
                                          const idRenderModel* renderModel,
                                          float radius, float fade) {
  idVec3 localPos{};
  GlobalToLocalPos(pos, localPos, renderModel);
  const int centerX =
      static_cast<int>(std::floor(localPos.x / spacing + 0.5f));
  const int centerY =
      static_cast<int>(std::floor(localPos.y / spacing + 0.5f));
  const int gridRadius =
      static_cast<int>(std::floor(radius / spacing + 0.5f));
  const int minY = std::clamp(centerY - gridRadius, 1,
                              static_cast<int>(height) - 1);
  const int maxY = std::clamp(centerY + gridRadius, 1,
                              static_cast<int>(height) - 1);
  const int minX = std::clamp(centerX - gridRadius, 1,
                              static_cast<int>(width) - 1);
  const int maxX = std::clamp(centerX + gridRadius, 1,
                              static_cast<int>(width) - 1);
  const float radiusSquared = radius * radius;

  for (int gridY = minY; gridY < maxY; ++gridY) {
    for (int gridX = minX; gridX < maxX; ++gridX) {
      const float deltaY = gridY * spacing - localPos.y;
      const float deltaX = gridX * spacing - localPos.x;
      const float distanceSquared =
          deltaY * deltaY + deltaX * deltaX;
      if (radiusSquared <= distanceSquared)
        continue;

      int blockLocation = 0;
      GridPosToBlockLoc(gridX, gridY, numBlocksX, numBlocksY,
                        &blockLocation);
      const float oldGradient = vertGradient.list[blockLocation] / 255.0f;
      const float radiusFade =
          (std::fmax(radiusSquared - distanceSquared, 0.0f) / radiusSquared) *
          fade;
      vertGradient.list[blockLocation] =
          FloatToByte(oldGradient - radiusFade);
    }
  }
}

void idWaterSurfaceSim::ClearRadiusEffects() {
  std::memset(vertGradient.list, 255,
              static_cast<std::size_t>(vertGradient.size));
}

bool idWaterSurfaceSim::GetSurfacePositionAtPoint(
    idVec3& pos, idRenderModelWaterSurface* waterSurfaceModel,
    float& retWaveHeight, idVec3& normal) {
  idRenderModel* renderModel =
      reinterpret_cast<idRenderModel*>(waterSurfaceModel);
  idWaterSurfaceRenderModelView& model =
      idWaterSurfaceRenderModelView::From(renderModel);
  idVec3 localPos{};
  GlobalToLocalPos(pos, localPos, renderModel);

  const float gridX = localPos.x / spacing;
  int lowX = static_cast<int>(std::floor(gridX + 0.5f));
  int highX = lowX;
  if (lowX < static_cast<int>(width) - 1) {
    if (static_cast<float>(lowX) <= gridX)
      ++highX;
    else
      --lowX;
  } else {
    highX = static_cast<int>(width) - 1;
    lowX = static_cast<int>(width) - 2;
  }

  const float gridY = localPos.y / spacing;
  int lowY = static_cast<int>(std::floor(gridY + 0.5f));
  int highY = lowY;
  if (lowY < static_cast<int>(height) - 1) {
    if (static_cast<float>(lowY) <= gridY)
      ++highY;
    else
      --lowY;
  } else {
    highY = static_cast<int>(height) - 1;
    lowY = static_cast<int>(height) - 2;
  }
  if (lowX < 0 || highX < 0 || lowY < 0 || highY < 0)
    return false;

  const float fractionX = gridX - static_cast<float>(lowX);
  const float fractionY = gridY - static_cast<float>(lowY);
  const bool sameParity = ((lowX & 1) == (lowY & 1));
  int triangleX[3]{};
  int triangleY[3]{};
  if (sameParity) {
    triangleX[0] = lowX;
    triangleY[0] = lowY;
    triangleX[1] = highX;
    triangleY[1] = highY;
    if (fractionX > fractionY) {
      triangleX[2] = highX;
      triangleY[2] = lowY;
    } else {
      triangleX[2] = lowX;
      triangleY[2] = highY;
    }
  } else {
    triangleX[0] = highX;
    triangleY[0] = lowY;
    triangleX[1] = lowX;
    triangleY[1] = highY;
    if (1.0f - fractionX <= fractionY) {
      triangleX[2] = highX;
      triangleY[2] = highY;
    } else {
      triangleX[2] = lowX;
      triangleY[2] = lowY;
    }
  }

  idVec3 triangle[3]{};
  const float* heights = waveHeight[currentDeferred].list;
  for (int index = 0; index < 3; ++index) {
    int blockLocation = 0;
    GridPosToBlockLoc(triangleX[index], triangleY[index], numBlocksX,
                      numBlocksY, &blockLocation);
    triangle[index] = {triangleX[index] * spacing,
                       triangleY[index] * spacing, heights[blockLocation]};
  }

  normal = (triangle[0] - triangle[2]).Cross(triangle[1] - triangle[2]);
  normal.Normalize();
  const idVec3 rayOrigin{localPos.x, localPos.y,
                         localPos.z - kRayStartOffset};
  const float denominator = normal * kUp;
  float distance = static_cast<float>(highY);
  if (denominator != 0.0f) {
    distance = -((normal * rayOrigin) - (normal * triangle[2])) /
               denominator;
  }
  const idVec3 surfacePosition = rayOrigin + kUp * distance;
  retWaveHeight = surfacePosition.z;

  pos = ModelToGlobal(model, surfacePosition);
  pos += origin;

  // Retail also transformed the three selected triangle points into temporary
  // world-space positions before returning. Preserve those floating-point
  // operations even though the temporary values are not part of the API.
  for (idVec3& point : triangle) {
    point = ModelToGlobal(model, point);
    point += origin;
  }
  return true;
}

idWaterSurfaceSim::~idWaterSurfaceSim() {
  for (idList<float, 54>& map : waveHeight)
    map.ClearFree();
  for (idList<waterDisturbanceParms_t, 54>& disturbances :
       waterDisturbances)
    disturbances.ClearFree();
  dummyBlock.ClearFree();
  simulateParms.ClearFree();
  vertGradient.ClearFree();
}

idWaterSurfaceSim::idWaterSurfaceSim(const idBounds& bounds, float s,
                                     float waterDamping, float waterSpeed,
                                     float step)
    : width(0), height(0), spacing(s), damping(waterDamping),
      speed(waterSpeed), timeStep(step), visible(false), origin(vec3_origin),
      rnd{0}, currentDeferred(0), numDisturbances(0), allocWidth(0),
      allocHeight(0), numBlocksX(0), numBlocksY(0) {
  const idVec3 extents = bounds.b[1] - bounds.b[0];
  if (bounds.b[0].x >= bounds.b[1].x ||
      bounds.b[0].y >= bounds.b[1].y ||
      bounds.b[0].z >= bounds.b[1].z ||
      extents.x * extents.y * extents.z < 0.01f) {
    idLib::Warning("Dynamic water has an invalid bounds");
  }

  origin = {bounds.b[0].x, bounds.b[0].y, bounds.b[1].z};
  const float waterWidth = std::ceil(extents.x);
  const float waterHeight = std::ceil(extents.y);
  const float finestSpacing = std::sqrt(std::fmax(
      (waterHeight * waterWidth) / 65535.0f,
      std::numeric_limits<float>::min()));
  spacing = std::fmax(spacing, finestSpacing);
  width = static_cast<unsigned int>(
      std::max(static_cast<int>(std::floor(waterWidth / spacing)), 2));
  height = static_cast<unsigned int>(
      std::max(static_cast<int>(std::floor(waterHeight / spacing)), 2));
  allocWidth = (static_cast<int>(width) + 15) & ~15;
  allocHeight = (static_cast<int>(height) + 15) & ~15;
  numBlocksX = allocWidth / kWaterBlockSize;
  numBlocksY = allocHeight / kWaterBlockSize;
  const int allocationCount = allocWidth * allocHeight;

  for (idList<float, 54>& map : waveHeight)
    map.SetNum(allocationCount, 0.0f);
  for (idList<waterDisturbanceParms_t, 54>& disturbances :
       waterDisturbances)
    disturbances.SetNum(kMaximumDisturbances);
  dummyBlock.SetNum(kDummyBlockElements, 0.0f);
  simulateParms.SetNum(allocationCount, 0U);
  vertGradient.SetNum(allocationCount, static_cast<unsigned char>(255));
}
