#include "declvmtrpreload.h"

idDeclInfoTemplate<idDeclVmtrPreload> idDeclVmtrPreload::resourceList(
    "vmtrPreload", "idDeclVmtrPreload", "tdef", false);

idDeclVmtrPreload::idDeclVmtrPreload() {
  resourceListPtr = &resourceList;
}

idDeclVmtrPreload::~idDeclVmtrPreload() = default;

idDeclInfo* idDeclVmtrPreload::GetDeclInfo() const {
  return &resourceList;
}

namespace {
idList<preloadMaterial_t> emptyList;
}

const idList<preloadMaterial_t>& idDeclVmtrPreload::GetListForSpawnspot(
    const char* spawnSpot) const {
  for (int i = 0; i < preloadList.Num(); ++i) {
    const char* candidate = preloadList[i].spawnSpot.str;
    if (spawnSpot != nullptr) {
      if (candidate == spawnSpot || idStr::Cmp(candidate, spawnSpot) == 0)
        return preloadList[i].preloadList;
    } else if (candidate[0] == '\0') {
      return preloadList[i].preloadList;
    }
  }
  return emptyList;
}
