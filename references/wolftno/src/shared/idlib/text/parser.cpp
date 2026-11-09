#include "parser.h"
#include "shared/idlib/filesystem/filesystemlocal.h"

#include <algorithm>
#include <cmath>
#include <cstdarg>
#include <ctime>
#include <filesystem>
#include <functional>
#include <string>
#include <vector>

namespace {
idParser::define_t* globalDefines = nullptr;

unsigned DefineHash(const char* name) {
  return static_cast<unsigned>(idStr::IHash(name)) & 127u;
}

idToken* CloneToken(const idToken& source) {
  auto* token = new idToken;
  static_cast<idStr&>(*token) = static_cast<const idStr&>(source);
  token->type = source.type;
  token->subtype = source.subtype;
  token->line = source.line;
  token->linesCrossed = source.linesCrossed;
  token->flags = source.flags;
  token->intvalue = source.intvalue;
  token->floatvalue = source.floatvalue;
  token->SetSafeWhiteSpacePointers();
  token->next = nullptr;
  return token;
}

void DeleteTokens(idToken* token) {
  while (token != nullptr) {
    idToken* next = token->next;
    delete token;
    token = next;
  }
}

void AppendToken(idToken*& first, idToken*& last, idToken* token) {
  if (last != nullptr)
    last->next = token;
  else
    first = token;
  last = token;
}
} // namespace

int PC_OperatorPriority(int operation) {
  switch (operation) {
  case 6:
    return 1;
  case 5:
    return 2;
  case 33:
    return 3;
  case 34:
    return 4;
  case 32:
    return 5;
  case 9:
  case 10:
    return 6;
  case 7:
  case 8:
  case 37:
  case 38:
    return 7;
  case 21:
  case 22:
    return 8;
  case 29:
  case 30:
    return 9;
  case 26:
  case 27:
  case 28:
    return 10;
  default:
    return 0;
  }
}

idStr PreProcessorDate() {
  std::time_t now = std::time(nullptr);
  std::tm local{};
  localtime_s(&local, &now);
  char output[32];
  std::strftime(output, sizeof(output), "%b %d %Y", &local);
  return idStr(output);
}

idStr PreProcessorTime() {
  std::time_t now = std::time(nullptr);
  std::tm local{};
  localtime_s(&local, &now);
  char output[32];
  std::strftime(output, sizeof(output), "%H:%M:%S", &local);
  return idStr(output);
}

idParser::idParser(int parserFlags)
    : punctuations(idLexer::defaultPunctuations), flags(parserFlags) {
  SetupGlobalDefines();
}

idParser::~idParser() {
  FreeSource();
}

void idParser::AddDefineToHash(define_t* definition) {
  const unsigned hash = DefineHash(definition->name.data);
  definition->hashnext = definehash[hash];
  definehash[hash] = definition;
}

void idParser::PushDefineScope() {
  ++defineScope;
}

int idParser::GetNextWhiteSpace(idStr& whiteSpace, bool currentLine) {
  return scriptstack != nullptr ? scriptstack->GetNextWhiteSpace(whiteSpace, currentLine) : 0;
}

int idParser::GetLastWhiteSpace(idStr& whiteSpace) {
  return scriptstack != nullptr ? scriptstack->GetLastWhiteSpace(whiteSpace) : 0;
}

idParser::define_t* idParser::FindHashedDefine(const char* name) const {
  for (define_t* definition = definehash[DefineHash(name)]; definition != nullptr;
       definition = definition->hashnext)
    if (definition->name == name)
      return definition;
  return nullptr;
}

int idParser::FindDefineParm(define_t* definition, const char* name) const {
  int index = 0;
  for (idToken* parameter = definition->parms; parameter != nullptr;
       parameter = parameter->next, ++index)
    if (*parameter == name)
      return index;
  return -1;
}

void idParser::Error(const char* format, ...) {
  hadError = true;
  if (flags & LEXFL_NOERRORS)
    return;
  va_list arguments;
  va_start(arguments, format);
  idStr message;
  message.VFormat(format, arguments);
  va_end(arguments);
  if (scriptstack != nullptr)
    scriptstack->Error("%s", message.data);
}

