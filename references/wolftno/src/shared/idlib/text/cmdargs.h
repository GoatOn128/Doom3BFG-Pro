#pragma once

#include "str.h"

class idCmdArgs {
public:
  int argc;
  char* argv[256];
  char tokenized[4096];

  idCmdArgs();
  idCmdArgs(const idCmdArgs& other);
  idCmdArgs& operator=(const idCmdArgs& other);
  const char* Argv(int index) const {
    return index < 0 || index >= argc ? "" : argv[index];
  }
  void AppendArg(const char* text);
  const char* Args(int start = 0, int end = -1, bool quoteArgs = false,
                   bool escapeArgs = false) const;
  void TokenizeString(const char* text, bool keepAsStrings = false);
};

static_assert(sizeof(idCmdArgs) == 6152, "idCmdArgs ABI drift");
