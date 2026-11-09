#include "engine/models/static/staticmodel.h"

#include "engine/decls/declskins.h"
#include "engine/framework/resourcemanager.h"
#include "engine/renderer/declmaterial.h"
#include "shared/idlib/containers/heaparray.h"
#include "shared/idlib/csystems/cvarsystemlocal.h"
#include "shared/idlib/filesystem/filesystemlocal.h"
#include "shared/idlib/geometry/geometry.h"
#include "shared/idlib/lib.h"

#include <algorithm>
#include <cmath>
#include <cstdint>
#include <cstring>
#include <limits>
#include <new>

extern idCVar decl_mergeGenericMaterials;
extern idCVar resource_loadLogging;

idCVar r_freeTriangleCPUData(
    "r_freeTriangleCPUData", "0", CVAR_BOOL,
    "dump the CPU data after building vertex buffers");

namespace {

constexpr std::uint32_t kStaticModelVersion = 0x1e4c4d42u;
constexpr std::uint32_t kPreviousStaticModelVersion = 0x1d4c4d42u;

std::uint32_t ByteSwap32(std::uint32_t value) {
  return ((value & 0x000000ffu) << 24) |
         ((value & 0x0000ff00u) << 8) |
         ((value & 0x00ff0000u) >> 8) |
         ((value & 0xff000000u) >> 24);
}

bool ReadExact(idFile* file, void* output, std::size_t size) {
  return file != nullptr && file->Read(output, size) == size;
}

bool ReadBigInt(idFile* file, int& output) {
  std::uint32_t value = 0;
  if (!ReadExact(file, &value, sizeof(value)))
    return false;
  output = static_cast<int>(ByteSwap32(value));
  return true;
}

bool ReadBigUInt(idFile* file, std::uint32_t& output) {
  if (!ReadExact(file, &output, sizeof(output)))
    return false;
  output = ByteSwap32(output);
  return true;
}

bool ReadBigFloat(idFile* file, float& output) {
  std::uint32_t value = 0;
  if (!ReadBigUInt(file, value))
    return false;
  std::memcpy(&output, &value, sizeof(output));
  return true;
}

bool ReadBigVec3(idFile* file, idVec3& output) {
  return ReadBigFloat(file, output.x) && ReadBigFloat(file, output.y) &&
         ReadBigFloat(file, output.z);
}

bool ReadNativeString(idFile* file, idStr& output) {
  int length = 0;
  if (!ReadExact(file, &length, sizeof(length)) || length < 0 ||
      length > (1 << 26)) {
    return false;
  }
  if (length == 0) {
    output.Empty();
    return true;
  }
  output.Fill(' ', length);
  if (!ReadExact(file, output.data, static_cast<std::size_t>(length)))
    return false;
  output.data[length] = '\0';
  return true;
}

bool ValidCount(int count, std::size_t elementSize) {
  constexpr std::size_t kMaximumBinaryAllocation = 1u << 30;
  return count >= 0 && elementSize != 0 &&
         static_cast<std::size_t>(count) <=
             kMaximumBinaryAllocation / elementSize;
}

std::uint8_t EncodeDirection(float value) {
  const int encoded = static_cast<int>(
      std::nearbyint((value + 1.0f) * 127.5f + 0.5f));
  return static_cast<std::uint8_t>(std::clamp(encoded, 0, 255));
}

int VertexSizeFromMask(unsigned mask) {
  return ((mask & 1u) != 0u ? 12 : 0) + 4 * static_cast<int>(mask & 2u) +
         static_cast<int>(mask & 4u) + static_cast<int>((mask >> 1) & 4u) +
         static_cast<int>((mask >> 2) & 4u) -
         ((mask & 0x20u) != 0u ? 4 : 0) -
         ((mask & 0x40u) != 0u ? 4 : 0) +
         ((mask & 0x1000u) != 0u ? 12 : 0) +
         static_cast<int>((mask >> 9) & 4u);
}

const idMaterial* DefaultMaterial() {
  return static_cast<const idMaterial*>(
      idMaterial::resourceList.FindWithInheritance("_default", true, true));
}

const idMaterial* FindMaterial(const char* name, bool makeDefault = true) {
  if (name == nullptr || name[0] == '\0')
    return nullptr;
  return static_cast<const idMaterial*>(
      idMaterial::resourceList.FindWithInheritance(name, makeDefault, true));
}

class AutoStandardTriangles {
public:
  explicit AutoStandardTriangles(idTriangles* source)
      : triangles(source), owned(false) {
    if (source != nullptr && source->numIndexes != 0 &&
        (source->vertexMask != 31 || source->verts == nullptr ||
         source->indexes == nullptr)) {
      triangles = source->CreateStandardTriangles();
      owned = triangles != source;
    }
  }
  ~AutoStandardTriangles() {
    if (owned)
      delete triangles;
  }

