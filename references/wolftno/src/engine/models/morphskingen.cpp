#include "engine/models/morphskingen.h"

#include "engine/models/skeletalanimation/md6mesh.h"
#include "engine/models/static/staticmodel.h"
#include "engine/renderer/declmaterial.h"
#include "shared/idlib/filesystem/filesystemlocal.h"
#include "shared/idlib/lib.h"
#include "shared/idlib/math/mathlib.h"

#include <algorithm>
#include <cmath>
#include <cstdint>
#include <cstring>
#include <new>

idTypedResourceList<idMorphVertices> idMorphVertices::resourceList(
    "morphVertices");

namespace {

constexpr std::uint32_t kMorphFileMagic = 1297106952u;
constexpr std::uint32_t kMorphFileMagicBig = 138956877u;

idMorphResourceLookup resourceLookup = nullptr;
idMorphJointResolver jointResolver = nullptr;
idMorphDebugLine debugLine = nullptr;
bool forceGeneration = false;

std::uint16_t ByteSwap16(std::uint16_t value) {
  return static_cast<std::uint16_t>((value << 8) | (value >> 8));
}

std::uint32_t ByteSwap32(std::uint32_t value) {
  return (value << 24) | ((value << 8) & 0x00ff0000u) |
         ((value >> 8) & 0x0000ff00u) | (value >> 24);
}

std::size_t ReadBig32(idFile& file, std::uint32_t& value) {
  const std::size_t read = file.Read(&value, sizeof(value));
  value = ByteSwap32(value);
  return read;
}

std::size_t WriteBig32(idFile& file, std::uint32_t value) {
  value = ByteSwap32(value);
  return file.Write(&value, sizeof(value));
}

float ReadBigFloat(idFile& file) {
  std::uint32_t bits = 0;
  ReadBig32(file, bits);
  float value;
  std::memcpy(&value, &bits, sizeof(value));
  return value;
}

void WriteBigFloat(idFile& file, float value) {
  std::uint32_t bits;
  std::memcpy(&bits, &value, sizeof(bits));
  WriteBig32(file, bits);
}

void ReadBigVec3(idFile& file, idVec3& value) {
  value = {ReadBigFloat(file), ReadBigFloat(file), ReadBigFloat(file)};
}

void WriteBigVec3(idFile& file, const idVec3& value) {
  WriteBigFloat(file, value.x);
  WriteBigFloat(file, value.y);
  WriteBigFloat(file, value.z);
}

idStr BinaryName(const char* modelName) {
  idStr path("generated/skins/morphVertices/");
  path.Append(modelName != nullptr ? modelName : "unnamed");
  path.SetFileExtension(".morph");
  return path;
}

const char* ContextModelName(const idMorphResourceContext& context) {
  return context.decl != nullptr && context.decl->model != nullptr &&
                 context.decl->model->name.str != nullptr
             ? context.decl->model->name.str
             : nullptr;
}

bool LookupContext(const idMorphVertices& vertices,
                   idMorphResourceContext& context) {
  return vertices.name.str != nullptr && resourceLookup != nullptr &&
         resourceLookup(vertices.name.str, context) && context.decl != nullptr;
}

idVec3 Normalize(const idVec3& value) {
  const float inverse = 1.0f / std::sqrt(std::max(
      value.LengthSqr(), idMath::FLT_SMALLEST_NON_DENORMAL));
  return value * inverse;
}

bool IsInside(const idBounds& bounds, const idVec3& point) {
  return point.x >= bounds[0].x && point.y >= bounds[0].y &&
         point.z >= bounds[0].z && point.x <= bounds[1].x &&
         point.y <= bounds[1].y && point.z <= bounds[1].z;
}

const char* DeclName(const idDecl* decl) {
  return decl != nullptr && decl->name.str != nullptr ? decl->name.str
                                                      : "<unknown>";
}

} // namespace

void SetMorphResourceLookup(idMorphResourceLookup lookup) {
  resourceLookup = lookup;
}

void SetMorphJointResolver(idMorphJointResolver resolver) {
  jointResolver = resolver;
}

void SetMorphDebugLine(idMorphDebugLine value) { debugLine = value; }

void SetMorphForceGeneration(bool value) { forceGeneration = value; }

