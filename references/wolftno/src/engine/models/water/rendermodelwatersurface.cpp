#include "engine/models/water/rendermodelwatersurface.h"

#include "engine/framework/coderesource.h"
#include "engine/models/transparency/rendermodeltransparency.h"
#include "engine/renderer/declmaterial.h"
#include "engine/renderer/declrenderparm.h"
#include "engine/renderer/imagetools/image_files.h"
#include "engine/renderer/triangles.h"
#include "shared/idlib/csystems/cvarsystemlocal.h"
#include "shared/idlib/filesystem/filesystemlocal.h"
#include "shared/idlib/lib.h"
#include "shared/idlib/parallelism/paralleljoblist.h"
#include "shared/idlib/sys/sys_alloc.h"

#include <algorithm>
#include <cstdint>
#include <cstring>
#include <new>

namespace {
std::uint32_t WaterByteSwap32(std::uint32_t value) {
  return ((value & 0x000000ffu) << 24u) |
         ((value & 0x0000ff00u) << 8u) |
         ((value & 0x00ff0000u) >> 8u) |
         ((value & 0xff000000u) >> 24u);
}

float WaterByteSwapFloat(float value) {
  std::uint32_t bits;
  std::memcpy(&bits, &value, sizeof(bits));
  bits = WaterByteSwap32(bits);
  std::memcpy(&value, &bits, sizeof(value));
  return value;
}
} // namespace

template <> std::size_t idFile::ReadBig(std::uint32_t& value) {
  const std::size_t count = Read(&value, sizeof(value));
  value = WaterByteSwap32(value);
  return count;
}
template <> std::size_t idFile::WriteBig(const std::uint32_t& value) {
  const std::uint32_t swapped = WaterByteSwap32(value);
  return Write(&swapped, sizeof(swapped));
}
template <> std::size_t idFile::ReadBig(int& value) {
  return ReadBig(reinterpret_cast<std::uint32_t&>(value));
}
template <> std::size_t idFile::WriteBig(const int& value) {
  return WriteBig(reinterpret_cast<const std::uint32_t&>(value));
}
template <> std::size_t idFile::ReadBig(float& value) {
  const std::size_t count = Read(&value, sizeof(value));
  value = WaterByteSwapFloat(value);
  return count;
}
template <> std::size_t idFile::WriteBig(const float& value) {
  const float swapped = WaterByteSwapFloat(value);
  return Write(&swapped, sizeof(swapped));
}
template <> std::size_t idFile::ReadBig(idBounds& value) {
  const std::size_t count = Read(&value, sizeof(value));
  for (int bound = 0; bound < 2; ++bound)
    for (int axis = 0; axis < 3; ++axis)
      value[bound][axis] = WaterByteSwapFloat(value[bound][axis]);
  return count;
}
template <> std::size_t idFile::WriteBig(const idBounds& value) {
  idBounds swapped = value;
  for (int bound = 0; bound < 2; ++bound)
    for (int axis = 0; axis < 3; ++axis)
      swapped[bound][axis] = WaterByteSwapFloat(swapped[bound][axis]);
  return Write(&swapped, sizeof(swapped));
}
template <>
std::size_t idFile::ReadBigArray(idDrawVert* values, int count) {
  const std::size_t bytes = Read(values, sizeof(*values) * count);
  for (int index = 0; index < count; ++index) {
    for (int axis = 0; axis < 3; ++axis)
      values[index].xyz[axis] = WaterByteSwapFloat(values[index].xyz[axis]);
    values[index].st.x = WaterByteSwapFloat(values[index].st.x);
    values[index].st.y = WaterByteSwapFloat(values[index].st.y);
  }
  return bytes;
}
template <>
std::size_t idFile::WriteBigArray(const idDrawVert* values, int count) {
  std::size_t bytes = 0;
  for (int index = 0; index < count; ++index) {
    idDrawVert swapped = values[index];
    for (int axis = 0; axis < 3; ++axis)
      swapped.xyz[axis] = WaterByteSwapFloat(swapped.xyz[axis]);
    swapped.st.x = WaterByteSwapFloat(swapped.st.x);
    swapped.st.y = WaterByteSwapFloat(swapped.st.y);
    bytes += Write(&swapped, sizeof(swapped));
  }
  return bytes;
}
template <>
std::size_t idFile::ReadBigArray(std::uint16_t* values, int count) {
  const std::size_t bytes = Read(values, sizeof(*values) * count);
  for (int index = 0; index < count; ++index)
    values[index] = static_cast<std::uint16_t>((values[index] << 8u) |
                                               (values[index] >> 8u));
  return bytes;
}
template <>
std::size_t idFile::WriteBigArray(const std::uint16_t* values, int count) {
  std::size_t bytes = 0;
  for (int index = 0; index < count; ++index) {
    const std::uint16_t swapped = static_cast<std::uint16_t>(
        (values[index] << 8u) | (values[index] >> 8u));
    bytes += Write(&swapped, sizeof(swapped));
  }
  return bytes;
}

