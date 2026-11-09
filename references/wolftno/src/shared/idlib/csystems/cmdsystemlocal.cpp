#include "shared/idlib/csystems/cmdsystemlocal.h"

#include "shared/idlib/csystems/cvarsystemlocal.h"
#include "shared/idlib/lib.h"

#include <algorithm>
#include <filesystem>
#include <fstream>
#include <string>
#include <vector>

idCmdSystemLocal cmdSystemLocal;
idCmdSystem* cmdSystem = &cmdSystemLocal;

void idCmdSystemLocal::Init() {
  Shutdown();
  for (idCommandLink* link = CommandLinks(); link; link = link->next)
    AddCommand(link->cmdName_, link->function_, link->description_, link->argCompletion_);
  AddCommand("echo", Echo_f, "prints text");
  AddCommand("wait", Wait_f, "delays command execution");
  AddCommand("exec", Exec_f, "executes a command file");
  AddCommand("vstr", Vstr_f, "executes the contents of a cvar");
  AddCommand("parse", Parse_f, "prints tokenized arguments");
  AddCommand("listCmds", List_f, "lists commands");
}
void idCmdSystemLocal::Shutdown() {
  while (commands) {
    commandDef_s* next = commands->next;
    delete[] commands->name;
    delete[] commands->description;
    delete commands;
    commands = next;
  }
  textBuffer.Clear();
  wait = 0;
}
void idCmdSystemLocal::AddCommand(const char* name, commandFunction_t function,
                                  const char* description, argCompletion_t completion) {
  if (!name || !*name)
    return;
  for (commandDef_s* it = commands; it; it = it->next)
    if (idStr::Icmp(it->name, name) == 0) {
      it->function = function;
      it->argCompletion = completion;
      return;
    }
  auto copy = [](const char* s) {
    const int n = idStr::Length(s ? s : "");
    char* p = new char[n + 1];
    idStr::Copynz(p, s ? s : "", n + 1);
    return p;
  };
  auto* definition = new commandDef_s;
  definition->name = copy(name);
  definition->description = copy(description);
  definition->function = function;
  definition->argCompletion = completion;
  definition->next = commands;
  commands = definition;
}
const char* idCmdSystemLocal::GetCommandDescription(const char* name) const {
  for (commandDef_s* it = commands; it; it = it->next)
    if (idStr::Icmp(it->name, name) == 0)
      return it->description;
  return "NOT FOUND";
}
bool idCmdSystemLocal::CommandExists(const char* name, bool substring) const {
  for (commandDef_s* it = commands; it; it = it->next)
    if (substring ? idStr::Find(name, it->name, false) != -1 : idStr::Icmp(name, it->name) == 0)
      return true;
  return false;
}
void idCmdSystemLocal::FindCommands(const char* prefix, idList<idStr>& result) const {
  for (commandDef_s* it = commands; it; it = it->next)
    if (!prefix || !*prefix || idStr::Icmpn(it->name, prefix, idStr::Length(prefix)) == 0)
      result.Append(idStr(it->name));
}
void idCmdSystemLocal::CommandCompletion(idAutoComplete* completion) const {
  if (!completion)
    return;
  for (commandDef_s* it = commands; it; it = it->next)
    completion->Append(idStr(it->name), 0);
}
void idCmdSystemLocal::AppendCommandText(const char* text) {
  if (text)
    textBuffer.Append(text);
}
void idCmdSystemLocal::InsertCommandText(const char* text) {
  if (!text)
    return;
  idStr next(text);
  next.Append(textBuffer);
  textBuffer = next;
}
void idCmdSystemLocal::ExecuteCommandText(const char* text) {
  idCmdArgs args;
  args.TokenizeString(text);
  ExecuteTokenizedString(args);
}
void idCmdSystemLocal::ExecuteTokenizedString(const idCmdArgs& args) {
  if (args.argc == 0)
    return;
  for (commandDef_s* it = commands; it; it = it->next)
    if (idStr::Icmp(it->name, args.argv[0]) == 0) {
      if (it->function)
        it->function(&args);
      return;
    }
  if (cvarSystem && cvarSystem->Command(args))
    return;
  idLib::Warning("Unknown command '%s'", args.argv[0]);
}
void idCmdSystemLocal::ExecuteCommandBuffer() {
  if (wait > 0) {
    --wait;
    return;
  }
  std::string text = textBuffer.data ? textBuffer.data : "";
  textBuffer.Clear();
  std::string command;
  bool quoted = false;
  for (std::size_t i = 0; i < text.size(); ++i) {
    const char c = text[i];
    if (c == '\"')
      quoted = !quoted;
    if (!quoted && (c == '\n' || c == ';')) {
      ExecuteCommandText(command.c_str());
      command.clear();
      if (wait > 0) {
        InsertCommandText(text.substr(i + 1).c_str());
        return;
      }
    } else
      command += c;
  }
  if (!command.empty())
    ExecuteCommandText(command.c_str());
}
void idCmdSystemLocal::ArgCompletion_FolderExtension(idAutoComplete* completion, const char* folder,
                                                     const char* extension, bool stripFolder) {
  if (!completion)
    return;
  std::error_code error;
  const std::filesystem::path root = folder && *folder ? folder : ".";
  for (const auto& entry : std::filesystem::directory_iterator(root, error)) {
    if (error)
      break;
    if (extension && *extension && entry.path().extension() != std::string(".") + extension)
      continue;
    const std::string value =
        stripFolder ? entry.path().filename().string() : entry.path().string();
    completion->Append(idStr(value.c_str()));
  }
}
void idCmdSystem::ArgCompletion_ConfigName(idAutoComplete* c) {
  cmdSystem->ArgCompletion_FolderExtension(c, ".", "cfg", true);
}
void idCmdSystem::ArgCompletion_MapName(idAutoComplete* c) {
  cmdSystem->ArgCompletion_FolderExtension(c, ".", "map", true);
}
void idCmdSystem::ArgCompletion_Boolean(idAutoComplete* completion) {
  idStr suggestion("0");
  completion->Append(suggestion, -1);
  suggestion = "1";
  completion->Append(suggestion, -1);
}
void idCmdSystemLocal::Echo_f(const idCmdArgs* a) {
  if (a)
    idLib::Printf("%s\n", a->Args(1));
}
void idCmdSystemLocal::Wait_f(const idCmdArgs* a) {
  cmdSystem->SetWait(a && a->argc > 1 ? std::max(1, std::atoi(a->argv[1])) : 1);
}
void idCmdSystemLocal::Vstr_f(const idCmdArgs* a) {
  if (a && a->argc > 1)
    cmdSystem->InsertCommandText(cvarSystem->GetCVarString(a->argv[1], ""));
}
void idCmdSystemLocal::Parse_f(const idCmdArgs* a) {
  if (a)
    for (int i = 0; i < a->argc; ++i)
      idLib::Printf("%d: %s\n", i, a->argv[i]);
}
void idCmdSystemLocal::Exec_f(const idCmdArgs* a) {
  if (!a || a->argc < 2)
    return;
  std::ifstream file(a->argv[1], std::ios::binary);
  if (!file)
    return;
  std::string text((std::istreambuf_iterator<char>(file)), {});
  cmdSystem->InsertCommandText(text.c_str());
}
void idCmdSystemLocal::List_f(const idCmdArgs*) {
  idList<idStr> names;
  cmdSystemLocal.FindCommands("", names);
  std::vector<std::string> sorted;
  for (int i = 0; i < names.Num(); ++i)
    sorted.emplace_back(names[i].data);
  std::sort(sorted.begin(), sorted.end());
  for (const auto& name : sorted)
    idLib::Printf("%s\n", name.c_str());
}
