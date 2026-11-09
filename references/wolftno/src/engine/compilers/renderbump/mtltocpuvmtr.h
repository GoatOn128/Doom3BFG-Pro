#pragma once

#include "shared/idlib/containers/list.h"
#include "shared/idlib/math/vector.h"
#include "shared/idlib/text/str.h"

class idLexer;
class idMaterial;

struct idMtlOptions {
  idStr loadedFile;
  bool flipV = false;
  bool scaleKd = false;
  bool scaleKs = false;
};

struct idParsedMtl {
  idParsedMtl();
  ~idParsedMtl();
  idParsedMtl& operator=(const idParsedMtl& other);

  idStr mtlName;
  float Ns;
  float Ni;
  float d;
  float Tr;
  float sharpness;
  idVec3 Tf;
  int illum;
  idVec3 Ka;
  idVec3 Kd;
  idVec3 Ks;
  idVec3 Ke;
  idStr map_Ka;
  idStr map_Kd;
  idStr map_Ks;
  idStr map_Ke;
  idStr map_bump;
  idStr map_opacity;
  idStr map_d;
  idStr map_Ns;
  idStr map_refl;
  idStr decal;
  idStr disp;
  bool map_aat;
};

idStr ExtractFinalFilename(const char* str);
idStr StripBaseDirFromFilename(const char* str);
idStr LocalFileNameToGameFileName(const char* localFileName, const char* currentDirectory);
idVec3 ParseVec3(idLexer& src);
const idMaterial* DeclMaterialForParsedMtl(const idParsedMtl& mtl, idMtlOptions* mtlOptions);
bool ParseMtlFile(const char* mtlText, idList<idParsedMtl, 5>& parsedMtls);

static_assert(sizeof(idMtlOptions) == 48, "idMtlOptions ABI drift");
static_assert(sizeof(idParsedMtl) == 576, "idParsedMtl ABI drift");