namespace {
constexpr std::uint32_t kWaterModelMagic = 72109399u;

void* AllocWater(std::size_t size, bool zero) {
  return idMem::Singleton()->AllocWithLocation(
      "rendermodelwatersurface TAG_WATER", size, TAG_RENDERMODEL, zero,
      ALIGN_16, HEAP_DEFAULTHEAP);
}

const idMaterial* FindMaterial(const char* name) {
  return name != nullptr
             ? static_cast<const idMaterial*>(
                   idMaterial::resourceList.FindWithInheritance(name, true,
                                                                true))
             : nullptr;
}

void InitializeDrawVert(idDrawVert& vert) {
  std::memset(&vert, 0, sizeof(vert));
  vert.normal[0] = 128;
  vert.normal[1] = 128;
  vert.normal[2] = 255;
  vert.tangent[0] = 255;
  vert.tangent[1] = 128;
  vert.tangent[2] = 128;
  vert.tangent[3] = 128;
  std::memset(vert.color, 255, sizeof(vert.color));
}

struct WaterSimConstructionGuard {
  void* storage{};
  bool active{true};
  ~WaterSimConstructionGuard() {
    if (active)
      idMem::Singleton()->Free(storage, ALIGN_16);
  }
};
} // namespace

idCVar r_skipWater("r_skipWater", "0", CVAR_INTEGER,
                    "1 = skip render update of all water surfaces", 0.0f,
                    1.0f);
idCVar r_useWaterSimulationJobs("r_useWaterSimulationJobs", "1",
                                CVAR_INTEGER,
                                "0 = off, 1 = threaded");
idParallelJobRegistration register_WaterGenJob(
    reinterpret_cast<jobRun_t>(WaterGenJob), "WaterGenJob");
idCodeResource<idDeclRenderParm> rpWaterHeight("rpWaterHeight");

void WaterGenJob(const deferredWaterBlockGenParms_t* parms) {
  ApplyDisturbances(parms);
  GenWaterSurface(parms, parms->prevBlock);
}

