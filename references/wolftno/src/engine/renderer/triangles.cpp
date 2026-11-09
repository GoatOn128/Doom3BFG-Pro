#include "engine/renderer/triangles.h"

#include "engine/compilers/meshoptimizer/meshoptimizer.h"
#include "engine/decls/declskins.h"
#include "engine/models/skinremap.h"
#include "engine/models/static/staticmodel.h"
#include "engine/models/static/staticmodeltristrips.h"
#include "engine/renderer/declmaterial.h"
#include "engine/renderer/triverts.h"
#include "shared/idlib/containers/hashindex.h"
#include "shared/idlib/containers/heaparray.h"
#include "shared/idlib/containers/list.h"
#include "shared/idlib/csystems/cvarsystemlocal.h"
#include "shared/idlib/filesystem/file.h"
#include "shared/idlib/lib.h"
#include "shared/idlib/sys/sys_alloc.h"

#include <algorithm>
#include <cmath>
#include <cstdint>
#include <cstring>
#include <immintrin.h>
#include <limits>
#include <new>

extern idCVar feature_lowgore;

#if defined(WOLF_RENDERER_TRIANGLES_VALIDATION)
extern const idMaterial* WolfTrianglesValidationFindMaterial(
    const char* name, bool makeDefault);
#endif

namespace {

constexpr const char* kTriangleVertexAllocation =
    "w:\\build\\tech5\\engine\\renderer\\Triangles.cpp(309) : "
    "TAG_TRIANGLES";
constexpr const char* kTriangleIndexAllocation =
    "w:\\build\\tech5\\engine\\renderer\\Triangles.cpp(328) : "
    "TAG_TRIANGLES";
constexpr const char* kResizeVertexAllocation =
    "w:\\build\\tech5\\engine\\renderer\\Triangles.cpp(348) : "
    "TAG_TRIANGLES";
constexpr const char* kResizeIndexAllocation =
    "w:\\build\\tech5\\engine\\renderer\\Triangles.cpp(364) : "
    "TAG_TRIANGLES";
constexpr const char* kOptimizeIndexAllocation =
    "w:\\build\\tech5\\engine\\renderer\\Triangles.cpp(871) : "
    "TAG_TRIANGLES";
constexpr const char* kOptimizeVertexAllocation =
    "w:\\build\\tech5\\engine\\renderer\\Triangles.cpp(892) : "
    "TAG_TRIANGLES";
constexpr const char* kQuadVertexAllocation =
    "w:\\build\\tech5\\engine\\renderer\\Triangles.cpp(1779) : "
    "TAG_TRIANGLES";
constexpr const char* kQuadIndexAllocation =
    "w:\\build\\tech5\\engine\\renderer\\Triangles.cpp(1797) : "
    "TAG_TRIANGLES";

int unstrippedBytes = 0;
int strippedBytes = 0;

struct tangentVert_t {
  bool polarityUsed[2];
  int negativeRemap;
};

struct vertexTangents_t {
  idVec3 normal;
  idVec3 tangent;
  idVec3 biTangent;
};

static_assert(sizeof(tangentVert_t) == 8, "tangentVert_t ABI drift");
static_assert(sizeof(vertexTangents_t) == 36,
              "vertexTangents_t ABI drift");

template <typename T>
T* Allocate(const char* location, std::size_t count, memTag_t tag,
            bool clear) {
  return static_cast<T*>(idMem::Singleton()->AllocWithLocation(
      location, sizeof(T) * count, tag, clear, ALIGN_16, HEAP_DEFAULTHEAP));
}

void Free(void* pointer) {
  if (pointer != nullptr) idMem::Singleton()->Free(pointer, ALIGN_16);
}

int VertexSizeFromMask(int mask) {
  return (mask & 4) + ((mask >> 1) & 4) + ((mask >> 2) & 4) +
         ((mask >> 9) & 4) + ((mask & 1) != 0 ? 12 : 0) +
         ((mask & 0x20) != 0 ? -4 : 0) +
         ((mask & 0x40) != 0 ? -4 : 0) +
         ((mask & 0x1000) != 0 ? 12 : 0) + 4 * (mask & 2);
}

std::uint16_t ByteSwap16(std::uint16_t value) {
  return static_cast<std::uint16_t>((value << 8) | (value >> 8));
}

std::uint32_t ByteSwap32(std::uint32_t value) {
  return (value << 24) | ((value << 8) & 0x00ff0000u) |
         ((value >> 8) & 0x0000ff00u) | (value >> 24);
}

void SwapFloat(float& value) {
  std::uint32_t bits = 0;
  std::memcpy(&bits, &value, sizeof(bits));
  bits = ByteSwap32(bits);
  std::memcpy(&value, &bits, sizeof(value));
}

void WriteBig32(idFile* file, std::uint32_t value) {
  value = ByteSwap32(value);
  file->Write(&value, sizeof(value));
}

std::uint32_t ReadBig32(idFile* file) {
  std::uint32_t value = 0;
  file->Read(&value, sizeof(value));
  return ByteSwap32(value);
}

void WriteBigFloats(idFile* file, const float* values, int count) {
  for (int index = 0; index < count; ++index) {
    float value = values[index];
    SwapFloat(value);
    file->Write(&value, sizeof(value));
  }
}

void ReadBigFloats(idFile* file, float* values, int count) {
  for (int index = 0; index < count; ++index) {
    file->Read(&values[index], sizeof(values[index]));
    SwapFloat(values[index]);
  }
}

void SwapDrawVert(idDrawVert& vertex) {
  SwapFloat(vertex.xyz.x);
  SwapFloat(vertex.xyz.y);
  SwapFloat(vertex.xyz.z);
  SwapFloat(vertex.st.x);
  SwapFloat(vertex.st.y);
}

void WriteBigDrawVerts(idFile* file, const idDrawVert* vertices, int count) {
  for (int index = 0; index < count; ++index) {
    idDrawVert vertex = vertices[index];
    SwapDrawVert(vertex);
    file->Write(&vertex, sizeof(vertex));
  }
}

void ReadBigDrawVerts(idFile* file, idDrawVert* vertices, int count) {
  file->Read(vertices, sizeof(idDrawVert) * static_cast<std::size_t>(count));
  for (int index = 0; index < count; ++index) SwapDrawVert(vertices[index]);
}

void WriteBigIndexes(idFile* file, const std::uint16_t* indexes, int count) {
  for (int index = 0; index < count; ++index) {
    const std::uint16_t value = ByteSwap16(indexes[index]);
    file->Write(&value, sizeof(value));
  }
}

void ReadBigIndexes(idFile* file, std::uint16_t* indexes, int count) {
  file->Read(indexes, sizeof(std::uint16_t) * static_cast<std::size_t>(count));
  for (int index = 0; index < count; ++index)
    indexes[index] = ByteSwap16(indexes[index]);
}

void ClearBounds(idBounds& bounds) {
  constexpr float infinity = 1.0e30f;
  bounds[0] = {infinity, infinity, infinity};
  bounds[1] = {-infinity, -infinity, -infinity};
}

void ClearDrawVert(idDrawVert& vertex) {
  std::memset(&vertex, 0, sizeof(vertex));
  std::memset(vertex.color, 0xff, sizeof(vertex.color));
}

float InverseLength(const idVec3& value) {
  return 1.0f /
         std::sqrt(std::max(value.LengthSqr(),
                            std::numeric_limits<float>::min()));
}

void Normalize(idVec3& value) { value *= InverseLength(value); }

std::uint8_t EncodeDirection(float value) {
  const float encoded = (value + 1.0f) * 127.5f + 0.5f;
  const int rounded = _mm_cvt_ss2si(_mm_set_ss(encoded));
  return static_cast<std::uint8_t>(std::clamp(rounded, 0, 255));
}

bool FaceHasNegativePolarity(const idTriangles& triangles,
                             int firstIndex) {
  const idDrawVert& a = triangles.verts[triangles.indexes[firstIndex]];
  const idDrawVert& b = triangles.verts[triangles.indexes[firstIndex + 1]];
  const idDrawVert& c = triangles.verts[triangles.indexes[firstIndex + 2]];
  return (c.st.y - a.st.y) * (b.st.x - a.st.x) -
             (c.st.x - a.st.x) * (b.st.y - a.st.y) <
         0.0f;
}

bool HasVirtualMapping(const idVec4& mapping) {
  std::uint32_t bits[4]{};
  std::memcpy(bits, &mapping, sizeof(bits));
  return ((bits[0] | bits[1] | bits[2] | bits[3]) & 0x7fffffffu) != 0;
}

template <typename T, int Tag>
bool Contains(const idList<T, Tag>& list, const T& value) {
  for (int index = 0; index < list.num; ++index) {
    if (list.list[index] == value) return true;
  }
  return false;
}

template <typename T, int Tag>
void AppendUnique(idList<T, Tag>& list, const T& value) {
  if (!Contains(list, value)) list.Append(value);
}

const idMaterial* FindMaterial(const char* name, bool makeDefault) {
  if (name == nullptr) return nullptr;
#if defined(WOLF_RENDERER_TRIANGLES_VALIDATION)
  return WolfTrianglesValidationFindMaterial(name, makeDefault);
#else
  return static_cast<const idMaterial*>(
      idMaterial::resourceList.FindWithInheritance(name, makeDefault, true));
#endif
}

} // namespace

