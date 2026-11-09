#pragma once

#include "lexer.h"

#include <cstddef>
#include <cstdint>

// Annosoft's control words and numbers are ASCII; payload text remains UTF-8.
class idLexerUTF8 {
public:
  explicit idLexerUTF8(int lexerFlags = 0)
      : flags(lexerFlags), buffer(nullptr), cur(nullptr), end(nullptr), prev(nullptr), len(0),
        lastline(1), line(1), punctuations(nullptr), punctuationtable(nullptr),
        nextpunctuation(nullptr), loaded(false), hadError(false), hadWarning(false) {}

  bool LoadMemory(const void* source, std::size_t sourceLength, const char* sourceName) {
    filename = sourceName ? sourceName : "memory";
    buffer = static_cast<const std::uint8_t*>(source);
    cur = buffer;
    end = buffer ? buffer + sourceLength : nullptr;
    prev = buffer;
    len = sourceLength;
    lastline = line = 1;
    loaded = buffer != nullptr || sourceLength == 0;
    hadError = false;
    hadWarning = false;
    return loaded;
  }

  bool ReadLine(idStr& result) {
    result.Clear();
    if (!loaded || cur == nullptr || cur >= end)
      return false;
    prev = cur;
    lastline = line;
    const std::uint8_t* first = cur;
    while (cur < end && *cur != '\r' && *cur != '\n')
      ++cur;
    result.CopyRange(reinterpret_cast<const char*>(first), 0,
                     static_cast<int>(cur - first));
    if (cur < end && *cur == '\r')
      ++cur;
    if (cur < end && *cur == '\n')
      ++cur;
    ++line;
    return true;
  }

  bool IsLoaded() const { return loaded; }
  bool EndOfFile() const { return !loaded || cur == nullptr || cur >= end; }
  bool HadError() const { return hadError; }
  bool HadWarning() const { return hadWarning; }

  idStr filename;
  int flags;
  const std::uint8_t* buffer;
  const std::uint8_t* cur;
  const std::uint8_t* end;
  const std::uint8_t* prev;
  std::uint64_t len;
  int lastline;
  int line;
  const punctuation_t* punctuations;
  int* punctuationtable;
  int* nextpunctuation;
  bool loaded;
  bool hadError;
  bool hadWarning;
};

static_assert(sizeof(idLexerUTF8) == 128, "idLexerUTF8 ABI drift");
