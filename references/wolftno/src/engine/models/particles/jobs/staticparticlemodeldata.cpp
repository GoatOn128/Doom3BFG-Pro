#include "engine/models/particles/jobs/staticparticlemodeldata.h"

#include "engine/models/static/staticmodel.h"
#include "shared/idlib/filesystem/filesystemlocal.h"
#include "shared/idlib/lib.h"

#include <algorithm>
#include <cmath>
#include <cstdint>
#include <cstring>
#include <immintrin.h>
#include <new>

idTypedResourceList<idStaticParticleModelData>
    idStaticParticleModelData::resourceList("staticParticleModel");

namespace {

const __m128 vector_float_abs_mask = _mm_castsi128_ps(
    _mm_setr_epi32(0x7fffffff, 0x7fffffff, 0x7fffffff, 0x7fffffff));
const __m128 vector_float_sign_bit = _mm_castsi128_ps(
    _mm_setr_epi32(static_cast<int>(0x80000000u),
                   static_cast<int>(0x80000000u),
                   static_cast<int>(0x80000000u),
                   static_cast<int>(0x80000000u)));
const __m128 vector_float_last_sign_bit = _mm_castsi128_ps(
    _mm_setr_epi32(0, 0, 0, static_cast<int>(0x80000000u)));
const __m128 vector_float_first_sign_bit = _mm_castsi128_ps(
    _mm_setr_epi32(static_cast<int>(0x80000000u), 0, 0, 0));
const __m128 vector_float_clear_last = _mm_castsi128_ps(
    _mm_setr_epi32(-1, -1, -1, 0));
const __m128 DC_OFFSET_V128 =
    _mm_castsi128_ps(_mm_set1_epi32(0x26901d7d));

constexpr memTag_t kParticleMemoryTag = static_cast<memTag_t>(0x4f);
constexpr std::uint32_t kStaticParticleMagic = 0x014f4d50u;

std::uint32_t ByteSwap32(std::uint32_t value) {
  return ((value & 0x000000ffu) << 24) |
         ((value & 0x0000ff00u) << 8) |
         ((value & 0x00ff0000u) >> 8) |
         ((value & 0xff000000u) >> 24);
}

bool ReadExact(idFile* file, void* data, std::size_t size) {
  return file->Read(data, size) == size;
}

bool WriteExact(idFile* file, const void* data, std::size_t size) {
  return file->Write(data, size) == size;
}

bool ReadBig32(idFile* file, std::uint32_t& value) {
  if (!ReadExact(file, &value, sizeof(value)))
    return false;
  value = ByteSwap32(value);
  return true;
}

bool WriteBig32(idFile* file, std::uint32_t value) {
  value = ByteSwap32(value);
  return WriteExact(file, &value, sizeof(value));
}

void SwapDrawVert(idDrawVert& vertex) {
  std::uint32_t* words = reinterpret_cast<std::uint32_t*>(&vertex);
  for (int index = 0; index < 5; ++index)
    words[index] = ByteSwap32(words[index]);
}

bool SameParticleVertex(const idDrawVert& lhs, const idDrawVert& rhs) {
  return lhs.xyz.x == rhs.xyz.x && lhs.xyz.y == rhs.xyz.y &&
         lhs.xyz.z == rhs.xyz.z && lhs.st.x == rhs.st.x &&
         lhs.st.y == rhs.st.y;
}

idVec3 TriangleNormal(const idDrawVert* vertices,
                      const std::uint16_t* indices, int triangle) {
  const idVec3& a = vertices[indices[triangle * 3]].xyz;
  const idVec3& b = vertices[indices[triangle * 3 + 1]].xyz;
  const idVec3& c = vertices[indices[triangle * 3 + 2]].xyz;
  const idVec3 ab{b.x - a.x, b.y - a.y, b.z - a.z};
  const idVec3 ac{c.x - a.x, c.y - a.y, c.z - a.z};
  idVec3 normal{ab.y * ac.z - ab.z * ac.y,
                ab.z * ac.x - ab.x * ac.z,
                ab.x * ac.y - ab.y * ac.x};
  const float length = std::sqrt(normal.x * normal.x + normal.y * normal.y +
                                 normal.z * normal.z);
  if (length > 0.0f) {
    normal.x /= length;
    normal.y /= length;
    normal.z /= length;
  }
  return normal;
}

bool SameEdge(std::uint16_t a0, std::uint16_t a1, std::uint16_t b0,
              std::uint16_t b1) {
  return (a0 == b0 && a1 == b1) || (a0 == b1 && a1 == b0);
}

bool TrianglesShareEdge(const std::uint16_t* indices, int first,
                        int second) {
  const std::uint16_t a[3] = {indices[first * 3], indices[first * 3 + 1],
                              indices[first * 3 + 2]};
  const std::uint16_t b[3] = {indices[second * 3], indices[second * 3 + 1],
                              indices[second * 3 + 2]};
  for (int edgeA = 0; edgeA < 3; ++edgeA) {
    for (int edgeB = 0; edgeB < 3; ++edgeB) {
      if (SameEdge(a[edgeA], a[(edgeA + 1) % 3], b[edgeB],
                   b[(edgeB + 1) % 3]))
        return true;
    }
  }
  return false;
}

int CreateQuads(const std::uint16_t* indexes, int numIndexes,
                std::uint16_t* quads) {
  int outputCount = 0;
  int index = 0;
  while (index < numIndexes - 3) {
    const int base = index;
    const std::uint16_t a = indexes[base];
    const std::uint16_t b = indexes[base + 1];
    const std::uint16_t c = indexes[base + 2];
    const std::uint16_t d = indexes[base + 3];
    const std::uint16_t e = indexes[base + 4];
    const std::uint16_t f = indexes[base + 5];
    std::uint16_t* out = quads + outputCount * 4;
    bool paired = true;
    if (e == b && f == a) {
      out[0] = a; out[1] = d; out[2] = c; out[3] = b;
    } else if (e == c && f == b) {
      out[0] = c; out[1] = a; out[2] = d; out[3] = b;
    } else if (e == a && f == c) {
      out[0] = a; out[1] = b; out[2] = d; out[3] = c;
    } else if (f == b && d == a) {
      out[0] = a; out[1] = e; out[2] = c; out[3] = b;
    } else if (f == c && d == b) {
      out[0] = c; out[1] = a; out[2] = e; out[3] = b;
    } else if (f == a && d == c) {
      out[0] = a; out[1] = b; out[2] = e; out[3] = c;
    } else if (d == b && e == a) {
      out[0] = a; out[1] = f; out[2] = c; out[3] = b;
    } else if (d == c && e == b) {
      out[0] = c; out[1] = a; out[2] = f; out[3] = b;
    } else if (d == a && e == c) {
      out[0] = a; out[1] = b; out[2] = f; out[3] = c;
    } else {
      paired = false;
      out[0] = a; out[1] = b; out[2] = c; out[3] = c;
    }
    index += paired ? 6 : 3;
    ++outputCount;
  }
  if (index < numIndexes) {
    std::uint16_t* out = quads + outputCount * 4;
    out[0] = indexes[index];
    out[1] = indexes[index + 1];
    out[2] = indexes[index + 2];
    out[3] = indexes[index + 2];
    ++outputCount;
  }
  return outputCount;
}

} // namespace

