#pragma once

#include "engine/guis/swf/swf_scriptobject.h"
#include "shared/idlib/containers/list.h"

class idSWF;

class idSWFScriptFunction_NestedSWF : public idSWFScriptFunction {
public:
  idSWFScriptFunction_NestedSWF() : pThis(nullptr) {}
  idSWF* pThis;
};

class idSWFScriptNativeVariable_NestedSWF : public idSWFScriptNativeVariable {
public:
  idSWFScriptNativeVariable_NestedSWF() : pThis(nullptr) {}
  idSWF* pThis;
};

static_assert(sizeof(idSWFScriptFunction_NestedSWF) == 120,
              "nested SWF function ABI drift");
static_assert(sizeof(idSWFScriptNativeVariable_NestedSWF) == 16,
              "nested SWF native variable ABI drift");

class idSWFConstantPoolAS2 {
public:
  idSWFConstantPoolAS2();
  ~idSWFConstantPoolAS2(); // implemented with the AS2 interpreter compiland
  void Clear();
  void Copy(const idSWFConstantPoolAS2& other);

  idList<idStrRefCounted*, 72> pool;
};

static_assert(sizeof(idSWFConstantPoolAS2) == 24, "idSWFConstantPoolAS2 ABI drift");
