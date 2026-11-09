#include "str.h"

#include <algorithm>
#include <cassert>
#include <cctype>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <string>
#include <vector>

namespace {
constexpr int kCapacityMask = 0x3FFFFFFF;
thread_local char stringBuffers[4][16384]{};
thread_local int stringBufferIndex = 0;

char* NextBuffer() {
  char* result = stringBuffers[stringBufferIndex++ & 3];
  result[0] = '\0';
  return result;
}

int CompareResult(int value) {
  return value < 0 ? -1 : value > 0 ? 1 : 0;
}

unsigned char Lower(unsigned char value) {
  return static_cast<unsigned char>(std::tolower(value));
}

bool SamePathChar(char left, char right) {
  if (left == '\\')
    left = '/';
  if (right == '\\')
    right = '/';
  return Lower(static_cast<unsigned char>(left)) == Lower(static_cast<unsigned char>(right));
}

std::string NormalizeExtension(const char* extension) {
  if (extension == nullptr || *extension == '\0')
    return {};
  return *extension == '.' ? extension : std::string(".") + extension;
}
} // namespace

va::va(const char* format, ...) {
  std::va_list arguments;
  va_start(arguments, format);
  idStr::vsnPrintf(buffer, static_cast<int>(sizeof(buffer)), format,
                   arguments);
  va_end(arguments);
}

const idVec4& idStr::ColorForIndex(char index) {
  static const idVec4 colors[16] = {
      {0, 0, 0, 1},          {1, 0, 0, 1},       {0, 1, 0, 1},       {1, 1, 0, 1},
      {0, 0, 1, 1},          {0, 1, 1, 1},       {1, 0, 1, 1},       {1, 1, 1, 1},
      {0.5f, 0.5f, 0.5f, 1}, {1, 0.5f, 0.5f, 1}, {0.5f, 1, 0.5f, 1}, {1, 1, 0.5f, 1},
      {0.5f, 0.5f, 1, 1},    {0.5f, 1, 1, 1},    {1, 0.5f, 1, 1},    {0.75f, 0.75f, 0.75f, 1}};
  return colors[static_cast<unsigned char>(index) & 15];
}

char* idStr::CStyleUnQuote(const char* text) {
  char* out = NextBuffer();
  if (text == nullptr)
    return out;
  const char* cursor = text;
  if (*cursor == '"')
    ++cursor;
  int used = 0;
  while (*cursor != '\0' && *cursor != '"' && used < 16383) {
    char value;
    if (*cursor == '\\' && EmitEscapeChar(&cursor, &value))
      out[used++] = value;
    else
      out[used++] = *cursor++;
  }
  out[used] = '\0';
  return out;
}

char* idStr::CStyleQuote(const char* text) {
  char* out = NextBuffer();
  int used = 0;
  out[used++] = '"';
  for (const char* p = text != nullptr ? text : ""; *p && used < 16381; ++p) {
    const char* escape = nullptr;
    switch (*p) {
    case '\n':
      escape = "\\n";
      break;
    case '\r':
      escape = "\\r";
      break;
    case '\t':
      escape = "\\t";
      break;
    case '\\':
      escape = "\\\\";
      break;
    case '"':
      escape = "\\\"";
      break;
    default:
      break;
    }
    if (escape) {
      out[used++] = escape[0];
      out[used++] = escape[1];
    } else
      out[used++] = *p;
  }
  out[used++] = '"';
  out[used] = '\0';
  return out;
}

bool idStr::IsNumeric(const char* text) {
  if (text == nullptr || *text == '\0')
    return false;
  char* end = nullptr;
  std::strtod(text, &end);
  return end != text && *end == '\0';
}

void idStr::Copynz(char* destination, const char* source, int destinationSize) {
  if (destination == nullptr || destinationSize <= 0)
    return;
  const char* input = source != nullptr ? source : "";
  const std::size_t count =
      std::min(std::strlen(input), static_cast<std::size_t>(destinationSize - 1));
  std::memmove(destination, input, count);
  destination[count] = '\0';
}

void idStr::CopyCountAndZero(char* destination, std::size_t destinationSize, const char* source,
                             std::size_t count, std::size_t sourceSize) {
  if (destination == nullptr || destinationSize == 0)
    return;
  const std::size_t available =
      sourceSize == ~std::size_t(0) ? std::strlen(source != nullptr ? source : "") : sourceSize;
  const std::size_t copy = std::min({count, available, destinationSize - 1});
  if (copy != 0 && source != nullptr)
    std::memmove(destination, source, copy);
  destination[copy] = '\0';
}

