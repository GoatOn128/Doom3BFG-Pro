#include "engine/guis/swf/swf_scriptobject.h"

#include "engine/guis/swf/swf_spriteinstance.h"
#include "engine/guis/swf/swf_textinstance.h"

#include "shared/idlib/lib.h"

#include <algorithm>
#include <cstdlib>
#include <cstring>

const char* const idSWFScriptObject::CLASS = "Object";
const char* const idSWFScriptArray::CLASS = "Array";
const char* const idSWFScriptSprite::CLASS = "MovieClip";
const char* const idSWFScriptText::CLASS = "TextField";
const char* const idSWFScriptFunction::CLASS = "Function";

namespace {
int FindSparseIndex(const idList<int, 72>& indices, int index) {
  int low = 0;
  int high = indices.Num();
  while (low < high) {
    const int middle = low + (high - low) / 2;
    if (indices[middle] < index)
      low = middle + 1;
    else
      high = middle;
  }
  return low;
}

bool ParseArrayIndex(const char* name, int& index) {
  if (name == nullptr || name[0] < '0' || name[0] > '9')
    return false;
  index = std::atol(name);
  char canonical[40];
  idStr::snPrintf(canonical, sizeof(canonical), "%d", index);
  return idStr::Cmp(name, canonical) == 0;
}
} // namespace

idSWFScriptObject::swfNamedVar_t&
idSWFScriptObject::swfNamedVar_t::operator=(const swfNamedVar_t& other) {
  if (this != &other) {
    hashNext = other.hashNext;
    name = other.name;
    value = other.value;
    native = other.native;
    flags = other.flags;
  }
  return *this;
}

idSWFScriptObject::swfNamedVar_t::~swfNamedVar_t() = default;

bool idSWFScriptObject::Delete(const char* name) {
  const int bucket = idStr::Hash(name) & 15;
  for (int index = variablesHash[bucket]; index >= 0; index = variables[index].hashNext) {
    const char* variableName = variables[index].name.str;
    if (variableName != name && idStr::Cmp(variableName, name) != 0)
      continue;
    if ((variables[index].flags & SWF_VAR_FLAG_DONTDELETE) != 0)
      return false;
    variables.RemoveIndex(index);
    std::fill(std::begin(variablesHash), std::end(variablesHash), -1);
    for (int i = 0; i < variables.Num(); ++i) {
      const int rebuiltBucket = idStr::Hash(variables[i].name.str) & 15;
      variables[i].hashNext = variablesHash[rebuiltBucket];
      variablesHash[rebuiltBucket] = i;
    }
    return true;
  }
  return true;
}

bool idSWFScriptArray::Delete(int index) {
  if (index >= 0) {
    const int sparse = FindSparseIndex(sparseIndex, index);
    if (sparse < sparseIndex.Num() && sparseIndex[sparse] == index) {
      sparseIndex.RemoveIndex(sparse);
      sparseValues.RemoveIndex(sparse);
    }
  }
  return true;
}

bool idSWFScriptArray::Delete(const char* name) {
  if (idStr::Cmp(name, "length") == 0)
    return false;
  int index = 0;
  return ParseArrayIndex(name, index) ? Delete(index) : idSWFScriptObject::Delete(name);
}

idSWFScriptVar idSWFScriptArray::Get(int index) {
  if (index < 0 || index >= length)
    return idSWFScriptVar();
  const int sparse = FindSparseIndex(sparseIndex, index);
  return sparse < sparseIndex.Num() && sparseIndex[sparse] == index ? sparseValues[sparse]
                                                                    : idSWFScriptVar();
}

void idSWFScriptObject::Clear() {
  variables.ClearFree();
  std::fill(std::begin(variablesHash), std::end(variablesHash), -1);
}

void idSWFScriptObject::PrintToConsole() {
  idList<idSWFScriptVar> names;
  EnumVariables(names);
  int maximumNameLength = 0;
  for (int i = 0; i < names.Num(); ++i)
    maximumNameLength = std::max(maximumNameLength, names[i].ToString().Length());

  if (names.Num() == 0)
    return;
  idLib::Printf("\n");
  idLib::Printf("%-2s %-*s %-10s %-s\n", "#", maximumNameLength + 2, "VarName", "Type", "Value");
  idLib::Printf("------------------------------------------------------------\n");
  for (int i = 0; i < names.Num(); ++i) {
    const idStr name = names[i].ToString();
    const idSWFScriptVar variable = Get(name.data);
    const idStr valueString = variable.ToString();
    idLib::Printf("%-2d %-*s %-10s %-s\n", i, maximumNameLength + 2, name.data,
                  variable.TypeOf(), valueString.data);
  }
}

