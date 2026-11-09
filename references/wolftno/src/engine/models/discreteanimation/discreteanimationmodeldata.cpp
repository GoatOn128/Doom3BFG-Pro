#include "engine/models/discreteanimation/discreteanimationmodeldata.h"

#include "engine/cm/collisionmodel.h"
#include "engine/models/discreteanimation/discreteanimationmodeldatagenerator.h"
#include "engine/models/skinremap.h"
#include "engine/renderer/declmaterial.h"
#include "shared/idlib/csystems/cvarsystemlocal.h"
#include "shared/idlib/filesystem/filesystemlocal.h"
#include "shared/idlib/lib.h"

#include <cstring>
#include <new>

extern idCVar decl_mergeGenericMaterials;
extern idCVar resource_loadLogging;

idCollisionModel* LoadDiscreteAnimationCollisionModel(const char* modelName);

void ReadSourceSurfaces(idFile* file,
                        idList<sourceSurface_t, 5>* sourceSurfaces,
                        bool skipOver);

idCVar r_discretemodel_freecpudata(
    "r_discretemodel_freecpudata", "1", CVAR_BOOL,
    "Free Discrete Animatation model CPU data ");
idCVar r_discretemodel_forcestale(
    "r_discretemodel_forcestale", "0", CVAR_BOOL,
    "Force resources as stale during reload");

idTypedResourceList<idDiscreteAnimationModelData>
    idDiscreteAnimationModelData::resourceList("discreteAnimation");

namespace {

constexpr std::uint32_t kDiscreteAnimationVersion = 0x0a4c4d44u;
constexpr std::uint32_t kDiscreteAnimationVersion8 = 0x084c4d44u;
constexpr std::uint32_t kDiscreteAnimationVersion9 = 0x094c4d44u;

std::uint16_t ByteSwap16(std::uint16_t value) {
  return static_cast<std::uint16_t>((value << 8) | (value >> 8));
}

std::uint32_t ByteSwap32(std::uint32_t value) {
  return ((value & 0x000000ffu) << 24) |
         ((value & 0x0000ff00u) << 8) |
         ((value & 0x00ff0000u) >> 8) |
         ((value & 0xff000000u) >> 24);
}

void ByteSwapFloat(float& value) {
  std::uint32_t bits = 0;
  std::memcpy(&bits, &value, sizeof(bits));
  bits = ByteSwap32(bits);
  std::memcpy(&value, &bits, sizeof(value));
}

bool ReadExact(idFile* file, void* data, std::size_t size) {
  return file != nullptr && file->Read(data, size) == size;
}

bool ReadBig32(idFile* file, std::uint32_t& value) {
  if (!ReadExact(file, &value, sizeof(value)))
    return false;
  value = ByteSwap32(value);
  return true;
}

bool ReadBigInt(idFile* file, int& value) {
  std::uint32_t bits = 0;
  if (!ReadBig32(file, bits))
    return false;
  value = static_cast<int>(bits);
  return true;
}

bool ReadBigFloat(idFile* file, float& value) {
  std::uint32_t bits = 0;
  if (!ReadBig32(file, bits))
    return false;
  std::memcpy(&value, &bits, sizeof(value));
  return true;
}

bool ReadNativeString(idFile* file, idStr& value) {
  int length = 0;
  if (!ReadExact(file, &length, sizeof(length)) || length < 0)
    return false;
  if (length == 0) {
    value.Empty();
    return true;
  }
  value.Fill(' ', length);
  return ReadExact(file, value.data, static_cast<std::size_t>(length));
}

const idMaterial* FindMaterial(const idStr& materialName, bool makeDefault) {
  if (materialName.IsEmpty())
    return nullptr;
  return static_cast<const idMaterial*>(
      idMaterial::resourceList.FindWithInheritance(materialName.data,
                                                   makeDefault, true));
}

bool ValidCount(int count, std::size_t elementSize) {
  constexpr std::size_t kMaximumBinaryAllocation = 1u << 30;
  return count >= 0 &&
         static_cast<std::size_t>(count) <=
             kMaximumBinaryAllocation / elementSize;
}

} // namespace

idDiscreteAnimationModelSurface::idDiscreteAnimationModelSurface()
    : jointOffset(0), numJoints(0), triangles(nullptr), material(nullptr) {}

