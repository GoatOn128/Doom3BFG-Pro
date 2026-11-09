#pragma once

#include "lexer.h"
#include "shared/idlib/containers/list.h"

class idParser {
public:
  struct define_t {
    idStr name;
    int scope = 0;
    int builtin = 0;
    int numparms = 0;
    idToken* parms = nullptr;
    idToken* tokens = nullptr;
    define_t* next = nullptr;
    define_t* hashnext = nullptr;
  };

  struct indent_t {
    int type;
    int skip;
    int skipElse;
    idLexer* script;
  };

  struct idDependency {
    int includeLevel = 0;
    idStr fileName;
  };

  explicit idParser(int parserFlags = 0);
  ~idParser();
  bool LoadFile(const char* fileName, bool useOSPath = false);
  bool LoadMemory(const char* pointer, int sourceLength, const char* name);
  void FreeSource();
  bool ReadToken(idToken& token);
  bool ReadTokenOnLine(idToken& token);
  void UnreadToken(const idToken& token);
  bool CheckTokenString(const char* string);
  bool CheckTokenType(int type, int subtype, idToken& token);
  bool PeekTokenString(const char* string);
  bool PeekTokenType(int type, int subtype, idToken& token);
  bool ExpectTokenString(const char* string, idToken& token);
  bool ExpectTokenString(const char* string);
  bool ExpectTokenType(int type, int subtype, idToken& token);
  bool ExpectAnyToken(idToken& token);
  bool SkipUntilString(const char* string);
  bool SkipRestOfLine();
  bool SkipBracedSection(bool parseFirstBrace = true);
  idStr& ParseRestOfLine(idStr& output);
  bool ParseBracedSectionExact(idStr& output, bool parseFirstBrace = true);
  int ParseInt();
  bool ParseBool();
  float ParseFloat(bool* errorFlag = nullptr);
  bool Parse1DMatrix(int count, float* matrix);
  int GetNextWhiteSpace(idStr& whiteSpace, bool currentLine = false);
  int GetLastWhiteSpace(idStr& whiteSpace);
  int GetPunctuationId(const char* punctuation) const;
  int GetFlags() const { return flags; }
  void SetFlags(int parserFlags);
  bool EndOfFile() const;
  bool HadError() const;
  bool HadWarning() const;
  void Error(const char* format, ...);
  void Warning(const char* format, ...);
  void PushDefineScope();
  void PopDefineScope();
  define_t* AddDefine(const char* definition);
  static define_t* AddGlobalDefine(const char* definition);
  static void SetupGlobalDefines();
  void AddInclude(const char* fileName);
  void AddDependency(int dependencyLevel, const char* fileName);
  int GetNumDependencies(int dependencyLevel = -1) const;
  const idDependency& GetDependency(int index) const;
  void SetSourceLine(int sourceLine);
  int GetTokenSourceOffset(const idToken& token) const;
  int GetCurrentSourceOffset() const;
  int GetCurrentSourceLine() const;
  int GetIncludeLevel() const { return includeLevel; }
  const char* GetCurrentSourceName() const {
    return scriptstack != nullptr ? scriptstack->filename.data : "";
  }

private:
  void AddDefineToHash(define_t* definition);
  define_t* FindHashedDefine(const char* name) const;
  int FindDefineParm(define_t* definition, const char* name) const;
  define_t* CopyDefine(const define_t* definition) const;
  static void FreeDefine(define_t* definition);
  bool ReadSourceToken(idToken& token);
  bool UnreadSourceToken(const idToken& token);
  void UnreadSignToken();
  bool ExpandBuiltinDefine(const idToken& defineToken, define_t* definition, idToken*& firstToken,
                           idToken*& lastToken);
  bool ExpandDefine(const idToken& defineToken, define_t* definition, idToken*& firstToken,
                    idToken*& lastToken);
  bool ExpandDefineIntoSource(const idToken& defineToken, define_t* definition);
  int ReadDefineParms(define_t* definition, idToken** parameters, int maximum);
  bool ReadLine(idToken& token, bool multiline = false);
  bool EvaluateTokens(idToken* expressionTokens, int& integerValue, long double& floatValue,
                      bool integer) const;
  bool Evaluate(int& integerValue, long double& floatValue, bool integer);
  bool EvaluateFunction(int& integerValue, long double& floatValue, bool integer);
  void PushIndent(int type, int shouldSkip, int skipElse);
  bool Directive_define(bool isTemplate = false);
  bool Directive_undef();
  bool Directive_include();
  bool Directive_if_def(int type);
  bool Directive_else();
  bool Directive_if();
  bool Directive_elif();
  bool Directive_endif();
  bool Directive_line();
  bool Directive_error();
  bool Directive_warning();
  bool Directive_pragma();
  bool Directive_evalint();
  bool Directive_evalfloat();
  bool DollarDirective_if_def(int type);
  bool DollarDirective_else();
  bool DollarDirective_elif();
  bool DollarDirective_endif();
  bool ReadDirective();
  bool ReadDollarDirective();
  static define_t* DefineFromString(const char* definition);
  bool PushScript(idLexer* script);
  void AddGlobalDefinesToSource();

  bool loaded = false;
  idStr filename;
  idStr includepath;
  bool OSPath = false;
  const punctuation_t* punctuations = nullptr;
  int flags = 0;
  idLexer* scriptstack = nullptr;
  idToken* tokens = nullptr;
  int defineScope = 0;
  define_t* definehash[128]{};
  idList<indent_t, 11> indentstack;
  int skip = 0;
  void (*pragmaCallback)(void*, const char*) = nullptr;
  void* pragmaData = nullptr;
  bool hadError = false;
  bool hadWarning = false;
  int includeLevel = 0;
  idList<idDependency, 11> dependencies;
};

int PC_OperatorPriority(int operation);
idStr PreProcessorDate();
idStr PreProcessorTime();

static_assert(sizeof(idParser::define_t) == 88, "parser define ABI drift");
static_assert(sizeof(idParser) == 1240, "idParser ABI drift");