void idParser::Warning(const char* format, ...) {
  hadWarning = true;
  if (flags & LEXFL_NOWARNINGS)
    return;
  va_list arguments;
  va_start(arguments, format);
  idStr message;
  message.VFormat(format, arguments);
  va_end(arguments);
  if (scriptstack != nullptr)
    scriptstack->Warning("%s", message.data);
}

bool idParser::EvaluateTokens(idToken* expressionTokens, int& integerValue, long double& floatValue,
                              bool integer) const {
  std::vector<idToken*> expression;
  for (idToken* token = expressionTokens; token != nullptr; token = token->next)
    expression.push_back(token);
  std::size_t position = 0;
  std::function<long double(int)> parse = [&](int minimumPriority) -> long double {
    if (position >= expression.size())
      return 0.0L;
    idToken* token = expression[position++];
    long double left;
    if (*token == "(") {
      left = parse(1);
      if (position < expression.size() && *expression[position] == ")")
        ++position;
    } else if (*token == "!" || *token == "~" || *token == "-" || *token == "+") {
      const long double value = parse(11);
      left = *token == "!"   ? (value == 0.0L)
             : *token == "~" ? static_cast<long double>(~static_cast<long long>(value))
             : *token == "-" ? -value
                             : value;
    } else if (token->type == TT_NUMBER) {
      token->NumberValue();
      left = (token->subtype & TT_FLOAT) ? token->floatvalue : token->intvalue;
    } else {
      left = 0.0L;
    }
    while (position < expression.size()) {
      idToken* operation = expression[position];
      const int priority =
          operation->type == TT_PUNCTUATION ? PC_OperatorPriority(operation->subtype) : 0;
      if (priority < minimumPriority)
        break;
      ++position;
      const long double right = parse(priority + 1);
      if (*operation == "+")
        left += right;
      else if (*operation == "-")
        left -= right;
      else if (*operation == "*")
        left *= right;
      else if (*operation == "/")
        left = right == 0.0L ? 0.0L : left / right;
      else if (*operation == "%")
        left = static_cast<long double>(static_cast<long long>(right) == 0
                                            ? 0
                                            : static_cast<long long>(left) %
                                                  static_cast<long long>(right));
      else if (*operation == "==")
        left = left == right;
      else if (*operation == "!=")
        left = left != right;
      else if (*operation == "<")
        left = left < right;
      else if (*operation == ">")
        left = left > right;
      else if (*operation == "<=")
        left = left <= right;
      else if (*operation == ">=")
        left = left >= right;
      else if (*operation == "&&")
        left = left != 0.0L && right != 0.0L;
      else if (*operation == "||")
        left = left != 0.0L || right != 0.0L;
      else if (*operation == "&")
        left =
            static_cast<long double>(static_cast<long long>(left) & static_cast<long long>(right));
      else if (*operation == "|")
        left =
            static_cast<long double>(static_cast<long long>(left) | static_cast<long long>(right));
      else if (*operation == "^")
        left =
            static_cast<long double>(static_cast<long long>(left) ^ static_cast<long long>(right));
      else if (*operation == "<<")
        left = static_cast<long double>(static_cast<long long>(left) << static_cast<int>(right));
      else if (*operation == ">>")
        left = static_cast<long double>(static_cast<long long>(left) >> static_cast<int>(right));
    }
    return left;
  };
  floatValue = parse(1);
  integerValue = static_cast<int>(floatValue);
  if (integer)
    floatValue = integerValue;
  return true;
}

int idParser::GetPunctuationId(const char* punctuation) const {
  return scriptstack != nullptr ? scriptstack->GetPunctuationId(punctuation) : 0;
}

void idParser::SetFlags(int parserFlags) {
  flags = parserFlags;
  for (idLexer* script = scriptstack; script != nullptr; script = script->next)
    script->flags = parserFlags;
}

bool idParser::EndOfFile() const {
  return tokens == nullptr && scriptstack == nullptr;
}
bool idParser::HadError() const {
  return hadError;
}
bool idParser::HadWarning() const {
  return hadWarning;
}

idParser::define_t* idParser::CopyDefine(const define_t* definition) const {
  auto* copy = new define_t;
  copy->name = definition->name;
  copy->scope = definition->scope;
  copy->builtin = definition->builtin;
  copy->numparms = definition->numparms;
  idToken* last = nullptr;
  for (idToken* token = definition->parms; token != nullptr; token = token->next)
    AppendToken(copy->parms, last, CloneToken(*token));
  last = nullptr;
  for (idToken* token = definition->tokens; token != nullptr; token = token->next)
    AppendToken(copy->tokens, last, CloneToken(*token));
  return copy;
}

