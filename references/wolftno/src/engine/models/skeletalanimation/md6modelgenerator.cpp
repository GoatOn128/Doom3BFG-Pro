#include "engine/models/skeletalanimation/md6modelgenerator.h"

#include "engine/models/skeletalanimation/jobs/md6blend.h"
#include "engine/models/skeletalanimation/md6skel.h"
#include "engine/compilers/meshoptimizer/meshoptimizer.h"
#include "engine/models/static/staticmodel_lwo.h"
#include "engine/renderer/declmaterial.h"
#include "engine/renderer/triangles.h"
#include "shared/idlib/csystems/cvarsystemlocal.h"
#include "shared/idlib/filesystem/filesystemlocal.h"
#include "shared/idlib/geometry/drawvert.h"
#include "shared/idlib/lib.h"
#include "shared/idlib/text/lexer.h"

#include <algorithm>
#include <array>
#include <cmath>
#include <cstdio>
#include <cstring>
#include <limits>
#include <new>
#include <queue>
#include <vector>

idCVar md6_generateTexcoordRebase(
    "md6_generateTexcoordRebase", "1", CVAR_BOOL,
    "rebase wrapping MD6 texture coordinates for generic materials");

extern idCVar feature_hq;

namespace {

template <typename T, int Tag>
void InitializeGeneratorList(idList<T, Tag>& list) {
  list.granularity = 5;
  list.memTag = 0;
}

int ClampMorphByte(float value) {
  return std::max(0, std::min(255, static_cast<int>(value * 4.0f + 128.0f)));
}

constexpr std::uint32_t kMD6ModelMagic = 0x28364d4du;
constexpr unsigned int kMorphVMapId =
    (static_cast<unsigned int>('M') << 24u) |
    (static_cast<unsigned int>('O') << 16u) |
    (static_cast<unsigned int>('R') << 8u) | static_cast<unsigned int>('F');

idMD6GeneratorSkeletonLoader g_generatorSkeletonLoader = nullptr;

std::uint16_t ByteSwap16(std::uint16_t value) {
  return static_cast<std::uint16_t>((value << 8u) | (value >> 8u));
}

std::uint32_t ByteSwap32(std::uint32_t value) {
  return (value << 24u) | ((value << 8u) & 0x00ff0000u) |
         ((value >> 8u) & 0x0000ff00u) | (value >> 24u);
}

void WriteBigUInt(idFile* file, std::uint32_t value) {
  value = ByteSwap32(value);
  file->Write(&value, sizeof(value));
}

void WriteBigInt(idFile* file, int value) {
  WriteBigUInt(file, static_cast<std::uint32_t>(value));
}

void WriteBigFloat(idFile* file, float value) {
  std::uint32_t word = 0;
  std::memcpy(&word, &value, sizeof(word));
  WriteBigUInt(file, word);
}

void WriteBigVec2(idFile* file, const idVec2& value) {
  WriteBigFloat(file, value.x);
  WriteBigFloat(file, value.y);
}

void WriteBigVec3(idFile* file, const idVec3& value) {
  WriteBigFloat(file, value.x);
  WriteBigFloat(file, value.y);
  WriteBigFloat(file, value.z);
}

void WriteBigBounds(idFile* file, const idBounds& value) {
  WriteBigVec3(file, value.b[0]);
  WriteBigVec3(file, value.b[1]);
}

void WriteNativeString(idFile* file, const char* value) {
  const int length = value != nullptr ? idStr::Length(value) : 0;
  file->Write(&length, sizeof(length));
  if (length > 0)
    file->Write(value, static_cast<std::size_t>(length));
}

std::uint8_t PackDirection(float value) {
  const int packed = static_cast<int>((value + 1.0f) * 127.5f + 0.5f);
  return static_cast<std::uint8_t>(std::max(0, std::min(255, packed)));
}

void WriteBigDrawVert(idFile* file, const idDrawVert& value) {
  WriteBigVec3(file, value.xyz);
  WriteBigVec2(file, value.st);
  file->Write(value.normal, sizeof(value.normal));
  file->Write(value.tangent, sizeof(value.tangent));
  file->Write(value.color, sizeof(value.color));
}

void ComputeMeshJointRange(idProcessMesh& mesh) {
  if (mesh.vertexes.num <= 0) {
    mesh.minJoint = 0;
    mesh.maxJoint = 0;
    return;
  }
  int minimum = 255;
  int maximum = 0;
  for (int vertexIndex = 0; vertexIndex < mesh.vertexes.num; ++vertexIndex) {
    const processVert_t& vertex = mesh.vertexes[vertexIndex];
    for (int weight = 0; weight < 4; ++weight) {
      minimum = std::min(minimum, static_cast<int>(vertex.jointIdxs[weight]));
      maximum = std::max(maximum, static_cast<int>(vertex.jointIdxs[weight]));
    }
  }
  mesh.minJoint = minimum & 0xf0;
  mesh.maxJoint = maximum;
}

void CopySplitMetadata(const idProcessMesh& source, idProcessMesh& output) {
  output.uvs.SetNum(source.uvs.num);
  for (int index = 0; index < source.uvs.num; ++index)
    output.uvs[index].slot = source.uvs[index].slot;
  output.blendShapes.SetNum(source.blendShapes.num);
  for (int index = 0; index < source.blendShapes.num; ++index) {
    output.blendShapes[index].name = source.blendShapes[index].name;
    output.blendShapes[index].userchannelExpression =
        source.blendShapes[index].userchannelExpression;
    output.blendShapes[index].multiplier =
        source.blendShapes[index].multiplier;
  }
  output.hashST = source.hashST;
}

void AppendRemappedVertex(const idProcessMesh& source, int sourceVertex,
                          idProcessMesh& output,
                          idHeapArray<int>& vertexRemap) {
  if (sourceVertex < 0 || sourceVertex >= source.vertexes.num)
    return;
  int& remapped = vertexRemap.buffer[sourceVertex];
  if (remapped >= 0) {
    output.indexes.Append(remapped);
    return;
  }
  remapped = output.vertexes.num;
  output.vertexes.Append(source.vertexes[sourceVertex]);
  for (int setIndex = 0; setIndex < source.uvs.num; ++setIndex) {
    if (sourceVertex < source.uvs[setIndex].st.num)
      output.uvs[setIndex].st.Append(source.uvs[setIndex].st[sourceVertex]);
  }
  for (int shapeIndex = 0; shapeIndex < source.blendShapes.num;
       ++shapeIndex) {
    const blendShape_t& inputShape = source.blendShapes[shapeIndex];
    blendShape_t& outputShape = output.blendShapes[shapeIndex];
    const int first = 3 * sourceVertex;
    for (int component = 0; component < 3; ++component) {
      outputShape.vertexOffsets.Append(
          first + component < inputShape.vertexOffsets.num
              ? inputShape.vertexOffsets[first + component]
              : static_cast<std::uint8_t>(128));
    }
  }
  output.indexes.Append(remapped);
}

void FinishSplitMesh(int meshIndex, const idProcessMesh& source,
                     idProcessMesh& output,
                     const idHeapArray<int>& vertexRemap) {
  RemapMorphSets(meshIndex, source, output, vertexRemap);
  ComputeMeshJointRange(output);
  output.hashST = source.hashST;
}

bool TokenEquals(const idToken& token, const char* text) {
  return idStr::Icmp(token.data, text) == 0;
}

bool ReadStringToken(idLexer& parser, idStr& result) {
  idToken token;
  if (!parser.ExpectAnyToken(token) ||
      (token.type != TT_STRING && token.type != TT_NAME)) {
    return false;
  }
  result = token;
  return true;
}

} // namespace

bool MatchMorphVerts(
    const idList<vertexMorphOffset_t, 5>& vertexMorphOffsets,
    idList<processVert_t, 5>& vertexes) {
  int matched = 0;
  for (int vertexIndex = 0; vertexIndex < vertexes.num; ++vertexIndex) {
    processVert_t& vertex = vertexes[vertexIndex];
    vertex.morph[3] = 0;
    float closestDistance = std::numeric_limits<float>::max();
    int closest = -1;
    for (int morphIndex = 0; morphIndex < vertexMorphOffsets.num;
         ++morphIndex) {
      const idVec3 delta = {
          vertexMorphOffsets[morphIndex].xyz.x - vertex.xyz.x,
          vertexMorphOffsets[morphIndex].xyz.y - vertex.xyz.y,
          vertexMorphOffsets[morphIndex].xyz.z - vertex.xyz.z};
      const float distance = delta.x * delta.x + delta.y * delta.y +
                             delta.z * delta.z;
      if (distance < closestDistance) {
        closestDistance = distance;
        closest = morphIndex;
      }
    }
    if (closest >= 0 && closestDistance <= 0.1f) {
      const idVec3& offset = vertexMorphOffsets[closest].offset;
      vertex.morph[0] = static_cast<std::uint8_t>(ClampMorphByte(offset.x));
      vertex.morph[1] = static_cast<std::uint8_t>(ClampMorphByte(offset.y));
      vertex.morph[2] = static_cast<std::uint8_t>(ClampMorphByte(offset.z));
      ++matched;
    } else {
      vertex.morph[0] = 128;
      vertex.morph[1] = 128;
      vertex.morph[2] = 128;
    }
  }
  return matched == vertexes.num;
}