idStaticParticleModelData::idStaticParticleModelData()
    : timestamp(static_cast<std::uint32_t>(-1)), staticVerts(nullptr),
      numStaticVerts(0) {
  idMem::Singleton()->Free(staticVerts, ALIGN_16);
  staticVerts = nullptr;
  numStaticVerts = 0;
}

idStaticParticleModelData::~idStaticParticleModelData() { FreeData(); }

idResourceList* idStaticParticleModelData::GetResourceList() const {
  return &resourceList;
}

bool idStaticParticleModelData::ReloadIfStale() {
  if (fileSystem->GetTimestamp(name.str, false) == timestamp)
    return false;
  char binaryFileName[256];
  fileSystem->FixLongFilename("generated", "pmodel", name.str,
                              binaryFileName, sizeof(binaryFileName));
  fileSystem->RemoveFile(binaryFileName, FSPATH_BASE);
  LoadResource();
  return true;
}

bool idStaticParticleModelData::LoadBinary(const char* fileName) {
  idFileLocal file(fileSystem->OpenFileRead(fileName, true, false));
  if (file.file == nullptr)
    return false;

  std::uint32_t magic = 0;
  if (!ReadBig32(file.file, magic) || magic != kStaticParticleMagic) {
    SetResourceError("bad magic on staticparticlemodeldata '%s'", name.str);
    return false;
  }

  std::uint32_t count = 0;
  if (!ReadBig32(file.file, timestamp) || !ReadBig32(file.file, count))
    return false;
  numStaticVerts = static_cast<int>(count);
  staticVerts = static_cast<idDrawVert*>(idMem::Singleton()->AllocWithLocation(
      "w:\\build\\tech5\\engine\\models\\particles\\jobs\\StaticParticleModelData.cpp(125) : TAG_PARTICLE",
      sizeof(idDrawVert) * static_cast<std::size_t>(numStaticVerts),
      kParticleMemoryTag, false, ALIGN_16, HEAP_DEFAULTHEAP));
  if (!ReadExact(file.file, staticVerts,
                 sizeof(idDrawVert) * static_cast<std::size_t>(numStaticVerts)))
    return false;
  for (int index = 0; index < numStaticVerts; ++index)
    SwapDrawVert(staticVerts[index]);
  return true;
}

