#include "engine/models/discreteanimation/discreteanimationmodeldatagenerator.h"

#include "engine/cm/collisionmodel.h"
#include "engine/renderer/declmaterial.h"
#include "shared/idlib/filesystem/filesystemlocal.h"
#include "shared/idlib/lib.h"

#include <algorithm>
#include <cmath>
#include <cstdint>
#include <cstring>
#include <memory>
#include <map>
#include <new>
#include <set>
#include <type_traits>
#include <utility>
#include <vector>

idCollisionModel* BuildDiscreteAnimationCollisionModel(
    const char* name, const idVec3* vertices, int numVertices,
    const std::uint32_t* indexes, int numIndexes, idTraceModel* traceModel);
bool WriteDiscreteAnimationCollisionModel(
    const char* name, const idVec3* vertices, int numVertices,
    const std::uint32_t* indexes, int numIndexes);
void WriteSourceSurfaces(idFile* file,
                         const idList<sourceSurface_t, 5>* sourceSurfaces);

constexpr std::uint32_t kDiscreteAnimationVersion = 0x0a4c4d44u;
constexpr int kMaximumJointsPerSurface = 140;

template <typename First, typename Second> struct idPair {
  First first{};
  Second second{};
};

template <typename T> class idSort {
public:
  virtual ~idSort() = default;
  virtual void Sort(T* base, unsigned int count) = 0;
};

template <typename T, typename Comparator>
class idSort_Quick : public idSort<T> {
public:
  void Sort(T* base, unsigned int count) override;
};

class idSort_DiscreteMaterialNumber
    : public idSort_Quick<idPair<int, int>, idSort_DiscreteMaterialNumber> {
public:
  int Compare(const idPair<int, int>& lhs,
              const idPair<int, int>& rhs) const {
    return lhs.second - rhs.second;
  }
  ~idSort_DiscreteMaterialNumber() override = default;
};

template <typename T, typename Comparator>
void idSort_Quick<T, Comparator>::Sort(T* base, unsigned int count) {
  if (base == nullptr || count < 2)
    return;
  const auto* comparator = static_cast<const Comparator*>(this);
  std::sort(base, base + count,
            [comparator](const T& lhs, const T& rhs) {
              return comparator->Compare(lhs, rhs) < 0;
            });
}