template <>
std::size_t idFile::ReadBig<idMat3>(idMat3& value) {
  const std::size_t result = Read(&value, sizeof(value));
  auto* words = reinterpret_cast<std::uint32_t*>(&value);
  for (std::size_t index = 0; index != sizeof(value) / sizeof(*words); ++index)
    words[index] = ByteSwap32(words[index]);
  return result;
}

template <>
std::size_t idFile::WriteBig<idMat3>(const idMat3& value) {
  idMat3 swapped = value;
  auto* words = reinterpret_cast<std::uint32_t*>(&swapped);
  for (std::size_t index = 0; index != sizeof(swapped) / sizeof(*words);
       ++index)
    words[index] = ByteSwap32(words[index]);
  return Write(&swapped, sizeof(swapped));
}

bool morphShape_t::SkinMeshMorphPoint(const idVec3& inPoint,
                                      idVec3& outPoint) {
  static float thresh = 0.5f;
  outPoint = {};
  float nearestDistance = 1.0e30f;
  int nearest = -1;
  for (int index = 0; index != points.num; ++index) {
    const idVec3 delta = points.list[index] - inPoint;
    const float distance = delta.LengthSqr();
    if (distance < thresh && distance < nearestDistance) {
      nearestDistance = distance;
      nearest = index;
    }
  }
  return nearest != -1;
}

bool morphShape_t::MakeSphere(idTriangles* morphTris,
                              const idDrawVert* morphTriVerts,
                              const idDecl* decl) {
  if (morphTris->numVerts < 16) {
    idLib::Warning("Bad Clip Sphere for %s, too few vertices", DeclName(decl));
    return false;
  }
  idBounds bounds;
  bounds.Clear();
  for (int index = 0; index != morphTris->numVerts; ++index)
    bounds.AddPoint(morphTriVerts[index].xyz);
  center = (bounds[0] + bounds[1]) * 0.5f;
  const idVec3 dimensions = bounds[1] - bounds[0];
  radius = 0.5f * std::max({dimensions.x, dimensions.y, dimensions.z});
  radiusSqr = radius * radius;
  return true;
}

bool morphShape_t::MakeEllipsoid(idTriangles* morphTris,
                                 const idDrawVert* morphTriVerts,
                                 const idDecl* decl) {
  idBounds bounds;
  bounds.Clear();
  for (int index = 0; index != morphTris->numVerts; ++index)
    bounds.AddPoint(morphTriVerts[index].xyz);
  center = (bounds[0] + bounds[1]) * 0.5f;

  int farthest = -1;
  int nearest = -1;
  float farthestDistance = -1.0e30f;
  float nearestDistance = 1.0e30f;
  for (int index = 0; index != morphTris->numVerts; ++index) {
    const float distance = (morphTriVerts[index].xyz - center).LengthSqr();
    if (distance > farthestDistance) {
      farthestDistance = distance;
      farthest = index;
    }
    if (distance < nearestDistance) {
      nearestDistance = distance;
      nearest = index;
    }
  }
  if (farthest == -1 || nearest == -1) {
    idLib::Warning(
        "Bad Clip ellipsoid for %s, couldn't determine min/max distances",
        DeclName(decl));
    return false;
  }

  const idVec3 farthestVector = morphTriVerts[farthest].xyz - center;
  const idVec3 nearestVector = morphTriVerts[nearest].xyz - center;
  zAxis = Normalize(farthestVector);
  xAxis = Normalize(nearestVector);
  yAxis = zAxis.Cross(xAxis);
  if (debugLine != nullptr) {
    debugLine({1.0f, 0.0f, 0.0f, 1.0f}, center, center + xAxis, 60000,
              false);
    debugLine({0.0f, 1.0f, 0.0f, 1.0f}, center, center + yAxis, 60000,
              false);
    debugLine({0.0f, 0.0f, 1.0f, 1.0f}, center, center + zAxis, 60000,
              false);
  }
  zAxis *= 1.0f / std::sqrt(std::max(
      farthestVector.LengthSqr(), idMath::FLT_SMALLEST_NON_DENORMAL));
  xAxis *= 1.0f / std::sqrt(std::max(
      nearestVector.LengthSqr(), idMath::FLT_SMALLEST_NON_DENORMAL));

  float yRadius = -1.0f;
  const idDrawVert* radiusVertices = morphTris->verts;
  for (int index = 0; index != morphTris->numVerts; ++index)
    yRadius = std::max(yRadius, yAxis * (radiusVertices[index].xyz - center));
  yAxis *= 1.0f / yRadius;
  return true;
}