void idParser::FreeDefine(define_t* definition) {
  if (definition == nullptr)
    return;
  DeleteTokens(definition->parms);
  DeleteTokens(definition->tokens);
  delete definition;
}

bool idParser::UnreadSourceToken(const idToken& token) {
  idToken* copy = CloneToken(token);
  copy->next = tokens;
  tokens = copy;
  return true;
}

bool idParser::ExpandBuiltinDefine(const idToken& defineToken, define_t* definition,
                                   idToken*& firstToken, idToken*& lastToken) {
  idToken* token = new idToken;
  token->line = defineToken.line;
  if (definition->builtin == 1) {
    token->Format("%d", defineToken.line);
    token->type = TT_NUMBER;
    token->subtype = TT_INTEGER | TT_DECIMAL;
  } else if (definition->builtin == 2) {
    *token = filename;
    token->type = TT_STRING;
  } else if (definition->builtin == 3) {
    *token = PreProcessorDate();
    token->type = TT_STRING;
  } else if (definition->builtin == 4) {
    *token = PreProcessorTime();
    token->type = TT_STRING;
  } else {
    delete token;
    return false;
  }
  AppendToken(firstToken, lastToken, token);
  return true;
}

void idParser::PopDefineScope() {
  if (defineScope <= 0)
    return;
  for (define_t*& bucket : definehash) {
    define_t** link = &bucket;
    while (*link != nullptr) {
      if ((*link)->scope == defineScope) {
        define_t* removed = *link;
        *link = removed->hashnext;
        FreeDefine(removed);
      } else
        link = &(*link)->hashnext;
    }
  }
  --defineScope;
}

void idParser::AddGlobalDefinesToSource() {
  for (define_t* global = globalDefines; global != nullptr; global = global->next)
    if (FindHashedDefine(global->name.data) == nullptr)
      AddDefineToHash(CopyDefine(global));
}

void idParser::UnreadSignToken() {
  idToken sign;
  sign = "-";
  sign.type = TT_PUNCTUATION;
  sign.subtype = GetPunctuationId("-");
  UnreadSourceToken(sign);
}

bool idParser::CheckTokenString(const char* string) {
  idToken token;
  if (!ReadToken(token))
    return false;
  if (token == string)
    return true;
  UnreadToken(token);
  return false;
}

void idParser::UnreadToken(const idToken& token) {
  UnreadSourceToken(token);
}

bool idParser::LoadFile(const char* fileName, bool useOSPath) {
  if (loaded)
    return false;
  auto* script = new idLexer(flags);
  if (!script->LoadFile(fileName, useOSPath)) {
    delete script;
    return false;
  }
  scriptstack = script;
  filename = fileName;
  filename.ExtractFilePath(includepath);
  OSPath = useOSPath;
  loaded = true;
  AddGlobalDefinesToSource();
  AddDependency(0, fileName);
  return true;
}

bool idParser::LoadMemory(const char* pointer, int sourceLength, const char* name) {
  if (loaded)
    return false;
  auto* script = new idLexer(flags);
  if (!script->LoadMemory(pointer, static_cast<std::size_t>(sourceLength), name)) {
    delete script;
    return false;
  }
  scriptstack = script;
  filename = name;
  filename.ExtractFilePath(includepath);
  loaded = true;
  AddGlobalDefinesToSource();
  return true;
}

void idParser::FreeSource() {
  while (scriptstack != nullptr) {
    idLexer* nextScript = scriptstack->next;
    delete scriptstack;
    scriptstack = nextScript;
  }
  DeleteTokens(tokens);
  tokens = nullptr;
  for (define_t*& bucket : definehash) {
    while (bucket != nullptr) {
      define_t* nextDefine = bucket->hashnext;
      FreeDefine(bucket);
      bucket = nextDefine;
    }
  }
  loaded = false;
  filename.Clear();
  indentstack.Clear();
  dependencies.Clear();
  skip = includeLevel = defineScope = 0;
}