namespace {

enum class TraceValidationCode {
  Ok,
  MaxVerts,
  MaxEdges,
  MaxPolys,
  MinVerts,
  Open,
  Invalid,
};

struct TraceValidation {
  TraceValidationCode code{TraceValidationCode::Ok};
  int count{};
};

std::uint16_t ByteSwap16(std::uint16_t value) {
  return static_cast<std::uint16_t>((value << 8) | (value >> 8));
}

std::uint32_t ByteSwap32(std::uint32_t value) {
  return ((value & 0x000000ffu) << 24) |
         ((value & 0x0000ff00u) << 8) |
         ((value & 0x00ff0000u) >> 8) |
         ((value & 0xff000000u) >> 24);
}

std::size_t WriteBig32(idFile* file, std::uint32_t value) {
  value = ByteSwap32(value);
  return file != nullptr ? file->Write(&value, sizeof(value)) : 0;
}

std::size_t WriteBigInt(idFile* file, int value) {
  return WriteBig32(file, static_cast<std::uint32_t>(value));
}

std::size_t WriteBigFloat(idFile* file, float value) {
  std::uint32_t bits = 0;
  std::memcpy(&bits, &value, sizeof(bits));
  return WriteBig32(file, bits);
}

void WriteBigBounds(idFile* file, const idBounds& bounds) {
  for (const idVec3& point : bounds.b) {
    WriteBigFloat(file, point.x);
    WriteBigFloat(file, point.y);
    WriteBigFloat(file, point.z);
  }
}

void WriteNativeString(idFile* file, const char* value) {
  const int length = value != nullptr ? static_cast<int>(std::strlen(value)) : 0;
  file->Write(&length, sizeof(length));
  if (length > 0)
    file->Write(value, static_cast<std::size_t>(length));
}

template <typename T>
void AppendUnique(idList<T, 5>& values, const T& value) {
  for (int index = 0; index < values.Num(); ++index) {
    if (values[index] == value)
      return;
  }
  values.Append(value);
}

void SetJointTransform(idJointMat& transform, const idVec3& translation) {
  std::memset(transform.mat, 0, sizeof(transform.mat));
  transform.mat[0] = 1.0f;
  transform.mat[5] = 1.0f;
  transform.mat[10] = 1.0f;
  transform.mat[3] = translation.x;
  transform.mat[7] = translation.y;
  transform.mat[11] = translation.z;
}

const idDrawVert* MapVertices(const idTriangles* geometry, bool& mapped) {
  mapped = false;
  if (geometry == nullptr)
    return nullptr;
  if (geometry->verts != nullptr)
    return geometry->verts;
  const auto* vertices = static_cast<const idDrawVert*>(
      geometry->vertexBuffer.MapBuffer(BM_READ));
  mapped = vertices != nullptr;
  return vertices;
}

const std::uint16_t* MapIndexes(const idTriangles* geometry, bool& mapped) {
  mapped = false;
  if (geometry == nullptr)
    return nullptr;
  if (geometry->indexes != nullptr)
    return geometry->indexes;
  const auto* indexes = static_cast<const std::uint16_t*>(
      geometry->indexBuffer.MapBuffer(BM_READ));
  mapped = indexes != nullptr;
  return indexes;
}

idTriangles* CloneTriangles(const idTriangles* source) {
  if (source == nullptr)
    return nullptr;

  auto destination = std::make_unique<idTriangles>();
  destination->bounds = source->bounds;
  destination->generateNormals = source->generateNormals;
  destination->detailOffset = source->detailOffset;
  destination->allowGpuHosting = source->allowGpuHosting;
  destination->vsb = source->vsb;
  destination->cpuVertexMask = source->cpuVertexMask;
  destination->vertexMask = source->vertexMask;
  destination->numVerts = source->numVerts;
  destination->numIndexes = source->numIndexes;

  bool mappedVertices = false;
  const idDrawVert* vertices = MapVertices(source, mappedVertices);
  if (source->numVerts > 0 && vertices != nullptr) {
    destination->verts = new idDrawVert[source->numVerts];
    std::memcpy(destination->verts, vertices,
                sizeof(idDrawVert) * static_cast<std::size_t>(source->numVerts));
  }
  if (mappedVertices)
    source->vertexBuffer.UnmapBuffer();

  bool mappedIndexes = false;
  const std::uint16_t* indexes = MapIndexes(source, mappedIndexes);
  if (source->numIndexes > 0 && indexes != nullptr) {
    destination->indexes = new std::uint16_t[source->numIndexes];
    std::memcpy(destination->indexes, indexes,
                sizeof(std::uint16_t) *
                    static_cast<std::size_t>(source->numIndexes));
  }
  if (mappedIndexes)
    source->indexBuffer.UnmapBuffer();
  return destination.release();
}

void ExtractCollisionGeometry(const idTriangles* geometry,
                              std::vector<idVec3>& vertices,
                              std::vector<std::uint32_t>& indexes) {
  vertices.clear();
  indexes.clear();
  if (geometry == nullptr)
    return;

  bool mappedVertices = false;
  const idDrawVert* sourceVertices = MapVertices(geometry, mappedVertices);
  if (sourceVertices != nullptr) {
    vertices.reserve(static_cast<std::size_t>(std::max(geometry->numVerts, 0)));
    for (int index = 0; index < geometry->numVerts; ++index)
      vertices.push_back(sourceVertices[index].xyz);
  }
  if (mappedVertices)
    geometry->vertexBuffer.UnmapBuffer();

  bool mappedIndexes = false;
  const std::uint16_t* sourceIndexes = MapIndexes(geometry, mappedIndexes);
  if (sourceIndexes != nullptr) {
    indexes.reserve(
        static_cast<std::size_t>(std::max(geometry->numIndexes, 0)));
    for (int index = 0; index < geometry->numIndexes; ++index)
      indexes.push_back(sourceIndexes[index]);
  }
  if (mappedIndexes)
    geometry->indexBuffer.UnmapBuffer();
}

TraceValidation ValidateTraceGeometry(const std::vector<idVec3>& vertices,
                                      const std::vector<std::uint32_t>& indexes) {
  if (vertices.size() < 4)
    return {TraceValidationCode::MinVerts, static_cast<int>(vertices.size())};
  if (vertices.size() > idTraceModel::MaxVerts)
    return {TraceValidationCode::MaxVerts, static_cast<int>(vertices.size())};
  if (indexes.empty() || indexes.size() % 3 != 0)
    return {TraceValidationCode::Invalid, static_cast<int>(indexes.size())};
  const int polygons = static_cast<int>(indexes.size() / 3);
  if (polygons > static_cast<int>(idTraceModel::MaxPolys))
    return {TraceValidationCode::MaxPolys, polygons};

  std::map<std::pair<std::uint32_t, std::uint32_t>, int> edgeUses;
  for (std::size_t triangle = 0; triangle < indexes.size(); triangle += 3) {
    for (int edge = 0; edge < 3; ++edge) {
      const std::uint32_t first = indexes[triangle + edge];
      const std::uint32_t second = indexes[triangle + (edge + 1) % 3];
      if (first >= vertices.size() || second >= vertices.size())
        return {TraceValidationCode::Invalid, static_cast<int>(first)};
      ++edgeUses[std::minmax(first, second)];
    }
  }
  if (edgeUses.size() > idTraceModel::MaxEdges)
    return {TraceValidationCode::MaxEdges, static_cast<int>(edgeUses.size())};
  for (const auto& edge : edgeUses) {
    if (edge.second != 2)
      return {TraceValidationCode::Open, static_cast<int>(edgeUses.size())};
  }
  return {};
}

const char* SurfaceTypeSuffix(surfTypes_t type) {
  static constexpr const char* names[] = {
      "none",          "metal",          "stone",
      "flesh",         "wood",           "cardboard",
      "liquid",        "glass",          "plastic",
      "asphalt",       "dirt",           "concrete",
      "foliage",       "linoleum",       "fabric",
      "rubber",        "rock",           "steam_pipe",
      "water_pipe",    "armor",          "sludge",
      "imp_nest",      "hollow_metal",   "flesh_player",
      "flesh_darkangel", "blood_pool",    "ash",
      "flesh_mutant",  "thick_padding",  "player_apc_metal",
      "rickety_metal", "rickety_wood",   "sheet_metal",
      "porcelain",     "plaster",        "vent_metal",
      "electronics",   "oil_pipe",       "panzer_metal",
      "breakable_armor", "electric_shield"};
  const int index = static_cast<int>(type);
  return index >= 0 &&
                 index < static_cast<int>(sizeof(names) / sizeof(names[0]))
             ? names[index]
             : "none";
}

const idMaterial* FindMaterial(const char* name, bool makeDefault = true) {
  if (name == nullptr || *name == '\0')
    return nullptr;
  return static_cast<const idMaterial*>(
      idMaterial::resourceList.FindWithInheritance(name, makeDefault, true));
}

void AppendError(idStr& error, const char* format, int first, int second = 0,
                 int third = 0) {
  idStr message;
  message.Format(format, first, second, third);
  error.Append(message);
}

void AddErrorSurface(idStaticModel* errorModel,
                     const idStaticModelSurface& source,
                     const char* errorMaterialName) {
  if (errorModel == nullptr)
    return;
  idStaticModelSurface copy = source;
  copy.geometry = CloneTriangles(source.geometry);
  copy.material = FindMaterial(errorMaterialName);
  errorModel->surfaces.Append(copy);
}

} // namespace