bool morphShape_t::MakeSkinMesh(idTriangles* morphTris,
                                const idDrawVert* morphTriVerts,
                                const idDecl* decl) {
  (void)decl;
  for (int index = 0; index != morphTris->numVerts; ++index)
    points.Append(morphTriVerts[index].xyz);
  return true;
}

morphShape_t::morphShape_t(idTriangles* morphTris,
                           const idDrawVert* morphTriVerts,
                           const char* materialName, const idDecl* decl) {
  points.ClearFree();
  if (std::strstr(materialName, "skinmesh") != nullptr) {
    if (MakeSkinMesh(morphTris, morphTriVerts, decl))
      type = SKIN_MESH;
  } else if (std::strstr(materialName, "clipsphere") != nullptr) {
    if (MakeSphere(morphTris, morphTriVerts, decl))
      type = CLIP_SPHERE;
  } else if (std::strstr(materialName, "skinsphere") != nullptr) {
    if (MakeSphere(morphTris, morphTriVerts, decl))
      type = SKIN_SPHERE;
  } else if (std::strstr(materialName, "clipellipsoid") != nullptr &&
             MakeEllipsoid(morphTris, morphTriVerts, decl)) {
    type = CLIP_ELLIPSOID;
  }
}

morphShape_t::~morphShape_t() = default;

idMorphVertices::idMorphVertices() = default;

idMorphVertices::~idMorphVertices() = default;

idResourceList* idMorphVertices::GetResourceList() const {
  return &resourceList;
}

template <>
idMorphVertices* idTypedResourceList<idMorphVertices>::Alloc(
    const char* resourceName) const {
  idMorphVertices* resource = new (std::nothrow) idMorphVertices;
  if (resource != nullptr)
    resource->SetName(resourceName);
  return resource;
}

template <>
int idTypedResourceList<idMorphVertices>::AllocSize() const {
  return 80;
}

bool idMorphVertices::Load_Binary() {
  idMorphResourceContext context;
  if (!LookupContext(*this, context)) {
    idLib::Warning("idMorphVertices failed to find md6Def '%s'", name.str);
    return false;
  }
  const char* modelName = ContextModelName(context);
  if (modelName == nullptr) {
    if (idLib::production == PROD_DEVELOPMENT || context.declLoadedResource)
      idLib::Warning("idMorphVertices failed to find model '%s'", name.str);
    return false;
  }
  if (fileSystem == nullptr)
    return false;
  const idStr path = BinaryName(modelName);
  idFileLocal file(fileSystem->OpenFileRead(path.data));
  if (file.file == nullptr || file->Length() <= 0)
    return false;
  std::uint32_t magic = 0;
  if (file->Read(&magic, sizeof(magic)) != sizeof(magic) ||
      ByteSwap32(magic) != kMorphFileMagicBig)
    return false;
  ReadBig32(*file.file, timestampDecl);
  ReadBig32(*file.file, timestampModel);
  idStr storedName;
  idStr storedFileName;
  file->ReadString(storedName);
  file->ReadString(storedFileName);
  const char* expectedFileName =
      context.declFileName != nullptr ? context.declFileName : "";
  if (storedName.Icmp(context.decl->name.str) != 0 ||
      storedFileName.Icmp(expectedFileName) != 0) {
    idLib::Warning(
        "Loading morph vertices md6Def '%s' generated by md6Def '%s' using "
        "file '%s' onto md6Def '%s'",
        name.str, storedName.data, storedFileName.data, expectedFileName);
  }
  std::uint32_t setCount = 0;
  ReadBig32(*file.file, setCount);
  morphVertSet.SetNum(static_cast<int>(setCount));
  for (int setIndex = 0; setIndex != morphVertSet.num; ++setIndex) {
    idMD6MorphSet& set = morphVertSet[setIndex];
    std::uint32_t value = 0;
    ReadBig32(*file.file, value);
    set.meshIndex = static_cast<int>(value);
    ReadBig32(*file.file, value);
    set.groupIndex = static_cast<int>(value);
    ReadBig32(*file.file, value);
    set.type = static_cast<morphDataType_t>(value);
    ReadBigVec3(*file.file, set.pos);
    ReadBigVec3(*file.file, set.size);
    file->ReadBig(set.axis);
    ReadBig32(*file.file, value);
    set.indices.SetNum(static_cast<int>(value));
    for (int index = 0; index != set.indices.num; ++index) {
      std::uint16_t item = 0;
      file->Read(&item, sizeof(item));
      set.indices[index] = ByteSwap16(item);
    }
    ReadBig32(*file.file, value);
    set.direction.SetNum(static_cast<int>(value));
    if (set.direction.num != 0)
      file->Read(set.direction.list,
                 static_cast<std::size_t>(set.direction.num));
  }
  file->Read(&magic, sizeof(magic));
  return true;
}

