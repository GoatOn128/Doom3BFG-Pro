#include "autocomplete.h"

void idAutoComplete::Append(const idStr& suggestion, int completingArgument) {
  int argument = completingArgument < 0 ? args.argc - 1 : completingArgument;
  if (argument >= 0 && argument < args.argc && args.argv[argument][0] != '\0') {
    const int prefixLength = idStr::Length(args.argv[argument]);
    if (idStr::Icmpn(suggestion.data, args.argv[argument], prefixLength) != 0)
      return;
  }
  if (args.argc == 1 && argument == 0) {
    suggestions.Append(suggestion);
    return;
  }
  idStr completed;
  for (int i = 0; i < argument && i < args.argc; ++i) {
    completed.Append(args.argv[i]);
    completed.Append(' ');
  }
  completed.Append(suggestion);
  suggestions.Append(completed);
}