template <typename T>
T* idListArrayNew(int count, unsigned int, bool zeroBuffer) {
  if (count <= 0)
    return nullptr;
  T* values = new T[count];
  if constexpr (std::is_trivial<T>::value) {
    if (zeroBuffer)
      std::memset(values, 0, sizeof(T) * static_cast<std::size_t>(count));
  }
  return values;
}

template <typename T>
void idListArrayDelete(T* values, unsigned int) {
  delete[] values;
}

template <typename T>
T* idListArrayResize(T* oldValues, unsigned int oldCount, int newCount,
                     unsigned int tag, bool zeroBuffer) {
  T* replacement = idListArrayNew<T>(newCount, tag, zeroBuffer);
  const int copyCount = std::min(static_cast<int>(oldCount), newCount);
  for (int index = 0; index < copyCount; ++index)
    replacement[index] = std::move(oldValues[index]);
  idListArrayDelete(oldValues, oldCount);
  return replacement;
}

template idDiscreteAnimationModelSurface*
idListArrayNew<idDiscreteAnimationModelSurface>(int, unsigned int, bool);
template idDiscreteGenSurface*
idListArrayNew<idDiscreteGenSurface>(int, unsigned int, bool);
template idDiscreteBuildSurface*
idListArrayNew<idDiscreteBuildSurface>(int, unsigned int, bool);
template void idListArrayDelete<idDiscreteGenSurface>(idDiscreteGenSurface*,
                                                       unsigned int);
template void idListArrayDelete<idDiscreteBuildSurface>(
    idDiscreteBuildSurface*, unsigned int);
template idDiscreteGenSurface* idListArrayResize<idDiscreteGenSurface>(
    idDiscreteGenSurface*, unsigned int, int, unsigned int, bool);
template idDiscreteBuildSurface* idListArrayResize<idDiscreteBuildSurface>(
    idDiscreteBuildSurface*, unsigned int, int, unsigned int, bool);
template idDiscreteAnimationModelSurface*
idListArrayResize<idDiscreteAnimationModelSurface>(
    idDiscreteAnimationModelSurface*, unsigned int, int, unsigned int, bool);

const idMaterial* GetCollisionTypeMaterial(const idMaterial* material) {
  surfTypes_t surfaceType = SURFTYPE_NONE;
  if (material != nullptr) {
    surfaceType = material->GetSurfaceType();
    if (surfaceType == SURFTYPE_INVALID)
      surfaceType = SURFTYPE_NONE;
  }
  idStr materialName;
  materialName.Format("textures/common/collision%s",
                      surfaceType == SURFTYPE_NONE
                          ? "_none"
                          : SurfaceTypeSuffix(surfaceType));
  materialName.ToLower();
  return FindMaterial(materialName.data, false);
}

idDiscreteGenSurface::idDiscreteGenSurface()
    : materialNum(-1), surfTypeMaterial(nullptr) {}

idDiscreteGenSurface::~idDiscreteGenSurface() {
  materialNum = -1;
  surfTypeMaterial = nullptr;
  FreeVisibleData();
}

void idDiscreteGenSurface::FreeVisibleData() {
  visibleGeometry.DeleteContents();
  visibleMappingMaterial.ClearFree();
  visibleGenericMaterial.ClearFree();
}

idDiscreteBuildSurface::idDiscreteBuildSurface() = default;
idDiscreteBuildSurface::~idDiscreteBuildSurface() = default;

idDiscreteAnimationModelDataGenerator::idDiscreteAnimationModelDataGenerator() {
  timestamp = static_cast<std::uint32_t>(-1);
  errorModel = nullptr;
}