idTriangles::idTriangles()
    : generateNormals(false), numIndexes(0), numVerts(0), detailOffset(0),
      allowGpuHosting(true), cpuVertexMask(31), verts(nullptr), indexes(nullptr),
      vertexMask(31), vertexBuffer(), indexBuffer() {
  vsb.xyzScale = {1.0f, 1.0f, 1.0f};
  vsb.xyzBias = {0.0f, 0.0f, 0.0f};
  vsb.stScale = {1.0f, 1.0f};
  vsb.stBias = {0.0f, 0.0f};
  ClearBounds(bounds);
}

idTriangles::idTriangles(int numVerts_, int numIndexes_)
    : idTriangles() {
  numVerts = numVerts_;
  numIndexes = numIndexes_;
  AllocStaticTriSurfVerts(numVerts);
  AllocStaticTriSurfIndexes(numIndexes);
}

idTriangles::~idTriangles() { FreeData(); }

int idTriangles::CPUMemoryUsed() {
  if (this == nullptr) return 0;
  int memory = sizeof(*this);
  if (verts != nullptr) memory += 32 * numVerts;
  if (indexes != nullptr) memory += 2 * numIndexes;
  return memory;
}

int idTriangles::GPUMemoryUsed() {
  return vertexBuffer.GetSize() + indexBuffer.GetSize();
}

void idTriangles::FreeCPUData() {
  Free(verts);
  verts = nullptr;
  Free(indexes);
  indexes = nullptr;
}

void idTriangles::FreeData() {
  vertexBuffer.FreeBufferObject();
  indexBuffer.FreeBufferObject();
  FreeCPUData();
  generateNormals = false;
  numIndexes = 0;
  numVerts = 0;
}

void idTriangles::AllocStaticTriSurfIndexes(int count) {
  Free(indexes);
  indexes = nullptr;
  if (count <= 0) return;
  indexes = Allocate<std::uint16_t>(kTriangleIndexAllocation,
                                    static_cast<std::size_t>(count),
                                    TAG_TRIANGLES, true);
  if (indexes == nullptr) {
    idLib::Error(
        "AllocStaticTriSurfIndexes: failed to allocate memory for indices "
        "(count %d)",
        count);
  }
}

void idTriangles::AllocStaticTriSurfVerts(int count) {
  Free(verts);
  verts = nullptr;
  if (count > 0xffff) {
    idLib::Error("AllocStaticTriSurfVerts: numVerts ( %d ) > %d", count,
                 0xffff);
  }
  if (count <= 0) return;
  const int vertexSize = VertexSizeFromMask(static_cast<int>(cpuVertexMask));
  verts = static_cast<idDrawVert*>(idMem::Singleton()->AllocWithLocation(
      kTriangleVertexAllocation,
      static_cast<std::size_t>(vertexSize) * static_cast<std::size_t>(count),
      TAG_TRIANGLES, true, ALIGN_16, HEAP_DEFAULTHEAP));
  if (verts == nullptr) {
    idLib::Error(
        "AllocStaticTriSurfVerts: failed to allocate memory for vertices "
        "(count %d)",
        count);
  }
}

void idTriangles::ResizeVerts(int newNumVerts) {
  if (newNumVerts > MAX_TRIANGLE_VERTS) {
    idLib::Error("ResizeVerts: numVerts > %d", MAX_TRIANGLE_VERTS);
  }
  idDrawVert* replacement = Allocate<idDrawVert>(
      kResizeVertexAllocation, static_cast<std::size_t>(newNumVerts),
      TAG_TRIANGLES, false);
  if (replacement == nullptr) {
    idLib::Error(
        "ResizeVerts: failed to allocate memory for vertices (count %d)",
        newNumVerts);
  }
  const int copyCount = std::min(newNumVerts, numVerts);
  std::memmove(replacement, verts,
               sizeof(idDrawVert) * static_cast<std::size_t>(copyCount));
  Free(verts);
  verts = replacement;
}

void idTriangles::ResizeIndexes(int newNumIndexes) {
  std::uint16_t* replacement = Allocate<std::uint16_t>(
      kResizeIndexAllocation, static_cast<std::size_t>(newNumIndexes),
      TAG_TRIANGLES, false);
  if (replacement == nullptr) {
    idLib::Error(
        "ResizeIndexes: failed to allocate memory for indices (count %d)",
        newNumIndexes);
  }
  const int copyCount = std::min(newNumIndexes, numIndexes);
  std::memmove(replacement, indexes,
               sizeof(std::uint16_t) * static_cast<std::size_t>(copyCount));
  Free(indexes);
  indexes = replacement;
}

void idTriangles::RangeCheckIndexes(std::uint16_t* checkedIndexes,
                                    int checkedNumIndexes,
                                    int checkedNumVerts) {
  if (checkedNumIndexes < 0)
    idLib::Error("RangeCheckIndexes: numIndexes < 0");
  if (checkedNumVerts < 0)
    idLib::Error("RangeCheckIndexes: numVerts < 0");
  for (int index = 0; index < checkedNumIndexes; ++index) {
    const std::uint16_t value = checkedIndexes[index];
    if (value != 0xffffu && value >= checkedNumVerts)
      idLib::Error("RangeCheckIndexes: index out of range");
  }
}

