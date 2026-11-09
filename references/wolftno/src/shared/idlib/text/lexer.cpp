#include "lexer.h"

#include <algorithm>
#include <cctype>
#include <cstdarg>
#include <cstring>
#include <fstream>
#include <iterator>
#include <string>

const punctuation_t idLexer::defaultPunctuations[] = {
    {">>=", 1}, {"<<=", 2}, {"...", 3}, {"##", 4},  {"&&", 5},   {"||", 6},  {">=", 7},  {"<=", 8},
    {"==", 9},  {"!=", 10}, {"*=", 11}, {"/=", 12}, {"%=", 13},  {"+=", 14}, {"-=", 15}, {"++", 16},
    {"--", 17}, {"&=", 18}, {"|=", 19}, {"^=", 20}, {">>", 21},  {"<<", 22}, {"->", 23}, {"::", 24},
    {".*", 25}, {"*", 26},  {"/", 27},  {"%", 28},  {"+", 29},   {"-", 30},  {"=", 31},  {"&", 32},
    {"|", 33},  {"^", 34},  {"~", 35},  {"!", 36},  {">", 37},   {"<", 38},  {".", 39},  {",", 40},
    {";", 41},  {":", 42},  {"?", 43},  {"(", 44},  {")", 45},   {"{", 46},  {"}", 47},  {"[", 48},
    {"]", 49},  {"\\", 50}, {"#", 51},  {"$", 52},  {nullptr, 0}};

idLexer::idLexer(int lexerFlags) : flags(lexerFlags) {
  SetPunctuations(defaultPunctuations);
}

idLexer::~idLexer() {
  FreeSource();
  delete[] punctuationtable;
  delete[] nextpunctuation;
}

void idLexer::CreatePunctuationTable(const punctuation_t* punctuationList) {
  delete[] punctuationtable;
  delete[] nextpunctuation;
  punctuationtable = new int[256];
  std::fill_n(punctuationtable, 256, -1);
  int count = 0;
  while (punctuationList[count].p != nullptr)
    ++count;
  nextpunctuation = new int[static_cast<std::size_t>(count)];
  std::fill_n(nextpunctuation, count, -1);
  for (int index = 0; index < count; ++index) {
    const unsigned char first = static_cast<unsigned char>(punctuationList[index].p[0]);
    int* link = &punctuationtable[first];
    while (*link >= 0 &&
           std::strlen(punctuationList[*link].p) >= std::strlen(punctuationList[index].p)) {
      link = &nextpunctuation[*link];
    }
    nextpunctuation[index] = *link;
    *link = index;
  }
}

const char* idLexer::GetPunctuationFromId(int id) const {
  for (int index = 0; punctuations[index].p != nullptr; ++index)
    if (punctuations[index].n == id)
      return punctuations[index].p;
  return "unknown punctuation";
}

int idLexer::GetPunctuationId(const char* punctuation) const {
  for (int index = 0; punctuations[index].p != nullptr; ++index)
    if (idStr::Cmp(punctuations[index].p, punctuation) == 0)
      return punctuations[index].n;
  return 0;
}

bool idLexer::ReadPunctuation(idToken& token) {
  if (script_p >= end_p)
    return false;
  int index = punctuationtable[static_cast<unsigned char>(*script_p)];
  while (index >= 0) {
    const char* punctuation = punctuations[index].p;
    const std::size_t punctuationLength = std::strlen(punctuation);
    if (script_p + punctuationLength <= end_p &&
        std::memcmp(script_p, punctuation, punctuationLength) == 0) {
      token = punctuation;
      token.type = TT_PUNCTUATION;
      token.subtype = punctuations[index].n;
      script_p += punctuationLength;
      return true;
    }
    index = nextpunctuation[index];
  }
  return false;
}

void idLexer::UnreadToken() {
  if (lastScript_p != nullptr) {
    script_p = lastScript_p;
    line = lastline;
  }
}

idStr& idLexer::ParseRestOfLine(idStr& output) {
  output.Empty();
  idToken token;
  while (ReadToken(token)) {
    if (token.linesCrossed > 0) {
      UnreadToken();
      break;
    }
    if (!output.IsEmpty())
      output.Append(' ');
    output.Append(token);
  }
  return output;
}

