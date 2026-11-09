#include "engine/models/static/staticmodelrawsurface.h"

#include "shared/idlib/containers/heaparray.h"
#include "shared/idlib/geometry/geometry.h"
#include "shared/idlib/lib.h"
#include "shared/idlib/sys/sys_alloc.h"

#include <algorithm>
#include <cmath>
#include <cstring>
#include <limits>
#include <new>

namespace {

struct tangentVert_t {
  bool polarityUsed[2]{};
  int negativeRemap{};
};

struct morphDrawVert_t {
  idVec3 xyz;
  idVec3 xyz1;
  idVec2 st;
  std::uint8_t normal[4];
  std::uint8_t tangent[4];
  std::uint8_t biTangent[4];
};

static_assert(sizeof(tangentVert_t) == 8);
static_assert(sizeof(morphDrawVert_t) == 44);

idVec3 NormalizeSafe(const idVec3& value) {
  const float inverseLength =
      1.0f / std::sqrt(std::max(value.LengthSqr(),
                                std::numeric_limits<float>::min()));
  return value * inverseLength;
}

std::uint8_t EncodeDirection(float value) {
  const int encoded =
      static_cast<int>((value + 1.0f) * 127.5f + 0.5f);
  return static_cast<std::uint8_t>(std::clamp(encoded, 0, 255));
}

void EncodeDirection(const idVec3& value, std::uint8_t output[4]) {
  output[0] = EncodeDirection(value.x);
  output[1] = EncodeDirection(value.y);
  output[2] = EncodeDirection(value.z);
}

const char* MaterialName(const idMaterial* material) {
  if (material == nullptr)
    return "<null>";
  return reinterpret_cast<const idResource*>(material)->name.str;
}

} // namespace

template <typename T>
void* idListArrayNew(int count, memTag_t tag, bool zeroBuffer) {
  if (count <= 0)
    return nullptr;
  idMem* memory = idMem::Singleton();
  T* result = static_cast<T*>(memory->AllocWithLocation(
      zeroBuffer ? "w:\\build\\tech5\\shared\\idlib\\containers\\List.h(27) : tag"
                 : "w:\\build\\tech5\\shared\\idlib\\containers\\List.h(29) : tag",
      sizeof(T) * static_cast<std::size_t>(count), tag, zeroBuffer, ALIGN_16,
      HEAP_DEFAULTHEAP));
  if (result == nullptr)
    return nullptr;
  int constructed = 0;
  try {
    for (; constructed < count; ++constructed)
      ::new (static_cast<void*>(result + constructed)) T;
  } catch (...) {
    while (constructed > 0)
      result[--constructed].~T();
    memory->Free(result, ALIGN_16);
    throw;
  }
  return result;
}

template <typename T>
void idListArrayDelete(void* allocation, int count) {
  T* pointer = static_cast<T*>(allocation);
  for (int index = 0; index < count; ++index)
    pointer[index].~T();
  idMem::Singleton()->Free(pointer, ALIGN_16);
}

template <typename T>
void* idListArrayResize(void* oldAllocation, int oldCount, int newCount,
                        memTag_t tag, bool zeroBuffer) {
  T* oldValues = static_cast<T*>(oldAllocation);
  T* replacement =
      static_cast<T*>(idListArrayNew<T>(newCount, tag, zeroBuffer));
  if (replacement != nullptr) {
    const int copyCount = std::min(oldCount, newCount);
    for (int index = 0; index < copyCount; ++index)
      replacement[index] = oldValues[index];
  }
  if (oldValues != nullptr)
    idListArrayDelete<T>(oldValues, oldCount);
  return replacement;
}

void idRawSurface::InitHash(const idList<idVec3, 5>& vList,
                            const idList<idVec2, 5>& tvList,
                            const idList<idVec3, 5>& morphList) {
  pvList = &vList;
  ptvList = &tvList;
  pMorphList = &morphList;
  vertHash.SetNum(vList.Num());
  for (int index = 0; index < vList.Num(); ++index)
    vertHash[index] = -1;
}

