#include "declsimple.h"

idDeclInfoTemplate<idDeclElectricBolt> idDeclElectricBolt::resourceList("ebolt", "idDeclElectricBolt", "ebolt", false);

idDeclElectricBolt::idDeclElectricBolt()
    : material(nullptr), applyGradient(false), revealTime(0), brightnessTable(nullptr),
      startWidth(10.0f), endWidth(10.0f), maxDeviation(50.0f), color{1, 1, 1, 1},
      brightness(1.0f), subdivisions(1), branchProbability(0.2f), maxBranchLevels(0),
      branchStartWidth(5.0f), branchEndWidth(5.0f), branchLength{10.0f, 20.0f},
      maxBranchAngle(10.0f), branchSubdivisions(2), maxBranchDeviation(15.0f),
      jitterTable(nullptr), jitterFalloffTable(nullptr), jitterDecay(0.0f), jitterSpeed(0.01f),
      jitterLeftMag{0, 0}, jitterUpMag{0, 0}, useBranchOverride(false),
      branchOverride{nullptr, 1, 10} {
  resourceListPtr = &resourceList;
}

idDeclInfo* idDeclElectricBolt::GetDeclInfo() const { return &resourceList; }