idStr& idLexer::ParseCompleteLine(idStr& output) {
  output.Empty();
  while (script_p < end_p && (*script_p == ' ' || *script_p == '\t'))
    ++script_p;
  const char* start = script_p;
  while (script_p < end_p && *script_p != '\n' && *script_p != '\r')
    ++script_p;
  output.CopyRange(start, 0, static_cast<int>(script_p - start));
  SkipRestOfLine();
  return output;
}

int idLexer::GetLastWhiteSpace(idStr& whiteSpace) const {
  whiteSpace.Empty();
  if (whiteSpaceStart_p == nullptr || whiteSpaceEnd_p == nullptr)
    return 0;
  whiteSpace.CopyRange(whiteSpaceStart_p, 0, static_cast<int>(whiteSpaceEnd_p - whiteSpaceStart_p));
  return whiteSpace.Length();
}

bool idLexer::GetWhiteSpaceBeforeToken(const idToken& token, idStr& whiteSpace) const {
  whiteSpace.Empty();
  if (token.whiteSpaceStart_p == nullptr || token.whiteSpaceEnd_p == nullptr ||
      token.whiteSpaceEnd_p < token.whiteSpaceStart_p)
    return false;
  whiteSpace.CopyRange(token.whiteSpaceStart_p, 0,
                       static_cast<int>(token.whiteSpaceEnd_p - token.whiteSpaceStart_p));
  return true;
}

bool idLexer::EndOfFile() const {
  return !loaded || script_p >= end_p;
}

bool idLexer::LoadMemory(const char* pointer, std::size_t sourceLength, const char* name) {
  if (loaded || pointer == nullptr)
    return false;
  buffer = pointer;
  length = sourceLength;
  script_p = lastScript_p = pointer;
  end_p = pointer + sourceLength;
  filename = name != nullptr ? name : "memory";
  allocated = 0;
  line = lastline = 1;
  loaded = true;
  hadError = hadWarning = false;
  return true;
}

bool idLexer::HadError() const {
  return hadError;
}
bool idLexer::HadWarning() const {
  return hadWarning;
}

bool idLexer::SetScriptP(const char* pointer) {
  if (!loaded || pointer < buffer || pointer > end_p)
    return false;
  script_p = pointer;
  line = 1;
  for (const char* scan = buffer; scan < script_p; ++scan)
    if (*scan == '\n')
      ++line;
  lastScript_p = script_p;
  lastline = line;
  return true;
}

void idLexer::Error(const char* format, ...) {
  hadError = true;
  if (flags & LEXFL_NOERRORS)
    return;
  va_list arguments;
  va_start(arguments, format);
  errorMsg.VFormat(format, arguments);
  va_end(arguments);
}

void idLexer::Warning(const char* format, ...) {
  hadWarning = true;
  if (flags & LEXFL_NOWARNINGS)
    return;
  va_list arguments;
  va_start(arguments, format);
  errorMsg.VFormat(format, arguments);
  va_end(arguments);
}

void idLexer::SetPunctuations(const punctuation_t* punctuationList) {
  punctuations = punctuationList != nullptr ? punctuationList : defaultPunctuations;
  CreatePunctuationTable(punctuations);
}

bool idLexer::SkipWhiteSpace(bool currentLine) {
  for (;;) {
    bool advanced = false;
    while (script_p < end_p && static_cast<unsigned char>(*script_p) <= ' ') {
      if (*script_p == '\n') {
        if (currentLine)
          return false;
        ++line;
      }
      ++script_p;
      advanced = true;
    }
    if (script_p + 1 < end_p && script_p[0] == '/' && script_p[1] == '/') {
      while (script_p < end_p && *script_p != '\n')
        ++script_p;
      advanced = true;
      continue;
    }
    if (script_p + 1 < end_p && script_p[0] == '/' && script_p[1] == '*') {
      script_p += 2;
      while (script_p + 1 < end_p && !(script_p[0] == '*' && script_p[1] == '/')) {
        if (*script_p++ == '\n') {
          if (currentLine)
            return false;
          ++line;
        }
      }
      if (script_p + 1 < end_p)
        script_p += 2;
      else {
        Error("unterminated comment");
        return false;
      }
      advanced = true;
      continue;
    }
    return advanced || script_p < end_p;
  }
}