void idRenderModelWaterSurface::CreateGeometryInternal(
    const idMaterial*, const char* imageName, const idVec3& origin, int width,
    int height, float spacing, int numBlocksX, int numBlocksY, int numVerts,
    int, idDrawVert* verts, std::uint16_t* indices, idBounds& bounds) {
  bool alternate = false;
  int index = 0;
  for (int y = 0; y < height - 1; ++y) {
    for (int x = 0; x < width - 1; ++x) {
      int p00, p01, p10, p11, diagonalA, diagonalB;
      GridPosToBlockLoc(x, y, numBlocksX, numBlocksY, &p00);
      GridPosToBlockLoc(x, y + 1, numBlocksX, numBlocksY, &p01);
      GridPosToBlockLoc(x + 1, y, numBlocksX, numBlocksY, &p10);
      GridPosToBlockLoc(x + 1, y + 1, numBlocksX, numBlocksY, &p11);
      GridPosToBlockLoc(x + (alternate ? 1 : 0), y, numBlocksX,
                        numBlocksY, &diagonalA);
      GridPosToBlockLoc(x + (alternate ? 1 : 0), y + 1, numBlocksX,
                        numBlocksY, &diagonalB);
      indices[index++] = static_cast<std::uint16_t>(p00);
      indices[index++] = static_cast<std::uint16_t>(diagonalB);
      indices[index++] = static_cast<std::uint16_t>(p10);
      indices[index++] = static_cast<std::uint16_t>(diagonalA);
      indices[index++] = static_cast<std::uint16_t>(p01);
      indices[index++] = static_cast<std::uint16_t>(p11);
      alternate = !alternate;
    }
    if ((width & 1) != 0)
      alternate = !alternate;
  }

  std::uint8_t* image = nullptr;
  unsigned imageWidth = 0;
  unsigned imageHeight = 0;
  R_LoadImage(imageName, &image, &imageWidth, &imageHeight, nullptr);
  for (int blockY = 0; blockY < numBlocksY; ++blockY) {
    for (int blockX = 0; blockX < numBlocksX; ++blockX) {
      int blockOffset = 0;
      BlockPosToBlockLoc(blockX, blockY, numBlocksX, &blockOffset);
      for (int localY = 0; localY < 16; ++localY) {
        for (int localX = 0; localX < 16; ++localX) {
          int gridX = 0;
          int gridY = 0;
          BlockToGridPos(blockX, blockY, localX, localY, &gridX, &gridY);
          if (gridX >= width || gridY >= height)
            continue;
          idDrawVert& vert = verts[blockOffset + localY * 16 + localX];
          InitializeDrawVert(vert);
          const float localPositionX = static_cast<float>(gridX) * spacing;
          const float localPositionY = static_cast<float>(gridY) * spacing;
          vert.xyz = {origin.x + localPositionX, origin.y + localPositionY,
                      origin.z};
          vert.st = {localPositionX /
                         (static_cast<float>(width - 1) * spacing),
                     localPositionY /
                         (static_cast<float>(height - 1) * spacing)};
          if (image != nullptr && imageWidth > 0 && imageHeight > 0) {
            const unsigned px = static_cast<unsigned>(
                static_cast<float>(imageWidth - 1) * vert.st.x);
            const unsigned py = static_cast<unsigned>(
                static_cast<float>(imageHeight - 1) * vert.st.y);
            std::memcpy(vert.color, image + 4 * (px + imageWidth * py), 4);
          }
        }
      }
    }
  }
  if (image != nullptr)
    idMem::Singleton()->Free(image, ALIGN_16);

  bounds.Clear();
  for (int vertex = 0; vertex < numVerts; ++vertex)
    bounds.AddPoint(verts[vertex].xyz);
}

