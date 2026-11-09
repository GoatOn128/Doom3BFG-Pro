#define WOLFTNO_RETAIL_IDMEM_SINGLETON_ABI
#define WOLFTNO_RETAIL_IDSTR_INLINE_FREEDATA_ABI
#define WOLFTNO_RETAIL_TRIVIAL_IDINDEX_ABI
#define WOLFTNO_RETAIL_BLOCKALLOC_DTOR_ABI
#include "engine/gamelib/aas2/aas2_core_abi.h"
#undef WOLFTNO_RETAIL_BLOCKALLOC_DTOR_ABI
#undef WOLFTNO_RETAIL_TRIVIAL_IDINDEX_ABI
#undef WOLFTNO_RETAIL_IDSTR_INLINE_FREEDATA_ABI
#undef WOLFTNO_RETAIL_IDMEM_SINGLETON_ABI

#include "shared/idlib/lib.h"

#include <cstdint>
#include <new>

idAAS2::~idAAS2() = default;

idAAS2Local::idAAS2Local() : clipModelLinkAllocator(false) {
  file = nullptr;
  defaulted = false;
  areaCacheIndex = nullptr;
  areaCacheIndexSize = 0;
  portalCacheIndex = nullptr;
  portalCacheIndexSize = 0;
  areaUpdate = nullptr;
  portalUpdate = nullptr;
  goalAreaTravelTimes = nullptr;
  areaTravelTimes = nullptr;
  numAreaTravelTimes = 0;
  cacheListStart = nullptr;
  cacheListEnd = nullptr;
  totalCacheMemory = 0;
  groundSpeedMultiplier = 1.0f;
  obstaclePVS = nullptr;
  obstaclePVSAreaNum = 0;
  waterSpeedMultiplier = 1.0f;
  numObstaclePVSBytes = 0;
  areaClipModels = nullptr;
  verify.currentIndex = 0;
  verify.recoveredCount = 0;
  verify.invalidVertices = nullptr;
  verify.clipModel = nullptr;
}

bool idAAS2Local::IsDefault() const { return defaulted; }

void idAAS2Local::Shutdown() {
  if (file == nullptr) {
    return;
  }

  ShutdownRouting();
  ShutdownObstaclePVS();
  file = nullptr;
  areaCacheIndex = nullptr;
  portalCacheIndex = nullptr;
  areaUpdate = nullptr;
  portalUpdate = nullptr;
  goalAreaTravelTimes = nullptr;
  areaTravelTimes = nullptr;
  cacheListStart = nullptr;
  cacheListEnd = nullptr;
  obstaclePVS = nullptr;
  areaClipModels = nullptr;
}

int idAAS2Local::PointAreaNum(idAAS2TreeIndex tree,
                              const idVec3& origin) const {
  return file->PointAreaNum(tree, origin);
}

int idAAS2Local::PointAreaNumMaxheight(idAAS2TreeIndex tree,
                                       const idVec3& origin,
                                       float maxHeight) const {
  if (!(maxHeight > 0.0f)) {
    return file->PointAreaNum(tree, origin);
  }
  return file->PointAreaNumMaxheight(tree, origin, maxHeight);
}

int idAAS2Local::PointReachableAreaNum(
    idAAS2TreeIndex tree, const idVec3& origin,
    const idBounds& /*searchBounds*/, int areaFlags,
    int excludeTravelFlags) const {
  return file->PointReachableAreaNum(tree, origin, areaFlags,
                                     excludeTravelFlags);
}

int idAAS2Local::BoundsReachableAreaNum(idAAS2TreeIndex tree,
                                        const idBounds& bounds,
                                        int areaFlags,
                                        int excludeTravelFlags) const {
  return file->BoundsReachableAreaNum(tree, bounds, areaFlags,
                                      excludeTravelFlags);
}

int idAAS2Local::BoundsReachableAreaNum(idAAS2TreeIndex tree,
                                        const idVec3& origin,
                                        const idBounds& bounds,
                                        int areaFlags,
                                        int excludeTravelFlags) const {
  idBounds translated;
  translated.b[1].x = origin.x + bounds.b[1].x;
  translated.b[1].y = origin.y + bounds.b[1].y;
  translated.b[1].z = origin.z + bounds.b[1].z;
  translated.b[0].x = origin.x + bounds.b[0].x;
  translated.b[0].y = origin.y + bounds.b[0].y;
  translated.b[0].z = origin.z + bounds.b[0].z;
  return file->BoundsReachableAreaNum(tree, translated, areaFlags,
                                      excludeTravelFlags);
}

bool idAAS2Local::PushPointIntoNearestAreaNum(int& areaNum,
                                              idVec3& point) const {
  return file->PushPointIntoNearestAreaNum(areaNum, point);
}

