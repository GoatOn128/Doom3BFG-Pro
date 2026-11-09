#include "shared/idlib/typeinfo/typeinfo.h"

#include <algorithm>
#include <cstring>

void idTypeInfoTools::Init() {
  ClearWarnings();
}
void idTypeInfoTools::Shutdown() {
  enums.clear();
  classes.clear();
  warnings.clear();
}
void idTypeInfoTools::AddEnum(const idEnumInfo& e) {
  enums.push_back(e);
}
void idTypeInfoTools::AddClass(const idClassInfo& c) {
  classes.push_back(c);
}
int idTypeInfoTools::FindEnumIndex(const char* n) const {
  for (int i = 0; i < static_cast<int>(enums.size()); ++i)
    if (enums[i].name == n)
      return i;
  return -1;
}
const idEnumInfo* idTypeInfoTools::FindEnumInfo(const char* n) const {
  const int i = FindEnumIndex(n);
  return i >= 0 ? &enums[i] : nullptr;
}
const idEnumValueInfo* idTypeInfoTools::FindEnumValueInfo(const idEnumInfo& e, int value) const {
  for (const auto& v : e.values)
    if (v.value == value)
      return &v;
  return nullptr;
}
const idEnumValueInfo* idTypeInfoTools::FindEnumValueInfo(
    const idEnumInfo& e, const char* name, bool allowScopedName) const {
  if (name == nullptr)
    return nullptr;
  const char* valueName = name;
  if (allowScopedName) {
    const char* scope = std::strrchr(name, ':');
    if (scope != nullptr)
      valueName = scope + 1;
  }
  for (const auto& value : e.values) {
    if (idStr::Icmp(value.name.data, name) == 0 ||
        (allowScopedName && idStr::Icmp(value.name.data, valueName) == 0))
      return &value;
  }
  return nullptr;
}
const char* idTypeInfoTools::FindEnumValueName(const idEnumInfo& e, int value) const {
  const auto* v = FindEnumValueInfo(e, value);
  return v ? v->name.data : "";
}
int idTypeInfoTools::FindClassIndex(const char* n) const {
  for (int i = 0; i < static_cast<int>(classes.size()); ++i)
    if (classes[i].name == n)
      return i;
  return -1;
}
const idClassInfo* idTypeInfoTools::FindClassInfo(const char* n) const {
  const int i = FindClassIndex(n);
  return i >= 0 ? &classes[i] : nullptr;
}
const idClassVariableInfo* idTypeInfoTools::FindClassVariableInfo(const idClassInfo& c,
                                                                  const char* n) const {
  for (const auto& v : c.variables)
    if (v.name == n)
      return &v;
  return nullptr;
}
std::size_t idTypeInfoTools::SizeForType(const char* t) const {
  if (!t)
    return 0;
  if (idStr::Icmp(t, "bool") == 0 || idStr::Icmp(t, "char") == 0)
    return 1;
  if (idStr::Icmp(t, "short") == 0)
    return 2;
  if (idStr::Icmp(t, "int") == 0 || idStr::Icmp(t, "float") == 0)
    return 4;
  if (idStr::Icmp(t, "double") == 0 || idStr::Find(t, "*", true) != -1)
    return 8;
  if (idStr::Icmp(t, "idVec3") == 0)
    return sizeof(idVec3);
  const auto* c = FindClassInfo(t);
  return c ? c->size : 0;
}
bool idTypeInfoTools::IsSubclassOf(const char* child, const char* parent) const {
  for (const idClassInfo* c = FindClassInfo(child); c; c = FindClassInfo(c->super.data))
    if (c->name == parent)
      return true;
  return false;
}
bool idTypeInfoTools::WriteObject(idTypeInfoFile& file, const void* object,
                                  const char* type) const {
  const auto* c = FindClassInfo(type);
  if (!c || !object)
    return false;
  file.WriteType(type, "object");
  file.WriteOpeningBrace();
  for (const auto& v : c->variables) {
    file.WriteType(v.type.data, v.name.data);
    file.WriteMemory(static_cast<const std::uint8_t*>(object) + v.offset,
                     v.size ? v.size : SizeForType(v.type.data));
  }
  file.WriteClosingBrace();
  return true;
}
bool idTypeInfoTools::ReadObject(idTypeInfoFile& file, void* object, const char* type) const {
  const auto* c = FindClassInfo(type);
  if (!c || !object || !file.ExpectType(type, "object") || !file.ReadOpeningBrace())
    return false;
  for (const auto& v : c->variables) {
    if (!file.ExpectType(v.type.data, v.name.data) ||
        !file.ReadMemory(static_cast<std::uint8_t*>(object) + v.offset,
                         v.size ? v.size : SizeForType(v.type.data)))
      return false;
  }
  return file.ReadClosingBrace();
}
void idTypeInfoTools::ClearWarnings() {
  warnings.clear();
}
void idTypeInfoTools::AddWarning(const char* w) {
  warnings.emplace_back(w ? w : "");
}