idDiscreteAnimationModelDataGenerator::~idDiscreteAnimationModelDataGenerator() {
  FreeData();
}

void idDiscreteAnimationModelDataGenerator::FreeRenderSurfaces() {
  for (int index = 0; index < renderSurfaces.Num(); ++index) {
    delete renderSurfaces[index].triangles;
    renderSurfaces[index].triangles = nullptr;
  }
  renderSurfaces.ClearFree();
}

void idDiscreteAnimationModelDataGenerator::FreeData() {
  FreeRenderSurfaces();
  for (int index = 0; index < surfaces.Num(); ++index)
    surfaces[index].FreeVisibleData();
  surfaces.ClearFree();
  baseGeometry.DeleteContents();
  collisionModels.ClearFree();
  traceModels.ClearFree();
  traceMaterials.ClearFree();
  transforms.ClearFree();
  sourceSurfaces.ClearFree();
  baseMaterial.ClearFree();
  delete errorModel;
  errorModel = nullptr;
}

void idDiscreteAnimationModelDataGenerator::WriteBaseModel(
    const char* fileName) {
  std::vector<idVec3> vertices;
  std::vector<std::uint32_t> indexes;
  for (int surfaceIndex = 0; surfaceIndex < baseGeometry.Num();
       ++surfaceIndex) {
    std::vector<idVec3> surfaceVertices;
    std::vector<std::uint32_t> surfaceIndexes;
    ExtractCollisionGeometry(baseGeometry[surfaceIndex], surfaceVertices,
                             surfaceIndexes);
    const std::uint32_t vertexOffset =
        static_cast<std::uint32_t>(vertices.size());
    vertices.insert(vertices.end(), surfaceVertices.begin(),
                    surfaceVertices.end());
    for (std::uint32_t index : surfaceIndexes)
      indexes.push_back(index + vertexOffset);
  }
  WriteDiscreteAnimationCollisionModel(
      fileName, vertices.data(), static_cast<int>(vertices.size()),
      indexes.data(), static_cast<int>(indexes.size()));
}