void idDiscreteAnimationModelData::UpdateBuffers() {
  for (int index = 0; index < surfaces.Num(); ++index) {
    idDiscreteAnimationModelSurface* surface = surfaces[index];
    if (surface == nullptr || surface->triangles == nullptr)
      continue;
    idTriangles::UpdateVertexBuffer(surface->triangles);
    idTriangles::UpdateIndexBuffer(surface->triangles);
  }
}

void idDiscreteAnimationModelData::FreeCPUData() {
  for (int index = 0; index < surfaces.Num(); ++index) {
    idDiscreteAnimationModelSurface* surface = surfaces[index];
    if (surface != nullptr && surface->triangles != nullptr)
      surface->triangles->FreeCPUData();
  }
}

idStr idDiscreteAnimationModelData::GetBinaryFilename(const char* modelName) {
  idStr result;
  if (idStr::Icmpn(modelName, "maps/", 5) == 0) {
    result = modelName;
    result.SetFileExtension("dmodel");
    return result;
  }

  char fixedFileName[256]{};
  idStr generatedPath;
  generatedPath.Format("generated/%s",
                       decl_mergeGenericMaterials.GetBool() ? "merged"
                                                            : "unmerged");
  if (fileSystem != nullptr) {
    fileSystem->FixLongFilename(generatedPath.data, "dmodel", modelName,
                                fixedFileName,
                                static_cast<int>(sizeof(fixedFileName)));
    result = fixedFileName;
  } else {
    generatedPath.AppendPath(modelName);
    generatedPath.SetFileExtension("dmodel");
    result = generatedPath;
  }
  return result;
}

void idDiscreteAnimationModelData::FreeData() {
  for (int index = 0; index < surfaces.Num(); ++index) {
    idDiscreteAnimationModelSurface* surface = surfaces[index];
    if (surface == nullptr)
      continue;
    delete surface->triangles;
    surface->triangles = nullptr;
    delete surface;
  }
  surfaces.ClearFree();
  traceModels.ClearFree();
  traceMaterials.ClearFree();
  transforms.ClearFree();
  sourceSurfaces.ClearFree();
  bounds = {};
  maxRadius = 0.0f;
  sourceTimestamp = static_cast<std::uint32_t>(-1);
  binaryTimestamp = static_cast<std::uint32_t>(-1);
  baseModel = nullptr;
  version = 0;
}

bool idDiscreteAnimationModelData::ReloadIfStale() {
  bool stale = r_discretemodel_forcestale.GetBool();
  const bool explicitBinary = idStr::CheckExtension(name.str, "dmodel");
  const idStr binaryFilename = GetBinaryFilename(name.str);

  if (!stale && fileSystem != nullptr) {
    stale = binaryTimestamp !=
            fileSystem->GetTimestamp(binaryFilename.data, false);
  }

  if (!explicitBinary && !stale) {
    if (version != 0 && version != kDiscreteAnimationVersion) {
      stale = true;
    } else if ((sourceTimestamp != static_cast<std::uint32_t>(-1) ||
                binaryTimestamp == static_cast<std::uint32_t>(-1)) &&
               fileSystem != nullptr &&
               sourceTimestamp != fileSystem->GetTimestamp(name.str, false)) {
      stale = true;
    }

    for (int index = 0; !stale && index < sourceSurfaces.Num(); ++index) {
      const sourceSurface_t& source = sourceSurfaces[index];
      if (source.mtr == nullptr)
        continue;
      const idMaterial* current = static_cast<const idMaterial*>(
          idMaterial::resourceList.FindWithInheritance(source.mtr->name.str,
                                                       true, true));
      stale = current == nullptr || source.mtrChecksum != current->md5Checksum;
    }
  }

  if (!stale)
    return false;
  if (!explicitBinary && fileSystem != nullptr)
    fileSystem->RemoveFile(binaryFilename.data, FSPATH_BASE);
  LoadResource();
  return true;
}

idDiscreteAnimationModelData::idDiscreteAnimationModelData() {
  FreeData();
}

idResourceList* idDiscreteAnimationModelData::GetResourceList() const {
  return &resourceList;
}

idDiscreteAnimationModelData::~idDiscreteAnimationModelData() {
  FreeData();
}

