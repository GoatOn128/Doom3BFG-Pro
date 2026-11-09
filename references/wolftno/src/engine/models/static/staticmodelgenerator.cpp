#include "engine/models/static/staticmodel.h"

#include "engine/compilers/renderbump/mtltocpuvmtr.h"
#include "engine/decls/declskins.h"
#include "engine/models/static/staticmodel_ase.h"
#include "engine/models/static/staticmodel_lwo.h"
#include "engine/models/static/staticmodel_obj.h"
#include "engine/models/static/staticmodelrawsurface.h"
#include "engine/models/static/staticmodeltristrips.h"
#include "engine/renderer/declmaterial.h"
#include "engine/renderer/declrenderparm.h"
#include "engine/renderer/triangles.h"
#include "shared/idlib/csystems/cvarsystemlocal.h"
#include "shared/idlib/filesystem/filesystemlocal.h"
#include "shared/idlib/geometry/geometry.h"
#include "shared/idlib/lib.h"

#include <algorithm>
#include <array>
#include <cmath>
#include <cstdint>
#include <cstdlib>
#include <cstring>
#include <limits>
#include <map>
#include <vector>

extern idCVar decl_mergeGenericMaterials;

template <typename Vector, int Dimension> class idVectorSubset {
public:
  ~idVectorSubset();
  void Init(const Vector* minimums, const Vector* maximums, int hashSize,
            int initialSize);
  int FindVector(const Vector* vectors, int vectorNumber, float epsilon);

private:
  const Vector* vectors{};
  int vectorCount{};
  Vector mins{};
  Vector maxs{};
  int boxHashSize{};
};

template <typename Vector, int Dimension>
idVectorSubset<Vector, Dimension>::~idVectorSubset() = default;

template <typename Vector, int Dimension>
void idVectorSubset<Vector, Dimension>::Init(const Vector* minimums,
                                             const Vector* maximums,
                                             int hashSize, int) {
  vectors = nullptr;
  vectorCount = 0;
  mins = *minimums;
  maxs = *maximums;
  boxHashSize = hashSize;
}

template <typename Vector, int Dimension>
int idVectorSubset<Vector, Dimension>::FindVector(const Vector* vectorList,
                                                  int vectorNumber,
                                                  float epsilon) {
  vectors = vectorList;
  for (int candidate = 0; candidate < vectorNumber; ++candidate) {
    bool equal = true;
    for (int component = 0; component < Dimension; ++component) {
      if (std::fabs(vectorList[vectorNumber][component] -
                    vectorList[candidate][component]) > epsilon) {
        equal = false;
        break;
      }
    }
    if (equal)
      return candidate;
  }
  vectorCount = vectorNumber + 1;
  return vectorNumber;
}