void idTriangles::RemoveDegenerate() {
  for (int triangle = 0; triangle < numIndexes / 3; ++triangle) {
    const int first = triangle * 3;
    const std::uint16_t a = indexes[first];
    const std::uint16_t b = indexes[first + 1];
    const std::uint16_t c = indexes[first + 2];
    if (a != b && a != c && b != c) continue;
    numIndexes -= 3;
    std::memmove(&indexes[first], &indexes[first + 3],
                 sizeof(std::uint16_t) *
                     static_cast<std::size_t>(numIndexes - first));
    --triangle;
  }
}

void idTriangles::OptimizeIndexOrder() {
  std::uint16_t* replacement = Allocate<std::uint16_t>(
      kOptimizeIndexAllocation, static_cast<std::size_t>(numIndexes),
      TAG_TRIANGLES, false);
  MeshOptimizer(indexes, static_cast<unsigned int>(numIndexes), replacement,
                1.0f, 0.0f, 0.80000001f);
  Free(indexes);
  indexes = replacement;
}

void idTriangles::MergeTriangles(const idTriangles* source) {
  ResizeVerts(numVerts + source->numVerts);
  ResizeIndexes(numIndexes + source->numIndexes);
  std::memmove(&verts[numVerts], source->verts,
               sizeof(idDrawVert) *
                   static_cast<std::size_t>(source->numVerts));
  for (int index = 0; index < source->numIndexes; ++index) {
    indexes[numIndexes + index] = static_cast<std::uint16_t>(
        static_cast<std::uint16_t>(numVerts) + source->indexes[index]);
  }
  numVerts += source->numVerts;
  numIndexes += source->numIndexes;
}

idBounds idTriangles::BoundsFromVertScan() {
  idDrawVert* scanVerts = verts;
  if (scanVerts == nullptr) {
    if (vertexBuffer.GetSize() == 0)
      idLib::Error("CalcBounds() - invalid vert and vertexBuffer data\n");
    scanVerts = static_cast<idDrawVert*>(vertexBuffer.MapBuffer(BM_READ));
  }

  idBounds result;
  ClearBounds(result);
  if (scanVerts != nullptr) {
    for (int index = 0; index < numVerts; ++index) {
      const idVec3& xyz = scanVerts[index].xyz;
      result[0].x = std::fmin(result[0].x, xyz.x);
      result[0].y = std::fmin(result[0].y, xyz.y);
      result[0].z = std::fmin(result[0].z, xyz.z);
      result[1].x = std::fmax(result[1].x, xyz.x);
      result[1].y = std::fmax(result[1].y, xyz.y);
      result[1].z = std::fmax(result[1].z, xyz.z);
    }
    result[0].x = result[0].x * vsb.xyzScale.x + vsb.xyzBias.x;
    result[0].y = result[0].y * vsb.xyzScale.y + vsb.xyzBias.y;
    result[0].z = result[0].z * vsb.xyzScale.z + vsb.xyzBias.z;
    result[1].x = result[1].x * vsb.xyzScale.x + vsb.xyzBias.x;
    result[1].y = result[1].y * vsb.xyzScale.y + vsb.xyzBias.y;
    result[1].z = result[1].z * vsb.xyzScale.z + vsb.xyzBias.z;
  }
  if (scanVerts != verts) vertexBuffer.UnmapBuffer();
  return result;
}

void idTriangles::TriangleVectors(const idDrawVert* a, const idDrawVert* b,
                                  const idDrawVert* c, idVec3& normal,
                                  idVec3& tangent, idVec3& biTangent) {
  const idVec3 cDelta = c->xyz - a->xyz;
  const idVec3 bDelta = b->xyz - a->xyz;
  const float cS = c->st.x - a->st.x;
  const float cT = c->st.y - a->st.y;
  const float bS = b->st.x - a->st.x;
  const float bT = b->st.y - a->st.y;

  normal = cDelta.Cross(bDelta);
  Normalize(normal);

  const float textureArea = cT * bS - cS * bT;
  if (std::fabs(textureArea) < 9.9999997e-21f) {
    normal = {0.0f, 0.0f, 0.0f};
    tangent = {0.0f, 0.0f, 0.0f};
    biTangent = {0.0f, 0.0f, 0.0f};
    return;
  }

  const float sign = textureArea >= 0.0f ? 1.0f : -1.0f;
  tangent = {(cT * bDelta.x - cDelta.x * bT) * sign,
             (cT * bDelta.y - cDelta.y * bT) * sign,
             (cT * bDelta.z - cDelta.z * bT) * sign};
  Normalize(tangent);
  biTangent = {(cDelta.x * bS - cS * bDelta.x) * sign,
               (cDelta.y * bS - cS * bDelta.y) * sign,
               (cDelta.z * bS - cS * bDelta.z) * sign};
  Normalize(biTangent);
}

void idTriangles::UpdateIndexBuffer() {
  if (numIndexes == 0 || indexes == nullptr) return;
  const int requiredSize = 2 * numIndexes;
  const int currentSize = indexBuffer.GetSize();
  if (currentSize == 0) {
    indexBuffer.AllocBufferObject(indexes, requiredSize, BU_DEFAULT);
  } else if (requiredSize <= currentSize) {
    indexBuffer.Update(indexes, requiredSize);
  } else {
    idLib::Warning(
        "idTriangles::UpdateIndexBuffer: size overrun, %i > %i - resizing "
        "the buffer",
        requiredSize, currentSize);
    indexBuffer.FreeBufferObject();
    indexBuffer.AllocBufferObject(indexes, requiredSize, BU_DEFAULT);
  }
}

void idTriangles::AddCubeFace(const idVec3& v1, const idVec3& v2,
                              const idVec3& v3, const idVec3& v4) {
  idDrawVert& a = verts[numVerts];
  idDrawVert& b = verts[numVerts + 1];
  idDrawVert& c = verts[numVerts + 2];
  idDrawVert& d = verts[numVerts + 3];
  ClearDrawVert(a);
  ClearDrawVert(b);
  ClearDrawVert(c);
  ClearDrawVert(d);
  a.xyz = v1;
  a.st = {0.0f, 0.0f};
  b.xyz = v2;
  b.st = {1.0f, 0.0f};
  c.xyz = v3;
  c.st = {1.0f, 1.0f};
  d.xyz = v4;
  d.st = {0.0f, 1.0f};
  indexes[numIndexes] = static_cast<std::uint16_t>(numVerts);
  indexes[numIndexes + 1] = static_cast<std::uint16_t>(numVerts + 1);
  indexes[numIndexes + 2] = static_cast<std::uint16_t>(numVerts + 2);
  indexes[numIndexes + 3] = static_cast<std::uint16_t>(numVerts);
  indexes[numIndexes + 4] = static_cast<std::uint16_t>(numVerts + 2);
  indexes[numIndexes + 5] = static_cast<std::uint16_t>(numVerts + 3);
  numVerts += 4;
  numIndexes += 6;
}

