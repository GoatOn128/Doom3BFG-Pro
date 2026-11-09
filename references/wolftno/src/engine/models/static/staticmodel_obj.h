#pragma once

#include "engine/compilers/renderbump/mtltocpuvmtr.h"
#include "shared/idlib/containers/list.h"
#include "shared/idlib/math/vector.h"
#include "shared/idlib/text/lexer.h"
#include "shared/idlib/text/str.h"

#include <cstddef>
#include <cstdint>

struct mrgb_t {
  std::uint8_t mrgb[4]{};
};

struct idOBJFace {
  int v[3]{};
  int vt[3]{};
  int vn[3]{};
};

class idOBJMesh {
public:
  idOBJMesh();
  idOBJMesh(const idOBJMesh&) = default;
  idOBJMesh& operator=(const idOBJMesh&) = default;
  ~idOBJMesh() = default;

  idStr materialName;
  idStr groupName;
  bool hasTexCoords = false;
  bool hasNormals = false;
  std::uint8_t padding52[6]{};
  idList<idOBJFace, 5> f;
};

class idOBJModel {
public:
  idOBJModel();
  idOBJModel(const idOBJModel&) = default;
  idOBJModel& operator=(const idOBJModel&) = default;
  ~idOBJModel() = default;

  bool Load(const char* fileName);
  bool Save(const char* fileName);

  idList<idParsedMtl, 5> materials;
  idList<idVec3, 5> v;
  idList<mrgb_t, 5> mrgb;
  idList<idVec2, 5> vt;
  idList<idVec3, 5> vn;
  idList<idOBJMesh, 5> meshes;
};

int ParseFaceLine(const char* line, int attributes[8][3]);
void OBJ_Parse(idOBJModel* model, idLexer& source, const idStr& path);
idOBJModel* OBJ_Load(const char* fileName);

static_assert(sizeof(mrgb_t) == 4);
static_assert(sizeof(idOBJFace) == 36);
static_assert(sizeof(idOBJMesh) == 112);
static_assert(offsetof(idOBJMesh, f) == 0x58);
static_assert(sizeof(idOBJModel) == 0x90);
static_assert(offsetof(idOBJModel, v) == 0x18);
static_assert(offsetof(idOBJModel, mrgb) == 0x30);
static_assert(offsetof(idOBJModel, vt) == 0x48);
static_assert(offsetof(idOBJModel, vn) == 0x60);
static_assert(offsetof(idOBJModel, meshes) == 0x78);