bool idStaticParticleModelData::WriteBinary(const char* fileName) {
  idLib::Printf("Writing %s\n", fileName);
  idFileLocal file(fileSystem->OpenFileWrite(fileName));
  if (file.file == nullptr) {
    idLib::Warning(
        "idStaticParticleModelData::WriteBinary() - Could not open %s",
        fileName);
    return false;
  }

  if (!WriteBig32(file.file, kStaticParticleMagic) ||
      !WriteBig32(file.file, timestamp) ||
      !WriteBig32(file.file, static_cast<std::uint32_t>(numStaticVerts)))
    return false;
  for (int index = 0; index < numStaticVerts; ++index) {
    idDrawVert bigVertex = staticVerts[index];
    SwapDrawVert(bigVertex);
    if (!WriteExact(file.file, &bigVertex, sizeof(bigVertex)))
      return false;
  }
  return true;
}

bool idStaticParticleModelData::Generate() {
  idStaticModel* model = new idStaticModel;
  model->SetName(name.str);
  model->PristineLoadResource();
  if (model->resourceError != nullptr || model->surfaces.Num() == 0) {
    delete model;
    idLib::Warning("idStaticParticleModelData: Couldn't load model: %s",
                   name.str);
    return false;
  }

  timestamp = model->sourceTimeStamp;
  idTriangles* geometry = model->surfaces[0].geometry;
  if (model->surfaces.Num() != 1 || geometry->numVerts > 128) {
    FreeData();
    delete model;
    idLib::Warning(
        "Particle %s: Cannot have a static mesh with more than 1 surface or more than %d verts",
        name.str, 128);
    return false;
  }

  const idDrawVert* sourceVertices = geometry->verts;
  bool mappedVertices = false;
  if (sourceVertices == nullptr) {
    sourceVertices = static_cast<const idDrawVert*>(
        geometry->vertexBuffer.MapBuffer(BM_READ));
    mappedVertices = true;
  }
  const std::uint16_t* sourceIndices = geometry->indexes;
  bool mappedIndices = false;
  if (sourceIndices == nullptr) {
    sourceIndices = static_cast<const std::uint16_t*>(
        geometry->indexBuffer.MapBuffer(BM_READ));
    mappedIndices = true;
  }

  idList<idDrawVert, 5> uniqueVertices;
  idList<std::uint16_t, 5> remappedIndices;
  idList<int, 5> vertexRemap;
  idList<int, 5> triangleOrder;
  idList<int, 5> coplanarIndices;
  idList<float, 5> coplanarDots;
  idList<int, 5> orderedIndices;
  for (int vertexIndex = 0; vertexIndex < geometry->numVerts;
       ++vertexIndex) {
    idDrawVert canonicalVertex{};
    canonicalVertex.xyz = sourceVertices[vertexIndex].xyz;
    canonicalVertex.st = sourceVertices[vertexIndex].st;
    std::memset(canonicalVertex.color, 0xff, sizeof(canonicalVertex.color));
    int uniqueIndex = -1;
    for (int index = 0; index < uniqueVertices.Num(); ++index) {
      if (SameParticleVertex(canonicalVertex, uniqueVertices[index])) {
        uniqueIndex = index;
        break;
      }
    }
    if (uniqueIndex < 0)
      uniqueIndex = uniqueVertices.Append(canonicalVertex);
    vertexRemap.Append(uniqueIndex);
  }
  for (int index = 0; index < geometry->numIndexes; ++index)
    remappedIndices.Append(
        static_cast<std::uint16_t>(vertexRemap[sourceIndices[index]]));

  if (mappedVertices)
    geometry->vertexBuffer.UnmapBuffer();
  if (mappedIndices)
    geometry->indexBuffer.UnmapBuffer();

  const int triangleCount = geometry->numIndexes / 3;
  for (int triangle = 0; triangle < triangleCount; ++triangle)
    triangleOrder.Append(triangle);

  while (triangleOrder.Num() > 0) {
    const int baseTriangle = triangleOrder[0];
    orderedIndices.Append(remappedIndices[baseTriangle * 3]);
    orderedIndices.Append(remappedIndices[baseTriangle * 3 + 1]);
    orderedIndices.Append(remappedIndices[baseTriangle * 3 + 2]);

    coplanarIndices.Clear();
    coplanarDots.Clear();
    const idVec3 baseNormal = TriangleNormal(
        uniqueVertices.list, remappedIndices.list, baseTriangle);
    for (int orderIndex = 1; orderIndex < triangleOrder.Num();
         ++orderIndex) {
      const idVec3 normal = TriangleNormal(
          uniqueVertices.list, remappedIndices.list,
          triangleOrder[orderIndex]);
      const float dot = baseNormal.x * normal.x + baseNormal.y * normal.y +
                        baseNormal.z * normal.z;
      if (dot < 0.999999f)
        continue;
      int insertIndex = 0;
      while (insertIndex < coplanarDots.Num() &&
             coplanarDots[insertIndex] < dot)
        ++insertIndex;
      coplanarDots.Insert(dot, insertIndex);
      coplanarIndices.Insert(orderIndex, insertIndex);
    }

    int pairedOrderIndex = -1;
    for (int index = 0; index < coplanarIndices.Num(); ++index) {
      const int orderIndex = coplanarIndices[index];
      if (TrianglesShareEdge(remappedIndices.list, baseTriangle,
                             triangleOrder[orderIndex])) {
        pairedOrderIndex = orderIndex;
        break;
      }
    }
    if (pairedOrderIndex >= 0) {
      const int pairedTriangle = triangleOrder[pairedOrderIndex];
      triangleOrder.RemoveIndexFast(pairedOrderIndex);
      orderedIndices.Append(remappedIndices[pairedTriangle * 3]);
      orderedIndices.Append(remappedIndices[pairedTriangle * 3 + 1]);
      orderedIndices.Append(remappedIndices[pairedTriangle * 3 + 2]);
    }
    triangleOrder.RemoveIndexFast(0);
  }

  idList<std::uint16_t, 5> orderedShortIndices;
  for (int index = 0; index < orderedIndices.Num(); ++index)
    orderedShortIndices.Append(
        static_cast<std::uint16_t>(orderedIndices[index]));
  idList<std::uint16_t, 5> quads;
  quads.SetNum(4 * (orderedShortIndices.Num() / 3));
  const int numQuads = CreateQuads(orderedShortIndices.list,
                                   orderedShortIndices.Num(), quads.list);
  const int generatedVertexCount = 4 * numQuads;
  if (generatedVertexCount > 128) {
    FreeData();
    delete model;
    idLib::Warning(
        "Particle %s creates %d verts : Cannot have a static mesh with more than %d verts",
        name.str, generatedVertexCount, 128);
    return false;
  }

  numStaticVerts = generatedVertexCount;
  staticVerts = static_cast<idDrawVert*>(idMem::Singleton()->AllocWithLocation(
      "w:\\build\\tech5\\engine\\models\\particles\\jobs\\StaticParticleModelData.cpp(338) : TAG_PARTICLE",
      sizeof(idDrawVert) * static_cast<std::size_t>(numStaticVerts),
      kParticleMemoryTag, false, ALIGN_16, HEAP_DEFAULTHEAP));
  for (int index = 0; index < numStaticVerts; ++index)
    staticVerts[index] = uniqueVertices[quads[index]];
  delete model;
  return true;
}

void idStaticParticleModelData::LoadResource() {
  FreeData();
  char binaryFileName[256];
  fileSystem->FixLongFilename("generated", "pmodel", name.str,
                              binaryFileName, sizeof(binaryFileName));
  if (LoadBinary(binaryFileName))
    return;
  if (Generate()) {
    WriteBinary(binaryFileName);
    return;
  }
  SetResourceError("Couldn't load staticparticlemodeldata '%s'", name.str);
  FreeData();
}

template <>
idStaticParticleModelData*
idTypedResourceList<idStaticParticleModelData>::Alloc(
    const char* resourceName) const {
  idStaticParticleModelData* resource =
      new (std::nothrow) idStaticParticleModelData;
  if (resource != nullptr)
    resource->SetName(resourceName);
  return resource;
}