bool idMorphVertices::Write_Binary() {
  idMorphResourceContext context;
  if (!LookupContext(*this, context)) {
    idLib::Warning(
        "idMorphVertices::Write_Binary() : failed to find md6Def '%s'",
        name.str);
    return false;
  }
  const char* modelName = ContextModelName(context);
  if (modelName == nullptr) {
    idLib::Warning(
        "idMorphVertices::Write_Binary() : failed to findmodel for '%s'",
        name.str);
    return false;
  }
  if (fileSystem == nullptr)
    return false;
  const idStr path = BinaryName(modelName);
  idFileLocal file(fileSystem->OpenFileWrite(path.data));
  if (file.file == nullptr) {
    idLib::Warning("idMorphVertices::Write_Binary() : Could not open %s",
                   path.data);
    return false;
  }
  idLib::Printf("Writing %s\n", path.data);
  timestampDecl = context.declTimestampChecksum;
  timestampModel = context.modelTimestamp;
  if (context.morphDefs != nullptr)
    idMorphVertGen::GenerateMorphVerts(context.decl, *context.morphDefs,
                                       *this);
  file->Write(&kMorphFileMagic, sizeof(kMorphFileMagic));
  WriteBig32(*file.file, timestampDecl);
  WriteBig32(*file.file, timestampModel);
  file->WriteString(context.decl->name.str);
  file->WriteString(context.declFileName != nullptr ? context.declFileName
                                                    : "");
  WriteBig32(*file.file, static_cast<std::uint32_t>(morphVertSet.num));
  for (int setIndex = 0; setIndex != morphVertSet.num; ++setIndex) {
    const idMD6MorphSet& set = morphVertSet[setIndex];
    WriteBig32(*file.file, static_cast<std::uint32_t>(set.meshIndex));
    WriteBig32(*file.file, static_cast<std::uint32_t>(set.groupIndex));
    WriteBig32(*file.file, static_cast<std::uint32_t>(set.type));
    WriteBigVec3(*file.file, set.pos);
    WriteBigVec3(*file.file, set.size);
    file->WriteBig(set.axis);
    WriteBig32(*file.file, static_cast<std::uint32_t>(set.indices.num));
    for (int index = 0; index != set.indices.num; ++index) {
      const std::uint16_t item = ByteSwap16(set.indices[index]);
      file->Write(&item, sizeof(item));
    }
    WriteBig32(*file.file, static_cast<std::uint32_t>(set.direction.num));
    if (set.direction.num != 0)
      file->Write(set.direction.list,
                  static_cast<std::size_t>(set.direction.num));
  }
  file->Write(&kMorphFileMagic, sizeof(kMorphFileMagic));
  return true;
}

bool idMorphVertices::ReloadIfStale() {
  idMorphResourceContext context;
  if (name.str == nullptr || !LookupContext(*this, context) ||
      ContextModelName(context) == nullptr)
    return true;
  const bool declarationChanged =
      timestampDecl == 0xffffffffu ||
      timestampDecl != context.declTimestampChecksum;
  if (!declarationChanged && timestampModel != 0xffffffffu &&
      timestampModel == context.modelTimestamp)
    return false;
  const idStr path = BinaryName(ContextModelName(context));
  if (fileSystem != nullptr)
    fileSystem->RemoveFile(path.data);
  morphVertSet.ClearFree();
  LoadResource();
  return true;
}