  idTriangles* triangles;
  bool owned;
};

} // namespace

idTypedResourceList<idStaticModel> idStaticModel::resourceList("model");

void idStaticModel::PristineLoadResource() {
  pristine = true;
  LoadResource();
  pristine = false;
}

static void AddExtrudeBoxFace(idDrawVert* verts, const idVec3& center,
                              const idVec3& right, const idVec3& up) {
  std::memset(verts, 0, sizeof(idDrawVert) * 4);
  for (int index = 0; index < 4; ++index) {
    verts[index].normal[0] = EncodeDirection(center.x);
    verts[index].normal[1] = EncodeDirection(center.y);
    verts[index].normal[2] = EncodeDirection(center.z);
  }
  verts[0].xyz = center - right + up;
  verts[1].xyz = center + right + up;
  verts[2].xyz = center + right - up;
  verts[3].xyz = center - right - up;
}

void idStaticModel::FreeCPUData() {
  for (int index = 0; index < surfaces.num; ++index) {
    idStaticModelSurface& surface = surfaces[index];
    if (r_freeTriangleCPUData.valueInteger != 0 &&
        surface.geometry != nullptr && surface.material != nullptr &&
        !surface.material->MaterialIsTransparencySorted()) {
      surface.geometry->FreeCPUData();
    }
  }
}

bool idStaticModel::ReloadIfStale() {
  if (!isReloadable || fileSystem == nullptr)
    return false;

  const bool explicitBinary = std::strstr(name.str, ".bmodel") != nullptr;
  bool stale = fileSystem->GetTimestamp(binaryFileName, false) !=
               binaryTimeStamp;
  if (!stale && explicitBinary)
    return false;

  if (!stale) {
    stale = fileSystem->GetTimestamp(name.str, false) != sourceTimeStamp;
    for (int index = 0; !stale && index < sourceSurfaces.num; ++index) {
      const sourceSurface_t& source = sourceSurfaces[index];
      if (source.mtr == nullptr)
        continue;
      const idMaterial* current = FindMaterial(source.mtr->name.str);
      stale = current == nullptr ||
              idStr::Icmp(current->name.str,
                          "textures/common/missing_material") == 0 ||
              source.mtrChecksum != static_cast<int>(current->md5Checksum);
    }
  }

  if (!stale)
    return false;
  if (!explicitBinary)
    fileSystem->RemoveFile(binaryFileName, FSPATH_BASE);
  LoadResource();
  return true;
}

void idStaticModel::PostLoadSurface(int surfaceIndex,
                                    const idDeclSkins* skins) {
  if (surfaceIndex < 0 || surfaceIndex >= surfaces.num)
    return;
  idStaticModelSurface& surface = surfaces[surfaceIndex];
  if (surface.geometry == nullptr || surface.material == nullptr)
    return;

  surface.geometry->VmtrMapTexCoordsWithSkins(
      &sourceSurfaces, surfaceIndex, surface.material, skins,
      &surface.skinRemaps, &materials);
  if ((modelLoadFlags & 2) != 0) {
    if (surface.material->MaterialIsTransparencySorted()) {
      surface.geometry->TurnIntoQuads();
      if (surface.geometry->numVerts > 512) {
        idLib::Error(
            "static model '%s' has transparency sort surface '%s' with more than %d quads",
            name.str, surface.material->name.str, 512);
      }
    } else {
      surface.geometry->UpdateVertexBuffer();
      surface.geometry->UpdateIndexBuffer();
    }
  }
  if ((modelLoadFlags & 1) == 0 &&
      r_freeTriangleCPUData.valueInteger != 0 &&
      !surface.material->MaterialIsTransparencySorted()) {
    surface.geometry->FreeCPUData();
  }
}

