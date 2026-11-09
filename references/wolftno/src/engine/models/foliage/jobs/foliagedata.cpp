#include "engine/models/foliage/jobs/foliagedata.h"

void SetupSubTreePtrsFromData(dm_subTreeDataPtrs_t& subTreePtrs,
                              const dm_subTreeData_t* subTree) {
  char* const base =
      const_cast<char*>(reinterpret_cast<const char*>(subTree));
  subTreePtrs.detail =
      reinterpret_cast<dm_detail_t*>(base + subTree->detailOffset);
  subTreePtrs.attributes =
      reinterpret_cast<dm_attribute_t*>(base + subTree->attributeOffset);
}
