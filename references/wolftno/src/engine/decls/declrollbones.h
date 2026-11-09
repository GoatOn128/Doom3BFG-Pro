#pragma once

#include "decl.h"

class idDeclRollBones final : public idDeclTypeInfo {
public:
  struct idDeclRollBoneInstance {
    idStr rollSourceJoint;
    idStr rollDestJoint;
    float rollAmount;
  };

  idDeclRollBones();
  idDeclInfo* GetDeclInfo() const override;

  idList<idDeclRollBoneInstance> rollBones;

  static idDeclInfoTemplate<idDeclRollBones> resourceList;
};

static_assert(sizeof(idDeclRollBones::idDeclRollBoneInstance) == 88,
              "idDeclRollBones::idDeclRollBoneInstance ABI drift");
static_assert(sizeof(idDeclRollBones) == 120, "idDeclRollBones ABI drift");