void idDiscreteAnimationModelData::RemapSourceSurfaces() {
  idList<skinRemap_t, 5> skinRemaps;
  idList<const idMaterial*, 5> usedMaterials;
  auto* sourceList =
      reinterpret_cast<idList<sourceSurface_t, 5>*>(&sourceSurfaces);
  for (int index = 0; index < surfaces.Num(); ++index) {
    idDiscreteAnimationModelSurface* surface = surfaces[index];
    if (surface == nullptr || surface->triangles == nullptr ||
        surface->material == nullptr ||
        surface->material->VirtualTexturePageFlavor() == ID_VTF_BAKE) {
      continue;
    }
    surface->triangles->VmtrMapTexCoordsWithSkins(
        sourceList, index, surface->material, nullptr, &skinRemaps,
        &usedMaterials);
  }
}

template <>
void idSwap::BigArray<idDrawVert>(idDrawVert* values, int count) {
  if (values == nullptr || count <= 0)
    return;
  for (int vertex = 0; vertex < count; ++vertex) {
    auto* words = reinterpret_cast<std::uint32_t*>(&values[vertex]);
    for (int word = 0; word < 5; ++word)
      words[word] = ByteSwap32(words[word]);
  }
}

template <>
std::size_t idFile::ReadBigArray<idDrawVert>(idDrawVert* values, int count) {
  const std::size_t bytes =
      sizeof(idDrawVert) * static_cast<std::size_t>(count);
  const std::size_t result = Read(values, bytes);
  idSwap::BigArray(values, count);
  return result;
}

bool idDiscreteAnimationModelData::LoadBinary(const char* fileName) {
  if (fileSystem == nullptr || fileName == nullptr)
    return false;
  idFileLocal file(fileSystem->OpenFileRead(fileName, true, false));
  if (file.file == nullptr)
    return false;

  const auto fail = [&]() {
    FreeData();
    return false;
  };

  std::uint32_t magic = 0;
  if (!ReadBig32(file.file, magic) ||
      (magic != kDiscreteAnimationVersion &&
       magic != kDiscreteAnimationVersion8 &&
       magic != kDiscreteAnimationVersion9)) {
    SetResourceError("bad magic on discreteanimation '%s'", fileName);
    return fail();
  }
  version = magic;
  if (version != kDiscreteAnimationVersion) {
    idLib::Note(
        "DiscreteAnimationModel '%s' needs to be re-compiled, it's an old version file",
        fileName);
  }
  binaryTimestamp = file.file->Timestamp();

  if (!ReadBig32(file.file, sourceTimestamp) ||
      !ReadBigFloat(file.file, maxRadius) ||
      !ReadExact(file.file, &bounds, sizeof(bounds))) {
    return fail();
  }
  for (idVec3& bound : bounds.b) {
    ByteSwapFloat(bound.x);
    ByteSwapFloat(bound.y);
    ByteSwapFloat(bound.z);
  }

  int count = 0;
  if (!ReadBigInt(file.file, count) ||
      !ValidCount(count, sizeof(idTraceModel)) || !traceModels.SetNum(count) ||
      !ReadExact(file.file, traceModels.list,
                 sizeof(idTraceModel) * static_cast<std::size_t>(count))) {
    return fail();
  }

  if (!ReadBigInt(file.file, count) ||
      !ValidCount(count, sizeof(const idMaterial*)) ||
      !traceMaterials.SetNum(count)) {
    return fail();
  }
  for (int index = 0; index < count; ++index) {
    idStr materialName;
    if (!ReadNativeString(file.file, materialName))
      return fail();
    traceMaterials[index] = FindMaterial(materialName, false);
  }

  if (!ReadBigInt(file.file, count) ||
      !ValidCount(count, sizeof(idJointMat)) || !transforms.SetNum(count) ||
      !ReadExact(file.file, transforms.list,
                 sizeof(idJointMat) * static_cast<std::size_t>(count))) {
    return fail();
  }
  for (int index = 0; index < transforms.Num(); ++index) {
    for (float& value : transforms[index].mat)
      ByteSwapFloat(value);
  }

  if (!ReadBigInt(file.file, count) ||
      !ValidCount(count, sizeof(idDiscreteAnimationModelSurface*)) ||
      !surfaces.SetNum(count)) {
    return fail();
  }
  for (int index = 0; index < surfaces.Num(); ++index) {
    auto* surface = new (std::nothrow) idDiscreteAnimationModelSurface;
    auto* triangles = new (std::nothrow) idTriangles{};
    if (surface == nullptr || triangles == nullptr) {
      delete triangles;
      delete surface;
      return fail();
    }
    surface->triangles = triangles;
    triangles->bounds = bounds;
    surfaces[index] = surface;

    idStr materialName;
    if (!ReadNativeString(file.file, materialName) ||
        !ReadBigInt(file.file, surface->jointOffset) ||
        !ReadBigInt(file.file, surface->numJoints) ||
        !ReadBigInt(file.file, triangles->numVerts) ||
        !ReadBigInt(file.file, triangles->numIndexes) ||
        !ValidCount(triangles->numVerts, sizeof(idDrawVert)) ||
        !ValidCount(triangles->numIndexes, sizeof(std::uint16_t))) {
      return fail();
    }
    surface->material = FindMaterial(materialName, true);
    idTriangles::AllocStaticTriSurfVerts(triangles, triangles->numVerts);
    idTriangles::AllocStaticTriSurfIndexes(triangles, triangles->numIndexes);
    if ((triangles->numVerts > 0 && triangles->verts == nullptr) ||
        (triangles->numIndexes > 0 && triangles->indexes == nullptr) ||
        file.file->ReadBigArray(triangles->verts, triangles->numVerts) !=
            sizeof(idDrawVert) *
                static_cast<std::size_t>(triangles->numVerts) ||
        !ReadExact(file.file, triangles->indexes,
                   sizeof(std::uint16_t) *
                       static_cast<std::size_t>(triangles->numIndexes))) {
      return fail();
    }
    for (int element = 0; element < triangles->numIndexes; ++element)
      triangles->indexes[element] = ByteSwap16(triangles->indexes[element]);
  }

  bool allUnique = true;
  for (int index = 0; index < surfaces.Num(); ++index) {
    allUnique = allUnique && surfaces[index]->material != nullptr &&
                surfaces[index]->material->VirtualTexturePageFlavor() ==
                    ID_VTF_BAKE;
  }
  ReadSourceSurfaces(
      file.file,
      reinterpret_cast<idList<sourceSurface_t, 5>*>(&sourceSurfaces),
      allUnique);

  bool hasBaseModel = false;
  if (!ReadExact(file.file, &hasBaseModel, sizeof(hasBaseModel)))
    return fail();
  if (hasBaseModel) {
    idStr baseModelName;
    if (!ReadNativeString(file.file, baseModelName)) {
      return fail();
    }
    baseModel = LoadDiscreteAnimationCollisionModel(baseModelName.data);
    if (baseModel == nullptr)
      return fail();
  }

  for (int index = 0; index < traceModels.Num(); ++index)
    traceModels[index].Swap();
  return true;
}