void idSWFScriptArray::Clear() {
  idSWFScriptObject::Clear();
  sparseIndex.ClearFree();
  sparseValues.ClearFree();
}

void idSWFScriptArray::Set(int index, const idSWFScriptVar& newValue) {
  if (index < 0)
    return;
  if (length <= index)
    length = index + 1;
  const int sparse = FindSparseIndex(sparseIndex, index);
  if (sparse < sparseIndex.Num() && sparseIndex[sparse] == index) {
    sparseValues[sparse] = newValue;
  } else {
    sparseIndex.Insert(index, sparse);
    sparseValues.Insert(newValue, sparse);
  }
}

idSWFScriptObject::idSWFScriptObject()
    : refCount(1), variables(), variablesHash{}, prototype(nullptr) {
  variables.memTag = 72;
  Clear();
}

const char* idSWFScriptObject::Class() const { return CLASS; }

bool idSWFScriptObject::DerivedFrom(const char* className) const { return className == CLASS; }

idSWFScriptObject::~idSWFScriptObject() {
  if (prototype != nullptr)
    prototype->Release();
}

void idSWFScriptObject::EnumVariables(idList<idSWFScriptVar>& names) {
  for (int i = 0; i < variables.Num(); ++i) {
    if ((variables[i].flags & SWF_VAR_FLAG_DONTENUM) == 0)
      names.Append(idSWFScriptVar(variables[i].name.str));
  }
}

void idSWFScriptArray::EnumVariables(idList<idSWFScriptVar>& names) {
  idSWFScriptObject::EnumVariables(names);
  for (int i = 0; i < sparseIndex.Num(); ++i)
    names.Append(idSWFScriptVar(sparseIndex[i]));
}

idSWFScriptVar idSWFScriptObject::DefaultValue(swfTypeHint_t typeHint) {
  const char* methods[2] = {"toString", "valueOf"};
  if (typeHint == SWF_HINT_NUMBER)
    std::swap(methods[0], methods[1]);

  for (const char* methodName : methods) {
    const idSWFScriptVar method = Get(methodName);
    if (!method.IsFunction())
      continue;
    idSWFParmList parameters;
    const idSWFScriptVar value = method.GetFunction()->Call(this, parameters);
    if (value.type != idSWFScriptVar::SWF_VAR_OBJECT && !value.IsFunction())
      return value;
  }

  if (typeHint == SWF_HINT_NUMBER)
    return idSWFScriptVar();
  idStr result;
  result.Format("[object %s]", Class());
  return idSWFScriptVar(result);
}

idSWFScriptObject::swfNamedVar_t* idSWFScriptObject::GetVariable(const char* name, bool create) {
  const int bucket = idStr::Hash(name) & 15;
  for (int index = variablesHash[bucket]; index >= 0; index = variables[index].hashNext) {
    const char* variableName = variables[index].name.str;
    if (variableName == name || idStr::Cmp(variableName, name) == 0)
      return &variables[index];
  }

  if (prototype != nullptr) {
    swfNamedVar_t* inherited = prototype->GetVariable(name, false);
    if (inherited != nullptr) {
      if (inherited->native != nullptr || !create)
        return inherited;
    }
  }

  if (!create)
    return nullptr;
  swfNamedVar_t* variable = variables.Alloc();
  variable->flags = SWF_VAR_FLAG_NONE;
  variable->name.Set(name);
  variable->native = nullptr;
  variable->hashNext = variablesHash[bucket];
  variablesHash[bucket] = variables.Num() - 1;
  return variable;
}

bool idSWFScriptObject::HasProperty(const char* name) { return GetVariable(name, false) != nullptr; }

bool idSWFScriptObject::HasValidProperty(const char* name) {
  swfNamedVar_t* variable = GetVariable(name, false);
  if (variable == nullptr)
    return false;
  const idSWFScriptVar value =
      variable->native != nullptr ? variable->native->Get(this) : variable->value;
  return value.type != idSWFScriptVar::SWF_VAR_NULL && value.type != idSWFScriptVar::SWF_VAR_UNDEF;
}

