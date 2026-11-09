#pragma once

#include "engine/decls/decl.h"
#include "shared/idlib/csystems/cvarsystemlocal.h"
#include "shared/idlib/math/vector.h"

#include <cstddef>

class idDeclDetail final : public idDecl {
public:
  idDeclDetail();

  const char* DefaultDefinition() const override;
  idDeclInfo* GetDeclInfo() const override;
  void Parse(idParser& source) override;

  idAtomicString modelName;
  idVec3 colorVariance;

  static idDeclInfoTemplate<idDeclDetail> resourceList;
};

extern idCVar r_skipDetail;

static_assert(sizeof(idDeclDetail) == 104);
static_assert(offsetof(idDeclDetail, modelName) == 80);
static_assert(offsetof(idDeclDetail, colorVariance) == 88);
