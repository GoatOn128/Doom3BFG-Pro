#pragma once

#include "shared/idlib/containers/list.h"
#include "shared/idlib/math/vector.h"
#include "shared/idlib/text/str.h"

#include <array>

class idAIActionParm {
public:
  enum parmType_t {
    PARM_NONE = 0,
    PARM_INTEGER = 1,
    PARM_VECTOR = 2,
    PARM_STRING = 3,
  };

  enum subParm_t {
    SUBPARM_NONE = 0,
    SUBPARM_ENTITY,
    SUBPARM_DECL,
    SUBPARM_ENUM,
    SUBPARM_FLAGS,
    SUBPARM_COUNT,
  };

  struct subParmInfo_t {
    bool isEnum{false};
  };

  bool IsEnum() const;
  void Set(int value);
  void Set(const char* value, subParm_t subTypeValue);

  static const std::array<subParmInfo_t, SUBPARM_COUNT> subParmInfo;

  parmType_t type{PARM_NONE};
  subParm_t subType{SUBPARM_NONE};
  int intVal{0};
  idVec3 vectorVal{};
  void* objectVal{nullptr};
  idStr strVal;
};

class idScriptAction {
public:
  idStr name;
  idList<idAIActionParm, 5> parameters;
};