void idMorphVertices::LoadResource() {
  if (forceGeneration || !Load_Binary()) {
    forceGeneration = false;
    if (
      static_cast<unsigned int>(idLib::production) <= PROD_BUILDING &&
      !Write_Binary()) {
      SetResourceError("idMorphVertices load resource failed '%s'", name.str);
    }
  }
}

void idMorphVertGen::ProcessModel(const idMD6Model& model,
                                  const idDeclMD6* decl,
                                  const idMorphDef& morphDef,
                                  idMorphVertices& vertices) {
  int groupIndex = 0;
  idVec3 center{};
  if (idStr::Cmp(morphDef.jointName.data, "none") != 0) {
    const idMorphJointResolveResult result =
        jointResolver != nullptr
            ? jointResolver(decl, morphDef.jointName.data, groupIndex, center,
                            morphDef.pos)
            : MORPH_JOINT_NOT_FOUND;
    if (result == MORPH_JOINT_NOT_FOUND) {
      idLib::Warning("Joint '%s' not found for model def '%s'",
                     morphDef.jointName.data,
                     decl != nullptr ? decl->name.str : "<unknown>");
      return;
    }
    if (result == MORPH_JOINT_GROUP_EMPTY) {
      idLib::Warning(
          "Error parsing damage decls for '%s'.  No joint group found or "
          "jointList is NULL",
          decl != nullptr ? decl->name.str : "<unknown>");
      return;
    }
    if (result == MORPH_JOINT_INVALID) {
      idLib::Warning("Invalid joint when parsing damage decls for '%s'",
                     decl != nullptr ? decl->name.str : "<unknown>");
      return;
    }
  }
  for (int meshIndex = 0; meshIndex != model.meshes.num; ++meshIndex) {
    const idMD6Mesh* mesh = model.meshes[meshIndex];
    if (mesh == nullptr || mesh->tris == nullptr)
      continue;
    const idTriangles& triangles = *mesh->tris;
    const idDrawVert* drawVertices = triangles.verts;
    bool mapped = false;
    if (drawVertices == nullptr &&
        (triangles.vertexBuffer.size & 0x7fffffff) != 0) {
      drawVertices = static_cast<const idDrawVert*>(
          const_cast<idVertexBuffer&>(triangles.vertexBuffer).MapBuffer(0));
      mapped = true;
    }
    if (drawVertices == nullptr) {
      idLib::Warning("idMorphVertGen::ProcessModel(). Unable to Map vertex buffer for '%s",
                     model.name.str);
      continue;
    }
    idMD6MorphSet set;
    set.meshIndex = meshIndex;
    set.groupIndex = groupIndex;
    set.pos = morphDef.pos;
    set.size = morphDef.size;
    set.axis = mat3_identity;
    if ((morphDef.flags & 1u) != 0) {
      set.type = MORPHDATA_SPHERE;
      const float radiusSquared = morphDef.size.x * morphDef.size.x;
      for (int vertex = 0; vertex != triangles.numVerts; ++vertex) {
        if ((drawVertices[vertex].xyz - center).LengthSqr() <= radiusSquared)
          set.indices.Append(static_cast<std::uint16_t>(vertex));
      }
    } else {
      set.type = MORPHDATA_BOX;
      const idBounds bounds{{morphDef.pos - morphDef.size,
                             morphDef.pos + morphDef.size}};
      for (int vertex = 0; vertex != triangles.numVerts; ++vertex) {
        if (IsInside(bounds, drawVertices[vertex].xyz))
          set.indices.Append(static_cast<std::uint16_t>(vertex));
      }
    }
    if (mapped)
      const_cast<idVertexBuffer&>(triangles.vertexBuffer).UnmapBuffer();
    if (set.indices.num != 0)
      vertices.morphVertSet.Append(set);
  }
}

