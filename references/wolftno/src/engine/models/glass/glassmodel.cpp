#include "engine/models/glass/glassmodel.h"

#include "engine/cm/collisionmodel.h"
#include "engine/models/glass/glassmodelgenerator.h"
#include "engine/models/particles/particlemeshdistribution.h"
#include "engine/models/static/staticmodel.h"
#include "engine/renderer/declmaterial.h"
#include "shared/idlib/filesystem/filesystemlocal.h"
#include "shared/idlib/lib.h"

#include <algorithm>
#include <cstdint>
#include <cstring>
#include <new>

idTypedResourceList<idGlassModel> idGlassModel::resourceList("glassModel");

namespace {

constexpr std::uint32_t kGlassModelMagic = 55332167u;

std::uint32_t ByteSwap32(std::uint32_t value) {
  return ((value & 0x000000ffu) << 24) |
         ((value & 0x0000ff00u) << 8) |
         ((value & 0x00ff0000u) >> 8) |
         ((value & 0xff000000u) >> 24);
}

std::uint32_t ReadBigUInt(idFile* file) {
  std::uint32_t value = 0;
  file->Read(&value, sizeof(value));
  return ByteSwap32(value);
}

int ReadBigInt(idFile* file) {
  return static_cast<int>(ReadBigUInt(file));
}

float ReadBigFloat(idFile* file) {
  const std::uint32_t word = ReadBigUInt(file);
  float value = 0.0f;
  std::memcpy(&value, &word, sizeof(value));
  return value;
}

} // namespace

idStr idGlassModel::GetBinaryFileName(const char* modelName) {
  char binaryFileName[256]{};
  if (idStr::CheckExtension(modelName, "gmodel")) {
    idStr::Copynz(binaryFileName, modelName, sizeof(binaryFileName));
  } else {
    fileSystem->FixLongFilename("generated", "gmodel", modelName,
                                binaryFileName, sizeof(binaryFileName), false);
  }
  return idStr(binaryFileName);
}

const idGlassModelSurface& idGlassModel::ConstSurface(int index) const {
  return surfaces[index];
}

const char* idGlassModel::GetDecl(int index) const {
  return decls[index].data;
}

int idGlassModel::GetSurfaceDeclIndex(int index) const {
  return surfaces[index].declIndex;
}

idCollisionModel* idGlassModel::GetCollisionModel(int index) const {
  idStr collisionModelName;
  collisionModelName.Format("%s%d", collisionBaseName.data, index);
  return LoadCollisionModelByName(collisionModelName.data);
}

const idParticleMeshDistribution*
idGlassModel::GetParticleMeshDistribution(int index) const {
  idStr meshName;
  meshName.Format("%s%d", collisionBaseName.data, index);
  return static_cast<const idParticleMeshDistribution*>(
      idParticleMeshDistribution::resourceList.Load(meshName.data, false,
                                                    false));
}

fractureEffectInfo_t idGlassModelSurface::GetEffectInfo() const {
  fractureEffectInfo_t result{};
  int fractureInfo = 0;
  if (material != nullptr) {
    fractureInfo = static_cast<int>(material->GetFractureInfo());
    result.colorType = material->GetFractureColorType();
    result.colorContent = material->GetFractureColorContent();
  } else {
    result.colorType = {1.0f, 1.0f, 1.0f, 1.0f};
    result.colorContent = {1.0f, 1.0f, 1.0f, 1.0f};
  }
  result.type = static_cast<unsigned char>(fractureInfo);
  result.content = fractureInfo & 0x0000ff00;
  result.flags = fractureInfo & static_cast<int>(0xff000000u);
  return result;
}

fractureEffectInfo_t idGlassModel::GetEffectInfo(int index) const {
  return surfaces[index].GetEffectInfo();
}

bool idGlassModel::ReloadIfStale() {
  const bool directBinary = idStr::CheckExtension(name.str, "gmodel");
  const idStr binaryFileName = GetBinaryFileName(name.str);

  if (binaryTimestamp == -1 ||
      fileSystem->GetTimestamp(binaryFileName.data, false) >
          static_cast<std::uint32_t>(binaryTimestamp)) {
    if (!directBinary)
      fileSystem->RemoveFile(binaryFileName.data, FSPATH_BASE);
  } else {
    if (directBinary ||
        static_cast<int>(fileSystem->GetTimestamp(name.str, false)) ==
            sourceTimestamp) {
      return false;
    }
    fileSystem->RemoveFile(binaryFileName.data, FSPATH_BASE);
  }

  reinterpret_cast<idGlassModelGenerator*>(this)->FreeSurfaces();
  decls.ClearFree();
  LoadResource();
  return true;
}