int idStr::vsnPrintf(char* destination, int size, const char* format, std::va_list arguments) {
  if (destination == nullptr || size <= 0)
    return -1;
  const int result = std::vsnprintf(destination, static_cast<std::size_t>(size), format, arguments);
  destination[size - 1] = '\0';
  return result < 0 || result >= size ? -1 : result;
}

int idStr::snPrintf(char* destination, int size, const char* format, ...) {
  std::va_list arguments;
  va_start(arguments, format);
  const int result = vsnPrintf(destination, size, format, arguments);
  va_end(arguments);
  return result;
}

std::uint32_t idStr::UTF8Char(const std::uint8_t* text, int* index) {
  if (text == nullptr || index == nullptr)
    return 0;
  const std::uint8_t first = text[(*index)++];
  if (first < 0x80)
    return first;
  int trailing;
  std::uint32_t value;
  if ((first & 0xE0) == 0xC0) {
    trailing = 1;
    value = first & 0x1F;
  } else if ((first & 0xF0) == 0xE0) {
    trailing = 2;
    value = first & 0x0F;
  } else if ((first & 0xF8) == 0xF0) {
    trailing = 3;
    value = first & 7;
  } else
    return 0xFFFD;
  for (int i = 0; i < trailing; ++i) {
    const std::uint8_t next = text[(*index)++];
    if ((next & 0xC0) != 0x80)
      return 0xFFFD;
    value = (value << 6) | (next & 0x3F);
  }
  return value;
}

bool idStr::EmitEscapeChar(const char** text, char* value) {
  if (text == nullptr || *text == nullptr || **text != '\\' || value == nullptr)
    return false;
  ++*text;
  switch (*(*text)++) {
  case 'n':
    *value = '\n';
    break;
  case 'r':
    *value = '\r';
    break;
  case 't':
    *value = '\t';
    break;
  case 'v':
    *value = '\v';
    break;
  case 'b':
    *value = '\b';
    break;
  case 'f':
    *value = '\f';
    break;
  case 'a':
    *value = '\a';
    break;
  case '\\':
    *value = '\\';
    break;
  case '"':
    *value = '"';
    break;
  case '\'':
    *value = '\'';
    break;
  case '0':
    *value = '\0';
    break;
  default:
    --*text;
    return false;
  }
  return true;
}

int idStr::Length(const char* text) {
  return text ? static_cast<int>(std::strlen(text)) : 0;
}
int idStr::Hash(const char* text) {
  int hash = 0;
  for (int i = 0; text && text[i]; ++i)
    hash += text[i] * (i + 119);
  return hash;
}
bool idStr::IsColor(const char* text) {
  return text && text[0] == '^' && text[1] && text[1] != '^';
}
int idStr::ToLower(unsigned char value) {
  return std::tolower(value);
}
int idStr::ToUpper(unsigned char value) {
  return std::toupper(value);
}
bool idStr::CharIsPrintable(int value) {
  return value >= 0x20 && value <= 0x7E;
}
bool idStr::CharIsLower(int value) {
  return value >= 'a' && value <= 'z';
}
bool idStr::CharIsUpper(int value) {
  return value >= 'A' && value <= 'Z';
}
bool idStr::CharIsAlpha(int value) {
  return CharIsLower(value) || CharIsUpper(value);
}
bool idStr::CharIsNumeric(int value) {
  return value >= '0' && value <= '9';
}
int idStr::ColorIndex(char value) {
  return static_cast<unsigned char>(value) & 15;
}

int idStr::Find(const char* text, char value, int start, int end) {
  const int length = Length(text);
  if (end < 0 || end > length)
    end = length;
  for (int i = std::max(0, start); i < end; ++i)
    if (text[i] == value)
      return i;
  return -1;
}

int idStr::Find(const char* text, const char* search, bool caseSensitive, int start, int end) {
  if (!text || !search)
    return -1;
  const int length = Length(text), searchLength = Length(search);
  if (end < 0 || end > length)
    end = length;
  for (int i = std::max(0, start); i + searchLength <= end; ++i) {
    if ((caseSensitive ? Cmpn(text + i, search, searchLength)
                       : Icmpn(text + i, search, searchLength)) == 0)
      return i;
  }
  return -1;
}

