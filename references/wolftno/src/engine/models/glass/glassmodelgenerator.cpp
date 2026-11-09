#include "engine/models/glass/glassmodelgenerator.h"

#include "engine/models/particles/particlemeshdistributiongenerator.h"
#include "engine/models/static/staticmodel.h"
#include "engine/renderer/autorenderparms.h"
#include "engine/renderer/declmaterial.h"
#include "shared/idlib/containers/BitArray.h"
#include "shared/idlib/containers/hashindex.h"
#include "shared/idlib/filesystem/filesystemlocal.h"
#include "shared/idlib/lib.h"

#include <algorithm>
#include <cmath>
#include <cstdint>
#include <cstring>
#include <new>

idCollisionModel* BuildGlassCollisionModel(
    const char* name, const idRenderModelSurface** surfaces, int numSurfaces,
    bool writeBinary);

template <typename T> class idSearch {
public:
  virtual ~idSearch() = default;
  virtual int Compare(const T& left, const T& right) const = 0;
};

template <typename T> class idSearch_BinaryDefault : public idSearch<T> {
public:
  ~idSearch_BinaryDefault() override = default;
  int Compare(const T& left, const T& right) const override {
    return left < right ? -1 : (left > right ? 1 : 0);
  }
};

template <typename T, typename Search = idSearch_BinaryDefault<T>>
class idSearch_Binary : public Search {
public:
  int Search(const T* base, int num, const T& value) const {
    if (num == 0 || base == nullptr)
      return -1;
    int offset = 0;
    while (num > 0) {
      const int half = num >> 1;
      if (this->Compare(base[offset + half], value) <= 0)
        offset += half;
      num -= half;
      if (half == 0)
        break;
    }
    return this->Compare(base[offset], value) == 0 ? offset : -1;
  }

  int Search_FirstGreater(const T* base, int num, const T& value) const {
    int offset = 0;
    int greater = 0;
    while (num > 0) {
      const int half = num >> 1;
      if (this->Compare(base[offset + half], value) <= 0) {
        offset += half;
        greater = 1;
      } else {
        greater = 0;
      }
      num -= half;
      if (half == 0)
        break;
    }
    return offset + greater;
  }

  int Search_FirstGreaterEqual(const T* base, int num,
                               const T& value) const {
    int offset = 0;
    int greaterEqual = 0;
    while (num > 0) {
      const int half = num >> 1;
      if (this->Compare(base[offset + half], value) < 0) {
        offset += half;
        greaterEqual = 1;
      } else {
        greaterEqual = 0;
      }
      num -= half;
      if (half == 0)
        break;
    }
    return offset + greaterEqual;
  }

  int Search_LastLess(const T* base, int num, const T& value) const {
    int offset = 0;
    while (num > 0) {
      const int half = num >> 1;
      if (this->Compare(base[offset + half], value) < 0)
        offset += half;
      num -= half;
      if (half == 0)
        break;
    }
    return offset;
  }

  int Search_LastLessEqual(const T* base, int num, const T& value) const {
    int offset = 0;
    while (num > 0) {
      const int half = num >> 1;
      if (this->Compare(base[offset + half], value) <= 0)
        offset += half;
      num -= half;
      if (half == 0)
        break;
    }
    return offset;
  }
};

template class idSearch_BinaryDefault<int>;
template class idSearch_Binary<int, idSearch_BinaryDefault<int>>;

template <>
std::size_t idFile::WriteBig<idVec4>(const idVec4& value);

namespace {

constexpr std::uint32_t kGlassModelMagic = 55332167u;

std::uint32_t ByteSwap32(std::uint32_t value) {
  return ((value & 0x000000ffu) << 24) |
         ((value & 0x0000ff00u) << 8) |
         ((value & 0x00ff0000u) >> 8) |
         ((value & 0xff000000u) >> 24);
}

void WriteBigInt(idFile* file, int value) {
  const std::uint32_t big = ByteSwap32(static_cast<std::uint32_t>(value));
  file->Write(&big, sizeof(big));
}

void WriteNativeString(idFile* file, const char* text) {
  const int length = text != nullptr ? idStr::Length(text) : 0;
  file->Write(&length, sizeof(length));
  if (length > 0)
    file->Write(text, static_cast<std::size_t>(length));
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
      "flesh_darkangel", "blood_pool",   "ash",
      "flesh_mutant",  "thick_padding",  "player_apc_metal",
      "rickety_metal", "rickety_wood",   "sheet_metal",
      "porcelain",     "plaster",        "vent_metal",
      "electronics",   "oil_pipe",       "panzer_metal",
      "breakable_armor", "electric_shield"};
  const int index = static_cast<int>(type);
  return index >= 0 && index < static_cast<int>(std::size(names))
             ? names[index]
             : "none";
}

