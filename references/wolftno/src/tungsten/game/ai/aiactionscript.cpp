#include "tungsten/game/ai/aiactionscript.h"

const std::array<idAIActionParm::subParmInfo_t, idAIActionParm::SUBPARM_COUNT>
    idAIActionParm::subParmInfo{{
        {false},
        {false},
        {false},
        {true},
        {true},
    }};

// DIA: idAIActionParm::IsEnum
bool idAIActionParm::IsEnum() const {
  const int index = static_cast<int>(subType);
  return index >= 0 && index < static_cast<int>(subParmInfo.size()) &&
         subParmInfo[static_cast<std::size_t>(index)].isEnum;
}

// DIA: idAIActionParm::Set
void idAIActionParm::Set(int value) {
  type = PARM_INTEGER;
  subType = SUBPARM_NONE;
  intVal = value;
  vectorVal = {};
  objectVal = nullptr;
  strVal.Empty();
}

// DIA: idAIActionParm::Set
void idAIActionParm::Set(const char* value, subParm_t subTypeValue) {
  type = PARM_STRING;
  subType = subTypeValue;
  intVal = 0;
  vectorVal = {};
  objectVal = nullptr;
  strVal = value != nullptr ? value : "";
}

// DIA: idList<idScriptAction,5>::Resize
template bool idList<idScriptAction, 5>::Resize(int newSize);
