#pragma once

#include "engine/cm/collisionmodel.h"

#include <unordered_map>

struct cmGridCell_t {
  std::vector<std::size_t> polygons;
  bool active{};
};

class idGenGridModel {
public:
  idGenGridModel();
  ~idGenGridModel();
  void FreeData();
  bool LoadBinary(const char* path);
  idBounds bounds{};
  float cellSize{64.0f};
  std::vector<cmPolygon_t> polygons;
};

class idGridRasterize {
public:
  static void RasterizePolygon(const cmPolygon_t& polygon, float cellSize,
                               std::unordered_map<std::uint64_t, cmGridCell_t>& cells,
                               std::size_t polygonIndex);
};

class idCollisionGrid {
public:
  virtual ~idCollisionGrid();
  virtual bool IsValid() const = 0;
};

class idCollisionGridLocal final : public idCollisionGrid {
public:
  bool Load_Binary(const char* path);
  bool LoadResource(const char* path);
  bool ReloadIfStale();
  bool IsValid() const override;
  static const std::vector<idCollisionGridLocal*>& GetResourceList();
  idStr GetBinaryFileName(const char* source) const;
  void CreateState();
  void InactivateFill();
  const cmGridCell_t* FindCell(int x, int y, int z) const;

  idGenGridModel model;
  std::unordered_map<std::uint64_t, cmGridCell_t> cells;
  bool valid{};
};