int MakeEdge(int first, int second) {
  const int low = std::min(first, second);
  const int high = std::max(first, second);
  return low | (high << 16);
}

void InsertUniqueEdge(idList<int, 5>& edges, int edge) {
  idSearch_Binary<int, idSearch_BinaryDefault<int>> search;
  const int insertion =
      search.Search_FirstGreaterEqual(edges.list, edges.num, edge);
  if (insertion >= edges.num || edges[insertion] != edge)
    edges.Insert(edge, insertion);
}

bool ContainsEdge(const idList<int, 5>& edges, int edge) {
  idSearch_Binary<int, idSearch_BinaryDefault<int>> search;
  return search.Search(edges.list, edges.num, edge) >= 0;
}

bool IsMerged(const idBitArray& merged, int index) {
  return index >= 0 && static_cast<std::size_t>(index) < merged.bits &&
         (merged.buffer[index >> 3] & (1u << (index & 7))) != 0;
}

void SetMerged(idBitArray& merged, int index) {
  if (index >= 0 && static_cast<std::size_t>(index) < merged.bits)
    merged.buffer[index >> 3] |= static_cast<unsigned char>(1u << (index & 7));
}

} // namespace

int idGlassModelGenerator::FindVertexId(const idHashIndex& hash,
                                        const idVec3& vertex,
                                        idDrawVert* vertices,
                                        int numVertices) {
  const int key = static_cast<int>(vertex.x) + static_cast<int>(vertex.y) +
                  static_cast<int>(vertex.z);
  for (int index = hash.First(key); index != -1; index = hash.Next(index)) {
    if (index >= numVertices)
      continue;
    const idVec3& candidate = vertices[index].xyz;
    if (std::fabs(vertex.x - candidate.x) <= 0.0001f &&
        std::fabs(vertex.y - candidate.y) <= 0.0001f &&
        std::fabs(vertex.z - candidate.z) <= 0.0001f) {
      return index;
    }
  }
  return -1;
}

const idMaterial* GetSurfaceTypeMaterial(const idMaterial* material) {
  surfTypes_t surfaceType = SURFTYPE_NONE;
  if (material != nullptr &&
      material->parmBlock.SetsRenderParm(rp_surfaceType.Get())) {
    surfaceType = material->GetSurfaceType();
    if (surfaceType == SURFTYPE_INVALID)
      surfaceType = SURFTYPE_NONE;
  }

  idStr materialName;
  materialName.Format("textures/common/surftype/surf_%s",
                      SurfaceTypeSuffix(surfaceType));
  materialName.ToLower();
  return static_cast<const idMaterial*>(
      idMaterial::resourceList.FindWithInheritance(materialName.data, false,
                                                   true));
}

void idGlassModelGenerator::FreeSurfaces() {
  for (int index = 0; index < glassSurfaces.num; ++index) {
    idGlassModelSurface& surface = glassSurfaces[index];
    if (surface.geometry != nullptr && !surface.geometryIsReference &&
        (surface.referenceMask & 1) == 0) {
      delete surface.geometry;
      surface.geometry = nullptr;
    }
  }
  glassSurfaces.ClearFree();
}

void idGlassModelGenerator::FreeSourceSurfaces() {
  for (int index = 0; index < sourceSurfaces.num; ++index) {
    delete sourceSurfaces[index].triangles;
    sourceSurfaces[index].triangles = nullptr;
  }
  sourceSurfaces.ClearFree();
  decls.ClearFree();
}

idGlassModelGenerator::idGlassModelGenerator() {
  decls.granularity = 5;
  sourceSurfaces.granularity = 5;
  glassSurfaces.granularity = 5;
  sourceTimestamp = -1;
}

idGlassModelGenerator::~idGlassModelGenerator() {
  FreeSurfaces();
  FreeSourceSurfaces();
}

int idGlassModelGenerator::AddDecl(const char* declName) {
  if (declName == nullptr)
    return -1;
  idStr decl(declName);
  if (decl.IsEmpty())
    return -1;
  decl.ToLower();
  for (int index = 0; index < decls.num; ++index) {
    if (decls[index] == decl)
      return index;
  }
  return decls.Append(decl);
}

void idGlassModelGenerator::AddModel(const idStaticModel* model,
                                     const char* declName) {
  std::uint32_t timestamp =
      fileSystem->GetTimestamp(model->name.str, false);
  if (sourceTimestamp == -1 && timestamp != 0xffffffffu)
    timestamp = 0xffffffffu;
  sourceTimestamp = static_cast<int>(timestamp);

  for (int index = 0; index < model->surfaces.num; ++index) {
    const idStaticModelSurface& surface = model->surfaces[index];
    if (surface.material->Coverage() == MC_INVISIBLE)
      continue;
    glassSourceSurface_t sourceSurface{};
    sourceSurface.triangles = surface.geometry->CreateStandardTriangles();
    sourceSurface.material = surface.material;
    sourceSurface.declIndex = AddDecl(declName);
    sourceSurfaces.Append(sourceSurface);
  }
}

