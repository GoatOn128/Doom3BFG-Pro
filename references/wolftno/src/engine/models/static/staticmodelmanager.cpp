#include "engine/models/static/staticmodel.h"

#include "engine/animation/md6.h"
#include "engine/renderer/renderersystem.h"
#include "shared/idlib/csystems/cmdsystemlocal.h"
#include "shared/idlib/lib.h"
#include "shared/idlib/sys/sys_alloc.h"
#include "shared/idlib/sys/sys_time.h"
#include "shared/idlib/text/cmdargs.h"

#include <algorithm>
#include <cstdint>
#include <cstring>
#include <new>

namespace {

constexpr memTag_t kStaticModelTag = static_cast<memTag_t>(84);
constexpr char kTaggedNewLocation[] =
    "w:\\build\\tech5\\shared\\idlib\\Heap.h(95) : tag";

std::uint8_t EncodeDirection(float value) {
  const int encoded = static_cast<int>((value + 1.0f) * 127.5f + 0.5f);
  return static_cast<std::uint8_t>(std::clamp(encoded, 0, 255));
}

} // namespace

__forceinline void* operator new(std::size_t size, memTag_t tag) noexcept {
  return idMem::Singleton()->AllocWithLocation(
      kTaggedNewLocation, size, tag, false, ALIGN_16, HEAP_DEFAULTHEAP);
}

__forceinline void operator delete(void* pointer, memTag_t) noexcept {
  idMem::Singleton()->Free(pointer, ALIGN_16);
}

class idStaticModelManagerLocal final : public idStaticModelManager {
public:
  void Init() override;
  const idStaticModel* FindStaticModel(const char* modelName,
                                       bool makeDefault) override;
  void AddStaticModel(idStaticModel* model) override;
  void ReloadModels() override;
  void CalculateStats(idStats& stats) override;

private:
  static void ListModels_f(const idCmdArgs& args);
  static void ReloadModels_f(const idCmdArgs& args);
};

idStaticModelManagerLocal localModelManager;
idStaticModelManager* staticModelManager = &localModelManager;

void idStaticModelManagerLocal::AddStaticModel(idStaticModel* model) {
  model->isReloadable = false;
  idStaticModel::resourceList.Add(model);
}

void idStaticModelManagerLocal::ReloadModels() {
  idLib::Printf("Reloading models...\n");
  const float startTime = static_cast<float>(Sys_Milliseconds());
  renderSystem->SyncRenderThread(false);
  renderSystem->InhibitEndFrameRendering(1);
  if (idStaticModel::resourceList.ReloadStaleResources() ||
      idMD6Model::resourceList.ReloadStaleResources()) {
    renderSystem->RegenerateReferences();
  }
  renderSystem->InhibitEndFrameRendering(-1);
  const int endTime = static_cast<int>(Sys_Milliseconds());
  idLib::Printf("Models Reloaded: %.0fs elapsed.\n",
                (static_cast<float>(endTime) - startTime) / 1000.0f);
}

void idStaticModelManagerLocal::ReloadModels_f(const idCmdArgs&) {
  localModelManager.ReloadModels();
}

const idStaticModel* idStaticModelManagerLocal::FindStaticModel(
    const char* modelName, bool makeDefault) {
  idStr canonical;
  idStr extension;
  if (modelName == nullptr || modelName[0] == '\0')
    return nullptr;

  idStr bmodelName(modelName);
  if (bmodelName.Cmpn("maps/", 5) == 0) {
    idStr extractedExtension;
    bmodelName.ExtractFileExtension(extractedExtension);
    if (extractedExtension[0] == '\0') {
      bmodelName += ".bmodel";
      modelName = bmodelName.data;
    }
  }

  canonical = modelName;
  canonical.MakeNameCanonical();
  auto* model = static_cast<idStaticModel*>(
      idStaticModel::resourceList.Load(canonical.data, makeDefault, false));
  if ((model == nullptr || model->resourceError != nullptr) && !makeDefault)
    return nullptr;
  return model;
}

void idStaticModelManagerLocal::CalculateStats(idStats& stats) {
  stats = {};
  for (int modelIndex = 0;
       modelIndex < idStaticModel::resourceList.resourceList.num;
       ++modelIndex) {
    auto* model = static_cast<idStaticModel*>(
        idStaticModel::resourceList.Index(modelIndex));
    stats.totalCPUMem += model->CPUMemory();
    stats.totalGPUMem += model->GPUMemory();
    for (int surfaceIndex = 0; surfaceIndex < model->surfaces.num;
         ++surfaceIndex) {
      const idTriangles* geometry = model->surfaces[surfaceIndex].geometry;
      ++stats.totalSurfaces;
      stats.totalVerts += geometry->numVerts;
      stats.totalTriangles += geometry->numIndexes / 3;
    }
    ++stats.inUse;
  }
}

