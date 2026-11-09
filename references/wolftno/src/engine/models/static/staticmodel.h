#pragma once

#include "engine/decls/decl.h"
#include "engine/models/rendermodel.h"
#include "engine/models/sourcesurface.h"
#include "engine/models/skinremap.h"

#include <cstddef>
#include <cstdint>

class idASEModel;
class idOBJModel;
struct st_lwLayer;
struct st_lwObject;

class idStaticModelSurface : public idRenderModelSurface {
public:
  idList<skinRemap_t, 5> skinRemaps;
};

class idStaticModel : public idResource {
public:
  static idTypedResourceList<idStaticModel> resourceList;

  idStaticModel();
  ~idStaticModel() override;
  idResourceList* GetResourceList() const override;
  void LoadResource() override;
  bool ReloadIfStale() override;
  void Print() override { static_cast<const idStaticModel*>(this)->Print(); }
  void Print() const;
  void PristineLoadResource();
  void FreeCPUData();
  void FreeSurfaces();
  void PostLoadSurface(int surfaceIndex, const idDeclSkins* skins);
  void MakeDefaultModel(float minimum, float maximum);
  void MakeSimpleCube(float minimum, float maximum);
  void MakeExtrudeBox();
  void FinishStaticModel(bool cleanup);
  void AddSurface(const idStaticModelSurface& surface);
  bool GenerateStaticModel(const char* sourceFile);
  void WriteStaticBModel(const char* fileName, fsPath_t basePath) const;
  static void WriteStaticBModel(
      const char* fileName, std::uint32_t sourceTimeStamp,
      const idList<idStaticModelSurface, 5>& surfaces,
      const idList<sourceSurface_t, 5>& sourceSurfaces,
      const idVec3& referencePosition, fsPath_t basePath);
  int CPUMemory() const;
  int GPUMemory() const;
  void List() const override;

  // Implemented by staticmodelgenerator.cpp.  Keeping the retail member
  // boundary here lets LoadResource retain its recovered control flow while
  // that compiland is ported independently.
  bool GenerateBinaryModelFile(const char* sourceFile,
                               const char* binaryFile);

  bool pristine{};
  bool cmodel{};
  bool discardReferencePosition{};
  bool isReloadable{};
  char binaryFileName[256]{};
  std::uint32_t binaryTimeStamp{};
  std::uint32_t sourceTimeStamp{};
  int reloadCount{};
  bool modelIsAutosprite{};
  bool usesTransparencySort{};
  std::uint8_t recoveredBoundsPadding[2]{};
  idBounds referenceBounds{};
  idVec3 referencePosition{};
  bool sharedSurfaces{};
  std::uint8_t recoveredFlagsPadding[3]{};
  int modelLoadFlags{};
  idList<idStaticModelSurface, 84> surfaces;
  idList<sourceSurface_t, 5> sourceSurfaces;
  idList<const idMaterial*, 5> materials;

private:
  friend struct StaticModelGeneratorValidationAccess;
  void UpdateBuffers();
  bool ReadStaticBModel(const char* fileName, bool allowPreviousVersion);
  void BuildAutospriteData();
  void ConvertToTriStrips();
  void MergeSurfacesAndBuildSourceSurfaces();
  void BuildMergeList(const idList<idStr, 5>& materialNames,
                      idList<const idMaterial*, 5>& materials,
                      idList<int, 5>& mergeTo,
                      idList<const idMaterial*, 5>& uniqueMaterials,
                      idList<int, 5>& uniqueMaterialNums);
  bool ConvertASEToModelSurfaces(const idASEModel* model);
  bool ConvertLWOToModelSurfaces(const st_lwObject* object,
                                 const st_lwLayer* layer,
                                 float vertexEpsilon,
                                 float texCoordEpsilon,
                                 float normalEpsilon);
  bool ConvertOBJToModelSurfaces(const idOBJModel* model);
};

class idStaticModelManager {
public:
  struct idStats {
    int totalCPUMem{};
    int totalGPUMem{};
    int inUse{};
    int totalVerts{};
    int totalTriangles{};
    int totalSurfaces{};
  };

  virtual ~idStaticModelManager() = default;
  static void operator delete(void* pointer) noexcept {
    idMem::Singleton()->Free(pointer, ALIGN_16);
  }
  static void operator delete(void* pointer, std::size_t) noexcept {
    operator delete(pointer);
  }
  virtual void Init() {}
  virtual const idStaticModel* FindStaticModel(const char* name,
                                               bool load) = 0;
  virtual void AddStaticModel(idStaticModel*) {}
  virtual void ReloadModels() {}
  virtual void CalculateStats(idStats&) {}
};

extern idStaticModelManager* staticModelManager;

static_assert(sizeof(idStaticModelSurface) == 128,
              "idStaticModelSurface ABI drift");
static_assert(sizeof(sourceSurface_t) == 24,
              "sourceSurface_t ABI drift");
static_assert(offsetof(sourceSurface_t, mtrChecksum) == 8,
              "sourceSurface_t checksum offset drift");
static_assert(offsetof(sourceSurface_t, lastVertex) == 20,
              "sourceSurface_t last-vertex offset drift");
static_assert(offsetof(idStaticModelSurface, skinRemaps) == 104,
              "idStaticModelSurface remap offset drift");
static_assert(sizeof(idStaticModel) == 440, "idStaticModel ABI drift");
static_assert(sizeof(idStaticModelManager::idStats) == 24,
              "idStaticModelManager::idStats ABI drift");
static_assert(offsetof(idStaticModelManager::idStats, totalCPUMem) == 0x0,
              "idStaticModelManager::idStats CPU-memory offset drift");
static_assert(offsetof(idStaticModelManager::idStats, inUse) == 0x8,
              "idStaticModelManager::idStats in-use offset drift");
static_assert(offsetof(idStaticModelManager::idStats, totalVerts) == 0xC,
              "idStaticModelManager::idStats vertex offset drift");
static_assert(offsetof(idStaticModelManager::idStats, totalTriangles) == 0x10,
              "idStaticModelManager::idStats triangle offset drift");
static_assert(offsetof(idStaticModelManager::idStats, totalSurfaces) == 0x14,
              "idStaticModelManager::idStats surface offset drift");
static_assert(offsetof(idStaticModel, binaryFileName) == 52,
              "idStaticModel filename offset drift");
static_assert(offsetof(idStaticModel, referenceBounds) == 324,
              "idStaticModel bounds offset drift");
static_assert(offsetof(idStaticModel, surfaces) == 368,
              "idStaticModel surfaces offset drift");
