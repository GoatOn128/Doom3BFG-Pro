#include "engine/models/skeletalanimation/md6model.h"
#include "engine/models/skeletalanimation/md6modelgenerator.h"

#include "engine/decls/declskins.h"
#include "engine/models/skeletalanimation/jobs/md6blend.h"
#include "engine/models/skeletalanimation/md6mesh.h"
#include "engine/models/skeletalanimation/md6util.h"
#include "engine/renderer/declmaterial.h"
#include "engine/renderer/triangles.h"
#include "shared/idlib/csystems/cvarsystemlocal.h"
#include "shared/idlib/filesystem/filesystemlocal.h"
#include "shared/idlib/lib.h"

#include <algorithm>
#include <cstdint>
#include <cstring>
#include <limits>
#include <new>
#include <vector>

extern idCVar decl_mergeGenericMaterials;

idCVar r_skipMergeMeshes("r_skipMergeMeshes", "0", CVAR_BOOL,
                         "skip MD6 source-surface mesh merging");
idCVar r_skipGeneratedFileCheck(
    "r_skipGeneratedFileCheck", "0", CVAR_BOOL,
    "skip generated MD6 model files while rebuilding assets");

idTypedResourceList<idMD6Model> idMD6Model::resourceList("MD6Model");

namespace {

constexpr std::uint32_t kMD6ModelMagic = 0x28364d4du;
constexpr std::uint32_t kPreviousMD6ModelMagic = 0x27364d4du;
constexpr std::uint32_t kInvalidTimestamp =
    std::numeric_limits<std::uint32_t>::max();
constexpr int kMaximumSerializedMeshes = 1 << 16;

idMD6ModelSkeletonLoader g_skeletonLoader = nullptr;
idMD6ModelGenerateBinary g_binaryGenerator = nullptr;
idMD6ModelRewriteCallback g_rewriteCallback = nullptr;

std::uint16_t ByteSwap16(std::uint16_t value) {
  return static_cast<std::uint16_t>((value << 8u) | (value >> 8u));
}

std::uint32_t ByteSwap32(std::uint32_t value) {
  return (value << 24u) | ((value << 8u) & 0x00ff0000u) |
         ((value >> 8u) & 0x0000ff00u) | (value >> 24u);
}

bool ReadExact(idFile* file, void* output, std::size_t size) {
  return file != nullptr && file->Read(output, size) == size;
}

bool ReadBigUInt(idFile* file, std::uint32_t& value) {
  if (!ReadExact(file, &value, sizeof(value)))
    return false;
  value = ByteSwap32(value);
  return true;
}

bool ReadBigInt(idFile* file, int& value) {
  std::uint32_t word = 0;
  if (!ReadBigUInt(file, word))
    return false;
  value = static_cast<int>(word);
  return true;
}

bool ReadBigFloat(idFile* file, float& value) {
  std::uint32_t word = 0;
  if (!ReadBigUInt(file, word))
    return false;
  std::memcpy(&value, &word, sizeof(value));
  return true;
}

bool ReadBigVec3(idFile* file, idVec3& value) {
  return ReadBigFloat(file, value.x) && ReadBigFloat(file, value.y) &&
         ReadBigFloat(file, value.z);
}

bool ReadBigBounds(idFile* file, idBounds& value) {
  return ReadBigVec3(file, value.b[0]) && ReadBigVec3(file, value.b[1]);
}

bool ReadNativeString(idFile* file, idStr& value) {
  int length = 0;
  if (!ReadExact(file, &length, sizeof(length)) || length < 0 ||
      length > (1 << 20)) {
    return false;
  }
  if (length == 0) {
    value.Empty();
    return true;
  }
  value.Fill(' ', length);
  return ReadExact(file, value.data, static_cast<std::size_t>(length));
}

void InitializeList(idList<idMD6Mesh*, 19>& list) {
  list.granularity = 19;
  list.memTag = 0;
}

void InitializeList(idList<std::uint8_t, 19>& list) {
  list.granularity = 19;
  list.memTag = 0;
}

template <typename T>
void InitializeModelList(idList<T, 5>& list) {
  list.granularity = 5;
  list.memTag = 0;
}

const idDeclSkins* FindModelSkins(const char* modelName) {
  if (modelName == nullptr || *modelName == '\0')
    return nullptr;
  idStr skinsName(modelName);
  skinsName.StripFileExtension();
  return static_cast<const idDeclSkins*>(
      idDeclSkins::resourceList.FindWithInheritance(skinsName.data, false,
                                                    true));
}

void GeneratedModelName(const char* modelName, char* output,
                        int outputLength) {
  const char* mergeFolder = decl_mergeGenericMaterials.valueInteger != 0
                                ? "generated/merged"
                                : "generated/unmerged";
  if (fileSystem != nullptr) {
    fileSystem->FixLongFilename(mergeFolder, "bmd6model", modelName, output,
                                outputLength, -2);
  } else {
    idStr generated(mergeFolder);
    generated.AppendPath(modelName != nullptr ? modelName : "");
    generated.SetFileExtension("bmd6model");
    idStr::Copynz(output, generated.data, outputLength);
  }
}

class idAutoStandardTriangles {
public:
  ~idAutoStandardTriangles();
  const idTriangles* standardTris{};
  bool freeOnDelete{};
};

idAutoStandardTriangles::~idAutoStandardTriangles() {
  if (freeOnDelete)
    delete const_cast<idTriangles*>(standardTris);
}

} // namespace

