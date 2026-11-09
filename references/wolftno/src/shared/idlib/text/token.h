#pragma once

#include "str.h"

class idToken : public idStr {
public:
  using idStr::operator=;

  int type{};
  int subtype{};
  int line{};
  int linesCrossed{};
  int flags{};
  std::uint32_t intvalue{};
  float floatvalue{};
  const char* whiteSpaceStart_p{};
  const char* whiteSpaceEnd_p{};
  idToken* next{};

  static const char DEFAULT_WHITESPACE[2];
  void SetSafeWhiteSpacePointers();
  void InheritWhiteSpace(const idToken& other);
  void NumberValue();
  void AppendDirty(char value);
};

static_assert(sizeof(idToken) == 96, "idToken ABI drift");
