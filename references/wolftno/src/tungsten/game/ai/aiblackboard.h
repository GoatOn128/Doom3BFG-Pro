#pragma once

#include "shared/idlib/containers/staticlist.h"
#include "shared/idlib/text/str.h"

enum idAIVarType {
  AIVAR_STRING,
  AIVAR_FLOAT,
  AIVAR_INT,
};

struct idAIVarBase {
  idStr name;
  idAIVarType type{AIVAR_STRING};
};

struct idAIVar_String : idAIVarBase {
  idAIVar_String() { type = AIVAR_STRING; }
  idStr value;
};

struct idAIVar_Float : idAIVarBase {
  idAIVar_Float() { type = AIVAR_FLOAT; }
  float value{0.0F};
};

struct idAIVar_Int : idAIVarBase {
  idAIVar_Int() { type = AIVAR_INT; }
  unsigned int value{0};
};

class idAIBlackboard {
public:
  enum varArray_t {
    VARRAY_STRING,
    VARRAY_FLOAT,
    VARRAY_INT,
  };

  struct idVarInfo {
    varArray_t varArray{VARRAY_STRING};
    int index{0};
  };

  idAIBlackboard();
  ~idAIBlackboard();

  idAIVarBase* Find(const char* name);
  void Init();
  void Shutdown();
  idAIVar_Int* AllocInt(const char* name, unsigned int value);
  idAIVar_Float* AllocFloat(const char* name, float value);
  idAIVar_String* AllocString(const char* name, const char* value);
  int NumVariables() const { return vars.Num(); }

private:
  idStaticList<idAIVar_String, 8> stringVars;
  idStaticList<idAIVar_Float, 8> floatVars;
  idStaticList<idAIVar_Int, 8> intVars;
  idList<idVarInfo, 5> vars;
};