int idStr::Last(const char* text, char value, unsigned index) {
  if (!text)
    return -1;
  const unsigned length = static_cast<unsigned>(Length(text));
  unsigned i = std::min(index, length);
  while (i > 0) {
    --i;
    if (text[i] == value)
      return static_cast<int>(i);
  }
  return -1;
}

int idStr::FileNameHash(const char* text, int hashSize) {
  int hash = 0;
  for (int i = 0; text && text[i] && text[i] != '.'; ++i) {
    char c = text[i];
    if (c == '\\')
      c = '/';
    c = static_cast<char>(Lower(c));
    hash += c * (i + 119);
  }
  return hashSize > 0 ? hash & (hashSize - 1) : hash;
}

void idStr::BackSlashesToSlashes(char* path) {
  if (path)
    for (; *path; ++path)
      if (*path == '\\')
        *path = '/';
}
void idStr::SlashesToBackSlashes(char* path) {
  if (path)
    for (; *path; ++path)
      if (*path == '/')
        *path = '\\';
}
bool idStr::IsNameCanonical(const char* name) {
  if (!name)
    return false;
  for (; *name; ++name)
    if (*name == '\\' || std::isupper(static_cast<unsigned char>(*name)))
      return false;
  return true;
}
int idStr::Cmp(const char* a, const char* b) {
  return CompareResult(std::strcmp(a ? a : "", b ? b : ""));
}
int idStr::Cmpn(const char* a, const char* b, int n) {
  return CompareResult(
      std::strncmp(a ? a : "", b ? b : "", static_cast<std::size_t>(std::max(0, n))));
}
int idStr::CmpPrefix(const char* a, const char* b) {
  return Cmpn(a, b, Length(b));
}
int idStr::Icmp(const char* a, const char* b) {
  while (*a && Lower(*a) == Lower(*b)) {
    ++a;
    ++b;
  }
  return CompareResult(int(Lower(*a)) - int(Lower(*b)));
}
int idStr::Icmpn(const char* a, const char* b, int n) {
  for (int i = 0; i < n; ++i) {
    const int d = int(Lower(*a)) - int(Lower(*b));
    if (d || !*a || !*b)
      return CompareResult(d);
    ++a;
    ++b;
  }
  return 0;
}
int idStr::IcmpPrefix(const char* a, const char* b) {
  return Icmpn(a, b, Length(b));
}
int idStr::IcmpPath(const char* a, const char* b) {
  while (*a && SamePathChar(*a, *b)) {
    ++a;
    ++b;
  }
  char ac = *a == '\\' ? '/' : *a, bc = *b == '\\' ? '/' : *b;
  return CompareResult(int(Lower(ac)) - int(Lower(bc)));
}
int idStr::IcmpnPath(const char* a, const char* b, int n) {
  for (int i = 0; i < n; ++i) {
    if (!SamePathChar(*a, *b))
      return IcmpPath(a, b);
    if (!*a || !*b)
      return 0;
    ++a;
    ++b;
  }
  return 0;
}
void idStr::Append(char* destination, int size, const char* source) {
  if (!destination || size <= 0)
    return;
  const int used = Length(destination);
  if (used < size)
    Copynz(destination + used, source, size - used);
}
int idStr::LengthWithoutColors(const char* text) {
  int count = 0;
  while (text && *text) {
    if (IsColor(text))
      text += 2;
    else {
      ++count;
      ++text;
    }
  }
  return count;
}
char* idStr::RemoveColors(char* text) {
  if (!text)
    return text;
  char* out = text;
  for (char* in = text; *in;) {
    if (IsColor(in))
      in += 2;
    else
      *out++ = *in++;
  }
  *out = '\0';
  return text;
}

bool idStr::IsValidUTF8(const std::uint8_t* text, int maxLength, utf8Encoding_t* encoding) {
  if (encoding)
    *encoding = UTF8_ASCII;
  if (!text)
    return false;
  int i = 0;
  while (i < maxLength && text[i]) {
    if (text[i] < 0x80) {
      ++i;
      continue;
    }
    if (encoding)
      *encoding = UTF8_ENCODED;
    const int start = i;
    const auto value = UTF8Char(text, &i);
    if (value == 0xFFFD || i > maxLength || i <= start)
      return false;
  }
  return true;
}
bool idStr::IsValidUTF8(const std::uint8_t* text, int maxLength) {
  return IsValidUTF8(text, maxLength, nullptr);
}