idProcessMesh::~idProcessMesh() = default;

idMD6BinaryGenerator::idMD6BinaryGenerator() {
  InitializeModelList(jointRemap);
  InitializeModelList(surfaces);
  InitializeModelList(sourceSurfaces);
  InitializeModelList(morphSets.sets);
  defaultBounds.Clear();
}

idMD6BinaryGenerator::~idMD6BinaryGenerator() { surfaces.DeleteContents(); }

void SetMD6ModelSkeletonLoader(idMD6ModelSkeletonLoader loader) {
  g_skeletonLoader = loader;
}

void SetMD6ModelGenerateBinary(idMD6ModelGenerateBinary generator) {
  g_binaryGenerator = generator;
}

void SetMD6ModelRewriteCallback(idMD6ModelRewriteCallback rewrite) {
  g_rewriteCallback = rewrite;
}

bool idBounds::operator!=(const idBounds& other) const {
  return b[0].x != other.b[0].x || b[0].y != other.b[0].y ||
         b[0].z != other.b[0].z || b[1].x != other.b[1].x ||
         b[1].y != other.b[1].y || b[1].z != other.b[1].z;
}

idMD6Mesh::idMD6Mesh() {
  material = nullptr;
  discrete = false;
  allowSkinRemapping = true;
  numTexCoordSets = 0;
  numVerts = 0;
  numTris = 0;
  jointOffset = 0;
  numJoints = 0;
  hashST = 0;
  InitializeModelList(morphMap);
  InitializeModelList(skinRemaps);
  InitializeModelList(blendShapes);
  vertexes.granularity = 19;
  vertexes.memTag = 0;
  stSets.granularity = 19;
  stSets.memTag = 0;
  indexes.granularity = 19;
  indexes.memTag = 0;
  bounds.Clear();
  tris = new (std::nothrow) idTriangles;
}

idMD6Mesh::~idMD6Mesh() { Free(); }

idMD6Morph::~idMD6Morph() = default;

idMD6Model::idMD6Model()
    : timestamp(kInvalidTimestamp), skelTimestamp(kInvalidTimestamp),
      remapForSkinning(true), skinChecksum(0), morphSkin(kInvalidTimestamp),
      numMeshesBeforeSplitting(0), skeleton(nullptr) {
  InitializeList(meshes);
  InitializeList(jointRemap);
  InitializeModelList(morphSets.sets);
  InitializeModelList(sourceSurfaces);
  InitializeModelList(materials);
  defaultBounds.Clear();
  minBoundsExpansion = idVec3{};
  maxBoundsExpansion = idVec3{};
}

idMD6Model::~idMD6Model() noexcept(false) { Free(); }

// The recovered scalar deleting destructor is supplied by the C++ ABI from
// this virtual destructor and the ordinary delete expression. The 83 dtor$
// records are compiler exception-cleanup funclets represented by the RAII
// members and local idStr/idFileLocal/list owners in this translation unit.

idResourceList* idMD6Model::GetResourceList() const { return &resourceList; }

template <>
idMD6Model* idTypedResourceList<idMD6Model>::Alloc(
    const char* resourceName) const {
  idMD6Model* resource = new (std::nothrow) idMD6Model;
  if (resource != nullptr) {
    resource->resourceListPtr =
        const_cast<idTypedResourceList<idMD6Model>*>(this);
    resource->SetName(resourceName);
  }
  return resource;
}

