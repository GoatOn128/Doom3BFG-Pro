#pragma once

class idCmdArgs;
class idAutoComplete;

using commandFunction_t = void (*)(const idCmdArgs*);
using commandFunctionRef_t = void (*)(const idCmdArgs&);
using argCompletion_t = void (*)(idAutoComplete*);
using argCompletionRef_t = void (*)(idAutoComplete&);

struct idCommandLink {
  idCommandLink* next;
  const char* cmdName_;
  commandFunction_t function_;
  const char* description_;
  argCompletion_t argCompletion_;

  idCommandLink(const char* commandName, commandFunction_t function, const char* description,
                argCompletion_t completion);
  idCommandLink(const char* commandName, commandFunctionRef_t function,
                const char* description, argCompletion_t completion)
      : idCommandLink(commandName,
                      reinterpret_cast<commandFunction_t>(function),
                      description, completion) {}
};

idCommandLink* CommandLinks();
static_assert(sizeof(idCommandLink) == 40, "idCommandLink ABI drift");