int idTriangles::CreateQuads(std::uint16_t* quads) {
  const std::uint16_t* sourceIndexes = indexes;
  if (sourceIndexes == nullptr) {
    sourceIndexes =
        static_cast<const std::uint16_t*>(indexBuffer.MapBuffer(BM_READ));
  }

  int input = 0;
  int quadCount = 0;
  while (input < numIndexes - 3) {
    const std::uint16_t a = sourceIndexes[input];
    const std::uint16_t b = sourceIndexes[input + 1];
    const std::uint16_t c = sourceIndexes[input + 2];
    const std::uint16_t d = sourceIndexes[input + 3];
    const std::uint16_t e = sourceIndexes[input + 4];
    const std::uint16_t f = sourceIndexes[input + 5];
    std::uint16_t* quad = &quads[quadCount * 4];
    bool paired = true;
    if (e == b && f == a) {
      quad[0] = a; quad[1] = d; quad[2] = c; quad[3] = b;
    } else if (e == c && f == b) {
      quad[0] = c; quad[1] = a; quad[2] = d; quad[3] = b;
    } else if (e == a && f == c) {
      quad[0] = a; quad[1] = b; quad[2] = d; quad[3] = c;
    } else if (f == b && d == a) {
      quad[0] = a; quad[1] = e; quad[2] = c; quad[3] = b;
    } else if (f == c && d == b) {
      quad[0] = c; quad[1] = a; quad[2] = e; quad[3] = b;
    } else if (f == a && d == c) {
      quad[0] = a; quad[1] = b; quad[2] = e; quad[3] = c;
    } else if (d == b && e == a) {
      quad[0] = a; quad[1] = f; quad[2] = c; quad[3] = b;
    } else if (d == c && e == b) {
      quad[0] = c; quad[1] = a; quad[2] = f; quad[3] = b;
    } else if (d == a && e == c) {
      quad[0] = a; quad[1] = b; quad[2] = f; quad[3] = c;
    } else {
      paired = false;
      quad[0] = a; quad[1] = b; quad[2] = c; quad[3] = c;
    }
    input += paired ? 6 : 3;
    ++quadCount;
  }
  if (input < numIndexes) {
    std::uint16_t* quad = &quads[quadCount * 4];
    quad[0] = sourceIndexes[input];
    quad[1] = sourceIndexes[input + 1];
    quad[2] = sourceIndexes[input + 2];
    quad[3] = sourceIndexes[input + 2];
    ++quadCount;
  }
  if (sourceIndexes != indexes) indexBuffer.UnmapBuffer();
  return quadCount;
}

const idDrawVert* idTriangles::GetFullSizeDrawVerts() {
  if (verts != nullptr && (cpuVertexMask & 0xfffffcffu) == 31u) return verts;

  AllocStaticTriSurfVerts(numVerts);
  const auto* const inputStart = static_cast<const std::uint8_t*>(
      vertexBuffer.MapBuffer(BM_READ));
  const std::uint8_t* input = inputStart;
  for (int index = 0; index < numVerts; ++index) {
    idDrawVert& vertex = verts[index];
    ClearDrawVert(vertex);

    if ((vertexMask & VERTEX_MASK_XYZ_SHORT) != 0) {
      std::int16_t xyz[3]{};
      std::memcpy(xyz, input, sizeof(xyz));
      input += 8;
      vertex.xyz = {
          static_cast<float>(xyz[0]) * vsb.xyzScale.x + vsb.xyzBias.x,
          static_cast<float>(xyz[1]) * vsb.xyzScale.y + vsb.xyzBias.y,
          static_cast<float>(xyz[2]) * vsb.xyzScale.z + vsb.xyzBias.z};
    } else if ((vertexMask & VERTEX_MASK_XYZ) != 0) {
      std::memcpy(&vertex.xyz, input, sizeof(vertex.xyz));
      input += sizeof(vertex.xyz);
    }

    if ((vertexMask & VERTEX_MASK_XYZ1) != 0) input += 12;

    if ((vertexMask & VERTEX_MASK_ST_SHORT) != 0) {
      std::int16_t st[2]{};
      std::memcpy(st, input, sizeof(st));
      input += sizeof(st);
      vertex.st = {
          static_cast<float>(st[0]) * vsb.stScale.x + vsb.stBias.x,
          static_cast<float>(st[1]) * vsb.stScale.y + vsb.stBias.y};
    } else if ((vertexMask & VERTEX_MASK_ST) != 0) {
      std::memcpy(&vertex.st, input, sizeof(vertex.st));
      input += sizeof(vertex.st);
    }

    if ((vertexMask & VERTEX_MASK_ST1_EXTERNAL) == 0 &&
        (vertexMask & VERTEX_MASK_ST1_SHORT) != 0) {
      input += 4;
    }
    if ((vertexMask & VERTEX_MASK_NORMAL) != 0) {
      std::memcpy(vertex.normal, input, sizeof(vertex.normal));
      input += sizeof(vertex.normal);
    }
    if ((vertexMask & VERTEX_MASK_TANGENT) != 0) {
      std::memcpy(vertex.tangent, input, sizeof(vertex.tangent));
      input += sizeof(vertex.tangent);
    }
    if ((vertexMask & VERTEX_MASK_COLOR) != 0) {
      std::memcpy(vertex.color, input, sizeof(vertex.color));
      input += sizeof(vertex.color);
    }
  }
  const std::ptrdiff_t consumed = input - inputStart;
  const std::ptrdiff_t expected =
      static_cast<std::ptrdiff_t>(numVerts) *
      VertexSizeFromMask(static_cast<int>(vertexMask));
  if (consumed != expected) {
    idLib::FatalError("idTriangles::GetFullSizeDrawVerts: miscount");
  }
  vertexBuffer.UnmapBuffer();
  return verts;
}

idTriangles* idTriangles::CopyStaticTriSurf() {
  idTriangles* copy = new idTriangles;
  copy->vertexMask = vertexMask;
  copy->cpuVertexMask = cpuVertexMask;
  copy->numVerts = numVerts;
  copy->numIndexes = numIndexes;
  copy->bounds = bounds;
  copy->detailOffset = detailOffset;

  if (verts != nullptr) {
    copy->AllocStaticTriSurfVerts(numVerts);
    std::memmove(copy->verts, verts,
                 static_cast<std::size_t>(numVerts) *
                     VertexSizeFromMask(static_cast<int>(cpuVertexMask)));
  }
  if (indexes != nullptr) {
    copy->AllocStaticTriSurfIndexes(numIndexes);
    std::memmove(copy->indexes, indexes,
                 sizeof(std::uint16_t) *
                     static_cast<std::size_t>(numIndexes));
  }
  if (vertexBuffer.GetSize() != 0) {
    const void* data = vertexBuffer.MapBuffer(BM_READ);
    copy->vertexBuffer.AllocBufferObject(
        data, numVerts * VertexSizeFromMask(static_cast<int>(vertexMask)),
        static_cast<int>(vertexMask), BU_DEFAULT);
    vertexBuffer.UnmapBuffer();
  }
  if (indexBuffer.GetSize() != 0) {
    const void* data = indexBuffer.MapBuffer(BM_READ);
    copy->indexBuffer.AllocBufferObject(data, 2 * numIndexes, BU_DEFAULT);
    indexBuffer.UnmapBuffer();
  }
  return copy;
}

void idTriangles::VmtrMapTexCoords(int firstVertex, int lastVertex,
                                   const idVec4 map) {
  if (lastVertex < firstVertex) return;
  idDrawVert* fullVerts = const_cast<idDrawVert*>(GetFullSizeDrawVerts());
  for (int index = firstVertex; index <= lastVertex; ++index) {
    float* coordinates = &fullVerts[index].st.x;
    for (int axis = 0; axis < 2; ++axis) {
      float coordinate = coordinates[axis];
      if (coordinate < 0.0f && coordinate > -0.01f) {
        coordinate = 0.0f;
      } else if (coordinate > 1.0f && coordinate < 1.01f) {
        coordinate = 1.0f;
      } else if (coordinate < 0.0f || coordinate > 1.0f) {
        coordinate -= std::floor(coordinate);
      }
      coordinates[axis] = coordinate * map[axis] + map[axis + 2];
    }
  }
}

