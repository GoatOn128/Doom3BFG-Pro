#define WOLFTNO_RETAIL_IDLIB_ERROR_RETURNS_ABI
#define WOLFTNO_RETAIL_IDLIST_FORCEINLINE_CLEARFREE_ABI
#define WOLFTNO_RETAIL_IDLIST_FORCEINLINE_CTOR_DTOR_ABI
#define WOLFTNO_RETAIL_IDMEM_SINGLETON_ABI

#include "engine/gamelib/class.h"

#undef WOLFTNO_RETAIL_IDLIST_FORCEINLINE_CTOR_DTOR_ABI
#undef WOLFTNO_RETAIL_IDLIST_FORCEINLINE_CLEARFREE_ABI

#include "shared/idlib/lib.h"
#include "shared/idlib/text/str.h"

#include <bit>
#include <intrin.h>

class idMD6Anim;

template <>
bool idList<idMD6Anim*, 5>::Resize(int newSize);

static idTypeInfo* typelist = nullptr;
static idHierarchy<idTypeInfo> classHierarchy;

bool idClass::initialized = false;
int idClass::typeNumBits = 0;
std::uint64_t idClass::memused = 0;
int idClass::numobjects = 0;
idList<idTypeInfo*, 51> idClass::types;
idList<idTypeInfo*, 51> idClass::typenums;

idClass::SpawnFunction idClass::CallSpawnFunc(idTypeInfo* cls) {
  SpawnFunction result;
  if (cls->super == nullptr ||
      (result = CallSpawnFunc(cls->super)) != cls->Spawn) {
    (this->*cls->Spawn)();
    return cls->Spawn;
  }
  return result;
}

const char* idClass::GetClassname() const {
  return GetType()->classname;
}

void idClass::CallSpawn() {
  idTypeInfo* const type = GetType();
  if (type->super == nullptr || CallSpawnFunc(type->super) != type->Spawn) {
    (this->*type->Spawn)();
  }
}

idTypeInfo* idClass::GetClass(const char* name) {
  if (!initialized) {
    idTypeInfo* type = typelist;
    while (type != nullptr) {
      if (idStr::Cmp(type->classname, name) == 0) {
        return type;
      }
      type = type->next;
    }
    return nullptr;
  }

  int low = 0;
  int high = types.num - 1;
  while (low <= high) {
    const int middle = (high + low) / 2;
    idTypeInfo* const type = types.list[middle];
    const int comparison = idStr::Cmp(type->classname, name);
    if (comparison == 0) {
      return type;
    }
    if (comparison <= 0) {
      low = middle + 1;
    } else {
      high = middle - 1;
    }
  }
  return nullptr;
}

idClass* idClass::CreateInstance(const char* name) {
  idTypeInfo* const type = GetClass(name);
  return type != nullptr ? type->CreateInstance() : nullptr;
}

void idClass::Shutdown() {
  for (idTypeInfo* type = typelist; type != nullptr; type = type->next) {
    _ReadWriteBarrier();
  }
  types.ClearFree();
  typenums.ClearFree();
  initialized = false;
}

template <>
void idHierarchy<idTypeInfo>::RemoveFromParent() {
  idHierarchy<idTypeInfo>* const currentParent = parent;
  if (currentParent != nullptr) {
    idHierarchy<idTypeInfo>* current = currentParent->child;
    if (current == this) {
      currentParent->child = sibling;
    } else {
      idHierarchy<idTypeInfo>* prior;
      do {
        if (current == nullptr) {
          idLib::Error(
              "idHierarchy::GetPriorSibling: could not find node in parent's "
              "list of children");
          __debugbreak();
        }
        prior = current;
        current = current->sibling;
      } while (current != this);
      prior->sibling = sibling;
    }
  }
  parent = nullptr;
  sibling = nullptr;
}

template <>
void idHierarchy<idTypeInfo>::RemoveFromHierarchy() noexcept {
  idHierarchy<idTypeInfo>* const oldParent = parent;
  RemoveFromParent();

  if (oldParent != nullptr) {
    while (child != nullptr) {
      idHierarchy<idTypeInfo>* const currentChild = child;
      currentChild->RemoveFromParent();
      currentChild->parent = oldParent;
      currentChild->sibling = oldParent->child;
      oldParent->child = currentChild;
    }
  } else {
    while (child != nullptr) {
      child->RemoveFromParent();
    }
  }
}

template <>
idHierarchy<idTypeInfo>::~idHierarchy() {
  RemoveFromHierarchy();
}

void idTypeInfo::Init() {
  if (initialized) {
    return;
  }

  if (super != nullptr && !super->initialized) {
    super->Init();
  }

  idHierarchy<idTypeInfo>& newParent =
      super != nullptr ? super->node : classHierarchy;
  node.ParentTo(newParent);
  node.owner = this;

  for (idTypeInfo* ancestor = super; ancestor != nullptr;
       ancestor = ancestor->super) {
    ++ancestor->lastChild;
  }
  initialized = true;
}

idTypeInfo::idTypeInfo(const char* classname_, const char* superclass_,
                       int size_, bool abstract_,
                       CreateInstanceFunction createInstance_,
                       SpawnFunction spawn_) {
  node.owner = nullptr;
  node.parent = nullptr;
  node.sibling = nullptr;
  node.child = nullptr;
  initialized = false;
  classname = classname_;
  superclass = superclass_;
  Spawn = spawn_;
  CreateInstance = createInstance_;
  super = idClass::GetClass(superclass_);
  isAbstract = abstract_;
  size = size_;
  typeNum = 0;
  lastChild = 0;

  for (idTypeInfo* type = typelist; type != nullptr; type = type->next) {
    if (type->super == nullptr && idStr::Cmp(type->classname, "idClass") != 0 &&
        idStr::Cmp(type->superclass, classname) == 0) {
      type->super = this;
    }
  }

  idTypeInfo** link = &typelist;
  while (*link != nullptr && idStr::Cmp(classname, (*link)->classname) >= 0) {
    link = &(*link)->next;
  }
  next = *link;
  *link = this;
}

idTypeInfo::~idTypeInfo() = default;

void idClass::Init() {
  idLib::Printf("Initializing class hierarchy\n");
  if (initialized) {
    idLib::Printf("...already initialized\n");
    return;
  }

  for (idTypeInfo* type = typelist; type != nullptr; type = type->next) {
    type->Init();
  }

  int count = 0;
  for (idTypeInfo* type = classHierarchy.GetNext(); type != nullptr;
       type = type->node.GetNext()) {
    type->lastChild += count;
    type->typeNum = count;
    ++count;
  }

  const std::uint32_t floatBits =
      std::bit_cast<std::uint32_t>(static_cast<float>(count));
  typeNumBits = static_cast<int>(static_cast<std::uint8_t>(floatBits >> 23)) -
                126;

  if (count > types.size &&
      !reinterpret_cast<idList<idMD6Anim*, 5>&>(types).Resize(count)) {
    goto types_resize_failed;
  }
  types.num = count < types.size ? count : types.size;

types_resize_failed:
  if (count > typenums.size &&
      !reinterpret_cast<idList<idMD6Anim*, 5>&>(typenums).Resize(count)) {
    goto typenums_resize_failed;
  }
  typenums.num = count < typenums.size ? count : typenums.size;

typenums_resize_failed:
  int index = 0;
  for (idTypeInfo* type = typelist; type != nullptr; type = type->next) {
    types.list[index++] = type;
    typenums.list[type->typeNum] = type;
  }
  initialized = true;
}