bool idLexer::ReadEscapeCharacter(char& value) {
  if (script_p >= end_p)
    return false;
  const char escaped = *script_p++;
  switch (escaped) {
  case 'n':
    value = '\n';
    return true;
  case 'r':
    value = '\r';
    return true;
  case 't':
    value = '\t';
    return true;
  case 'v':
    value = '\v';
    return true;
  case 'b':
    value = '\b';
    return true;
  case 'f':
    value = '\f';
    return true;
  case 'a':
    value = '\a';
    return true;
  case '\\':
  case '\'':
  case '"':
  case '?':
    value = escaped;
    return true;
  case 'x': {
    int result = 0, digits = 0;
    while (script_p < end_p && std::isxdigit(static_cast<unsigned char>(*script_p))) {
      const char digit = *script_p++;
      result = result * 16 + (digit <= '9' ? digit - '0' : std::tolower(digit) - 'a' + 10);
      ++digits;
    }
    if (digits == 0)
      return false;
    value = static_cast<char>(result);
    return true;
  }
  default:
    if (escaped >= '0' && escaped <= '7') {
      int result = escaped - '0';
      for (int digit = 1; digit < 3 && script_p < end_p && *script_p >= '0' && *script_p <= '7';
           ++digit)
        result = result * 8 + (*script_p++ - '0');
      value = static_cast<char>(result);
      return true;
    }
    value = escaped;
    return true;
  }
}

bool idLexer::ReadString(idToken& token, int quote) {
  token.Empty();
  token.type = quote == '"' ? TT_STRING : TT_LITERAL;
  ++script_p;
  while (script_p < end_p) {
    char value = *script_p++;
    if (value == quote) {
      token.subtype = token.Length();
      return true;
    }
    if (value == '\n' || value == '\r') {
      Error("newline inside string");
      return false;
    }
    if (value == '\\' && !(flags & LEXFL_NOSTRINGESCAPECHARS)) {
      if (!ReadEscapeCharacter(value))
        return false;
    }
    token.AppendDirty(value);
  }
  Error("missing trailing quote");
  return false;
}

bool idLexer::ReadName(idToken& token) {
  token.Empty();
  while (script_p < end_p) {
    const unsigned char value = static_cast<unsigned char>(*script_p);
    const bool normal = std::isalnum(value) || value == '_';
    const bool path = (flags & LEXFL_ALLOWPATHNAMES) &&
                      (value == '/' || value == '\\' || value == ':' || value == '.');
    const bool onlyString = (flags & LEXFL_ONLYSTRINGS) && value > ' ';
    if (!normal && !path && !onlyString)
      break;
    token.AppendDirty(*script_p++);
  }
  token.type = TT_NAME;
  token.subtype = token.Length();
  return !token.IsEmpty();
}

