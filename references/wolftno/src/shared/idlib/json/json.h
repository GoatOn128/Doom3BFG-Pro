#pragma once

#include "shared/idlib/text/lexer.h"

#include <cstdint>

enum jsonTokenType_t {
  JSON_ARRAY_BEGIN = 10,
  JSON_ARRAY_END = 11,
  JSON_OBJECT_BEGIN = 12,
  JSON_OBJECT_END = 13,
  JSON_KEY = 14,
  JSON_VALUE = 15,
  JSON_COMMA = 16,
  JSON_COLON = 17,
};

class mgJsonLexer : public idLexer {
public:
  using idLexer::idLexer;
  bool ReadToken(idToken& token);
};

class mgJsonParser {
public:
  struct state_t {
    std::uint32_t keyState : 3;
    std::uint32_t inArray : 1;
    std::uint32_t inObject : 1;
  };

  explicit mgJsonParser(mgJsonLexer* sourceLexer);
  bool ReadToken(idToken& token);
  bool CheckTokenType(idToken& token, int type, int subtype = 0);
  bool ExpectTokenType(idToken& token, int type, int subtype = 0);

private:
  struct stateStack_t {
    state_t* list;
    int num;
    int size;
    short granularity;
    unsigned char memTag;
    unsigned char listStatic;
    state_t staticList[30];

    stateStack_t();
    bool Push(state_t state);
    void Pop();
  };

  bool Fail(const char* message, const idToken* token = nullptr);
  mgJsonLexer* lexer;
  idToken tempToken;
  stateStack_t stack;
};

static_assert(sizeof(mgJsonLexer) == sizeof(idLexer), "mgJsonLexer ABI drift");
static_assert(sizeof(mgJsonParser::state_t) == 4, "JSON state ABI drift");
static_assert(sizeof(mgJsonParser) == 248, "mgJsonParser ABI drift");