void idTriangles::SkipInFile(idFile* file) {
  const int fileNumVerts = static_cast<int>(ReadBig32(file));
  const int fileNumIndexes = static_cast<int>(ReadBig32(file));
  if (fileNumVerts < 0)
    idLib::FatalError("idTriangles::ReadFromFile: bad numVerts (surf %d)",
                      fileNumVerts);
  if (fileNumIndexes < 0)
    idLib::FatalError("idTriangles::ReadFromFile: bad numIndexes (surf %d)",
                      fileNumIndexes);
  const int fileVertexMask = static_cast<int>(ReadBig32(file));
  const int vertexSize =
      fileVertexMask == 31 ? 32 : VertexSizeFromMask(fileVertexMask);
  file->Seek(static_cast<std::int64_t>(fileNumVerts) * vertexSize +
                 2LL * fileNumIndexes + 68,
             FS_SEEK_CUR);
}

void WriteSourceSurfaces(idFile* file,
                         const idList<sourceSurface_t, 5>& sourceSurfaces) {
  WriteBig32(file, static_cast<std::uint32_t>(sourceSurfaces.num));
  for (int index = 0; index < sourceSurfaces.num; ++index) {
    const sourceSurface_t& surface = sourceSurfaces.list[index];
    const char* name = surface.mtr->name.str;
    const int length = name != nullptr ? idStr::Length(name) : 0;
    file->Write(&length, sizeof(length));
    if (length > 0) file->Write(name, static_cast<std::size_t>(length));
    WriteBig32(file, static_cast<std::uint32_t>(surface.mtrChecksum));
    WriteBig32(file, static_cast<std::uint32_t>(surface.renderSurface));
    WriteBig32(file, static_cast<std::uint32_t>(surface.firstVertex));
    WriteBig32(file, static_cast<std::uint32_t>(surface.lastVertex));
  }
}

void ReadSourceSurfaces(idFile* file,
                        idList<sourceSurface_t, 5>& sourceSurfaces,
                        bool skipOver) {
  const int count = static_cast<int>(ReadBig32(file));
  if (!skipOver) sourceSurfaces.SetNum(count);
  for (int index = 0; index < count; ++index) {
    int length = 0;
    file->Read(&length, sizeof(length));
    idStr materialName;
    if (length > 0) {
      materialName.Fill(' ', length);
      file->Read(materialName.data, static_cast<std::size_t>(length));
      materialName.data[length] = '\0';
    } else {
      materialName.Empty();
    }
    sourceSurface_t surface{};
    surface.mtrChecksum = static_cast<int>(ReadBig32(file));
    surface.renderSurface = static_cast<int>(ReadBig32(file));
    surface.firstVertex = static_cast<int>(ReadBig32(file));
    surface.lastVertex = static_cast<int>(ReadBig32(file));
    if (!skipOver) {
      surface.mtr = FindMaterial(materialName.data, true);
      sourceSurfaces.list[index] = surface;
    }
  }
}

void idTriangles::DuplicateMirroredVertexes() {
  const int originalNumVerts = numVerts;
  idHeapArray<tangentVert_t> tangentVerts;
  tangentVerts.Alloc(static_cast<unsigned int>(originalNumVerts));
  std::memset(tangentVerts.buffer, 0,
              sizeof(tangentVert_t) *
                  static_cast<std::size_t>(originalNumVerts));

  for (int firstIndex = 0; firstIndex < numIndexes; firstIndex += 3) {
    const int polarity = FaceHasNegativePolarity(*this, firstIndex) ? 1 : 0;
    tangentVerts.buffer[indexes[firstIndex]].polarityUsed[polarity] = true;
    tangentVerts.buffer[indexes[firstIndex + 1]].polarityUsed[polarity] = true;
    tangentVerts.buffer[indexes[firstIndex + 2]].polarityUsed[polarity] = true;
  }

  int totalVerts = originalNumVerts;
  for (int index = 0; index < originalNumVerts; ++index) {
    tangentVert_t& tangentVert = tangentVerts.buffer[index];
    if (tangentVert.polarityUsed[0] && tangentVert.polarityUsed[1])
      tangentVert.negativeRemap = totalVerts++;
  }
  const int mirroredCount = totalVerts - originalNumVerts;
  if (mirroredCount == 0) return;

  ResizeVerts(totalVerts);
  idHeapArray<int> mirroredVerts;
  mirroredVerts.Alloc(static_cast<unsigned int>(mirroredCount));
  int mirroredIndex = 0;
  for (int index = 0; index < originalNumVerts; ++index) {
    const int remap = tangentVerts.buffer[index].negativeRemap;
    if (remap == 0) continue;
    verts[remap] = verts[index];
    mirroredVerts.buffer[mirroredIndex++] = index;
  }

  numVerts = totalVerts;
  for (int index = 0; index < numIndexes; ++index) {
    const int remap = tangentVerts.buffer[indexes[index]].negativeRemap;
    if (remap != 0 &&
        FaceHasNegativePolarity(*this, 3 * (index / 3))) {
      indexes[index] = static_cast<std::uint16_t>(remap);
    }
  }
  numVerts = totalVerts;
}

