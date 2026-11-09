#include "declsimple.h"

idDeclInfoTemplate<idDeclRibbon> idDeclRibbon::resourceList("ribbon", "idDeclRibbon", "ribbon", false);

idDeclRibbon::idDeclRibbon()
    : material(nullptr), duration(0), maxLength(0), fadeInFraction(0), fadeOutFraction(0),
      gravity{0, 0, 0}, velocity{0, 0, 0}, startWidth(2.0f), endWidth(32.0f), color{1, 1, 1, 1},
      viewOriented(true), reorientNodes(false), textureRepeat(false), textureRepeatStretch(false),
      textureTMin(0), textureTMax(1), nodeSubdivision(0), minNodeDistance(0),
      helix{0, 0, 0, {0, 0}, {0, 0}}, turbulence{0, 0, 1, {0.93f, 1.0f, 0.91f, 0.73f}} {
  resourceListPtr = &resourceList;
}
idDeclInfo* idDeclRibbon::GetDeclInfo() const { return &resourceList; }
