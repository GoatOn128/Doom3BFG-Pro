#include "engine/cm/collisiongrid.h"

#include <algorithm>
#include <cmath>
#include <filesystem>

namespace {
std::uint64_t GridKey(int x, int y, int z) {
  constexpr std::uint64_t mask = (1ull << 21) - 1;
  return (static_cast<std::uint64_t>(x) & mask) | ((static_cast<std::uint64_t>(y) & mask) << 21) |
         ((static_cast<std::uint64_t>(z) & mask) << 42);
}
} // namespace

idPlane CM_GetPolygonPlane(const cmPolygon_t& polygon) {
  return polygon.plane;
}

idGenGridModel::idGenGridModel() = default;
idGenGridModel::~idGenGridModel() {
  FreeData();
}
idCollisionGrid::~idCollisionGrid() = default;

const std::vector<idCollisionGridLocal*>& idCollisionGridLocal::GetResourceList() {
  static std::vector<idCollisionGridLocal*> resources;
  return resources;
}

bool idCollisionGridLocal::IsValid() const {
  return valid;
}

bool idCollisionGridLocal::LoadResource(const char* path) {
  const bool loaded = Load_Binary(path);
  if (loaded) {
    auto& resources = const_cast<std::vector<idCollisionGridLocal*>&>(GetResourceList());
    if (std::find(resources.begin(), resources.end(), this) == resources.end()) {
      resources.push_back(this);
    }
  }
  return loaded;
}

bool idCollisionGridLocal::ReloadIfStale() {
  return valid && !model.polygons.empty();
}

void idGenGridModel::FreeData() {
  polygons.clear();
  bounds.Clear();
}

bool idGenGridModel::LoadBinary(const char* path) {
  idCollisionModelLocal collisionModel;
  if (!collisionModel.Load_Binary(path)) {
    return false;
  }
  bounds = collisionModel.bounds;
  polygons = collisionModel.polygons;
  return true;
}

void idGridRasterize::RasterizePolygon(const cmPolygon_t& polygon, float cellSize,
                                       std::unordered_map<std::uint64_t, cmGridCell_t>& cells,
                                       std::size_t polygonIndex) {
  if (cellSize <= 0.0f) {
    return;
  }
  int minimum[3]{};
  int maximum[3]{};
  for (int axis = 0; axis < 3; ++axis) {
    minimum[axis] = static_cast<int>(std::floor(polygon.bounds[0][axis] / cellSize));
    maximum[axis] = static_cast<int>(std::floor(polygon.bounds[1][axis] / cellSize));
  }
  for (int z = minimum[2]; z <= maximum[2]; ++z) {
    for (int y = minimum[1]; y <= maximum[1]; ++y) {
      for (int x = minimum[0]; x <= maximum[0]; ++x) {
        cells[GridKey(x, y, z)].polygons.push_back(polygonIndex);
      }
    }
  }
}

idStr idCollisionGridLocal::GetBinaryFileName(const char* source) const {
  std::filesystem::path path(source ? source : "collision");
  path.replace_extension(".bcg");
  return idStr(path.string().c_str());
}

bool idCollisionGridLocal::Load_Binary(const char* path) {
  valid = model.LoadBinary(path);
  if (valid) {
    CreateState();
  }
  return valid;
}

void idCollisionGridLocal::CreateState() {
  cells.clear();
  for (std::size_t i = 0; i < model.polygons.size(); ++i) {
    idGridRasterize::RasterizePolygon(model.polygons[i], model.cellSize, cells, i);
  }
  for (auto& entry : cells) {
    entry.second.active = true;
  }
}

void idCollisionGridLocal::InactivateFill() {
  for (auto& entry : cells) {
    entry.second.active = false;
  }
}

const cmGridCell_t* idCollisionGridLocal::FindCell(int x, int y, int z) const {
  const auto found = cells.find(GridKey(x, y, z));
  return found == cells.end() ? nullptr : &found->second;
}
