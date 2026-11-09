#pragma once

#include "shared/idlib/langdict.h"
#include "shared/idlib/text/str.h"

#include <cstdint>

class idSWFScriptFunction;
class idSWFScriptObject;
class idSWFSpriteInstance;
class idSWFTextInstance;

enum swfTypeHint_t {
  SWF_HINT_NONE = 0,
  SWF_HINT_NUMBER = 1,
  SWF_HINT_STRING = 2,
};

class idStrRefCounted : public idStr {
public:
  idStrRefCounted() : refCount(1) {}
  explicit idStrRefCounted(const idStr& value) : idStr(value), refCount(1) {}
  explicit idStrRefCounted(const char* value) : idStr(value), refCount(1) {}

  int refCount;
};

class idSWFScriptVar {
public:
  enum swfScriptVarType {
    SWF_VAR_STRINGID = 0,
    SWF_VAR_STRING = 1,
    SWF_VAR_DOUBLE = 2,
    SWF_VAR_INTEGER = 3,
    SWF_VAR_NULL = 4,
    SWF_VAR_UNDEF = 5,
    SWF_VAR_BOOL = 6,
    SWF_VAR_OBJECT = 7,
  };

  union swfScriptVarValue_t {
    double d;
    std::int64_t i;
    bool b;
    idSWFScriptObject* object;
    idStrRefCounted* string;
  };

  idSWFScriptVar() : type(SWF_VAR_UNDEF), value{} {}
  idSWFScriptVar(const idSWFScriptVar& other);
  explicit idSWFScriptVar(idSWFScriptObject* object) : type(SWF_VAR_UNDEF), value{} {
    SetObject(object);
  }
  explicit idSWFScriptVar(const char* string) : type(SWF_VAR_UNDEF), value{} {
    SetString(string);
  }
  explicit idSWFScriptVar(const idStr& string) : type(SWF_VAR_UNDEF), value{} {
    SetString(string);
  }
  explicit idSWFScriptVar(const va& string) : type(SWF_VAR_UNDEF), value{} {
    SetString(string);
  }
  explicit idSWFScriptVar(float number) : type(SWF_VAR_DOUBLE), value{} { value.d = number; }
  explicit idSWFScriptVar(double number) : type(SWF_VAR_DOUBLE), value{} { value.d = number; }
  explicit idSWFScriptVar(bool boolean) : type(SWF_VAR_BOOL), value{} { value.b = boolean; }
  explicit idSWFScriptVar(int integer) : type(SWF_VAR_INTEGER), value{} { value.i = integer; }
  ~idSWFScriptVar();

  idSWFScriptVar& operator=(const idSWFScriptVar& other);

  void SetString(idStrId string) {
    Free();
    type = SWF_VAR_STRINGID;
    value.i = string.id;
  }
  void SetString(const char* string) {
    Free();
    type = SWF_VAR_STRING;
    value.string = new idStrRefCounted(string);
  }
  void SetString(const idStr& string) {
    Free();
    type = SWF_VAR_STRING;
    value.string = new idStrRefCounted(string);
  }
  void SetString(const va& string) { SetString(static_cast<const char*>(string)); }
  void SetFloat(float number) {
    Free();
    type = SWF_VAR_DOUBLE;
    value.d = number;
  }
  void SetDouble(double number) {
    Free();
    type = SWF_VAR_DOUBLE;
    value.d = number;
  }
  void SetInteger(int integer) {
    Free();
    type = SWF_VAR_INTEGER;
    value.i = integer;
  }
  void SetInteger64(std::int64_t integer) {
    Free();
    type = SWF_VAR_INTEGER;
    value.i = integer;
  }
  void SetBool(bool boolean) {
    Free();
    type = SWF_VAR_BOOL;
    value.b = boolean;
  }
  void SetNULL() {
    Free();
    type = SWF_VAR_NULL;
  }
  void SetUndefined() { Free(); }
  void SetObject(idSWFScriptObject* object);

  idSWFScriptVar ToPrimitive(swfTypeHint_t typeHint = SWF_HINT_NONE) const;
  idStr ToString() const;
  double ToNumber() const;
  std::int64_t ToInteger() const;
  int ToInt32() const;
  bool ToBool() const;
  const char* TypeOf() const;
  bool Equals(const idSWFScriptVar& other, bool strict) const;

  bool IsString() const { return type == SWF_VAR_STRINGID || type == SWF_VAR_STRING; }
  bool IsNULL() const { return type == SWF_VAR_NULL; }
  bool IsUndefined() const { return type == SWF_VAR_UNDEF; }
  bool IsValid() const { return type != SWF_VAR_NULL && type != SWF_VAR_UNDEF; }
  bool IsObject() const { return type == SWF_VAR_OBJECT; }
  bool IsNumeric() const {
    return type == SWF_VAR_DOUBLE || type == SWF_VAR_INTEGER || type == SWF_VAR_BOOL;
  }
  bool IsFunction() const;
  idSWFScriptFunction* GetFunction() const;
  idSWFSpriteInstance* GetSprite() const;
  idSWFTextInstance* GetText() const;
  idSWFScriptObject* GetObject() const { return value.object; }
  swfScriptVarType GetType() const { return type; }

  void Free();

  swfScriptVarType type;
  swfScriptVarValue_t value;
};

static_assert(sizeof(idStrRefCounted) == 48, "idStrRefCounted ABI drift");
static_assert(sizeof(idSWFScriptVar) == 16, "idSWFScriptVar ABI drift");
