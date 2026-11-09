#pragma once

#include "engine/models/static/staticmodel.h"
#include "shared/idlib/containers/list.h"
#include "shared/idlib/geometry/drawvert.h"
#include "shared/idlib/math/vector.h"

#include <cstddef>
#include <cstdint>

struct matchVert_t {
  int next{};
  int v{};
  int tv{};
  int morph{};
  unsigned int color{};
  idVec3 normal{};
  idVec3 tangents[2]{};
};

struct vertexTangents_t {
  idVec3 normal{};
  idVec3 tangents[2]{};
};

class idSplitResults {
public:
  idList<int, 5> indexes;
  idList<int, 5> vertexMap;
};

class idRawSurface {
public:
  idRawSurface()
      : material(nullptr), materialNum(0), pvList(nullptr), ptvList(nullptr),
        pMorphList(nullptr), indexes(), verts(), vertHash(),
        generateNormals(false), normalEpsilon(0.98000002f) {}

  void InitHash(const idList<idVec3, 5>& vList,
                const idList<idVec2, 5>& tvList,
                const idList<idVec3, 5>& morphList);
  void InitIndex(int maxIndexes);
  void AddVert(int v, int tv, const idVec3& normal, unsigned int color,
               int morph);
  void GenerateNormals();
  void DuplicateMirroredVertexes();
  void MakeSurfaces(idList<idStaticModelSurface, 5>& surfaces);

  void SetMaterial(const idMaterial* value) { material = value; }
  const idMaterial* GetMaterial() const { return material; }
  void SetMaterialNum(int value) { materialNum = value; }
  int GetMaterialNum() const { return materialNum; }
  void SetGenerateNormalsFlag(bool value) { generateNormals = value; }
  void SetNormalEpsilon(float value) { normalEpsilon = value; }

  const idMaterial* material;
  int materialNum;
  std::uint8_t recoveredPadding0C[4]{};
  const idList<idVec3, 5>* pvList;
  const idList<idVec2, 5>* ptvList;
  const idList<idVec3, 5>* pMorphList;
  idList<int, 5> indexes;
  idList<matchVert_t, 5> verts;
  idList<int, 5> vertHash;
  bool generateNormals;
  std::uint8_t recoveredPadding71[3]{};
  float normalEpsilon;
};

bool BuildAveragedSurfaceNormals(
    const idList<idVec3, 5>& verts, const idList<int, 5>& indexes,
    idList<idVec3, 5>& averagedSurfaceNormals);
void DeriveVertexTangents(const idList<int, 5>& indexes,
                          const idList<idDrawVert, 5>& verts,
                          idList<vertexTangents_t, 5>& vertTangents);
void SplitTrianglesForVertexLimit(const idList<int, 5>& indexes,
                                  int maxVerts,
                                  idList<idSplitResults, 5>& split);
void EmitRawSurfacesToStaticModel(idList<idRawSurface, 5>& rawSurfaces,
                                  idStaticModel& model);

static_assert(sizeof(matchVert_t) == 56, "matchVert_t ABI drift");
static_assert(offsetof(matchVert_t, normal) == 0x14,
              "matchVert_t normal offset drift");
static_assert(offsetof(matchVert_t, tangents) == 0x20,
              "matchVert_t tangent offset drift");
static_assert(sizeof(vertexTangents_t) == 36,
              "vertexTangents_t ABI drift");
static_assert(sizeof(idSplitResults) == 48, "idSplitResults ABI drift");
static_assert(sizeof(idRawSurface) == 0x78, "idRawSurface ABI drift");
static_assert(offsetof(idRawSurface, indexes) == 0x28,
              "idRawSurface index offset drift");
static_assert(offsetof(idRawSurface, verts) == 0x40,
              "idRawSurface vertex offset drift");
static_assert(offsetof(idRawSurface, vertHash) == 0x58,
              "idRawSurface hash offset drift");
static_assert(offsetof(idRawSurface, normalEpsilon) == 0x74,
              "idRawSurface epsilon offset drift");
