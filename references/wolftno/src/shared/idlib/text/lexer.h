#pragma once

#include "token.h"

#include <cstddef>
#include <cstdint>

struct punctuation_t {
  const char* p;
  int n;
};

enum tokenType_t {
  TT_STRING = 1,
  TT_LITERAL = 2,
  TT_NUMBER = 3,
  TT_NAME = 4,
  TT_PUNCTUATION = 5,
};

enum numberSubtype_t {
  TT_INTEGER = 0x00001,
  TT_DECIMAL = 0x00002,
  TT_HEX = 0x00004,
  TT_OCTAL = 0x00008,
  TT_BINARY = 0x00010,
  TT_LONG = 0x00020,
  TT_UNSIGNED = 0x00040,
  TT_FLOAT = 0x00080,
  TT_SINGLE_PRECISION = 0x00100,
  TT_DOUBLE_PRECISION = 0x00200,
  TT_EXTENDED_PRECISION = 0x00400,
  TT_INFINITE = 0x00800,
  TT_INDEFINITE = 0x01000,
  TT_NAN = 0x02000,
  TT_IPADDRESS = 0x04000,
  TT_VALUESVALID = 0x10000,
};

enum lexerFlags_t {
  LEXFL_NOERRORS = 0x000001,
  LEXFL_NOWARNINGS = 0x000002,
  LEXFL_NOFATALERRORS = 0x000004,
  LEXFL_VCSTYLEREPORTS = 0x000008,
  LEXFL_NOSTRINGCONCAT = 0x000010,
  LEXFL_NOSTRINGESCAPECHARS = 0x000020,
  LEXFL_NODOLLARPRECOMPILE = 0x000040,
  LEXFL_NOBASEINCLUDES = 0x000080,
  LEXFL_NOGLOBALDEFINES = 0x000100,
  LEXFL_ALLOWPATHNAMES = 0x000200,
  LEXFL_ALLOWNUMBERNAMES = 0x000400,
  LEXFL_ALLOWIPADDRESSES = 0x000800,
  LEXFL_ALLOWFLOATEXCEPTIONS = 0x001000,
  LEXFL_ALLOWMULTICHARLITERALS = 0x002000,
  LEXFL_ALLOWBACKSLASHSTRINGCONCAT = 0x004000,
  LEXFL_ONLYSTRINGS = 0x008000,
  LEXFL_NOEMITSTRINGESCAPECHARS = 0x010000,
  LEXFL_ALLOWRAWSTRINGBLOCKS = 0x020000,
  LEXFL_THROWFATAL = 0x040000,
  LEXFL_NOSTRINGS = 0x080000,
  LEXFL_ALLOWWILDCARD = 0x100000,
  LEXFL_REPORT_MULTIPLE_ERRORS = 0x200000,
};

class idLexer {
public:
  explicit idLexer(int lexerFlags = 0);
  ~idLexer();

  void CreatePunctuationTable(const punctuation_t* punctuationList);
  const char* GetPunctuationFromId(int id) const;
  int GetPunctuationId(const char* punctuation) const;
  bool ReadPunctuation(idToken& token);
  void UnreadToken();
  idStr& ParseRestOfLine(idStr& output);
  idStr& ParseCompleteLine(idStr& output);
  int GetLastWhiteSpace(idStr& whiteSpace) const;
  bool GetWhiteSpaceBeforeToken(const idToken& token, idStr& whiteSpace) const;
  bool EndOfFile() const;
  bool LoadMemory(const char* pointer, std::size_t sourceLength, const char* name);
  bool HadError() const;
  bool HadWarning() const;
  bool SetScriptP(const char* pointer);
  void Error(const char* format, ...);
  void Warning(const char* format, ...);
  void SetPunctuations(const punctuation_t* punctuationList);
  bool SkipWhiteSpace(bool currentLine = false);
  bool ReadEscapeCharacter(char& value);
  bool ReadString(idToken& token, int quote);
  bool ReadName(idToken& token);
  bool ReadNumber(idToken& token);
  bool ReadRawStringBlock(idToken& token);
  bool ReadToken(idToken& token);
  bool ExpectTokenString(const char* string, idToken& token);
  bool ExpectTokenString(const char* string);
  bool ExpectAnyToken(idToken& token);
  int GetNextWhiteSpace(idStr& whiteSpace, bool currentLine = false);
  void Reset();
  bool LoadFile(const char* path, bool osPath = false);
  void FreeSource();
  bool ReadTokenOnLine(idToken& token);
  idStr& ParseBracedSection(idStr& output, int tabs = -1, bool parseFirstBrace = true,
                            char intro = '{', char outro = '}');
  bool ParseBracedSectionExact(idStr& output, bool parseFirstBrace = true, char intro = '{',
                               char outro = '}');
  bool ExpectTokenType(int type, int subtype, idToken& token);
  bool CheckTokenString(const char* string);
  bool CheckTokenType(int type, int subtype, idToken& token);
  bool PeekTokenString(const char* string);
  bool PeekTokenType(int type, int subtype, idToken& token);
  bool SkipUntilString(const char* string);
  bool SkipRestOfLine();
  bool SkipBracedSection(bool parseFirstBrace = true);
  int ParseInt();
  unsigned ParseUnsignedInt();
  bool ParseBool();
  float ParseFloat(bool* errorFlag = nullptr);
  bool Parse1DMatrix(int count, float* matrix, bool expectCommas = false);
  bool Parse2DMatrix(int rows, int columns, float* matrix);
  void SetFlags(int lexerFlags) { flags = lexerFlags; }
  unsigned GetFileTime() const { return fileTime; }
  const char* GetScriptP() const { return script_p; }

  static const punctuation_t defaultPunctuations[];

  friend class idParser;

protected:
  bool loaded = false;
  idStr filename;
  int allocated = 0;
  const char* buffer = nullptr;
  const char* script_p = nullptr;
  const char* end_p = nullptr;
  const char* lastScript_p = nullptr;
  const char* whiteSpaceStart_p = nullptr;
  const char* whiteSpaceEnd_p = nullptr;
  unsigned fileTime = 0;
  std::uint64_t length = 0;
  int line = 0;
  int lastline = 0;
  int flags = 0;
  const punctuation_t* punctuations = nullptr;
  int* punctuationtable = nullptr;
  int* nextpunctuation = nullptr;
  idLexer* next = nullptr;
  idStr errorMsg;
  bool hadError = false;
  bool hadWarning = false;
};

static_assert(sizeof(idLexer) == 216, "idLexer ABI drift");
