#include "warningsfilter.h"

#include "shared/idlib/json/json.h"

#include <regex>
#include <string>

namespace {
using NativeRegex = std::regex;

void FreeRegex(warningRegex_t& regex) {
  delete static_cast<NativeRegex*>(regex.value);
  regex.value = nullptr;
  regex.subExpressionCount = 0;
}
} // namespace

mgWarningsFilter warningsFilter;

idStr EscapeRegexString(const idStr& text) {
  idStr result;
  result.EnsureAlloced(text.Length() + text.CountChar('^') + 1, false, false);
  for (int index = 0; index < text.Length(); ++index) {
    if (text[index] == '^')
      result.Append('\\');
    result.Append(text[index]);
  }
  return result;
}

warningfilter_t::~warningfilter_t() {
  FreeRegex(reg);
}

bool mgWarningsFilter::Handle(const char* text) const {
  if (!init || text == nullptr)
    return false;
  idStr cleaned(text);
  cleaned.RemoveColors();
  for (int index = 0; index < filters.Num(); ++index) {
    const auto* expression = static_cast<const NativeRegex*>(filters[index].reg.value);
    if (expression != nullptr && std::regex_search(cleaned.data, *expression))
      return true;
  }
  return false;
}

void mgWarningsFilter::ParseWarningsFile(const char* file) {
  mgJsonLexer lexer;
  if (!lexer.LoadFile(file, false))
    return;
  mgJsonParser parser(&lexer);
  idToken token;
  if (!parser.ExpectTokenType(token, JSON_OBJECT_BEGIN))
    return;
  while (parser.CheckTokenType(token, JSON_KEY)) {
    if (!parser.ExpectTokenType(token, JSON_ARRAY_BEGIN))
      return;
    while (parser.CheckTokenType(token, JSON_OBJECT_BEGIN)) {
      warningfilter_t filter;
      while (parser.CheckTokenType(token, JSON_KEY)) {
        const idStr key(token);
        if (!parser.ExpectTokenType(token, JSON_VALUE, 0x100000))
          return;
        if (key == "owner")
          filter.owner = token;
        else if (key == "pattern")
          filter.pattern = token;
        else
          lexer.Warning("unknown warning filter key '%s'", key.data);
      }
      if (!parser.ExpectTokenType(token, JSON_OBJECT_END))
        return;
      if (!filter.owner.IsEmpty())
        filters.Append(filter);
    }
    if (!parser.ExpectTokenType(token, JSON_ARRAY_END))
      return;
  }
  parser.ExpectTokenType(token, JSON_OBJECT_END);
}

mgWarningsFilter::~mgWarningsFilter() = default;

void mgWarningsFilter::Init(const char* file, bool enabled) {
  if (!enabled)
    return;
  ParseWarningsFile(file);
  for (int index = 0; index < filters.Num(); ++index) {
    FreeRegex(filters[index].reg);
    try {
      auto* expression = new NativeRegex(filters[index].pattern.data, std::regex::extended);
      filters[index].reg.value = expression;
      filters[index].reg.subExpressionCount = expression->mark_count();
    } catch (const std::regex_error&) {
      filters[index].reg.value = nullptr;
    }
  }
  init = true;
}