void idRenderModelWaterSurface::BuildStaticModel(
    idRenderModel* model, const idMaterial* material,
    const idBounds& waterBounds, const char*) {
  model->FreeSurfaces();
  idDrawVert verts[4];
  for (auto& vert : verts)
    InitializeDrawVert(vert);
  const float z = waterBounds.b[1].z;
  verts[0].xyz = {waterBounds.b[0].x, waterBounds.b[0].y, z};
  verts[1].xyz = {waterBounds.b[1].x, waterBounds.b[0].y, z};
  verts[2].xyz = {waterBounds.b[0].x, waterBounds.b[1].y, z};
  verts[3].xyz = {waterBounds.b[1].x, waterBounds.b[1].y, z};
  verts[0].st = {0.0f, 0.0f};
  verts[1].st = {1.0f, 0.0f};
  verts[2].st = {0.0f, 1.0f};
  verts[3].st = {1.0f, 1.0f};
  const std::uint16_t indices[6] = {0, 1, 2, 2, 1, 3};
  auto* geometry = new idTriangles;
  geometry->bounds.b[0] = {waterBounds.b[0].x, waterBounds.b[0].y,
                           waterBounds.b[1].z};
  geometry->bounds.b[1] = {waterBounds.b[1].x, waterBounds.b[1].y,
                           waterBounds.b[1].z};
  geometry->bounds.b[0] -= idVec3{1.0f, 1.0f, 1.0f};
  geometry->bounds.b[1] += idVec3{1.0f, 1.0f, 1.0f};
  geometry->numVerts = 4;
  geometry->numIndexes = 6;
  geometry->vertexBuffer.AllocBufferObject(verts, sizeof(verts), 31,
                                           BU_DEFAULT);
  geometry->indexBuffer.AllocBufferObject(indices, sizeof(indices),
                                          BU_DEFAULT);
  idRenderModelSurface surface;
  surface.material = material;
  surface.geometry = geometry;
  model->AddSurface(surface);
  model->FinishSurfaces();
}

void idRenderModelWaterSurface::FreeData() {
  if (waterSim != nullptr) {
    waterSim->~idWaterSurfaceSim();
    idMem::Singleton()->Free(waterSim, ALIGN_16);
    waterSim = nullptr;
  }
  if (deferredVerts != nullptr)
    vertexBuffer[currentIndex].UnmapBuffer();
  deferredVerts = nullptr;
  for (auto& buffer : vertexBuffer)
    buffer.FreeBufferObject();
  indexBuffer.FreeBufferObject();
  idMem::Singleton()->Free(waterGenBlockParms, ALIGN_16);
  waterGenBlockParms = nullptr;
}

bool idRenderModelWaterSurface::UpdateInView(
    const idRenderView*, const idRenderView*, idRenderModelUpdateTools* tools) {
  if (r_skipWater.GetInteger() != 0 || surfaces.Num() == 0 ||
      waterSim == nullptr)
    return false;
  if (deferredVerts != nullptr && vertexBuffer[currentIndex].IsMapped())
    vertexBuffer[currentIndex].UnmapBuffer();
  if (surfaces[0].geometry == nullptr)
    return false;
  surfaces[0].geometry->vertexBuffer.Reference(vertexBuffer[currentIndex]);
  currentIndex = (currentIndex + 1) % NUM_VERTEX_BUFFERS;
  deferredVerts = static_cast<idDrawVert*>(
      vertexBuffer[currentIndex].MapBuffer(BM_WRITE_NOSYNC));

  const int sourceIndex = waterSim->currentDeferred ^ 1;
  int parmIndex = 0;
  for (int blockY = 0; blockY < waterSim->numBlocksY; ++blockY) {
    for (int blockX = 0; blockX < waterSim->numBlocksX; ++blockX) {
      deferredWaterBlockGenParms_t& parms = waterGenBlockParms[parmIndex++];
      int offset, left, right, up, down;
      BlockPosToBlockLoc(blockX, blockY, waterSim->numBlocksX, &offset);
      BlockPosToBlockLoc(blockX - 1, blockY, waterSim->numBlocksX, &left);
      BlockPosToBlockLoc(blockX + 1, blockY, waterSim->numBlocksX, &right);
      BlockPosToBlockLoc(blockX, blockY - 1, waterSim->numBlocksX, &up);
      BlockPosToBlockLoc(blockX, blockY + 1, waterSim->numBlocksX, &down);
      float* heights = waterSim->waveHeight[sourceIndex].list;
      parms.currBlock = heights + offset;
      parms.currBlockL = blockX > 0 ? heights + left : waterSim->dummyBlock.list;
      parms.currBlockR = blockX + 1 < waterSim->numBlocksX
                             ? heights + right
                             : waterSim->dummyBlock.list;
      parms.currBlockU = blockY > 0 ? heights + up : waterSim->dummyBlock.list;
      parms.currBlockD = blockY + 1 < waterSim->numBlocksY
                             ? heights + down
                             : waterSim->dummyBlock.list;
      parms.blockLocX = static_cast<std::int16_t>(blockX);
      parms.blockLocY = static_cast<std::int16_t>(blockY);
      parms.simulationParms = waterSim->simulateParms.list + offset;
      parms.vertGradient = waterSim->vertGradient.list + offset;
      parms.disturbances =
          waterSim->waterDisturbances[waterSim->currentDeferred].list;
      parms.numDisturbances =
          static_cast<std::uint32_t>(waterSim->numDisturbances);
      parms.spacing = waterSim->spacing;
      parms.speed = waterSim->speed;
      parms.damping = waterSim->damping;
      parms.timeDelta = waterSim->GetTimeStep();
      parms.waterOrg = waterSim->origin;
      parms.gridWidth = static_cast<std::uint16_t>(waterSim->width);
      parms.gridHeight = static_cast<std::uint16_t>(waterSim->height);
      parms.verts = deferredVerts + offset;
      parms.prevBlock = waterSim->waveHeight[waterSim->currentDeferred].list +
                        offset;
      if (r_useWaterSimulationJobs.GetInteger() == 1 && tools != nullptr &&
          tools->renderModelJobList != nullptr)
        tools->renderModelJobList->AddJob(
            reinterpret_cast<jobRun_t>(WaterGenJob), &parms);
      else
        WaterGenJob(&parms);
    }
  }
  waterSim->Swap();
  return true;
}

