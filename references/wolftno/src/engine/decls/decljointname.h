#pragma once

#include "shared/idlib/text/str.h"

class idJointName : public idStr {
public:
  using idStr::idStr;
};

static_assert(sizeof(idJointName) == 40, "idJointName ABI drift");