bool idLexer::ReadNumber(idToken& token) {
  token.Empty();
  token.type = TT_NUMBER;
  const char* start = script_p;
  bool isFloat = false;
  if (script_p + 2 <= end_p && script_p[0] == '0' && (script_p[1] == 'x' || script_p[1] == 'X')) {
    script_p += 2;
    while (script_p < end_p && std::isxdigit(static_cast<unsigned char>(*script_p)))
      ++script_p;
    token.subtype = TT_INTEGER | TT_HEX;
  } else if (script_p + 2 <= end_p && script_p[0] == '0' &&
             (script_p[1] == 'b' || script_p[1] == 'B')) {
    script_p += 2;
    while (script_p < end_p && (*script_p == '0' || *script_p == '1'))
      ++script_p;
    token.subtype = TT_INTEGER | TT_BINARY;
  } else {
    while (script_p < end_p && std::isdigit(static_cast<unsigned char>(*script_p)))
      ++script_p;
    if (script_p < end_p && *script_p == '.') {
      isFloat = true;
      ++script_p;
      while (script_p < end_p && std::isdigit(static_cast<unsigned char>(*script_p)))
        ++script_p;
    }
    if (script_p < end_p && (*script_p == 'e' || *script_p == 'E')) {
      isFloat = true;
      ++script_p;
      if (script_p < end_p && (*script_p == '+' || *script_p == '-'))
        ++script_p;
      while (script_p < end_p && std::isdigit(static_cast<unsigned char>(*script_p)))
        ++script_p;
    }
    token.subtype = isFloat ? TT_FLOAT | TT_DECIMAL | TT_DOUBLE_PRECISION : TT_INTEGER | TT_DECIMAL;
  }
  while (script_p < end_p && std::isalpha(static_cast<unsigned char>(*script_p))) {
    const char suffix = static_cast<char>(std::tolower(static_cast<unsigned char>(*script_p++)));
    if (suffix == 'u')
      token.subtype |= TT_UNSIGNED;
    else if (suffix == 'l')
      token.subtype |= TT_LONG;
    else if (suffix == 'f')
      token.subtype |= TT_FLOAT | TT_SINGLE_PRECISION;
  }
  token.CopyRange(start, 0, static_cast<int>(script_p - start));
  token.NumberValue();
  return true;
}

bool idLexer::ReadRawStringBlock(idToken& token) {
  token.Empty();
  token.type = TT_STRING;
  script_p += 2;
  while (script_p < end_p) {
    if (script_p + 1 < end_p && script_p[0] == '%' && script_p[1] == '>') {
      script_p += 2;
      token.subtype = token.Length();
      return true;
    }
    if (*script_p == '\n')
      ++line;
    token.AppendDirty(*script_p++);
  }
  Error("missing trailing identifier");
  return false;
}

bool idLexer::ReadToken(idToken& token) {
  if (!loaded || script_p >= end_p)
    return false;
  lastScript_p = script_p;
  lastline = line;
  token.Empty();
  whiteSpaceStart_p = script_p;
  token.whiteSpaceStart_p = script_p;
  if (!SkipWhiteSpace(false) || script_p >= end_p)
    return false;
  whiteSpaceEnd_p = script_p;
  token.whiteSpaceEnd_p = script_p;
  token.line = line;
  token.linesCrossed = line - lastline;
  token.flags = 0;
  token.intvalue = 0;
  token.floatvalue = 0.0f;
  const unsigned char value = static_cast<unsigned char>(*script_p);
  if ((flags & LEXFL_ALLOWRAWSTRINGBLOCKS) && script_p + 1 < end_p && script_p[0] == '<' &&
      script_p[1] == '%')
    return ReadRawStringBlock(token);
  if (!(flags & LEXFL_NOSTRINGS) && (value == '"' || value == '\''))
    return ReadString(token, value);
  if (!(flags & LEXFL_ONLYSTRINGS) &&
      (std::isdigit(value) || (value == '.' && script_p + 1 < end_p &&
                               std::isdigit(static_cast<unsigned char>(script_p[1]))))) {
    if (!ReadNumber(token))
      return false;
    if ((flags & LEXFL_ALLOWNUMBERNAMES) && script_p < end_p &&
        (std::isalpha(static_cast<unsigned char>(*script_p)) || *script_p == '_')) {
      while (script_p < end_p &&
             (std::isalnum(static_cast<unsigned char>(*script_p)) || *script_p == '_'))
        token.AppendDirty(*script_p++);
      token.type = TT_NAME;
      token.subtype = token.Length();
    }
    return true;
  }
  if (std::isalpha(value) || value == '_' || (flags & LEXFL_ONLYSTRINGS) ||
      ((flags & LEXFL_ALLOWPATHNAMES) && value == '.'))
    return ReadName(token);
  if (ReadPunctuation(token))
    return true;
  Error("unknown punctuation %c", value);
  ++script_p;
  return false;
}

bool idLexer::ExpectTokenString(const char* string, idToken& token) {
  if (!ReadToken(token)) {
    Error("expected '%s' but found EOF", string);
    return false;
  }
  if (idStr::Cmp(token.data, string) != 0) {
    Error("expected '%s' but found '%s'", string, token.data);
    return false;
  }
  return true;
}