bool idRenderModelWaterSurface::CheckValid(const idWaterSurfaceSim* simulation,
                                           const idBounds&) const {
  if (idLib::production == PROD_PRODUCTION)
    return true;
  if (waterSim == nullptr || simulation == nullptr || surfaces.Num() != 1 ||
      surfaces[0].geometry == nullptr)
    return false;
  return surfaces[0].geometry->numVerts ==
             simulation->numBlocksX * simulation->numBlocksY * 256 &&
         surfaces[0].geometry->numIndexes ==
             (simulation->width - 1) * 6 * (simulation->height - 1);
}

void idRenderModelWaterSurface::CreateGeometry(
    const idMaterial* surfaceMaterial, const char* imageName,
    const idVec3& origin, int width, int height, float spacing,
    int numBlocksX, int numBlocksY) {
  FreeSurfaces();
  const int numVerts = numBlocksX * numBlocksY * 256;
  const int numIndices = 6 * (height - 1) * (width - 1);
  auto* verts = static_cast<idDrawVert*>(
      AllocWater(sizeof(idDrawVert) * static_cast<std::size_t>(numVerts), true));
  auto* indices = static_cast<std::uint16_t*>(
      AllocWater(sizeof(std::uint16_t) * static_cast<std::size_t>(numIndices),
                 true));
  idBounds bounds;
  CreateGeometryInternal(surfaceMaterial, imageName, origin, width, height,
                         spacing, numBlocksX, numBlocksY, numVerts, numIndices,
                         verts, indices, bounds);
  indexBuffer.AllocBufferObject(indices,
                                numIndices * sizeof(std::uint16_t), BU_DEFAULT);
  for (auto& buffer : vertexBuffer)
    buffer.AllocBufferObject(verts, numVerts * sizeof(idDrawVert), 31,
                             BU_DYNAMIC);
  auto* geometry = new idTriangles;
  geometry->bounds = bounds;
  geometry->numVerts = numVerts;
  geometry->numIndexes = numIndices;
  geometry->vertexBuffer.Reference(vertexBuffer[0]);
  geometry->indexBuffer.Reference(indexBuffer);
  idRenderModelSurface surface;
  surface.material = surfaceMaterial;
  surface.geometry = geometry;
  AddSurface(surface);
  FinishSurfaces();
  idMem::Singleton()->Free(verts, ALIGN_16);
  idMem::Singleton()->Free(indices, ALIGN_16);
}