void idMD6BinaryGenerator::CreateHashST() {
  char number[32]{};
  for (int surfaceIndex = 0; surfaceIndex < surfaces.num; ++surfaceIndex) {
    idProcessSurface* surface = surfaces[surfaceIndex];
    if (surface == nullptr)
      continue;
    idStr hashName;
    std::snprintf(number, sizeof(number), "%d", surface->mesh.uvs.num);
    hashName = number;
    for (int setIndex = 0; setIndex < surface->mesh.uvs.num; ++setIndex) {
      std::snprintf(number, sizeof(number), "_%u",
                    static_cast<unsigned>(surface->mesh.uvs[setIndex].slot));
      hashName += number;
    }
    surface->mesh.hashST = surface->mesh.uvs.num > 0
                               ? static_cast<int>(idStr::Hash(hashName.data))
                               : 0;
  }
}

void AppendMorphSets(idMD6Morph& destination, const idMD6Morph& source) {
  for (int index = 0; index < source.sets.num; ++index) {
    idMD6MorphSet& output = destination.AllocSet();
    output = source.sets[index];
  }
}

blendShape_t::blendShape_t() {
  InitializeGeneratorList(vertexOffsets);
}

void RemapMorphSets(int meshIndex, const idProcessMesh& sourceMesh,
                    idProcessMesh& outputMesh,
                    const idHeapArray<int>& vertexRemap) {
  for (int setIndex = 0; setIndex < sourceMesh.morphSets.sets.num;
       ++setIndex) {
    const idMD6MorphSet& sourceSet = sourceMesh.morphSets.sets[setIndex];
    idList<std::uint16_t, 5> indices;
    idList<std::uint8_t, 5> direction;
    for (int index = 0; index < sourceSet.indices.num; ++index) {
      const unsigned int sourceVertex = sourceSet.indices[index];
      if (sourceVertex >= vertexRemap.num)
        continue;
      const int remapped = vertexRemap.buffer[sourceVertex];
      if (remapped < 0 || remapped > 0xffff)
        continue;
      indices.Append(static_cast<std::uint16_t>(remapped));
      if (sourceSet.direction.num >= 3 * static_cast<int>(sourceVertex + 1)) {
        direction.Append(sourceSet.direction[3 * sourceVertex]);
        direction.Append(sourceSet.direction[3 * sourceVertex + 1]);
        direction.Append(sourceSet.direction[3 * sourceVertex + 2]);
      }
    }
    if (indices.num == 0)
      continue;
    idMD6MorphSet& output = outputMesh.morphSets.AllocSet();
    output.meshIndex = meshIndex;
    output.groupIndex = sourceSet.groupIndex;
    output.type = sourceSet.type;
    output.pos = sourceSet.pos;
    output.size = sourceSet.size;
    output.axis = sourceSet.axis;
    output.indices.SetNum(indices.num);
    for (int index = 0; index < indices.num; ++index)
      output.indices[index] = indices[index];
    output.direction.SetNum(direction.num);
    for (int index = 0; index < direction.num; ++index)
      output.direction[index] = direction[index];
  }
}

void SetupJointRemap(const idMD6Skel* skeleton,
                     const idArray<bool, 256>& usedJoints,
                     bool renumberBreadthFirst,
                     idList<std::uint8_t, 37>& jointRemap) {
  const int numJoints =
      skeleton != nullptr && skeleton->data != nullptr
          ? std::min<int>(skeleton->data->numJoints, 256)
          : 0;
  const int paddedJoints = (numJoints + 7) & ~7;
  jointRemap.SetNum(paddedJoints, static_cast<std::uint8_t>(255));
  if (numJoints == 0)
    return;

  bool required[256]{};
  for (int joint = 0; joint < numJoints; ++joint)
    required[joint] = usedJoints[joint];
  const auto* parents = reinterpret_cast<const std::int16_t*>(
      reinterpret_cast<const std::uint8_t*>(skeleton->data) +
      skeleton->data->parentTblOffset);
  for (int joint = numJoints - 1; joint >= 0; --joint) {
    if (!required[joint])
      continue;
    int parent = parents[joint];
    int guard = 0;
    while (parent >= 0 && parent < numJoints && guard++ < numJoints) {
      required[parent] = true;
      const int next = parents[parent];
      if (next == parent)
        break;
      parent = next;
    }
  }

  int nextJoint = 0;
  for (int joint = 0; joint < numJoints; ++joint) {
    if (required[joint]) {
      jointRemap[joint] = static_cast<std::uint8_t>(
          renumberBreadthFirst ? nextJoint++ : joint);
    }
  }
}

idProcessMesh::idProcessMesh() {
  InitializeGeneratorList(vertexes);
  InitializeGeneratorList(uvs);
  InitializeGeneratorList(indexes);
  InitializeGeneratorList(faceMaterialIndex);
  InitializeGeneratorList(blendShapes);
  InitializeGeneratorList(vertexRemap);
  InitializeGeneratorList(morphSets.sets);
  minJoint = 0;
  maxJoint = 255;
  hashST = 0;
}

idProcessMesh::idProcessMesh(const idProcessMesh& other) : idProcessMesh() {
  *this = other;
}

idProcessMesh& idProcessMesh::operator=(const idProcessMesh& other) {
  if (this == &other)
    return *this;
  vertexes = other.vertexes;
  uvs = other.uvs;
  indexes = other.indexes;
  faceMaterialIndex = other.faceMaterialIndex;
  blendShapes = other.blendShapes;
  vertexRemap = other.vertexRemap;
  morphSets.sets = other.morphSets.sets;
  minJoint = other.minJoint;
  maxJoint = other.maxJoint;
  hashST = other.hashST;
  return *this;
}

idProcessSurface* idProcessSurface::DuplicateShallow() const {
  idProcessSurface* duplicate = new (std::nothrow) idProcessSurface;
  if (duplicate == nullptr)
    return nullptr;
  duplicate->name = name;
  duplicate->material = material;
  duplicate->discrete = discrete;
  duplicate->allowSkinRemapping = allowSkinRemapping;
  duplicate->allowGenericMaterial = allowGenericMaterial;
  duplicate->numTexCoordSets = numTexCoordSets;
  return duplicate;
}

void SetMD6GeneratorSkeletonLoader(idMD6GeneratorSkeletonLoader loader) {
  g_generatorSkeletonLoader = loader;
}

void idMD6BinaryGenerator::LoadMorphMap(
    const char* fileName, const idVec3& offset,
    idList<vertexMorphOffset_t, 5>& vertexMorphOffsets) {
  if (fileName == nullptr || *fileName == '\0')
    return;
  unsigned int failId = 0;
  int failPosition = 0;
  st_lwObject* object = lwGetObject(fileName, 0.0f, &failId, &failPosition);
  if (object == nullptr)
    return;
  if (object->layer == nullptr) {
    lwFreeObject(object);
    return;
  }

  st_lwLayer* layer = object->layer;
  for (int pointIndex = 0; pointIndex < layer->point.count; ++pointIndex) {
    if (layer->point.ptVMaps == nullptr)
      continue;
    const st_lwVMapList& maps = layer->point.ptVMaps[pointIndex];
    for (int mapIndex = 0; mapIndex < maps.nvmaps; ++mapIndex) {
      const st_lwVMapPt& mapPoint = maps.vm[mapIndex];
      const st_lwVMap* map = mapPoint.vmap;
      if (map == nullptr || map->type != kMorphVMapId || map->val == nullptr ||
          mapPoint.index < 0 || mapPoint.index >= map->nverts) {
        continue;
      }
      const float* value = map->val[mapPoint.index];
      if (value == nullptr)
        continue;
      vertexMorphOffset_t entry{};
      const st_lwPoint& point = layer->point.pt[pointIndex];
      entry.xyz = {point.pos[0] + offset.x, point.pos[2] + offset.y,
                   point.pos[1] + offset.z};
      entry.offset = {value[0], value[2], value[1]};
      vertexMorphOffsets.Append(entry);
    }
  }
  lwFreeObject(object);
}