idVec3 idAAS2Local::AreaCenter(int areaNum) const {
  return file->AreaCenter(areaNum);
}

bool idAAS2Local::Trace(idAAS2TreeIndex tree, aas2Trace_t& trace,
                        const idVec3& start, const idVec3& end) const {
  return file->Trace(tree, trace, start, end);
}

bool idAAS2Local::TraceHeight(idAAS2TreeIndex tree,
                              aas2TraceHeight_t& trace,
                              const idVec3& start,
                              const idVec3& end) const {
  return file->TraceHeight(tree, trace, start, end);
}

bool idAAS2Local::TraceFloor(aas2TraceFloor_t& trace,
                             const idVec3& start, int startAreaNum,
                             const idVec3& end, int travelFlags,
                             bool allowFloorNormalChange,
                             bool ignoreGravityDirectionDistance,
                             bool ignoreSameArea) const {
  return file->TraceFloor(trace, start, startAreaNum, end, 0, travelFlags,
                          allowFloorNormalChange,
                          ignoreGravityDirectionDistance, ignoreSameArea);
}

bool idAAS2Local::GetAASAnim(idAAS2AnimIndex index,
                             const aas2AnimName_t*& name_) const {
  return file->GetAASAnim(index, name_);
}

idAAS2AnimIndex
idAAS2Local::GetAASAnimIndexByName(const char* name_) const {
  return file->GetAASAnimIndexByName(name_);
}

bool idAAS2Local::GetAASDependency(
    idAAS2DependencyIndex index,
    const aas2DependencyName_t*& name_) const {
  return file->GetAASDependency(index, name_);
}

idAAS2DependencyIndex
idAAS2Local::GetAASDependencyIndexByName(const char* name_) const {
  return file->GetAASDependencyIndexByName(name_);
}

bool idAAS2Local::GetAASInteractionEntity(
    idAAS2InteractionEntIndex index,
    const aas2InteractionEntityName_t*& name_) const {
  return file->GetAASInteractionEntity(index, name_);
}

idAAS2InteractionEntIndex
idAAS2Local::GetAASInteractionEntityIndexByName(const char* name_) const {
  return file->GetAASInteractionEntityIndexByName(name_);
}

bool idAAS2Local::GetAASTraversalNameIndex(
    idAAS2TraversalNameIndex index,
    const aas2TraversalEntityName_t*& name_) const {
  return file->GetAASTraversalNameIndex(index, name_);
}

idAAS2TraversalNameIndex
idAAS2Local::GetAASTraversalNameIndexByName(const char* name_) const {
  return file->GetAASTraversalNameIndexByName(name_);
}

int idAAS2Local::GetAASTraversalIndexByNameIndex(
    idAAS2TraversalNameIndex index) const {
  return file->GetAASTraversalIndexByNameIndex(index);
}

bool idAAS2Local::SetTraversalFlag(int index, int flags) {
  if (!file->SetTraversalFlag(index, flags)) {
    return false;
  }

  int startArea;
  int goalArea;
  if (file->GetAASTraversalAreas(index, startArea, goalArea)) {
    RemoveRoutingCacheUsingArea(startArea);
    RemoveRoutingCacheUsingArea(goalArea);
  }
  return true;
}

bool idAAS2Local::ClearTraversalFlag(int index, int flags) {
  if (!file->ClearTraversalFlag(index, flags)) {
    return false;
  }

  int startArea;
  int goalArea;
  if (file->GetAASTraversalAreas(index, startArea, goalArea)) {
    RemoveRoutingCacheUsingArea(startArea);
    RemoveRoutingCacheUsingArea(goalArea);
  }
  return true;
}

void idAAS2Local::ClipGridToAreas(const idVec3& startOrigin,
                                  int startAreaNum, int travelFlags,
                                  int cellSize, int cellsWide,
                                  unsigned char* reachable) const {
  file->ClipGridToAreas(startOrigin, startAreaNum, travelFlags, cellSize,
                        cellsWide, reachable);
}

idAAS2TreeIndex idAAS2Local::GetTreeForFloorNormal(
    const idVec3& floorNormal) const {
  return file->GetTreeForFloorNormal(floorNormal);
}

int idAAS2Local::GetNumAreasInTree(idAAS2TreeIndex treeNum) const {
  return file->GetNumAreasInTree(treeNum);
}