void idRenderModelWaterSurface::Save(idFile* file) const {
  idRenderModel::Save(file);
  const bool hasMaterial = material != nullptr;
  file->Write(&hasMaterial, 1);
  if (hasMaterial)
    file->WriteString(material->GetName());
  file->WriteBig(waterSimParms.waterBounds);
  file->WriteBig(waterSimParms.spacing);
  file->WriteBig(waterSimParms.damping);
  file->WriteBig(waterSimParms.speed);
  file->WriteBig(waterSimParms.timeStep);
}

bool idRenderModelWaterSurface::Load(idFile* file) {
  if (!idRenderModel::Load(file))
    return false;
  bool hasMaterial = false;
  if (file->Read(&hasMaterial, 1) == 0)
    return false;
  if (hasMaterial) {
    idStr materialName;
    if (file->ReadString(materialName) == 0)
      return false;
    material = FindMaterial(materialName.data);
  } else {
    material = nullptr;
  }
  return file->ReadBig(waterSimParms.waterBounds) != 0 &&
         file->ReadBig(waterSimParms.spacing) != 0 &&
         file->ReadBig(waterSimParms.damping) != 0 &&
         file->ReadBig(waterSimParms.speed) != 0 &&
         file->ReadBig(waterSimParms.timeStep) != 0;
}

bool idRenderModelWaterSurface::LoadBinary(const char* modelName) {
  if (fileSystem == nullptr)
    return false;
  idFileLocal file(fileSystem->OpenFileRead(modelName, true, false));
  if (file.file == nullptr || file.file->Length() <= 0)
    return false;
  std::uint32_t magic = 0;
  if (file.file->ReadBig(magic) == 0 || magic != kWaterModelMagic) {
    idLib::Warning("idRenderModelWaterSurface::LoadBinary - Bad magic");
    return false;
  }
  idStr materialName;
  if (file.file->ReadString(materialName) == 0)
    return false;
  material = FindMaterial(materialName.data);
  if (file.file->ReadBig(waterSimParms.spacing) == 0 ||
      file.file->ReadBig(waterSimParms.damping) == 0 ||
      file.file->ReadBig(waterSimParms.speed) == 0 ||
      file.file->ReadBig(waterSimParms.timeStep) == 0 ||
      file.file->ReadBig(waterSimParms.waterBounds) == 0)
    return false;
  idBounds bounds;
  int numVerts = 0;
  int numIndexes = 0;
  if (file.file->ReadBig(bounds) == 0 || file.file->ReadBig(numVerts) == 0 ||
      file.file->ReadBig(numIndexes) == 0)
    return false;
  auto* verts = static_cast<idDrawVert*>(
      AllocWater(sizeof(idDrawVert) * static_cast<std::size_t>(numVerts), false));
  auto* indexes = static_cast<std::uint16_t*>(AllocWater(
      sizeof(std::uint16_t) * static_cast<std::size_t>(numIndexes), false));
  if (file.file->ReadBigArray(verts, numVerts) == 0 ||
      file.file->ReadBigArray(indexes, numIndexes) == 0) {
    idMem::Singleton()->Free(verts, ALIGN_16);
    idMem::Singleton()->Free(indexes, ALIGN_16);
    return false;
  }
  FreeSurfaces();
  indexBuffer.AllocBufferObject(indexes, numIndexes * sizeof(*indexes),
                                BU_DEFAULT);
  for (auto& buffer : vertexBuffer)
    buffer.AllocBufferObject(verts, numVerts * sizeof(*verts), 31, BU_DYNAMIC);
  auto* geometry = new idTriangles;
  geometry->bounds = bounds;
  geometry->numVerts = numVerts;
  geometry->numIndexes = numIndexes;
  geometry->vertexBuffer.Reference(vertexBuffer[0]);
  geometry->indexBuffer.Reference(indexBuffer);
  idRenderModelSurface surface;
  surface.material = material;
  surface.geometry = geometry;
  AddSurface(surface);
  FinishSurfaces();
  if (waterSim != nullptr) {
    waterSim->~idWaterSurfaceSim();
    idMem::Singleton()->Free(waterSim, ALIGN_16);
  }
  void* simStorage = AllocWater(sizeof(idWaterSurfaceSim), false);
  waterSim = new (simStorage) idWaterSurfaceSim(
      waterSimParms.waterBounds, waterSimParms.spacing, waterSimParms.damping,
      waterSimParms.speed, waterSimParms.timeStep);
  for (int blockY = 0; blockY < waterSim->numBlocksY; ++blockY) {
    for (int blockX = 0; blockX < waterSim->numBlocksX; ++blockX) {
      int offset = 0;
      BlockPosToBlockLoc(blockX, blockY, waterSim->numBlocksX, &offset);
      for (int localY = 0; localY < 16; ++localY) {
        for (int localX = 0; localX < 16; ++localX) {
          int gridX, gridY;
          BlockToGridPos(blockX, blockY, localX, localY, &gridX, &gridY);
          if (gridX >= 1 && gridX <= static_cast<int>(waterSim->width) - 2 &&
              gridY >= 1 && gridY <= static_cast<int>(waterSim->height) - 2) {
            const idDrawVert& vert = verts[offset + localY * 16 + localX];
            idVec4 color{vert.color[0] / 255.0f, vert.color[1] / 255.0f,
                         vert.color[2] > 127 ? 1.0f : 0.0f,
                         vert.color[3] / 255.0f};
            waterSim->SetSimulationParms(gridX, gridY, color);
          }
        }
      }
    }
  }
  const int blockCount = waterSim->numBlocksX * waterSim->numBlocksY;
  waterGenBlockParms = static_cast<deferredWaterBlockGenParms_t*>(
      AllocWater(sizeof(deferredWaterBlockGenParms_t) * blockCount, false));
  idMem::Singleton()->Free(verts, ALIGN_16);
  idMem::Singleton()->Free(indexes, ALIGN_16);
  SetParm(rpWaterHeight.Ptr(), waterSim->origin.z);
  CommitThisFrame();
  return true;
}

