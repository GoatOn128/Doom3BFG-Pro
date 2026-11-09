#pragma once

#include "shared/idlib/math/vector.h"

#include <cstdarg>
#include <cstddef>
#include <cstdint>

enum utf8Encoding_t { UTF8_INVALID, UTF8_ASCII, UTF8_ENCODED };

class va {
public:
  va() : buffer{} {}
  va(const char* format, ...);
  char buffer[16384];
  operator const char*() const { return buffer; }
};

class idStr {
public:
  int len;
  char* data;
  int allocedAndFlag;
  char baseBuffer[20];

  idStr();
  idStr(const idStr& text);
  idStr(const va& text);
  idStr(const idStr& text, int start, int end);
  idStr(const char* text);
  idStr(const char* text, int start, int end);
  explicit idStr(bool value);
  explicit idStr(char value);
  explicit idStr(int value);
  explicit idStr(float value);
#if defined(WOLFTNO_RETAIL_IDSTR_INLINE_FREEDATA_ABI)
  __forceinline ~idStr() { FreeData(); }
#else
  ~idStr();
#endif

  idStr& operator=(const idStr& text);
  idStr& operator=(const va& text);
  idStr& operator=(const char* text);
  idStr& operator=(char value);
  char operator[](int index) const;
  char& operator[](int index);

  static const idVec4& ColorForIndex(char index);
  static char* CStyleUnQuote(const char* text);
  static char* CStyleQuote(const char* text);
  static bool IsNumeric(const char* text);
  static void Copynz(char* destination, const char* source, int destinationSize);
  static void CopyCountAndZero(char* destination, std::size_t destinationSize, const char* source,
                               std::size_t count, std::size_t sourceSize = ~std::size_t(0));
  static int vsnPrintf(char* destination, int size, const char* format, std::va_list arguments);
  static int snPrintf(char* destination, int size, const char* format, ...);
  static std::uint32_t UTF8Char(const std::uint8_t* text, int* index);
  static bool EmitEscapeChar(const char** text, char* value);
  static int Length(const char* text);
  static int Hash(const char* text);
  static bool IsColor(const char* text);
  static int ToLower(unsigned char value);
  static int ToUpper(unsigned char value);
  static bool CharIsPrintable(int value);
  static bool CharIsLower(int value);
  static bool CharIsUpper(int value);
  static bool CharIsAlpha(int value);
  static bool CharIsNumeric(int value);
  static int ColorIndex(char value);
  static int Find(const char* text, char value, int start = 0, int end = -1);
  static int Find(const char* text, const char* search, bool caseSensitive = true, int start = 0,
                  int end = -1);
  static int Last(const char* text, char value, unsigned index);
  static int FileNameHash(const char* text, int hashSize);
  static void BackSlashesToSlashes(char* path);
  static void SlashesToBackSlashes(char* path);
  static bool IsNameCanonical(const char* name);
  static int Cmp(const char* left, const char* right);
  static int Cmpn(const char* left, const char* right, int count);
  static int CmpPrefix(const char* left, const char* right);
  static int Icmp(const char* left, const char* right);
  static int Icmpn(const char* left, const char* right, int count);
  static int IcmpPrefix(const char* left, const char* right);
  static int IcmpPath(const char* left, const char* right);
  static int IcmpnPath(const char* left, const char* right, int count);
  static void Append(char* destination, int size, const char* source);
  static int LengthWithoutColors(const char* text);
  static char* RemoveColors(char* text);
  static bool IsValidUTF8(const std::uint8_t* text, int maxLength, utf8Encoding_t* encoding);
  static bool IsValidUTF8(const std::uint8_t* text, int maxLength);
  static bool ToEscapedString(const char* text, int length, char* output, int outputSize);
  static int IHash(const char* text);
  static char* FloatArrayToString(const float* values, int count, int precision);
  static idStr FormatInt(int number, bool cash = false);
  static bool CheckExtension(const char* name, const char* extension);

  bool IsEmpty() const;
  bool IsNumeric() const;
  int Length() const;
  const char* c_str() const;
  int Allocated() const;
  int Size() const;
  int CountChar(char value) const;
  std::uint32_t UTF8Char(int* index) const;
  void EnsureAlloced(int amount, bool keepOld = true, bool geometricGrowth = true);
  void ReAllocate(int amount, bool keepOld);
  void FreeData();
  void CapLength(int newLength);
  void Empty();
  void Clear();
  void CopyRange(const char* text, int start, int end);
  int Last(char value, int index = -1) const;
  int Find(char value, unsigned start = 0, int end = -1) const;
  int Find(const char* search, bool caseSensitive = true, int start = 0, int end = -1) const;
  bool EndsWith(const char* text) const;
  int Cmp(const char* text) const;
  int Cmpn(const char* text, int count) const;
  int CmpPrefix(const char* text) const;
  int Icmp(const char* text) const;
  int Icmpn(const char* text, int count) const;
  int IcmpPrefix(const char* text) const;
  int IcmpPath(const char* text) const;
  int IcmpnPath(const char* text, int count) const;