void idStaticModel::UpdateBuffers() {
  for (int index = 0; index < surfaces.num; ++index) {
    idStaticModelSurface& surface = surfaces[index];
    if (surface.geometry == nullptr || surface.material == nullptr)
      continue;
    if (surface.material->MaterialIsTransparencySorted()) {
      surface.geometry->TurnIntoQuads();
      if (surface.geometry->numVerts > 512) {
        idLib::Error(
            "static model '%s' has transparency sort surface '%s' with more than %d quads",
            name.str, surface.material->name.str, 512);
      }
    } else {
      surface.geometry->UpdateVertexBuffer();
      surface.geometry->UpdateIndexBuffer();
    }
  }
}

void idStaticModel::Print() const {
  idLib::Printf("%s\n", name.str);
  idLib::Printf("Static model.\n");
  idLib::Printf("referenceBounds: (%f %f %f) to (%f %f %f)\n",
                referenceBounds.b[0].x, referenceBounds.b[0].y,
                referenceBounds.b[0].z, referenceBounds.b[1].x,
                referenceBounds.b[1].y, referenceBounds.b[1].z);
  idLib::Printf("    verts  indx material (render surfaces)\n");

  float totalArea = 0.0f;
  for (int index = 0; index < surfaces.num; ++index) {
    const idStaticModelSurface& surface = surfaces[index];
    const char* materialName =
        surface.material != nullptr ? surface.material->name.str : "<NULL>";
    if (surface.geometry == nullptr) {
      idLib::Printf("%2i: %s, NULL surface geometry\n", index, materialName);
      continue;
    }

    idTriangles* geometry = surface.geometry;
    idLib::Printf("%2i: %5i %5i %s", index, geometry->numVerts,
                  geometry->numIndexes, materialName);
    if ((geometry->cpuVertexMask != 31 || geometry->verts == nullptr) &&
        geometry->verts != nullptr) {
      idLib::Printf(" (%i byte CPU verts)",
                    VertexSizeFromMask(geometry->cpuVertexMask));
    }
    if ((geometry->vertexMask & 0xfffffdffu) != 31u) {
      idLib::Printf(" (%i byte GPU verts)",
                    VertexSizeFromMask(geometry->vertexMask));
    }
    idLib::Printf(geometry->generateNormals ? " (smoothed)\n" : "\n");

    AutoStandardTriangles standard(geometry);
    if (standard.triangles == nullptr || standard.triangles->verts == nullptr ||
        standard.triangles->indexes == nullptr) {
      continue;
    }
    for (int first = 0; first + 2 < standard.triangles->numIndexes;
         first += 3) {
      const std::uint16_t* indexes = standard.triangles->indexes + first;
      totalArea += idGeometry::TriangleArea(
          standard.triangles->verts[indexes[0]].xyz,
          standard.triangles->verts[indexes[1]].xyz,
          standard.triangles->verts[indexes[2]].xyz);
    }
  }

  if (sourceSurfaces.num != 0) {
    idLib::Printf("\nSS  RS start verts material (source surfaces)\n");
    for (int index = 0; index < sourceSurfaces.num; ++index) {
      const sourceSurface_t& source = sourceSurfaces[index];
      const idDeclSource* declSource =
          source.mtr != nullptr ? source.mtr->DeclSource() : nullptr;
      const char* fileName =
          declSource != nullptr ? declSource->GetFileName() : "(IMPLICIT)";
      idLib::Printf("%2i: %2i %5i %5i %s from %s\n", index,
                    source.renderSurface, source.firstVertex,
                    source.lastVertex - source.firstVertex + 1,
                    source.mtr != nullptr ? source.mtr->name.str : "<NULL>",
                    fileName);
    }
  }
  idLib::Printf("\n%f units of total surface area\n\n", totalArea);
  List();
}