bool idStr::ToEscapedString(const char* text, int length, char* output, int outputSize) {
  if (!output || outputSize <= 0)
    return false;
  int used = 0;
  for (int i = 0; text && i < length && text[i]; ++i) {
    const char* escape = nullptr;
    switch (text[i]) {
    case '\n':
      escape = "\\n";
      break;
    case '\r':
      escape = "\\r";
      break;
    case '\t':
      escape = "\\t";
      break;
    case '\\':
      escape = "\\\\";
      break;
    case '"':
      escape = "\\\"";
      break;
    default:
      break;
    }
    const int needed = escape ? 2 : 1;
    if (used + needed >= outputSize) {
      output[used] = '\0';
      return false;
    }
    if (escape) {
      output[used++] = escape[0];
      output[used++] = escape[1];
    } else
      output[used++] = text[i];
  }
  output[used] = '\0';
  return true;
}

int idStr::IHash(const char* text) {
  int hash = 0;
  for (int i = 0; text && text[i]; ++i)
    hash += Lower(text[i]) * (i + 119);
  return hash;
}

char* idStr::FloatArrayToString(const float* values, int count, int precision) {
  char* out = NextBuffer();
  int used = 0;
  for (int i = 0; i < count && used < 16383; ++i) {
    const int wrote =
        std::snprintf(out + used, 16384 - used, i ? " %.*f" : "%.*f", precision, values[i]);
    if (wrote < 0)
      break;
    used += wrote;
  }
  return out;
}

char* CopyString(const char* text, unsigned) {
  const std::size_t length = std::strlen(text ? text : "") + 1;
  char* copy = new char[length];
  std::memcpy(copy, text ? text : "", length);
  return copy;
}

idStr::idStr() : len(0), data(baseBuffer), allocedAndFlag(sizeof(baseBuffer)) {
  baseBuffer[0] = '\0';
}
idStr::idStr(const idStr& text) : idStr() {
  *this = text;
}
idStr::idStr(const va& text) : idStr() {
  *this = text.buffer;
}
idStr::idStr(const idStr& text, int start, int end) : idStr() {
  CopyRange(text.data, start, end);
}
idStr::idStr(const char* text) : idStr() {
  *this = text;
}
idStr::idStr(const char* text, int start, int end) : idStr() {
  CopyRange(text, start, end);
}
idStr::idStr(bool value) : idStr(value ? "true" : "false") {}
idStr::idStr(char value) : idStr() {
  Append(value);
}
idStr::idStr(int value) : idStr() {
  char buffer[64];
  snPrintf(buffer, sizeof(buffer), "%d", value);
  *this = buffer;
}
idStr::idStr(float value) : idStr() {
  char buffer[64];
  snPrintf(buffer, sizeof(buffer), "%g", value);
  *this = buffer;
}
idStr::~idStr() {
  FreeData();
}

void idStr::EnsureAlloced(int amount, bool keepOld, bool geometricGrowth) {
  if (amount < 1)
    amount = 1;
  if (amount <= (allocedAndFlag & kCapacityMask))
    return;
  if (geometricGrowth) {
    int grown = (allocedAndFlag & kCapacityMask);
    while (grown < amount)
      grown = std::max(grown + 1, grown + grown / 2);
    amount = grown;
  }
  ReAllocate(amount, keepOld);
}
void idStr::ReAllocate(int amount, bool keepOld) {
  char* replacement = new char[amount];
  if (keepOld) {
    const int copy = std::min(len, amount - 1);
    std::memcpy(replacement, data, copy);
    replacement[copy] = '\0';
    len = copy;
  } else {
    replacement[0] = '\0';
    len = 0;
  }
  if ((allocedAndFlag & 0x40000000) != 0)
    delete[] data;
  data = replacement;
  allocedAndFlag = static_cast<int>(0x40000000u | amount);
}
void idStr::FreeData() {
  if ((allocedAndFlag & 0x40000000) != 0)
    delete[] data;
  data = baseBuffer;
  len = 0;
  allocedAndFlag = sizeof(baseBuffer);
  baseBuffer[0] = '\0';
}

