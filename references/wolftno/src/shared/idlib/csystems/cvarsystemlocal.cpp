#include "shared/idlib/csystems/cvarsystemlocal.h"

#include "shared/idlib/csystems/cmdsystemlocal.h"
#include "shared/idlib/lib.h"

#include <algorithm>
#include <cmath>
#include <cstdlib>
#include <string>

namespace {
idCVar* staticVars = nullptr;
char* CopyText(const char* text) {
  const int n = idStr::Length(text ? text : "");
  char* p = new char[n + 1];
  idStr::Copynz(p, text ? text : "", n + 1);
  return p;
}
} // namespace
idCVarSystemLocal cvarSystemLocal;
idCVarSystem* cvarSystem = &cvarSystemLocal;

idCVar::idCVar() = default;
idCVar::idCVar(const char* n, const char* v, int f, const char* d,
               argCompletion_t completion) {
  if (completion == nullptr && (f & CVAR_BOOL) != 0) {
    completion = idCmdSystem::ArgCompletion_Boolean;
  }
  Init(n, v, f, d, 1.0f, -1.0f, nullptr, completion);
}
idCVar::idCVar(const char* n, const char* v, int f, const char* d,
               float min, float max, argCompletion_t completion) {
  Init(n, v, f, d, min, max, nullptr, completion);
}
idCVar::idCVar(const char* n, const char* v, int f, const char* d,
               const char** values, argCompletion_t completion) {
  Init(n, v, f, d, 1.0f, -1.0f, values, completion);
}
idCVar::idCVar(const char* n, const char* v, int f, const char* d,
               const char** values, argCompletionRef_t completion) {
  Init(n, v, f, d, 1.0f, -1.0f, values,
       reinterpret_cast<argCompletion_t>(completion));
}
idCVar::idCVar(const char* n, const char* v, int f, const char* d,
               float min, float max, const char** values,
               argCompletionRef_t completion) {
  Init(n, v, f, d, min, max, values,
       reinterpret_cast<argCompletion_t>(completion));
}
void idCVar::Init(const char* n, const char* v, int f, const char* d, float min, float max,
                  const char** values, argCompletion_t completion) {
  name = CopyText(n);
  resetString = CopyText(v);
  description = CopyText(d);
  flags = f;
  valueMin = min;
  valueMax = max;
  valueStrings = values;
  valueCompletion = completion;
  valueString = v ? v : "";
  UpdateValue();
  next = staticVars;
  staticVars = this;
}
void idCVar::UpdateValue() {
  if (valueStrings) {
    int found = 0;
    for (int i = 0; valueStrings[i]; ++i)
      if (idStr::Icmp(valueString.data, valueStrings[i]) == 0) {
        found = i;
        break;
      }
    valueInteger = found;
    valueFloat = static_cast<float>(found);
    valueString = valueStrings[found];
    return;
  }
  valueFloat = std::strtof(valueString.data, nullptr);
  valueInteger = static_cast<int>(valueFloat);
  if (flags & CVAR_BOOL) {
    valueInteger = valueFloat != 0.0f;
    valueFloat = static_cast<float>(valueInteger);
    valueString = valueInteger ? "1" : "0";
  } else if ((flags & (CVAR_INTEGER | CVAR_FLOAT)) && valueMin < valueMax) {
    valueFloat = std::clamp(valueFloat, valueMin, valueMax);
    valueInteger = static_cast<int>(valueFloat);
    if (flags & CVAR_INTEGER)
      valueString = idStr(valueInteger);
    else
      valueString = idStr(valueFloat);
  }
}
void idCVar::Reset() {
  Set(resetString, true);
}
void idCVar::Set(const char* value, bool force) {
  if (!force && (flags & (CVAR_ROM | CVAR_INIT)))
    return;
  if (idStr::Cmp(valueString.data, value ? value : "") == 0)
    return;
  valueString = value ? value : "";
  flags |= CVAR_MODIFIED;
  UpdateValue();
  for (cvarCallback_t* c = onChange; c; c = c->next)
    if (c->callback)
      c->callback(*this);
}
void idCVar::SetString(const char* v, bool force) {
  Set(v, force);
}
void idCVar::SetBool(bool v, bool force) {
  Set(v ? "1" : "0", force);
}
void idCVar::SetInteger(int v, bool force) {
  Set(idStr(v).data, force);
}
void idCVar::SetFloat(float v, bool force) {
  Set(idStr(v).data, force);
}
idCVar* idCVar::StaticVars() {
  return staticVars;
}
void idCVar::RegisterStaticVars() {
  if (cvarSystem)
    cvarSystem->RegisterStaticVars();
}

