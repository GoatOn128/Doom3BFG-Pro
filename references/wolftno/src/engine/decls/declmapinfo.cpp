#include "declmapinfo.h"

idDeclInfoTemplate<idDeclMapInfo> idDeclMapInfo::resourceList(
    "mapInfo", "idDeclMapInfo", "tdef", false);

idDeclMapInfo::idDeclMapInfo()
    : discNumber(-1), dlcId(0), vmtrChunkNumber(0), installChunkAllocationNumber(-1) {
  resourceListPtr = &resourceList;
}

idDeclMapInfo::~idDeclMapInfo() = default;

idDeclInfo* idDeclMapInfo::GetDeclInfo() const {
  return &resourceList;
}