void idStaticModelManagerLocal::ListModels_f(const idCmdArgs& args) {
  int totalGPUMemory = 0;
  int totalModels = 0;
  int totalVertices = 0;
  int totalIndexes = 0;
  int totalSurfaces = 0;
  const bool nonWorld = args.argc > 1 &&
                        std::strcmp(args.argv[1], "nonworld") == 0;

  idLib::Printf("   gmem srf  verts  index\n");
  idLib::Printf("  ----- --- ------ ------\n");
  idList<idResource*, 5> sortedList;
  idStaticModel::resourceList.GetLoadedResources(
      reinterpret_cast<idList<idResource*, 99>&>(sortedList));
  for (int modelIndex = 0; modelIndex < sortedList.num; ++modelIndex) {
    auto* model = dynamic_cast<idStaticModel*>(sortedList[modelIndex]);
    if (nonWorld && std::strstr(model->name.str, "_area") != nullptr)
      continue;
    model->List();
    totalGPUMemory += model->GPUMemory();
    for (int surfaceIndex = 0; surfaceIndex < model->surfaces.num;
         ++surfaceIndex) {
      const idTriangles* geometry = model->surfaces[surfaceIndex].geometry;
      ++totalSurfaces;
      totalVertices += geometry->numVerts;
      totalIndexes += geometry->numIndexes;
    }
    ++totalModels;
  }
  idLib::Printf("  ----- --- ------ ------\n");
  idLib::Printf("   gmem srf  verts  index\n");
  idLib::Printf("%i loaded models\n", totalModels);
  idLib::Printf("total GPU memory: %4.1fM\n",
                static_cast<float>(totalGPUMemory) * 0.00000095367432f);
  idLib::Printf("total Surfaces : %i\n", totalSurfaces);
  idLib::Printf("total Vertices : %i\n", totalVertices);
  idLib::Printf("total Indexes  : %i\n", totalIndexes);
}

void idStaticModelManagerLocal::Init() {
  cmdSystem->AddCommand(
      "listModels", reinterpret_cast<commandFunction_t>(&ListModels_f),
      "lists all models", nullptr);
  cmdSystem->AddCommand(
      "reloadModels", reinterpret_cast<commandFunction_t>(&ReloadModels_f),
      "reloads models", nullptr);

  idStaticModel* defaultModel = new (kStaticModelTag) idStaticModel;
  defaultModel->SetName("_default");
  defaultModel->MakeDefaultModel(-8.0f, 8.0f);
  AddStaticModel(defaultModel);

  idStaticModel* unitCube = new (kStaticModelTag) idStaticModel;
  unitCube->SetName("_unitCube");
  unitCube->MakeSimpleCube(-1.0f, 1.0f);
  AddStaticModel(unitCube);

  idStaticModel* zeroOneCube = new (kStaticModelTag) idStaticModel;
  zeroOneCube->SetName("_zeroOneCube");
  zeroOneCube->MakeSimpleCube(0.0f, 1.0f);
  AddStaticModel(zeroOneCube);

  idStaticModel* extrudeBox = new (kStaticModelTag) idStaticModel;
  extrudeBox->SetName("_extrudeBox");
  extrudeBox->MakeExtrudeBox();
  AddStaticModel(extrudeBox);

  for (int minimum = -1; minimum <= 0; ++minimum) {
    idStaticModel* squareModel = new (kStaticModelTag) idStaticModel;
    squareModel->SetName(minimum == -1 ? "_unitSquare" : "_zeroOneSquare");

    idTriangles* geometry = new (kStaticModelTag) idTriangles;
    geometry->numVerts = 4;
    geometry->numIndexes = 6;
    idTriangles::AllocStaticTriSurfVerts(geometry, geometry->numVerts);
    idTriangles::AllocStaticTriSurfIndexes(geometry, geometry->numIndexes);
    const std::uint16_t indexes[6]{3, 0, 2, 2, 0, 1};
    std::memcpy(geometry->indexes, indexes, sizeof(indexes));
    idTriangles::UpdateIndexBuffer(geometry);

    const float low = static_cast<float>(minimum);
    const idVec3 positions[4]{{low, low, 0.0f},
                              {1.0f, low, 0.0f},
                              {1.0f, 1.0f, 0.0f},
                              {low, 1.0f, 0.0f}};
    const idVec2 textureCoordinates[4]{{0.0f, 0.0f},
                                       {1.0f, 0.0f},
                                       {1.0f, 1.0f},
                                       {0.0f, 1.0f}};
    const std::uint8_t normal[3]{EncodeDirection(0.0f),
                                 EncodeDirection(0.0f),
                                 EncodeDirection(1.0f)};
    const std::uint8_t tangent[3]{EncodeDirection(1.0f),
                                  EncodeDirection(0.0f),
                                  EncodeDirection(0.0f)};
    for (int vertexIndex = 0; vertexIndex < geometry->numVerts;
         ++vertexIndex) {
      idDrawVert& vertex = geometry->verts[vertexIndex];
      vertex.xyz = positions[vertexIndex];
      vertex.st = textureCoordinates[vertexIndex];
      std::memcpy(vertex.normal, normal, sizeof(normal));
      std::memcpy(vertex.tangent, tangent, sizeof(tangent));
      std::memset(vertex.color, 0xFF, sizeof(vertex.color));
    }
    geometry->bounds = geometry->BoundsFromVertScan();
    idTriangles::UpdateVertexBuffer(geometry);

    idStaticModelSurface surface;
    surface.geometry = geometry;
    surface.materialNum = 0;
    squareModel->AddSurface(surface);
    AddStaticModel(squareModel);
  }
}
