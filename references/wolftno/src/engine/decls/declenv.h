#pragma once

#include "decl.h"
#include "engine/renderer/parmblock.h"

// NEEDS_WORK(engine/renderer): parsing delegates render-parm expressions to the
// recovered idParmBlockParser interface. Its implementation remains in the
// renderer recovery scope.

class idDeclEnv final : public idDecl {
public:
  idDeclEnv() : parentDeclName(), blendDuration(0.0f) {}
  const char* DefaultDefinition() const override;
  void FreeData() override;
  void Parse(idParser& source) override;
  idDeclInfo* GetDeclInfo() const override;

  idParmBlock parmBlock;
  idAtomicString parentDeclName;
  float blendDuration;

  static idDeclInfoTemplate<idDeclEnv> resourceList;
};

const idDeclEnv* BuildEnvDecl(const char* entityName, char* parentDeclName,
                              char* worldName, const char* userEnvMap);

static_assert(sizeof(idDeclEnv) == 144, "idDeclEnv ABI drift");
