#pragma once

#include "shared/idlib/containers/list.h"
#include "shared/idlib/math/vector.h"
#include "shared/idlib/text/str.h"

#include <cstddef>
#include <cstdint>

struct aseFace_t {
  int vertexNum[3]{};
  int tVertexNum[3]{};
  idVec3 faceNormal{};
  idVec3 vertexNormals[3]{};
  unsigned char vertexColors[4][3]{};
};

class idASEMesh {
public:
  idASEMesh();
  idASEMesh(const idASEMesh&) = default;
  idASEMesh& operator=(const idASEMesh&) = default;
  ~idASEMesh();

  int timeValue;
  int numFaces;
  int numTVFaces;
  int numCVFaces;
  idVec3 transform[4];
  bool colorsParsed;
  bool normalsParsed;
  std::uint8_t recoveredPadding42[6];
  idList<idVec3, 5> vertexes;
  idList<idVec2, 5> tvertexes;
  idList<idVec3, 5> cvertexes;
  aseFace_t* faces;
};

class idASEMaterial {
public:
  idASEMaterial()
      : name(), uOffset(0.0f), vOffset(0.0f), uTiling(0.0f),
        vTiling(0.0f), angle(0.0f), recoveredPadding3C{} {}

  idStr name;
  float uOffset;
  float vOffset;
  float uTiling;
  float vTiling;
  float angle;
  std::uint8_t recoveredPadding3C[4];
};

class idASEObject {
public:
  idASEObject();
  idASEObject(const idASEObject&) = default;
  idASEObject& operator=(const idASEObject&) = default;
  ~idASEObject() noexcept(false);

  idStr name;
  int materialRef;
  std::uint8_t recoveredPadding2C[4];
  idASEMesh mesh;
  idList<idASEMesh*, 5> frames;
};

class idASEModel {
public:
  idASEModel();
  idASEModel(const idASEModel&) = default;
  idASEModel& operator=(const idASEModel&) = default;
  ~idASEModel();

  idList<idASEMaterial*, 5> materials;
  idList<idASEObject*, 5> objects;
};

int ASE_GetToken(bool restOfLine);
void ASE_ParseBracedBlock(void (*parser)(const char* token));
void ASE_SkipEnclosingBraces();
void ASE_SkipRestOfLine();
void ASE_KeyMESH_FACE_LIST(const char* token);
void ASE_KeyTFACE_LIST(const char* token);
void ASE_KeyMAP_DIFFUSE(const char* token);
void ASE_KeyMATERIAL(const char* token);
void ASE_KeyNODE_TM(const char* token);
void ASE_KeyMESH_VERTEX_LIST(const char* token);
void ASE_KeyCFACE_LIST(const char* token);
void ASE_KeyMESH_TVERTLIST(const char* token);
void ASE_KeyMESH_CVERTLIST(const char* token);
void ASE_KeyMESH_NORMALS(const char* token);
void ASE_KeyMESH(const char* token);
void ASE_KeyMATERIAL_LIST(const char* token);
void ASE_KeyMESH_ANIMATION(const char* token);
void ASE_KeyGEOMOBJECT(const char* token);
void ASE_ParseGeomObject();
void ASE_KeyGROUP(const char* token);
idASEModel* ASE_Parse(const char* buffer, bool verbose);
idASEModel* ASE_Load(const char* fileName);
void ASE_Free(idASEModel* aseModel);

static_assert(sizeof(aseFace_t) == 0x54, "aseFace_t ABI drift");
static_assert(offsetof(aseFace_t, faceNormal) == 0x18,
              "aseFace_t normal offset drift");
static_assert(offsetof(aseFace_t, vertexColors) == 0x48,
              "aseFace_t color offset drift");
static_assert(sizeof(idASEMesh) == 0x98, "idASEMesh ABI drift");
static_assert(offsetof(idASEMesh, transform) == 0x10,
              "idASEMesh transform offset drift");
static_assert(offsetof(idASEMesh, vertexes) == 0x48,
              "idASEMesh vertex list offset drift");
static_assert(offsetof(idASEMesh, faces) == 0x90,
              "idASEMesh face offset drift");
static_assert(sizeof(idASEMaterial) == 0x40, "idASEMaterial ABI drift");
static_assert(offsetof(idASEMaterial, uOffset) == 0x28,
              "idASEMaterial UV offset drift");
static_assert(sizeof(idASEObject) == 0xE0, "idASEObject ABI drift");
static_assert(offsetof(idASEObject, mesh) == 0x30,
              "idASEObject mesh offset drift");
static_assert(offsetof(idASEObject, frames) == 0xC8,
              "idASEObject frame offset drift");
static_assert(sizeof(idASEModel) == 0x30, "idASEModel ABI drift");