template <>
int idTypedResourceList<idMD6Model>::AllocSize() const {
  return 328;
}

bool idMD6Model::GetJointsForAnimFrame(
    idJointMat* joints, const idMD6Anim* anim, int frame,
    const idVec3* offset, bool removeOriginOffset) const {
  return skeleton != nullptr && skeleton->GetJointsForAnimFrame(
                                    joints, anim, frame, offset,
                                    removeOriginOffset);
}

bool idMD6Model::GetJointsForAnimTime(
    idJointMat* joints, const idMD6Anim* anim, int timeMS,
    const idVec3* offset, bool removeOriginOffset) const {
  return skeleton != nullptr && skeleton->GetJointsForAnimTime(
                                    joints, anim, timeMS, offset,
                                    removeOriginOffset);
}

bool idMD6Model::GetUserChannelsForAnimFrame(
    float* userChannels, unsigned int userChannelsSize,
    const idMD6Anim* anim, float frame) const {
  return skeleton != nullptr && skeleton->GetUserChannelsForAnimFrame(
                                    userChannels, userChannelsSize, anim,
                                    frame);
}

void idMD6Model::Print() {
  idLib::Printf("MD6 model: %s\n", name.str != nullptr ? name.str : "");
  idLib::Printf("meshes:\n");
  unsigned int totalVertices = 0;
  unsigned int totalTriangles = 0;
  for (int index = 0; index < meshes.num; ++index) {
    const idMD6Mesh* mesh = meshes[index];
    if (mesh == nullptr)
      continue;
    totalVertices += static_cast<unsigned int>(std::max(mesh->numVerts, 0));
    totalTriangles += static_cast<unsigned int>(std::max(mesh->numTris, 0));
    const char* materialName =
        mesh->material != nullptr && mesh->material->name.str != nullptr
            ? mesh->material->name.str
            : "<null>";
    idLib::Printf("%3d: %6d verts %6d tris %s\n", index, mesh->numVerts,
                  mesh->numTris, materialName);
  }
  idLib::Printf("total vertices: %u\n", totalVertices);
  idLib::Printf("total triangles: %u\n", totalTriangles);
  idLib::Printf("joints: %u\n",
                skeleton != nullptr && skeleton->data != nullptr
                    ? static_cast<unsigned int>(skeleton->data->numJoints)
                    : 0u);
  idLib::Printf("meshes before splitting: %d\n", numMeshesBeforeSplitting);
  for (int index = 0; index < sourceSurfaces.num; ++index) {
    const sourceSurface_t& surface = sourceSurfaces[index];
    const char* materialName =
        surface.mtr != nullptr && surface.mtr->name.str != nullptr
            ? surface.mtr->name.str
            : "<null>";
    idLib::Printf("source %3d: render %d verts %d..%d %s\n", index,
                  surface.renderSurface, surface.firstVertex,
                  surface.lastVertex, materialName);
  }
}

void idMD6Model::List() const {
  int totalVertices = 0;
  int totalTriangles = 0;
  std::uint64_t memory = sizeof(*this) +
                         static_cast<std::uint64_t>(sourceSurfaces.size) *
                             sizeof(sourceSurface_t) +
                         static_cast<std::uint64_t>(jointRemap.size);
  if (skeleton != nullptr && skeleton->data != nullptr)
    memory += skeleton->data->size;
  for (int index = 0; index < meshes.num; ++index) {
    const idMD6Mesh* mesh = meshes[index];
    if (mesh == nullptr)
      continue;
    totalVertices += mesh->numVerts;
    totalTriangles += mesh->numTris;
    memory += mesh->Memory();
  }
  idLib::Printf("%8llu KB %3d/%3d meshes %7d verts %7d tris %s\n",
                static_cast<unsigned long long>(memory >> 10), meshes.num,
                numMeshesBeforeSplitting, totalVertices, totalTriangles,
                name.str != nullptr ? name.str : "");
}

