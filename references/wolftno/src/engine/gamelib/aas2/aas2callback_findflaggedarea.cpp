#define WOLFTNO_RETAIL_IDMEM_SINGLETON_ABI
#include "engine/gamelib/aas2/aas2callback_findflaggedarea.h"
#undef WOLFTNO_RETAIL_IDMEM_SINGLETON_ABI

idAAS2Callback_FindFlaggedArea::idAAS2Callback_FindFlaggedArea(int areaFlag_,
                                                               bool set)
    : idAAS2Callback_AvoidLocation(), areaFlag(areaFlag_), test(set) {}

idAAS2Callback_FindFlaggedArea::~idAAS2Callback_FindFlaggedArea() = default;

bool idAAS2Callback_FindFlaggedArea::AreaIsGoal(
    const idAAS2* aas, int areaNum, const idVec3& potentialGoal) {
  (void)potentialGoal;
  return (areaFlag & aas->GetAreaFlags(areaNum)) == test;
}