struct idSortedMaterial {
  const char* name{};
  const idMaterial* material{};
  int materialNumber{-1};
  int originalIndex{};
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

class idSort_SortedMaterial final
    : public idSort_Quick<idSortedMaterial, idSort_SortedMaterial> {
public:
  int Compare(const idSortedMaterial& left,
              const idSortedMaterial& right) const {
    const int leftInvisible =
        left.material != nullptr && left.material->Coverage() == MC_INVISIBLE;
    const int rightInvisible =
        right.material != nullptr && right.material->Coverage() == MC_INVISIBLE;
    return leftInvisible == rightInvisible
               ? left.materialNumber - right.materialNumber
               : leftInvisible - rightInvisible;
  }
  ~idSort_SortedMaterial() override = default;
};

template <typename T, typename Comparator>
void idSort_Quick<T, Comparator>::Sort(T* base, unsigned int count) {
  if (base == nullptr || count < 2)
    return;
  const auto* comparator = static_cast<const Comparator*>(this);
  std::sort(base, base + count,
            [comparator](const T& left, const T& right) {
              return comparator->Compare(left, right) < 0;
            });
}

idCVar r_slopVertex("r_slopVertex", "0.01", 0,
                    "merge xyz coordinates this far apart");
idCVar r_slopTexCoord("r_slopTexCoord", "0.0001", 0,
                      "merge texture coordinates this far apart");
idCVar r_slopNormal("r_slopNormal", "0.0001", 0,
                    "merge normals that dot less than this");
idCVar r_staticModelTriStrips("r_staticModelTriStrips", "1", CVAR_BOOL,
                             "build tristrips for static models");
idCVar r_mergeModelSurfaces("r_mergeModelSurfaces", "1", CVAR_BOOL,
                            "combine model surfaces with the same material");

namespace {

constexpr std::uint32_t kStaticModelDiskMagic = 0x424d4c1eu;
constexpr unsigned int kLwoMorph = 0x4d4f5246u;
constexpr unsigned int kLwoTexCoord = 0x54585556u;
constexpr unsigned int kLwoNormal = 0x4e4f524du;
constexpr unsigned int kLwoColor = 0x52474241u;

std::uint32_t ByteSwap32(std::uint32_t value) {
  return ((value & 0x000000ffu) << 24) |
         ((value & 0x0000ff00u) << 8) |
         ((value & 0x00ff0000u) >> 8) |
         ((value & 0xff000000u) >> 24);
}

void WriteBig32(idFile* file, std::uint32_t value) {
  const std::uint32_t swapped = ByteSwap32(value);
  file->Write(&swapped, sizeof(swapped));
}

void WriteBigVec3(idFile* file, const idVec3& value) {
  std::uint32_t bits[3]{};
  std::memcpy(bits, &value, sizeof(bits));
  for (std::uint32_t& part : bits)
    part = ByteSwap32(part);
  file->Write(bits, sizeof(bits));
}

std::uint8_t ClampByte(float value) {
  return static_cast<std::uint8_t>(
      std::clamp(static_cast<int>(value), 0, 255));
}

std::uint8_t EncodeDirection(float value) {
  return ClampByte(std::nearbyint((value + 1.0f) * 127.5f + 0.5f));
}

idVec3 NormalizeSafe(const idVec3& value) {
  const float inverseLength =
      1.0f / std::sqrt(std::max(value.LengthSqr(),
                                std::numeric_limits<float>::min()));
  return value * inverseLength;
}

void FixDegenerateNormal(idVec3& normal) {
  if (normal.LengthSqr() <= std::numeric_limits<float>::min()) {
    normal = {0.0f, 0.0f, 1.0f};
    return;
  }
  normal = NormalizeSafe(normal);
  if (normal.x == 1.0f || normal.x == -1.0f) {
    normal.y = 0.0f;
    normal.z = 0.0f;
  } else if (normal.y == 1.0f || normal.y == -1.0f) {
    normal.x = 0.0f;
    normal.z = 0.0f;
  } else if (normal.z == 1.0f || normal.z == -1.0f) {
    normal.x = 0.0f;
    normal.y = 0.0f;
  }
}

bool ValidateMaterialNames(const char* modelName,
                           const idList<idStr, 5>& strings) {
  bool valid = true;
  for (int index = 0; index < strings.num; ++index) {
    if (!idMaterial::IsValidMaterialName(strings[index].data, true, true)) {
      idLib::Warning("Model '%s' has invalid materialname '%s'", modelName,
                     strings[index].data);
      valid = false;
    }
  }
  return valid;
}

template <typename Vector, int Dimension>
void BuildRemapTableImpl(idList<int, 5>& remap,
                         const idList<Vector, 5>& vectors, float epsilon) {
  remap.SetNum(vectors.num);
  if (epsilon == 0.0f) {
    for (int index = 0; index < vectors.num; ++index)
      remap[index] = index;
    return;
  }

  Vector minimums{};
  Vector maximums{};
  for (int component = 0; component < Dimension; ++component) {
    minimums[component] = 1.0e30f;
    maximums[component] = -1.0e30f;
  }
  for (int index = 0; index < vectors.num; ++index) {
    for (int component = 0; component < Dimension; ++component) {
      minimums[component] =
          std::min(minimums[component], vectors[index][component]);
      maximums[component] =
          std::max(maximums[component], vectors[index][component]);
    }
  }
  for (int component = 0; component < Dimension; ++component) {
    minimums[component] -= epsilon * 64.0f;
    maximums[component] += epsilon * 64.0f;
  }

  idVectorSubset<Vector, Dimension> subset;
  subset.Init(&minimums, &maximums, 32, vectors.num);
  for (int index = 0; index < vectors.num; ++index) {
    remap[index] = subset.FindVector(vectors.list, index, epsilon);
  }
}

void BuildRemapTable(idList<int, 5>& remap,
                     const idList<idVec3, 5>& vectors, float epsilon) {
  BuildRemapTableImpl<idVec3, 3>(remap, vectors, epsilon);
}

void BuildRemapTable(idList<int, 5>& remap,
                     const idList<idVec2, 5>& vectors, float epsilon) {
  BuildRemapTableImpl<idVec2, 2>(remap, vectors, epsilon);
}

const idMaterial* FindMaterial(const char* name) {
  return FindMaterialOrAutoMaterial(name);
}

bool MaterialForcesNormalSmoothing(const idMaterial* material) {
  return material != nullptr && material->MaterialForcesNormalSmoothing();
}

void AddLwoMapData(const st_lwVMapList* maps,
                   const idList<int, 5>& texCoordRemap,
                   const idList<idVec3, 5>& normalList,
                   const idList<idVec3, 5>& morphList, int& texCoord,
                   idVec3& normal, int& morph, std::uint8_t color[4]) {
  if (maps == nullptr)
    return;
  for (int mapIndex = 0; mapIndex < maps->nvmaps; ++mapIndex) {
    const st_lwVMapPt& mapPoint = maps->vm[mapIndex];
    const st_lwVMap* map = mapPoint.vmap;
    if (map == nullptr)
      continue;
    const int valueIndex = map->offset + mapPoint.index;
    switch (map->type) {
      case kLwoTexCoord:
        if (valueIndex >= 0 && valueIndex < texCoordRemap.num)
          texCoord = texCoordRemap[valueIndex];
        break;
      case kLwoNormal:
        if (valueIndex >= 0 && valueIndex < normalList.num)
          normal = normalList[valueIndex];
        break;
      case kLwoMorph:
        if (map->offset <= 1 && valueIndex >= 0 && valueIndex < morphList.num)
          morph = valueIndex;
        break;
      case kLwoColor:
        if (mapPoint.index >= 0 && mapPoint.index < map->nverts &&
            map->val != nullptr && map->val[mapPoint.index] != nullptr) {
          const float* rgba = map->val[mapPoint.index];
          color[0] = ClampByte(rgba[0] * 255.0f);
          color[1] = ClampByte(rgba[1] * 255.0f);
          color[2] = ClampByte(rgba[2] * 255.0f);
          color[3] = ClampByte(rgba[3] * 255.0f);
        }
        break;
      default:
        break;
    }
  }
}

} // namespace

void idStaticModel::BuildAutospriteData() {
  if (surfaces.num == 0 || surfaces[0].material == nullptr)
    return;

  const autospriteType_t type = surfaces[0].material->GetAutoSpriteType();
  if (type == AUTOSPRITE_NONE)
    return;
  for (int index = 1; index < surfaces.num; ++index) {
    if (surfaces[index].material == nullptr ||
        surfaces[index].material->GetAutoSpriteType() != type) {
      idLib::Warning("Not all surfaces in %s use an autosprite material",
                     name.str);
      return;
    }
  }

  modelIsAutosprite = true;
  referenceBounds.Clear();
  constexpr int edgeVerts[6][2] = {
      {0, 1}, {1, 2}, {2, 0}, {3, 4}, {4, 5}, {5, 3}};

  for (int surfaceIndex = 0; surfaceIndex < surfaces.num; ++surfaceIndex) {
    idTriangles* geometry = surfaces[surfaceIndex].geometry;
    if (geometry == nullptr || surfaces[surfaceIndex].material == nullptr)
      continue;
    if ((geometry->numVerts & 3) != 0) {
      idLib::Warning("Autosprite %s has odd vertex count", name.str);
      continue;
    }
    if (geometry->numIndexes != 6 * (geometry->numVerts / 4)) {
      idLib::Warning("Autosprite %s has odd index count", name.str);
      continue;
    }

    geometry->CreateBounds();
    float maximumRadius = 0.0f;
    if (type == AUTOSPRITE_VIEW_ORIENTED) {
      for (int first = 0; first < geometry->numVerts; first += 4) {
        idVec3 center{};
        for (int corner = 0; corner < 4; ++corner)
          center += geometry->verts[first + corner].xyz;
        center *= 0.25f;
        float radius =
            (geometry->verts[first].xyz - center).Length() / std::sqrt(2.0f);
        if (radius > 255.0f) {
          idLib::Warning("Autosprite 1 %s max radius exceeded", name.str);
          radius = 255.0f;
        }
        maximumRadius = std::max(maximumRadius, radius);
        const std::uint8_t radiusByte = ClampByte(radius);
        static constexpr std::uint8_t cornerBytes[4][2] = {
            {255, 255}, {0, 255}, {0, 0}, {255, 0}};
        static constexpr float texCoords[4][2] = {
            {0.0f, 0.0f}, {1.0f, 0.0f}, {1.0f, 1.0f}, {0.0f, 1.0f}};
        for (int corner = 0; corner < 4; ++corner) {
          idDrawVert& vertex = geometry->verts[first + corner];
          vertex.xyz = center;
          vertex.st = {texCoords[corner][0], texCoords[corner][1]};
          vertex.tangent[0] = radiusByte;
          vertex.tangent[1] = cornerBytes[corner][0];
          vertex.tangent[2] = cornerBytes[corner][1];
          vertex.tangent[3] = 0;
        }
        const int firstIndex = 6 * (first / 4);
        const std::uint16_t quadIndexes[6] = {
            static_cast<std::uint16_t>(first),
            static_cast<std::uint16_t>(first + 1),
            static_cast<std::uint16_t>(first + 2),
            static_cast<std::uint16_t>(first),
            static_cast<std::uint16_t>(first + 2),
            static_cast<std::uint16_t>(first + 3)};
        std::memcpy(geometry->indexes + firstIndex, quadIndexes,
                    sizeof(quadIndexes));
      }
    } else if (type == AUTOSPRITE_LONGEST_AXIS_ALIGNED ||
               type == AUTOSPRITE_X_AXIS_ALIGNED ||
               type == AUTOSPRITE_Y_AXIS_ALIGNED ||
               type == AUTOSPRITE_Z_AXIS_ALIGNED) {
      for (int first = 0; first < geometry->numVerts; first += 4) {
        const int firstIndex = 6 * (first / 4);
        idVec3 lockAxis{0.0f, 0.0f, 1.0f};
        if (type == AUTOSPRITE_X_AXIS_ALIGNED) {
          lockAxis = {1.0f, 0.0f, 0.0f};
        } else if (type == AUTOSPRITE_Y_AXIS_ALIGNED) {
          lockAxis = {0.0f, 1.0f, 0.0f};
        } else if (type == AUTOSPRITE_LONGEST_AXIS_ALIGNED) {
          int shortest = 0;
          float shortestLength = std::numeric_limits<float>::max();
          for (int edge = 0; edge < 6; ++edge) {
            const idVec3& start = geometry->verts[
                geometry->indexes[firstIndex + edgeVerts[edge][0]]].xyz;
            const idVec3& end = geometry->verts[
                geometry->indexes[firstIndex + edgeVerts[edge][1]]].xyz;
            const float length = (start - end).LengthSqr();
            if (length < shortestLength) {
              shortestLength = length;
              shortest = edge;
            }
          }
          const int otherEdge = shortest == 5 ? 0 : 5;
          const idVec3 firstMid =
              (geometry->verts[geometry->indexes[
                   firstIndex + edgeVerts[shortest][0]]].xyz +
               geometry->verts[geometry->indexes[
                   firstIndex + edgeVerts[shortest][1]]].xyz) *
              0.5f;
          const idVec3 secondMid =
              (geometry->verts[geometry->indexes[
                   firstIndex + edgeVerts[otherEdge][0]]].xyz +
               geometry->verts[geometry->indexes[
                   firstIndex + edgeVerts[otherEdge][1]]].xyz) *
              0.5f;
          lockAxis = NormalizeSafe(secondMid - firstMid);
        }

        idBounds quadBounds;
        quadBounds.Clear();
        for (int corner = 0; corner < 4; ++corner)
          quadBounds.AddPoint(geometry->verts[first + corner].xyz);
        const idVec3 center = (quadBounds[0] + quadBounds[1]) * 0.5f;
        const idVec3& p0 = geometry->verts[geometry->indexes[firstIndex]].xyz;
        const idVec3& p1 = geometry->verts[geometry->indexes[firstIndex + 1]].xyz;
        const idVec3& p2 = geometry->verts[geometry->indexes[firstIndex + 2]].xyz;
        const idVec3 planeNormal = NormalizeSafe((p0 - p1).Cross(p2 - p1));
        const float normalXYLength =
            std::sqrt(planeNormal.x * planeNormal.x +
                      planeNormal.y * planeNormal.y);
        const idVec3 side = normalXYLength == 0.0f
                                ? idVec3{1.0f, 0.0f, 0.0f}
                                : idVec3{-planeNormal.y / normalXYLength,
                                         planeNormal.x / normalXYLength, 0.0f};

        for (int corner = 0; corner < 4; ++corner) {
          idDrawVert& vertex = geometry->verts[first + corner];
          const float rayPosition =
              idGeometry::ClosestPositionOnRay(vertex.xyz, center, lockAxis);
          const idVec3 projected = center + lockAxis * rayPosition;
          const idVec3 radial = vertex.xyz - projected;
          const float radius = radial.Length();
          const idVec3 radialDirection = NormalizeSafe(radial);
          vertex.xyz = projected;
          vertex.normal[0] = EncodeDirection(lockAxis.x);
          vertex.normal[1] = EncodeDirection(lockAxis.y);
          vertex.normal[2] = EncodeDirection(lockAxis.z);
          vertex.tangent[0] = ClampByte(radius);
          vertex.tangent[1] =
              ClampByte((radius - std::floor(radius)) * 256.0f);
          vertex.tangent[2] = 0;
          vertex.tangent[3] = radialDirection * side < 0.0f ? 0 : 255;
        }
        maximumRadius = std::max(maximumRadius, quadBounds.GetRadius());
      }
    }

    geometry->bounds[0].x -= maximumRadius;
    geometry->bounds[0].y -= maximumRadius;
    geometry->bounds[0].z -= maximumRadius;
    geometry->bounds[1].x += maximumRadius;
    geometry->bounds[1].y += maximumRadius;
    geometry->bounds[1].z += maximumRadius;
    referenceBounds.AddBounds(geometry->bounds);
  }

  for (int index = 0; index < surfaces.num; ++index) {
    if (surfaces[index].geometry != nullptr) {
      surfaces[index].geometry->UpdateVertexBuffer();
      surfaces[index].geometry->UpdateIndexBuffer();
    }
  }
}

void idStaticModel::ConvertToTriStrips() {
  if (!r_staticModelTriStrips.GetBool())
    return;
  for (int index = 0; index < surfaces.num; ++index) {
    idStaticModelSurface& surface = surfaces[index];
    if (surface.material == nullptr || surface.geometry == nullptr ||
        surface.material->MaterialIsTransparencySorted())
      continue;
    idList<std::uint16_t, 5> stripIndexes;
    TriangleStripper(surface.geometry->indexes,
                     static_cast<unsigned int>(surface.geometry->numIndexes),
                     &stripIndexes);
    if (stripIndexes.num < surface.geometry->numIndexes) {
      std::memmove(surface.geometry->indexes, stripIndexes.list,
                   sizeof(std::uint16_t) * stripIndexes.num);
      surface.geometry->numIndexes = stripIndexes.num;
      surface.geometry->vertexMask |= 0x200u;
    }
  }
}

void idStaticModel::WriteStaticBModel(
    const char* fileName, std::uint32_t sourceTimestamp,
    const idList<idStaticModelSurface, 5>& modelSurfaces,
    const idList<sourceSurface_t, 5>& modelSourceSurfaces,
    const idVec3& modelReferencePosition, fsPath_t basePath) {
  idStr outputName(fileName);
  idStr extension;
  outputName.ExtractFileExtension(extension);
  if (extension.Icmp("pmodel") != 0 && extension.Icmp("cmodel") != 0)
    outputName.SetFileExtension("bmodel");

  idLib::Printf("Writing %s\n", outputName.data);
  idFileLocal file(fileSystem->OpenFileWrite(outputName.data, basePath));
  if (file.file == nullptr) {
    idLib::Warning("idStaticModel::WriteStaticBModel() - Could not open %s",
                   fileName);
    return;
  }

  file->Write(&kStaticModelDiskMagic, sizeof(kStaticModelDiskMagic));
  WriteBig32(file.file, sourceTimestamp);
  WriteBig32(file.file, static_cast<std::uint32_t>(modelSurfaces.num));
  for (int index = 0; index < modelSurfaces.num; ++index) {
    idStr materialName;
    if (modelSurfaces[index].material != nullptr)
      materialName = modelSurfaces[index].material->NameWithoutAutoMaterial();
    const int length = materialName.Length();
    file->Write(&length, sizeof(length));
    file->Write(materialName.data, static_cast<std::size_t>(length));
  }
  WriteBigVec3(file.file, modelReferencePosition);
  WriteSourceSurfaces(file.file, modelSourceSurfaces);
  WriteBig32(file.file, static_cast<std::uint32_t>(modelSurfaces.num));
  for (int index = 0; index < modelSurfaces.num; ++index) {
    const idStaticModelSurface& surface = modelSurfaces[index];
    if (surface.material == nullptr || surface.geometry == nullptr)
      continue;
    WriteBig32(file.file, static_cast<std::uint32_t>(surface.materialNum));
    WriteBig32(file.file, static_cast<std::uint32_t>(surface.binaryModelId));
    surface.geometry->WriteToFile(file.file);
    file->Write(&kStaticModelDiskMagic, sizeof(kStaticModelDiskMagic));
  }
}

void idStaticModel::WriteStaticBModel(const char* fileName,
                                      fsPath_t basePath) const {
  WriteStaticBModel(
      fileName, 0,
      reinterpret_cast<const idList<idStaticModelSurface, 5>&>(surfaces),
      sourceSurfaces, referencePosition, basePath);
}

void idStaticModel::MergeSurfacesAndBuildSourceSurfaces() {
  idStr skinName(name.str);
  skinName.StripFileExtension();
  const bool hasSkin =
      idDeclSkins::resourceList.FindWithInheritance(skinName.data, false,
                                                    true) != nullptr;
  sourceSurfaces.ClearFree();
  for (int surfaceIndex = 0; surfaceIndex < surfaces.num; ++surfaceIndex) {
    idStaticModelSurface& surface = surfaces[surfaceIndex];
    if (surface.material == nullptr || surface.geometry == nullptr)
      continue;
    sourceSurface_t source{};
    source.mtr = surface.material;
    source.mtrChecksum = surface.material->md5Checksum;
    source.renderSurface = surfaceIndex;
    source.firstVertex = 0;
    source.lastVertex = surface.geometry->numVerts - 1;

    if (hasSkin && surface.geometry->cpuVertexMask == 31 &&
        surface.geometry->verts != nullptr) {
      const idMaterial* compatible = surface.material->CompatibleGenericMaterial();
      if (compatible != nullptr)
        surface.material = compatible;
      for (int previous = 0; previous < surfaceIndex; ++previous) {
        idStaticModelSurface& destination = surfaces[previous];
        if (destination.material != surface.material ||
            destination.geometry == nullptr ||
            destination.geometry->cpuVertexMask != 31 ||
            destination.geometry->verts == nullptr)
          continue;
        source.renderSurface = previous;
        source.firstVertex = destination.geometry->numVerts;
        source.lastVertex =
            source.firstVertex + surface.geometry->numVerts - 1;
        destination.geometry->MergeTriangles(surface.geometry);
        surfaces.RemoveIndex(surfaceIndex--);
        break;
      }
    }
    sourceSurfaces.Append(source);
  }
}

void idStaticModel::BuildMergeList(
    const idList<idStr, 5>& materialNames,
    idList<const idMaterial*, 5>& sourceMaterials,
    idList<int, 5>& mergeTo,
    idList<const idMaterial*, 5>& uniqueMaterials,
    idList<int, 5>& uniqueMaterialNums) {
  sourceMaterials.SetNum(materialNames.num);
  mergeTo.SetNum(materialNames.num);
  uniqueMaterials.SetNum(materialNames.num);
  uniqueMaterialNums.SetNum(materialNames.num);
  if (materialNames.num == 0)
    return;

  idList<idSortedMaterial, 5> sortedMaterials;
  sortedMaterials.SetNum(materialNames.num);
  bool hasMaterialNumbers = false;
  for (int index = 0; index < materialNames.num; ++index) {
    idStr materialName(materialNames[index]);
    int materialNumber = -1;
    const int ampersand = materialName.Find('&');
    if (ampersand != -1 && idStr::IsNumeric(materialName.data + ampersand + 1)) {
      materialNumber = std::atoi(materialName.data + ampersand + 1);
      materialName.CapLength(ampersand);
      hasMaterialNumbers = true;
    }
    const idMaterial* material = FindMaterial(materialName.data);
    sourceMaterials[index] = material;
    uniqueMaterials[index] = material;
    uniqueMaterialNums[index] = materialNumber;
    mergeTo[index] = index;
    sortedMaterials[index] =
        {materialNames[index].data, material, materialNumber, index};
  }

  if (hasMaterialNumbers) {
    idSort_SortedMaterial sorter;
    sorter.Sort(sortedMaterials.list,
                static_cast<unsigned int>(sortedMaterials.num));
    for (int index = 0; index < sortedMaterials.num; ++index) {
      mergeTo[sortedMaterials[index].originalIndex] = index;
      uniqueMaterials[index] = sortedMaterials[index].material;
      uniqueMaterialNums[index] = sortedMaterials[index].materialNumber;
    }
    return;
  }

  if (!r_mergeModelSurfaces.GetBool())
    return;
  const idDeclRenderParm* discrete =
      static_cast<const idDeclRenderParm*>(
          idDeclRenderParm::resourceList.FindWithInheritance("discrete", true,
                                                             true));
  uniqueMaterials.Clear();
  for (int index = 0; index < sourceMaterials.num; ++index) {
    const idMaterial* material = sourceMaterials[index];
    if (material == nullptr)
      continue;
    const bool forceDiscrete =
        discrete != nullptr && material->GetInteger(discrete) != 0;
    int uniqueIndex = 0;
    if (!forceDiscrete) {
      while (uniqueIndex < uniqueMaterials.num &&
             uniqueMaterials[uniqueIndex] != material)
        ++uniqueIndex;
    } else {
      uniqueIndex = uniqueMaterials.num;
    }
    if (uniqueIndex == uniqueMaterials.num) {
      uniqueMaterials.Append(material);
    }
    mergeTo[index] = uniqueIndex;
  }
}

bool idStaticModel::ConvertASEToModelSurfaces(const idASEModel* ase) {
  if (ase == nullptr || ase->objects.num < 1)
    return false;

  idList<idStr, 5> materialNames;
  if (ase->materials.num > 0) {
    materialNames.SetNum(ase->objects.num);
    for (int index = 0; index < ase->objects.num; ++index) {
      const idASEObject* object = ase->objects[index];
      if (object == nullptr || object->materialRef < 0 ||
          object->materialRef >= ase->materials.num ||
          ase->materials[object->materialRef] == nullptr)
        materialNames[index] = "_default";
      else
        materialNames[index] = ase->materials[object->materialRef]->name;
    }
  }
  if (!ValidateMaterialNames(name.str, materialNames))
    return false;

  idList<const idMaterial*, 5> sourceMaterials;
  idList<int, 5> mergeTo;
  idList<const idMaterial*, 5> mergedMaterials;
  idList<int, 5> mergedMaterialNums;
  BuildMergeList(materialNames, sourceMaterials, mergeTo, mergedMaterials,
                 mergedMaterialNums);
  if (mergedMaterials.num == 0)
    return false;

  idList<idRawSurface, 5> rawSurfaces;
  rawSurfaces.SetNum(mergedMaterials.num);
  for (int index = 0; index < rawSurfaces.num; ++index) {
    rawSurfaces[index].SetMaterial(mergedMaterials[index]);
    rawSurfaces[index].SetMaterialNum(mergedMaterialNums[index]);
  }

  idList<idVec3, 5> vertexList;
  idList<idVec2, 5> transformedTexCoords;
  idList<idVec3, 5> morphList;
  std::vector<int> vertexOffsets(static_cast<std::size_t>(ase->objects.num));
  std::vector<int> texCoordOffsets(
      static_cast<std::size_t>(ase->objects.num));
  for (int objectIndex = 0; objectIndex < ase->objects.num; ++objectIndex) {
    const idASEObject* object = ase->objects[objectIndex];
    vertexOffsets[static_cast<std::size_t>(objectIndex)] = vertexList.num;
    texCoordOffsets[static_cast<std::size_t>(objectIndex)] =
        transformedTexCoords.num;
    if (object == nullptr)
      continue;
    const idASEMesh& mesh = object->mesh;
    for (int index = 0; index < mesh.vertexes.num; ++index)
      vertexList.Append(mesh.vertexes[index]);

    float uOffset = 0.0f;
    float vOffset = 0.0f;
    float uTiling = 1.0f;
    float vTiling = 1.0f;
    float sine = 0.0f;
    float cosine = 1.0f;
    if (ase->materials.num > 0 && object->materialRef >= 0 &&
        object->materialRef < ase->materials.num &&
        ase->materials[object->materialRef] != nullptr) {
      const idASEMaterial& material = *ase->materials[object->materialRef];
      uOffset = -material.uOffset;
      vOffset = material.vOffset;
      uTiling = material.uTiling;
      vTiling = material.vTiling;
      sine = std::sin(material.angle);
      cosine = std::cos(material.angle);
    }
    for (int index = 0; index < mesh.tvertexes.num; ++index) {
      const float u = uTiling * mesh.tvertexes[index].x + uOffset;
      const float v = vTiling * mesh.tvertexes[index].y + vOffset;
      transformedTexCoords.Append(
          {u * cosine + v * sine, v * cosine - u * sine});
    }
  }

  idList<int, 5> vertexRemap;
  idList<int, 5> texCoordRemap;
  BuildRemapTable(vertexRemap, vertexList,
                  pristine ? 0.0f : r_slopVertex.GetFloat());
  BuildRemapTable(texCoordRemap, transformedTexCoords,
                  pristine ? 0.0f : r_slopTexCoord.GetFloat());
  for (int index = 0; index < rawSurfaces.num; ++index)
    rawSurfaces[index].InitHash(vertexList, transformedTexCoords, morphList);

  for (int objectIndex = 0; objectIndex < ase->objects.num; ++objectIndex) {
    const idASEObject* object = ase->objects[objectIndex];
    if (object == nullptr)
      continue;
    const idASEMesh& mesh = object->mesh;
    bool hasNormals = mesh.normalsParsed;
    if (objectIndex < sourceMaterials.num &&
        MaterialForcesNormalSmoothing(sourceMaterials[objectIndex]))
      hasNormals = false;

    idRawSurface& raw = rawSurfaces[mergeTo[objectIndex]];
    raw.SetGenerateNormalsFlag(!hasNormals);
    raw.SetNormalEpsilon(1.0f - r_slopNormal.GetFloat());
    for (int faceIndex = 0; faceIndex < mesh.numFaces; ++faceIndex) {
      const aseFace_t& face = mesh.faces[faceIndex];
      for (int corner = 0; corner < 3; ++corner) {
        const int vertexIndex = face.vertexNum[corner];
        if (vertexIndex < 0 || vertexIndex >= mesh.vertexes.num) {
          idLib::Error(
              "ConvertASEToModelSurfaces: bad vertex index in ASE file %s",
              name.str);
          return false;
        }
        int texCoord = 0;
        if (mesh.numTVFaces == mesh.numFaces && mesh.tvertexes.num != 0) {
          const int sourceTexCoord = face.tVertexNum[corner];
          if (sourceTexCoord < 0 || sourceTexCoord >= mesh.tvertexes.num) {
            idLib::Error(
                "ConvertASEToModelSurfaces: bad tex coord index in ASE file %s",
                name.str);
            return false;
          }
          texCoord = texCoordRemap[
              texCoordOffsets[static_cast<std::size_t>(objectIndex)] +
              sourceTexCoord];
        }
        const idVec3 normal =
            hasNormals ? face.vertexNormals[corner] : idVec3{0.0f, 0.0f, 1.0f};
        std::uint32_t color = 0xffffffffu;
        if (mesh.colorsParsed) {
          const auto* bytes = reinterpret_cast<const std::uint8_t*>(
              face.vertexColors);
          std::memcpy(&color, bytes + corner * 4, sizeof(color));
        }
        raw.AddVert(
            vertexRemap[
                vertexOffsets[static_cast<std::size_t>(objectIndex)] +
                vertexIndex],
            texCoord, normal, color, 0);
      }
    }
  }
  EmitRawSurfacesToStaticModel(rawSurfaces, *this);
  return true;
}

bool idStaticModel::ConvertLWOToModelSurfaces(
    const st_lwObject* object, const st_lwLayer* layer, float vertexEpsilon,
    float texCoordEpsilon, float normalEpsilon) {
  if (object == nullptr || layer == nullptr)
    return false;

  idList<idVec3, 5> vertexList;
  vertexList.SetNum(layer->point.count);
  for (int index = 0; index < layer->point.count; ++index) {
    vertexList[index] = {layer->point.pt[index].pos[0] - layer->pivot[0],
                         layer->point.pt[index].pos[2] - layer->pivot[2],
                         layer->point.pt[index].pos[1] - layer->pivot[1]};
  }
  idList<idVec3, 5> morphList;
  morphList.Append(vec3_origin);
  idList<idVec2, 5> texCoordList;
  idList<idVec3, 5> normalList;
  for (st_lwVMap* map = layer->vmap; map != nullptr; map = map->next) {
    if (map->type == kLwoMorph) {
      map->offset = morphList.num;
      for (int index = 0; index < map->nverts; ++index) {
        const float* value = map->val[index];
        morphList.Append({value[0], value[1], value[2]});
      }
    } else if (map->type == kLwoTexCoord) {
      map->offset = texCoordList.num;
      for (int index = 0; index < map->nverts; ++index) {
        const float* value = map->val[index];
        texCoordList.Append({value[0], 1.0f - value[1]});
      }
    } else if (map->type == kLwoNormal) {
      map->offset = normalList.num;
      for (int index = 0; index < map->nverts; ++index) {
        const float* value = map->val[index];
        normalList.Append({value[0], value[2], value[1]});
      }
    }
  }
  if (texCoordList.num == 0)
    idLib::Warning(
        "ConvertLWOToModelSurfaces: model '%s' has bad or missing uv data",
        name.str);

  idList<int, 5> vertexRemap;
  idList<int, 5> texCoordRemap;
  BuildRemapTable(vertexRemap, vertexList, vertexEpsilon);
  BuildRemapTable(texCoordRemap, texCoordList, texCoordEpsilon);

  idList<idStr, 5> materialNames;
  std::vector<const st_lwSurface*> lwoSurfaces;
  for (const st_lwSurface* surface = object->surf; surface != nullptr;
       surface = surface->next) {
    materialNames.Append(surface->name != nullptr ? surface->name : "");
    lwoSurfaces.push_back(surface);
  }
  if (!ValidateMaterialNames(name.str, materialNames)) {
    idLib::Warning("Model '%s' contains surfaces with invalid names", name.str);
    return false;
  }

  idList<const idMaterial*, 5> sourceMaterials;
  idList<int, 5> mergeTo;
  idList<const idMaterial*, 5> mergedMaterials;
  idList<int, 5> mergedMaterialNums;
  BuildMergeList(materialNames, sourceMaterials, mergeTo, mergedMaterials,
                 mergedMaterialNums);

  idList<int, 5> polygonCounts;
  polygonCounts.SetNum(mergedMaterials.num, 0);
  for (int materialIndex = 0; materialIndex < materialNames.num;
       ++materialIndex) {
    for (int polygonIndex = 0; polygonIndex < layer->polygon.count;
         ++polygonIndex) {
      if (layer->polygon.pol[polygonIndex].surf ==
          lwoSurfaces[static_cast<std::size_t>(materialIndex)])
        ++polygonCounts[mergeTo[materialIndex]];
    }
  }

  idList<idRawSurface, 5> rawSurfaces;
  rawSurfaces.SetNum(mergedMaterials.num);
  for (int index = 0; index < rawSurfaces.num; ++index) {
    rawSurfaces[index].SetMaterial(mergedMaterials[index]);
    rawSurfaces[index].SetMaterialNum(mergedMaterialNums[index]);
    rawSurfaces[index].InitHash(vertexList, texCoordList, morphList);
    rawSurfaces[index].InitIndex(3 * polygonCounts[index]);
    const bool generateNormals =
        MaterialForcesNormalSmoothing(mergedMaterials[index]);
    rawSurfaces[index].SetGenerateNormalsFlag(generateNormals);
    if (!cmodel && normalList.num > 0 && generateNormals) {
      idLib::Printf(
          "Model '%s' contains vertex normals but material '%s' is set to "
          "autogenerate normals which will override the vertex normals\n",
          name.str, mergedMaterials[index] != nullptr
                        ? mergedMaterials[index]->name.str
                        : "<null>");
    }
    rawSurfaces[index].SetNormalEpsilon(1.0f - normalEpsilon);
  }
  if (rawSurfaces.num == 0) {
    idLib::Warning("Model '%s' is contains no surfaces", name.str);
    return false;
  }

  for (int materialIndex = 0; materialIndex < materialNames.num;
       ++materialIndex) {
    const st_lwSurface* lwoSurface =
        lwoSurfaces[static_cast<std::size_t>(materialIndex)];
    idRawSurface& raw = rawSurfaces[mergeTo[materialIndex]];
    for (int polygonIndex = 0; polygonIndex < layer->polygon.count;
         ++polygonIndex) {
      const st_lwPolygon& polygon = layer->polygon.pol[polygonIndex];
      if (polygon.surf != lwoSurface || polygon.nverts <= 2)
        continue;
      for (int fanVertex = 2; fanVertex < polygon.nverts; ++fanVertex) {
        const int corners[3] = {0, fanVertex - 1, fanVertex};
        for (int triangleCorner = 0; triangleCorner < 3; ++triangleCorner) {
          const int polygonVertex = polygon.vindex + corners[triangleCorner];
          const st_lwPolVert& sourceVertex =
              layer->polygon.polVerts[polygonVertex];
          if (sourceVertex.index < 0 || sourceVertex.index >= vertexList.num) {
            idLib::Error(
                "ConvertLWOToModelSurfaces: bad vertex index in file %s",
                name.str);
            return false;
          }
          int texCoord = 0;
          int morph = 0;
          idVec3 normal{sourceVertex.norm[0], sourceVertex.norm[2],
                        sourceVertex.norm[1]};
          std::uint8_t color[4] = {
              ClampByte(lwoSurface->color.rgb[0] * 255.0f),
              ClampByte(lwoSurface->color.rgb[1] * 255.0f),
              ClampByte(lwoSurface->color.rgb[2] * 255.0f), 255};
          if (layer->point.ptVMaps != nullptr) {
            AddLwoMapData(&layer->point.ptVMaps[sourceVertex.index],
                          texCoordRemap, normalList, morphList, texCoord,
                          normal, morph, color);
          }
          if (layer->polygon.polVertVMaps != nullptr) {
            AddLwoMapData(&layer->polygon.polVertVMaps[polygonVertex],
                          texCoordRemap, normalList, morphList, texCoord,
                          normal, morph, color);
          }
          FixDegenerateNormal(normal);
          std::uint32_t packedColor = 0;
          std::memcpy(&packedColor, color, sizeof(packedColor));
          raw.AddVert(vertexRemap[sourceVertex.index], texCoord, normal,
                      packedColor, morph);
        }
      }
    }
  }

  EmitRawSurfacesToStaticModel(rawSurfaces, *this);
  referencePosition = {layer->pivot[0], layer->pivot[2], layer->pivot[1]};
  return true;
}

bool idStaticModel::ConvertOBJToModelSurfaces(const idOBJModel* object) {
  if (object == nullptr || object->meshes.num < 1)
    return false;

  idMtlOptions options;
  options.loadedFile = name.str;
  std::map<idStr, const idMaterial*> materialMap;
  for (int index = 0; index < object->materials.num; ++index) {
    materialMap[object->materials[index].mtlName] =
        DeclMaterialForParsedMtl(object->materials[index], &options);
  }

  idList<idStr, 5> materialNames;
  materialNames.SetNum(object->meshes.num);
  for (int index = 0; index < object->meshes.num; ++index) {
    if (object->mrgb.num > 0) {
      materialNames[index] = "zbrush";
      continue;
    }
    const idMaterial* material = materialMap[object->meshes[index].materialName];
    materialNames[index] = material != nullptr ? material->name.str
                                                : "greyAutoMaterial";
  }
  if (!ValidateMaterialNames(name.str, materialNames))
    return false;

  idList<const idMaterial*, 5> sourceMaterials;
  idList<int, 5> mergeTo;
  idList<const idMaterial*, 5> mergedMaterials;
  idList<int, 5> mergedMaterialNums;
  BuildMergeList(materialNames, sourceMaterials, mergeTo, mergedMaterials,
                 mergedMaterialNums);
  idList<int, 5> vertexRemap;
  idList<int, 5> texCoordRemap;
  BuildRemapTable(vertexRemap, object->v,
                  pristine ? 0.0f : r_slopVertex.GetFloat());
  BuildRemapTable(texCoordRemap, object->vt,
                  pristine ? 0.0f : r_slopTexCoord.GetFloat());
  idList<idVec3, 5> morphList;

  idList<idRawSurface, 5> rawSurfaces;
  rawSurfaces.SetNum(mergedMaterials.num);
  for (int index = 0; index < rawSurfaces.num; ++index) {
    rawSurfaces[index].SetMaterial(mergedMaterials[index]);
    rawSurfaces[index].SetMaterialNum(mergedMaterialNums[index]);
    rawSurfaces[index].InitHash(object->v, object->vt, morphList);
    rawSurfaces[index].SetGenerateNormalsFlag(false);
    rawSurfaces[index].SetNormalEpsilon(1.0f - r_slopNormal.GetFloat());
  }

  for (int meshIndex = 0; meshIndex < object->meshes.num; ++meshIndex) {
    const idOBJMesh& mesh = object->meshes[meshIndex];
    bool useFileNormals = mesh.hasNormals;
    if (meshIndex < sourceMaterials.num &&
        MaterialForcesNormalSmoothing(sourceMaterials[meshIndex]))
      useFileNormals = false;
    idList<idVec3, 5> averagedNormals;
    if (!useFileNormals) {
      idList<int, 5> indexes;
      for (int faceIndex = 0; faceIndex < mesh.f.num; ++faceIndex) {
        for (int corner = 0; corner < 3; ++corner)
          indexes.Append(mesh.f[faceIndex].v[corner] - 1);
      }
      if (!BuildAveragedSurfaceNormals(object->v, indexes, averagedNormals)) {
        idLib::Error("BuildAveragedSurfaceNormals failed in file %s",
                     name.str);
        return false;
      }
    }

    idRawSurface& raw = rawSurfaces[mergeTo[meshIndex]];
    for (int faceIndex = 0; faceIndex < mesh.f.num; ++faceIndex) {
      const idOBJFace& face = mesh.f[faceIndex];
      for (int corner = 0; corner < 3; ++corner) {
        const int sourceVertex = face.v[corner] - 1;
        if (sourceVertex < 0 || sourceVertex >= object->v.num) {
          idLib::Error(
              "ConvertOBJToModelSurfaces: bad vertex index in file %s",
              name.str);
          return false;
        }
        int texCoord = 0;
        if (mesh.hasTexCoords) {
          const int sourceTexCoord = face.vt[corner] - 1;
          if (sourceTexCoord < 0 || sourceTexCoord >= object->vt.num) {
            idLib::Error(
                "ConvertOBJToModelSurfaces: bad tex coord index in file %s",
                name.str);
            return false;
          }
          texCoord = texCoordRemap[sourceTexCoord];
        }
        idVec3 normal{};
        if (useFileNormals) {
          const int sourceNormal = face.vn[corner] - 1;
          if (sourceNormal < 0 || sourceNormal >= object->vn.num) {
            idLib::Error(
                "ConvertOBJToModelSurfaces: out of range normal in file %s",
                name.str);
            return false;
          }
          normal = object->vn[sourceNormal];
        } else {
          normal = averagedNormals[sourceVertex];
        }
        std::uint32_t color = 0xffffffffu;
        if (object->mrgb.num > sourceVertex) {
          const mrgb_t& sourceColor = object->mrgb[sourceVertex];
          const std::uint8_t reordered[4] = {
              sourceColor.mrgb[1], sourceColor.mrgb[2], sourceColor.mrgb[3],
              sourceColor.mrgb[0]};
          std::memcpy(&color, reordered, sizeof(color));
        }
        raw.AddVert(vertexRemap[sourceVertex], texCoord, normal, color, 0);
      }
    }
  }
  EmitRawSurfacesToStaticModel(rawSurfaces, *this);
  return true;
}

bool idStaticModel::GenerateStaticModel(const char* sourceFile) {
  idStr modelName(sourceFile);
  idStr extension;
  modelName.ExtractFileExtension(extension);
  modelName.StripFileExtension();
  SetName(modelName.data);
  const bool requestedPristine = pristine;
  sourceTimeStamp = fileSystem->GetTimestamp(sourceFile, false);

  bool converted = false;
  if (extension.Icmp("ase") == 0) {
    idASEModel* ase = ASE_Load(sourceFile);
    if (ase == nullptr)
      return false;
    int faceCount = 0;
    for (int index = 0; index < ase->objects.num; ++index) {
      if (ase->objects[index] != nullptr)
        faceCount += ase->objects[index]->mesh.numFaces;
    }
    if (faceCount >= 100000 && !cmodel)
      pristine = true;
    converted = ConvertASEToModelSurfaces(ase);
    ASE_Free(ase);
  } else if (extension.Icmp("lwo") == 0) {
    unsigned int failID = 0;
    int failPosition = 0;
    st_lwObject* lwo = lwGetObject(sourceFile, 0.0f, &failID, &failPosition);
    if (lwo == nullptr)
      return false;
    if (lwo->layer != nullptr && lwo->layer->polygon.count >= 100000 &&
        !cmodel)
      pristine = true;
    converted = ConvertLWOToModelSurfaces(
        lwo, lwo->layer, pristine ? 0.0f : r_slopVertex.GetFloat(),
        pristine ? 0.0f : r_slopTexCoord.GetFloat(),
        pristine ? 0.0f : r_slopNormal.GetFloat());
    lwFreeObject(lwo);
  } else if (extension.Icmp("obj") == 0) {
    idOBJModel* obj = OBJ_Load(sourceFile);
    if (obj == nullptr)
      return false;
    int faceCount = 0;
    for (int index = 0; index < obj->meshes.num; ++index)
      faceCount += obj->meshes[index].f.num;
    if (faceCount >= 100000 && !cmodel)
      pristine = true;
    converted = ConvertOBJToModelSurfaces(obj);
    delete obj;
  } else {
    SetResourceError("Unknown model type for StaticModel '%s'", sourceFile);
    return false;
  }

  if (!converted)
    return false;
  for (int index = 0; index < surfaces.num; ++index) {
    idTriangles* geometry = surfaces[index].geometry;
    if (geometry == nullptr)
      continue;
    idTriangles::RangeCheckIndexes(geometry->indexes, geometry->numIndexes,
                                   geometry->numVerts);
    geometry->RemoveDegenerate();
    if (!pristine && !cmodel) {
      geometry->OptimizeIndexOrder();
      geometry->OptimizeVertexOrder();
    }
  }
  FinishStaticModel(false);
  BuildAutospriteData();
  if (pristine) {
    if (!requestedPristine) {
      idLib::Warning(
          "Model '%s' is loaded in pristine mode, it will look flaky if used "
          "in the game",
          sourceFile);
    }
  } else {
    MergeSurfacesAndBuildSourceSurfaces();
    if (!cmodel)
      ConvertToTriStrips();
  }

  if (discardReferencePosition && !(referencePosition == vec3_origin)) {
    idLib::Printf(
        "Moving vertices for model '%s' using referencePosition (%s)\n",
        sourceFile, referencePosition.ToString(2));
    for (int surfaceIndex = 0; surfaceIndex < surfaces.num; ++surfaceIndex) {
      idTriangles* geometry = surfaces[surfaceIndex].geometry;
      if (geometry == nullptr)
        continue;
      for (int vertexIndex = 0; vertexIndex < geometry->numVerts;
           ++vertexIndex)
        geometry->verts[vertexIndex].xyz += referencePosition;
    }
    referencePosition = vec3_origin;
  }
  return true;
}

bool idStaticModel::GenerateBinaryModelFile(const char* sourceFile,
                                            const char* /*binaryFile*/) {
  idStaticModel generator;
  generator.pristine = pristine;
  generator.cmodel = cmodel;
  generator.discardReferencePosition = discardReferencePosition;
  if (!generator.GenerateStaticModel(sourceFile))
    return false;
  WriteStaticBModel(binaryFileName, generator.sourceTimeStamp,
                    reinterpret_cast<const idList<idStaticModelSurface, 5>&>(
                        generator.surfaces),
                    generator.sourceSurfaces,
                    generator.referencePosition, FSPATH_BASE);
  return true;
}