void idGlassModelGenerator::GenerateIslands(
    const glassSourceSurface_t& sourceSurface) {
  idTriangles* const source = sourceSurface.triangles;
  const int numVertices = source->numVerts;
  const int numTriangles = source->numIndexes / 3;

  idList<int, 5> edges;
  idList<int, 5> island;
  idList<int, 5> remap;
  idList<unsigned short, 5> indices;
  idList<idDrawVert, 5> vertices;
  edges.granularity = 5;
  island.granularity = 5;
  remap.granularity = 5;
  indices.granularity = 5;
  vertices.granularity = 5;
  edges.ResizeForPort(3 * numTriangles);
  island.ResizeForPort(numTriangles);
  remap.ResizeForPort(3 * numTriangles);
  indices.ResizeForPort(3 * numTriangles);
  vertices.ResizeForPort(numVertices);

  idBitArray merged;
  merged.Alloc(numTriangles);
  if (merged.buffer != nullptr)
    std::memset(merged.buffer, 0, (merged.bits + 7) >> 3);

  idHashIndex vertexHash(4096, 1024);
  for (int index = 0; index < numVertices; ++index) {
    if (FindVertexId(vertexHash, source->verts[index].xyz, source->verts,
                     numVertices) < 0) {
      const idVec3& position = source->verts[index].xyz;
      const int key = static_cast<int>(position.x) +
                      static_cast<int>(position.y) +
                      static_cast<int>(position.z);
      vertexHash.Add(key, index);
    }
  }

  for (int seed = 0; seed < numTriangles; ++seed) {
    if (IsMerged(merged, seed))
      continue;

    edges.Clear();
    island.Clear();
    remap.Clear();
    indices.Clear();
    vertices.Clear();

    island.Append(seed);
    SetMerged(merged, seed);
    const unsigned short* seedIndexes = source->indexes + seed * 3;
    const int seedVertex[3] = {
        FindVertexId(vertexHash, source->verts[seedIndexes[0]].xyz,
                     source->verts, numVertices),
        FindVertexId(vertexHash, source->verts[seedIndexes[1]].xyz,
                     source->verts, numVertices),
        FindVertexId(vertexHash, source->verts[seedIndexes[2]].xyz,
                     source->verts, numVertices)};
    InsertUniqueEdge(edges, MakeEdge(seedVertex[0], seedVertex[1]));
    InsertUniqueEdge(edges, MakeEdge(seedVertex[0], seedVertex[2]));
    InsertUniqueEdge(edges, MakeEdge(seedVertex[1], seedVertex[2]));

    bool restartScan = true;
    while (restartScan) {
      restartScan = false;
      for (int triangle = 0; triangle < numTriangles; ++triangle) {
        if (IsMerged(merged, triangle))
          continue;
        const unsigned short* triangleIndexes = source->indexes + triangle * 3;
        const int welded[3] = {
            FindVertexId(vertexHash, source->verts[triangleIndexes[0]].xyz,
                         source->verts, numVertices),
            FindVertexId(vertexHash, source->verts[triangleIndexes[1]].xyz,
                         source->verts, numVertices),
            FindVertexId(vertexHash, source->verts[triangleIndexes[2]].xyz,
                         source->verts, numVertices)};
        const int triangleEdges[3] = {
            MakeEdge(welded[0], welded[1]), MakeEdge(welded[0], welded[2]),
            MakeEdge(welded[1], welded[2])};
        if (!ContainsEdge(edges, triangleEdges[0]) &&
            !ContainsEdge(edges, triangleEdges[1]) &&
            !ContainsEdge(edges, triangleEdges[2])) {
          continue;
        }
        SetMerged(merged, triangle);
        InsertUniqueEdge(edges, triangleEdges[0]);
        InsertUniqueEdge(edges, triangleEdges[1]);
        InsertUniqueEdge(edges, triangleEdges[2]);
        island.Append(triangle);
        restartScan = true;
        break;
      }
    }

    for (int islandIndex = 0; islandIndex < island.num; ++islandIndex) {
      const int triangle = island[islandIndex];
      for (int corner = 0; corner < 3; ++corner) {
        const int sourceVertex = source->indexes[triangle * 3 + corner];
        int localVertex = -1;
        for (int index = 0; index < remap.num; ++index) {
          if (remap[index] == sourceVertex) {
            localVertex = index;
            break;
          }
        }
        if (localVertex < 0) {
          localVertex = remap.Append(sourceVertex);
          vertices.Append(source->verts[sourceVertex]);
        }
        indices.Append(static_cast<unsigned short>(localVertex));
      }
    }

    idTriangles* geometry =
        new (std::nothrow) idTriangles(vertices.num, indices.num);
    if (geometry == nullptr)
      continue;
    if (vertices.num > 0)
      std::memcpy(geometry->verts, vertices.list,
                  sizeof(idDrawVert) * static_cast<std::size_t>(vertices.num));
    if (indices.num > 0)
      std::memcpy(geometry->indexes, indices.list,
                  sizeof(unsigned short) *
                      static_cast<std::size_t>(indices.num));
    geometry->bounds = geometry->BoundsFromVertScan();

    idGlassModelSurface glassSurface{};
    glassSurface.material = sourceSurface.material;
    glassSurface.geometry = geometry;
    glassSurface.boundSphere.origin =
        (geometry->bounds[0] + geometry->bounds[1]) * 0.5f;
    glassSurface.boundSphere.radius =
        geometry->bounds.GetRadius(glassSurface.boundSphere.origin);
    glassSurface.declIndex = sourceSurface.declIndex;
    glassSurfaces.Append(glassSurface);
  }
}