bool idLexer::ExpectTokenString(const char* string) {
  idToken token;
  return ExpectTokenString(string, token);
}

bool idLexer::ExpectAnyToken(idToken& token) {
  if (ReadToken(token))
    return true;
  Error("couldn't read expected token");
  return false;
}

int idLexer::GetNextWhiteSpace(idStr& whiteSpace, bool currentLine) {
  const char* saved = script_p;
  const int savedLine = line;
  whiteSpaceStart_p = script_p;
  SkipWhiteSpace(currentLine);
  whiteSpaceEnd_p = script_p;
  GetLastWhiteSpace(whiteSpace);
  const int crossed = line - savedLine;
  script_p = saved;
  line = savedLine;
  return crossed;
}

void idLexer::Reset() {
  script_p = lastScript_p = buffer;
  whiteSpaceStart_p = whiteSpaceEnd_p = buffer;
  line = lastline = 1;
  hadError = hadWarning = false;
}

bool idLexer::LoadFile(const char* path, bool) {
  if (loaded || path == nullptr)
    return false;
  std::ifstream file(path, std::ios::binary);
  if (!file)
    return false;
  std::string contents((std::istreambuf_iterator<char>(file)), std::istreambuf_iterator<char>());
  char* owned = new char[contents.size() + 1];
  std::memcpy(owned, contents.data(), contents.size());
  owned[contents.size()] = '\0';
  buffer = owned;
  length = contents.size();
  script_p = lastScript_p = buffer;
  end_p = buffer + length;
  filename = path;
  allocated = 1;
  line = lastline = 1;
  loaded = true;
  return true;
}

void idLexer::FreeSource() {
  if (allocated != 0)
    delete[] buffer;
  buffer = script_p = end_p = lastScript_p = nullptr;
  whiteSpaceStart_p = whiteSpaceEnd_p = nullptr;
  allocated = 0;
  loaded = false;
  length = 0;
  filename.Clear();
}

bool idLexer::ReadTokenOnLine(idToken& token) {
  if (!ReadToken(token))
    return false;
  if (token.linesCrossed == 0)
    return true;
  UnreadToken();
  return false;
}

idStr& idLexer::ParseBracedSection(idStr& output, int tabs, bool parseFirstBrace, char intro,
                                   char outro) {
  output.Empty();
  int depth = 1;
  if (parseFirstBrace) {
    idToken first;
    char expected[2]{intro, '\0'};
    if (!ExpectTokenString(expected, first))
      return output;
    output.Append(intro);
  }
  idToken token;
  while (depth > 0 && ReadToken(token)) {
    if (token[0] == intro)
      ++depth;
    else if (token[0] == outro)
      --depth;
    if (tabs >= 0 && token.linesCrossed > 0) {
      output.Append('\n');
      output.Append(tabs + depth - (token[0] == outro ? 1 : 0), '\t');
    } else if (!output.IsEmpty() && token.type != TT_PUNCTUATION) {
      output.Append(' ');
    }
    output.Append(token);
  }
  return output;
}

bool idLexer::ParseBracedSectionExact(idStr& output, bool parseFirstBrace, char intro, char outro) {
  output.Empty();
  int depth = 1;
  if (parseFirstBrace) {
    char expected[2]{intro, '\0'};
    if (!ExpectTokenString(expected))
      return false;
    output.Append(intro);
  }
  idToken token;
  while (depth > 0 && ReadToken(token)) {
    idStr whitespace;
    GetWhiteSpaceBeforeToken(token, whitespace);
    output.Append(whitespace);
    if (token.type == TT_STRING) {
      output.Append('"');
      output.Append(token);
      output.Append('"');
    } else if (token.type == TT_LITERAL) {
      output.Append('\'');
      output.Append(token);
      output.Append('\'');
    } else
      output.Append(token);
    if (token.type == TT_PUNCTUATION && token[0] == intro)
      ++depth;
    else if (token.type == TT_PUNCTUATION && token[0] == outro)
      --depth;
  }
  return depth == 0;
}

