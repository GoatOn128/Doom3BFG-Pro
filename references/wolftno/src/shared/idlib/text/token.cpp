#include "token.h"

#include <cmath>
#include <cstdlib>

const char idToken::DEFAULT_WHITESPACE[2] = {' ', 0};

void idToken::SetSafeWhiteSpacePointers() {
  const bool hadWhitespace = whiteSpaceEnd_p > whiteSpaceStart_p;
  whiteSpaceStart_p = DEFAULT_WHITESPACE;
  whiteSpaceEnd_p = DEFAULT_WHITESPACE + (hadWhitespace ? 1 : 0);
}
void idToken::InheritWhiteSpace(const idToken& other) {
  whiteSpaceStart_p = other.whiteSpaceStart_p;
  whiteSpaceEnd_p = other.whiteSpaceEnd_p;
  linesCrossed = other.linesCrossed;
}

void idToken::NumberValue() {
  intvalue = 0;
  floatvalue = 0.0f;
  if (subtype & 0x80) {
    if (subtype & 0x800)
      floatvalue = INFINITY;
    else if (subtype & 0x3000)
      floatvalue = NAN;
    else
      floatvalue = std::strtof(data, nullptr);
    intvalue = static_cast<std::uint32_t>(static_cast<int>(floatvalue));
  } else if (subtype & 0x4000) {
    int groupDigits = 0;
    for (const char* p = data; *p && *p != ':'; ++p) {
      if (*p == '.') {
        while (groupDigits++ < 3)
          intvalue *= 10;
        groupDigits = 0;
      } else if (*p >= '0' && *p <= '9') {
        intvalue = intvalue * 10 + (*p - '0');
        ++groupDigits;
      }
    }
    while (groupDigits++ < 3)
      intvalue *= 10;
    floatvalue = static_cast<float>(static_cast<int>(intvalue));
  } else {
    int base = 10;
    const char* start = data;
    if (subtype & 8) {
      base = 8;
      if (*start == '0')
        ++start;
    } else if (subtype & 4) {
      base = 16;
      if (start[0] == '0' && (start[1] == 'x' || start[1] == 'X'))
        start += 2;
    } else if (subtype & 0x10) {
      base = 2;
      if (start[0] == '0' && (start[1] == 'b' || start[1] == 'B'))
        start += 2;
    }
    intvalue = static_cast<std::uint32_t>(std::strtoul(start, nullptr, base));
    floatvalue = static_cast<float>(static_cast<int>(intvalue));
  }
  subtype |= 0x10000;
}

void idToken::AppendDirty(char value) {
  EnsureAlloced(len + 2, true, false);
  data[len++] = value;
  data[len] = '\0';
}