bool idGlassModelGenerator::Generate() {
  FreeSurfaces();
  for (int index = 0; index < sourceSurfaces.num; ++index)
    GenerateIslands(sourceSurfaces[index]);
  return true;
}

bool idGlassModelGenerator::WriteBinary(const char* filename) {
  idStr baseName(filename);
  baseName.StripFileExtension();
  const bool comboWorldGlass =
      idStr::Find(filename, "/_combo/worldglass.gmodel", false, 0, -1) != -1;
  idStr collisionBaseName;
  if (comboWorldGlass) {
    collisionBaseName = baseName;
    collisionBaseName.StripFilename();
    collisionBaseName.StripTrailing("/_combo");
    collisionBaseName.Append("/_fracture/part_");
  } else {
    collisionBaseName.Format("generated/_fracture/%s/part_", baseName.data);
  }

  for (int index = 0; index < glassSurfaces.num; ++index) {
    const idGlassModelSurface& glassSurface = glassSurfaces[index];
    idTriangles* collisionTriangles =
        glassSurface.geometry->CreateStandardTriangles();
    for (int triangle = 0; triangle < collisionTriangles->numIndexes;
         triangle += 3) {
      std::swap(collisionTriangles->indexes[triangle],
                collisionTriangles->indexes[triangle + 2]);
    }
    collisionTriangles->MergeTriangles(glassSurface.geometry);

    idRenderModelSurface collisionSurface{};
    collisionSurface.geometry = collisionTriangles;
    collisionSurface.material = GetSurfaceTypeMaterial(glassSurface.material);
    collisionSurface.geometryIsReference = true;
    collisionSurface.referenceMask = 1;
    const idRenderModelSurface* surfaces[] = {&collisionSurface};
    idStr meshName;
    meshName.Format("%s%d", collisionBaseName.data, index);
    BuildGlassCollisionModel(meshName.data, surfaces, 1, true);
    delete collisionTriangles;
  }

  for (int index = 0; index < glassSurfaces.num; ++index) {
    idTriangles* distributionTriangles =
        glassSurfaces[index].geometry->CreateStandardTriangles();
    idStr meshName;
    meshName.Format("%s%d", collisionBaseName.data, index);
    idParticleMeshDistributionGenerator::GenerateFromTriangles(
        meshName, distributionTriangles);
    delete distributionTriangles;
  }

  idFileLocal file(fileSystem->OpenFileWrite(filename));
  if (file.file == nullptr) {
    idLib::Warning("Could not open %s", filename);
    return false;
  }
  idLib::Printf("Writing %s\n", filename);
  WriteBigInt(file.file, static_cast<int>(kGlassModelMagic));
  WriteBigInt(file.file, sourceTimestamp);
  WriteNativeString(file.file, collisionBaseName.data);
  WriteBigInt(file.file, decls.num);
  for (int index = 0; index < decls.num; ++index)
    WriteNativeString(file.file, decls[index].data);
  WriteBigInt(file.file, glassSurfaces.num);
  for (int index = 0; index < glassSurfaces.num; ++index) {
    const idGlassModelSurface& surface = glassSurfaces[index];
    WriteNativeString(file.file,
                      surface.material != nullptr ? surface.material->name.str
                                                  : nullptr);
    file.file->WriteBig(
        *reinterpret_cast<const idVec4*>(&surface.boundSphere));
    WriteBigInt(file.file, surface.declIndex);
    surface.geometry->WriteToFile(file.file);
  }
  return true;
}