void idParser::AddInclude(const char* fileName) {
  std::filesystem::path path(fileName);
  if (path.is_relative() && !includepath.IsEmpty())
    path = std::filesystem::path(includepath.data) / path;
  idStr logicalPath(path.generic_string().c_str());
  idStr loadPath(logicalPath);
  if (!OSPath) {
    if (auto* local = dynamic_cast<idFileSystemLocal*>(fileSystem))
      loadPath = local->BuildOSPath(logicalPath.data);
  }
  auto* script = new idLexer(flags);
  if (!script->LoadFile(loadPath.data, true)) {
    delete script;
    Warning("couldn't include '%s'", fileName);
    return;
  }
  ++includeLevel;
  AddDependency(includeLevel, logicalPath.data);
  PushScript(script);
}

bool idParser::ReadSourceToken(idToken& token) {
  if (tokens != nullptr) {
    idToken* source = tokens;
    tokens = source->next;
    source->next = nullptr;
    token = static_cast<const idStr&>(*source);
    token.type = source->type;
    token.subtype = source->subtype;
    token.line = source->line;
    token.linesCrossed = source->linesCrossed;
    token.flags = source->flags;
    token.intvalue = source->intvalue;
    token.floatvalue = source->floatvalue;
    delete source;
    return true;
  }
  while (scriptstack != nullptr) {
    if (scriptstack->ReadToken(token))
      return true;
    idLexer* finished = scriptstack;
    scriptstack = finished->next;
    delete finished;
    if (includeLevel > 0)
      --includeLevel;
  }
  return false;
}

int idParser::ReadDefineParms(define_t* definition, idToken** parameters, int maximum) {
  if (definition == nullptr || parameters == nullptr || maximum <= 0)
    return -1;
  for (int index = 0; index < maximum; ++index)
    parameters[index] = nullptr;

  idToken token;
  if (!ReadSourceToken(token))
    return -1;
  if (token != "(") {
    UnreadSourceToken(token);
    return -1;
  }

  int parameter = 0;
  int depth = 0;
  idToken* tails[128]{};
  if (maximum > 128)
    maximum = 128;
  while (ReadSourceToken(token)) {
    if (token == "(") {
      ++depth;
    } else if (token == ")") {
      if (depth == 0) {
        const bool emptyList = parameter == 0 && parameters[0] == nullptr;
        const int count = emptyList ? 0 : parameter + 1;
        if (count != definition->numparms) {
          Error("macro '%s' expects %d arguments, got %d", definition->name.data,
                definition->numparms, count);
          for (int index = 0; index < maximum; ++index)
            DeleteTokens(parameters[index]);
          return -1;
        }
        return count;
      }
      --depth;
    } else if (token == "," && depth == 0) {
      if (++parameter >= maximum) {
        Error("more than %d macro arguments", maximum);
        for (int index = 0; index < maximum; ++index)
          DeleteTokens(parameters[index]);
        return -1;
      }
      continue;
    }
    AppendToken(parameters[parameter], tails[parameter], CloneToken(token));
  }

  Error("missing ')' in macro '%s'", definition->name.data);
  for (int index = 0; index < maximum; ++index)
    DeleteTokens(parameters[index]);
  return -1;
}

bool idParser::ExpandDefine(const idToken& defineToken, define_t* definition, idToken*& firstToken,
                            idToken*& lastToken) {
  if (definition->builtin != 0)
    return ExpandBuiltinDefine(defineToken, definition, firstToken, lastToken);

  idToken* parameters[128]{};
  if (definition->numparms > 0 && ReadDefineParms(definition, parameters, 128) < 0)
    return false;

  for (idToken* token = definition->tokens; token != nullptr; token = token->next) {
    const int parmIndex = token->type == TT_NAME ? FindDefineParm(definition, token->data) : -1;
    if (parmIndex >= 0) {
      for (idToken* value = parameters[parmIndex]; value != nullptr; value = value->next)
        AppendToken(firstToken, lastToken, CloneToken(*value));
    } else {
      AppendToken(firstToken, lastToken, CloneToken(*token));
    }
  }
  for (int index = 0; index < definition->numparms; ++index)
    DeleteTokens(parameters[index]);
  return true;
}

bool idParser::ExpandDefineIntoSource(const idToken& defineToken, define_t* definition) {
  idToken* first = nullptr;
  idToken* last = nullptr;
  if (!ExpandDefine(defineToken, definition, first, last))
    return false;
  if (last != nullptr)
    last->next = tokens;
  tokens = first;
  return true;
}

