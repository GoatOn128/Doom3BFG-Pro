#pragma once

#include "shared/idlib/containers/list.h"

#include <cstddef>
#include <cstdint>

class __single_inheritance idClass;
class idTypeInfo;
class idCmdArgs;

template <typename Type> class idHierarchy {
public:
  idHierarchy() = default;
  ~idHierarchy();

  void RemoveFromParent();
  __declspec(noinline) void RemoveFromHierarchy() noexcept;

  __forceinline void ParentTo(idHierarchy<Type>& newParent) {
    RemoveFromParent();
    parent = &newParent;
    sibling = newParent.child;
    newParent.child = this;
  }

  __forceinline Type* GetChild() const {
    return child != nullptr ? child->owner : nullptr;
  }

  __forceinline Type* GetNext() const {
    if (child != nullptr) {
      return child->owner;
    }
    const idHierarchy<Type>* node = this;
    while (node != nullptr && node->sibling == nullptr) {
      node = node->parent;
    }
    return node != nullptr ? node->sibling->owner : nullptr;
  }

  idHierarchy<Type>* parent;
  idHierarchy<Type>* sibling;
  idHierarchy<Type>* child;
  Type* owner;
};

class idTypeInfo {
public:
  using CreateInstanceFunction = idClass* (*)();
  using SpawnFunction = void (idClass::*)();

  static constexpr int INVALID_TYPE = -1;

  idTypeInfo(const char* classname_, const char* superclass_, int size_,
             bool abstract_, CreateInstanceFunction createInstance_,
             SpawnFunction spawn_);
  ~idTypeInfo();

  void Init();
  void Shutdown() {}

  const char* classname;
  const char* superclass;
  CreateInstanceFunction CreateInstance;
  SpawnFunction Spawn;
  bool initialized;
  std::uint8_t recoveredInitializedPad[7];
  idTypeInfo* super;
  idTypeInfo* next;
  bool isAbstract;
  std::uint8_t recoveredAbstractPad[3];
  int size;
  int typeNum;
  int lastChild;
  idHierarchy<idTypeInfo> node;
};

class idClass {
public:
  idClass() = default;
  virtual idTypeInfo* GetType() const { return &Type; }
  virtual ~idClass();

  void Spawn();
  void CallSpawn();
  const char* GetClassname() const;

  static idClass* CreateInstance(const char* name);
  static idClass* CreateInstance();
  static idTypeInfo* GetClass(const char* name);
  static void Init();
  static void Shutdown();
  static void DisplayInfo_f(const idCmdArgs& args);
  static void ListClasses_f(const idCmdArgs& args);
  static int GetNumTypes() { return types.num; }
  static int GetTypeNumBits() { return typeNumBits; }
  static idTypeInfo* GetType(int typeNumber) {
    return typenums.list[typeNumber];
  }

  static idTypeInfo Type;

private:
  using SpawnFunction = idTypeInfo::SpawnFunction;

  __declspec(noinline) SpawnFunction CallSpawnFunc(idTypeInfo* cls);

  static bool initialized;
  static idList<idTypeInfo*, 51> types;
  static idList<idTypeInfo*, 51> typenums;
  static int typeNumBits;
  static std::uint64_t memused;
  static int numobjects;

  friend class ClassValidationAccess;
};

static_assert(sizeof(idHierarchy<idTypeInfo>) == 0x20,
              "idHierarchy<idTypeInfo> ABI drift");
static_assert(offsetof(idHierarchy<idTypeInfo>, parent) == 0x00,
              "idHierarchy parent offset drift");
static_assert(offsetof(idHierarchy<idTypeInfo>, owner) == 0x18,
              "idHierarchy owner offset drift");
static_assert(sizeof(idTypeInfo) == 0x68, "idTypeInfo ABI drift");
static_assert(offsetof(idTypeInfo, CreateInstance) == 0x10,
              "idTypeInfo factory offset drift");
static_assert(offsetof(idTypeInfo, Spawn) == 0x18,
              "idTypeInfo spawn offset drift");
static_assert(offsetof(idTypeInfo, super) == 0x28,
              "idTypeInfo super offset drift");
static_assert(offsetof(idTypeInfo, next) == 0x30,
              "idTypeInfo next offset drift");
static_assert(offsetof(idTypeInfo, typeNum) == 0x40,
              "idTypeInfo type-number offset drift");
static_assert(offsetof(idTypeInfo, node) == 0x48,
              "idTypeInfo hierarchy offset drift");
static_assert(sizeof(idClass) == 0x08, "idClass ABI drift");
