#pragma once

#include "engine/gamelib/aas2/aas2_interface.h"
#include "engine/gamelib/aas2/aas2callback_avoidlocation.h"

#include <cstddef>

class idAAS2Callback_FindFlaggedArea final
    : public idAAS2Callback_AvoidLocation {
public:
  idAAS2Callback_FindFlaggedArea(int areaFlag, bool set);
  ~idAAS2Callback_FindFlaggedArea() override;
  bool AreaIsGoal(const idAAS2* aas, int areaNum,
                  const idVec3& potentialGoal) override;

  int areaFlag;
  int test;
};

static_assert(offsetof(idAAS2Callback_FindFlaggedArea, areaFlag) == 0x28,
              "idAAS2Callback_FindFlaggedArea ABI drift");
static_assert(sizeof(idAAS2Callback_FindFlaggedArea) == 0x30,
              "idAAS2Callback_FindFlaggedArea ABI drift");