idStr& idStr::operator=(const idStr& text) {
  if (this != &text)
    *this = text.data;
  return *this;
}
idStr& idStr::operator=(const va& text) {
  return *this = text.buffer;
}
idStr& idStr::operator=(const char* text) {
  if (text == data)
    return *this;
  if (!text)
    text = "";
  const int length = Length(text);
  EnsureAlloced(length + 1, false);
  std::memmove(data, text, length + 1);
  len = length;
  return *this;
}
idStr& idStr::operator=(char value) {
  Empty();
  Append(value);
  return *this;
}
char idStr::operator[](int index) const {
  assert(index >= 0 && index <= len);
  return data[index];
}
char& idStr::operator[](int index) {
  assert(index >= 0 && index <= len);
  return data[index];
}
bool idStr::IsEmpty() const {
  return len == 0;
}
bool idStr::IsNumeric() const {
  return IsNumeric(data);
}
int idStr::Length() const {
  return len;
}
int idStr::Allocated() const {
  return allocedAndFlag & kCapacityMask;
}
int idStr::Size() const {
  return sizeof(*this) + (data == baseBuffer ? 0 : Allocated());
}
int idStr::CountChar(char value) const {
  return static_cast<int>(std::count(data, data + len, value));
}
std::uint32_t idStr::UTF8Char(int* index) const {
  return UTF8Char(reinterpret_cast<const std::uint8_t*>(data), index);
}
void idStr::CapLength(int newLength) {
  if (newLength < 0)
    newLength = 0;
  if (newLength < len) {
    len = newLength;
    data[len] = '\0';
  }
}
void idStr::Empty() {
  len = 0;
  data[0] = '\0';
}
void idStr::Clear() {
  Empty();
}
void idStr::CopyRange(const char* text, int start, int end) {
  if (!text) {
    Empty();
    return;
  }
  const int length = Length(text);
  start = std::clamp(start, 0, length);
  end = std::clamp(end, start, length);
  EnsureAlloced(end - start + 1, false);
  len = end - start;
  std::memmove(data, text + start, len);
  data[len] = '\0';
}
int idStr::Last(char value, int index) const {
  if (index < 0 || index > len)
    index = len;
  return Last(data, value, static_cast<unsigned>(index));
}
int idStr::Find(char value, unsigned start, int end) const {
  return Find(data, value, static_cast<int>(start), end);
}
int idStr::Find(const char* search, bool caseSensitive, int start, int end) const {
  return Find(data, search, caseSensitive, start, end);
}
bool idStr::EndsWith(const char* text) const {
  const int count = Length(text);
  return count <= len && Cmp(data + len - count, text) == 0;
}
int idStr::Cmp(const char* text) const {
  return Cmp(data, text);
}
int idStr::Cmpn(const char* text, int count) const {
  return Cmpn(data, text, count);
}
int idStr::CmpPrefix(const char* text) const {
  return CmpPrefix(data, text);
}
int idStr::Icmp(const char* text) const {
  return Icmp(data, text);
}
int idStr::Icmpn(const char* text, int count) const {
  return Icmpn(data, text, count);
}
int idStr::IcmpPrefix(const char* text) const {
  return IcmpPrefix(data, text);
}
int idStr::IcmpPath(const char* text) const {
  return IcmpPath(data, text);
}
int idStr::IcmpnPath(const char* text, int count) const {
  return IcmpnPath(data, text, count);
}

void idStr::Append(char value) {
  EnsureAlloced(len + 2, true);
  data[len++] = value;
  data[len] = '\0';
}
void idStr::Append(const idStr& text) {
  Append(text.data, text.len);
}
void idStr::Append(const va& text) {
  Append(text.buffer);
}
void idStr::Append(const char* text) {
  Append(text, Length(text));
}
void idStr::Append(const char* text, int length) {
  if (!text || length <= 0)
    return;
  if (text >= data && text <= data + len) {
    idStr copy(text);
    Append(copy.data, copy.len);
    return;
  }
  EnsureAlloced(len + length + 1, true);
  std::memmove(data + len, text, length);
  len += length;
  data[len] = '\0';
}
void idStr::Append(int count, char value) {
  if (count <= 0)
    return;
  EnsureAlloced(len + count + 1, true);
  std::memset(data + len, value, count);
  len += count;
  data[len] = '\0';
}
void idStr::Insert(char value, int index) {
  index = std::clamp(index, 0, len);
  EnsureAlloced(len + 2, true);
  std::memmove(data + index + 1, data + index, len - index + 1);
  data[index] = value;
  ++len;
}
void idStr::Insert(const char* text, int index) {
  if (!text)
    return;
  index = std::clamp(index, 0, len);
  const int count = Length(text);
  EnsureAlloced(len + count + 1, true);
  std::memmove(data + index + count, data + index, len - index + 1);
  std::memmove(data + index, text, count);
  len += count;
}
idStr& idStr::operator+=(const idStr& text) {
  Append(text);
  return *this;
}
idStr& idStr::operator+=(const va& text) {
  Append(text);
  return *this;
}
idStr& idStr::operator+=(const char* text) {
  Append(text);
  return *this;
}
idStr& idStr::operator+=(int value) {
  char buffer[64];
  snPrintf(buffer, sizeof(buffer), "%d", value);
  Append(buffer);
  return *this;
}
idStr& idStr::operator+=(char value) {
  Append(value);
  return *this;
}