void idDiscreteAnimationModelData::LoadResource() {
  FreeData();
  const bool explicitBinary = idStr::CheckExtension(name.str, "dmodel");
  const idStr binaryFilename = GetBinaryFilename(name.str);
  if (!LoadBinary(binaryFilename.data)) {
    if (explicitBinary) {
      idLib::Error("Couldn't load explicit dmodel '%s'.", binaryFilename.data);
      return;
    }
    SetResourceError(nullptr);
    idDiscreteAnimationModelDataGenerator generator;
    if (!generator.Generate(name.str)) {
      SetResourceError("Couldn't generate DiscreteAnimationModelData '%s'",
                       name.str);
      FreeData();
      return;
    }
    if (!generator.WriteBinary(binaryFilename.data)) {
      SetResourceError("Couldn't write DiscreteAnimationModelData '%s'",
                       binaryFilename.data);
      FreeData();
      return;
    }
    if (!LoadBinary(binaryFilename.data)) {
      SetResourceError("Couldn't load DiscreteAnimationModelData '%s'",
                       binaryFilename.data);
      FreeData();
      return;
    }
  }

  if (resource_loadLogging.GetBool()) {
    idLib::Printf("idDiscreteAnimationModelData::LoadResource '%s' -> '%s'\n",
                  name.str, binaryFilename.data);
  }
  RemapSourceSurfaces();
  UpdateBuffers();
  if (r_discretemodel_freecpudata.GetBool())
    FreeCPUData();
}

template <>
idDiscreteAnimationModelData*
idTypedResourceList<idDiscreteAnimationModelData>::Alloc(
    const char* resourceName) const {
  auto* resource = new (std::nothrow) idDiscreteAnimationModelData;
  if (resource != nullptr)
    resource->SetName(resourceName);
  return resource;
}