bool idDiscreteAnimationModelDataGenerator::Generate(const char* modelName) {
  FreeData();
  errorModel = new (std::nothrow) idStaticModel;
  errorString.Empty();
  if (modelName == nullptr || errorModel == nullptr)
    return false;

  idStr baseName(modelName);
  baseName.StripFileExtension();

  std::unique_ptr<idStaticModel> staticModel(new (std::nothrow) idStaticModel);
  if (!staticModel)
    return false;
  staticModel->SetName(modelName);
  staticModel->PristineLoadResource();
  if (staticModel->resourceError != nullptr) {
    idLib::Warning(
        "idDiscreteAnimationModelDataGenerator: Couldn't load model '%s' to generate from",
        modelName);
    return false;
  }
  if (staticModel->surfaces.Num() == 0) {
    idLib::Warning(
        "idDiscreteAnimationModelDataGenerator: model '%s' doesn't contain any surfaces to generate from",
        modelName);
    return false;
  }

  idList<idPair<int, int>, 5> materialIndices;
  idList<int, 5> materialNumbers;
  materialIndices.SetNum(staticModel->surfaces.Num());
  int invalidMaterials = 0;
  for (int index = 0; index < staticModel->surfaces.Num(); ++index) {
    const idStaticModelSurface& surface = staticModel->surfaces[index];
    materialIndices[index] = {index, surface.materialNum};
    if (surface.materialNum == -1) {
      ++invalidMaterials;
      const char* materialName =
          surface.material != nullptr ? surface.material->name.str : "<unknown>";
      idLib::Warning(
          "idDiscreteAnimationModelDataGenerator: model '%s' uses invalid material '%s'",
          staticModel->name.str, materialName);
      AddErrorSurface(errorModel, surface,
                      "editor/error/discrete/invalidmaterialname");
    } else {
      AppendUnique(materialNumbers, surface.materialNum);
    }
  }
  if (invalidMaterials != 0) {
    AppendError(errorString,
                "Found %d surfaces using invalid or missing material names",
                invalidMaterials);
    idLib::Warning(
        "idDiscreteAnimationModelDataGenerator: model contains %d invalid materials",
        invalidMaterials);
    return false;
  }

  idSort_DiscreteMaterialNumber sorter;
  sorter.Sort(materialIndices.list,
              static_cast<unsigned int>(materialIndices.Num()));

  surfaces.SetNum(materialNumbers.Num());
  surfaces.Clear();
  idList<const idRenderModelSurface*, 5> collisionSurfaces;
  collisionSurfaces.SetNum(materialNumbers.Num(), nullptr);
  idList<int, 5> invalidInvisibleIndex;
  idList<int, 5> invalidCollisionIndex;

  for (int sortedIndex = 0; sortedIndex < materialIndices.Num();
       ++sortedIndex) {
    const int sourceIndex = materialIndices[sortedIndex].first;
    const idStaticModelSurface& source = staticModel->surfaces[sourceIndex];
    int groupIndex = -1;
    for (int index = 0; index < surfaces.Num(); ++index) {
      if (surfaces[index].materialNum == source.materialNum) {
        groupIndex = index;
        break;
      }
    }
    if (groupIndex < 0) {
      idDiscreteGenSurface* group = surfaces.Alloc();
      if (group == nullptr)
        return false;
      group->materialNum = source.materialNum;
      groupIndex = surfaces.Num() - 1;
    }

    idDiscreteGenSurface& group = surfaces[groupIndex];
    if (source.material->coverage != MC_INVISIBLE) {
      idTriangles* geometry = CloneTriangles(source.geometry);
      if (geometry == nullptr)
        return false;
      group.visibleGeometry.Append(geometry);
      group.visibleGenericMaterial.Append(
          source.material->CompatibleGenericMaterial());
      group.visibleMappingMaterial.Append(source.material);
    } else if ((source.material->GetSurfaceFlags() & SURF_COLLISION) != 0) {
      if (collisionSurfaces[groupIndex] == nullptr)
        collisionSurfaces[groupIndex] = &source;
      else
        invalidCollisionIndex.Append(sortedIndex);
    } else {
      invalidInvisibleIndex.Append(sortedIndex);
    }
  }

  collisionModels.SetNum(surfaces.Num(), nullptr);
  idList<int, 5> baseSurfaces;
  idList<TraceValidation, 5> collisionValidation;
  idList<idTraceModel, 5> collisionTraceCandidates;
  collisionValidation.SetNum(surfaces.Num());
  collisionTraceCandidates.SetNum(surfaces.Num());
  for (int groupIndex = 0; groupIndex < surfaces.Num(); ++groupIndex) {
    bool invalidInvisible = false;
    bool invalidCollision = false;
    for (int index = 0; index < invalidInvisibleIndex.Num(); ++index)
      invalidInvisible |= materialIndices[invalidInvisibleIndex[index]].second ==
                          surfaces[groupIndex].materialNum;
    for (int index = 0; index < invalidCollisionIndex.Num(); ++index)
      invalidCollision |= materialIndices[invalidCollisionIndex[index]].second ==
                          surfaces[groupIndex].materialNum;
    if (invalidInvisible || invalidCollision ||
        collisionSurfaces[groupIndex] == nullptr) {
      continue;
    }

    const idStaticModelSurface* collisionSurface =
        static_cast<const idStaticModelSurface*>(collisionSurfaces[groupIndex]);
    surfaces[groupIndex].surfTypeMaterial =
        GetCollisionTypeMaterial(collisionSurface->material);
    if (surfaces[groupIndex].surfTypeMaterial == nullptr ||
        surfaces[groupIndex].surfTypeMaterial->GetSurfaceType() ==
            SURFTYPE_NONE) {
      for (int visible = 0;
           visible < surfaces[groupIndex].visibleMappingMaterial.Num();
           ++visible) {
        const idMaterial* candidate = GetCollisionTypeMaterial(
            surfaces[groupIndex].visibleMappingMaterial[visible]);
        if (candidate != nullptr &&
            candidate->GetSurfaceType() != SURFTYPE_NONE) {
          surfaces[groupIndex].surfTypeMaterial = candidate;
          break;
        }
      }
    }

    std::vector<idVec3> vertices;
    std::vector<std::uint32_t> indexes;
    ExtractCollisionGeometry(collisionSurface->geometry, vertices, indexes);
    collisionValidation[groupIndex] = ValidateTraceGeometry(vertices, indexes);
    idStr collisionName;
    collisionName.Format("discreteanimation/%s_%.02i", baseName.data,
                         groupIndex);
    collisionModels[groupIndex] = BuildDiscreteAnimationCollisionModel(
        collisionName.data, vertices.data(), static_cast<int>(vertices.size()),
        indexes.data(), static_cast<int>(indexes.size()),
        &collisionTraceCandidates[groupIndex]);
    if (collisionValidation[groupIndex].code != TraceValidationCode::Ok ||
        collisionModels[groupIndex] == nullptr) {
      baseSurfaces.Append(groupIndex);
    }
  }

  const int baseSurface =
      baseSurfaces.Num() > 0 ? baseSurfaces[baseSurfaces.Num() - 1] : -1;
  traceModels.Clear();
  traceMaterials.Clear();
  transforms.Clear();
  maxRadius = -1.0e30f;
  for (int groupIndex = 0; groupIndex < surfaces.Num(); ++groupIndex) {
    if (groupIndex == baseSurface || collisionModels[groupIndex] == nullptr ||
        collisionValidation[groupIndex].code != TraceValidationCode::Ok) {
      continue;
    }
    idTraceModel trace = collisionTraceCandidates[groupIndex];
    float mass = 0.0f;
    idVec3 centerOfMass{};
    idMat3 inertiaTensor{};
    trace.GetMassProperties(1.0f, mass, centerOfMass, inertiaTensor);
    trace.Translate({-centerOfMass.x, -centerOfMass.y, -centerOfMass.z});
    traceModels.Append(trace);
    traceMaterials.Append(surfaces[groupIndex].surfTypeMaterial);
    idJointMat transform{};
    SetJointTransform(transform, centerOfMass);
    transforms.Append(transform);
    maxRadius = std::max(maxRadius, trace.bounds.GetRadius());
  }

  if (baseSurface >= 0) {
    idJointMat identity{};
    SetJointTransform(identity, {});
    transforms.Append(identity);
    const idStaticModelSurface* collisionSurface =
        static_cast<const idStaticModelSurface*>(collisionSurfaces[baseSurface]);
    const surfTypes_t collisionType =
        collisionSurface->material != nullptr
            ? collisionSurface->material->GetSurfaceType()
            : SURFTYPE_NONE;
    if (collisionType != SURFTYPE_INVALID &&
        collisionType != SURFTYPE_NONE) {
      idTriangles* geometry = CloneTriangles(collisionSurface->geometry);
      if (geometry != nullptr) {
        baseGeometry.Append(geometry);
        baseMaterial.Append(collisionSurface->material);
      }
    } else {
      const int materialNumber = surfaces[baseSurface].materialNum;
      for (int groupIndex = 0; groupIndex < surfaces.Num(); ++groupIndex) {
        if (surfaces[groupIndex].materialNum != materialNumber)
          continue;
        for (int visible = 0;
             visible < surfaces[groupIndex].visibleGeometry.Num(); ++visible) {
          const idMaterial* mapping =
              surfaces[groupIndex].visibleMappingMaterial[visible];
          if (mapping == nullptr)
            continue;
          idTriangles* geometry =
              CloneTriangles(surfaces[groupIndex].visibleGeometry[visible]);
          if (geometry != nullptr) {
            baseGeometry.Append(geometry);
            baseMaterial.Append(mapping);
          }
        }
      }
    }
  }

  referenceBounds = staticModel->referenceBounds;

  if (invalidInvisibleIndex.Num() > 0) {
    AppendError(
        errorString,
        "Found %d invisible surface(s), all invisible surfaces must be flaged as collision surfaces.\n",
        invalidInvisibleIndex.Num());
    for (int index = 0; index < invalidInvisibleIndex.Num(); ++index) {
      const idStaticModelSurface& source = staticModel->surfaces[
          materialIndices[invalidInvisibleIndex[index]].first];
      AddErrorSurface(errorModel, source,
                      "editor/error/discrete/invalidinvisible");
    }
    errorString.Append("\n");
  }

  if (invalidCollisionIndex.Num() > 0) {
    idList<int, 5> invalidGroups;
    for (int index = 0; index < invalidCollisionIndex.Num(); ++index)
      AppendUnique(invalidGroups,
                   materialIndices[invalidCollisionIndex[index]].second);
    AppendError(
        errorString,
        "Found %d group(s) with multiple collision meshes, each group can only contain one mesh.\n",
        invalidGroups.Num());
    for (int index = 0; index < invalidCollisionIndex.Num(); ++index) {
      const idStaticModelSurface& source = staticModel->surfaces[
          materialIndices[invalidCollisionIndex[index]].first];
      AddErrorSurface(errorModel, source,
                      "editor/error/discrete/multiplecollision");
    }
    errorString.Append("\n");
  }

  if (baseSurfaces.Num() > 1) {
    AppendError(
        errorString,
        "Using group '%d' as base collision but found '%d additional model(s), can't have more than one.\n",
        surfaces[baseSurface].materialNum, baseSurfaces.Num() - 1);
    for (int index = 0; index < baseSurfaces.Num(); ++index) {
      const int groupIndex = baseSurfaces[index];
      if (groupIndex == baseSurface)
        continue;
      const TraceValidation validation = collisionValidation[groupIndex];
      switch (validation.code) {
        case TraceValidationCode::MaxVerts:
          AppendError(errorString,
                      "   - Collision group number '%d' is to complex, too many vertices %d / %d.\n",
                      surfaces[groupIndex].materialNum, validation.count,
                      static_cast<int>(idTraceModel::MaxVerts));
          break;
        case TraceValidationCode::MaxEdges:
          AppendError(errorString,
                      "   - Collision group number '%d' is to complex, too many edges %d / %d.\n",
                      surfaces[groupIndex].materialNum, validation.count,
                      static_cast<int>(idTraceModel::MaxEdges));
          break;
        case TraceValidationCode::MaxPolys:
          AppendError(errorString,
                      "   - Collision group number '%d' is to complex, too many polygons %d / %d.\n",
                      surfaces[groupIndex].materialNum, validation.count,
                      static_cast<int>(idTraceModel::MaxPolys));
          break;
        case TraceValidationCode::MinVerts:
          AppendError(errorString,
                      "   - Collision group number '%d' is to simple, too few vertices %d / %d.\n",
                      surfaces[groupIndex].materialNum, validation.count, 4);
          break;
        case TraceValidationCode::Open:
          AppendError(errorString,
                      "   - Collision group number '%d' is not a closed hull.\n",
                      surfaces[groupIndex].materialNum);
          break;
        default:
          AppendError(errorString,
                      "   - Collision group number '%d' can't be converted to physics geometry.\n",
                      surfaces[groupIndex].materialNum);
          break;
      }
      const idStaticModelSurface* source =
          static_cast<const idStaticModelSurface*>(collisionSurfaces[groupIndex]);
      if (source != nullptr)
        AddErrorSurface(errorModel, *source,
                        "editor/error/discrete/invalidcollision");
    }
    errorString.Append("\n");
  }

  if (invalidInvisibleIndex.Num() > 0 || invalidCollisionIndex.Num() > 0 ||
      baseSurfaces.Num() > 1) {
    for (int index = 0; index < staticModel->surfaces.Num(); ++index) {
      const idStaticModelSurface& source = staticModel->surfaces[index];
      if (source.material != nullptr &&
          source.material->coverage != MC_INVISIBLE) {
        AddErrorSurface(errorModel, source, source.material->name.str);
      }
    }
    return false;
  }

  delete errorModel;
  errorModel = nullptr;
  timestamp = staticModel->sourceTimeStamp;
  return true;
}