void idMD6Model::RemapSourceSurfaces() {
  const idDeclSkins* skins = FindModelSkins(name.str);
  morphSkin = kInvalidTimestamp;
  skinChecksum = 0;
  if (skins != nullptr) {
    morphSkin = static_cast<std::uint32_t>(
        skins->IndexForSkin(morphSkinName.data));
    skinChecksum = skins->md5Checksum;
  }

  if (r_skipMergeMeshes.valueInteger != 0)
    return;
  for (int index = 0; index < meshes.num; ++index) {
    idMD6Mesh* mesh = meshes[index];
    if (mesh == nullptr || mesh->tris == nullptr || mesh->material == nullptr ||
        mesh->material->VirtualTexturePageFlavor() == ID_VTF_BAKE) {
      continue;
    }
    mesh->tris->VmtrMapTexCoordsWithSkins(
        &sourceSurfaces, index, mesh->material,
        mesh->allowSkinRemapping ? skins : nullptr, &mesh->skinRemaps,
        &materials);
  }
}

void idMD6Model::Free() {
  skinChecksum = 0;
  jointRemap.ClearFree();
  meshes.DeleteContents();
  sourceSurfaces.ClearFree();
  skeleton = nullptr;
}

bool idMD6Model::LoadBinary(const char* binaryFileName, bool explicitFile) {
  if (binaryFileName == nullptr || fileSystem == nullptr)
    return false;
  idFileLocal file(fileSystem->OpenFileRead(binaryFileName, true, false));
  if (file.file == nullptr)
    return false;

  std::uint32_t magic = 0;
  if (!ReadBigUInt(file.file, magic))
    return false;
  const unsigned int version = magic >> 24u;
  if (magic != kMD6ModelMagic && magic != kPreviousMD6ModelMagic) {
    if (!explicitFile || version < 0x25u)
      return false;
  }

  if (!ReadBigUInt(file.file, timestamp))
    return false;
  if (!explicitFile && idLib::production < PROD_PRODUCTION &&
      fileSystem->GetTimestamp(name.str, false) != timestamp) {
    return false;
  }
  if (!ReadBigUInt(file.file, skelTimestamp))
    return false;

  idStr skeletonFileName;
  if (!ReadNativeString(file.file, skeletonFileName))
    return false;
  if (!explicitFile && idLib::production < PROD_PRODUCTION &&
      fileSystem->GetTimestamp(skeletonFileName.data, false) !=
          skelTimestamp) {
    return false;
  }
  if (!ReadBigVec3(file.file, minBoundsExpansion) ||
      !ReadBigVec3(file.file, maxBoundsExpansion) ||
      !ReadExact(file.file, &remapForSkinning, sizeof(remapForSkinning)) ||
      !ReadNativeString(file.file, morphSkinName)) {
    return false;
  }

  skeleton = g_skeletonLoader != nullptr
                 ? g_skeletonLoader(skeletonFileName.data)
                 : nullptr;
  if (skeleton == nullptr || skeleton->data == nullptr)
    return false;
  const int paddedJoints = (skeleton->data->numJoints + 7) & ~7;
  if (!jointRemap.SetNum(paddedJoints) ||
      !ReadExact(file.file, jointRemap.list,
                 static_cast<std::size_t>(jointRemap.num)) ||
      !ReadBigBounds(file.file, defaultBounds)) {
    return false;
  }

  int numMeshes = 0;
  if (!ReadBigInt(file.file, numMeshes) || numMeshes < 0 ||
      numMeshes > kMaximumSerializedMeshes || !meshes.SetNum(numMeshes)) {
    return false;
  }
  for (int index = 0; index < numMeshes; ++index) {
    meshes[index] = new (std::nothrow) idMD6Mesh;
    if (meshes[index] == nullptr) {
      meshes.DeleteContents();
      return false;
    }
    meshes[index]->LoadMesh_Binary(file.file, magic);
  }

  bool allUnique = true;
  for (int index = 0; index < meshes.num; ++index) {
    allUnique = allUnique && meshes[index] != nullptr &&
                meshes[index]->material != nullptr &&
                meshes[index]->material->VirtualTexturePageFlavor() ==
                    ID_VTF_BAKE;
  }
  if (version > 0x25u)
    morphSets.Load_Binary(file.file, magic);
  sourceSurfaces.ClearFree();
  ReadSourceSurfaces(file.file, sourceSurfaces, allUnique);

  std::uint32_t trailingMagic = 0;
  if (!ReadBigUInt(file.file, trailingMagic) ||
      (trailingMagic != kMD6ModelMagic &&
       (trailingMagic >> 24u) < 0x25u)) {
    meshes.DeleteContents();
    return false;
  }
  return true;
}

