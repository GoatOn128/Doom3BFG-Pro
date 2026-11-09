#include "json.h"

#include <algorithm>

bool mgJsonLexer::ReadToken(idToken& token) {
  if (!idLexer::ReadToken(token))
    return false;
  if (token == "[")
    token.type = JSON_ARRAY_BEGIN;
  else if (token == "]")
    token.type = JSON_ARRAY_END;
  else if (token == "{")
    token.type = JSON_OBJECT_BEGIN;
  else if (token == "}")
    token.type = JSON_OBJECT_END;
  else if (token == ",")
    token.type = JSON_COMMA;
  else if (token == ":")
    token.type = JSON_COLON;
  else if (token.type == TT_STRING) {
    token.type = JSON_VALUE;
    token.subtype = 0x100000;
  } else if (token.type == TT_NUMBER) {
    token.type = JSON_VALUE;
  } else if (token == "true" || token == "false") {
    token.type = JSON_VALUE;
    token.subtype = 0x200000;
  } else if (token == "null") {
    token.type = JSON_VALUE;
    token.subtype = 0x400000;
  }
  return true;
}

mgJsonParser::stateStack_t::stateStack_t()
    : list(staticList), num(0), size(30), granularity(1), memTag(5), listStatic(1), staticList{} {}

bool mgJsonParser::stateStack_t::Push(state_t state) {
  if (num >= size)
    return false;
  list[num++] = state;
  return true;
}

void mgJsonParser::stateStack_t::Pop() {
  if (num > 0)
    --num;
}

mgJsonParser::mgJsonParser(mgJsonLexer* sourceLexer) : lexer(sourceLexer) {
  tempToken.intvalue = 0;
  tempToken.floatvalue = -3.4028235e38f;
  stack.Push(state_t{2, 0, 0});
}

bool mgJsonParser::Fail(const char* message, const idToken* token) {
  if (lexer != nullptr) {
    if (token != nullptr)
      lexer->Error("JSON syntax error: %s (got '%s')", message, token->data);
    else
      lexer->Error("JSON syntax error: %s", message);
  }
  return false;
}

bool mgJsonParser::ReadToken(idToken& token) {
  if (lexer == nullptr || stack.num == 0)
    return false;
  while (lexer->ReadToken(token)) {
    state_t& state = stack.list[stack.num - 1];
    if (state.keyState == 0) {
      if (state.inObject && token.type == JSON_OBJECT_END) {
        stack.Pop();
        return true;
      }
      if (!state.inObject || token.type != JSON_VALUE || token.subtype != 0x100000)
        return Fail("expected object key", &token);
      token.type = JSON_KEY;
      state.keyState = 1;
      return true;
    }
    if (state.keyState == 1) {
      if (token.type != JSON_COLON)
        return Fail("expected colon", &token);
      state.keyState = 2;
      continue;
    }
    if (state.keyState == 2) {
      if (state.inArray && token.type == JSON_ARRAY_END) {
        stack.Pop();
        return true;
      }
      state.keyState = 3;
      if (token.type == JSON_ARRAY_BEGIN) {
        if (!stack.Push(state_t{2, 1, 0}))
          return Fail("JSON nesting is too deep");
        return true;
      }
      if (token.type == JSON_OBJECT_BEGIN) {
        if (!stack.Push(state_t{0, 0, 1}))
          return Fail("JSON nesting is too deep");
        return true;
      }
      if (token.type == JSON_VALUE)
        return true;
      return Fail("expected value", &token);
    }
    if (state.keyState == 3) {
      const bool isArray = state.inArray != 0;
      const bool isObject = state.inObject != 0;
      if (token.type == JSON_COMMA) {
        state.keyState = isObject ? 0u : 2u;
        continue;
      }
      if ((isArray && token.type == JSON_ARRAY_END) ||
          (isObject && token.type == JSON_OBJECT_END)) {
        stack.Pop();
        return true;
      }
      if (!isArray && !isObject)
        return Fail("trailing data after root value", &token);
      return Fail("expected comma or container end", &token);
    }
  }
  return false;
}

bool mgJsonParser::CheckTokenType(idToken& token, int type, int subtype) {
  const int savedCount = stack.num;
  state_t savedStates[30];
  std::copy_n(stack.list, stack.num, savedStates);
  if (ReadToken(token) && token.type == type &&
      (subtype == 0 || (token.subtype & subtype) == subtype))
    return true;
  lexer->UnreadToken();
  stack.num = savedCount;
  std::copy_n(savedStates, savedCount, stack.list);
  return false;
}

bool mgJsonParser::ExpectTokenType(idToken& token, int type, int subtype) {
  if (!ReadToken(token) || token.type != type ||
      (subtype != 0 && (token.subtype & subtype) != subtype))
    return Fail("couldn't read expected token", &token);
  return true;
}