void idMorphVertGen::OptimizeVertices(idMorphVertices& outputVertices,
                                      idMorphVertices& inputVertices) {
  idList<bool, 5> included;
  included.SetNum(65536);
  idList<bool, 5> processed;
  processed.SetNum(inputVertices.morphVertSet.num);
  if (processed.num != 0)
    std::memset(processed.list, 0,
                static_cast<std::size_t>(processed.num));
  for (int first = 0; first != inputVertices.morphVertSet.num; ++first) {
    if (processed[first])
      continue;
    const idMD6MorphSet& source = inputVertices.morphVertSet[first];
    std::memset(included.list, 0, 65536);
    for (int candidate = first; candidate != inputVertices.morphVertSet.num;
         ++candidate) {
      const idMD6MorphSet& other = inputVertices.morphVertSet[candidate];
      if (other.meshIndex != source.meshIndex ||
          other.groupIndex != source.groupIndex)
        continue;
      processed[candidate] = true;
      for (int index = 0; index != other.indices.num; ++index)
        included[other.indices[index]] = true;
    }
    idMD6MorphSet* merged = &outputVertices.morphVertSet.Alloc();
    merged->meshIndex = source.meshIndex;
    merged->groupIndex = source.groupIndex;
    merged->type = source.type;
    merged->pos = source.pos;
    merged->size = source.size;
    merged->axis = source.axis;
    for (std::uint32_t index = 0; index != 65536; ++index) {
      if (included[static_cast<int>(index)])
        merged->indices.Append(static_cast<std::uint16_t>(index));
    }
  }
}