int idStaticModel::CPUMemory() const {
  unsigned total = 0;
  for (int index = 0; index < surfaces.num; ++index) {
    if (surfaces[index].geometry != nullptr)
      total += surfaces[index].geometry->CPUMemoryUsed();
  }
  return static_cast<int>(total);
}

int idStaticModel::GPUMemory() const {
  unsigned total = 0;
  for (int index = 0; index < surfaces.num; ++index) {
    if (surfaces[index].geometry != nullptr)
      total += surfaces[index].geometry->GPUMemoryUsed();
  }
  return static_cast<int>(total);
}

void idStaticModel::List() const {
  int totalIndexes = 0;
  int totalVerts = 0;
  for (int index = 0; index < surfaces.num; ++index) {
    const idTriangles* geometry = surfaces[index].geometry;
    if (geometry != nullptr) {
      totalIndexes += geometry->numIndexes;
      totalVerts += geometry->numVerts;
    }
  }
  idLib::Printf(" %5ik %3i %6i %6i %s", GPUMemory() / 1024,
                surfaces.num, totalVerts, totalIndexes, name.str);
  if (resourceError != nullptr)
    idLib::Printf(" %s", resourceError);
  if (referenceBounds.b[0].x >= referenceBounds.b[1].x)
    idLib::Printf(" (EMPTY BOUNDS)");
  idLib::Printf("\n");
}

template <>
int idTypedResourceList<idStaticModel>::AllocSize() const {
  return sizeof(idStaticModel);
}

void idStaticModel::FreeSurfaces() {
  ++reloadCount;
  if (!sharedSurfaces) {
    for (int index = 0; index < surfaces.num; ++index) {
      idStaticModelSurface& surface = surfaces[index];
      if (surface.geometry != nullptr && !surface.geometryIsReference &&
          (surface.referenceMask & 1u) == 0u) {
        delete surface.geometry;
        surface.geometry = nullptr;
      }
      if (surface.stMap != nullptr && (surface.referenceMask & 8u) == 0u) {
        delete surface.stMap;
        surface.stMap = nullptr;
      }
    }
  }
  surfaces.ClearFree();
}

idStaticModel::idStaticModel()
    : pristine(false), cmodel(false), discardReferencePosition(false),
      isReloadable(false), binaryFileName{},
      binaryTimeStamp(static_cast<std::uint32_t>(-1)),
      sourceTimeStamp(static_cast<std::uint32_t>(-1)), reloadCount(0),
      modelIsAutosprite(false), usesTransparencySort(false),
      recoveredBoundsPadding{}, referenceBounds{}, referencePosition{},
      sharedSurfaces(false), recoveredFlagsPadding{}, modelLoadFlags(2),
      surfaces(), sourceSurfaces(), materials() {}

idResourceList* idStaticModel::GetResourceList() const {
  return &resourceList;
}

idStaticModel::~idStaticModel() {
  FreeSurfaces();
}

template <>
idStaticModel* idTypedResourceList<idStaticModel>::Alloc(
    const char* resourceName) const {
  idStaticModel* resource = new (std::nothrow) idStaticModel;
  if (resource != nullptr) {
    resource->resourceListPtr =
        const_cast<idTypedResourceList<idStaticModel>*>(this);
    resource->SetName(resourceName);
  }
  return resource;
}

void idStaticModel::AddSurface(const idStaticModelSurface& surface) {
  surfaces.Append(surface);
}

