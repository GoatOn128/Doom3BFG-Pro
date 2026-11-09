#include "tungsten/game/ai/aasviscache.h"
#include "tungsten/game/game_runtime.h"
#include "shared/idlib/lib.h"

#include <utility>

idAASVisBitSet::idAASVisBitSet(std::size_t bitCount) : bits(bitCount) {
  const std::size_t bytes = ByteCount();
  if (bytes != 0) {
    buffer = new std::uint8_t[bytes]{};
    free = true;
  }
}

// DIA: idAASVisCache::Init
void idAASVisCache::Init(const idAAS2* newAAS, int newSubDivisionSize,
                         float newEyeHeight) {
  eyeHeight = newEyeHeight;
  aas = newAAS;
  subDivisionSize = newSubDivisionSize;
}

// DIA: idAASVisCache::idAASVisCache
idAASVisCache::idAASVisCache()
    : aas(nullptr), eyeHeight(0.0F), subDivisionSize(0), visPoints{} {}

// DIA: idAASVisCache::Size
std::uint64_t idAASVisCache::Size() const {
  std::uint64_t bytes = 40;
  for (const std::unique_ptr<idAASVisPoint>& point : visPoints) {
    bytes += 48;
    for (const std::unique_ptr<idAASVisArea>& area : point->visAreas) {
      bytes += 64 + area->cellVis.ByteCount();
    }
  }
  return bytes;
}

// DIA: idAASVisCache::Cmd_VisCacheInfo_f
void idAASVisCache::Cmd_VisCacheInfo_f(const idCmdArgs*) {
  const idAASVisCache* cache = gameLocal->GetAASVisCache();
  idLib::Printf("AAS Visibility Cache info:\n");

  std::size_t areaCount = 0;
  for (const std::unique_ptr<idAASVisPoint>& point : cache->visPoints) {
    if (point != nullptr) {
      areaCount += point->visAreas.size();
    }
  }
  idLib::Printf("  eyeHeight: %.2f\n", cache->eyeHeight);
  idLib::Printf("  bytes: %d\n", static_cast<int>(cache->Size()));
  idLib::Printf("  cached points: %d\n",
                static_cast<int>(cache->visPoints.size()));
  idLib::Printf("  cached areas: %d\n", static_cast<int>(areaCount));
}

// DIA: idAASVisPoint::Free
void idAASVisPoint::Free() {
  for (std::unique_ptr<idAASVisArea>& area : visAreas) {
    if (area == nullptr) {
      continue;
    }
    if (area->cellVis.buffer != nullptr && area->cellVis.free) {
      delete[] area->cellVis.buffer;
      area->cellVis.buffer = nullptr;
      area->cellVis.bits = 0;
      area->cellVis.free = false;
    }
    area.reset();
  }
}

// DIA: idAASVisCache::Free
// DIA: `idAASVisCache::Free'::`1'::dtor$0
void idAASVisCache::Free() {
  // Keeping released points in a local owner list mirrors the retail cleanup
  // scope and guarantees exception-safe destruction while the cache is freed.
  std::vector<std::unique_ptr<idAASVisPoint>> released;
  released.swap(visPoints);
  for (std::unique_ptr<idAASVisPoint>& point : released) {
    if (point != nullptr) {
      point->Free();
    }
  }
}

// DIA: idAASVisCache::Shutdown
void idAASVisCache::Shutdown() {
  aas = nullptr;
  Free();
}

// DIA: idAASVisCache::~idAASVisCache
idAASVisCache::~idAASVisCache() {
  Free();
}

idAASVisPoint* idAASVisCache::AddPoint() {
  visPoints.push_back(std::make_unique<idAASVisPoint>());
  return visPoints.back().get();
}

idAASVisArea* idAASVisPoint::AddArea(std::size_t bitCount) {
  visAreas.push_back(std::make_unique<idAASVisArea>(bitCount));
  return visAreas.back().get();
}