void idTriangles::DeriveTangents() {
  if (indexes == nullptr || verts == nullptr) return;

  idHeapArray<vertexTangents_t> vertexTangents;
  vertexTangents.Alloc(static_cast<unsigned int>(numVerts));
  std::memset(vertexTangents.buffer, 0,
              sizeof(vertexTangents_t) *
                  static_cast<std::size_t>(numVerts));

  for (int firstIndex = 0; firstIndex < numIndexes; firstIndex += 3) {
    idVec3 normal{};
    idVec3 tangent{};
    idVec3 biTangent{};
    TriangleVectors(&verts[indexes[firstIndex]],
                    &verts[indexes[firstIndex + 1]],
                    &verts[indexes[firstIndex + 2]], normal, tangent,
                    biTangent);
    for (int corner = 0; corner < 3; ++corner) {
      vertexTangents_t& accumulated =
          vertexTangents.buffer[indexes[firstIndex + corner]];
      accumulated.normal += normal;
      accumulated.tangent += tangent;
      accumulated.biTangent += biTangent;
    }
  }

  if (generateNormals) {
    idHeapArray<int> remap;
    remap.Alloc(static_cast<unsigned int>(numVerts));
    idHashIndex hash(1024, numVerts);
    for (int index = 0; index < numVerts; ++index) {
      const idVec3& xyz = verts[index].xyz;
      const int key = static_cast<int>(xyz.x) + static_cast<int>(xyz.y) +
                      static_cast<int>(xyz.z);
      int match = hash.First(key);
      while (match != -1) {
        const idVec3& candidate = verts[match].xyz;
        if (std::fabs(candidate.x - xyz.x) < 0.0001f &&
            std::fabs(candidate.y - xyz.y) < 0.0001f &&
            std::fabs(candidate.z - xyz.z) < 0.0001f) {
          break;
        }
        match = hash.Next(match);
      }
      if (match == -1) {
        remap.buffer[index] = index;
        hash.Add(key, index);
      } else {
        remap.buffer[index] = match;
      }
    }

    for (int index = 0; index < numVerts; ++index) {
      const int target = remap.buffer[index];
      if (target != index)
        vertexTangents.buffer[target].normal +=
            vertexTangents.buffer[index].normal;
    }
    for (int index = 0; index < numVerts; ++index)
      vertexTangents.buffer[index].normal =
          vertexTangents.buffer[remap.buffer[index]].normal;
  } else {
    for (int index = 0; index < numVerts; ++index) {
      idVec3 decoded{
          static_cast<float>(verts[index].normal[0]) * 0.0078431377f - 1.0f,
          static_cast<float>(verts[index].normal[1]) * 0.0078431377f - 1.0f,
          static_cast<float>(verts[index].normal[2]) * 0.0078431377f - 1.0f};
      Normalize(decoded);
      vertexTangents.buffer[index].normal = decoded;
    }
  }

  for (int index = 0; index < numVerts; ++index) {
    vertexTangents_t& vectors = vertexTangents.buffer[index];
    Normalize(vectors.normal);
    vectors.tangent -= vectors.normal * (vectors.normal * vectors.tangent);
    Normalize(vectors.tangent);
    vectors.biTangent -=
        vectors.normal * (vectors.normal * vectors.biTangent);
    Normalize(vectors.biTangent);

    idDrawVert& vertex = verts[index];
    if (generateNormals) {
      vertex.normal[0] = EncodeDirection(vectors.normal.x);
      vertex.normal[1] = EncodeDirection(vectors.normal.y);
      vertex.normal[2] = EncodeDirection(vectors.normal.z);
    }
    vertex.tangent[0] = EncodeDirection(vectors.tangent.x);
    vertex.tangent[1] = EncodeDirection(vectors.tangent.y);
    vertex.tangent[2] = EncodeDirection(vectors.tangent.z);
    vertex.SetBiTangent(vectors.biTangent);
  }
}

void idTriangles::OptimizeVertexOrder() {
  if (cpuVertexMask != 31 || verts == nullptr) return;

  idHeapArray<int> vertexRemap;
  vertexRemap.Alloc(static_cast<unsigned int>(numVerts));
  for (int index = 0; index < numVerts; ++index)
    vertexRemap.buffer[index] = -1;

  idDrawVert* reordered = Allocate<idDrawVert>(
      kOptimizeVertexAllocation, static_cast<std::size_t>(numVerts),
      TAG_TRIANGLES, false);
  int reorderedCount = 0;
  for (int index = 0; index < numIndexes; ++index) {
    const std::uint16_t oldIndex = indexes[index];
    if (vertexRemap.buffer[oldIndex] == -1) {
      vertexRemap.buffer[oldIndex] = reorderedCount;
      reordered[reorderedCount++] = verts[oldIndex];
    }
    indexes[index] =
        static_cast<std::uint16_t>(vertexRemap.buffer[oldIndex]);
  }
  numVerts = reorderedCount;
  Free(verts);
  verts = reordered;
}

void idTriangles::Cleanup(bool optimize) {
  RangeCheckIndexes(indexes, numIndexes, numVerts);
  RemoveDegenerate();
  DuplicateMirroredVertexes();
  if (optimize) {
    OptimizeIndexOrder();
    OptimizeVertexOrder();
  }
  bounds = BoundsFromVertScan();
  DeriveTangents();
}

void idTriangles::UpdateVertexBuffer() {
  if (numVerts == 0 || verts == nullptr) return;
  const int vertexSize = VertexSizeFromMask(static_cast<int>(vertexMask));
  if (vertexSize == 0) return;

  const int requiredSize = vertexSize * numVerts;
  const int currentSize = vertexBuffer.GetSize();
  int uploadedVerts = 0;
  if (currentSize != 0) {
    if (requiredSize > currentSize) {
      const int oldCapacity = currentSize / vertexSize;
      idLib::Warning(
          "idTriangles::UpdateVertexBuffer: size overrun, %i > %i - "
          "resizing the buffer",
          requiredSize, currentSize);
      vertexBuffer.FreeBufferObject();
      vertexBuffer.AllocBufferObject(verts, requiredSize,
                                     static_cast<int>(vertexMask), BU_DEFAULT);
      uploadedVerts = numVerts - oldCapacity;
    }
  } else {
    vertexBuffer.AllocBufferObject(verts, requiredSize,
                                   static_cast<int>(vertexMask), BU_DEFAULT);
    uploadedVerts = numVerts;
  }
  unstrippedBytes += 32 * uploadedVerts;
  strippedBytes += vertexSize * uploadedVerts;

  if (vertexSize == VertexSizeFromMask(static_cast<int>(cpuVertexMask))) {
    vertexBuffer.Update(verts, requiredSize, static_cast<int>(vertexMask));
    return;
  }

  idHeapArray<unsigned char> strippedVerts;
  strippedVerts.Alloc(static_cast<unsigned int>(requiredSize));
  PackToVertexFormat(verts, strippedVerts.buffer, numVerts,
                     static_cast<int>(vertexMask));
  vertexBuffer.Update(strippedVerts.buffer, requiredSize,
                      static_cast<int>(vertexMask));
}

void idTriangles::TurnIntoQuads() {
  idHeapArray<std::uint16_t> quads;
  quads.Alloc(static_cast<unsigned int>(4 * (numIndexes / 3)));
  const int quadCount = CreateQuads(quads.buffer);

  idDrawVert* sourceVerts = verts;
  if (sourceVerts == nullptr)
    sourceVerts = static_cast<idDrawVert*>(vertexBuffer.MapBuffer(BM_READ));

  idDrawVert* quadVerts = Allocate<idDrawVert>(
      kQuadVertexAllocation, static_cast<std::size_t>(4 * quadCount),
      TAG_TRIANGLES, false);
  for (int quad = 0; quad < quadCount; ++quad) {
    for (int corner = 0; corner < 4; ++corner)
      quadVerts[4 * quad + corner] =
          sourceVerts[quads.buffer[4 * quad + corner]];
  }
  if (sourceVerts == verts)
    Free(verts);
  else
    vertexBuffer.UnmapBuffer();
  verts = quadVerts;
  numVerts = 4 * quadCount;

  Free(indexes);
  indexes = Allocate<std::uint16_t>(
      kQuadIndexAllocation, static_cast<std::size_t>(6 * quadCount),
      TAG_TRIANGLES, false);
  numIndexes = 0;
  for (int quad = 0; quad < quadCount; ++quad) {
    indexes[numIndexes] = static_cast<std::uint16_t>(4 * quad);
    indexes[numIndexes + 1] = static_cast<std::uint16_t>(4 * quad + 2);
    indexes[numIndexes + 2] = static_cast<std::uint16_t>(4 * quad + 3);
    indexes[numIndexes + 3] = static_cast<std::uint16_t>(4 * quad);
    indexes[numIndexes + 4] = static_cast<std::uint16_t>(4 * quad + 3);
    indexes[numIndexes + 5] = static_cast<std::uint16_t>(4 * quad + 1);
    numIndexes += 6;
  }
}