void idMorphVertGen::GenerateMorphVertsFromFile(
    const idDeclMD6* decl, const char* generationFile,
    idMorphVertices& vertices) {
  if (decl == nullptr) {
    idLib::Warning("%s - NULL decl", "idMorphVertGen::GenerateMorphVertsFromFile");
    return;
  }
  if (generationFile == nullptr) {
    idLib::Warning("%s - NULL projectionMeshName",
                   "idMorphVertGen::GenerateMorphVertsFromFile");
    return;
  }
  if (decl->model == nullptr) {
    idLib::Warning("%s - %s has no model",
                   "idMorphVertGen::GenerateMorphVertsFromFile", decl->name.str);
    return;
  }
  idStaticModel projectionModel;
  projectionModel.SetName(generationFile);
  projectionModel.PristineLoadResource();
  if (projectionModel.surfaces.num == 0) {
    idLib::Warning("%s - Unable to load '%s'",
                   "idMorphVertGen::GenerateMorphVertsFromFile", generationFile);
    return;
  }

  for (int surfaceIndex = 0; surfaceIndex != projectionModel.surfaces.num;
       ++surfaceIndex) {
    const idStaticModelSurface& surface = projectionModel.surfaces[surfaceIndex];
    if (surface.material == nullptr) {
      idLib::Warning("%s - Could not find material for %s",
                     "idMorphVertGen::GenerateMorphVertsFromFile",
                     decl->name.str);
      continue;
    }
    if (surface.geometry == nullptr)
      continue;
    const idTriangles& projection = *surface.geometry;
    const idDrawVert* projectionVertices = projection.verts;
    if (projectionVertices == nullptr) {
      projectionVertices = static_cast<const idDrawVert*>(
          const_cast<idVertexBuffer&>(projection.vertexBuffer).MapBuffer(0));
    }
    if (projectionVertices == nullptr) {
      idLib::Warning("%s - Could not find vertices for %s",
                     "idMorphVertGen::GenerateMorphVertsFromFile",
                     decl->name.str);
      continue;
    }
    const std::uint16_t* projectionIndexes = projection.indexes;
    if (projectionIndexes == nullptr) {
      projectionIndexes = static_cast<const std::uint16_t*>(
          const_cast<idIndexBuffer&>(projection.indexBuffer).MapBuffer(0));
    }
    if (projectionIndexes == nullptr) {
      idLib::Warning("%s - Could not find indexes for %s",
                     "idMorphVertGen::GenerateMorphVertsFromFile",
                     decl->name.str);
      continue;
    }
    morphShape_t shape(const_cast<idTriangles*>(&projection), projectionVertices,
                       surface.material->name.str, nullptr);
    if (shape.type == morphShape_t::CLIP_SPHERE) {
      idMD6MorphSet* set = &vertices.morphVertSet.Alloc();
      set->type = MORPHDATA_CLIPSPHERE;
      set->groupIndex = surface.materialNum;
      set->meshIndex = -1;
      set->pos = shape.center;
      set->size = {shape.radius, 0.0f, 0.0f};
      set->axis = mat3_identity;
      continue;
    }
    if (shape.type == morphShape_t::CLIP_ELLIPSOID) {
      idMD6MorphSet* set = &vertices.morphVertSet.Alloc();
      set->type = MORPHDATA_CLIPELLIPSOID;
      set->groupIndex = surface.materialNum;
      set->meshIndex = -1;
      set->pos = shape.center;
      set->axis[0] = shape.xAxis;
      set->axis[1] = shape.yAxis;
      set->axis[2] = shape.zAxis;
      continue;
    }
    if (shape.type == morphShape_t::NONE)
      continue;

    for (int meshIndex = 0; meshIndex != decl->model->meshes.num; ++meshIndex) {
      const idMD6Mesh* mesh = decl->model->meshes[meshIndex];
      if (mesh == nullptr || mesh->tris == nullptr ||
          (mesh->material != nullptr &&
           std::strstr(mesh->material->name.str, "gore") != nullptr))
        continue;
      const idTriangles& target = *mesh->tris;
      const idDrawVert* targetVertices = target.verts;
      bool targetMapped = false;
      if (targetVertices == nullptr &&
          (target.vertexBuffer.size & 0x7fffffff) != 0) {
        targetVertices = static_cast<const idDrawVert*>(
            const_cast<idVertexBuffer&>(target.vertexBuffer).MapBuffer(0));
        targetMapped = true;
      }
      if (targetVertices == nullptr) {
        idLib::Warning("%s - Unable to Map vertex buffer for '%s",
                       "idMorphVertGen::GenerateMorphVertsFromFile",
                       decl->model->name.str);
        continue;
      }
      idMD6MorphSet set;
      set.meshIndex = meshIndex;
      set.groupIndex = surface.materialNum;
      set.type = MORPHDATA_MORPH;
      for (int vertex = 0; vertex != target.numVerts; ++vertex) {
        idVec3 direction{};
        bool selected = false;
        if (shape.type == morphShape_t::SKIN_MESH) {
          selected =
              shape.SkinMeshMorphPoint(targetVertices[vertex].xyz, direction);
        } else if (shape.type == morphShape_t::SKIN_SPHERE) {
          direction = {};
          selected = (targetVertices[vertex].xyz - shape.center).LengthSqr() <=
                     shape.radiusSqr;
        }
        if (!selected)
          continue;
        if (direction.LengthSqr() != 0.0f) {
          for (int component = 0; component != 3; ++component) {
            const int encoded = std::clamp(
                static_cast<int>(((direction[component] * 0.03125f) * 0.5f +
                                  0.5f) *
                                 255.0f),
                0, 255);
            set.direction.Append(static_cast<std::uint8_t>(encoded));
          }
        }
        set.indices.Append(static_cast<std::uint16_t>(vertex));
      }
      if (set.indices.num != 0)
        vertices.morphVertSet.Append(set);
      if (targetMapped)
        const_cast<idVertexBuffer&>(target.vertexBuffer).UnmapBuffer();
    }
  }
}

void idMorphVertGen::GenerateMorphVerts(
    const idDeclMD6* decl, const idList<idMorphDef, 5>& morphDefs,
    idMorphVertices& vertices) {
  if (decl == nullptr) {
    idLib::Warning("idMorphVertGen::GenMorphVerts() - null decl passed in");
    return;
  }
  if (decl->model == nullptr) {
    idLib::Warning("idMorphVertGen::GenMorphVerts() - no model passed in for %s.",
                   decl->name.str);
    return;
  }
  if (morphDefs.num == 0) {
    idLib::Warning("idMorphVertGen::GenMorphVerts() - no morphDef passed in for %s.",
                   decl->name.str);
    return;
  }
  idMorphVertices temporary;
  for (int index = 0; index != morphDefs.num; ++index) {
    if (morphDefs[index].fileName.IsEmpty())
      ProcessModel(*decl->model, decl, morphDefs[index], temporary);
  }
  OptimizeVertices(vertices, temporary);
  for (int index = 0; index != morphDefs.num; ++index) {
    if (!morphDefs[index].fileName.IsEmpty())
      GenerateMorphVertsFromFile(decl, morphDefs[index].fileName.data,
                                 vertices);
  }
}
