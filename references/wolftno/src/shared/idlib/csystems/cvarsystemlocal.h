#pragma once

#include "shared/idlib/containers/hashindex.h"
#include "shared/idlib/containers/list.h"
#include "shared/idlib/commandlink.h"
#include "shared/idlib/csystems/autocomplete.h"
#include "shared/idlib/filesystem/file.h"
#include "shared/idlib/text/str.h"

enum cvarFlags_t {
  CVAR_ALL = 0,
  CVAR_BOOL = 1 << 0,
  CVAR_INTEGER = 1 << 1,
  CVAR_FLOAT = 1 << 2,
  CVAR_CHEAT = 1 << 3,
  CVAR_NOCHEAT = 1 << 4,
  CVAR_SYSTEM = 1 << 5,
  CVAR_RENDERER = 1 << 6,
  CVAR_SOUND = 1 << 7,
  CVAR_GUI = 1 << 8,
  CVAR_GAME = 1 << 9,
  CVAR_TOOL = 1 << 10,
  CVAR_USERINFO = 1 << 11,
  CVAR_SERVERINFO = 1 << 12,
  CVAR_NETWORKSYNC = 1 << 13,
  CVAR_INIT = 1 << 14,
  CVAR_ROM = 1 << 15,
  CVAR_ARCHIVE = 1 << 16,
  CVAR_MODIFIED = 1 << 17,
  CVAR_MODIFIED2 = 1 << 18
};

class idCVar {
public:
  struct cvarCallback_t {
    void (*callback)(idCVar&);
    cvarCallback_t* next;
  };
  idCVar();
#if defined(WOLFTNO_RETAIL_CVAR_INLINE_FREEDATA_ABI)
  ~idCVar() noexcept(false) { valueString.FreeData(); }
#else
  ~idCVar() = default;
#endif
  idCVar(const char* name, const char* value, int flags,
         const char* description, argCompletion_t completion = nullptr);
  // Retail overload used by gamelib CVars whose completion callback accepts
  // idAutoComplete by reference.  Its distinct MSVC decoration is part of the
  // executable ABI even when the supplied callback is null.
  idCVar(const char* name, const char* value, int flags,
         const char* description, argCompletionRef_t completion);
  // Preserve untyped nullptr call sites while the two retail callback ABIs
  // coexist.  Decompiled compilands that require one exact decoration cast to
  // that callback type explicitly.
  idCVar(const char* name, const char* value, int flags,
         const char* description, decltype(nullptr))
      : idCVar(name, value, flags, description,
               static_cast<argCompletion_t>(nullptr)) {}
  idCVar(const char* name, const char* value, int flags,
         const char* description, float minimum, float maximum,
         argCompletion_t completion = nullptr);
  // Retail gamelib overload whose callback takes idAutoComplete by reference.
  // Keep the distinct decoration available to recovered compilands.
  idCVar(const char* name, const char* value, int flags,
         const char* description, float minimum, float maximum,
         argCompletionRef_t completion);
  idCVar(const char* name, const char* value, int flags,
         const char* description, const char** values,
         argCompletion_t completion);
  idCVar(const char* name, const char* value, int flags,
         const char* description, const char** values,
         argCompletionRef_t completion);
  idCVar(const char* name, const char* value, int flags,
         const char* description, float minimum, float maximum,
         const char** values, argCompletionRef_t completion);
  void Init(const char* name, const char* value, int flags, const char* description,
            float minimum = 0.0f, float maximum = 0.0f, const char** values = nullptr,
            argCompletion_t completion = nullptr);
  void UpdateValue();
  void Reset();
  void Set(const char* value, bool force = false);
  void SetString(const char*, bool force = false);
  void SetBool(bool, bool force = false);
  void SetInteger(int, bool force = false);
  void SetFloat(float, bool force = false);
  const char* GetString() const { return valueString.data; }
  bool GetBool() const { return valueInteger != 0; }
  int GetInteger() const { return valueInteger; }
  float GetFloat() const { return valueFloat; }
  static void RegisterStaticVars();
  static idCVar* StaticVars();
#if defined(WOLFTNO_RETAIL_CVAR_INLINE_FREEDATA_ABI)
  union {
    idStr valueString;
  };
#else
  idStr valueString;
#endif
  int valueInteger{};
  float valueFloat{};
  const char* name{};
  const char* resetString{};
  const char* description{};
  int flags{};
  float valueMin{}, valueMax{};
  const char** valueStrings{};
  argCompletion_t valueCompletion{};
  cvarCallback_t* onChange{};
  idCVar* next{};
};

class idScopeCVar {
public:
  idScopeCVar(idCVar* cvar, const char* newValue);
  idScopeCVar(idCVar* cvar, bool newValue);
  idScopeCVar(idCVar* cvar, int newValue);
  idScopeCVar(idCVar* cvar, float newValue);
  ~idScopeCVar();

