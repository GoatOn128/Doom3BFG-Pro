#pragma once

#include "decl.h"
#include "shared/idlib/math/vector.h"

enum footStepType_t : int {
  FOOTSTEP_DEFAULT = 0,
};

struct idIKLeg {
  idAtomicString foot;
  idAtomicString ankle;
  idAtomicString knee;
  idAtomicString hip;
  idAtomicString forward;
  footStepType_t footstep;
  idVec3 kneeDir;
};

class idDeclWalkIK final : public idDeclTypeInfo {
public:
  idDeclWalkIK();
  ~idDeclWalkIK() override;
  void FreeData() override;
  idDeclInfo* GetDeclInfo() const override;

  idList<idIKLeg, 83> legJoints;
  idAtomicString waistJoint;
  idVec3 kneeDir;
  float smoothing;
  float waistSmoothing;
  float stairSmoothing;
  float stairWaistSmoothing;
  float footSmoothing;
  float footShift;
  float waistShift;
  float minWaistFloorDist;
  float minWaistAnkleDist;
  float footSize;
  float footUpTrace;
  float footDownTrace;
  float footDownTraceDead;
  float restrictToBoundsTolerance;
  bool usePivot;
  bool restrictToBounds;
  bool dirsFromBasePose;
  bool checkFeetForSolids;

  static idDeclInfoTemplate<idDeclWalkIK> resourceList;
};

static_assert(sizeof(idIKLeg) == 56, "idIKLeg ABI drift");
static_assert(sizeof(idDeclWalkIK) == 200, "idDeclWalkIK ABI drift");