idGlassModel::idGlassModel() {
  surfaces.granularity = 5;
  surfaces.memTag = 5;
  decls.granularity = 5;
  decls.memTag = 5;
  binaryTimestamp = -1;
  sourceTimestamp = -1;
}

idTypedResourceList<idGlassModel>* idGlassModel::GetResourceList() const {
  return &resourceList;
}

idGlassModel::~idGlassModel() {
  reinterpret_cast<idGlassModelGenerator*>(this)->FreeSurfaces();
  decls.ClearFree();
}

template <>
idGlassModel* idTypedResourceList<idGlassModel>::Alloc(
    const char* resourceName) const {
  idGlassModel* resource = new (std::nothrow) idGlassModel;
  if (resource != nullptr)
    resource->SetName(resourceName);
  return resource;
}

bool idGlassModel::LoadBinary(const char* filename) {
  idFileLocal file(fileSystem->OpenFileRead(filename, true, false));
  if (file.file == nullptr) {
    SetResourceError("Failed to open file '%s' when loading glass model '%s'",
                     filename, name.str);
    return false;
  }

  binaryTimestamp =
      static_cast<int>(fileSystem->GetTimestamp(filename, false));
  if (ReadBigUInt(file.file) != kGlassModelMagic) {
    SetResourceError(
        "Invalid file magic in file '%s' when loading glass model '%s', recompile model",
        filename, name.str);
    return false;
  }

  sourceTimestamp = ReadBigInt(file.file);
  file.file->ReadString(collisionBaseName);

  const int numDecls = ReadBigInt(file.file);
  decls.SetNum(numDecls);
  idStr decl;
  for (int index = 0; index < numDecls; ++index) {
    file.file->ReadString(decl);
    decls[index] = decl;
  }

  const int numSurfaces = ReadBigInt(file.file);
  surfaces.SetNum(numSurfaces);
  for (int index = 0; index < numSurfaces; ++index) {
    idGlassModelSurface& surface = surfaces[index];
    surface.geometry = new (std::nothrow) idTriangles{};
    surface.geometry->allowGpuHosting = true;
    surface.geometry->vsb.xyzScale = {1.0f, 1.0f, 1.0f};
    surface.geometry->vsb.stScale = {1.0f, 1.0f};
    surface.geometry->cpuVertexMask = 31;
    surface.geometry->vertexMask = 31;
    surface.geometry->bounds.Clear();

    idStr materialName;
    file.file->ReadString(materialName);
    surface.boundSphere.origin.x = ReadBigFloat(file.file);
    surface.boundSphere.origin.y = ReadBigFloat(file.file);
    surface.boundSphere.origin.z = ReadBigFloat(file.file);
    surface.boundSphere.radius = ReadBigFloat(file.file);
    surface.declIndex = ReadBigInt(file.file);
    surface.geometry->ReadFromFile(file.file);
    const char* const materialText = materialName.data;
    surface.material = materialText != nullptr
                           ? static_cast<const idMaterial*>(
                                 idMaterial::resourceList.FindWithInheritance(
                                     materialText, false, true))
                           : nullptr;
    idTriangles::UpdateVertexBuffer(surface.geometry);
    idTriangles::UpdateIndexBuffer(surface.geometry);
  }
  return true;
}

void idGlassModel::LoadResource() {
  const bool directBinary = idStr::CheckExtension(name.str, "gmodel");
  const idStr binaryFileName = GetBinaryFileName(name.str);
  if (LoadBinary(binaryFileName.data) || directBinary)
    return;

  SetResourceError(nullptr);
  const idStaticModel* sourceModel =
      staticModelManager->FindStaticModel(name.str, false);
  if (sourceModel == nullptr) {
    idLib::Warning("Couldn't generate glass model from source model '%s'",
                   name.str);
    SetResourceError("Can't find source model '%s'", name.str);
    return;
  }

  idGlassModelGenerator generator;
  generator.AddModel(sourceModel, nullptr);
  if (!generator.Generate()) {
    idLib::Warning("Can't generate glass from source model '%s'", name.str);
    SetResourceError("Can't generate glass from source model '%s'", name.str);
    return;
  }
  if (!generator.WriteBinary(binaryFileName.data)) {
    idLib::Warning("Couldn't write generated file '%s' from glass model '%s'",
                   binaryFileName.data, name.str);
    SetResourceError(
        "Couldn't write generated file '%s' from glass model '%s'",
        binaryFileName.data, name.str);
    return;
  }
  if (!LoadBinary(binaryFileName.data)) {
    idLib::Warning("Couldn't load generated glass model '%s'", name.str);
    SetResourceError("Couldn't load generated glass model '%s'", name.str);
  }
}