idScopeCVar::idScopeCVar(idCVar* variable, const char* newValue) {
  oldStr = variable->GetString();
  if (idStr::Cmp(oldStr.data, newValue) != 0) {
    cvar = variable;
    cvarType = 0;
    variable->SetString(newValue, true);
  }
}

idScopeCVar::idScopeCVar(idCVar* variable, bool newValue) {
  oldBool = variable->GetBool();
  if (oldBool != newValue) {
    cvar = variable;
    cvarType = 1;
    variable->SetBool(newValue, true);
  }
}

idScopeCVar::idScopeCVar(idCVar* variable, int newValue) {
  oldInt = variable->GetInteger();
  if (oldInt != newValue) {
    cvar = variable;
    cvarType = 2;
    variable->SetInteger(newValue, true);
  }
}

idScopeCVar::idScopeCVar(idCVar* variable, float newValue) {
  oldFloat = variable->GetFloat();
  if (oldFloat != newValue) {
    cvar = variable;
    cvarType = 3;
    variable->SetFloat(newValue, true);
  }
}

idScopeCVar::~idScopeCVar() {
  if (cvar != nullptr) {
    switch (cvarType) {
    case 0:
      cvar->SetString(oldStr.data, true);
      break;
    case 1:
      cvar->SetInteger(oldBool, true);
      break;
    case 2:
      cvar->SetInteger(oldInt, true);
      break;
    default:
      break;
    }
  }
}

