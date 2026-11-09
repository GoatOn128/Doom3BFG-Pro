#include "declsimple.h"

idDeclInfoTemplate<idDeclImpactParticle> idDeclImpactParticle::resourceList(
    "impactParticles", "idDeclImpactParticle", "tdef", false);

idDeclImpactParticle::idDeclImpactParticle()
    : defaultPrt(nullptr), metalPrt(nullptr), stonePrt(nullptr), fleshPrt(nullptr), woodPrt(nullptr),
      cardboardPrt(nullptr), liquidPrt(nullptr), glassPrt(nullptr), plasticPrt(nullptr),
      asphaltPrt(nullptr), dirtPrt(nullptr), concretePrt(nullptr), foliagePrt(nullptr),
      linoleumPrt(nullptr), fabricPrt(nullptr), rubberPrt(nullptr), rockPrt(nullptr),
      steamPipePrt(nullptr), waterPipePrt(nullptr), armorPrt(nullptr), sludgePrt(nullptr),
      shieldPrt(nullptr), bloodPoolPrt(nullptr), ashPrt(nullptr), mutantFleshPrt(nullptr),
      thickPaddingPrt(nullptr), porcelainPrt(nullptr), plasterPrt(nullptr), electronicsPrt(nullptr),
      oilPipePrt(nullptr), panzerMetalPrt(nullptr), breakableArmorPrt(nullptr),
      electricShieldPrt(nullptr) {
  resourceListPtr = &resourceList;
}

const idDeclParticle* idDeclImpactParticle::GetImpactParticleForMaterial(int materialID) const {
  switch (materialID) {
    case 1: case 22: case 29: case 30: case 32: case 35: return metalPrt;
    case 2: return stonePrt;
    case 3: return fleshPrt;
    case 4: case 31: return woodPrt;
    case 5: return cardboardPrt;
    case 6: return liquidPrt;
    case 7: return glassPrt;
    case 8: return plasticPrt;
    case 9: return asphaltPrt;
    case 10: return dirtPrt;
    case 11: return concretePrt;
    case 12: return foliagePrt;
    case 13: return linoleumPrt;
    case 14: return fabricPrt;
    case 15: return rubberPrt;
    case 16: return rockPrt;
    case 17: return steamPipePrt;
    case 18: return waterPipePrt;
    case 19: return armorPrt;
    case 20: return sludgePrt;
    case 25: return bloodPoolPrt;
    case 26: return ashPrt;
    case 27: return mutantFleshPrt;
    case 28: return thickPaddingPrt;
    case 33: return porcelainPrt;
    case 34: return plasterPrt;
    case 36: return electronicsPrt;
    case 37: return oilPipePrt;
    case 38: return panzerMetalPrt;
    case 39: return breakableArmorPrt;
    case 40: return electricShieldPrt;
    default: return defaultPrt;
  }
}

idDeclInfo* idDeclImpactParticle::GetDeclInfo() const { return &resourceList; }
