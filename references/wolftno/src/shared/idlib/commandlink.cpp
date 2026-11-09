#include "commandlink.h"

namespace {
idCommandLink* commandLinks = nullptr;
}

idCommandLink* CommandLinks() {
  return commandLinks;
}

idCommandLink::idCommandLink(const char* commandName, commandFunction_t function,
                             const char* description, argCompletion_t completion)
    : next(commandLinks), cmdName_(commandName), function_(function), description_(description),
      argCompletion_(completion) {
  commandLinks = this;
}