void idStr::StripLeading(char value) {
  int count = 0;
  while (count < len && data[count] == value)
    ++count;
  if (count)
    Erase(0, count);
}
void idStr::StripLeading(const char* text) {
  const int count = Length(text);
  while (count && Cmpn(data, text, count) == 0)
    Erase(0, count);
}
bool idStr::StripLeadingOnce(const char* text) {
  const int count = Length(text);
  if (count && Cmpn(data, text, count) == 0) {
    Erase(0, count);
    return true;
  }
  return false;
}
void idStr::StripTrailing(char value) {
  while (len > 0 && data[len - 1] == value)
    --len;
  data[len] = '\0';
}
void idStr::StripTrailing(const char* text) {
  const int count = Length(text);
  while (count <= len && count > 0 && Cmp(data + len - count, text) == 0) {
    len -= count;
    data[len] = '\0';
  }
}
bool idStr::StripTrailingOnce(const char* text) {
  const int count = Length(text);
  if (count <= len && count > 0 && Cmp(data + len - count, text) == 0) {
    len -= count;
    data[len] = '\0';
    return true;
  }
  return false;
}
void idStr::Strip(char value) {
  StripLeading(value);
  StripTrailing(value);
}
void idStr::Strip(const char* text) {
  StripLeading(text);
  StripTrailing(text);
}
idStr& idStr::StripQuotes() {
  if (len >= 2 && data[0] == '"' && data[len - 1] == '"') {
    Erase(0, 1);
    CapLength(len - 1);
  }
  return *this;
}
void idStr::StripLeadingWhiteSpace() {
  int count = 0;
  while (count < len && std::isspace(static_cast<unsigned char>(data[count])))
    ++count;
  if (count)
    Erase(0, count);
}
void idStr::StripTrailingWhitespace() {
  while (len > 0 && std::isspace(static_cast<unsigned char>(data[len - 1])))
    --len;
  data[len] = '\0';
}
bool idStr::ReplaceChar(char oldValue, char newValue) {
  bool changed = false;
  for (int i = 0; i < len; ++i)
    if (data[i] == oldValue) {
      data[i] = newValue;
      changed = true;
    }
  return changed;
}

bool idStr::Replace(const char* oldText, const char* newText) {
  const int oldLength = Length(oldText);
  if (oldLength == 0)
    return false;
  bool changed = false;
  int position = 0;
  while ((position = Find(data, oldText, true, position, -1)) >= 0) {
    Erase(position, oldLength);
    Insert(newText, position);
    position += Length(newText);
    changed = true;
  }
  return changed;
}
bool idStr::IReplace(const char* oldText, const char* newText) {
  const int oldLength = Length(oldText);
  if (oldLength == 0)
    return false;
  bool changed = false;
  int position = 0;
  while ((position = Find(data, oldText, false, position, -1)) >= 0) {
    Erase(position, oldLength);
    Insert(newText, position);
    position += Length(newText);
    changed = true;
  }
  return changed;
}
bool idStr::ReplaceFirst(const char* oldText, const char* newText) {
  const int position = Find(data, oldText, true, 0, -1);
  if (position < 0)
    return false;
  Erase(position, Length(oldText));
  Insert(newText, position);
  return true;
}
void idStr::Erase(int start, int length) {
  if (start < 0) {
    length += start;
    start = 0;
  }
  if (start >= len || length <= 0)
    return;
  length = std::min(length, len - start);
  std::memmove(data + start, data + start + length, len - start - length + 1);
  len -= length;
}
void idStr::ToLower() {
  for (int i = 0; i < len; ++i)
    data[i] = static_cast<char>(Lower(data[i]));
}
void idStr::ToUpper() {
  for (int i = 0; i < len; ++i)
    data[i] = static_cast<char>(std::toupper(static_cast<unsigned char>(data[i])));
}
idStr& idStr::RemoveColors() {
  RemoveColors(data);
  len = Length(data);
  return *this;
}
int idStr::LengthWithoutColors() const {
  return LengthWithoutColors(data);
}
void idStr::PadLength(int newLength) {
  if (newLength > len)
    Append(newLength - len, ' ');
}
void idStr::Fill(char value, int newLength) {
  Empty();
  Append(newLength, value);
}
idStr& idStr::BackSlashesToSlashes() {
  BackSlashesToSlashes(data);
  return *this;
}
idStr& idStr::SlashesToBackSlashes() {
  SlashesToBackSlashes(data);
  return *this;
}

