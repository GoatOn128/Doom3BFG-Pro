#pragma once

#include "shared/idlib/containers/list.h"
#include "shared/idlib/text/cmdargs.h"

#include <cstddef>

class idAutoComplete {
public:
  idAutoComplete();
  ~idAutoComplete();
  int matchLength;
  int currentIndex;
  idCmdArgs args;
  idList<idStr> suggestions;

  void Clear();
  void Append(const idStr& suggestion, int completingArgument = -1);
  int NumSuggestions() const { return suggestions.Num(); }
  const idStr& GetSuggestion(int index) const { return suggestions[index]; }
  int NumArgs() const { return args.argc; }
  const char* GetArg(int index) const { return args.Argv(index); }
  idCmdArgs& GetArgs() { return args; }
};

static_assert(offsetof(idAutoComplete,matchLength)==0x0,
              "idAutoComplete match length ABI drift");
static_assert(offsetof(idAutoComplete,currentIndex)==0x4,
              "idAutoComplete current index ABI drift");
static_assert(offsetof(idAutoComplete,args)==0x8,
              "idAutoComplete args ABI drift");
static_assert(offsetof(idAutoComplete,suggestions)==0x1810,
              "idAutoComplete suggestions ABI drift");
static_assert(sizeof(idAutoComplete)==0x1828,
              "idAutoComplete size ABI drift");
