#include "engine/guis/swf/swf_scriptobject.h"

#include <cmath>
#include <cstdlib>
#include <limits>

namespace {
const char* ScriptString(const idSWFScriptVar& value) {
  if (value.type == idSWFScriptVar::SWF_VAR_STRING)
    return value.value.string->data;
  idStrId stringId;
  stringId.id = static_cast<std::uint32_t>(value.value.i);
  return stringId.GetLocalizedString();
}
} // namespace

const char* idSWFScriptVar::TypeOf() const {
  switch (type) {
  case SWF_VAR_STRINGID:
  case SWF_VAR_STRING:
    return "string";
  case SWF_VAR_DOUBLE:
  case SWF_VAR_INTEGER:
    return "number";
  case SWF_VAR_NULL:
    return "object";
  case SWF_VAR_UNDEF:
    return "undefined";
  case SWF_VAR_BOOL:
    return "boolean";
  case SWF_VAR_OBJECT:
    return value.object->Class();
  default:
    return "";
  }
}

idSWFScriptVar::idSWFScriptVar(const idSWFScriptVar& other)
    : type(other.type), value(other.value) {
  if (type == SWF_VAR_STRING)
    ++value.string->refCount;
  else if (type == SWF_VAR_OBJECT)
    value.object->AddRef();
}

bool idSWFScriptVar::ToBool() const {
  switch (type) {
  case SWF_VAR_STRINGID:
    return ScriptString(*this)[0] != '\0';
  case SWF_VAR_STRING:
    return !value.string->IsEmpty();
  case SWF_VAR_DOUBLE:
    return value.d > 0.0 || value.d < 0.0;
  case SWF_VAR_INTEGER:
    return value.i != 0;
  case SWF_VAR_BOOL:
    return value.b;
  case SWF_VAR_OBJECT:
    return true;
  default:
    return false;
  }
}

bool idSWFScriptVar::IsFunction() const {
  return type == SWF_VAR_OBJECT && value.object != nullptr &&
         value.object->DerivedFrom(idSWFScriptFunction::CLASS);
}

idSWFScriptFunction* idSWFScriptVar::GetFunction() const {
  return IsFunction() ? static_cast<idSWFScriptFunction*>(value.object) : nullptr;
}

idSWFSpriteInstance* idSWFScriptVar::GetSprite() const {
  if (type != SWF_VAR_OBJECT || value.object == nullptr ||
      !value.object->DerivedFrom(idSWFScriptSprite::CLASS))
    return nullptr;
  return static_cast<idSWFScriptSprite*>(value.object)->sprite;
}

idSWFTextInstance* idSWFScriptVar::GetText() const {
  if (type != SWF_VAR_OBJECT || value.object == nullptr ||
      !value.object->DerivedFrom(idSWFScriptText::CLASS))
    return nullptr;
  return static_cast<idSWFScriptText*>(value.object)->text;
}

void idSWFScriptVar::Free() {
  if (type == SWF_VAR_STRING) {
    if (value.string->refCount-- == 1)
      delete value.string;
  } else if (type == SWF_VAR_OBJECT) {
    value.object->Release();
  }
  type = SWF_VAR_UNDEF;
}

void idSWFScriptVar::SetObject(idSWFScriptObject* object) {
  Free();
  if (object == nullptr) {
    type = SWF_VAR_NULL;
    return;
  }
  type = SWF_VAR_OBJECT;
  value.object = object;
  object->AddRef();
}

idSWFScriptVar& idSWFScriptVar::operator=(const idSWFScriptVar& other) {
  if (this == &other)
    return *this;
  Free();
  type = other.type;
  value = other.value;
  if (type == SWF_VAR_STRING)
    ++value.string->refCount;
  else if (type == SWF_VAR_OBJECT)
    value.object->AddRef();
  return *this;
}

idSWFScriptVar::~idSWFScriptVar() { Free(); }

idSWFScriptVar idSWFScriptVar::ToPrimitive(swfTypeHint_t typeHint) const {
  if (type == SWF_VAR_OBJECT)
    return value.object->DefaultValue(typeHint);
  return *this;
}

double idSWFScriptVar::ToNumber() const {
  switch (type) {
  case SWF_VAR_STRINGID:
  case SWF_VAR_STRING: {
    const char* string = ScriptString(*this);
    if (idStr::Cmp(string, "Infinity") == 0 || idStr::Cmp(string, "+Infinity") == 0)
      return std::numeric_limits<double>::infinity();
    if (idStr::Cmp(string, "-Infinity") == 0)
      return -std::numeric_limits<double>::infinity();
    char* end = nullptr;
    const double number = std::strtod(string, &end);
    return end == string ? std::numeric_limits<double>::quiet_NaN() : number;
  }
  case SWF_VAR_DOUBLE:
    return value.d;
  case SWF_VAR_INTEGER:
    return static_cast<double>(static_cast<int>(value.i));
  case SWF_VAR_UNDEF:
    return std::numeric_limits<double>::quiet_NaN();
  case SWF_VAR_BOOL:
    return value.b ? 1.0 : 0.0;
  case SWF_VAR_OBJECT:
    return value.object->DefaultValue(SWF_HINT_NUMBER).ToNumber();
  default:
    return 0.0;
  }
}