bool idParser::ReadLine(idToken& token, bool multiline) {
  if (!ReadSourceToken(token))
    return false;
  if (!multiline && token.linesCrossed > 0) {
    UnreadSourceToken(token);
    return false;
  }
  return true;
}

bool idParser::Evaluate(int& integerValue, long double& floatValue, bool integer) {
  idToken* first = nullptr;
  idToken* last = nullptr;
  idToken token;
  while (ReadLine(token, false)) {
    if (token.type == TT_NAME && token != "defined") {
      if (define_t* definition = FindHashedDefine(token.data)) {
        if (!ExpandDefineIntoSource(token, definition)) {
          DeleteTokens(first);
          return false;
        }
        continue;
      }
    }
    AppendToken(first, last, CloneToken(token));
  }
  const bool result = EvaluateTokens(first, integerValue, floatValue, integer);
  DeleteTokens(first);
  return result;
}

bool idParser::EvaluateFunction(int& integerValue, long double& floatValue, bool integer) {
  return Evaluate(integerValue, floatValue, integer);
}

bool idParser::Directive_undef() {
  idToken name;
  if (!ReadLine(name))
    return false;
  const unsigned hash = DefineHash(name.data);
  define_t** link = &definehash[hash];
  while (*link != nullptr) {
    if ((*link)->name == name) {
      define_t* removed = *link;
      *link = removed->hashnext;
      FreeDefine(removed);
      break;
    }
    link = &(*link)->hashnext;
  }
  SkipRestOfLine();
  return true;
}

bool idParser::Directive_endif() {
  if (indentstack.Num() == 0)
    return false;
  indentstack.SetNum(indentstack.Num() - 1);
  skip = 0;
  for (int index = 0; index < indentstack.Num(); ++index)
    if (indentstack[index].skip)
      ++skip;
  return true;
}

bool idParser::Directive_line() {
  SkipRestOfLine();
  return true;
}
bool idParser::Directive_error() {
  idStr message;
  ParseRestOfLine(message);
  Error("%s", message.data);
  return true;
}
bool idParser::Directive_warning() {
  idStr message;
  ParseRestOfLine(message);
  Warning("%s", message.data);
  return true;
}
bool idParser::Directive_pragma() {
  idStr text;
  ParseRestOfLine(text);
  if (pragmaCallback)
    pragmaCallback(pragmaData, text.data);
  return true;
}
bool idParser::Directive_evalint() {
  int value;
  long double f;
  if (!Evaluate(value, f, true))
    return false;
  idToken token;
  token.Format("%d", value);
  token.type = TT_NUMBER;
  token.subtype = TT_INTEGER | TT_DECIMAL;
  UnreadSourceToken(token);
  return true;
}
bool idParser::Directive_evalfloat() {
  int value;
  long double f;
  if (!Evaluate(value, f, false))
    return false;
  idToken token;
  token.Format("%.9g", static_cast<double>(f));
  token.type = TT_NUMBER;
  token.subtype = TT_FLOAT | TT_DECIMAL;
  UnreadSourceToken(token);
  return true;
}
bool idParser::DollarDirective_endif() {
  return Directive_endif();
}

void idParser::PushIndent(int type, int shouldSkip, int skipElse) {
  indentstack.Append({type, shouldSkip, skipElse, scriptstack});
  if (shouldSkip)
    ++skip;
}

void idParser::AddDependency(int dependencyLevel, const char* fileName) {
  idDependency* dependency = dependencies.Alloc();
  dependency->includeLevel = dependencyLevel;
  dependency->fileName = fileName;
}

int idParser::GetNumDependencies(int dependencyLevel) const {
  if (dependencyLevel == -1)
    return dependencies.Num();

  int count = 0;
  for (int i = 0; i < dependencies.Num(); ++i) {
    if (dependencies[i].includeLevel == dependencyLevel)
      ++count;
  }
  return count;
}

const idParser::idDependency& idParser::GetDependency(int index) const {
  return dependencies[index];
}

void idParser::SetSourceLine(int sourceLine) {
  if (scriptstack == nullptr)
    return;
  sourceLine = std::max(sourceLine, 1);
  scriptstack->line = sourceLine;
  scriptstack->lastline = sourceLine;
}