bool BuildAveragedSurfaceNormals(
    const idList<idVec3, 5>& sourceVerts, const idList<int, 5>& sourceIndexes,
    idList<idVec3, 5>& averagedSurfaceNormals) {
  averagedSurfaceNormals.SetNum(sourceVerts.Num());
  for (int index = 0; index < averagedSurfaceNormals.Num(); ++index)
    averagedSurfaceNormals[index] = {0.0f, 0.0f, 0.0f};

  for (int index = 0; index < sourceIndexes.Num(); index += 3) {
    for (int corner = 0; corner < 3; ++corner) {
      if (sourceIndexes[index + corner] < 0 ||
          sourceIndexes[index + corner] >= sourceVerts.Num())
        return false;
    }
    const idVec3 normal = idGeometry::TriangleNormal(
        sourceVerts[sourceIndexes[index]], sourceVerts[sourceIndexes[index + 1]],
        sourceVerts[sourceIndexes[index + 2]]);
    averagedSurfaceNormals[sourceIndexes[index]] += normal;
    averagedSurfaceNormals[sourceIndexes[index + 1]] += normal;
    averagedSurfaceNormals[sourceIndexes[index + 2]] += normal;
  }

  for (int index = 0; index < averagedSurfaceNormals.Num(); ++index)
    averagedSurfaceNormals[index] = NormalizeSafe(averagedSurfaceNormals[index]);
  return true;
}

void idRawSurface::GenerateNormals() {
  idList<int, 5> xyzIndexes;
  xyzIndexes.SetNum(indexes.Num());
  for (int index = 0; index < indexes.Num(); ++index)
    xyzIndexes[index] = verts[indexes[index]].v;

  idList<idVec3, 5> normals;
  if (!BuildAveragedSurfaceNormals(*pvList, xyzIndexes, normals))
    idLib::Error("BuildAveragedSurfaceNormals failed");
  for (int index = 0; index < verts.Num(); ++index)
    verts[index].normal = normals[verts[index].v];
}

void idRawSurface::InitIndex(int maxIndexes) {
  if (maxIndexes > indexes.size)
    indexes.ResizeForPort(maxIndexes);
  if (maxIndexes > verts.size)
    verts.ResizeForPort(maxIndexes);
}

void idRawSurface::DuplicateMirroredVertexes() {
  if (ptvList == nullptr || ptvList->Num() == 0)
    return;

  idHeapArray<tangentVert_t> tangentVerts;
  tangentVerts.Alloc(static_cast<unsigned int>(verts.Num()));
  std::memset(tangentVerts.buffer, 0,
              sizeof(tangentVert_t) * tangentVerts.num);

  idHeapArray<bool> faceNegativePolarity;
  faceNegativePolarity.Alloc(static_cast<unsigned int>(indexes.Num() / 3));
  for (unsigned int face = 0; face < faceNegativePolarity.num; ++face) {
    const int firstIndex = static_cast<int>(face) * 3;
    const idVec2 tc0 = (*ptvList)[verts[indexes[firstIndex]].tv];
    const idVec2 tc1 = (*ptvList)[verts[indexes[firstIndex + 1]].tv];
    const idVec2 tc2 = (*ptvList)[verts[indexes[firstIndex + 2]].tv];
    faceNegativePolarity.buffer[face] =
        (tc2.y - tc0.y) * (tc1.x - tc0.x) -
            (tc2.x - tc0.x) * (tc1.y - tc0.y) <
        0.0f;
  }

  for (int index = 0; index < indexes.Num(); ++index) {
    const bool polarity = faceNegativePolarity.buffer[index / 3];
    tangentVerts.buffer[indexes[index]].polarityUsed[polarity] = true;
  }

  const int originalVertexCount = verts.Num();
  for (int index = 0; index < originalVertexCount; ++index) {
    tangentVert_t& tangent = tangentVerts.buffer[index];
    if (tangent.polarityUsed[0] && tangent.polarityUsed[1]) {
      tangent.negativeRemap = verts.Num();
      const matchVert_t duplicate = verts[index];
      verts.Append(duplicate);
    }
  }

  for (int index = 0; index < indexes.Num(); ++index) {
    const int sourceIndex = indexes[index];
    if (tangentVerts.buffer[sourceIndex].negativeRemap != 0 &&
        faceNegativePolarity.buffer[index / 3]) {
      indexes[index] = tangentVerts.buffer[sourceIndex].negativeRemap;
    }
  }
}

void idRawSurface::AddVert(int vertexIndex, int texCoordIndex,
                           const idVec3& sourceNormal, unsigned int color,
                           int morphIndex) {
  int matchedIndex = vertHash[vertexIndex];
  for (int index = matchedIndex; index != -1; index = verts[index].next) {
    const matchVert_t& match = verts[index];
    if (match.v == vertexIndex && match.tv == texCoordIndex &&
        match.color == color && match.morph == morphIndex &&
        (generateNormals || normalEpsilon < sourceNormal * match.normal)) {
      matchedIndex = index;
      indexes.Append(matchedIndex);
      return;
    }
  }

  matchVert_t match{};
  match.next = vertHash[vertexIndex];
  match.v = vertexIndex;
  match.tv = texCoordIndex;
  match.morph = morphIndex;
  match.color = color;
  match.normal = sourceNormal;
  matchedIndex = verts.Append(match);
  vertHash[vertexIndex] = matchedIndex;
  indexes.Append(matchedIndex);
}

