#pragma once

#include "engine/decls/decl.h"

struct idViseme {
  idViseme();
  idAtomicString name;
  idAtomicString aliasName;
  std::uint8_t viseme;
};

struct idPhoneme {
  idPhoneme();
  idAtomicString name;
  int phoneme;
  std::uint8_t viseme;
  float weightScale;
  float durationScale;
  int timeOffsetMS;
};

class idDeclVisemeSet final : public idDecl {
public:
  idDeclVisemeSet();
  ~idDeclVisemeSet() override;
  const char* DefaultDefinition() const override;
  void FreeData() override;
  idDeclInfo* GetDeclInfo() const override;
  void Parse(idParser& source) override;
  std::uint64_t Size() const override;
  std::uint8_t VisemeForName(const char* name) const;
  int PhonemeForName(const char* name) const;

  idStr phonemeSet;
  idList<idViseme, 69> visemes;
  idList<idPhoneme, 70> phonemes;
  std::uint8_t silenceViseme;
  int silencePhoneme;

  static idDeclInfoTemplate<idDeclVisemeSet> resourceList;
};

static_assert(sizeof(idViseme) == 24, "idViseme ABI drift");
static_assert(sizeof(idPhoneme) == 32, "idPhoneme ABI drift");
static_assert(sizeof(idDeclVisemeSet) == 176, "viseme-set ABI drift");
