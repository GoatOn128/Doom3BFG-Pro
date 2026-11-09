#include "tungsten/game/ai/aiblackboard.h"

// DIA: idAIBlackboard::Find
idAIVarBase* idAIBlackboard::Find(const char* variableName) {
  if (variableName == nullptr) return nullptr;
  for (int index = 0; index < vars.Num(); ++index) {
    const idVarInfo& info = vars[index];
    idAIVarBase* variable = nullptr;
    if (info.varArray == VARRAY_STRING) variable = &stringVars[info.index];
    if (info.varArray == VARRAY_FLOAT) variable = &floatVars[info.index];
    if (info.varArray == VARRAY_INT) variable = &intVars[info.index];
    if (variable != nullptr && idStr::Icmp(variableName, variable->name.data) == 0) {
      return variable;
    }
  }
  return nullptr;
}

// DIA: idAIBlackboard::Shutdown
void idAIBlackboard::Shutdown() {
  stringVars.Clear();
  floatVars.Clear();
  intVars.Clear();
  vars.ClearFree();
}

// DIA: idList<idAIVar_String,5>::~idList<idAIVar_String,5>
template idList<idAIVar_String, 5>::~idList();

// DIA: idStaticList<idAIVar_String,8>::~idStaticList<idAIVar_String,8>
template idStaticList<idAIVar_String, 8>::~idStaticList();
// DIA: `idStaticList<idAIVar_String,8>::~idStaticList<idAIVar_String,8>'::`1'::dtor$0

// DIA: idStaticList<idAIVar_Float,8>::~idStaticList<idAIVar_Float,8>
template idStaticList<idAIVar_Float, 8>::~idStaticList();
// DIA: `idStaticList<idAIVar_Float,8>::~idStaticList<idAIVar_Float,8>'::`1'::dtor$0

// DIA: idStaticList<idAIVar_Int,8>::~idStaticList<idAIVar_Int,8>
template idStaticList<idAIVar_Int, 8>::~idStaticList();

// DIA: idAIBlackboard::~idAIBlackboard
idAIBlackboard::~idAIBlackboard() = default;
// DIA: `idAIBlackboard::~idAIBlackboard'::`1'::dtor$0
// DIA: `idAIBlackboard::~idAIBlackboard'::`1'::dtor$1
// DIA: `idAIBlackboard::~idAIBlackboard'::`1'::dtor$2
// DIA: `idAIBlackboard::~idAIBlackboard'::`1'::dtor$5
// DIA: `idAIBlackboard::~idAIBlackboard'::`1'::dtor$7
// DIA: `idAIBlackboard::~idAIBlackboard'::`1'::dtor$9

// DIA: idAIBlackboard::Init
void idAIBlackboard::Init() {
  Shutdown();
}

// DIA: idList<idAIVar_String,5>::Resize
template bool idList<idAIVar_String, 5>::Resize(int);
// DIA: idList<idAIVar_Float,5>::Resize
template bool idList<idAIVar_Float, 5>::Resize(int);
// DIA: idList<idAIVar_Int,5>::Resize
template bool idList<idAIVar_Int, 5>::Resize(int);

// DIA: idAIBlackboard::idAIBlackboard
idAIBlackboard::idAIBlackboard() = default;
// DIA: `idAIBlackboard::idAIBlackboard'::`1'::dtor$4
// DIA: `idAIBlackboard::idAIBlackboard'::`1'::dtor$0
// DIA: `idAIBlackboard::idAIBlackboard'::`1'::dtor$6
// DIA: `idAIBlackboard::idAIBlackboard'::`1'::dtor$1
// DIA: `idAIBlackboard::idAIBlackboard'::`1'::dtor$8
// DIA: `idAIBlackboard::idAIBlackboard'::`1'::dtor$2

// DIA: idList<idAIBlackboard::idVarInfo,5>::Append
template int idList<idAIBlackboard::idVarInfo, 5>::Append(
    const idAIBlackboard::idVarInfo&);
// DIA: idList<idAIVar_String,5>::Alloc
template idAIVar_String* idList<idAIVar_String, 5>::Alloc();
// DIA: idList<idAIVar_Float,5>::Alloc
template idAIVar_Float* idList<idAIVar_Float, 5>::Alloc();
// DIA: idList<idAIVar_Int,5>::Alloc
template idAIVar_Int* idList<idAIVar_Int, 5>::Alloc();

// DIA: idAIBlackboard::AllocInt
idAIVar_Int* idAIBlackboard::AllocInt(const char* variableName,
                                      unsigned int value) {
  if (Find(variableName) != nullptr) return nullptr;
  idAIVar_Int* variable = intVars.Alloc();
  variable->name = variableName != nullptr ? variableName : "";
  variable->value = value;
  vars.Append({VARRAY_INT, intVars.Num() - 1});
  return variable;
}

// DIA: idAIBlackboard::AllocFloat
idAIVar_Float* idAIBlackboard::AllocFloat(const char* variableName,
                                          float value) {
  if (Find(variableName) != nullptr) return nullptr;
  idAIVar_Float* variable = floatVars.Alloc();
  variable->name = variableName != nullptr ? variableName : "";
  variable->value = value;
  vars.Append({VARRAY_FLOAT, floatVars.Num() - 1});
  return variable;
}

// DIA: idAIBlackboard::AllocString
idAIVar_String* idAIBlackboard::AllocString(const char* variableName,
                                            const char* value) {
  if (Find(variableName) != nullptr) return nullptr;
  idAIVar_String* variable = stringVars.Alloc();
  variable->name = variableName != nullptr ? variableName : "";
  variable->value = value != nullptr ? value : "";
  vars.Append({VARRAY_STRING, stringVars.Num() - 1});
  return variable;
}