void DeriveVertexTangents(const idList<int, 5>& sourceIndexes,
                          const idList<idDrawVert, 5>& sourceVerts,
                          idList<vertexTangents_t, 5>& vertTangents) {
  vertTangents.SetNum(sourceVerts.Num());
  for (int index = 0; index < sourceVerts.Num(); ++index) {
    vertexTangents_t& tangent = vertTangents[index];
    tangent.normal = NormalizeSafe(
        {sourceVerts[index].normal[0] * (2.0f / 255.0f) - 1.0f,
         sourceVerts[index].normal[1] * (2.0f / 255.0f) - 1.0f,
         sourceVerts[index].normal[2] * (2.0f / 255.0f) - 1.0f});
    tangent.tangents[0] = {0.0f, 0.0f, 0.0f};
    tangent.tangents[1] = {0.0f, 0.0f, 0.0f};
  }

  for (int index = 0; index < sourceIndexes.Num(); index += 3) {
    idVec3 normal{};
    idVec3 tangents[2]{};
    idTriangles::TriangleVectors(
        &sourceVerts[sourceIndexes[index]],
        &sourceVerts[sourceIndexes[index + 1]],
        &sourceVerts[sourceIndexes[index + 2]], normal, tangents[0],
        tangents[1]);
    for (int corner = 0; corner < 3; ++corner) {
      vertexTangents_t& destination =
          vertTangents[sourceIndexes[index + corner]];
      destination.tangents[0] += tangents[0];
      destination.tangents[1] += tangents[1];
    }
  }

  for (int index = 0; index < vertTangents.Num(); ++index) {
    vertexTangents_t& tangent = vertTangents[index];
    tangent.normal = NormalizeSafe(tangent.normal);
    for (idVec3& direction : tangent.tangents) {
      direction -= tangent.normal * (tangent.normal * direction);
      direction = NormalizeSafe(direction);
    }
  }
}

void SplitTrianglesForVertexLimit(const idList<int, 5>& sourceIndexes,
                                  int maxVerts,
                                  idList<idSplitResults, 5>& split) {
  split.SetNum(sourceIndexes.Num() / maxVerts + 1);
  split.Clear();

  int maximumIndex = 0;
  for (int index = 0; index < sourceIndexes.Num(); ++index)
    maximumIndex = std::max(maximumIndex, sourceIndexes[index]);

  int sourceIndex = 0;
  while (sourceIndex < sourceIndexes.Num()) {
    idHeapArray<int> vertexRemap;
    vertexRemap.Alloc(static_cast<unsigned int>(maximumIndex + 1));
    std::fill(vertexRemap.buffer, vertexRemap.buffer + vertexRemap.num, -1);

    idSplitResults* result = split.Alloc();
    result->indexes.ClearFree();
    result->vertexMap.ClearFree();
    idList<int, 5> subIndexes;
    (void)subIndexes;

    while (sourceIndex < sourceIndexes.Num()) {
      if (result->vertexMap.Num() + 3 > maxVerts)
        break;
      for (int corner = 0; corner < 3; ++corner) {
        const int originalIndex = sourceIndexes[sourceIndex + corner];
        int& remappedIndex = vertexRemap.buffer[originalIndex];
        if (remappedIndex == -1) {
          remappedIndex = result->vertexMap.Num();
          result->vertexMap.Append(originalIndex);
        }
        result->indexes.Append(remappedIndex);
      }
      sourceIndex += 3;
    }
  }
}