idSWFScriptVar idSWFScriptObject::Get(const char* name) {
  swfNamedVar_t* variable = GetVariable(name, false);
  if (variable == nullptr)
    return idSWFScriptVar();
  return variable->native != nullptr ? variable->native->Get(this) : variable->value;
}

void idSWFScriptObject::Set(const char* name, const idSWFScriptVar& newValue) {
  swfNamedVar_t* variable = GetVariable(name, true);
  if ((variable->flags & SWF_VAR_FLAG_READONLY) != 0)
    return;
  if (variable->native != nullptr)
    variable->native->Set(this, newValue);
  else
    variable->value = newValue;
}

void idSWFScriptObject::SetNative(const char* name, idSWFScriptNativeVariable* native) {
  swfNamedVar_t* variable = GetVariable(name, true);
  variable->flags = SWF_VAR_FLAG_DONTENUM;
  variable->native = native;
  if (native->IsReadOnly())
    variable->flags |= SWF_VAR_FLAG_READONLY;
}

idSWFScriptVar idSWFScriptArray::Get(const char* name) {
  if (idStr::Cmp(name, "length") == 0)
    return idSWFScriptVar(length);
  int index = 0;
  return ParseArrayIndex(name, index) ? Get(index) : idSWFScriptObject::Get(name);
}

void idSWFScriptArray::Set(const char* name, const idSWFScriptVar& newValue) {
  if (idStr::Cmp(name, "length") != 0) {
    int index = 0;
    if (ParseArrayIndex(name, index))
      Set(index, newValue);
    else
      idSWFScriptObject::Set(name, newValue);
    return;
  }

  length = newValue.ToInt32();
  while (sparseIndex.Num() > 0 && sparseIndex[sparseIndex.Num() - 1] >= length) {
    sparseIndex.RemoveIndex(sparseIndex.Num() - 1);
    sparseValues.RemoveIndex(sparseValues.Num() - 1);
  }
}

namespace {
idSWFScriptObject* FindDisplayObject(idSWFSpriteInstance* sprite, const char* name) {
  if (sprite == nullptr)
    return nullptr;
  for (int i = 0; i < sprite->displayList.Num(); ++i) {
    swfDisplayEntry_t& entry = sprite->displayList[i];
    if (entry.spriteInstance != nullptr) {
      const char* childName = entry.spriteInstance->name.str;
      if (childName == name || (name != nullptr && idStr::Cmp(childName, name) == 0))
        return entry.spriteInstance->scriptObject;
    } else if (entry.textInstance != nullptr) {
      const char* childName = entry.textInstance->name.str;
      if (childName == name || (name != nullptr && idStr::Cmp(childName, name) == 0))
        return entry.textInstance->scriptObject;
    }
  }
  return nullptr;
}
} // namespace

idSWFScriptVar idSWFScriptSprite::Get(const char* name) {
  idSWFScriptObject* object = FindDisplayObject(sprite, name);
  return object != nullptr ? idSWFScriptVar(object) : idSWFScriptObject::Get(name);
}

void idSWFScriptSprite::Set(const char* name, const idSWFScriptVar& value) {
  if (FindDisplayObject(sprite, name) == nullptr)
    idSWFScriptObject::Set(name, value);
}

bool idSWFScriptSprite::Delete(const char* name) {
  return FindDisplayObject(sprite, name) == nullptr && idSWFScriptObject::Delete(name);
}

void idSWFScriptSprite::EnumVariables(idList<idSWFScriptVar>& names) {
  if (sprite != nullptr) {
    for (int i = 0; i < sprite->displayList.Num(); ++i) {
      const swfDisplayEntry_t& entry = sprite->displayList[i];
      const char* childName = entry.spriteInstance != nullptr
                                  ? entry.spriteInstance->name.str
                                  : (entry.textInstance != nullptr ? entry.textInstance->name.str : nullptr);
      if (childName != nullptr && childName[0] != '\0')
        names.Append(idSWFScriptVar(childName));
    }
  }
  idSWFScriptObject::EnumVariables(names);
}

void idSWFScriptSprite::PrintToConsole() { idSWFScriptObject::PrintToConsole(); }
void idSWFScriptText::PrintToConsole() { idSWFScriptObject::PrintToConsole(); }
