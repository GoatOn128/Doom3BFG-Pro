#include "declsimple.h"

#include <algorithm>

idDeclInfoTemplate<idDeclProductionFilter> idDeclProductionFilter::resourceList(
    "productionfilter", "idDeclProductionFilter", "tdef", false);

idDeclProductionFilter::idDeclProductionFilter() { resourceListPtr = &resourceList; }
idDeclProductionFilter::~idDeclProductionFilter() = default;

void idDeclProductionFilter::Parse(idParser& source) {
  idDeclTypeInfo::Parse(source);
  const auto less = [](const idStr& left, const idStr& right) {
    return idStr::Cmp(left.data, right.data) < 0;
  };
  if (entityFilter.list != nullptr)
    std::sort(entityFilter.list, entityFilter.list + entityFilter.num, less);
  if (entityRenderModelFilter.list != nullptr)
    std::sort(entityRenderModelFilter.list,
              entityRenderModelFilter.list + entityRenderModelFilter.num, less);
}

idDeclInfo* idDeclProductionFilter::GetDeclInfo() const { return &resourceList; }
