#include "declflare.h"

idDeclInfoTemplate<idDeclFlare> idDeclFlare::resourceList("flare", "idDeclFlare", "flare", false);

idDeclFlare::idDeclFlare()
    : useAltViewVecCalc(0), viewAngleFade(0.0f), invViewAngleFade(1.0f), brightness(1.0f),
      cullDistance(4000.0f) {
  resourceListPtr = &resourceList;
}

void idDeclFlare::FreeData() {
  if (subflares.listStatic == 0 || subflares.listStatic == 2) {
    delete[] subflares.list;
    subflares.list = nullptr;
    subflares.size = 0;
  }
  subflares.num = 0;
}

idDeclInfo* idDeclFlare::GetDeclInfo() const {
  return &resourceList;
}
