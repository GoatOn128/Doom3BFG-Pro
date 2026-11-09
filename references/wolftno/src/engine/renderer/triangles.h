#pragma once

#include "engine/renderer/bufferobject.h"
#include "shared/idlib/bv/bounds.h"
#include "shared/idlib/geometry/drawvert.h"
#include "shared/idlib/sys/sys_alloc.h"

#include <cstddef>
#include <cstdint>
#include <new>

template <typename T, int DefaultTag> class idList;
class idDeclSkins;
class idFile;
class idMaterial;
struct skinRemap_t;
struct sourceSurface_t;

class idVertexScaleAndBias {
public:
  idVec3 xyzScale;
  idVec3 xyzBias;
  idVec2 stScale;
  idVec2 stBias;
};

class idTriangles {
public:
  static constexpr int MAX_TRIANGLE_VERTS = 0x10000;

  static __forceinline void* operator new(std::size_t size) noexcept {
    return idMem::Singleton()->AllocWithLocation(
        "w:\\build\\tech5\\shared\\idlib\\Heap.h(95) : tag", size,
        TAG_TRIANGLES, false, ALIGN_16, HEAP_DEFAULTHEAP);
  }
  static __forceinline void* operator new(
      std::size_t size, const std::nothrow_t&) noexcept {
    return operator new(size);
  }
  static __forceinline void* operator new(std::size_t size,
                                          memTag_t tag) noexcept {
    return idMem::Singleton()->AllocWithLocation(
        "w:\\build\\tech5\\shared\\idlib\\Heap.h(95) : tag", size, tag,
        false, ALIGN_16, HEAP_DEFAULTHEAP);
  }
  static __forceinline void* operator new(std::size_t,
                                          void* memory) noexcept {
    return memory;
  }
  static __forceinline void operator delete(void* memory) noexcept {
    if (memory != nullptr) idMem::Singleton()->Free(memory, ALIGN_16);
  }
  static __forceinline void operator delete(void* memory,
                                             std::size_t) noexcept {
    operator delete(memory);
  }
  static __forceinline void operator delete(
      void* memory, const std::nothrow_t&) noexcept {
    operator delete(memory);
  }
  static __forceinline void operator delete(void* memory,
                                             memTag_t) noexcept {
    operator delete(memory);
  }
  static __forceinline void operator delete(void*, void*) noexcept {}

  idTriangles();
  idTriangles(int numVerts, int numIndexes);
  ~idTriangles();

  const idDrawVert* GetFullSizeDrawVerts();
  idTriangles* CreateStandardTriangles();
  int CPUMemoryUsed();
  int GPUMemoryUsed();
  void FreeCPUData();
  void FreeData();
  void ResizeVerts(int newNumVerts);
  void ResizeIndexes(int newNumIndexes);
  void DeriveTangents();
  void VmtrMapTexCoords(int firstVertex, int lastVertex, const idVec4 map);
  void VmtrMapTexCoordsWithSkins(
      idList<sourceSurface_t, 5>& sourceSurfaces, int renderSurface,
      const idMaterial* originalMaterial, const idDeclSkins* skins,
      idList<skinRemap_t, 5>& skinRemaps,
      idList<const idMaterial*, 5>& usedMaterials);
  idBounds BoundsFromVertScan();
  void UpdateVertexBuffer();
  void UpdateIndexBuffer();
  void Cleanup(bool optimize);
  void OptimizeIndexOrder();
  void OptimizeVertexOrder();
  void AllocStaticTriSurfVerts(int numVerts);
  void AllocStaticTriSurfIndexes(int numIndexes);
  void RemoveDegenerate();
  void DuplicateMirroredVertexes();
  idTriangles* CopyStaticTriSurf();
  void MergeTriangles(const idTriangles* source);
  void AddCubeFace(const idVec3& v1, const idVec3& v2, const idVec3& v3,
                   const idVec3& v4);
  int CreateQuads(std::uint16_t* quads);
  void TurnIntoQuads();
  void WriteToFile(idFile* file);
  void ReadFromFile(idFile* file);

  static void RangeCheckIndexes(std::uint16_t* indexes, int numIndexes,
                                int numVerts);
  static void SkipInFile(idFile* file);
  static void TriangleVectors(const idDrawVert* a, const idDrawVert* b,
                              const idDrawVert* c, idVec3& normal,
                              idVec3& tangent, idVec3& biTangent);