bool idRenderModelWaterSurface::WriteBinary(const char* modelName) const {
  if (fileSystem == nullptr || surfaces.Num() == 0 ||
      surfaces[0].geometry == nullptr || material == nullptr)
    return false;
  idFileLocal file(fileSystem->OpenFileWrite(modelName));
  if (file.file == nullptr) {
    idLib::Warning(
        "idWaterSurfaceModelData::WriteBinary() - Could not open %s",
        modelName);
    return false;
  }
  file.file->WriteBig(kWaterModelMagic);
  file.file->WriteString(material->GetName());
  file.file->WriteBig(waterSimParms.spacing);
  file.file->WriteBig(waterSimParms.damping);
  file.file->WriteBig(waterSimParms.speed);
  file.file->WriteBig(waterSimParms.timeStep);
  file.file->WriteBig(waterSimParms.waterBounds);
  const idTriangles& geometry = *surfaces[0].geometry;
  file.file->WriteBig(geometry.bounds);
  file.file->WriteBig(geometry.numVerts);
  file.file->WriteBig(geometry.numIndexes);
  const auto* verts =
      static_cast<const idDrawVert*>(vertexBuffer[0].MapBuffer(BM_READ));
  file.file->WriteBigArray(verts, geometry.numVerts);
  vertexBuffer[0].UnmapBuffer();
  const auto* indexes =
      static_cast<const std::uint16_t*>(indexBuffer.MapBuffer(BM_READ));
  file.file->WriteBigArray(indexes, geometry.numIndexes);
  indexBuffer.UnmapBuffer();
  return true;
}