std::int64_t idSWFScriptVar::ToInteger() const {
  switch (type) {
  case SWF_VAR_STRINGID:
  case SWF_VAR_STRING: {
    const char* string = ScriptString(*this);
    if (idStr::Cmp(string, "Infinity") == 0 || idStr::Cmp(string, "+Infinity") == 0)
      return std::numeric_limits<std::int64_t>::max();
    if (idStr::Cmp(string, "-Infinity") == 0)
      return std::numeric_limits<std::int64_t>::min();
    return _atoi64(string);
  }
  case SWF_VAR_DOUBLE:
    return static_cast<std::uint32_t>(static_cast<int>(value.d));
  case SWF_VAR_INTEGER:
    return value.i;
  case SWF_VAR_BOOL:
    return value.b ? 1 : 0;
  case SWF_VAR_OBJECT:
    return value.object->DefaultValue(SWF_HINT_NUMBER).ToInteger();
  default:
    return 0;
  }
}

int idSWFScriptVar::ToInt32() const { return static_cast<int>(ToInteger()); }

idStr idSWFScriptVar::ToString() const {
  switch (type) {
  case SWF_VAR_STRINGID:
    return ScriptString(*this);
  case SWF_VAR_STRING:
    return *value.string;
  case SWF_VAR_DOUBLE:
    if (value.d == std::numeric_limits<double>::infinity())
      return "Infinity";
    if (value.d == -std::numeric_limits<double>::infinity())
      return "-Infinity";
    {
      idStr result;
      result.Format("%g", value.d);
      return result;
    }
  case SWF_VAR_INTEGER:
    if (value.i == std::numeric_limits<std::int64_t>::max())
      return "Infinity";
    if (value.i == std::numeric_limits<std::int64_t>::min())
      return "-Infinity";
    {
      idStr result;
      result.Format("%lld", static_cast<long long>(value.i));
      return result;
    }
  case SWF_VAR_NULL:
    return "null";
  case SWF_VAR_UNDEF:
    return "undefined";
  case SWF_VAR_BOOL:
    return value.b ? "true" : "false";
  case SWF_VAR_OBJECT:
    return value.object->DefaultValue(SWF_HINT_STRING).ToString();
  default:
    return "";
  }
}

bool idSWFScriptVar::Equals(const idSWFScriptVar& other, bool strict) const {
  if (type == SWF_VAR_UNDEF || type == SWF_VAR_NULL)
    return other.type == SWF_VAR_UNDEF || other.type == SWF_VAR_NULL;
  if (other.type == SWF_VAR_UNDEF || other.type == SWF_VAR_NULL)
    return false;

  if (IsString() && other.IsString())
    return idStr::Cmp(ScriptString(*this), ScriptString(other)) == 0;

  if ((type == SWF_VAR_DOUBLE || type == SWF_VAR_INTEGER) &&
      (other.type == SWF_VAR_DOUBLE || other.type == SWF_VAR_INTEGER)) {
    if (type == SWF_VAR_DOUBLE) {
      return other.type == SWF_VAR_DOUBLE
                 ? value.d == other.value.d
                 : value.d == static_cast<double>(static_cast<int>(other.value.i));
    }
    return other.type == SWF_VAR_INTEGER
               ? value.i == other.value.i
               : static_cast<double>(static_cast<int>(value.i)) == other.value.d;
  }

  if (type == SWF_VAR_BOOL && other.type == SWF_VAR_BOOL)
    return value.b == other.value.b;
  if (type == SWF_VAR_OBJECT && other.type == SWF_VAR_OBJECT)
    return value.object == other.value.object;
  if (strict)
    return false;

  if ((type == SWF_VAR_DOUBLE || type == SWF_VAR_INTEGER) && other.IsString())
    return type == SWF_VAR_DOUBLE ? value.d == other.ToNumber() : value.i == other.ToInteger();
  if (IsString() && (other.type == SWF_VAR_DOUBLE || other.type == SWF_VAR_INTEGER))
    return other.type == SWF_VAR_DOUBLE ? ToNumber() == other.value.d
                                        : ToInteger() == other.value.i;

  if (type == SWF_VAR_BOOL) {
    idSWFScriptVar converted(static_cast<int>(value.b));
    return converted.Equals(other, false);
  }
  if (other.type == SWF_VAR_BOOL) {
    idSWFScriptVar converted(static_cast<int>(other.value.b));
    return Equals(converted, false);
  }
  if (other.type == SWF_VAR_OBJECT)
    return Equals(other.ToPrimitive(SWF_HINT_NONE), false);
  if (type == SWF_VAR_OBJECT)
    return ToPrimitive(SWF_HINT_NONE).Equals(other, false);
  return false;
}