  // Compatibility forms retained for recovered callers that still use the
  // old free-function-style surface helpers.
  static idTriangles* CreateStandardTriangles(const idTriangles* source) {
    return source != nullptr
               ? const_cast<idTriangles*>(source)->CreateStandardTriangles()
               : nullptr;
  }
  static idTriangles* CopyStaticTriSurf(const idTriangles* source) {
    return source != nullptr
               ? const_cast<idTriangles*>(source)->CopyStaticTriSurf()
               : nullptr;
  }
  static void MergeTriangles(idTriangles* destination,
                             const idTriangles* source) {
    if (destination != nullptr) destination->MergeTriangles(source);
  }
  static void AllocStaticTriSurfVerts(idTriangles* triangles, int numVerts) {
    if (triangles != nullptr) triangles->AllocStaticTriSurfVerts(numVerts);
  }
  static void AllocStaticTriSurfIndexes(idTriangles* triangles,
                                        int numIndexes) {
    if (triangles != nullptr) triangles->AllocStaticTriSurfIndexes(numIndexes);
  }
  static void UpdateIndexBuffer(idTriangles* triangles) {
    if (triangles != nullptr) triangles->UpdateIndexBuffer();
  }
  static void UpdateVertexBuffer(idTriangles* triangles) {
    if (triangles != nullptr) triangles->UpdateVertexBuffer();
  }
  void VmtrMapTexCoordsWithSkins(
      idList<sourceSurface_t, 5>* sourceSurfaces, int renderSurface,
      const idMaterial* originalMaterial, const idDeclSkins* skins,
      idList<skinRemap_t, 5>* skinRemaps,
      idList<const idMaterial*, 5>* usedMaterials) {
    VmtrMapTexCoordsWithSkins(*sourceSurfaces, renderSurface, originalMaterial,
                              skins, *skinRemaps, *usedMaterials);
  }
  void CreateBounds();
  float GetSurfaceArea() const;

  idBounds bounds;
  bool generateNormals{};
  int numIndexes{};
  int numVerts{};
  int detailOffset{};
  bool allowGpuHosting{};
  idVertexScaleAndBias vsb{};
  unsigned cpuVertexMask{};
  idDrawVert* verts{};
  std::uint16_t* indexes{};
  unsigned vertexMask{};
  idVertexBuffer vertexBuffer;
  idIndexBuffer indexBuffer;
};

static_assert(sizeof(idVertexScaleAndBias) == 40,
              "idVertexScaleAndBias ABI drift");
static_assert(offsetof(idTriangles, generateNormals) == 24,
              "idTriangles normal-generation offset drift");
static_assert(offsetof(idTriangles, numIndexes) == 28,
              "idTriangles index-count offset drift");
static_assert(offsetof(idTriangles, numVerts) == 32,
              "idTriangles vertex-count offset drift");
static_assert(offsetof(idTriangles, allowGpuHosting) == 40,
              "idTriangles GPU-hosting offset drift");
static_assert(offsetof(idTriangles, vsb) == 44,
              "idTriangles scale/bias offset drift");
static_assert(offsetof(idTriangles, cpuVertexMask) == 84,
              "idTriangles CPU-mask offset drift");
static_assert(offsetof(idTriangles, verts) == 88, "idTriangles verts offset drift");
static_assert(offsetof(idTriangles, indexes) == 96,
              "idTriangles indexes offset drift");
static_assert(offsetof(idTriangles, vertexMask) == 104,
              "idTriangles vertex-mask offset drift");
static_assert(offsetof(idTriangles, vertexBuffer) == 112,
              "idTriangles vertex buffer offset drift");
static_assert(offsetof(idTriangles, indexBuffer) == 136,
              "idTriangles index buffer offset drift");
static_assert(sizeof(idTriangles) == 160, "idTriangles ABI drift");

void WriteSourceSurfaces(idFile* file,
                         const idList<sourceSurface_t, 5>& sourceSurfaces);
void ReadSourceSurfaces(idFile* file,
                        idList<sourceSurface_t, 5>& sourceSurfaces,
                        bool skipOver);