int idParser::GetTokenSourceOffset(const idToken& token) const {
  if (scriptstack == nullptr || scriptstack->buffer == nullptr || token.whiteSpaceEnd_p == nullptr)
    return -1;
  return static_cast<int>(token.whiteSpaceEnd_p - scriptstack->buffer);
}

int idParser::GetCurrentSourceOffset() const {
  if (scriptstack == nullptr || scriptstack->buffer == nullptr || scriptstack->script_p == nullptr)
    return -1;
  return static_cast<int>(scriptstack->script_p - scriptstack->buffer);
}

int idParser::GetCurrentSourceLine() const {
  return scriptstack != nullptr ? scriptstack->line : 0;
}

bool idParser::Directive_define(bool) {
  idToken name;
  if (!ReadLine(name))
    return false;
  auto* definition = new define_t;
  definition->name = name;
  definition->scope = defineScope;
  idToken token;
  idToken* last = nullptr;
  if (ReadLine(token, false)) {
    if (token == "(") {
      idToken* parmLast = nullptr;
      while (ReadLine(token, false) && token != ")") {
        if (token == ",")
          continue;
        if (token.type != TT_NAME) {
          Error("invalid parameter '%s' in macro '%s'", token.data, definition->name.data);
          FreeDefine(definition);
          SkipRestOfLine();
          return false;
        }
        AppendToken(definition->parms, parmLast, CloneToken(token));
        ++definition->numparms;
      }
      if (token != ")") {
        Error("missing ')' in macro definition '%s'", definition->name.data);
        FreeDefine(definition);
        return false;
      }
    } else {
      AppendToken(definition->tokens, last, CloneToken(token));
    }
  }
  while (ReadLine(token, false))
    AppendToken(definition->tokens, last, CloneToken(token));
  define_t* old = FindHashedDefine(definition->name.data);
  if (old != nullptr) {
    const unsigned int bucketIndex = idStr::Hash(old->name.data) & 127u;
    define_t** link = &definehash[bucketIndex];
    while (*link != nullptr && *link != old)
      link = &(*link)->hashnext;
    if (*link == old)
      *link = old->hashnext;
    FreeDefine(old);
  }
  AddDefineToHash(definition);
  return true;
}

bool idParser::Directive_if_def(int type) {
  idToken name;
  if (!ReadLine(name))
    return false;
  const bool defined = FindHashedDefine(name.data) != nullptr;
  PushIndent(type, type ? !defined : defined, 0);
  SkipRestOfLine();
  return true;
}

bool idParser::Directive_else() {
  if (indentstack.Num() == 0)
    return false;
  indent_t& indent = indentstack[indentstack.Num() - 1];
  if (indent.skip)
    --skip;
  indent.skip = !indent.skip && !indent.skipElse;
  indent.skipElse = 1;
  if (indent.skip)
    ++skip;
  return true;
}

bool idParser::Directive_if() {
  int integerValue = 0;
  long double floatValue = 0;
  Evaluate(integerValue, floatValue, true);
  PushIndent(2, integerValue == 0, 0);
  return true;
}

bool idParser::Directive_elif() {
  if (indentstack.Num() == 0)
    return false;
  indent_t& indent = indentstack[indentstack.Num() - 1];
  if (indent.skip)
    --skip;
  int integerValue = 0;
  long double floatValue = 0;
  Evaluate(integerValue, floatValue, true);
  indent.skip = indent.skipElse || integerValue == 0;
  if (!indent.skip)
    indent.skipElse = 1;
  if (indent.skip)
    ++skip;
  return true;
}

bool idParser::DollarDirective_if_def(int type) {
  return Directive_if_def(type);
}
bool idParser::DollarDirective_else() {
  return Directive_else();
}
bool idParser::DollarDirective_elif() {
  return Directive_elif();
}

bool idParser::ReadDollarDirective() {
  idToken directive;
  if (!ReadSourceToken(directive))
    return false;
  if (directive.linesCrossed > 0) {
    UnreadSourceToken(directive);
    return false;
  }
  if (directive == "if")
    return Directive_if();
  if (directive == "ifdef")
    return DollarDirective_if_def(1);
  if (directive == "ifndef")
    return DollarDirective_if_def(0);
  if (directive == "else")
    return DollarDirective_else();
  if (directive == "elif")
    return DollarDirective_elif();
  if (directive == "endif")
    return DollarDirective_endif();
  if (directive == "break" || skip > 0)
    return true;
  if (directive == "evalint")
    return Directive_evalint();
  if (directive == "evalfloat")
    return Directive_evalfloat();
  if (directive == "template")
    return Directive_define(true);
  UnreadSourceToken(directive);
  return false;
}