void idDiscreteAnimationModelDataGenerator::GenerateRenderSurfaces() {
  int totalIndexes = 0;
  int totalVertices = 0;
  for (int groupIndex = 0; groupIndex < surfaces.Num(); ++groupIndex) {
    for (int visible = 0; visible < surfaces[groupIndex].visibleGeometry.Num();
         ++visible) {
      const idTriangles* geometry = surfaces[groupIndex].visibleGeometry[visible];
      if (geometry != nullptr) {
        totalIndexes += geometry->numIndexes;
        totalVertices += geometry->numVerts;
      }
    }
  }

  std::vector<std::uint16_t> combinedIndexes(
      static_cast<std::size_t>(std::max(totalIndexes, 0)));
  std::vector<idDrawVert> combinedVertices(
      static_cast<std::size_t>(std::max(totalVertices, 0)));
  idList<idDiscreteBuildSurface, 5> buildSurfaces;

  for (int groupIndex = 0; groupIndex < surfaces.Num(); ++groupIndex) {
    idDiscreteGenSurface& group = surfaces[groupIndex];
    for (int visible = 0; visible < group.visibleGeometry.Num(); ++visible) {
      const idMaterial* material = group.visibleGenericMaterial[visible];
      if (material == nullptr)
        material = group.visibleMappingMaterial[visible];
      int buildIndex = -1;
      for (int index = 0; index < buildSurfaces.Num(); ++index) {
        if (buildSurfaces[index].material != nullptr && material != nullptr &&
            buildSurfaces[index].material->MaterialIsMergableWith(material) &&
            groupIndex <
                buildSurfaces[index].jointOffset + kMaximumJointsPerSurface) {
          buildIndex = index;
          break;
        }
      }
      if (buildIndex < 0) {
        idDiscreteBuildSurface* build = buildSurfaces.Alloc();
        if (build == nullptr)
          return;
        build->material = material;
        build->jointOffset = groupIndex & ~15;
        buildIndex = buildSurfaces.Num() - 1;
      }
      idDiscreteBuildSurface& build = buildSurfaces[buildIndex];
      build.maxJoint = groupIndex;
      build.sourceMaterial.Append(group.visibleMappingMaterial[visible]);
      build.geometry.Append(group.visibleGeometry[visible]);
      build.jointIndex.Append(groupIndex);
    }
  }

  FreeRenderSurfaces();
  renderSurfaces.SetNum(buildSurfaces.Num());
  renderSurfaces.Clear();
  for (int buildIndex = 0; buildIndex < buildSurfaces.Num(); ++buildIndex) {
    idDiscreteBuildSurface& build = buildSurfaces[buildIndex];
    int vertexCount = 0;
    int indexCount = 0;
    for (int geometryIndex = 0; geometryIndex < build.geometry.Num();
         ++geometryIndex) {
      const idTriangles* geometry = build.geometry[geometryIndex];
      if (geometry == nullptr)
        continue;
      const int jointIndex = build.jointIndex[geometryIndex];
      const idJointMat& transform = transforms[jointIndex];
      const idVec3 translation{transform.mat[3], transform.mat[7],
                               transform.mat[11]};

      sourceSurface_t source{};
      source.mtr = build.sourceMaterial[geometryIndex];
      source.mtrChecksum = source.mtr != nullptr ? source.mtr->md5Checksum : 0;
      source.renderSurface = renderSurfaces.Num();
      source.firstVertex = vertexCount;
      source.lastVertex = vertexCount + geometry->numVerts - 1;
      sourceSurfaces.Append(source);

      bool mappedIndexes = false;
      const std::uint16_t* indexes = MapIndexes(geometry, mappedIndexes);
      if (indexes != nullptr) {
        for (int index = 0; index < geometry->numIndexes; ++index)
          combinedIndexes[static_cast<std::size_t>(indexCount++)] =
              static_cast<std::uint16_t>(indexes[index] + vertexCount);
      }
      if (mappedIndexes)
        geometry->indexBuffer.UnmapBuffer();

      bool mappedVertices = false;
      const idDrawVert* vertices = MapVertices(geometry, mappedVertices);
      if (vertices != nullptr) {
        const std::uint8_t relativeJoint = static_cast<std::uint8_t>(
            jointIndex - build.jointOffset);
        for (int vertex = 0; vertex < geometry->numVerts; ++vertex) {
          idDrawVert& destination =
              combinedVertices[static_cast<std::size_t>(vertexCount++)];
          destination = vertices[vertex];
          destination.xyz = destination.xyz - translation;
          destination.color[0] = relativeJoint;
          destination.color[1] = relativeJoint;
          destination.color[2] = relativeJoint;
          destination.color[3] = relativeJoint;
          destination.normal[3] = 0;
        }
      }
      if (mappedVertices)
        geometry->vertexBuffer.UnmapBuffer();
    }

    auto* triangles = new idTriangles;
    triangles->bounds = referenceBounds;
    triangles->numVerts = vertexCount;
    triangles->numIndexes = indexCount;
    triangles->verts = vertexCount > 0 ? new idDrawVert[vertexCount] : nullptr;
    triangles->indexes =
        indexCount > 0 ? new std::uint16_t[indexCount] : nullptr;
    if (vertexCount > 0)
      std::memcpy(triangles->verts, combinedVertices.data(),
                  sizeof(idDrawVert) * static_cast<std::size_t>(vertexCount));
    if (indexCount > 0)
      std::memcpy(triangles->indexes, combinedIndexes.data(),
                  sizeof(std::uint16_t) * static_cast<std::size_t>(indexCount));

    idDiscreteAnimationModelSurface renderSurface;
    renderSurface.jointOffset = build.jointOffset;
    renderSurface.numJoints = build.maxJoint - build.jointOffset + 1;
    renderSurface.triangles = triangles;
    renderSurface.material = build.material;
    renderSurfaces.Append(renderSurface);
  }
}

