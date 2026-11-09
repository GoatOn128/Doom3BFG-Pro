#pragma once

#include "shared/idlib/containers/hashindex.h"
#include "shared/idlib/containers/list.h"
#include "shared/idlib/text/str.h"

#include <cstdint>

class idFile;
class idLexer;

struct idKeyValue {
  idStr key;
  idStr value;
  const idStr& GetKey() const { return key; }
  const idStr& GetValue() const { return value; }
};

class idDict {
public:
  idDict();
  idDict(const idDict& other);
  ~idDict() = default;
  idDict& operator=(const idDict& other);

  void Print() const;
  const idKeyValue* FindKey(const char* key) const;
  int FindKeyIndex(const char* key) const;
  bool Set(const char* key, const char* value);
  bool Set(const char* key, const idStr& value) { return Set(key, value.data); }
  void SetInt(const char* key, int value);
  void SetFloat(const char* key, float value);
  void SetBool(const char* key, bool value);
  const char* GetString(const char* key, const char* defaultValue = "") const;
  bool GetString(const char* key, const char* defaultValue, const char** output) const;
  bool GetFloat(const char* key, float defaultValue, float* output) const;
  bool GetInt(const char* key, int defaultValue, int* output) const;
  bool GetBool(const char* key, bool defaultValue, bool* output) const;
  void Delete(const char* key);
  void Clear();
  int GetNumKeyVals() const { return args.Num(); }
  const idKeyValue* GetKeyVal(int index) const {
    return index >= 0 && index < args.Num() ? &args[index] : nullptr;
  }
  std::uint32_t Checksum() const;

  void WriteToIniFile(idFile* file) const;
  bool ReadFromIniFile(idFile* file);
  idStr WriteToJSONString() const;
  idStr& WriteToJSONString(idStr& result) const;
  bool ReadFromJSONString(const idStr& text);
  bool Parse(idLexer* parser, const char* sectionName = nullptr);

  idList<idKeyValue, 5> args;
  idHashIndex argHash;

private:
  void RebuildHash();
  bool ReadJSONObject(const idStr& parentName, idLexer* parser);
};

idStr GetNameFromLocationInPath(const idStr& path, int& location);
bool IsChild(const idStr& newObjectPath, const idStr& previousObjectPath);

static_assert(sizeof(idKeyValue) == 80, "idKeyValue ABI drift");
static_assert(sizeof(idDict) == 64, "idDict ABI drift");