idStr& idStr::CollapsePath() {
  BackSlashesToSlashes();
  const bool rooted = len > 0 && data[0] == '/';
  std::vector<std::string> parts;
  std::string current;
  for (int i = 0; i <= len; ++i) {
    if (i == len || data[i] == '/') {
      if (current == ".." && !parts.empty() && parts.back() != "..")
        parts.pop_back();
      else if (!current.empty() && current != ".")
        parts.push_back(current);
      current.clear();
    } else
      current.push_back(data[i]);
  }
  Empty();
  if (rooted)
    Append('/');
  for (std::size_t i = 0; i < parts.size(); ++i) {
    if (i)
      Append('/');
    Append(parts[i].c_str());
  }
  return *this;
}

idStr& idStr::StripFileExtension() {
  for (int i = len - 1; i >= 0; --i) {
    if (data[i] == '/' || data[i] == '\\')
      break;
    if (data[i] == '.') {
      CapLength(i);
      break;
    }
  }
  return *this;
}
idStr& idStr::StripFilename() {
  for (int i = len - 1; i >= 0; --i)
    if (data[i] == '/' || data[i] == '\\') {
      CapLength(i + 1);
      return *this;
    }
  Empty();
  return *this;
}
idStr& idStr::SetFileExtension(const char* extension) {
  StripFileExtension();
  const std::string ext = NormalizeExtension(extension);
  Append(ext.c_str());
  return *this;
}
idStr& idStr::DefaultFileExtension(const char* extension) {
  for (int i = len - 1; i >= 0; --i) {
    if (data[i] == '/' || data[i] == '\\')
      break;
    if (data[i] == '.')
      return *this;
  }
  const std::string ext = NormalizeExtension(extension);
  Append(ext.c_str());
  return *this;
}
idStr& idStr::DefaultPath(const char* basePath) {
  if (len == 0 || (!(data[0] == '/' || data[0] == '\\') && !(len > 1 && data[1] == ':'))) {
    idStr result(basePath);
    result.AppendPath(data);
    *this = result;
  }
  return *this;
}
idStr& idStr::StripPath() {
  for (int i = len - 1; i >= 0; --i)
    if (data[i] == '/' || data[i] == '\\') {
      Erase(0, i + 1);
      break;
    }
  return *this;
}
void idStr::AppendPath(const char* text) {
  if (!text || !*text)
    return;
  if (len && data[len - 1] != '/' && data[len - 1] != '\\')
    Append('/');
  while (*text == '/' || *text == '\\')
    ++text;
  Append(text);
  BackSlashesToSlashes();
}
void idStr::ExtractFileBase(idStr& destination) const {
  ExtractFileName(destination);
  destination.StripFileExtension();
}
void idStr::ExtractFilePath(idStr& destination) const {
  destination = *this;
  destination.StripFilename();
}
void idStr::ExtractFileName(idStr& destination) const {
  destination = *this;
  destination.StripPath();
}
void idStr::ExtractFileExtension(idStr& destination) const {
  destination.Empty();
  for (int i = len - 1; i >= 0; --i) {
    if (data[i] == '/' || data[i] == '\\')
      break;
    if (data[i] == '.') {
      destination = data + i + 1;
      break;
    }
  }
}
idStr idStr::GetFilePath() const {
  idStr result;
  ExtractFilePath(result);
  return result;
}
idStr idStr::GetWithoutExtension() const {
  idStr result(*this);
  result.StripFileExtension();
  return result;
}
void idStr::MakeNameCanonical() {
  BackSlashesToSlashes();
  ToLower();
}
bool idStr::CheckExtension(const char* extension) const {
  return CheckExtension(data, extension);
}

