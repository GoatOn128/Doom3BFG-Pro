#include "cmdargs.h"

#include <cctype>
#include <cstring>
#include <stdexcept>

namespace {
thread_local char commandArguments[4096]{};
}

idCmdArgs::idCmdArgs() : argc(0), argv{}, tokenized{} {}
idCmdArgs::idCmdArgs(const idCmdArgs& other) {
  *this = other;
}

idCmdArgs& idCmdArgs::operator=(const idCmdArgs& other) {
  if (this == &other)
    return *this;
  argc = other.argc;
  std::memmove(tokenized, other.tokenized, sizeof(tokenized));
  for (int i = 0; i < argc; ++i)
    argv[i] = tokenized + (other.argv[i] - other.tokenized);
  return *this;
}

void idCmdArgs::AppendArg(const char* text) {
  if (argc >= 256)
    throw std::length_error("Too many command line arguments");
  char* destination = argc == 0 ? tokenized : argv[argc - 1] + idStr::Length(argv[argc - 1]) + 1;
  const int remaining = static_cast<int>(tokenized + sizeof(tokenized) - destination);
  if (remaining <= 0 || idStr::Length(text) + 1 > remaining)
    throw std::length_error("Command line arguments exceed token buffer");
  argv[argc++] = destination;
  idStr::Copynz(destination, text, remaining);
}

const char* idCmdArgs::Args(int start, int end, bool quoteArgs, bool escapeArgs) const {
  idStr result;
  start = std::max(0, start);
  if (end < 0 || end >= argc)
    end = argc - 1;
  if (escapeArgs)
    result.Append('"');
  for (int i = start; i <= end; ++i) {
    if (i > start)
      result.Append(escapeArgs ? "\\ " : " ");
    if (quoteArgs && !escapeArgs)
      result.Append('"');
    if (escapeArgs) {
      for (const char* p = argv[i]; *p; ++p) {
        if (*p == '\\' || *p == '"')
          result.Append('\\');
        result.Append(*p);
      }
    } else
      result.Append(argv[i]);
    if (quoteArgs && !escapeArgs)
      result.Append('"');
  }
  if (escapeArgs)
    result.Append('"');
  idStr::Copynz(commandArguments, result.data, sizeof(commandArguments));
  return commandArguments;
}

void idCmdArgs::TokenizeString(const char* text, bool keepAsStrings) {
  argc = 0;
  if (!text)
    return;
  const char* cursor = text;
  while (*cursor) {
    while (std::isspace(static_cast<unsigned char>(*cursor)))
      ++cursor;
    if (!*cursor)
      break;
    idStr token;
    const char quote = (*cursor == '"' || *cursor == '\'') ? *cursor++ : 0;
    while (*cursor &&
           (quote ? *cursor != quote : !std::isspace(static_cast<unsigned char>(*cursor)))) {
      if (*cursor == '\\' && cursor[1]) {
        char value;
        const char* escaped = cursor;
        if (idStr::EmitEscapeChar(&escaped, &value)) {
          token.Append(value);
          cursor = escaped;
          continue;
        }
      }
      token.Append(*cursor++);
    }
    if (quote && *cursor == quote)
      ++cursor;
    if (!keepAsStrings && token == "-" && *cursor &&
        !std::isspace(static_cast<unsigned char>(*cursor)))
      continue;
    AppendArg(token.data);
  }
}
