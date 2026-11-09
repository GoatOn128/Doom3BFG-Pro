#pragma once

#include "engine/models/skinremap.h"
#include "engine/renderer/declmaterial.h"
#include "engine/renderer/triangles.h"
#include "shared/idlib/containers/list.h"
#include "shared/idlib/csystems/cvarsystemlocal.h"
#include "shared/idlib/filesystem/file.h"

#include <cstddef>
#include <cstdint>

class idMD6Mesh {
public:
  struct vert_t {
    idVec3 xyz;
    idVec2 st;
    idVec3 normal;
    idVec4 tangent;
    std::uint8_t weights[4]{};
    std::uint8_t jointIdxs[4]{};
  };

  struct stset_t {
    std::uint8_t slot{};
    std::uint8_t recoveredPadding[7]{};
    idList<idVec2, 5> st;
    idVertexBuffer* vb{};
  };

  struct blendShape_t {
    idList<std::uint8_t, 5> blendShapeData;
    idStr blendShapeExpression;
    float multiplier{};
    std::uint32_t recoveredPadding{};
  };

  idMD6Mesh();
  ~idMD6Mesh();

  std::uint64_t Memory() const;
  void FreeLoadData();
  void Free();
  void LoadMesh_Binary(idFile* file, unsigned int magic);

  idStr name;
  const idMaterial* material{};
  bool discrete{};
  bool allowSkinRemapping{};
  std::uint8_t recoveredFlagsPadding[2]{};
  int numTexCoordSets{};
  idBounds bounds;
  int numVerts{};
  int numTris{};
  int jointOffset{};
  int numJoints{};
  int hashST{};
  std::uint32_t recoveredHashPadding{};
  idList<std::uint8_t, 5> morphMap;
  idTriangles* tris{};
  idList<skinRemap_t, 5> skinRemaps;
  idList<blendShape_t, 5> blendShapes;
  idList<vert_t, 19> vertexes;
  idList<stset_t, 19> stSets;
  idList<std::uint16_t, 19> indexes;

private:
  __declspec(noinline) void CreateHashST();
};

template <>
bool idList<idMD6Mesh::stset_t, 19>::Resize(int newSize);
template <>
bool idList<idMD6Mesh::blendShape_t, 5>::Resize(int newSize);

extern idCVar feature_hq;
extern idCVar r_skipBlendShapes;

static_assert(sizeof(idMD6Mesh::vert_t) == 56, "idMD6Mesh::vert_t ABI drift");
static_assert(sizeof(idMD6Mesh::stset_t) == 40,
              "idMD6Mesh::stset_t ABI drift");
static_assert(offsetof(idMD6Mesh::stset_t, st) == 8,
              "idMD6Mesh::stset_t st offset drift");
static_assert(sizeof(idMD6Mesh::blendShape_t) == 72,
              "idMD6Mesh::blendShape_t ABI drift");
static_assert(offsetof(idMD6Mesh::blendShape_t, blendShapeExpression) == 24,
              "idMD6Mesh::blend shape expression offset drift");
static_assert(sizeof(idMD6Mesh) == 256, "idMD6Mesh ABI drift");
static_assert(offsetof(idMD6Mesh, material) == 40,
              "idMD6Mesh material offset drift");
static_assert(offsetof(idMD6Mesh, morphMap) == 104,
              "idMD6Mesh morph map offset drift");
static_assert(offsetof(idMD6Mesh, blendShapes) == 160,
              "idMD6Mesh blend shapes offset drift");
static_assert(offsetof(idMD6Mesh, stSets) == 208,
              "idMD6Mesh ST sets offset drift");