  void Append(char value);
  void Append(const idStr& text);
  void Append(const va& text);
  void Append(const char* text);
  void Append(const char* text, int length);
  void Append(int count, char value);
  void Insert(char value, int index);
  void Insert(const char* text, int index);
  idStr& operator+=(const idStr& text);
  idStr& operator+=(const va& text);
  idStr& operator+=(const char* text);
  idStr& operator+=(int value);
  idStr& operator+=(char value);

  void StripLeading(char value);
  void StripLeading(const char* text);
  bool StripLeadingOnce(const char* text);
  void StripTrailing(char value);
  void StripTrailing(const char* text);
  bool StripTrailingOnce(const char* text);
  void Strip(char value);
  void Strip(const char* text);
  idStr& StripQuotes();
  void StripLeadingWhiteSpace();
  void StripTrailingWhitespace();
  bool ReplaceChar(char oldValue, char newValue);
  bool Replace(const char* oldText, const char* newText);
  bool IReplace(const char* oldText, const char* newText);
  bool ReplaceFirst(const char* oldText, const char* newText);
  void Erase(int start, int length);
  void ToLower();
  void ToUpper();
  idStr& RemoveColors();
  int LengthWithoutColors() const;
  void PadLength(int newLength);
  void Fill(char value, int newLength);
  idStr& BackSlashesToSlashes();
  idStr& SlashesToBackSlashes();
  idStr& CollapsePath();
  idStr& StripFileExtension();
  idStr& StripFilename();
  idStr& SetFileExtension(const char* extension);
  idStr& DefaultFileExtension(const char* extension);
  idStr& DefaultPath(const char* basePath);
  idStr& StripPath();
  void AppendPath(const char* text);
  void ExtractFileBase(idStr& destination) const;
  void ExtractFilePath(idStr& destination) const;
  void ExtractFileName(idStr& destination) const;
  void ExtractFileExtension(idStr& destination) const;
  idStr GetFilePath() const;
  idStr GetFileBase() const;
  idStr GetWithoutExtension() const;
  std::intptr_t GetFileName(...) const;
  void MakeNameCanonical();
  bool CheckExtension(const char* extension) const;
  idStr Mid(int start, int length) const;
  idStr& Mid(int start, int length, idStr& result) const;
  idStr Left(int length) const;
  idStr Right(int length) const;
  idStr& Left(int length, idStr& result) const;
  idStr& Right(int length, idStr& result) const;
  void VFormat(const char* format, std::va_list arguments);
  void Format(const char* format, ...);
  void AppendUTF8Char(std::uint32_t value);
  idStr& Shorten(int maximumLength, char separator = '.');
};

template <int Capacity, int Tag = 0> class idStrStaticBase : public idStr {
public:
  char buffer[Capacity];

  idStrStaticBase() : idStr() {
    data = buffer;
    len = 0;
    allocedAndFlag = static_cast<int>(0x80000000u | Capacity);
    buffer[0] = '\0';
  }
  idStrStaticBase(const char* text) : idStrStaticBase() { idStr::operator=(text); }
  idStrStaticBase(const idStr& text) : idStrStaticBase() { idStr::operator=(text); }
  idStrStaticBase(const idStrStaticBase& text) : idStrStaticBase() {
    idStr::operator=(static_cast<const idStr&>(text));
  }
  idStrStaticBase& operator=(const idStrStaticBase& text) {
    idStr::operator=(static_cast<const idStr&>(text));
    return *this;
  }
  using idStr::operator=;
};

template <int Capacity> class idStrStatic : public idStrStaticBase<Capacity, 0> {
public:
  using idStrStaticBase<Capacity, 0>::idStrStaticBase;
  using idStrStaticBase<Capacity, 0>::operator=;
};

// Retail uses this distinct name for dynamically-fallback-capable strings with
// inline storage.  Its storage contract is identical to idStrStaticBase.
template <int Capacity> class idStrDynStatic : public idStrStaticBase<Capacity, 0> {
public:
  using idStrStaticBase<Capacity, 0>::idStrStaticBase;
  using idStrStaticBase<Capacity, 0>::operator=;
};

char* CopyString(const char* text, unsigned tag = 0);
bool operator<(const idStr& left, const idStr& right);
bool operator==(const idStr& left, const idStr& right);
bool operator==(const idStr& left, const char* right);
bool operator==(const char* left, const idStr& right);
bool operator!=(const idStr& left, const idStr& right);
bool operator!=(const idStr& left, const char* right);
bool operator!=(const char* left, const idStr& right);
idStr operator+(const idStr& left, const idStr& right);
idStr operator+(const idStr& left, const char* right);
idStr operator+(const char* left, const idStr& right);

static_assert(sizeof(idStr) == 40, "idStr ABI drift");
static_assert(sizeof(idStrStatic<64>) == 104, "idStrStatic ABI drift");