void idStaticModel::MakeDefaultModel(float low, float high) {
  FreeSurfaces();
  idTriangles* triangles = new (std::nothrow) idTriangles;
  if (triangles == nullptr)
    return;
  triangles->AllocStaticTriSurfVerts(24);
  triangles->AllocStaticTriSurfIndexes(36);
  triangles->AddCubeFace({low, high, high}, {high, high, high},
                         {high, low, high}, {low, low, high});
  triangles->AddCubeFace({low, high, low}, {low, low, low},
                         {high, low, low}, {high, high, low});
  triangles->AddCubeFace({high, low, high}, {high, high, high},
                         {high, high, low}, {high, low, low});
  triangles->AddCubeFace({low, low, high}, {low, low, low},
                         {low, high, low}, {low, high, high});
  triangles->AddCubeFace({low, low, high}, {high, low, high},
                         {high, low, low}, {low, low, low});
  triangles->AddCubeFace({low, high, high}, {low, high, low},
                         {high, high, low}, {high, high, high});
  triangles->generateNormals = true;

  idStaticModelSurface surface;
  surface.material = DefaultMaterial();
  surface.geometry = triangles;
  AddSurface(surface);
  FinishStaticModel(true);
  UpdateBuffers();
  FreeCPUData();
}

void idStaticModel::MakeExtrudeBox() {
  FreeSurfaces();
  idTriangles* triangles = new (std::nothrow) idTriangles;
  if (triangles == nullptr)
    return;

  idHeapArray<idDrawVert> verts;
  verts.Alloc(24);
  if (verts.buffer == nullptr) {
    delete triangles;
    return;
  }
  AddExtrudeBoxFace(verts.buffer + 0, {0.0f, 0.0f, 1.0f},
                    {1.0f, 0.0f, 0.0f}, {0.0f, 1.0f, 0.0f});
  AddExtrudeBoxFace(verts.buffer + 4, {0.0f, 0.0f, -1.0f},
                    {1.0f, 0.0f, 0.0f}, {0.0f, -1.0f, 0.0f});
  AddExtrudeBoxFace(verts.buffer + 8, {1.0f, 0.0f, 0.0f},
                    {0.0f, 1.0f, 0.0f}, {0.0f, 0.0f, 1.0f});
  AddExtrudeBoxFace(verts.buffer + 12, {0.0f, -1.0f, 0.0f},
                    {1.0f, 0.0f, 0.0f}, {0.0f, 0.0f, -1.0f});
  AddExtrudeBoxFace(verts.buffer + 16, {0.0f, 1.0f, 0.0f},
                    {-1.0f, 0.0f, 0.0f}, {0.0f, 0.0f, 1.0f});
  AddExtrudeBoxFace(verts.buffer + 20, {-1.0f, 0.0f, 0.0f},
                    {0.0f, -1.0f, 0.0f}, {0.0f, 0.0f, 1.0f});
  triangles->numVerts = 24;
  triangles->vertexBuffer.AllocBufferObject(verts.buffer,
                                             sizeof(idDrawVert) * 24, 31,
                                             BU_DEFAULT);
  triangles->vertexMask = 31;

  idHeapArray<std::uint16_t> indexes;
  indexes.Alloc(108);
  const std::uint16_t sides[72] = {
      3,5,2,3,4,5,1,17,0,1,16,17,2,9,1,2,8,9,
      0,21,3,0,20,21,8,6,11,8,5,6,10,16,9,10,19,16,
      11,14,10,11,13,14,19,15,18,19,14,15,7,13,6,7,12,13,
      23,12,22,23,15,12,4,22,7,4,21,22,20,18,23,20,17,18};
  std::memcpy(indexes.buffer, sides, sizeof(sides));
  int output = 72;
  for (int face = 0; face < 6; ++face) {
    const std::uint16_t base = static_cast<std::uint16_t>(face * 4);
    indexes.buffer[output++] = base;
    indexes.buffer[output++] = static_cast<std::uint16_t>(base + 2);
    indexes.buffer[output++] = static_cast<std::uint16_t>(base + 1);
    indexes.buffer[output++] = base;
    indexes.buffer[output++] = static_cast<std::uint16_t>(base + 3);
    indexes.buffer[output++] = static_cast<std::uint16_t>(base + 2);
  }
  triangles->numIndexes = 108;
  triangles->indexBuffer.AllocBufferObject(indexes.buffer,
                                            sizeof(std::uint16_t) * 108,
                                            BU_DEFAULT);

  idStaticModelSurface surface;
  surface.material = DefaultMaterial();
  surface.geometry = triangles;
  AddSurface(surface);
}