idTriangles* idTriangles::CreateStandardTriangles() {
  idTriangles* standard = new idTriangles;
  if ((vertexMask & VERTEX_MASK_INDEX_LONG) != 0)
    idLib::FatalError(
        "idTriangles::CreateStandardTriangles(): VERTEX_MASK_INDEX_LONG");

  if (numIndexes != 0) {
    if ((vertexMask & VERTEX_MASK_INDEX_STRIP) != 0) {
      idList<std::uint16_t, 5> triangleIndexes;
      const std::uint16_t* sourceIndexes = indexes;
      if (sourceIndexes == nullptr)
        sourceIndexes = static_cast<const std::uint16_t*>(
            indexBuffer.MapBuffer(BM_READ));
      StripIndexesToTriangleIndexes(sourceIndexes, numIndexes,
                                    triangleIndexes);
      if (sourceIndexes != indexes) indexBuffer.UnmapBuffer();
      standard->AllocStaticTriSurfIndexes(triangleIndexes.num);
      standard->numIndexes = triangleIndexes.num;
      std::memmove(standard->indexes, triangleIndexes.list,
                   sizeof(std::uint16_t) *
                       static_cast<std::size_t>(triangleIndexes.num));
    } else {
      standard->AllocStaticTriSurfIndexes(numIndexes);
      standard->numIndexes = numIndexes;
      const std::uint16_t* sourceIndexes = indexes;
      if (sourceIndexes == nullptr)
        sourceIndexes = static_cast<const std::uint16_t*>(
            indexBuffer.MapBuffer(BM_READ));
      std::memmove(standard->indexes, sourceIndexes,
                   sizeof(std::uint16_t) *
                       static_cast<std::size_t>(numIndexes));
      if (sourceIndexes != indexes) indexBuffer.UnmapBuffer();
    }
  } else {
    standard->numIndexes = 0;
  }

  int requiredVerts = 0;
  for (int index = 0; index < standard->numIndexes; ++index)
    requiredVerts =
        std::max(requiredVerts, static_cast<int>(standard->indexes[index]) + 1);
  standard->AllocStaticTriSurfVerts(requiredVerts);
  standard->numVerts = requiredVerts;

  if ((cpuVertexMask == 31 && verts != nullptr) || !allowGpuHosting) {
    if (verts == nullptr)
      idLib::Error("idTriangles::CreateStandardTriangles(): NULL verts");
    std::memmove(standard->verts, verts,
                 sizeof(idDrawVert) *
                     static_cast<std::size_t>(requiredVerts));
  } else if (requiredVerts > 0) {
    const void* input = vertexBuffer.MapBuffer(BM_READ);
    TransferVerts(static_cast<int>(vertexMask), 31, &vsb, requiredVerts,
                  false, input, standard->verts);
    vertexBuffer.UnmapBuffer();
  }
  standard->bounds = bounds;
  return standard;
}

void idTriangles::WriteToFile(idFile* file) {
  WriteBig32(file, static_cast<std::uint32_t>(numVerts));
  WriteBig32(file, static_cast<std::uint32_t>(numIndexes));
  WriteBig32(file, vertexMask);
  WriteBigFloats(file, &vsb.xyzScale.x, 10);

  if (vertexMask == 31) {
    WriteBigDrawVerts(file, verts, numVerts);
  } else {
    const int bufferSize =
        numVerts * VertexSizeFromMask(static_cast<int>(vertexMask));
    idHeapArray<unsigned char> buffer;
    buffer.Alloc(static_cast<unsigned int>(bufferSize));
    if (verts != nullptr) {
      if (cpuVertexMask == (vertexMask & ~VERTEX_MASK_INDEX_STRIP)) {
        std::memmove(buffer.buffer, verts,
                     static_cast<std::size_t>(bufferSize));
      } else {
        PackToVertexFormat(verts, buffer.buffer, numVerts,
                           static_cast<int>(vertexMask));
      }
    } else {
      const void* mapped = vertexBuffer.MapBuffer(BM_READ);
      std::memmove(buffer.buffer, mapped,
                   static_cast<std::size_t>(bufferSize));
      vertexBuffer.UnmapBuffer();
    }
    if (SwapVertexBuffer(buffer.buffer, numVerts,
                         static_cast<int>(vertexMask)) != bufferSize) {
      idLib::FatalError("SwapVertexBuffer() returned mismatched size");
    }
    file->Write(buffer.buffer, static_cast<std::size_t>(bufferSize));
  }

  WriteBigIndexes(file, indexes, numIndexes);
  WriteBigFloats(file, &bounds[0].x, 6);
  WriteBig32(file, static_cast<std::uint32_t>(detailOffset));
}

void idTriangles::ReadFromFile(idFile* file) {
  numVerts = static_cast<int>(ReadBig32(file));
  numIndexes = static_cast<int>(ReadBig32(file));
  if (numVerts < 0)
    idLib::FatalError("idTriangles::ReadFromFile: bad numVerts (surf %d)",
                      numVerts);
  if (numIndexes < 0)
    idLib::FatalError("idTriangles::ReadFromFile: bad numIndexes (surf %d)",
                      numIndexes);
  vertexMask = ReadBig32(file);
  cpuVertexMask = vertexMask;
  ReadBigFloats(file, &vsb.xyzScale.x, 10);

  if (!allowGpuHosting || vertexMask == 31) {
    AllocStaticTriSurfVerts(numVerts);
    ReadBigDrawVerts(file, verts, numVerts);
  } else {
    const int bufferSize =
        numVerts * VertexSizeFromMask(static_cast<int>(vertexMask));
    idHeapArray<unsigned char> buffer;
    buffer.Alloc(static_cast<unsigned int>(bufferSize));
    file->Read(buffer.buffer, static_cast<std::size_t>(bufferSize));
    if (SwapVertexBuffer(buffer.buffer, numVerts,
                         static_cast<int>(vertexMask)) != bufferSize) {
      idLib::FatalError("SwapVertexBuffer() returned bad size");
    }
    vertexBuffer.AllocBufferObject(buffer.buffer, bufferSize,
                                   static_cast<int>(vertexMask), BU_DEFAULT);
  }

  AllocStaticTriSurfIndexes(numIndexes);
  ReadBigIndexes(file, indexes, numIndexes);
  ReadBigFloats(file, &bounds[0].x, 6);
  detailOffset = static_cast<int>(ReadBig32(file));
}

