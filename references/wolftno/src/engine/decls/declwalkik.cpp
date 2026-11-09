#include "declwalkik.h"

idDeclInfoTemplate<idDeclWalkIK> idDeclWalkIK::resourceList(
    "walkIK", "idDeclWalkIK", "walkik", false);

idDeclWalkIK::idDeclWalkIK() {
  resourceListPtr = &resourceList;
  waistJoint.Set("");
  FreeData();
}

idDeclWalkIK::~idDeclWalkIK() = default;

void idDeclWalkIK::FreeData() {
  legJoints.Clear();
  kneeDir = {1.0f, 0.0f, 0.0f};
  smoothing = 0.75f;
  waistSmoothing = 0.5f;
  stairSmoothing = 0.85000002f;
  stairWaistSmoothing = 0.75f;
  footSmoothing = 0.0f;
  footShift = 0.0f;
  waistShift = 0.0f;
  minWaistFloorDist = 0.0f;
  minWaistAnkleDist = 0.0f;
  footSize = 4.0f;
  footUpTrace = 32.0f;
  footDownTrace = 32.0f;
  footDownTraceDead = 255.0f;
  restrictToBoundsTolerance = 0.0f;
  usePivot = false;
  restrictToBounds = false;
  dirsFromBasePose = false;
  checkFeetForSolids = false;
}

idDeclInfo* idDeclWalkIK::GetDeclInfo() const {
  return &resourceList;
}