void idStaticModel::FinishStaticModel(bool cleanup) {
  ++reloadCount;
  if (surfaces.num == 0)
    return;

  modelIsAutosprite = surfaces[0].material != nullptr &&
                      surfaces[0].material->GetAutoSpriteType() >
                          AUTOSPRITE_NONE;
  if (modelIsAutosprite)
    return;

  const float infinity = 1.0e30f;
  referenceBounds.b[0] = {infinity, infinity, infinity};
  referenceBounds.b[1] = {-infinity, -infinity, -infinity};
  for (int index = 0; index < surfaces.num; ++index) {
    idStaticModelSurface& surface = surfaces[index];
    if (surface.geometry == nullptr) {
      MakeDefaultModel(-8.0f, 8.0f);
      idLib::Error("Model %s, surface %i had NULL geometry", name.str,
                   index);
      return;
    }
    if (surface.material == nullptr) {
      MakeDefaultModel(-8.0f, 8.0f);
      idLib::Error("Model %s, surface %i had NULL shader", name.str, index);
      return;
    }
    if (cleanup)
      surface.geometry->Cleanup(true);
    for (int axis = 0; axis < 3; ++axis) {
      referenceBounds.b[0][axis] = std::min(referenceBounds.b[0][axis],
                                            surface.geometry->bounds.b[0][axis]);
      referenceBounds.b[1][axis] = std::max(referenceBounds.b[1][axis],
                                            surface.geometry->bounds.b[1][axis]);
    }
    if (surface.material->MaterialIsTransparencySorted())
      usesTransparencySort = true;
  }
  if (referenceBounds.b[0].x > referenceBounds.b[1].x)
    referenceBounds = {};
}

void idStaticModel::MakeSimpleCube(float low, float high) {
  FreeSurfaces();
  idTriangles* triangles = new (std::nothrow) idTriangles;
  if (triangles == nullptr)
    return;
  triangles->AllocStaticTriSurfVerts(8);
  triangles->AllocStaticTriSurfIndexes(36);
  triangles->numVerts = 8;
  triangles->numIndexes = 36;
  std::memset(triangles->verts, 0, sizeof(idDrawVert) * 8);
  const idVec3 positions[8] = {
      {low, low, low},   {high, low, low}, {high, high, low},
      {low, high, low},  {low, low, high}, {high, low, high},
      {high, high, high},{low, high, high}};
  for (int index = 0; index < 8; ++index)
    triangles->verts[index].xyz = positions[index];
  const std::uint16_t indexes[36] = {
      2,3,0, 1,2,0, 5,1,0, 4,5,0, 7,4,0, 3,7,0,
      1,5,6, 2,1,6, 3,2,6, 7,3,6, 4,7,6, 5,4,6};
  std::memcpy(triangles->indexes, indexes, sizeof(indexes));
  triangles->generateNormals = true;

  idStaticModelSurface surface;
  surface.material = DefaultMaterial();
  surface.geometry = triangles;
  AddSurface(surface);
  FinishStaticModel(true);
  UpdateBuffers();
  FreeCPUData();
}