void idRawSurface::MakeSurfaces(
    idList<idStaticModelSurface, 5>& surfaces) {
  if (indexes.Num() == 0)
    return;
  if (generateNormals)
    GenerateNormals();
  DuplicateMirroredVertexes();

  idList<idDrawVert, 5> drawVerts;
  idList<int, 5> triangleConstructionCleanup;
  (void)triangleConstructionCleanup;
  drawVerts.SetNum(verts.Num());
  for (int index = 0; index < verts.Num(); ++index) {
    const matchVert_t& source = verts[index];
    idDrawVert& destination = drawVerts[index];
    std::memset(&destination, 0, sizeof(destination));
    destination.color[0] = destination.color[1] = destination.color[2] =
        destination.color[3] = 0xff;
    destination.xyz = (*pvList)[source.v];
    if (ptvList->Num() > 0)
      destination.st = (*ptvList)[source.tv];
    EncodeDirection(source.normal, destination.normal);
    std::memcpy(destination.color, &source.color, sizeof(source.color));
  }

  idList<vertexTangents_t, 5> tangents;
  DeriveVertexTangents(indexes, drawVerts, tangents);
  for (int index = 0; index < drawVerts.Num(); ++index) {
    EncodeDirection(tangents[index].tangents[0], drawVerts[index].tangent);
    drawVerts[index].SetBiTangent(tangents[index].tangents[1]);
  }

  idStaticModelSurface surface;
  surface.material = material;
  surface.materialNum = materialNum;

  idList<idSplitResults, 5> split;
  SplitTrianglesForVertexLimit(indexes, 0x7fff, split);
  for (int splitIndex = 0; splitIndex < split.Num(); ++splitIndex) {
    const idSplitResults& result = split[splitIndex];
    idMem* memory = idMem::Singleton();
    void* allocation = memory->AllocWithLocation(
        "w:\\build\\tech5\\shared\\idlib\\Heap.h(95) : tag",
        sizeof(idTriangles), TAG_NEW, false, ALIGN_16, HEAP_DEFAULTHEAP);
    idTriangles* triangles =
        allocation != nullptr ? ::new (allocation) idTriangles : nullptr;
    if (triangles == nullptr)
      idLib::Error("MakeSurfaces: failed to allocate triangles");

    triangles->bounds.Clear();
    for (int index = 0; index < result.vertexMap.Num(); ++index)
      triangles->bounds.AddPoint(drawVerts[result.vertexMap[index]].xyz);

    triangles->numVerts = result.vertexMap.Num();
    if (pMorphList->Num() <= 1) {
      triangles->AllocStaticTriSurfVerts(triangles->numVerts);
      for (int index = 0; index < result.vertexMap.Num(); ++index)
        triangles->verts[index] = drawVerts[result.vertexMap[index]];
    } else {
      triangles->cpuVertexMask |= 0x1000u;
      triangles->vertexMask |= 0x1000u;
      triangles->AllocStaticTriSurfVerts(triangles->numVerts);
      auto* morphVerts = reinterpret_cast<morphDrawVert_t*>(triangles->verts);
      for (int index = 0; index < result.vertexMap.Num(); ++index) {
        const int mappedIndex = result.vertexMap[index];
        const matchVert_t& source = verts[mappedIndex];
        const idDrawVert& drawVert = drawVerts[mappedIndex];
        morphDrawVert_t& destination = morphVerts[index];
        destination.xyz = (*pvList)[source.v];
        destination.xyz1 = (*pMorphList)[source.morph];
        if (ptvList->Num() > 0)
          destination.st = (*ptvList)[source.tv];
        std::memcpy(destination.normal, drawVert.normal, 4);
        std::memcpy(destination.tangent, drawVert.normal, 4);
        std::memcpy(destination.biTangent, drawVert.normal, 4);
      }
    }

    triangles->numIndexes = result.indexes.Num();
    triangles->AllocStaticTriSurfIndexes(triangles->numIndexes);
    for (int index = 0; index < result.indexes.Num(); ++index)
      triangles->indexes[index] =
          static_cast<std::uint16_t>(result.indexes[index]);
    surface.geometry = triangles;
    surfaces.Append(surface);
  }
}

void EmitRawSurfacesToStaticModel(idList<idRawSurface, 5>& rawSurfaces,
                                  idStaticModel& model) {
  for (int rawIndex = 0; rawIndex < rawSurfaces.Num(); ++rawIndex) {
    idList<idStaticModelSurface, 5> surfaces;
    rawSurfaces[rawIndex].MakeSurfaces(surfaces);
    for (int surfaceIndex = 0; surfaceIndex < surfaces.Num(); ++surfaceIndex) {
      idStaticModelSurface& surface = surfaces[surfaceIndex];
      if (surface.geometry->numVerts != 0) {
        model.AddSurface(surface);
      } else {
        idLib::Warning(
            "EmitRawSurfacesToStaticModel: model '%s' has surface '%s' with no triangles",
            model.name.str, MaterialName(surface.material));
      }
    }
  }
}

template void* idListArrayResize<matchVert_t>(void*, int, int, memTag_t,
                                              bool);
template bool idList<matchVert_t, 5>::Resize(int);
template int idList<matchVert_t, 5>::Append(const matchVert_t&);
template void* idListArrayResize<vertexTangents_t>(void*, int, int, memTag_t,
                                                   bool);
template bool idList<vertexTangents_t, 5>::Resize(int);
template void* idListArrayNew<idSplitResults>(int, memTag_t, bool);
template void idListArrayDelete<idSplitResults>(void*, int);
template void* idListArrayResize<idSplitResults>(void*, int, int, memTag_t,
                                                 bool);
template bool idList<idSplitResults, 5>::Resize(int);
template idList<idSplitResults, 5>::~idList();
