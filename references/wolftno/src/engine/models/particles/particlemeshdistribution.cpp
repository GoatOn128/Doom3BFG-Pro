#include "engine/models/particles/particlemeshdistribution.h"

#include "engine/models/particles/particlemeshdistributiongenerator.h"
#include "shared/idlib/filesystem/filesystemlocal.h"

#include <cstdint>
#include <cstring>
#include <new>

idTypedResourceList<idParticleMeshDistribution>
    idParticleMeshDistribution::resourceList("prtMeshDist");

namespace {

std::uint32_t ByteSwap32(std::uint32_t value) {
  return ((value & 0x000000ffu) << 24) |
         ((value & 0x0000ff00u) << 8) |
         ((value & 0x00ff0000u) >> 8) |
         ((value & 0xff000000u) >> 24);
}

int ReadBigInt(idFile* file) {
  std::uint32_t value = 0;
  file->Read(&value, sizeof(value));
  return static_cast<int>(ByteSwap32(value));
}

float ReadBigFloat(idFile* file) {
  std::uint32_t word = 0;
  file->Read(&word, sizeof(word));
  word = ByteSwap32(word);
  float result = 0.0f;
  std::memcpy(&result, &word, sizeof(result));
  return result;
}

idVec3 ReadBigVec3(idFile* file) {
  return {ReadBigFloat(file), ReadBigFloat(file), ReadBigFloat(file)};
}

bool IsDirectBinaryName(const char* name) {
  return idStr::Icmpn("maps/", name, 5) == 0 ||
         idStr::CheckExtension(name, "prtmeshdist");
}

} // namespace

idResourceList* idParticleMeshDistribution::GetResourceList() const {
  return &resourceList;
}

bool idParticleMeshDistribution::ReloadIfStale() {
  if (IsDirectBinaryName(name.str))
    return false;
  const idStr binaryFileName =
      idParticleMeshDistributionGenerator::GetBinaryFileName(name.str);
  if (binaryTimestamp != -1 &&
      fileSystem->GetTimestamp(binaryFileName.data) <=
          static_cast<std::uint32_t>(binaryTimestamp)) {
    return false;
  }
  fileSystem->RemoveFile(binaryFileName.data);
  LoadResource();
  return true;
}

void idParticleMeshDistribution::FreeData() {
  delete mesh;
  mesh = nullptr;
  delete[] meshTris;
  meshTris = nullptr;
}

idParticleMeshDistribution::~idParticleMeshDistribution() { FreeData(); }

bool idParticleMeshDistribution::LoadBinary(const char* filename) {
  idFileLocal file(fileSystem->OpenFileRead(filename));
  if (file.file == nullptr) {
    SetResourceError(
        "Failed to open file '%s' when loading particle distribution mesh '%s'",
        filename, name.str);
    return false;
  }

  binaryTimestamp =
      static_cast<int>(fileSystem->GetTimestamp(filename));
  const int magic = ReadBigInt(file.file);
  if (magic != 21253456) {
    SetResourceError(
        "Invalid file magic in file '%s' when loading particle distribution mesh '%s', recompile glass/whatever ...",
        filename, name.str);
    return false;
  }

  const int numTris = ReadBigInt(file.file);
  meshTris = new (std::nothrow)
      particleMeshDistTri_t[static_cast<std::size_t>(numTris)];
  for (int index = 0; index < numTris; ++index) {
    meshTris[index].vertices[0] = ReadBigVec3(file.file);
    meshTris[index].vertices[1] = ReadBigVec3(file.file);
    meshTris[index].vertices[2] = ReadBigVec3(file.file);
    meshTris[index].CaluclateArea();
  }

  particleMeshDist_t* loadedMesh = new particleMeshDist_t;
  mesh = loadedMesh;
  mesh->Construct(static_cast<std::uint16_t>(numTris), meshTris);
  return true;
}

void idParticleMeshDistribution::LoadResource() {
  FreeData();
  binaryTimestamp = -1;
  const bool directBinary = IsDirectBinaryName(name.str);
  const idStr binaryFileName =
      idParticleMeshDistributionGenerator::GetBinaryFileName(name.str);
  if (LoadBinary(binaryFileName.data) || directBinary)
    return;

  binaryTimestamp = -1;
  if (!idParticleMeshDistributionGenerator::Generate(idStr(name.str))) {
    SetResourceError(
        "Failed to generate particle mesh distribution model for '%s'",
        name.str);
    return;
  }
  LoadBinary(binaryFileName.data);
}

template <>
idParticleMeshDistribution*
idTypedResourceList<idParticleMeshDistribution>::Alloc(
    const char* resourceName) const {
  idParticleMeshDistribution* resource =
      new (std::nothrow) idParticleMeshDistribution;
  if (resource != nullptr)
    resource->SetName(resourceName);
  return resource;
}

template <>
int idTypedResourceList<idParticleMeshDistribution>::AllocSize() const {
  return 72;
}