bool ProcessMeshRebaseTexcoords(idProcessMesh& mesh) {
  const int numTriangles = mesh.indexes.num / 3;
  if (numTriangles <= 0 || mesh.uvs.num <= 0)
    return true;

  std::vector<std::vector<int>> adjacent(
      static_cast<std::size_t>(numTriangles));
  struct edge_t {
    int a;
    int b;
    int triangle;
  };
  std::vector<edge_t> edges;
  edges.reserve(static_cast<std::size_t>(mesh.indexes.num));
  for (int triangle = 0; triangle < numTriangles; ++triangle) {
    for (int edge = 0; edge < 3; ++edge) {
      int a = mesh.indexes[3 * triangle + edge];
      int b = mesh.indexes[3 * triangle + (edge + 1) % 3];
      if (a > b)
        std::swap(a, b);
      auto previous = std::find_if(
          edges.begin(), edges.end(), [a, b](const edge_t& candidate) {
            return candidate.a == a && candidate.b == b;
          });
      if (previous == edges.end()) {
        edges.push_back({a, b, triangle});
      } else {
        adjacent[static_cast<std::size_t>(triangle)].push_back(
            previous->triangle);
        adjacent[static_cast<std::size_t>(previous->triangle)].push_back(
            triangle);
      }
    }
  }

  std::vector<int> island(static_cast<std::size_t>(numTriangles), -1);
  int numIslands = 0;
  for (int first = 0; first < numTriangles; ++first) {
    if (island[static_cast<std::size_t>(first)] >= 0)
      continue;
    std::queue<int> pending;
    pending.push(first);
    island[static_cast<std::size_t>(first)] = numIslands;
    while (!pending.empty()) {
      const int triangle = pending.front();
      pending.pop();
      for (int next : adjacent[static_cast<std::size_t>(triangle)]) {
        if (island[static_cast<std::size_t>(next)] >= 0)
          continue;
        island[static_cast<std::size_t>(next)] = numIslands;
        pending.push(next);
      }
    }
    ++numIslands;
  }

  struct bounds2_t {
    float minX{std::numeric_limits<float>::max()};
    float minY{std::numeric_limits<float>::max()};
    float maxX{-std::numeric_limits<float>::max()};
    float maxY{-std::numeric_limits<float>::max()};
  };
  std::vector<bounds2_t> bounds(
      static_cast<std::size_t>(numIslands * mesh.uvs.num));
  for (int triangle = 0; triangle < numTriangles; ++triangle) {
    for (int setIndex = 0; setIndex < mesh.uvs.num; ++setIndex) {
      bounds2_t& area =
          bounds[static_cast<std::size_t>(island[triangle] * mesh.uvs.num +
                                          setIndex)];
      for (int corner = 0; corner < 3; ++corner) {
        const int vertex = mesh.indexes[3 * triangle + corner];
        if (vertex < 0 || vertex >= mesh.uvs[setIndex].st.num)
          return false;
        const idVec2& st = mesh.uvs[setIndex].st[vertex];
        area.minX = std::min(area.minX, st.x);
        area.minY = std::min(area.minY, st.y);
        area.maxX = std::max(area.maxX, st.x);
        area.maxY = std::max(area.maxY, st.y);
      }
    }
  }

  constexpr float epsilon = 0.01f;
  for (const bounds2_t& area : bounds) {
    const float centerX = 0.5f * (area.minX + area.maxX);
    const float centerY = 0.5f * (area.minY + area.maxY);
    const float allowedMinX = std::floor(centerX) - epsilon;
    const float allowedMinY = std::floor(centerY) - epsilon;
    const float allowedMaxX = std::ceil(centerX) + epsilon;
    const float allowedMaxY = std::ceil(centerY) + epsilon;
    if (area.minX < allowedMinX || area.minY < allowedMinY ||
        area.maxX > allowedMaxX || area.maxY > allowedMaxY) {
      return false;
    }
  }

  for (int setIndex = 0; setIndex < mesh.uvs.num; ++setIndex) {
    for (int vertex = 0; vertex < mesh.uvs[setIndex].st.num; ++vertex) {
      idVec2& st = mesh.uvs[setIndex].st[vertex];
      if (st.x < -epsilon || st.x > 1.0f + epsilon)
        st.x -= std::floor(st.x);
      if (st.y < -epsilon || st.y > 1.0f + epsilon)
        st.y -= std::floor(st.y);
    }
  }
  return true;
}

void ParseMD6MeshToSurfaces(int md6Version, idLexer& parser,
                            int numOriginalJoints, bool hasExportedNormals,
                            const idList<std::int16_t, 5>& remapTable,
                            idList<idProcessSurface*, 5>& outputSurfaces) {
  idToken token;
  idStr meshName;
  idProcessMesh mesh;
  idList<const idMaterial*, 5> materials;

  if (!parser.ExpectTokenType(TT_PUNCTUATION, 46, token) ||
      !parser.ExpectTokenString("name") || !parser.ReadToken(token)) {
    return;
  }
  meshName = token;

  if (md6Version >= 5) {
    if (!parser.ExpectTokenString("shaders"))
      return;
    const int numMaterials = parser.ParseInt();
    if (numMaterials < 0 ||
        !parser.ExpectTokenType(TT_PUNCTUATION, 46, token)) {
      return;
    }
    for (int materialIndex = 0; materialIndex < numMaterials;
         ++materialIndex) {
      if (!parser.ExpectTokenString("shader"))
        return;
      parser.ParseInt();
      if (!parser.ReadToken(token))
        return;
      materials.Append(FindMaterialOrAutoMaterial(token.data));
    }
    if (!parser.ExpectTokenType(TT_PUNCTUATION, 47, token))
      return;
  } else {
    if (!parser.ExpectTokenString("shader") || !parser.ReadToken(token))
      return;
    materials.Append(FindMaterialOrAutoMaterial(token.data));
  }

  bool discrete = false;
  if (parser.CheckTokenString("discrete") ||
      parser.CheckTokenString("nomerge")) {
    discrete = true;
  }
  const bool allowSkinRemapping =
      !parser.CheckTokenString("noSkinRemapping");

  int numTexCoordSets = 1;
  mesh.uvs.SetNum(1);
  mesh.uvs[0].slot = 0;
  if (parser.CheckTokenString("uvsets")) {
    numTexCoordSets = parser.ParseInt();
    if (numTexCoordSets < 0 || !mesh.uvs.SetNum(numTexCoordSets) ||
        !parser.ExpectTokenType(TT_PUNCTUATION, 46, token)) {
      return;
    }
    for (int setIndex = 0; setIndex < numTexCoordSets; ++setIndex) {
      if (!parser.ExpectTokenString("name"))
        return;
      mesh.uvs[setIndex].slot = static_cast<std::uint8_t>(parser.ParseInt());
      if (!parser.ReadToken(token))
        return;
    }
    if (!parser.ExpectTokenType(TT_PUNCTUATION, 47, token))
      return;
  }

  if (!parser.ExpectTokenString("verts"))
    return;
  const int numVerts = parser.ParseInt();
  if (numVerts < 0 || !mesh.vertexes.SetNum(numVerts))
    return;
  std::memset(mesh.vertexes.list, 0,
              sizeof(processVert_t) * static_cast<std::size_t>(numVerts));
  for (int setIndex = 0; setIndex < numTexCoordSets; ++setIndex) {
    if (!mesh.uvs[setIndex].st.SetNum(numVerts))
      return;
    std::memset(mesh.uvs[setIndex].st.list, 0,
                sizeof(idVec2) * static_cast<std::size_t>(numVerts));
  }
  if (!parser.ExpectTokenType(TT_PUNCTUATION, 46, token))
    return;

  for (int vertexIndex = 0; vertexIndex < numVerts; ++vertexIndex) {
    processVert_t& vertex = mesh.vertexes[vertexIndex];
    if (!parser.ExpectTokenString("vert"))
      return;
    parser.ParseInt();
    if (!parser.Parse1DMatrix(3, &vertex.xyz.x, false))
      return;
    if (hasExportedNormals &&
        !parser.Parse1DMatrix(3, &vertex.normal.x, false)) {
      return;
    }
    for (int setIndex = 0; setIndex < numTexCoordSets; ++setIndex) {
      if (!parser.Parse1DMatrix(2, &mesh.uvs[setIndex].st[vertexIndex].x,
                                false)) {
        return;
      }
    }

    float indexesAndWeights[8]{};
    if (!parser.Parse1DMatrix(8, indexesAndWeights, false))
      return;
    for (int influence = 0; influence < 4; ++influence) {
      int sourceJoint = static_cast<int>(indexesAndWeights[influence]);
      sourceJoint = std::max(0, std::min(255, sourceJoint));
      if (sourceJoint >= numOriginalJoints ||
          sourceJoint >= remapTable.num) {
        parser.Error("Model has %d joints, but vertex uses joint %d",
                     numOriginalJoints, sourceJoint);
        return;
      }
      const int remappedJoint = remapTable[sourceJoint];
      if (remappedJoint < 0 || remappedJoint > 255) {
        parser.Error("Joint %d has invalid remap %d", sourceJoint,
                     remappedJoint);
        return;
      }
      vertex.jointIdxs[influence] =
          static_cast<std::uint8_t>(remappedJoint);
      vertex.floatWeights[influence] = indexesAndWeights[4 + influence];
      const int packedWeight = static_cast<int>(
          indexesAndWeights[4 + influence] * 255.0f);
      vertex.weights[influence] = static_cast<std::uint8_t>(
          std::max(0, std::min(255, packedWeight)));
    }

    for (int first = 0; first < 3; ++first) {
      for (int later = first + 1; later < 4; ++later) {
        if (vertex.weights[first] != 0 && vertex.weights[later] != 0 &&
            vertex.jointIdxs[first] == vertex.jointIdxs[later]) {
          vertex.floatWeights[first] += vertex.floatWeights[later];
          const int combined = static_cast<int>(
              vertex.floatWeights[first] * 255.0f);
          vertex.weights[first] = static_cast<std::uint8_t>(
              std::max(0, std::min(255, combined)));
          vertex.jointIdxs[later] = 255;
          vertex.weights[later] = 0;
          vertex.floatWeights[later] = 0.0f;
        }
      }
    }
    for (int first = 0; first < 3; ++first) {
      for (int later = first + 1; later < 4; ++later) {
        if (vertex.weights[later] <= vertex.weights[first])
          continue;
        std::swap(vertex.floatWeights[first], vertex.floatWeights[later]);
        std::swap(vertex.weights[first], vertex.weights[later]);
        std::swap(vertex.jointIdxs[first], vertex.jointIdxs[later]);
      }
    }
    vertex.weights[0] = static_cast<std::uint8_t>(
        255u - vertex.weights[1] - vertex.weights[2] - vertex.weights[3]);
  }
  if (!parser.ExpectTokenType(TT_PUNCTUATION, 47, token) ||
      !parser.ExpectTokenString("tris")) {
    return;
  }

  const int numTriangles = parser.ParseInt();
  if (numTriangles < 0 || !mesh.indexes.SetNum(3 * numTriangles) ||
      !parser.ExpectTokenType(TT_PUNCTUATION, 46, token)) {
    return;
  }
  for (int triangleIndex = 0; triangleIndex < numTriangles;
       ++triangleIndex) {
    if (!parser.ExpectTokenString("tri"))
      return;
    parser.ParseInt();
    const int materialIndex = md6Version >= 5 ? parser.ParseInt() : 0;
    if (materialIndex < 0 || materialIndex >= materials.num) {
      parser.Error("Triangle %d uses invalid shader %d", triangleIndex,
                   materialIndex);
      return;
    }
    mesh.faceMaterialIndex.Append(materialIndex);
    for (int corner = 0; corner < 3; ++corner) {
      const int index = parser.ParseInt();
      if (index < 0 || index >= numVerts) {
        parser.Error("Triangle %d uses invalid vertex %d", triangleIndex,
                     index);
        return;
      }
      mesh.indexes[3 * triangleIndex + corner] = index;
    }
  }
  if (!parser.ExpectTokenType(TT_PUNCTUATION, 47, token) ||
      !parser.ExpectTokenType(TT_PUNCTUATION, 47, token)) {
    return;
  }

  idTriangles triangles;
  triangles.generateNormals = !hasExportedNormals;
  triangles.AllocStaticTriSurfVerts(numVerts);
  triangles.AllocStaticTriSurfIndexes(3 * numTriangles);
  triangles.numVerts = numVerts;
  triangles.numIndexes = 3 * numTriangles;
  int baseTexCoordSet = -1;
  for (int setIndex = 0; setIndex < numTexCoordSets; ++setIndex) {
    if (mesh.uvs[setIndex].slot == 0) {
      baseTexCoordSet = setIndex;
      break;
    }
  }
  for (int vertexIndex = 0; vertexIndex < numVerts; ++vertexIndex) {
    idDrawVert& output = triangles.verts[vertexIndex];
    output = {};
    output.xyz = mesh.vertexes[vertexIndex].xyz;
    if (baseTexCoordSet >= 0)
      output.st = mesh.uvs[baseTexCoordSet].st[vertexIndex];
    if (hasExportedNormals) {
      output.normal[0] = PackDirection(mesh.vertexes[vertexIndex].normal.x);
      output.normal[1] = PackDirection(mesh.vertexes[vertexIndex].normal.y);
      output.normal[2] = PackDirection(mesh.vertexes[vertexIndex].normal.z);
    }
  }
  for (int index = 0; index < 3 * numTriangles; ++index)
    triangles.indexes[index] = static_cast<std::uint16_t>(mesh.indexes[index]);
  idTriangles::RangeCheckIndexes(triangles.indexes, triangles.numIndexes,
                                 triangles.numVerts);
  triangles.DeriveTangents();
  for (int vertexIndex = 0; vertexIndex < numVerts; ++vertexIndex) {
    const idDrawVert& input = triangles.verts[vertexIndex];
    processVert_t& output = mesh.vertexes[vertexIndex];
    output.normal = {
        static_cast<float>(input.normal[0]) * (2.0f / 255.0f) - 1.0f,
        static_cast<float>(input.normal[1]) * (2.0f / 255.0f) - 1.0f,
        static_cast<float>(input.normal[2]) * (2.0f / 255.0f) - 1.0f};
    output.tangent = {
        static_cast<float>(input.tangent[0]) * (2.0f / 255.0f) - 1.0f,
        static_cast<float>(input.tangent[1]) * (2.0f / 255.0f) - 1.0f,
        static_cast<float>(input.tangent[2]) * (2.0f / 255.0f) - 1.0f,
        (input.tangent[3] & 0x80u) != 0 ? 1.0f : -1.0f};
    const float normalLength = std::sqrt(
        std::max(output.normal.x * output.normal.x +
                     output.normal.y * output.normal.y +
                     output.normal.z * output.normal.z,
                 std::numeric_limits<float>::min()));
    output.normal *= 1.0f / normalLength;
    const float tangentLength = std::sqrt(
        std::max(output.tangent.x * output.tangent.x +
                     output.tangent.y * output.tangent.y +
                     output.tangent.z * output.tangent.z,
                 std::numeric_limits<float>::min()));
    output.tangent.x /= tangentLength;
    output.tangent.y /= tangentLength;
    output.tangent.z /= tangentLength;
  }

  outputSurfaces.ClearFree();
  const idDeclRenderParm* discreteParm =
      static_cast<const idDeclRenderParm*>(
          idDeclRenderParm::resourceList.FindWithInheritance(
              "discrete", true, true));
  for (int materialIndex = 0; materialIndex < materials.num;
       ++materialIndex) {
    idProcessSurface* surface = new (std::nothrow) idProcessSurface;
    if (surface == nullptr)
      continue;
    surface->name = meshName;
    surface->material = materials[materialIndex];
    surface->discrete =
        discrete ||
        (surface->material != nullptr && discreteParm != nullptr &&
         surface->material->GetInteger(discreteParm) != 0);
    surface->allowSkinRemapping = allowSkinRemapping;
    surface->allowGenericMaterial = true;
    surface->numTexCoordSets = numTexCoordSets;
    surface->mesh.uvs.SetNum(numTexCoordSets);
    for (int setIndex = 0; setIndex < numTexCoordSets; ++setIndex)
      surface->mesh.uvs[setIndex].slot = mesh.uvs[setIndex].slot;
    surface->mesh.vertexRemap.SetNum(numVerts, -1);

    for (int triangleIndex = 0; triangleIndex < numTriangles;
         ++triangleIndex) {
      if (mesh.faceMaterialIndex[triangleIndex] != materialIndex)
        continue;
      for (int corner = 0; corner < 3; ++corner) {
        const int sourceVertex = mesh.indexes[3 * triangleIndex + corner];
        int& destinationVertex = surface->mesh.vertexRemap[sourceVertex];
        if (destinationVertex < 0) {
          destinationVertex = surface->mesh.vertexes.num;
          surface->mesh.vertexes.Append(mesh.vertexes[sourceVertex]);
          for (int setIndex = 0; setIndex < numTexCoordSets; ++setIndex) {
            const idVec2 st = mesh.uvs[setIndex].st[sourceVertex];
            if (st.x < -0.01f || st.x > 1.01f || st.y < -0.01f ||
                st.y > 1.01f) {
              surface->allowGenericMaterial = false;
            }
            surface->mesh.uvs[setIndex].st.Append(st);
          }
        }
        surface->mesh.indexes.Append(destinationVertex);
      }
    }
    outputSurfaces.Append(surface);
  }
}