idCVarSystemLocal::idCVarSystemLocal() : cvarHash(1024, 128) {
  cvarHash.InternalAllocate(1024, 128);
}
void idCVarSystemLocal::Add(idCVar* variable) {
  if (!variable || Find(variable->name))
    return;
  const int index = cvars.Append(variable);
  cvarHash.Add(idStr::IHash(variable->name), index);
}
const idCVar* idCVarSystemLocal::FindByIndex(int index) const {
  return index >= 0 && index < cvars.Num() ? cvars[index] : nullptr;
}
void idCVarSystemLocal::ResetCVarByIndex(int index) {
  if (index >= 0 && index < cvars.Num())
    cvars[index]->Reset();
}
idCVar* idCVarSystemLocal::Find(const char* name) const {
  if (!name)
    return nullptr;
  const int hash = idStr::IHash(name);
  for (int i = cvarHash.First(hash); i != -1; i = cvarHash.Next(i))
    if (i < cvars.Num() && idStr::Icmp(cvars[i]->name, name) == 0)
      return cvars[i];
  return nullptr;
}
bool idCVarSystemLocal::CvarExists(const char* name, bool substring) const {
  if (!substring)
    return Find(name) != nullptr;
  for (int i = 0; i < cvars.Num(); ++i)
    if (idStr::Find(name, cvars[i]->name, false) != -1)
      return true;
  return false;
}
void idCVarSystemLocal::FindCvarsByPrefix(const char* prefix, idList<idCVar*>& out) const {
  const int length = idStr::Length(prefix);
  for (int i = 0; i < cvars.Num(); ++i)
    if (idStr::Icmpn(cvars[i]->name, prefix, length) == 0)
      out.Append(cvars[i]);
}
void idCVarSystemLocal::FindCvarsByPrefix(const char* prefix, idList<idCVar*>& out) {
  static_cast<const idCVarSystemLocal*>(this)->FindCvarsByPrefix(prefix, out);
}
void idCVarSystemLocal::SetCVarString(const char* name, const char* value, int flags) {
  idCVar* v = Find(name);
  if (!v) {
    v = new idCVar(name, value, flags, "dynamically created cvar");
    Add(v);
  } else
    v->Set(value);
  modifiedFlags |= v->flags | flags;
}
void idCVarSystemLocal::SetCVarBool(const char* n, bool v, int f) {
  SetCVarString(n, v ? "1" : "0", f | CVAR_BOOL);
}
void idCVarSystemLocal::SetCVarInteger(const char* n, int v, int f) {
  SetCVarString(n, idStr(v).data, f | CVAR_INTEGER);
}
void idCVarSystemLocal::SetCVarFloat(const char* n, float v, int f) {
  SetCVarString(n, idStr(v).data, f | CVAR_FLOAT);
}
const char* idCVarSystemLocal::GetCVarString(const char* n, const char* d) const {
  const idCVar* v = Find(n);
  return v ? v->GetString() : d;
}
bool idCVarSystemLocal::GetCVarBool(const char* n, bool d) const {
  const idCVar* v = Find(n);
  return v ? v->GetBool() : d;
}
int idCVarSystemLocal::GetCVarInteger(const char* n, int d) const {
  const idCVar* v = Find(n);
  return v ? v->GetInteger() : d;
}
float idCVarSystemLocal::GetCVarFloat(const char* n, float d) const {
  const idCVar* v = Find(n);
  return v ? v->GetFloat() : d;
}
bool idCVarSystemLocal::Command(const idCmdArgs& a) {
  idCVar* v = Find(a.argc ? a.argv[0] : nullptr);
  if (!v)
    return false;
  if (a.argc == 1)
    idLib::Printf("%s = \"%s\" (default \"%s\")\n", v->name, v->GetString(), v->resetString);
  else {
    v->Set(a.Args(1));
    modifiedFlags |= v->flags;
  }
  return true;
}
void idCVarSystemLocal::CommandCompletion(idAutoComplete* c) const {
  if (!c)
    return;
  for (int i = 0; i < cvars.Num(); ++i)
    c->Append(idStr(cvars[i]->name), 0);
}
void idCVarSystemLocal::ResetFlaggedVariables(int flags) {
  for (int i = 0; i < cvars.Num(); ++i)
    if (flags == 0 || (cvars[i]->flags & flags))
      cvars[i]->Reset();
}
void idCVarSystemLocal::WriteFlaggedVariables(int flags, idFile* file) const {
  if (!file)
    return;
  for (int i = 0; i < cvars.Num(); ++i)
    if (flags == 0 || (cvars[i]->flags & flags))
      file->Printf("set %s \"%s\"\n", cvars[i]->name, cvars[i]->GetString());
}
void idCVarSystemLocal::ReportModifiedCVars() const {
  for (int i = 0; i < cvars.Num(); ++i)
    if (cvars[i]->flags & CVAR_MODIFIED)
      idLib::Printf("%s = %s\n", cvars[i]->name, cvars[i]->GetString());
}
void idCVarSystemLocal::ClearModifiedCVars() {
  for (int i = 0; i < cvars.Num(); ++i)
    cvars[i]->flags &= ~CVAR_MODIFIED;
  modifiedFlags = 0;
}
void idCVarSystemLocal::RegisterStaticVars() {
  for (idCVar* v = staticVars; v; v = v->next)
    Add(v);
}
void idCVarSystemLocal::ListCVars(const idCmdArgs&) const {
  for (int i = 0; i < cvars.Num(); ++i)
    idLib::Printf("%-32s %s\n", cvars[i]->name, cvars[i]->GetString());
}
