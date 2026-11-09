#pragma once

#include "engine/guis/swf/swf_scriptvar.h"
#include "shared/idlib/containers/list.h"
#include "shared/idlib/containers/staticlist.h"
#include "shared/idlib/text/atomicstring.h"
#include "shared/idlib/sys/sys_alloc.h"

class idSWFScriptNativeVariable {
public:
  virtual bool IsReadOnly() { return false; }
  virtual void Set(idSWFScriptObject* object, const idSWFScriptVar& value) = 0;
  virtual idSWFScriptVar Get(idSWFScriptObject* object) = 0;
};

class idSWFScriptObject {
public:
  static const char* const CLASS;

  enum swfNamedVarFlags_t {
    SWF_VAR_FLAG_NONE = 0,
    SWF_VAR_FLAG_READONLY = 2,
    SWF_VAR_FLAG_DONTENUM = 4,
    SWF_VAR_FLAG_DONTDELETE = 8,
  };

  struct swfNamedVar_t {
    swfNamedVar_t()
        : hashNext(-1), name(), value(), native(nullptr), flags(SWF_VAR_FLAG_NONE) {}
    ~swfNamedVar_t();
    swfNamedVar_t& operator=(const swfNamedVar_t& other);

    int hashNext;
    idAtomicString name;
    idSWFScriptVar value;
    idSWFScriptNativeVariable* native;
    int flags;
  };

  idSWFScriptObject();
  virtual const char* Class() const;
  virtual bool DerivedFrom(const char* className) const;
  virtual ~idSWFScriptObject();
  virtual void Clear();
  virtual idSWFScriptVar Get(const char* name);
  virtual void Set(const char* name, const idSWFScriptVar& value);
  virtual bool Delete(const char* name);
  virtual void EnumVariables(idList<idSWFScriptVar>& names);
  virtual void PrintToConsole();

  void AddRef() { ++refCount; }
  void Release() {
    if (refCount-- == 1)
      delete this;
  }
  void SetNative(const char* name, idSWFScriptNativeVariable* native);
  bool HasProperty(const char* name);
  bool HasValidProperty(const char* name);
  idSWFScriptVar DefaultValue(swfTypeHint_t typeHint);
  idSWFScriptObject* GetPrototype() const { return prototype; }
  void SetPrototype(idSWFScriptObject* value) {
    if (prototype != nullptr)
      prototype->Release();
    prototype = value;
    if (prototype != nullptr)
      prototype->AddRef();
  }

  int refCount;
  idList<swfNamedVar_t, 72> variables;
  int variablesHash[16];
  idSWFScriptObject* prototype;

protected:
  swfNamedVar_t* GetVariable(const char* name, bool create);
};

class idSWFScriptArray : public idSWFScriptObject {
public:
  static const char* const CLASS;
  idSWFScriptArray();
  ~idSWFScriptArray() override;
  const char* Class() const override;
  bool DerivedFrom(const char* className) const override;
  void Clear() override;
  idSWFScriptVar Get(int index);
  idSWFScriptVar Get(const char* name) override;
  void Set(int index, const idSWFScriptVar& value);
  void Set(const char* name, const idSWFScriptVar& value) override;
  bool Delete(int index);
  bool Delete(const char* name) override;
  void EnumVariables(idList<idSWFScriptVar>& names) override;

  int length;
  idList<int, 72> sparseIndex;
  idList<idSWFScriptVar, 72> sparseValues;
};

class idSWFScriptSprite : public idSWFScriptObject {
public:
  static const char* const CLASS;
  const char* Class() const override;
  bool DerivedFrom(const char* className) const override;
  idSWFScriptVar Get(const char* name) override;
  void Set(const char* name, const idSWFScriptVar& value) override;
  bool Delete(const char* name) override;
  void EnumVariables(idList<idSWFScriptVar>& names) override;
  void PrintToConsole() override;

  idSWFSpriteInstance* sprite;
};

class idSWFScriptText : public idSWFScriptObject {
public:
  static const char* const CLASS;
  const char* Class() const override;
  bool DerivedFrom(const char* className) const override;
  void PrintToConsole() override;

  idSWFTextInstance* text;
};

class idSWFParmList : public idStaticList<idSWFScriptVar, 16> {};
class idSWFScriptFunction : public idSWFScriptObject {
public:
  static const char* const CLASS;
  const char* Class() const override { return CLASS; }
  bool DerivedFrom(const char* className) const override {
    return className == CLASS || className == idSWFScriptObject::CLASS;
  }
  ~idSWFScriptFunction() override = default;
  virtual idSWFScriptVar Call(idSWFScriptObject* thisObject, idSWFParmList& parameters) = 0;
};

class idSWFScriptFunction_RefCounted : public idSWFScriptFunction {
public:
  idSWFScriptFunction_RefCounted() { refCount = 0; }
  static void* operator new(std::size_t size) {
    return idMem::Singleton()->AllocWithLocation(
        "w:\\build\\tech5\\shared\\idlib\\Heap.h(95) : tag",size,
        TAG_NEW,false,ALIGN_16,HEAP_DEFAULTHEAP);
  }
  static void operator delete(void* pointer) noexcept {
    if (pointer != nullptr)
      idMem::Singleton()->Free(pointer,ALIGN_16);
  }
  static void operator delete(void* pointer,std::size_t) noexcept {
    operator delete(pointer);
  }
};

static_assert(sizeof(idSWFScriptObject::swfNamedVar_t) == 48,
              "idSWFScriptObject::swfNamedVar_t ABI drift");
static_assert(sizeof(idSWFScriptObject) == 112, "idSWFScriptObject ABI drift");
static_assert(sizeof(idSWFScriptArray) == 168, "idSWFScriptArray ABI drift");
static_assert(sizeof(idSWFScriptSprite) == 120, "idSWFScriptSprite ABI drift");
static_assert(sizeof(idSWFScriptText) == 120, "idSWFScriptText ABI drift");
static_assert(sizeof(idSWFScriptFunction) == 112, "idSWFScriptFunction ABI drift");
static_assert(sizeof(idSWFScriptFunction_RefCounted) == 112,
              "reference-counted SWF function ABI drift");
static_assert(sizeof(idSWFParmList) == 280, "idSWFParmList ABI drift");