int SplitMeshForJointLimits(int meshIndexOffset, idProcessMesh& inputMesh,
                            int maxJointsPerMesh,
                            idList<idProcessMesh, 5>& outputMeshes) {
  struct triangleInfo_t {
    int triangle{};
    int firstJoint{255};
    int lastJoint{};
  };
  const int numTriangles = inputMesh.indexes.num / 3;
  if (numTriangles == 0)
    return -1;
  std::vector<triangleInfo_t> triangles;
  triangles.reserve(static_cast<std::size_t>(numTriangles));
  for (int triangle = 0; triangle < numTriangles; ++triangle) {
    triangleInfo_t info{};
    info.triangle = triangle;
    for (int corner = 0; corner < 3; ++corner) {
      const int vertexIndex = inputMesh.indexes[3 * triangle + corner];
      if (vertexIndex < 0 || vertexIndex >= inputMesh.vertexes.num)
        return triangle;
      const processVert_t& vertex = inputMesh.vertexes[vertexIndex];
      for (int weight = 0; weight < 4; ++weight) {
        info.firstJoint =
            std::min(info.firstJoint, static_cast<int>(vertex.jointIdxs[weight]));
        info.lastJoint =
            std::max(info.lastJoint, static_cast<int>(vertex.jointIdxs[weight]));
      }
    }
    if (info.lastJoint - info.firstJoint + 1 > maxJointsPerMesh)
      return triangle;
    triangles.push_back(info);
  }
  std::sort(triangles.begin(), triangles.end(),
            [](const triangleInfo_t& left, const triangleInfo_t& right) {
              if (left.firstJoint != right.firstJoint)
                return left.firstJoint < right.firstJoint;
              if (left.lastJoint != right.lastJoint)
                return left.lastJoint < right.lastJoint;
              return left.triangle < right.triangle;
            });

  std::size_t first = 0;
  while (first < triangles.size()) {
    std::size_t last = first + 1;
    while (last < triangles.size() &&
           triangles[last].lastJoint - triangles[first].firstJoint <
               maxJointsPerMesh) {
      ++last;
    }
    idProcessMesh output;
    CopySplitMetadata(inputMesh, output);
    idHeapArray<int> vertexRemap;
    vertexRemap.Alloc(static_cast<unsigned int>(inputMesh.vertexes.num));
    std::fill(vertexRemap.buffer,
              vertexRemap.buffer + inputMesh.vertexes.num, -1);
    for (std::size_t index = first; index < last; ++index) {
      const int triangle = triangles[index].triangle;
      for (int corner = 0; corner < 3; ++corner) {
        AppendRemappedVertex(inputMesh,
                             inputMesh.indexes[3 * triangle + corner], output,
                             vertexRemap);
      }
    }
    FinishSplitMesh(meshIndexOffset + outputMeshes.num, inputMesh, output,
                    vertexRemap);
    outputMeshes.Append(output);
    first = last;
  }
  return -1;
}

void SplitMeshForIndexLimit(int meshIndexOffset,
                            const idProcessMesh& inputMesh,
                            int maxIndexesPerMesh,
                            idList<idProcessMesh, 5>& outputMeshes) {
  outputMeshes.ClearFree();
  if (inputMesh.indexes.num <= 0)
    return;
  const int triangleLimit = std::max(3, maxIndexesPerMesh -
                                            maxIndexesPerMesh % 3);
  int firstIndex = 0;
  while (firstIndex < inputMesh.indexes.num) {
    idProcessMesh output;
    CopySplitMetadata(inputMesh, output);
    idHeapArray<int> vertexRemap;
    vertexRemap.Alloc(static_cast<unsigned int>(inputMesh.vertexes.num));
    std::fill(vertexRemap.buffer,
              vertexRemap.buffer + inputMesh.vertexes.num, -1);
    const int endIndex =
        std::min(inputMesh.indexes.num, firstIndex + triangleLimit);
    for (int index = firstIndex; index < endIndex; ++index)
      AppendRemappedVertex(inputMesh, inputMesh.indexes[index], output,
                           vertexRemap);
    FinishSplitMesh(meshIndexOffset + outputMeshes.num, inputMesh, output,
                    vertexRemap);
    outputMeshes.Append(output);
    firstIndex = endIndex;
  }
}

