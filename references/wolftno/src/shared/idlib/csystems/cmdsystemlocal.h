#pragma once

#include "shared/idlib/commandlink.h"
#include "shared/idlib/containers/list.h"
#include "shared/idlib/csystems/autocomplete.h"
#include "shared/idlib/text/str.h"

struct commandDef_s {
  commandDef_s* next{};
  const char* name{};
  commandFunction_t function{};
  argCompletion_t argCompletion{};
  const char* description{};
};

class idCmdSystem {
public:
  virtual ~idCmdSystem() = default;
  virtual void Init() = 0;
  virtual void Shutdown() = 0;
  virtual void AddCommand(const char*, commandFunction_t, const char*,
                          argCompletion_t = nullptr) = 0;
  void AddCommand(const char* name, commandFunctionRef_t function,
                  const char* description,
                  argCompletion_t completion = nullptr) {
    AddCommand(name, reinterpret_cast<commandFunction_t>(function),
               description, completion);
  }
  virtual const char* GetCommandDescription(const char*) const = 0;
  virtual bool CommandExists(const char*, bool searchForString = false) const = 0;
  virtual void FindCommands(const char*, idList<idStr>&) const = 0;
  virtual void CommandCompletion(idAutoComplete*) const = 0;
  virtual void ExecuteCommandText(const char*) = 0;
  virtual void AppendCommandText(const char*) = 0;
  virtual void ExecuteCommandBuffer() = 0;
  virtual void SetWait(int frames) = 0;
  virtual commandDef_s* GetCommands() const = 0;
  virtual void ExecuteTokenizedString(const idCmdArgs&) = 0;
  virtual void InsertCommandText(const char*) = 0;
  virtual void ArgCompletion_FolderExtension(idAutoComplete*, const char*, const char*, bool) = 0;
  static void ArgCompletion_ConfigName(idAutoComplete*);
  static void ArgCompletion_MapName(idAutoComplete*);
  static void ArgCompletion_Boolean(idAutoComplete*);
  static void ArgCompletion_FileName(idAutoComplete*);
  static void ArgCompletion_SaveGame(idAutoComplete*);
  static void ArgCompletion_EventName(idAutoComplete*);
  static void ArgCompletion_DemoFile(idAutoComplete*);
  template <int Minimum, int Maximum>
  static void ArgCompletion_Integer(idAutoComplete* completion) {
    for (int value = Minimum; value <= Maximum; ++value) {
      idStr suggestion(va("%d", value));
      completion->Append(suggestion, -1);
    }
  }
  template <const char** Strings>
  static void ArgCompletion_String(idAutoComplete* completion) {
    for (int index = 0; Strings[index] != nullptr; ++index) {
      idStr suggestion(Strings[index]);
      completion->Append(suggestion, -1);
    }
  }
  template <const char** Strings>
  static void ArgCompletion_String(idAutoComplete& completion) {
    for (int index = 0; Strings[index] != nullptr; ++index) {
      idStr suggestion(Strings[index]);
      completion.Append(suggestion, -1);
    }
  }
};

class idCmdSystemLocal final : public idCmdSystem {
public:
  idCmdSystemLocal() = default;
  ~idCmdSystemLocal() override { Shutdown(); }
  void Init() override;
  void Shutdown() override;
  void AddCommand(const char*, commandFunction_t, const char*, argCompletion_t = nullptr) override;
  const char* GetCommandDescription(const char*) const override;
  bool CommandExists(const char*, bool = false) const override;
  void FindCommands(const char*, idList<idStr>&) const override;
  void CommandCompletion(idAutoComplete*) const override;
  void ExecuteCommandText(const char*) override;
  void AppendCommandText(const char*) override;
  void ExecuteCommandBuffer() override;
  void SetWait(int frames) override { wait = frames; }
  commandDef_s* GetCommands() const override { return commands; }
  void ExecuteTokenizedString(const idCmdArgs&) override;
  void InsertCommandText(const char*) override;
  void ArgCompletion_FolderExtension(idAutoComplete*, const char*, const char*, bool) override;
  static void Vstr_f(const idCmdArgs*);
  static void Echo_f(const idCmdArgs*);
  static void Wait_f(const idCmdArgs*);
  static void Parse_f(const idCmdArgs*);
  static void Exec_f(const idCmdArgs*);
  static void List_f(const idCmdArgs*);
  commandDef_s* commands{};
  int wait{};
  idStr textBuffer;
};

extern idCmdSystemLocal cmdSystemLocal;
extern idCmdSystem* cmdSystem;