bool idAAS2Local::Init(const char* mapName, unsigned int mapFileCRC) {
  if (file != nullptr && idStr::Icmp(mapName, file->name.str) == 0 &&
      mapFileCRC == file->crc && mapFileCRC != 0) {
    idLib::Printf("Keeping %s\n", file->name.str);
    return true;
  }

  Shutdown();
  file = static_cast<idAAS2File*>(
      idAAS2File::resourceList.Load(mapName, true, false));
  if (file->resourceError != nullptr) {
    idLib::Warning("Couldn't load AAS file: '%s'", mapName);
    defaulted = true;
  }
  file->ResetCover();
  SetupRouting();
  SetupObstaclePVS();
  return true;
}

const idAAS2Settings* idAAS2Local::GetSettings() const {
  return &file->settings;
}

void idAAS2Local::Stats() const {
  idLib::Printf("[%s]\n", file->name.str);
  idLib::Printf("%6d areas\n", file->areas.num - 1);
  idLib::Printf("%6d kB file size\n", file->MemorySize() >> 10);
  RoutingStats();
}

bool idAAS2Local::PushPointIntoAreaNum(int areaNum,
                                       idVec3& origin) const {
  if ((file->areas.list[areaNum].flags & 0x10u) != 0) {
    return false;
  }
  return file->PushPointIntoAreaNum(areaNum, origin);
}

void idAAS2Local::GetEdgeVertexNumbers(int edgeNum, int* verts) const {
  const std::uint32_t bits = static_cast<std::uint32_t>(edgeNum);
  const std::uint32_t mask = 0u - (bits >> 31);
  const int absoluteEdgeNum = static_cast<int>((bits ^ mask) - mask);
  const unsigned int direction =
      static_cast<unsigned int>(edgeNum) >> 31;
  const aas2Edge_t& edge = file->edges.list[absoluteEdgeNum];
  verts[0] = edge.vertexNum[direction];
  verts[1] = edge.vertexNum[direction ^ 1u];
}

void idAAS2Local::GetEdge(int edgeNum, idVec3& start, idVec3& end) const {
  const std::uint32_t bits = static_cast<std::uint32_t>(edgeNum);
  const std::uint32_t mask = 0u - (bits >> 31);
  const int absoluteEdgeNum = static_cast<int>((bits ^ mask) - mask);
  const unsigned int direction =
      static_cast<unsigned int>(edgeNum) >> 31;
  const aas2Edge_t& edge = file->edges.list[absoluteEdgeNum];
  start = file->vertices.list[edge.vertexNum[direction]];
  end = file->vertices.list[edge.vertexNum[direction ^ 1u]];
}

int idAAS2Local::GetAreaFlags(int areaNum) const {
  return file->areas.list[areaNum].flags;
}

int idAAS2Local::GetEdgeFlags(int edgeNum) const {
  const std::uint32_t bits = static_cast<std::uint32_t>(edgeNum);
  const std::uint32_t mask = 0u - (bits >> 31);
  const int absoluteEdgeNum = static_cast<int>((bits ^ mask) - mask);
  return file->edges.list[absoluteEdgeNum].flags;
}

const idPlane& idAAS2Local::GetPlane(int index) const {
  return file->planes.list[index];
}

int idAAS2Local::GetNumAreas() const { return file->areas.num; }

const aas2Area_t& idAAS2Local::GetArea(int areaNum) const {
  return file->areas.list[areaNum];
}

const aas2Reachability_t& idAAS2Local::GetReachability(
    idAAS2ReachabilityIndex reachNum) const {
  return file->reachabilities.list[reachNum.value];
}

const aas2Traversal_t&
idAAS2Local::GetTraversal(int traversalNum) const {
  return file->traversalPoints.list[traversalNum];
}

int idAAS2Local::GetNumTrees() const { return file->trees.num; }

idAAS2TreeIndex idAAS2Local::GetTree(int index) const {
  return idAAS2TreeIndex(index);
}

const idVec3&
idAAS2Local::GetFloorNormalForTree(idAAS2TreeIndex treeNum) const {
  return file->trees.list[treeNum.value].floorNormal;
}

float idAAS2Local::GetGroundSpeedMultiplier() const {
  return groundSpeedMultiplier;
}

const idVec3& idAAS2Local::GetFloorNormalForArea(int areaNum) const {
  const idAAS2TreeIndex treeNum = file->GetTreeForArea(areaNum);
  return file->trees.list[treeNum.value].floorNormal;
}

idAAS2* idAAS2::Alloc() {
  void* const memory = idMem::Singleton().AllocWithLocation(
      "w:\\build\\tech5\\shared\\idlib\\Heap.h(53) : TAG_NEW",
      sizeof(idAAS2Local), TAG_NEW, false, ALIGN_16, HEAP_DEFAULTHEAP);
  idAAS2* const result =
      memory != nullptr ? ::new (memory) idAAS2Local : nullptr;
  _ReadWriteBarrier();
  return result;
}
