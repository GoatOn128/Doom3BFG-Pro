#pragma once

#include "engine/models/skeletalanimation/md6model.h"
#include "engine/models/skeletalanimation/md6morph.h"
#include "shared/idlib/math/vector.h"
#include "shared/idlib/containers/array.h"
#include "shared/idlib/containers/heaparray.h"

#include <cstddef>
#include <cstdint>

class idLexer;

using idMD6GeneratorSkeletonLoader = idMD6Skel* (*)(const char* name);

struct vertexMorphOffset_t {
  idVec3 xyz{};
  idVec3 offset{};
};

struct processVert_t {
  idVec3 xyz{};
  idVec3 normal{};
  idVec4 tangent{};
  float floatWeights[4]{};
  std::uint8_t morph[4]{};
  std::uint8_t weights[4]{};
  std::uint8_t jointIdxs[4]{};
};

struct processST_t {
  idList<idVec2, 5> st;
  std::uint8_t slot{};
  std::uint8_t padding[7]{};
};

struct processTri_t {
  int indexes[3]{};
};

struct usedJoints_t {
  std::uint8_t joints[32]{};
  int triangleIndex{};
};

struct blendShape_t {
  blendShape_t();

  idStr name;
  idStr userchannelExpression;
  idList<std::uint8_t, 5> vertexOffsets;
  float multiplier{};
  std::uint32_t padding{};
};

class idProcessMesh {
public:
  idProcessMesh();
  idProcessMesh(const idProcessMesh& other);
  ~idProcessMesh();
  idProcessMesh& operator=(const idProcessMesh& other);

  idList<processVert_t, 5> vertexes;
  idList<processST_t, 5> uvs;
  idList<int, 5> indexes;
  idList<int, 5> faceMaterialIndex;
  idList<blendShape_t, 5> blendShapes;
  idList<int, 5> vertexRemap;
  idMD6Morph morphSets;
  int minJoint{};
  int maxJoint{255};
  int hashST{};
};

class idProcessSurface {
public:
  idProcessSurface* DuplicateShallow() const;

  idStr name;
  const idMaterial* material{};
  bool discrete{};
  bool allowSkinRemapping{true};
  bool allowGenericMaterial{true};
  std::uint8_t padding{};
  int numTexCoordSets{1};
  idProcessMesh mesh;
};

class idMD6BinaryGenerator {
public:
  idMD6BinaryGenerator();
  ~idMD6BinaryGenerator();

  void CreateHashST();
  void LoadMorphData(const char* morphFileName);
  void LoadBlendShape(const idMD6Skel* skeleton,
                      idProcessSurface* parentMesh, idLexer& parser,
                      blendShape_t& blendShape);
  bool WriteBinaryFile(const char* binaryFileName);
  void LoadMorphMap(const char* morphMapFileName, const idVec3& offset,
                    idList<vertexMorphOffset_t, 5>& vertexMorphOffsets);
  void MergeMeshesAndBuildSourceSurfaces(int maxJointsPerSurface);
  void SplitMeshesForJointLimit(int maxJointsPerSurface);
  void SplitMeshesForIndexLimit(int maxIndexesPerSurface);
  bool LoadAndProcessText(const char* sourceName, bool onlySingleUVSet);

  idStr sourceFileName;
  std::uint32_t timestamp{};
  std::uint32_t skelTimestamp{};
  idBounds defaultBounds{};
  bool remapForSkinning{};
  std::uint8_t padding[3]{};
  idVec3 minBoundsExpansion{};
  idVec3 maxBoundsExpansion{};
  std::uint32_t boundsPadding{};
  idStr morphSkinName;
  idMD6Skel* modelSkeleton{};
  idList<std::uint8_t, 37> jointRemap;
  idList<idProcessSurface*, 5> surfaces;
  idList<sourceSurface_t, 5> sourceSurfaces;
  idMD6Morph morphSets;
};

bool GenerateMD6ModelBinary(const char* sourceFileName,
                            const char* binaryFileName);
void SetMD6GeneratorSkeletonLoader(idMD6GeneratorSkeletonLoader loader);
bool MatchMorphVerts(const idList<vertexMorphOffset_t, 5>& vertexMorphOffsets,
                     idList<processVert_t, 5>& vertexes);
void AppendMorphSets(idMD6Morph& destination, const idMD6Morph& source);
void RemapMorphSets(int meshIndex, const idProcessMesh& sourceMesh,
                    idProcessMesh& outputMesh,
                    const idHeapArray<int>& vertexRemap);
void SetupJointRemap(const idMD6Skel* skeleton,
                     const idArray<bool, 256>& usedJoints,
                     bool renumberBreadthFirst,
                     idList<std::uint8_t, 37>& jointRemap);
bool ProcessMeshRebaseTexcoords(idProcessMesh& mesh);
void ParseMD6MeshToSurfaces(int md6Version, idLexer& parser,
                            int numOriginalJoints, bool hasExportedNormals,
                            const idList<std::int16_t, 5>& remapTable,
                            idList<idProcessSurface*, 5>& surfaces);
int SplitMeshForJointLimits(int meshIndexOffset, idProcessMesh& inputMesh,
                            int maxJointsPerMesh,
                            idList<idProcessMesh, 5>& outputMeshes);
void SplitMeshForIndexLimit(int meshIndexOffset,
                            const idProcessMesh& inputMesh,
                            int maxIndexesPerMesh,
                            idList<idProcessMesh, 5>& outputMeshes);

static_assert(sizeof(processVert_t) == 68, "processVert_t ABI drift");
static_assert(sizeof(vertexMorphOffset_t) == 24,
              "vertexMorphOffset_t ABI drift");
static_assert(sizeof(processST_t) == 32, "processST_t ABI drift");
static_assert(sizeof(blendShape_t) == 112, "blendShape_t ABI drift");
static_assert(sizeof(idProcessMesh) == 184, "idProcessMesh ABI drift");
static_assert(offsetof(idProcessMesh, minJoint) == 168,
              "idProcessMesh min-joint offset drift");
static_assert(offsetof(idProcessMesh, hashST) == 176,
              "idProcessMesh hash offset drift");
static_assert(sizeof(idProcessSurface) == 240, "idProcessSurface ABI drift");
static_assert(offsetof(idProcessSurface, mesh) == 56,
              "idProcessSurface mesh offset drift");
static_assert(sizeof(idMD6BinaryGenerator) == 248,
              "idMD6BinaryGenerator ABI drift");
static_assert(offsetof(idMD6BinaryGenerator, defaultBounds) == 48,
              "idMD6BinaryGenerator bounds offset drift");
static_assert(offsetof(idMD6BinaryGenerator, morphSkinName) == 104,
              "idMD6BinaryGenerator morph-skin offset drift");
static_assert(offsetof(idMD6BinaryGenerator, jointRemap) == 152,
              "idMD6BinaryGenerator joint-remap offset drift");
static_assert(offsetof(idMD6BinaryGenerator, morphSets) == 224,
              "idMD6BinaryGenerator morph-set offset drift");