void idMD6BinaryGenerator::SplitMeshesForJointLimit(
    int maxJointsPerSurface) {
  idList<idProcessSurface*, 5> splitSurfaces;
  for (int surfaceIndex = 0; surfaceIndex < surfaces.num; ++surfaceIndex) {
    idProcessSurface* source = surfaces[surfaceIndex];
    if (source == nullptr)
      continue;
    idList<idProcessMesh, 5> splitMeshes;
    idProcessMesh inputCopy = source->mesh;
    const int badTriangle =
        SplitMeshForJointLimits(splitSurfaces.num, inputCopy,
                                maxJointsPerSurface, splitMeshes);
    if (badTriangle != -1) {
      idLib::Error(
          "model '%s' has surface '%s' with triangle %d that uses joints "
          "that are more than %d apart in the joint array.",
          sourceFileName.data, source->name.data, badTriangle,
          maxJointsPerSurface);
      return;
    }
    for (int meshIndex = 0; meshIndex < splitMeshes.num; ++meshIndex) {
      idProcessSurface* split = source->DuplicateShallow();
      if (split == nullptr)
        continue;
      split->mesh = splitMeshes[meshIndex];
      splitSurfaces.Append(split);
    }
  }
  surfaces.DeleteContents();
  surfaces = splitSurfaces;
  splitSurfaces.Clear();
}

void idMD6BinaryGenerator::SplitMeshesForIndexLimit(
    int maxIndexesPerSurface) {
  idList<idProcessSurface*, 5> splitSurfaces;
  for (int surfaceIndex = 0; surfaceIndex < surfaces.num; ++surfaceIndex) {
    idProcessSurface* source = surfaces[surfaceIndex];
    if (source == nullptr)
      continue;
    idList<idProcessMesh, 5> splitMeshes;
    SplitMeshForIndexLimit(splitSurfaces.num, source->mesh,
                           maxIndexesPerSurface, splitMeshes);
    for (int meshIndex = 0; meshIndex < splitMeshes.num; ++meshIndex) {
      idProcessSurface* split = source->DuplicateShallow();
      if (split == nullptr)
        continue;
      split->mesh = splitMeshes[meshIndex];
      splitSurfaces.Append(split);
    }
  }
  surfaces.DeleteContents();
  surfaces = splitSurfaces;
  splitSurfaces.Clear();
}

void idMD6BinaryGenerator::MergeMeshesAndBuildSourceSurfaces(
    int maxJointsPerSurface) {
  sourceSurfaces.SetNum(surfaces.num);
  for (int surfaceIndex = 0; surfaceIndex < surfaces.num; ++surfaceIndex) {
    idProcessSurface* surface = surfaces[surfaceIndex];
    sourceSurface_t& source = sourceSurfaces[surfaceIndex];
    source.mtr = surface != nullptr ? surface->material : nullptr;
    source.mtrChecksum = source.mtr != nullptr
                             ? static_cast<int>(source.mtr->md5Checksum)
                             : 0;
    if (surface == nullptr || surface->material == nullptr)
      continue;
    const idMaterial* compatible =
        surface->material->CompatibleGenericMaterial();
    if (compatible == nullptr) {
      surface->allowGenericMaterial = false;
      continue;
    }
    if (!surface->allowGenericMaterial &&
        md6_generateTexcoordRebase.valueInteger != 0) {
      surface->allowGenericMaterial = ProcessMeshRebaseTexcoords(surface->mesh);
    }
    if (surface->allowGenericMaterial) {
      surface->material = compatible;
    } else {
      idLib::Warning(
          "MD6 Model '%s' has mesh '%s' with wrapping uv-coordinates on "
          "possible generic material '%s' (must be inside range [0 - 1])!",
          sourceFileName.data, surface->name.data, surface->material->name.str);
    }
  }

  idList<idProcessSurface*, 5> merged;
  for (int sourceIndex = 0; sourceIndex < surfaces.num; ++sourceIndex) {
    idProcessSurface* source = surfaces[sourceIndex];
    if (source == nullptr)
      continue;
    int destinationIndex = -1;
    if (!source->discrete) {
      for (int index = 0; index < merged.num; ++index) {
        idProcessSurface* candidate = merged[index];
        if (candidate == nullptr || candidate->discrete ||
            candidate->material != source->material ||
            candidate->allowSkinRemapping != source->allowSkinRemapping ||
            candidate->allowGenericMaterial != source->allowGenericMaterial ||
            candidate->mesh.hashST != source->mesh.hashST) {
          continue;
        }
        const int minJoint =
            std::min(candidate->mesh.minJoint, source->mesh.minJoint);
        const int maxJoint =
            std::max(candidate->mesh.maxJoint, source->mesh.maxJoint);
        if (maxJoint - minJoint <= maxJointsPerSurface &&
            candidate->mesh.indexes.num + source->mesh.indexes.num <
                0x10000) {
          destinationIndex = index;
          break;
        }
      }
    }

    sourceSurface_t& sourceRecord = sourceSurfaces[sourceIndex];
    if (destinationIndex < 0) {
      idProcessSurface* copy = new (std::nothrow) idProcessSurface;
      if (copy == nullptr)
        continue;
      copy->name = source->name;
      copy->material = source->material;
      copy->discrete = source->discrete;
      copy->allowSkinRemapping = source->allowSkinRemapping;
      copy->allowGenericMaterial = source->allowGenericMaterial;
      copy->numTexCoordSets = source->numTexCoordSets;
      copy->mesh = source->mesh;
      destinationIndex = merged.num;
      merged.Append(copy);
      sourceRecord.renderSurface = destinationIndex;
      sourceRecord.firstVertex = 0;
      sourceRecord.lastVertex = source->mesh.vertexes.num - 1;
      continue;
    }

    idProcessMesh& destination = merged[destinationIndex]->mesh;
    const int firstVertex = destination.vertexes.num;
    const int firstIndex = destination.indexes.num;
    sourceRecord.renderSurface = destinationIndex;
    sourceRecord.firstVertex = firstVertex;
    sourceRecord.lastVertex = firstVertex + source->mesh.vertexes.num - 1;
    destination.minJoint =
        std::min(destination.minJoint, source->mesh.minJoint);
    destination.maxJoint =
        std::max(destination.maxJoint, source->mesh.maxJoint);
    destination.vertexes.Append(source->mesh.vertexes);
    destination.indexes.Append(source->mesh.indexes);
    for (int index = firstIndex; index < destination.indexes.num; ++index)
      destination.indexes[index] += firstVertex;
    AppendMorphSets(destination.morphSets, source->mesh.morphSets);
    for (int setIndex = 0;
         setIndex < destination.uvs.num && setIndex < source->mesh.uvs.num;
         ++setIndex) {
      destination.uvs[setIndex].st.Append(source->mesh.uvs[setIndex].st);
    }
  }
  surfaces.DeleteContents();
  surfaces = merged;
  merged.Clear();
}