void idRenderModelWaterSurface::BuildWaterModel(
    const idMaterial* surfaceMaterial, const idBounds& waterBounds,
    float spacing, float damping, float speed, float timeStep,
    const char* imageName) {
  FreeData();
  void* simStorage = AllocWater(sizeof(idWaterSurfaceSim), false);
  WaterSimConstructionGuard constructionGuard{simStorage, true};
  waterSim = new (simStorage)
      idWaterSurfaceSim(waterBounds, spacing, damping, speed, timeStep);
  constructionGuard.active = false;
  waterSimParms.spacing = spacing;
  waterSimParms.damping = damping;
  waterSimParms.speed = speed;
  waterSimParms.timeStep = timeStep;
  waterSimParms.waterBounds = waterBounds;
  material = surfaceMaterial;
  const int blockCount = waterSim->numBlocksX * waterSim->numBlocksY;
  waterGenBlockParms = static_cast<deferredWaterBlockGenParms_t*>(
      AllocWater(sizeof(deferredWaterBlockGenParms_t) * blockCount, false));
  CreateGeometry(surfaceMaterial, imageName, waterSim->origin,
                 static_cast<int>(waterSim->width),
                 static_cast<int>(waterSim->height), waterSim->spacing,
                 waterSim->numBlocksX, waterSim->numBlocksY);
  SetParm(rpWaterHeight.Ptr(), waterSim->origin.z);
  CommitThisFrame();
  auto* verts =
      static_cast<idDrawVert*>(vertexBuffer[0].MapBuffer(BM_READ));
  for (int blockY = 0; blockY < waterSim->numBlocksY; ++blockY) {
    for (int blockX = 0; blockX < waterSim->numBlocksX; ++blockX) {
      int offset = 0;
      BlockPosToBlockLoc(blockX, blockY, waterSim->numBlocksX, &offset);
      for (int localY = 0; localY < 16; ++localY) {
        for (int localX = 0; localX < 16; ++localX) {
          int gridX, gridY;
          BlockToGridPos(blockX, blockY, localX, localY, &gridX, &gridY);
          if (gridX >= 1 && gridX <= static_cast<int>(waterSim->width) - 2 &&
              gridY >= 1 && gridY <= static_cast<int>(waterSim->height) - 2) {
            const idDrawVert& vert = verts[offset + localY * 16 + localX];
            idVec4 color{vert.color[0] / 255.0f, vert.color[1] / 255.0f,
                         vert.color[2] > 127 ? 1.0f : 0.0f,
                         vert.color[3] / 255.0f};
            waterSim->SetSimulationParms(gridX, gridY, color);
          }
        }
      }
    }
  }
  vertexBuffer[0].UnmapBuffer();
  char binaryFileName[256];
  if (fileSystem != nullptr) {
    fileSystem->FixLongFilename("generated", "wmodel", name.str,
                                binaryFileName, sizeof(binaryFileName));
    WriteBinary(binaryFileName);
  }
}

bool idRenderModelWaterSurface::LoadModel(const char* modelName) {
  if (fileSystem == nullptr)
    return false;
  char binaryFileName[256];
  fileSystem->FixLongFilename("generated", "wmodel", modelName,
                              binaryFileName, sizeof(binaryFileName));
  return LoadBinary(binaryFileName);
}

idRenderModelWaterSurface::idRenderModelWaterSurface(const char* modelName)
    : idRenderModel(), waterSim(nullptr), vertexBuffer{}, indexBuffer(),
      deferredVerts(nullptr), currentIndex(0), waterGenBlockParms(nullptr),
      waterSimParms(), material(nullptr) {
  renderModelType = RENDERMODEL_WATERSURFACE;
  g.flags0 |= 0x80u;
  g.flags1 |= 0x01u;
  SetName(modelName);
  g.modelDepthSort = MODEL_DEPTH_SORT_AFTER_WORLD;
  LoadModel(modelName);
}

idRenderModelWaterSurface::~idRenderModelWaterSurface() { FreeData(); }