idParser::define_t* idParser::DefineFromString(const char* definition) {
  if (definition == nullptr)
    return nullptr;
  idLexer lexer;
  lexer.LoadMemory(definition, idStr::Length(definition), "define");
  idToken token;
  if (!lexer.ReadToken(token))
    return nullptr;
  auto* result = new define_t;
  result->name = token;
  idToken* last = nullptr;
  while (lexer.ReadToken(token))
    AppendToken(result->tokens, last, CloneToken(token));
  return result;
}

bool idParser::PushScript(idLexer* script) {
  if (script == nullptr)
    return false;
  script->next = scriptstack;
  scriptstack = script;
  return true;
}

idParser::define_t* idParser::AddDefine(const char* definition) {
  define_t* parsed = DefineFromString(definition);
  if (parsed == nullptr)
    return nullptr;
  parsed->scope = defineScope;
  AddDefineToHash(parsed);
  return parsed;
}

bool idParser::Directive_include() {
  idToken file;
  if (!ReadLine(file))
    return false;
  AddInclude(file.data);
  return true;
}

bool idParser::ReadDirective() {
  idToken directive;
  if (!ReadSourceToken(directive))
    return false;
  if (directive == "define")
    return Directive_define();
  if (directive == "undef")
    return Directive_undef();
  if (directive == "include")
    return Directive_include();
  if (directive == "ifdef")
    return Directive_if_def(1);
  if (directive == "ifndef")
    return Directive_if_def(0);
  if (directive == "if")
    return Directive_if();
  if (directive == "elif")
    return Directive_elif();
  if (directive == "else")
    return Directive_else();
  if (directive == "endif")
    return Directive_endif();
  if (directive == "line")
    return Directive_line();
  if (directive == "error")
    return Directive_error();
  if (directive == "warning")
    return Directive_warning();
  if (directive == "pragma")
    return Directive_pragma();
  if (directive == "eval")
    return Directive_evalint();
  if (directive == "evalfloat")
    return Directive_evalfloat();
  Warning("unknown directive '%s'", directive.data);
  SkipRestOfLine();
  return false;
}

bool idParser::ReadToken(idToken& token) {
  while (ReadSourceToken(token)) {
    if (token == "#") {
      if (!ReadDirective())
        return false;
      continue;
    }
    if (token == "$") {
      if ((flags & LEXFL_NODOLLARPRECOMPILE) != 0 ||
          !ReadDollarDirective())
        return true;
      continue;
    }
    if (skip != 0)
      continue;
    if (token.type == TT_NAME) {
      if (define_t* definition = FindHashedDefine(token.data)) {
        ExpandDefineIntoSource(token, definition);
        continue;
      }
    }
    return true;
  }
  return false;
}

