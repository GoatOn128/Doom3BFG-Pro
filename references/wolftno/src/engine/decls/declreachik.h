#pragma once

#include "decl.h"
#include "decljointname.h"

struct idIKArm {
  idJointName hand;
  idJointName elbow;
  idJointName shoulder;
};

struct idIKHand {
  idJointName attach;
  idJointName hand;
  idJointName foreArm;
  idJointName elbow;
  idJointName shoulder;
  idJointName target;
};

class idDeclReachIK final : public idDeclTypeInfo {
public:
  idDeclReachIK();
  ~idDeclReachIK() override;
  void FreeData() override;
  idDeclInfo* GetDeclInfo() const override;

  idList<idIKArm, 83> armInfo;
  static idDeclInfoTemplate<idDeclReachIK> resourceList;
};

class idDeclHandIK final : public idDeclTypeInfo {
public:
  idDeclHandIK();
  ~idDeclHandIK() override;
  void FreeData() override;
  idDeclInfo* GetDeclInfo() const override;

  idList<idIKHand, 83> handInfo;
  static idDeclInfoTemplate<idDeclHandIK> resourceList;
};

static_assert(sizeof(idJointName) == 40, "idJointName ABI drift");
static_assert(sizeof(idIKArm) == 120, "idIKArm ABI drift");
static_assert(sizeof(idIKHand) == 240, "idIKHand ABI drift");
static_assert(sizeof(idDeclReachIK) == 120, "idDeclReachIK ABI drift");
static_assert(sizeof(idDeclHandIK) == 120, "idDeclHandIK ABI drift");