bool idMD6BinaryGenerator::WriteBinaryFile(const char* filename) {
  if (filename == nullptr || fileSystem == nullptr || modelSkeleton == nullptr ||
      modelSkeleton->data == nullptr) {
    return false;
  }
  idFileLocal file(fileSystem->OpenFileWrite(filename));
  if (file.file == nullptr) {
    idLib::Warning("Could not open %s", filename);
    return false;
  }
  idLib::Printf("Writing %s\n", filename);
  WriteBigUInt(file.file, kMD6ModelMagic);
  WriteBigUInt(file.file, timestamp);
  WriteBigUInt(file.file, skelTimestamp);
  WriteNativeString(file.file, modelSkeleton->name.str);

  defaultBounds.Clear();
  for (int surfaceIndex = 0; surfaceIndex < surfaces.num; ++surfaceIndex) {
    const idProcessSurface* surface = surfaces[surfaceIndex];
    if (surface == nullptr)
      continue;
    for (int vertex = 0; vertex < surface->mesh.vertexes.num; ++vertex)
      defaultBounds.AddPoint(surface->mesh.vertexes[vertex].xyz);
  }

  const int numJoints = std::min<int>(modelSkeleton->data->numJoints, 256);
  if (numJoints > 0) {
    alignas(16) std::array<std::uint8_t, 0x4000> temporary{};
    alignas(16) std::array<__m128, 256 * 3> pose{};
    const float origin[4]{};
    idMD6Blend::DecodeBasePose(modelSkeleton->data,
                               reinterpret_cast<float*>(temporary.data()),
                               static_cast<std::uint32_t>(temporary.size()),
                               origin, true, pose.data());
    idBounds skeletonBounds;
    skeletonBounds.Clear();
    const float* matrix = reinterpret_cast<const float*>(pose.data());
    for (int joint = 0; joint < numJoints; ++joint) {
      skeletonBounds.AddPoint({matrix[12 * joint + 3],
                               matrix[12 * joint + 7],
                               matrix[12 * joint + 11]});
    }
    minBoundsExpansion.x +=
        std::min(0.0f, defaultBounds.b[0].x - (skeletonBounds.b[0].x - 16.0f));
    minBoundsExpansion.y +=
        std::min(0.0f, defaultBounds.b[0].y - (skeletonBounds.b[0].y - 16.0f));
    minBoundsExpansion.z +=
        std::min(0.0f, defaultBounds.b[0].z - (skeletonBounds.b[0].z - 16.0f));
    maxBoundsExpansion.x +=
        std::max(0.0f, defaultBounds.b[1].x - (skeletonBounds.b[1].x + 16.0f));
    maxBoundsExpansion.y +=
        std::max(0.0f, defaultBounds.b[1].y - (skeletonBounds.b[1].y + 16.0f));
    maxBoundsExpansion.z +=
        std::max(0.0f, defaultBounds.b[1].z - (skeletonBounds.b[1].z + 16.0f));
  }

  WriteBigVec3(file.file, minBoundsExpansion);
  WriteBigVec3(file.file, maxBoundsExpansion);
  file.file->Write(&remapForSkinning, sizeof(remapForSkinning));
  WriteNativeString(file.file, morphSkinName.data);
  if (jointRemap.num > 0)
    file.file->Write(jointRemap.list,
                     static_cast<std::size_t>(jointRemap.num));
  WriteBigBounds(file.file, defaultBounds);
  WriteBigInt(file.file, surfaces.num);

  for (int surfaceIndex = 0; surfaceIndex < surfaces.num; ++surfaceIndex) {
    idProcessSurface* surface = surfaces[surfaceIndex];
    if (surface == nullptr)
      return false;
    idProcessMesh& mesh = surface->mesh;
    WriteNativeString(file.file, surface->name.data);
    WriteNativeString(file.file,
                      surface->material != nullptr ? surface->material->name.str
                                                   : "");
    file.file->Write(&surface->allowSkinRemapping,
                     sizeof(surface->allowSkinRemapping));
    WriteBigInt(file.file, surface->numTexCoordSets);
    WriteBigInt(file.file, mesh.vertexes.num);
    WriteBigInt(file.file, mesh.indexes.num / 3);
    idBounds meshBounds;
    meshBounds.Clear();
    for (int vertex = 0; vertex < mesh.vertexes.num; ++vertex)
      meshBounds.AddPoint(mesh.vertexes[vertex].xyz);
    WriteBigBounds(file.file, meshBounds);

    int baseST = -1;
    for (int setIndex = 0; setIndex < mesh.uvs.num; ++setIndex) {
      if (mesh.uvs[setIndex].slot == 0) {
        baseST = setIndex;
        break;
      }
    }
    for (int vertexIndex = 0; vertexIndex < mesh.vertexes.num;
         ++vertexIndex) {
      const processVert_t& source = mesh.vertexes[vertexIndex];
      idDrawVert output{};
      output.xyz = source.xyz;
      if (baseST >= 0 && vertexIndex < mesh.uvs[baseST].st.num)
        output.st = mesh.uvs[baseST].st[vertexIndex];
      output.normal[0] = PackDirection(source.normal.x);
      output.normal[1] = PackDirection(source.normal.y);
      output.normal[2] = PackDirection(source.normal.z);
      output.tangent[0] = PackDirection(source.tangent.x);
      output.tangent[1] = PackDirection(source.tangent.y);
      output.tangent[2] = PackDirection(source.tangent.z);
      for (int joint = 0; joint < 4; ++joint) {
        output.color[joint] = source.jointIdxs[joint] != 0
                                  ? static_cast<std::uint8_t>(
                                        source.jointIdxs[joint] - mesh.minJoint)
                                  : output.color[0];
      }
      const std::uint16_t compressed =
          idDrawVert::CompressWeightsToBytes(source.weights);
      output.normal[3] = static_cast<std::uint8_t>(compressed & 0xffu);
      output.tangent[3] = static_cast<std::uint8_t>(compressed >> 8u);
      if (source.tangent.w >= 0.0f)
        output.tangent[3] |= 0x80u;
      WriteBigDrawVert(file.file, output);
    }
    for (int index = 0; index < mesh.indexes.num; ++index) {
      const std::uint16_t value =
          ByteSwap16(static_cast<std::uint16_t>(mesh.indexes[index]));
      file.file->Write(&value, sizeof(value));
    }
    WriteBigInt(file.file, mesh.minJoint);
    WriteBigInt(file.file, mesh.maxJoint - mesh.minJoint + 1);
    WriteBigInt(file.file, mesh.uvs.num - (baseST >= 0 ? 1 : 0));
    for (int setIndex = 0; setIndex < mesh.uvs.num; ++setIndex) {
      if (setIndex == baseST)
        continue;
      const processST_t& set = mesh.uvs[setIndex];
      file.file->Write(&set.slot, sizeof(set.slot));
      for (int vertex = 0; vertex < set.st.num; ++vertex)
        WriteBigVec2(file.file, set.st[vertex]);
    }

    const bool hasMorphMap = !morphSkinName.IsEmpty();
    file.file->Write(&hasMorphMap, sizeof(hasMorphMap));
    if (hasMorphMap) {
      for (int vertex = 0; vertex < mesh.vertexes.num; ++vertex)
        file.file->Write(mesh.vertexes[vertex].morph,
                         sizeof(mesh.vertexes[vertex].morph));
    }
    WriteBigInt(file.file, mesh.blendShapes.num);
    for (int shapeIndex = 0; shapeIndex < mesh.blendShapes.num;
         ++shapeIndex) {
      const blendShape_t& shape = mesh.blendShapes[shapeIndex];
      WriteNativeString(file.file, shape.userchannelExpression.data);
      if (shape.vertexOffsets.num > 0)
        file.file->Write(shape.vertexOffsets.list,
                         static_cast<std::size_t>(shape.vertexOffsets.num));
      WriteBigFloat(file.file, shape.multiplier);
    }
  }
  morphSets.Write_Binary(file.file);
  WriteSourceSurfaces(file.file, sourceSurfaces);
  WriteBigUInt(file.file, kMD6ModelMagic);
  return true;
}

bool GenerateMD6ModelBinary(const char* sourceFileName,
                            const char* binaryFileName) {
  idMD6BinaryGenerator generator;
  return generator.LoadAndProcessText(sourceFileName, false) &&
         generator.WriteBinaryFile(binaryFileName);
}

namespace {

bool ParseBlendShapeVerts(idLexer& parser, idList<idVec3, 5>& vertexData) {
  const int count = parser.ParseInt();
  if (count < 0 || !vertexData.SetNum(count))
    return false;
  idToken brace;
  if (!parser.ExpectTokenType(TT_PUNCTUATION, 46, brace))
    return false;
  for (int index = 0; index < count; ++index) {
    if (!parser.ExpectTokenString("vert") || parser.ParseInt() != index ||
        !parser.Parse1DMatrix(3, &vertexData[index].x, false)) {
      return false;
    }
    parser.SkipRestOfLine();
  }
  return parser.ExpectTokenType(TT_PUNCTUATION, 47, brace);
}

morphDataType_t ParseMorphDataType(const idToken& token) {
  if (token.type == TT_NUMBER) {
    idToken value = token;
    value.NumberValue();
    return static_cast<morphDataType_t>(value.intvalue);
  }
  const char* name = token.data;
  if (idStr::Icmpn(name, "morphdata_", 10) == 0)
    name += 10;
  if (idStr::Icmpn(name, "clip", 4) == 0)
    name += 4;
  if (idStr::Icmp(name, "sphere") == 0)
    return MORPHDATA_CLIPSPHERE;
  if (idStr::Icmp(name, "ellipsoid") == 0)
    return MORPHDATA_CLIPELLIPSOID;
  return MORPHDATA_NONE;
}

} // namespace

void idMD6BinaryGenerator::LoadBlendShape(
    const idMD6Skel* skeleton, idProcessSurface* parentMesh, idLexer& parser,
    blendShape_t& blendShape) {
  if (parentMesh == nullptr) {
    parser.Error("idMD6BinaryGenerator::LoadBlendShape: parentMesh is NULL");
    return;
  }
  if (skeleton == nullptr) {
    parser.Error("idMD6BinaryGenerator::LoadBlendShape: skeleton is NULL");
    return;
  }
  idToken token;
  parser.ExpectTokenType(TT_PUNCTUATION, 46, token);
  parser.ExpectTokenString("multiplier");
  blendShape.multiplier = parser.ParseFloat();
  parser.ExpectTokenString("userchannelexpression");
  parser.ReadToken(token);
  blendShape.userchannelExpression = token;
  parser.ExpectTokenString("verts");
  idList<idVec3, 5> destinationVertices;
  if (!ParseBlendShapeVerts(parser, destinationVertices))
    return;
  parser.ExpectTokenType(TT_PUNCTUATION, 47, token);
  if (parentMesh->mesh.vertexes.num != destinationVertices.num) {
    parser.Error(
        "idMD6BinaryGenerator::LoadBlendShape blendShape '%s' doesn't have "
        "same amount of vertices as '%s'",
        blendShape.name.data, parentMesh->name.data);
    return;
  }
  blendShape.vertexOffsets.SetNum(3 * destinationVertices.num,
                                  static_cast<std::uint8_t>(128));
  idVec3 maxOffset{};
  for (int sourceVertex = 0; sourceVertex < destinationVertices.num;
       ++sourceVertex) {
    if (sourceVertex >= parentMesh->mesh.vertexRemap.num)
      continue;
    const int destination = parentMesh->mesh.vertexRemap[sourceVertex];
    if (destination < 0 || destination >= parentMesh->mesh.vertexes.num)
      continue;
    const idVec3& base = parentMesh->mesh.vertexes[destination].xyz;
    const idVec3 offset = {destinationVertices[sourceVertex].x - base.x,
                           destinationVertices[sourceVertex].y - base.y,
                           destinationVertices[sourceVertex].z - base.z};
    maxOffset.x = std::max(maxOffset.x, std::fabs(offset.x));
    maxOffset.y = std::max(maxOffset.y, std::fabs(offset.y));
    maxOffset.z = std::max(maxOffset.z, std::fabs(offset.z));
    const int first = 3 * destination;
    blendShape.vertexOffsets[first] = static_cast<std::uint8_t>(
        std::max(0, std::min(255, static_cast<int>(
                                      offset.x * blendShape.multiplier + 128.0f))));
    blendShape.vertexOffsets[first + 1] = static_cast<std::uint8_t>(
        std::max(0, std::min(255, static_cast<int>(
                                      offset.y * blendShape.multiplier + 128.0f))));
    blendShape.vertexOffsets[first + 2] = static_cast<std::uint8_t>(
        std::max(0, std::min(255, static_cast<int>(
                                      offset.z * blendShape.multiplier + 128.0f))));
  }
  idLib::Printf("Blendshape MaxOffset: (%.2f %.2f %.2f)\n", maxOffset.x,
                maxOffset.y, maxOffset.z);
  blendShape.multiplier = 128.0f / blendShape.multiplier;
}