bool idLexer::ExpectTokenType(int expectedType, int expectedSubtype, idToken& token) {
  if (!ReadToken(token)) {
    Error("expected token but found EOF");
    return false;
  }
  if (token.type != expectedType ||
      (expectedSubtype != 0 && (token.subtype & expectedSubtype) != expectedSubtype)) {
    Error("unexpected token '%s'", token.data);
    return false;
  }
  return true;
}

bool idLexer::CheckTokenString(const char* string) {
  idToken token;
  if (!ReadToken(token))
    return false;
  if (idStr::Cmp(token.data, string) == 0)
    return true;
  UnreadToken();
  return false;
}

bool idLexer::CheckTokenType(int expectedType, int expectedSubtype, idToken& token) {
  if (!ReadToken(token))
    return false;
  if (token.type == expectedType &&
      (expectedSubtype == 0 || (token.subtype & expectedSubtype) == expectedSubtype))
    return true;
  UnreadToken();
  return false;
}

bool idLexer::PeekTokenString(const char* string) {
  idToken token;
  if (!ReadToken(token))
    return false;
  const bool matches = idStr::Cmp(token.data, string) == 0;
  UnreadToken();
  return matches;
}

bool idLexer::PeekTokenType(int expectedType, int expectedSubtype, idToken& token) {
  if (!ReadToken(token))
    return false;
  const bool matches =
      token.type == expectedType &&
      (expectedSubtype == 0 || (token.subtype & expectedSubtype) == expectedSubtype);
  UnreadToken();
  return matches;
}

bool idLexer::SkipUntilString(const char* string) {
  idToken token;
  while (ReadToken(token))
    if (idStr::Cmp(token.data, string) == 0)
      return true;
  return false;
}

bool idLexer::SkipRestOfLine() {
  while (script_p < end_p) {
    const char value = *script_p++;
    if (value == '\n') {
      ++line;
      return true;
    }
  }
  return false;
}

bool idLexer::SkipBracedSection(bool parseFirstBrace) {
  int depth = 1;
  if (parseFirstBrace && !ExpectTokenString("{"))
    return false;
  idToken token;
  while (depth > 0 && ReadToken(token)) {
    if (token == "{")
      ++depth;
    else if (token == "}")
      --depth;
  }
  return depth == 0;
}

int idLexer::ParseInt() {
  bool negative = CheckTokenString("-");
  idToken token;
  if (!ExpectTokenType(TT_NUMBER, TT_INTEGER, token))
    return 0;
  token.NumberValue();
  const int value = static_cast<int>(token.intvalue);
  return negative ? -value : value;
}

unsigned idLexer::ParseUnsignedInt() {
  idToken token;
  if (!ExpectTokenType(TT_NUMBER, TT_INTEGER, token))
    return 0;
  token.NumberValue();
  return token.intvalue;
}

bool idLexer::ParseBool() {
  idToken token;
  if (!ReadToken(token))
    return false;
  if (token.type == TT_NUMBER)
    return std::strtol(token.data, nullptr, 0) != 0;
  return idStr::Icmp(token.data, "true") == 0 || idStr::Icmp(token.data, "yes") == 0;
}

float idLexer::ParseFloat(bool* errorFlag) {
  const bool negative = CheckTokenString("-");
  idToken token;
  if (!ExpectTokenType(TT_NUMBER, 0, token)) {
    if (errorFlag)
      *errorFlag = true;
    return 0.0f;
  }
  if (errorFlag)
    *errorFlag = false;
  token.NumberValue();
  return negative ? -token.floatvalue : token.floatvalue;
}

bool idLexer::Parse1DMatrix(int count, float* matrix, bool expectCommas) {
  if (!ExpectTokenString("("))
    return false;
  for (int index = 0; index < count; ++index) {
    bool error = false;
    matrix[index] = ParseFloat(&error);
    if (error)
      return false;
    if (expectCommas && index + 1 < count && !ExpectTokenString(","))
      return false;
  }
  return ExpectTokenString(")");
}

bool idLexer::Parse2DMatrix(int rows, int columns, float* matrix) {
  if (!ExpectTokenString("("))
    return false;
  for (int row = 0; row < rows; ++row)
    if (!Parse1DMatrix(columns, matrix + row * columns, false))
      return false;
  return ExpectTokenString(")");
}
