#pragma once

#include "shared/idlib/containers/list.h"
#include "shared/idlib/text/atomicstring.h"

#include <cstdint>

class idLangDict {
public:
  struct langEntry_t {
    std::uint32_t id = 0;
    idAtomicString key;
    idAtomicString value;
  };

  static bool HasStrPrefix(const char* text);
  static std::uint32_t ComputeID(const char* key);
  int IndexFromKey(const char* key) const;
  const char* FindString(const char* key) const;
  const char* GetString(const char* key) const;
  bool Load(const char* buffer, int bufferLength, const char* name);
  bool Save(const char* fileName) const;
  bool Add(const char* key, const char* value);
  void Clear() { dictionary.SetNum(0); }
  int Num() const { return dictionary.Num(); }

  idList<langEntry_t, 119> dictionary;
};

namespace idLocalization {
extern idLangDict languageDict;
}

class idStrId {
public:
  idStrId() : id(0) {}
  explicit idStrId(const char* key) : id(0) { Set(key); }
  void Set(const char* key);
  const char* GetKey() const;
  const char* GetLocalizedString() const;
  bool IsValid() const { return id != 0; }
  std::uint32_t id;
};

static_assert(sizeof(idLangDict::langEntry_t) == 24, "lang entry ABI drift");
static_assert(sizeof(idLangDict) == 24, "idLangDict ABI drift");
static_assert(sizeof(idStrId) == 4, "idStrId ABI drift");