void idMD6BinaryGenerator::LoadMorphData(const char* fileName) {
  if (fileName == nullptr || *fileName == '\0')
    return;
  idLexer parser(560);
  if (!parser.LoadFile(fileName, false))
    return;
  if (!parser.ExpectTokenString("MD6MorphVersion") || parser.ParseInt() != 1)
    return;
  if (!parser.ExpectTokenString("init"))
    return;
  idToken token;
  parser.ExpectTokenType(TT_PUNCTUATION, 46, token);
  int numMeshes = 0;
  int numClips = 0;
  while (parser.ReadToken(token)) {
    if (token.type == TT_PUNCTUATION && token.subtype == 47)
      break;
    if (TokenEquals(token, "commandLine") || TokenEquals(token, "sourceFile")) {
      parser.ExpectAnyToken(token);
    } else if (TokenEquals(token, "numMeshes")) {
      numMeshes = parser.ParseInt();
    } else if (TokenEquals(token, "numClipPrimitives")) {
      numClips = parser.ParseInt();
    } else {
      parser.Error("Unknown token: %s", token.data);
      return;
    }
  }
  if (numMeshes + numClips <= 0) {
    parser.Error("No clip-primitives or meshes in morph-file");
    return;
  }

  while (parser.ReadToken(token)) {
    if (TokenEquals(token, "mesh")) {
      parser.ExpectTokenType(TT_PUNCTUATION, 46, token);
      idProcessSurface* surface = nullptr;
      int meshIndex = -1;
      while (parser.ReadToken(token)) {
        if (token.type == TT_PUNCTUATION && token.subtype == 47)
          break;
        if (TokenEquals(token, "name")) {
          parser.ExpectAnyToken(token);
          for (int index = 0; index < surfaces.num; ++index) {
            if (idStr::Icmp(surfaces[index]->name.data, token.data) == 0) {
              surface = surfaces[index];
              meshIndex = index;
              break;
            }
          }
          if (surface == nullptr) {
            parser.Error("Could not find mesh '%s' in model", token.data);
            return;
          }
        } else if (TokenEquals(token, "slot")) {
          if (surface == nullptr) {
            parser.Error("meshIndex is %d but the model only has %d surfaces",
                         meshIndex, surfaces.num);
            return;
          }
          idMD6MorphSet& set = surface->mesh.morphSets.AllocSet();
          set.type = MORPHDATA_MORPH;
          set.meshIndex = meshIndex;
          set.groupIndex = parser.ParseInt();
          parser.ExpectTokenType(TT_PUNCTUATION, 46, token);
          while (parser.ReadToken(token)) {
            if (token.type == TT_PUNCTUATION && token.subtype == 47)
              break;
            if (!TokenEquals(token, "indices")) {
              parser.Error("Unknown token '%s' in slot block.", token.data);
              return;
            }
            const int count = parser.ParseInt();
            parser.ExpectTokenType(TT_PUNCTUATION, 46, token);
            for (int index = 0; index < count; ++index) {
              parser.ExpectTokenString("index");
              const int sourceVertex = parser.ParseInt();
              if (sourceVertex >= 0 &&
                  sourceVertex < surface->mesh.vertexRemap.num) {
                set.indices.Append(static_cast<std::uint16_t>(
                    surface->mesh.vertexRemap[sourceVertex]));
              }
            }
            parser.ExpectTokenType(TT_PUNCTUATION, 47, token);
          }
        } else {
          parser.Error("Unknown token '%s' in mesh block.", token.data);
          return;
        }
      }
    } else if (TokenEquals(token, "clip")) {
      parser.ExpectTokenType(TT_PUNCTUATION, 46, token);
      idMD6MorphSet& set = morphSets.AllocSet();
      while (parser.ReadToken(token)) {
        if (token.type == TT_PUNCTUATION && token.subtype == 47)
          break;
        if (TokenEquals(token, "name")) {
          parser.ExpectAnyToken(token);
        } else if (TokenEquals(token, "type")) {
          parser.ExpectAnyToken(token);
          set.type = ParseMorphDataType(token);
          if (set.type != MORPHDATA_CLIPSPHERE &&
              set.type != MORPHDATA_CLIPELLIPSOID) {
            parser.Error("Unknown morph-set type %d in clip block.",
                         static_cast<int>(set.type));
            return;
          }
        } else if (TokenEquals(token, "slot")) {
          set.groupIndex = parser.ParseInt();
        } else if (TokenEquals(token, "position")) {
          parser.Parse1DMatrix(3, &set.pos.x, false);
        } else if (TokenEquals(token, "size")) {
          parser.Parse1DMatrix(3, &set.size.x, false);
        } else if (TokenEquals(token, "axis")) {
          parser.Parse2DMatrix(3, 3, &set.axis.mat[0].x);
        } else {
          parser.Error("Unknown token '%s' in clip block.", token.data);
          return;
        }
      }
    } else {
      parser.Error("Unknown token: %s", token.data);
      return;
    }
  }
}