idStr idStr::Mid(int start, int length) const {
  idStr result;
  Mid(start, length, result);
  return result;
}
idStr& idStr::Mid(int start, int length, idStr& result) const {
  start = std::clamp(start, 0, len);
  length = std::clamp(length, 0, len - start);
  result.CopyRange(data, start, start + length);
  return result;
}
idStr idStr::Left(int length) const {
  return Mid(0, std::clamp(length, 0, len));
}
idStr idStr::Right(int length) const {
  length = std::clamp(length, 0, len);
  return Mid(len - length, length);
}
idStr& idStr::Left(int length, idStr& result) const {
  return Mid(0, std::clamp(length, 0, len), result);
}
idStr& idStr::Right(int length, idStr& result) const {
  length = std::clamp(length, 0, len);
  return Mid(len - length, length, result);
}

void idStr::VFormat(const char* format, std::va_list arguments) {
  char buffer[16384];
  std::va_list copy;
  va_copy(copy, arguments);
  const int result = std::vsnprintf(buffer, sizeof(buffer), format, copy);
  va_end(copy);
  if (result < 0)
    Empty();
  else
    *this = buffer;
}
void idStr::Format(const char* format, ...) {
  std::va_list arguments;
  va_start(arguments, format);
  VFormat(format, arguments);
  va_end(arguments);
}

idStr idStr::FormatInt(int number, bool cash) {
  bool negative = number < 0;
  unsigned value = negative ? 0u - static_cast<unsigned>(number) : static_cast<unsigned>(number);
  char reversed[64];
  int count = 0, digits = 0;
  do {
    if (digits && digits % 3 == 0)
      reversed[count++] = ',';
    reversed[count++] = char('0' + value % 10);
    value /= 10;
    ++digits;
  } while (value);
  if (negative)
    reversed[count++] = '-';
  if (cash)
    reversed[count++] = '$';
  idStr result;
  while (count)
    result.Append(reversed[--count]);
  return result;
}

void idStr::AppendUTF8Char(std::uint32_t value) {
  if (value <= 0x7F)
    Append(static_cast<char>(value));
  else if (value <= 0x7FF) {
    Append(static_cast<char>(0xC0 | (value >> 6)));
    Append(static_cast<char>(0x80 | (value & 0x3F)));
  } else if (value <= 0xFFFF) {
    Append(static_cast<char>(0xE0 | (value >> 12)));
    Append(static_cast<char>(0x80 | ((value >> 6) & 0x3F)));
    Append(static_cast<char>(0x80 | (value & 0x3F)));
  } else {
    Append(static_cast<char>(0xF0 | (value >> 18)));
    Append(static_cast<char>(0x80 | ((value >> 12) & 0x3F)));
    Append(static_cast<char>(0x80 | ((value >> 6) & 0x3F)));
    Append(static_cast<char>(0x80 | (value & 0x3F)));
  }
}

idStr& idStr::Shorten(int maximumLength, char separator) {
  if (maximumLength < 0) {
    Empty();
    return *this;
  }
  if (len <= maximumLength)
    return *this;
  if (maximumLength <= 3) {
    CapLength(maximumLength);
    return *this;
  }
  const int left = (maximumLength - 3) / 2;
  const int right = maximumLength - 3 - left;
  idStr result = Left(left);
  result.Append(separator);
  result.Append(separator);
  result.Append(separator);
  result.Append(data + len - right);
  *this = result;
  return *this;
}

bool idStr::CheckExtension(const char* name, const char* extension) {
  if (!name || !extension)
    return false;
  const std::string ext = NormalizeExtension(extension);
  const int nameLength = Length(name), extLength = static_cast<int>(ext.size());
  return extLength <= nameLength && Icmp(name + nameLength - extLength, ext.c_str()) == 0;
}

bool operator<(const idStr& left, const idStr& right) {
  return left.Cmp(right.data) < 0;
}
bool operator==(const idStr& left, const idStr& right) {
  return left.Cmp(right.data) == 0;
}
bool operator==(const idStr& left, const char* right) {
  return left.Cmp(right) == 0;
}
bool operator==(const char* left, const idStr& right) {
  return idStr::Cmp(left, right.data) == 0;
}
bool operator!=(const idStr& left, const idStr& right) {
  return !(left == right);
}
bool operator!=(const idStr& left, const char* right) {
  return !(left == right);
}
bool operator!=(const char* left, const idStr& right) {
  return !(left == right);
}
idStr operator+(const idStr& left, const idStr& right) {
  idStr result(left);
  result.Append(right);
  return result;
}
idStr operator+(const idStr& left, const char* right) {
  idStr result(left);
  result.Append(right);
  return result;
}
idStr operator+(const char* left, const idStr& right) {
  idStr result(left);
  result.Append(right);
  return result;
}