bool idParser::ExpectTokenString(const char* string, idToken& token) {
  if (!ReadToken(token) || !(token == string)) {
    Error("expected '%s'", string);
    return false;
  }
  return true;
}
bool idParser::ExpectTokenString(const char* string) {
  idToken token;
  return ExpectTokenString(string, token);
}
bool idParser::ExpectTokenType(int type, int subtype, idToken& token) {
  if (!ReadToken(token) || token.type != type ||
      (subtype && (token.subtype & subtype) != subtype)) {
    Error("unexpected token");
    return false;
  }
  return true;
}
bool idParser::ExpectAnyToken(idToken& token) {
  if (ReadToken(token))
    return true;
  Error("expected token");
  return false;
}
bool idParser::CheckTokenType(int type, int subtype, idToken& token) {
  if (!ReadToken(token))
    return false;
  if (token.type == type && (!subtype || (token.subtype & subtype) == subtype))
    return true;
  UnreadToken(token);
  return false;
}
bool idParser::PeekTokenString(const char* string) {
  idToken token;
  if (!ReadToken(token))
    return false;
  const bool result = token == string;
  UnreadToken(token);
  return result;
}
bool idParser::PeekTokenType(int type, int subtype, idToken& token) {
  if (!ReadToken(token))
    return false;
  const bool result = token.type == type && (!subtype || (token.subtype & subtype) == subtype);
  UnreadToken(token);
  return result;
}
bool idParser::SkipUntilString(const char* string) {
  idToken token;
  while (ReadToken(token))
    if (token == string)
      return true;
  return false;
}
bool idParser::SkipRestOfLine() {
  idToken token;
  while (ReadToken(token)) {
    if (token.linesCrossed) {
      UnreadToken(token);
      break;
    }
  }
  return true;
}
bool idParser::SkipBracedSection(bool parseFirstBrace) {
  int depth = 1;
  if (parseFirstBrace && !ExpectTokenString("{"))
    return false;
  idToken token;
  while (depth && ReadToken(token)) {
    if (token == "{")
      ++depth;
    else if (token == "}")
      --depth;
  }
  return depth == 0;
}
idStr& idParser::ParseRestOfLine(idStr& output) {
  output.Empty();
  idToken token;
  while (ReadToken(token)) {
    if (token.linesCrossed) {
      UnreadToken(token);
      break;
    }
    if (!output.IsEmpty())
      output.Append(' ');
    output.Append(token);
  }
  return output;
}
bool idParser::ReadTokenOnLine(idToken& token) {
  if (!ReadToken(token))
    return false;
  if (token.linesCrossed) {
    UnreadToken(token);
    return false;
  }
  return true;
}
int idParser::ParseInt() {
  const bool negative = CheckTokenString("-");
  idToken token;
  if (!ExpectTokenType(TT_NUMBER, TT_INTEGER, token))
    return 0;
  token.NumberValue();
  return negative ? -static_cast<int>(token.intvalue) : static_cast<int>(token.intvalue);
}
bool idParser::ParseBool() {
  idToken token;
  if (!ReadToken(token))
    return false;
  if (token.type == TT_NUMBER) {
    token.NumberValue();
    return token.intvalue != 0;
  }
  return idStr::Icmp(token.data, "true") == 0 || idStr::Icmp(token.data, "yes") == 0;
}
float idParser::ParseFloat(bool* errorFlag) {
  const bool negative = CheckTokenString("-");
  idToken token;
  if (!ExpectTokenType(TT_NUMBER, 0, token)) {
    if (errorFlag)
      *errorFlag = true;
    return 0;
  }
  token.NumberValue();
  if (errorFlag)
    *errorFlag = false;
  return negative ? -token.floatvalue : token.floatvalue;
}
bool idParser::Parse1DMatrix(int count, float* matrix) {
  if (!ExpectTokenString("("))
    return false;
  for (int i = 0; i < count; ++i) {
    bool error = false;
    matrix[i] = ParseFloat(&error);
    if (error)
      return false;
  }
  return ExpectTokenString(")");
}

idParser::define_t* idParser::AddGlobalDefine(const char* definition) {
  define_t* parsed = DefineFromString(definition);
  if (parsed != nullptr) {
    parsed->next = globalDefines;
    globalDefines = parsed;
  }
  return parsed;
}

bool idParser::ParseBracedSectionExact(idStr& output, bool parseFirstBrace) {
  output.Empty();
  int depth = 1;
  if (parseFirstBrace) {
    if (!ExpectTokenString("{"))
      return false;
    output = "{";
  }
  idToken token;
  while (depth && ReadToken(token)) {
    idStr whitespace;
    GetLastWhiteSpace(whitespace);
    output.Append(whitespace);
    if (token.type == TT_STRING) {
      output.Append('"');
      output.Append(token);
      output.Append('"');
    } else
      output.Append(token);
    if (token == "{")
      ++depth;
    else if (token == "}")
      --depth;
  }
  return depth == 0;
}

void idParser::SetupGlobalDefines() {
  if (globalDefines != nullptr)
    return;
  define_t* lineDefine = AddGlobalDefine("__LINE__");
  lineDefine->builtin = 1;
  define_t* fileDefine = AddGlobalDefine("__FILE__");
  fileDefine->builtin = 2;
  define_t* dateDefine = AddGlobalDefine("__DATE__");
  dateDefine->builtin = 3;
  define_t* timeDefine = AddGlobalDefine("__TIME__");
  timeDefine->builtin = 4;
}
