#pragma once

class idAtomicStringManager {
public:
  const char* MakeAtomic(const char* text);
  void FreeDynamic();
};

class idAtomicString {
public:
  const char* str;
  static const char* const nullAtomicString;

  idAtomicString() : str(nullAtomicString) {}
  explicit idAtomicString(const char* text) : str(nullAtomicString) { Set(text); }
  void Set(const char* const text);
  static void FreeDynamic();
  operator const char*() const { return str; }
};

extern idAtomicStringManager atomicStringManager;
extern idAtomicString atsTestStatic;
static_assert(sizeof(idAtomicString) == 8, "idAtomicString ABI drift");