  idScopeCVar(const idScopeCVar&) = delete;
  idScopeCVar& operator=(const idScopeCVar&) = delete;

  idCVar* cvar{};
  int cvarType{};
  int oldInt{};
  float oldFloat{};
  bool oldBool{};
  idStr oldStr;
};

static_assert(sizeof(idScopeCVar) == 0x40, "idScopeCVar ABI drift");
static_assert(offsetof(idScopeCVar, cvar) == 0x00,
              "idScopeCVar::cvar ABI drift");
static_assert(offsetof(idScopeCVar, cvarType) == 0x08,
              "idScopeCVar::cvarType ABI drift");
static_assert(offsetof(idScopeCVar, oldInt) == 0x0C,
              "idScopeCVar::oldInt ABI drift");
static_assert(offsetof(idScopeCVar, oldFloat) == 0x10,
              "idScopeCVar::oldFloat ABI drift");
static_assert(offsetof(idScopeCVar, oldBool) == 0x14,
              "idScopeCVar::oldBool ABI drift");
static_assert(offsetof(idScopeCVar, oldStr) == 0x18,
              "idScopeCVar::oldStr ABI drift");

class idCVarSystem {
public:
  virtual ~idCVarSystem() = default;
  virtual int NumCVars() const = 0;
  virtual const idCVar* FindByIndex(int) const = 0;
  virtual void ResetCVarByIndex(int) = 0;
  virtual idCVar* Find(const char*) const = 0;
  virtual bool CvarExists(const char*, bool = false) const = 0;
  virtual void FindCvarsByPrefix(const char*, idList<idCVar*>&) const = 0;
  virtual void SetCVarString(const char*, const char*, int = 0) = 0;
  virtual void SetCVarBool(const char*, bool, int = 0) = 0;
  virtual void SetCVarInteger(const char*, int, int = 0) = 0;
  virtual void SetCVarFloat(const char*, float, int = 0) = 0;
  virtual const char* GetCVarString(const char* name, const char* fallback = "") const = 0;
  virtual bool GetCVarBool(const char*, bool = false) const = 0;
  virtual int GetCVarInteger(const char*, int = 0) const = 0;
  virtual float GetCVarFloat(const char*, float = 0) const = 0;
  virtual bool Command(const idCmdArgs&) = 0;
  virtual void CommandCompletion(idAutoComplete*) const = 0;
  virtual void SetModifiedFlags(int) = 0;
  virtual int GetModifiedFlags() const = 0;
  virtual void ClearModifiedFlags(int) = 0;
  virtual void ResetFlaggedVariables(int) = 0;
  virtual void WriteFlaggedVariables(int, idFile*) const = 0;
  virtual void ReportModifiedCVars() const = 0;
  virtual void ClearModifiedCVars() = 0;
  virtual void RegisterStaticVars() = 0;
  virtual void ListCVars(const idCmdArgs&) const = 0;
};

class idCVarSystemLocal final : public idCVarSystem {
public:
  idCVarSystemLocal();
  ~idCVarSystemLocal() override = default;
  int NumCVars() const override { return cvars.Num(); }
  const idCVar* FindByIndex(int) const override;
  void ResetCVarByIndex(int) override;
  idCVar* Find(const char*) const override;
  bool CvarExists(const char*, bool = false) const override;
  void FindCvarsByPrefix(const char*, idList<idCVar*>&) const override;
  void SetCVarString(const char*, const char*, int = 0) override;
  void SetCVarBool(const char*, bool, int = 0) override;
  void SetCVarInteger(const char*, int, int = 0) override;
  void SetCVarFloat(const char*, float, int = 0) override;
  const char* GetCVarString(const char* name, const char* fallback = "") const override;
  bool GetCVarBool(const char*, bool = false) const override;
  int GetCVarInteger(const char*, int = 0) const override;
  float GetCVarFloat(const char*, float = 0) const override;
  bool Command(const idCmdArgs&) override;
  void CommandCompletion(idAutoComplete*) const override;
  void SetModifiedFlags(int flags) override { modifiedFlags |= flags; }
  int GetModifiedFlags() const override { return modifiedFlags; }
  void ClearModifiedFlags(int flags) override { modifiedFlags &= ~flags; }
  void ResetFlaggedVariables(int) override;
  void WriteFlaggedVariables(int, idFile*) const override;
  void ReportModifiedCVars() const override;
  void ClearModifiedCVars() override;
  void RegisterStaticVars() override;
  void ListCVars(const idCmdArgs&) const override;
  void FindCvarsByPrefix(const char*, idList<idCVar*>&);
  idList<idCVar*> cvars;
  idHashIndex cvarHash;
  int modifiedFlags{};

private:
  void Add(idCVar*);
};

extern idCVarSystemLocal cvarSystemLocal;
extern idCVarSystem* cvarSystem;