bool idMD6BinaryGenerator::LoadAndProcessText(const char* sourceName,
                                              bool onlySingleUVSet) {
  if (sourceName == nullptr || *sourceName == '\0')
    return false;

  sourceFileName = sourceName;
  idLexer parser(560);
  if (!parser.LoadFile(sourceName, false))
    return false;

  int md6Version = 0;
  for (;;) {
    if (!parser.ExpectTokenString("MD6Version"))
      return false;
    md6Version = parser.ParseInt();
    if (md6Version == 5)
      break;
    if (!idMD6Model::RewriteText(const_cast<char*>(sourceName))) {
      parser.Error("Invalid MD6 version %d, expected %d", md6Version, 5);
      return false;
    }
    parser.FreeSource();
    if (!parser.LoadFile(sourceName, false))
      return false;
  }
  timestamp = parser.GetFileTime();

  bool hasExportedNormals = false;
  int numMeshes = 0;
  int numJoints = 0;
  idStr morphMapFileName;
  idStr morphDataFileName;
  idMD6Skel* simpleSkeleton = nullptr;
  idToken token;

  if (!parser.ExpectTokenString("init") ||
      !parser.ExpectTokenType(TT_PUNCTUATION, 46, token)) {
    return false;
  }
  while (parser.ReadToken(token)) {
    if (token.type == TT_PUNCTUATION && token.subtype == 47)
      break;
    if (TokenEquals(token, "commandLine") ||
        TokenEquals(token, "sourceFile")) {
      if (!parser.ExpectTokenType(TT_STRING, 0, token))
        return false;
    } else if (TokenEquals(token, "skeleton")) {
      if (!parser.ExpectTokenType(TT_STRING, 0, token)) {
        return false;
      }
      if (g_generatorSkeletonLoader == nullptr) {
        parser.Error("No MD6 skeleton loader is registered");
        return false;
      }
      modelSkeleton = g_generatorSkeletonLoader(token.data);
      if (modelSkeleton == nullptr) {
        parser.Error("Could not load skeleton '%s'", token.data);
        return false;
      }
      skelTimestamp = fileSystem != nullptr
                          ? fileSystem->GetTimestamp(modelSkeleton->name.str,
                                                     false)
                          : 0;
    } else if (TokenEquals(token, "simpleskeleton")) {
      if (!parser.ExpectTokenType(TT_STRING, 0, token)) {
        return false;
      }
      if (feature_hq.valueInteger == 0) {
        if (g_generatorSkeletonLoader == nullptr) {
          parser.Error("No MD6 skeleton loader is registered");
          return false;
        }
        simpleSkeleton = g_generatorSkeletonLoader(token.data);
        if (simpleSkeleton == nullptr) {
          parser.Error("Could not load simple skeleton '%s'", token.data);
          return false;
        }
      }
    } else if (TokenEquals(token, "morph")) {
      if (!parser.ExpectTokenType(TT_STRING, 0, token)) {
        return false;
      }
      morphDataFileName = token;
    } else if (TokenEquals(token, "numMeshes")) {
      numMeshes = parser.ParseInt();
    } else if (TokenEquals(token, "numJoints")) {
      numJoints = parser.ParseInt();
    } else if (TokenEquals(token, "numUserChannels") ||
               TokenEquals(token, "numWeightSets")) {
      parser.ParseInt();
    } else if (TokenEquals(token, "remapForSkinning")) {
      remapForSkinning = parser.ParseBool();
    } else if (TokenEquals(token, "exportNormals")) {
      hasExportedNormals = parser.ParseBool();
    } else if (TokenEquals(token, "minExpand")) {
      if (!parser.Parse1DMatrix(3, &minBoundsExpansion.x, false))
        return false;
    } else if (TokenEquals(token, "maxExpand")) {
      if (!parser.Parse1DMatrix(3, &maxBoundsExpansion.x, false))
        return false;
    } else if (TokenEquals(token, "morphMap")) {
      if (!parser.ExpectTokenType(TT_STRING, 0, token)) {
        return false;
      }
      morphMapFileName = token;
    } else if (TokenEquals(token, "morphSkin")) {
      if (!parser.ExpectTokenType(TT_STRING, 0, token)) {
        return false;
      }
      morphSkinName = token;
    } else {
      parser.Error("Unknown token '%s' in MD6 init block", token.data);
      return false;
    }
  }

  if (numMeshes <= 0) {
    parser.Error("MD6 model has invalid numMeshes %d", numMeshes);
    return false;
  }
  if (modelSkeleton == nullptr || modelSkeleton->data == nullptr) {
    parser.Error("MD6 model has no skeleton");
    return false;
  }
  idMD6Skel* outputSkeleton =
      simpleSkeleton != nullptr ? simpleSkeleton : modelSkeleton;
  if (outputSkeleton->data == nullptr) {
    parser.Error("MD6 output skeleton has no data");
    return false;
  }
  if (numJoints == 0)
    numJoints = modelSkeleton->data->numJoints;
  if (numJoints < 0 || numJoints > modelSkeleton->data->numJoints) {
    parser.Error("MD6 model declares %d joints but skeleton has %d", numJoints,
                 modelSkeleton->data->numJoints);
    return false;
  }

  idList<std::int16_t, 5> remapTable;
  remapTable.SetNum(numJoints);
  if (!parser.ExpectTokenString("joints") ||
      !parser.ExpectTokenType(TT_PUNCTUATION, 46, token)) {
    return false;
  }
  const auto* parents = reinterpret_cast<const std::int16_t*>(
      reinterpret_cast<const std::uint8_t*>(modelSkeleton->data) +
      modelSkeleton->data->parentTblOffset);
  for (int sourceJoint = 0; sourceJoint < numJoints; ++sourceJoint) {
    if (!parser.ReadToken(token))
      return false;
    int joint = modelSkeleton->GetJointIndex(token.data).value;
    int guard = 0;
    while (joint >= outputSkeleton->data->numJoints && joint >= 0 &&
           guard++ < numJoints) {
      joint = parents[joint];
    }
    if (joint < 0 || joint >= outputSkeleton->data->numJoints) {
      parser.Error("Joint '%s' is missing from skeleton '%s'", token.data,
                   outputSkeleton->name.str);
      return false;
    }
    remapTable[sourceJoint] = static_cast<std::int16_t>(joint);
  }
  if (!parser.ExpectTokenType(TT_PUNCTUATION, 47, token))
    return false;

  defaultBounds.Clear();
  surfaces.DeleteContents();
  for (int meshIndex = 0; meshIndex < numMeshes; ++meshIndex) {
    if (!parser.ExpectTokenString("mesh"))
      return false;
    idList<idProcessSurface*, 5> parsedSurfaces;
    ParseMD6MeshToSurfaces(md6Version, parser, numJoints,
                           hasExportedNormals, remapTable, parsedSurfaces);
    if (parser.HadError()) {
      parsedSurfaces.DeleteContents();
      return false;
    }
    bool invalidTexCoordCount = false;
    for (int surfaceIndex = 0; surfaceIndex < parsedSurfaces.num;
         ++surfaceIndex) {
      idProcessSurface* surface = parsedSurfaces[surfaceIndex];
      if (onlySingleUVSet && surface != nullptr &&
          surface->numTexCoordSets != 1) {
        invalidTexCoordCount = true;
        idLib::Warning("MD6 model '%s' mesh '%s' has %d UV sets", sourceName,
                       surface->name.data, surface->numTexCoordSets);
      }
      surfaces.Append(surface);
    }
    parsedSurfaces.Clear();
    if (invalidTexCoordCount) {
      idLib::Error("MD6 model '%s' must contain only one UV set", sourceName);
      return false;
    }
  }

  CreateHashST();
  const int paddedJoints = (outputSkeleton->data->numJoints + 7) & ~7;
  const auto* basePose = reinterpret_cast<const float*>(
      reinterpret_cast<const std::uint8_t*>(outputSkeleton->data) +
      outputSkeleton->data->basePoseOffset);
  const float* baseTranslation = basePose + paddedJoints * 7;
  const idVec3 morphOffset = {baseTranslation[0], baseTranslation[1],
                              baseTranslation[2]};
  idList<vertexMorphOffset_t, 5> vertexMorphOffsets;
  LoadMorphMap(morphMapFileName.data, morphOffset, vertexMorphOffsets);
  if (vertexMorphOffsets.num > 0) {
    for (int surfaceIndex = 0; surfaceIndex < surfaces.num; ++surfaceIndex) {
      if (!MatchMorphVerts(vertexMorphOffsets,
                           surfaces[surfaceIndex]->mesh.vertexes)) {
        idLib::Warning("Could not match all morph vertices for mesh '%s' in '%s'",
                       surfaces[surfaceIndex]->name.data, sourceName);
      }
    }
  }
  if (!morphDataFileName.IsEmpty())
    LoadMorphData(morphDataFileName.data);

  if (parser.CheckTokenString("blendshapes")) {
    if (!parser.ExpectTokenType(TT_PUNCTUATION, 46, token))
      return false;
    while (parser.ReadToken(token)) {
      if (token.type == TT_PUNCTUATION && token.subtype == 47)
        break;
      idProcessSurface* parentSurface = nullptr;
      for (int surfaceIndex = 0; surfaceIndex < surfaces.num;
           ++surfaceIndex) {
        if (idStr::Icmp(surfaces[surfaceIndex]->name.data, token.data) == 0) {
          parentSurface = surfaces[surfaceIndex];
          break;
        }
      }
      if (parentSurface == nullptr) {
        parser.Error("Could not find blendshape parent mesh '%s'", token.data);
        return false;
      }
      if (!parentSurface->discrete) {
        parser.Error("Blendshape parent mesh '%s' is not discrete", token.data);
        return false;
      }
      if (!parser.ExpectTokenType(TT_PUNCTUATION, 46, token))
        return false;
      while (parser.ReadToken(token)) {
        if (token.type == TT_PUNCTUATION && token.subtype == 47)
          break;
        blendShape_t* shape = parentSurface->mesh.blendShapes.Alloc();
        if (shape == nullptr)
          return false;
        shape->name = token;
        LoadBlendShape(outputSkeleton, parentSurface, parser, *shape);
        if (parser.HadError())
          return false;
      }
    }
  }

  idArray<bool, 256> usedJoints{};
  usedJoints[0] = true;
  for (int surfaceIndex = 0; surfaceIndex < surfaces.num; ++surfaceIndex) {
    idProcessMesh& mesh = surfaces[surfaceIndex]->mesh;
    for (int vertexIndex = 0; vertexIndex < mesh.vertexes.num;
         ++vertexIndex) {
      processVert_t& vertex = mesh.vertexes[vertexIndex];
      for (int influence = 1; influence < 4; ++influence) {
        if (vertex.weights[influence] == 0)
          vertex.jointIdxs[influence] = vertex.jointIdxs[0];
      }
      for (int influence = 0; influence < 4; ++influence)
        usedJoints[vertex.jointIdxs[influence]] = true;
    }
  }
  int numUsedJoints = 0;
  for (int joint = 0; joint < 256; ++joint)
    numUsedJoints += usedJoints[joint] ? 1 : 0;
  idLib::Printf("MD6 model uses %d of %d joints\n", numUsedJoints,
                outputSkeleton->data->numJoints);

  SetupJointRemap(outputSkeleton, usedJoints, remapForSkinning, jointRemap);
  for (int surfaceIndex = 0; surfaceIndex < surfaces.num; ++surfaceIndex) {
    idProcessMesh& mesh = surfaces[surfaceIndex]->mesh;
    for (int vertexIndex = 0; vertexIndex < mesh.vertexes.num;
         ++vertexIndex) {
      processVert_t& vertex = mesh.vertexes[vertexIndex];
      for (int influence = 0; influence < 4; ++influence) {
        const int original = vertex.jointIdxs[influence];
        if (original >= jointRemap.num || jointRemap[original] == 255) {
          parser.Error("Joint %d has no output remap", original);
          return false;
        }
        vertex.jointIdxs[influence] = jointRemap[original];
      }
    }
    ComputeMeshJointRange(mesh);
  }

  SplitMeshesForJointLimit(140);
  SplitMeshesForIndexLimit(0xffff);
  for (int surfaceIndex = 0; surfaceIndex < surfaces.num; ++surfaceIndex) {
    idProcessMesh& mesh = surfaces[surfaceIndex]->mesh;
    if (mesh.indexes.num <= 0)
      continue;
    std::vector<std::uint16_t> input(
        static_cast<std::size_t>(mesh.indexes.num));
    std::vector<std::uint16_t> optimized(
        static_cast<std::size_t>(mesh.indexes.num));
    for (int index = 0; index < mesh.indexes.num; ++index)
      input[static_cast<std::size_t>(index)] =
          static_cast<std::uint16_t>(mesh.indexes[index]);
    MeshOptimizer(input.data(), static_cast<unsigned int>(input.size()),
                  optimized.data(), 1.0f, 0.0f, 0.8f);
    for (int index = 0; index < mesh.indexes.num; ++index)
      mesh.indexes[index] = optimized[static_cast<std::size_t>(index)];
  }
  MergeMeshesAndBuildSourceSurfaces(140);
  for (int surfaceIndex = 0; surfaceIndex < surfaces.num; ++surfaceIndex)
    AppendMorphSets(morphSets, surfaces[surfaceIndex]->mesh.morphSets);
  modelSkeleton = outputSkeleton;
  return true;
}