template <>
std::size_t idFile::WriteBigArray<idDrawVert>(const idDrawVert* values,
                                              int count) {
  std::size_t written = 0;
  for (int index = 0; index < count; ++index) {
    idDrawVert swapped = values[index];
    auto* words = reinterpret_cast<std::uint32_t*>(&swapped);
    for (int word = 0; word < 5; ++word)
      words[word] = ByteSwap32(words[word]);
    written += Write(&swapped, sizeof(swapped));
  }
  return written;
}

bool idDiscreteAnimationModelDataGenerator::WriteBinary(const char* fileName) {
  GenerateRenderSurfaces();
  idStr baseModelName;
  if (baseGeometry.Num() > 0) {
    baseModelName = fileName;
    if (baseModelName.Icmpn("generated/", 10) == 0)
      baseModelName.StripLeadingOnce("generated/");
    baseModelName.StripFileExtension();
    baseModelName.Append("_base");
    WriteBaseModel(baseModelName.data);
  }

  if (fileSystem == nullptr || fileName == nullptr)
    return false;
  idFileLocal file(fileSystem->OpenFileWrite(fileName));
  if (file.file == nullptr) {
    idLib::Warning("Could not open %s", fileName);
    return false;
  }

  idLib::Printf("Writing %s\n", fileName);
  for (int index = 0; index < traceModels.Num(); ++index)
    traceModels[index].Swap();

  WriteBig32(file.file, kDiscreteAnimationVersion);
  WriteBig32(file.file, timestamp);
  WriteBigFloat(file.file, maxRadius);
  WriteBigBounds(file.file, referenceBounds);
  WriteBigInt(file.file, traceModels.Num());
  if (traceModels.Num() > 0) {
    file.file->Write(traceModels.list,
                     sizeof(idTraceModel) *
                         static_cast<std::size_t>(traceModels.Num()));
  }

  WriteBigInt(file.file, traceMaterials.Num());
  for (int index = 0; index < traceMaterials.Num(); ++index) {
    const idMaterial* material = traceMaterials[index];
    WriteNativeString(file.file,
                      material != nullptr ? material->name.str : nullptr);
  }

  WriteBigInt(file.file, transforms.Num());
  for (int index = 0; index < transforms.Num(); ++index) {
    for (float value : transforms[index].mat)
      WriteBigFloat(file.file, value);
  }

  WriteBigInt(file.file, renderSurfaces.Num());
  for (int index = 0; index < renderSurfaces.Num(); ++index) {
    const idDiscreteAnimationModelSurface& surface = renderSurfaces[index];
    WriteNativeString(file.file, surface.material != nullptr
                                     ? surface.material->name.str
                                     : nullptr);
    WriteBigInt(file.file, surface.jointOffset);
    WriteBigInt(file.file, surface.numJoints);
    WriteBigInt(file.file, surface.triangles->numVerts);
    WriteBigInt(file.file, surface.triangles->numIndexes);
    file.file->WriteBigArray(surface.triangles->verts,
                             surface.triangles->numVerts);
    for (int element = 0; element < surface.triangles->numIndexes; ++element) {
      const std::uint16_t swapped =
          ByteSwap16(surface.triangles->indexes[element]);
      file.file->Write(&swapped, sizeof(swapped));
    }
  }

  WriteSourceSurfaces(file.file, &sourceSurfaces);
  const bool hasBaseModel = !baseModelName.IsEmpty();
  file.file->Write(&hasBaseModel, sizeof(hasBaseModel));
  if (hasBaseModel)
    WriteNativeString(file.file, baseModelName.data);

  for (int index = 0; index < traceModels.Num(); ++index)
    traceModels[index].Swap();
  return true;
}