bool idStaticModel::ReadStaticBModel(const char* fileName,
                                     bool allowPreviousVersion) {
  FreeSurfaces();
  SetResourceError(nullptr);
  binaryTimeStamp = static_cast<std::uint32_t>(-1);
  if (fileSystem == nullptr)
    return false;

  idFileLocal file(fileSystem->OpenFileRead(fileName, true, false));
  if (file.file == nullptr)
    return false;
  binaryTimeStamp = file->Timestamp();

  std::uint32_t magic = 0;
  if (!ReadBigUInt(file.file, magic) ||
      (magic != kStaticModelVersion &&
       (magic != kPreviousStaticModelVersion || !allowPreviousVersion))) {
    SetResourceError("bad magic on staticmodel '%s'", fileName);
    return false;
  }
  const bool previousVersion = magic == kPreviousStaticModelVersion;
  if (previousVersion) {
    idLib::Note(
        "StaticModel '%s' needs to be re-exported, it's an old version file",
        fileName);
  }
  if (!ReadBigUInt(file.file, sourceTimeStamp))
    return false;
  if (sourceTimeStamp == 0 || sourceTimeStamp == static_cast<std::uint32_t>(-1))
    sourceTimeStamp = binaryTimeStamp;

  idList<const idMaterial*, 5> surfaceMaterials;
  bool allUnique = !previousVersion;
  if (!previousVersion) {
    int materialCount = 0;
    if (!ReadBigInt(file.file, materialCount) ||
        !ValidCount(materialCount, sizeof(const idMaterial*)) ||
        !surfaceMaterials.SetNum(materialCount)) {
      return false;
    }
    for (int index = 0; index < materialCount; ++index) {
      idStr materialName;
      if (!ReadNativeString(file.file, materialName) ||
          !idMaterial::IsValidMaterialName(materialName.data, true, true)) {
        SetResourceError("StaticModel '%s' contains invalid materialname '%s'",
                         fileName, materialName.data);
        return false;
      }
      surfaceMaterials[index] = FindMaterialOrAutoMaterial(materialName.data);
      if (surfaceMaterials[index] == nullptr) {
        SetResourceError(
            "StaticModel '%s' material %s loads to NULL (should be correct material or missing_material, buildgame/code/path error)",
            fileName, materialName.data);
        return false;
      }
      allUnique = allUnique &&
                  surfaceMaterials[index]->VirtualTexturePageFlavor() ==
                      ID_VTF_BAKE;
    }
  }

  if (!ReadBigVec3(file.file, referencePosition))
    return false;
  sourceSurfaces.ClearFree();
  ReadSourceSurfaces(file.file, sourceSurfaces, allUnique);

  int surfaceCount = 0;
  if (!ReadBigInt(file.file, surfaceCount) ||
      !ValidCount(surfaceCount, sizeof(idStaticModelSurface)) ||
      !surfaces.SetNum(0) || (surfaceCount > 0 &&
                             !surfaces.ResizeForPort(surfaceCount))) {
    SetResourceError("bad numSurfaces on staticmodel '%s'", fileName);
    return false;
  }

  idStr skinsName(name.str);
  skinsName.StripFileExtension();
  const idDeclSkins* skins = static_cast<const idDeclSkins*>(
      idDeclSkins::resourceList.FindWithInheritance(skinsName.data, false,
                                                    true));
  for (int index = 0; index < surfaceCount; ++index) {
    idStr previousMaterialName;
    if (previousVersion &&
        (!ReadNativeString(file.file, previousMaterialName) ||
         !idMaterial::IsValidMaterialName(previousMaterialName.data, true,
                                          true))) {
      SetResourceError("StaticModel '%s' contains invalid materialname '%s'",
                       fileName, previousMaterialName.data);
      FreeSurfaces();
      return false;
    }

    int materialNum = 0;
    int binaryModelId = 0;
    if (!ReadBigInt(file.file, materialNum) ||
        !ReadBigInt(file.file, binaryModelId)) {
      FreeSurfaces();
      return false;
    }

    idTriangles* triangles = new (std::nothrow) idTriangles;
    if (triangles == nullptr) {
      FreeSurfaces();
      return false;
    }
    bool placeholder = false;
    if (idLib::production < PROD_PRODUCTION || binaryModelId <= 0 ||
        resourceManager == nullptr ||
        resourceManager->GetBinaryModelMask(binaryModelId)) {
      if ((modelLoadFlags & 2) == 0)
        triangles->allowGpuHosting = false;
      triangles->ReadFromFile(file.file);
    } else {
      placeholder = true;
      idTriangles::SkipInFile(file.file);
      triangles->AllocStaticTriSurfVerts(4);
      triangles->AllocStaticTriSurfIndexes(6);
      const idVec3 zero{};
      triangles->AddCubeFace(zero, zero, zero, zero);
      triangles->generateNormals = true;
      triangles->Cleanup(false);
    }

    std::uint32_t trailingMagic = 0;
    if (!ReadBigUInt(file.file, trailingMagic) || trailingMagic != magic) {
      SetResourceError("Bad trailing magic on staticmodel '%s' surface",
                       fileName);
      delete triangles;
      FreeSurfaces();
      return false;
    }

    idStaticModelSurface surface;
    if (placeholder) {
      surface.material = DefaultMaterial();
    } else if (previousVersion) {
      surface.material = FindMaterialOrAutoMaterial(previousMaterialName.data);
    } else if (index >= 0 && index < surfaceMaterials.num) {
      surface.material = surfaceMaterials[index];
    }
    if (surface.material == nullptr) {
      delete triangles;
      FreeSurfaces();
      return false;
    }
    surface.materialNum = materialNum;
    surface.binaryModelId = binaryModelId;
    surface.geometry = triangles;
    AddSurface(surface);
    PostLoadSurface(surfaces.num - 1, skins);
  }

  if (idLib::production < PROD_PRODUCTION && skins != nullptr) {
    if (sourceSurfaces.num == 0) {
      FreeSurfaces();
      return false;
    }
    for (int index = 0; index < surfaces.num; ++index) {
      const idMaterial* compatible =
          surfaces[index].material->CompatibleGenericMaterial();
      if (compatible != nullptr && compatible != surfaces[index].material) {
        FreeSurfaces();
        return false;
      }
    }
  }

  idStr warningModelName(name.str);
  FinishStaticModel(false);
  if (!pristine) {
    for (int index = 0; index < surfaces.num; ++index) {
      const idMaterial* material = surfaces[index].material;
      if (material != nullptr && material->name.str != nullptr &&
          idStr::Icmpn(material->name.str, "generated/automaterials/", 24) ==
              0) {
        idLib::Warning("LWO '%s' is mapped with auto material '%s'",
                       warningModelName.data,
                       material->name.str);
      }
    }
  }
  return true;
}

