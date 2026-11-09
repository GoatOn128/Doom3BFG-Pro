#include "shared/idlib/typeinfo/typeinfo.h"

#include <cstdio>
#include <cstring>

idStr idTypeInfoObject::GetValueText(const char* member) const {
  if (!object || !type)
    return {};
  for (const auto& v : type->variables)
    if (v.name == member) {
      const auto* data = static_cast<const std::uint8_t*>(object) + v.offset;
      if (v.type == "int") {
        int value;
        std::memcpy(&value, data, 4);
        return idStr(value);
      }
      if (v.type == "float") {
        float value;
        std::memcpy(&value, data, 4);
        return idStr(value);
      }
      if (v.type == "bool")
        return idStr(*data != 0);
      char text[128];
      std::snprintf(text, sizeof(text), "%s@+%llu", v.type.data,
                    static_cast<unsigned long long>(v.offset));
      return idStr(text);
    }
  return {};
}
const char* idTypeInfoObject::GetStrPtrType(const char* member) const {
  if (!type)
    return "";
  for (const auto& v : type->variables)
    if (v.name == member && idStr::Find(v.type.data, "idStr", false) != -1)
      return v.type.data;
  return "";
}