void idTriangles::VmtrMapTexCoordsWithSkins(
    idList<sourceSurface_t, 5>& sourceSurfaces, int renderSurface,
    const idMaterial* originalMaterial, const idDeclSkins* skins,
    idList<skinRemap_t, 5>& skinRemaps,
    idList<const idMaterial*, 5>& usedMaterials) {
  skinRemaps.Clear();
  usedMaterials.Clear();

  if (skins != nullptr) {
    idList<const idMaterial*, 5> materials;
    materials.memTag = TAG_TRIANGLES;
    for (int index = 0; index < sourceSurfaces.num; ++index) {
      const sourceSurface_t& surface = sourceSurfaces.list[index];
      if (surface.renderSurface == renderSurface)
        AppendUnique(materials, surface.mtr);
    }

    if (materials.num == 1) {
      const idMaterial* sourceMaterial = materials.list[0];
      for (int skinIndex = 0; skinIndex < skins->skins.num; ++skinIndex) {
        const idSkin& skin = skins->skins.list[skinIndex];
        const idMaterial* skinMaterial = skin.Remap(sourceMaterial);
        const idMaterial* genericMaterial =
            skinMaterial->CompatibleGenericMaterial();
        sourceMaterial->CompatibleGenericMaterial();

        const bool sourceVirtual =
            HasVirtualMapping(sourceMaterial->virtualMapping);
        const bool skinVirtual = HasVirtualMapping(skinMaterial->virtualMapping);
        if (sourceVirtual != skinVirtual) {
          idLib::Warning(
              "skin %s on model %s tries to remap the %svirtual material "
              "%s to the %svirtual material %s",
              skin.name.str, skins->name.str, sourceVirtual ? "" : "non-",
              sourceMaterial->name.str, skinVirtual ? "" : "non-",
              skinMaterial->name.str);
          continue;
        }

        skinRemap_t remap{};
        remap.mtr = genericMaterial != nullptr ? genericMaterial : skinMaterial;
        if (sourceVirtual) {
          remap.texOffset = {
              skinMaterial->virtualMapping.z -
                  sourceMaterial->virtualMapping.z,
              skinMaterial->virtualMapping.w -
                  sourceMaterial->virtualMapping.w};
          if (genericMaterial == nullptr) {
            if (sourceMaterial->virtualMapping.x > 0.00000011920929f)
              remap.texOffset.x /= sourceMaterial->virtualMapping.x;
            if (sourceMaterial->virtualMapping.y > 0.00000011920929f)
              remap.texOffset.y /= sourceMaterial->virtualMapping.y;
          }
        }
        skinRemaps.Append(remap);
        AppendUnique(usedMaterials, skinMaterial);
      }
    } else {
      for (int pass = 0; pass < 2; ++pass) {
        skinRemaps.Clear();
        usedMaterials.Clear();

        idStr packedName(skins->name.str);
        packedName.StripFileExtension();
        packedName += "_";
        packedName += renderSurface;
        packedName += feature_lowgore.valueInteger != 0 ? "_lg" : "_hg";

        const idMaterial* packedMaterial =
            pass == 0 ? FindMaterial(packedName.data, false) : nullptr;
        if (pass != 0 || packedMaterial == nullptr ||
            packedMaterial->MaterialMappingNeedsReload(skins)) {
          packedMaterial = idMaterial::PackMaterials(
              packedName.data, materials,
              const_cast<idDeclSkins*>(skins)->GetDeclTimestamp());
        }

        for (int index = 0; index < sourceSurfaces.num; ++index) {
          sourceSurface_t& surface = sourceSurfaces.list[index];
          if (surface.renderSurface != renderSurface) continue;
          const float* mapping =
              packedMaterial->GetMaterialMapping(surface.mtr);
          const idVec4 scaleBias{mapping[0], mapping[1], mapping[2],
                                 mapping[3]};
          VmtrMapTexCoords(surface.firstVertex, surface.lastVertex,
                           scaleBias);
          surface.mtr = packedMaterial;
          surface.mtrChecksum = static_cast<int>(packedMaterial->md5Checksum);
        }

        bool rebuildAll = false;
        for (int skinIndex = 0; skinIndex < skins->skins.num; ++skinIndex) {
          const idSkin& skin = skins->skins.list[skinIndex];
          idList<const idMaterial*, 5> skinMaterials;
          skinMaterials.memTag = TAG_TRIANGLES;
          bool changed = false;
          for (int materialIndex = 0; materialIndex < materials.num;
               ++materialIndex) {
            const idMaterial* remapped =
                skin.Remap(materials.list[materialIndex]);
            skinMaterials.Append(remapped);
            changed |= remapped != materials.list[materialIndex];
          }

          const idMaterial* compatibleGeneric =
              skinMaterials.num > 0
                  ? skinMaterials.list[0]->CompatibleGenericMaterial()
                  : nullptr;
          for (int materialIndex = 0; materialIndex < skinMaterials.num;
               ++materialIndex) {
            const idMaterial* currentGeneric =
                skinMaterials.list[materialIndex]
                    ->CompatibleGenericMaterial();
            if (currentGeneric != compatibleGeneric) {
              idLib::Warning(
                  "skin %s on model %s tries to remap generic material %s "
                  "to the non-generic material %s",
                  skin.name.str, skins->name.str,
                  materials.list[materialIndex]->name.str,
                  skinMaterials.list[materialIndex]->name.str);
            }
          }
          if (compatibleGeneric == nullptr) {
            idLib::Error(
                "skin %s on model %s tries to remap generic materials on "
                "surface %d to non-generic materials",
                skin.name.str, skins->name.str, renderSurface);
          }

          if (!changed) {
            skinRemaps.Append({compatibleGeneric, {0.0f, 0.0f}});
            continue;
          }

          idStr packedSkinName(packedName);
          packedSkinName += "_";
          packedSkinName += skin.name.str;
          const idMaterial* packedSkinMaterial =
              pass == 0 ? FindMaterial(packedSkinName.data, false) : nullptr;
          if (pass != 0 || packedSkinMaterial == nullptr ||
              packedSkinMaterial->MaterialMappingNeedsReload(skins) ||
              !packedSkinMaterial->MaterialMappingsAreCompatible(
                  packedMaterial)) {
            packedSkinMaterial = idMaterial::PackMaterials(
                packedSkinName.data, skinMaterials,
                const_cast<idDeclSkins*>(skins)->GetDeclTimestamp());
          }

          if (!packedSkinMaterial->MaterialMappingsAreCompatible(
                  packedMaterial)) {
            if (pass != 0) {
              idLib::Error("skin %s on model %s uses different texture sizes",
                           skin.name.str, skins->name.str);
            }
            idLib::Warning("rebuilding all packed materials");
            rebuildAll = true;
            break;
          }

          skinRemap_t remap{compatibleGeneric, {0.0f, 0.0f}};
          if (HasVirtualMapping(packedMaterial->virtualMapping) &&
              HasVirtualMapping(packedSkinMaterial->virtualMapping)) {
            remap.texOffset = {
                packedSkinMaterial->virtualMapping.z -
                    packedMaterial->virtualMapping.z,
                packedSkinMaterial->virtualMapping.w -
                    packedMaterial->virtualMapping.w};
            skinRemaps.Append(remap);
            AppendUnique(usedMaterials, packedSkinMaterial);
          } else {
            skinRemaps.Append(remap);
            idLib::Warning(
                "Failed to remap skin %s on model %s, vmtr for material %s "
                "is NULL",
                skin.name.str, skins->name.str, packedSkinMaterial->name.str);
          }
        }

        if (!rebuildAll) break;
      }
    }
  }

  for (int index = 0; index < sourceSurfaces.num; ++index) {
    sourceSurface_t& surface = sourceSurfaces.list[index];
    if (surface.renderSurface != renderSurface) continue;
    AppendUnique(usedMaterials, surface.mtr);
    if (surface.mtr == originalMaterial ||
        !HasVirtualMapping(surface.mtr->virtualMapping)) {
      continue;
    }
    if (surface.lastVertex < numVerts) {
      VmtrMapTexCoords(surface.firstVertex, surface.lastVertex,
                       surface.mtr->virtualMapping);
    } else {
      idLib::Warning(
          "idTriangles::VmtrMapTexCoords: remap failed due to wrong number "
          "of vertices -- consider deleting your generated");
    }
  }
}