bool idMD6Model::ReloadIfStale() {
  if (fileSystem == nullptr || filename.IsEmpty())
    return false;

  bool stale = fileSystem->GetTimestamp(filename.data, false) != timestamp;
  if (!stale && skeleton != nullptr && skeleton->name.str != nullptr) {
    stale = fileSystem->GetTimestamp(skeleton->name.str, false) !=
            skelTimestamp;
  }
  if (!stale) {
    const idDeclSkins* skins = FindModelSkins(name.str);
    const std::uint32_t checksum = skins != nullptr ? skins->md5Checksum : 0u;
    stale = checksum != skinChecksum;
  }
  if (!stale) {
    for (int index = 0; index < sourceSurfaces.num; ++index) {
      const sourceSurface_t& source = sourceSurfaces[index];
      if (source.mtr == nullptr || source.mtr->name.str == nullptr)
        continue;
      const auto* current = static_cast<const idMaterial*>(
          idMaterial::resourceList.FindWithInheritance(source.mtr->name.str,
                                                       true, true));
      if (current == nullptr ||
          current->md5Checksum !=
              static_cast<std::uint32_t>(source.mtrChecksum)) {
        stale = true;
        break;
      }
    }
  }
  if (!stale)
    return false;

  char binaryFileName[256]{};
  GeneratedModelName(filename.data, binaryFileName,
                     static_cast<int>(sizeof(binaryFileName)));
  fileSystem->RemoveFile(binaryFileName, FSPATH_BASE);
  Free();
  LoadResource();
  return true;
}

bool idMD6Model::LoadFileNoRemap(const char* modelFileName) {
  Free();
  filename = modelFileName != nullptr ? modelFileName : "";
  idStr extension;
  filename.ExtractFileExtension(extension);
  const bool explicitBinary = idStr::Icmp(extension.data, "bmd6model") == 0;

  char binaryFileName[256]{};
  if (explicitBinary) {
    idStr::Copynz(binaryFileName, filename.data,
                  static_cast<int>(sizeof(binaryFileName)));
  } else {
    GeneratedModelName(filename.data, binaryFileName,
                       static_cast<int>(sizeof(binaryFileName)));
  }

  if ((r_skipGeneratedFileCheck.valueInteger == 0 || explicitBinary) &&
      LoadBinary(binaryFileName, explicitBinary)) {
    return true;
  }
  if (explicitBinary)
    return false;

  if (idLib::production < PROD_PRODUCTION && g_binaryGenerator != nullptr &&
      g_binaryGenerator(filename.data, binaryFileName) &&
      LoadBinary(binaryFileName, true)) {
    return true;
  }

  SetResourceError("MD6 model not found '%s'", name.str != nullptr ? name.str
                                                                   : "");
  return false;
}

void idMD6Model::LoadResource() {
  if (!LoadFileNoRemap(name.str)) {
    Free();
    return;
  }
  RemapSourceSurfaces();
  for (int index = 0; index < meshes.num; ++index) {
    if (meshes[index] != nullptr)
      meshes[index]->FreeLoadData();
  }
}

bool idMD6Model::RewriteText(char* fileName) {
  if (fileName == nullptr || *fileName == '\0')
    return false;
  if (g_rewriteCallback != nullptr)
    return g_rewriteCallback(fileName);
  if (fileSystem == nullptr)
    return false;

  // Version-five files already have the split skeleton/model representation
  // produced by the retail migrator. Validate that boundary without rewriting
  // comments or whitespace when the source-control tool callback is absent.
  std::vector<std::uint8_t> source;
  if (!fileSystem->ReadFile(fileName, source) || source.empty())
    return false;
  const char* begin = reinterpret_cast<const char*>(source.data());
  const char* end = begin + source.size();
  constexpr char marker[] = "MD6Version";
  const char* version = std::search(begin, end, marker,
                                    marker + sizeof(marker) - 1);
  if (version == end)
    return false;
  version += sizeof(marker) - 1;
  while (version < end && (*version == ' ' || *version == '\t'))
    ++version;
  return version < end && *version == '5';
}

int idSourceControl::Delete(const idStr& file) {
  idList<idStr> files;
  files.Append(file);
  return Delete(files);
}

// Recovered container records map to idList::SetNum, Alloc, Resize,
// operator=, ClearFree, DeleteContents, and idListArrayDelete in list.h. They
// are instantiated here for uint8, processST_t, blendShape_t, idMD6Mesh*,
// idProcessSurface*, and the model resource list.
