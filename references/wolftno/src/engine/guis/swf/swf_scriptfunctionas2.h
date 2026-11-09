#pragma once

#include "engine/guis/swf/swf_scriptfunction.h"
#include "shared/idlib/containers/staticlist.h"

class idSWFBitStream;

class idSWFStack : public idList<idSWFScriptVar, 72> {
public:
  idSWFScriptVar& A() { return (*this)[Num() - 1]; }
  idSWFScriptVar& B() { return (*this)[Num() - 2]; }
  idSWFScriptVar& C() { return (*this)[Num() - 3]; }
  idSWFScriptVar& D() { return (*this)[Num() - 4]; }
  void Pop(int count);
};

class idSWFScriptFunctionAS2 : public idSWFScriptFunction {
public:
  struct parmInfo_t {
    const char* name;
    std::uint8_t reg;
  };

  idSWFScriptFunctionAS2();
  ~idSWFScriptFunctionAS2() override;
  idSWFScriptVar Call(idSWFScriptObject* thisObject, idSWFParmList& parms) override;
  idSWFScriptVar Run(idSWFScriptObject* thisObject, idSWFStack& stack,
                     idSWFBitStream& bitstream);
  void SetScope(const idList<idSWFScriptObject*>& newScope);
  void AllocRegisters(int count);
  void AllocParameters(int count);
  void SetParameter(std::uint8_t index, std::uint8_t reg, const char* name) {
    parameters[index].reg = reg;
    parameters[index].name = name;
  }

  std::uint16_t flags;
  const std::uint8_t* data;
  std::uint32_t length;
  idSWFSpriteInstance* defaultSprite;
  idStaticList<idSWFScriptObject*, 19> scope;
  idSWFConstantPoolAS2 constants;
  idList<idSWFScriptVar, 72> registers;
  idList<parmInfo_t, 72> parameters;
};

static_assert(sizeof(idSWFStack) == 24, "AS2 stack ABI drift");
static_assert(sizeof(idSWFScriptFunctionAS2::parmInfo_t) == 16,
              "AS2 parameter ABI drift");
static_assert(sizeof(idSWFScriptFunctionAS2) == 392, "AS2 function ABI drift");
