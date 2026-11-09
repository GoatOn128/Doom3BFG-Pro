#include "engine/models/particles/particlemeshdistributiongenerator.h"

#include "shared/idlib/filesystem/filesystemlocal.h"
#include "shared/idlib/lib.h"

#include <cstdint>
#include <cstring>
#include <new>

namespace std {

template <typename T> class auto_ptr {
public:
  explicit auto_ptr(T* pointer = nullptr) : _Myptr(pointer) {}
  ~auto_ptr();
  T* get() const { return _Myptr; }

  T* _Myptr;
};

template <>
auto_ptr<idTriangles>::~auto_ptr() {
  delete _Myptr;
}

} // namespace std

namespace {

idStr FixDistributionFileName(const char* input) {
  const char* source = input != nullptr ? input : "";
  if (idStr::CmpPrefix(source, "../../") == 0)
    source += 6;
  idStr full(source);
  full.SetFileExtension("prtmeshdist");
  idStr fileName;
  full.ExtractFileName(fileName);
  if (fileName.Length() < 38) {
    idStr result("generated");
    result.AppendPath(source);
    result.SetFileExtension("prtmeshdist");
    return result;
  }
  idStr filePath;
  full.ExtractFilePath(filePath);
  fileName.StripFileExtension();
  idStr result("generated");
  result.AppendPath(filePath.data);
  int segmentLength = 0;
  for (int index = 0; index < fileName.Length(); ++index) {
    if (segmentLength >= 30) {
      result.Append('/');
      segmentLength = 0;
    }
    result.Append(fileName[index]);
    ++segmentLength;
  }
  result.SetFileExtension("prtmeshdist");
  return result;
}

std::uint32_t ByteSwap32(std::uint32_t value) {
  return ((value & 0x000000ffu) << 24) |
         ((value & 0x0000ff00u) << 8) |
         ((value & 0x00ff0000u) >> 8) |
         ((value & 0xff000000u) >> 24);
}

void WriteBigVec3(idFile* file, const idVec3& value) {
  std::uint32_t words[3];
  std::memcpy(words, &value, sizeof(words));
  for (std::uint32_t& word : words)
    word = ByteSwap32(word);
  file->Write(words, sizeof(words));
}

class StaticModelOwner {
public:
  explicit StaticModelOwner(idStaticModel* model) : model_(model) {}
  ~StaticModelOwner() { delete model_; }
  idStaticModel* get() const { return model_; }
  idStaticModel* operator->() const { return model_; }
  explicit operator bool() const { return model_ != nullptr; }

private:
  idStaticModel* model_;
};

} // namespace

idStr idParticleMeshDistributionGenerator::GetBinaryFileName(
    const char* name) {
  if (idStr::CheckExtension(name, "prtmeshdist"))
    return idStr(name);
  if (idStr::Icmpn(name, "maps/", 5) == 0) {
    idStr result(name);
    result.SetFileExtension("prtmeshdist");
    return result;
  }
  return FixDistributionFileName(name);
}

bool idParticleMeshDistributionGenerator::GenerateInternal(
    const idStr& name, const idTriangles* tris) {
  const idStr filename = GetBinaryFileName(name.data);
  idFileLocal file(fileSystem->OpenFileWrite(filename.data));
  if (file.file == nullptr) {
    idLib::Warning("Could not open %s", filename.data);
    return false;
  }

  const int numTriangles = tris->numIndexes / 3;
  const std::uint32_t magic = 0x504d4401u;
  file->Write(&magic, sizeof(magic));
  const std::uint32_t bigTriangleCount =
      ByteSwap32(static_cast<std::uint32_t>(numTriangles));
  file->Write(&bigTriangleCount, sizeof(bigTriangleCount));
  for (int index = 0; index < tris->numIndexes; index += 3) {
    WriteBigVec3(file.file, tris->verts[tris->indexes[index]].xyz);
    WriteBigVec3(file.file, tris->verts[tris->indexes[index + 1]].xyz);
    WriteBigVec3(file.file, tris->verts[tris->indexes[index + 2]].xyz);
  }
  return true;
}

bool idParticleMeshDistributionGenerator::GenerateFromTriangles(
    const idStr& name, const idTriangles* tris) {
  return GenerateInternal(name, tris);
}

bool idParticleMeshDistributionGenerator::GenerateFromModel(
    const idStr& name, const idStaticModel* model) {
  idTriangles* combined = nullptr;
  for (int index = 0; index < model->surfaces.num; ++index) {
    const idTriangles* geometry = model->surfaces.list[index].geometry;
    if (geometry == nullptr || geometry->numVerts == 0 ||
        geometry->numIndexes == 0)
      continue;
    if (combined != nullptr)
      idTriangles::MergeTriangles(combined, geometry);
    else
      combined = idTriangles::CopyStaticTriSurf(geometry);
  }
  std::auto_ptr<idTriangles> freeTriangles(combined);
  return GenerateInternal(name, combined);
}

bool idParticleMeshDistributionGenerator::Generate(const idStr& name) {
  StaticModelOwner model(new (std::nothrow) idStaticModel);
  if (!model)
    return false;
  model->SetName(name.data);
  model->PristineLoadResource();
  if (model->resourceError != nullptr) {
    idLib::Warning(
        "idParticleMeshDistributionGenerator: Couldn't load model '%s'",
        name.data);
    return false;
  }
  if (model->surfaces.num == 0) {
    idLib::Warning(
        "idParticleMeshDistributionGenerator: model '%s' doesn't contain any surfaces",
        name.data);
    return false;
  }
  return GenerateFromModel(name, model.get());
}
