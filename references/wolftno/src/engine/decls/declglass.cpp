#include "declsimple.h"

idDeclInfoTemplate<idDeclGlass> idDeclGlass::resourceList("glass", "idDeclGlass", "tdef", false);

idDeclGlass::idDeclGlass()
    : fx(nullptr), impact(0), destroy(0), impactParticle(nullptr), destroyParticle(nullptr),
      impactFillParticle(nullptr), destroyFillParticle(nullptr), impactSound(nullptr),
      destroySound(nullptr), impactMaterial(nullptr), decalMaterial(nullptr), decalSize{1.0f, 2.0f},
      decalClipAlpha{0, 128}, playerClip(false), monsterClip(false), floor(false), unbreakable(false) {
  resourceListPtr = &resourceList;
}
idDeclInfo* idDeclGlass::GetDeclInfo() const { return &resourceList; }