void idStaticModel::LoadResource() {
  isReloadable = true;
  const bool explicitBinary = std::strstr(name.str, ".bmodel") != nullptr;
  if (explicitBinary) {
    idStr::Copynz(binaryFileName, name.str, sizeof(binaryFileName));
  } else if (fileSystem != nullptr) {
    const char* extension = pristine ? "pmodel" : (cmodel ? "cmodel" : "bmodel");
    const char* mergeDirectory =
        decl_mergeGenericMaterials.valueInteger != 0 ? "merged" : "unmerged";
    char generated[64]{};
    idStr::snPrintf(generated, sizeof(generated), "generated/%s",
                    mergeDirectory);
    fileSystem->FixLongFilename(generated, extension, name.str,
                                binaryFileName, sizeof(binaryFileName));
  }

  if ((!pristine || explicitBinary) &&
      ReadStaticBModel(binaryFileName, explicitBinary)) {
    if (resource_loadLogging.valueInteger != 0) {
      idLib::Printf("idStaticModel::LoadResource '%s' -> '%s'\n", name.str,
                    binaryFileName);
    }
    return;
  }

  if (idLib::production >= PROD_PRODUCTION) {
    SetResourceError("Production model not found '%s'", name.str);
    MakeDefaultModel(-8.0f, 8.0f);
    return;
  }

  sourceTimeStamp = fileSystem != nullptr
                        ? fileSystem->GetTimestamp(name.str, false)
                        : static_cast<std::uint32_t>(-1);
  if (sourceTimeStamp == static_cast<std::uint32_t>(-1)) {
    SetResourceError("Static model source file '%s' File not found", name.str);
    MakeDefaultModel(-8.0f, 8.0f);
    return;
  }
  if (!GenerateBinaryModelFile(name.str, binaryFileName)) {
    SetResourceError("Static model GenerateBinaryModelFile( '%s' ) failed",
                     name.str);
    idLib::Warning("GenerateBinaryModelFile( '%s' ) failed", name.str);
    MakeDefaultModel(-8.0f, 8.0f);
    return;
  }
  if (!ReadStaticBModel(binaryFileName, explicitBinary)) {
    SetResourceError("Static model ReadStaticBModel( '%s' ) failed", name.str);
    MakeDefaultModel(-8.0f, 8.0f);
    return;
  }
  if (resource_loadLogging.valueInteger != 0) {
    idLib::Printf("idStaticModel::LoadResource '%s' -> '%s'\n", name.str,
                  binaryFileName);
  }
}
