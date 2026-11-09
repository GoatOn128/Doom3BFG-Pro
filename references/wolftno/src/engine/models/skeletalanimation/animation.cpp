#include "engine/models/skeletalanimation/animation.h"

#include <algorithm>
#include <cctype>
#include <cstring>
#include <limits>

const idJointIndex_t NULL_JOINT_INDEX{};
const idUserChannelIndex NULL_USERCHANNEL_INDEX{};

bool anim_showLoads = false;
bool anim_forceReload = false;
bool anim_allowWeakRefs = true;
const char* anim_debugAnimName = "";

namespace {
bool markStaticDone = false;

template <typename Handle>
Handle InvalidHandle() {
  return Handle{};
}

template <typename Hash, typename Handle>
typename Hash::entry_t* EntryForHandle(Hash& hash, Handle handle) {
  if (!handle.IsValid() || handle.value >= hash.list.num)
    return nullptr;
  return &hash.list[handle.value];
}

template <typename Hash, typename Handle>
const typename Hash::entry_t* EntryForHandle(const Hash& hash, Handle handle) {
  if (!handle.IsValid() || handle.value >= hash.list.num)
    return nullptr;
  return &hash.list[handle.value];
}
} // namespace

template <typename Object, typename Parent, typename Handle, int InitialSize>
std::string
idAnimationHash<Object, Parent, Handle, InitialSize>::NormalizeName(
    const char* name) {
  std::string normalized = name != nullptr ? name : "";
  for (char& character : normalized) {
    if (character == '\\')
      character = '/';
    else
      character = static_cast<char>(
          std::tolower(static_cast<unsigned char>(character)));
  }
  return normalized;
}

template <typename Object, typename Parent, typename Handle, int InitialSize>
idAnimationHash<Object, Parent, Handle, InitialSize>::idAnimationHash()
    : list(), nameHash(InitialSize, InitialSize) {
  list.SetNum(InitialSize);
  list.num = 0;
}

template <typename Object, typename Parent, typename Handle, int InitialSize>
idAnimationHash<Object, Parent, Handle, InitialSize>::~idAnimationHash() {
  Free();
}

template <typename Object, typename Parent, typename Handle, int InitialSize>
int idAnimationHash<Object, Parent, Handle, InitialSize>::IndexForName(
    const char* name, int* key) const {
  const std::string normalized = NormalizeName(name);
  std::uint32_t hash = 2166136261u;
  for (const unsigned char character : normalized) {
    hash ^= character;
    hash *= 16777619u;
  }
  const int hashKey = static_cast<int>(hash & 0x7fffffffu);
  if (key != nullptr)
    *key = hashKey;
  for (int index = nameHash.First(hashKey); index >= 0;
       index = nameHash.Next(index)) {
    if (index < list.num &&
        NormalizeName(list[index].name.str) == normalized)
      return index;
  }
  return -1;
}

template <typename Object, typename Parent, typename Handle, int InitialSize>
Handle idAnimationHash<Object, Parent, Handle, InitialSize>::FindHandleByName(
    const char* name) const {
  if (name == nullptr || *name == '\0')
    return InvalidHandle<Handle>();
  const int index = IndexForName(name);
  return index >= 0 ? list[index].handle : InvalidHandle<Handle>();
}

template <typename Object, typename Parent, typename Handle, int InitialSize>
Handle idAnimationHash<Object, Parent, Handle, InitialSize>::Add(
    Object* object, const Parent* parent, const char* name, entry_t** result) {
  if (name == nullptr || *name == '\0')
    return InvalidHandle<Handle>();

  int key = 0;
  int index = IndexForName(name, &key);
  if (index >= 0 && list[index].ptr != nullptr) {
    if (result != nullptr)
      *result = &list[index];
    return list[index].handle;
  }
  if (index < 0) {
    if (list.num >= std::numeric_limits<unsigned short>::max())
      return InvalidHandle<Handle>();
    index = list.num;
    entry_t* allocated = list.Alloc();
    if (allocated == nullptr)
      return InvalidHandle<Handle>();
    nameHash.Add(key, index);
  }

  entry_t& entry = list[index];
  const std::string normalized = NormalizeName(name);
  entry.handle.Set(static_cast<unsigned short>(index));
  entry.name.Set(normalized.c_str());
  entry.ptr = object;
  entry.parent = parent;
  entry.refType = object != nullptr ? REF_STRONG : REF_WEAK;
  if (result != nullptr)
    *result = &entry;
  return entry.handle;
}

template <typename Object, typename Parent, typename Handle, int InitialSize>
void idAnimationHash<Object, Parent, Handle, InitialSize>::FreeObject(
    Object*& object) {
  if constexpr (std::is_same_v<Object, char>) {
    delete[] object;
  } else if constexpr (std::is_same_v<Object, idAliasDeclList>) {
    delete object;
  }
  object = nullptr;
}

template <typename Object, typename Parent, typename Handle, int InitialSize>
void idAnimationHash<Object, Parent, Handle, InitialSize>::Free() {
  for (int index = 0; index < list.num; ++index) {
    FreeObject(list[index].ptr);
    list[index].parent = nullptr;
    list[index].refType = REF_FREED;
  }
  nameHash.InternalFree();
  list.ClearFree();
  markStaticNum = 0;
}

template <typename Object, typename Parent, typename Handle, int InitialSize>
void idAnimationHash<Object, Parent, Handle, InitialSize>::RebuildHash() {
  nameHash.InternalFree();
  nameHash.InternalInit(InitialSize, InitialSize);
  for (int index = 0; index < list.num; ++index) {
    int key = 0;
    IndexForName(list[index].name.str, &key);
    nameHash.Add(key, index);
  }
}

template <typename Object, typename Parent, typename Handle, int InitialSize>
void idAnimationHash<Object, Parent, Handle, InitialSize>::FreeDynamic() {
  const int retained = std::clamp(markStaticNum, 0, list.num);
  for (int index = retained; index < list.num; ++index) {
    FreeObject(list[index].ptr);
    list[index].parent = nullptr;
    list[index].refType = REF_FREED;
  }
  list.num = retained;
  RebuildHash();
}

template class idAnimationHash<char, char, idJointHandle_t, 256>;
template class idAnimationHash<char, char, idUserChannelHandle_t, 256>;
template class idAnimationHash<idAliasDeclList, idDeclMD6, idAliasHandle_t,
                               4096>;
template class idAnimationHash<idJointConversion, idJointConversion,
                               idJointConversionHandle_t, 256>;

char* idAnimation_StrDup(const char* text) {
  if (text == nullptr)
    return nullptr;
  const std::size_t length = std::strlen(text);
  char* duplicate = new char[length + 1];
  for (std::size_t index = 0; index <= length; ++index)
    duplicate[index] = static_cast<char>(
        std::tolower(static_cast<unsigned char>(text[index])));
  return duplicate;
}

idTypedResourceList<idJointResource> idJointResource::resourceList(
    "joint resource");

idResourceList* idJointResource::GetResourceList() const {
  return &resourceList;
}

template <>
idJointResource*
idTypedResourceList<idJointResource>::Alloc(const char* name) const {
  auto* resource = new idJointResource;
  resource->resourceListPtr =
      const_cast<idTypedResourceList<idJointResource>*>(this);
  resource->SetName(name);
  return resource;
}

template <>
int idTypedResourceList<idJointResource>::AllocSize() const {
  return 48;
}

idAnimationLocal::idAnimationLocal() = default;

idAnimationLocal::~idAnimationLocal() {
  SetJointConversionAnimation(nullptr);
}

void idAnimationLocal::Initialize() {
  originJointHandle = RegisterJoint(GetOriginJointName());
  randomSeed = 1;
  SetJointConversionAnimation(this);
}

void idAnimationLocal::Shutdown() {
  jointHash.Free();
  userChannelHash.Free();
  aliasHash.Free();
  jointConversionHash.Free();
  strongAliasRefs.ClearFree();
}

void idAnimationLocal::FreeDynamic() {
  if (!markStaticDone) {
    markStaticDone = true;
    jointHash.markStaticNum = jointHash.list.num;
    userChannelHash.markStaticNum = userChannelHash.list.num;
    aliasHash.markStaticNum = aliasHash.list.num;
    jointConversionHash.markStaticNum = jointConversionHash.list.num;
  }
  jointHash.FreeDynamic();
  userChannelHash.FreeDynamic();
  aliasHash.FreeDynamic();
  jointConversionHash.FreeDynamic();
  strongAliasRefs.ClearFree();
}

idJointHandle_t idAnimationLocal::RegisterJoint(const char* name) {
  if (name == nullptr || *name == '\0')
    return NULL_JOINT;
  const idJointHandle_t existing = jointHash.FindHandleByName(name);
  if (existing.IsValid())
    return existing;
  char* duplicate = idAnimation_StrDup(name);
  return jointHash.Add(duplicate, nullptr, duplicate);
}

idJointHandle_t idAnimationLocal::FindJointHandle(const char* name) const {
  return jointHash.FindHandleByName(name);
}

const char* idAnimationLocal::GetJointName(idJointHandle_t handle) const {
  const auto* entry = EntryForHandle(jointHash, handle);
  return entry != nullptr ? entry->name.str : nullptr;
}

idUserChannelHandle_t
idAnimationLocal::RegisterUserChannel(const char* name) {
  if (name == nullptr || *name == '\0')
    return NULL_USERCHANNEL;
  const idUserChannelHandle_t existing =
      userChannelHash.FindHandleByName(name);
  if (existing.IsValid())
    return existing;
  char* duplicate = idAnimation_StrDup(name);
  return userChannelHash.Add(duplicate, nullptr, duplicate);
}

idUserChannelHandle_t
idAnimationLocal::FindUserChannelHandle(const char* name) const {
  return userChannelHash.FindHandleByName(name);
}

const char*
idAnimationLocal::GetUserChannelName(idUserChannelHandle_t handle) const {
  const auto* entry = EntryForHandle(userChannelHash, handle);
  return entry != nullptr ? entry->name.str : nullptr;
}

int idAnimationLocal::GetNumUserChannels() const {
  return userChannelHash.list.num;
}

idJointConversionHandle_t
idAnimationLocal::LoadJointConversion(const char* name) {
  if (name == nullptr || *name == '\0')
    return NULL_JOINT_CONVERSION;
  idJointConversionHash::entry_t* entry = nullptr;
  idJointConversionHandle_t handle = jointConversionHash.FindHandleByName(name);
  if (handle.IsValid()) {
    entry = EntryForHandle(jointConversionHash, handle);
    if (entry != nullptr && entry->ptr != nullptr)
      return handle;
  } else {
    handle = jointConversionHash.Add(nullptr, nullptr, name, &entry);
  }
  if (entry == nullptr)
    entry = EntryForHandle(jointConversionHash, handle);
  idResource* loaded = idJointConversion::resourceList.Load(
      entry != nullptr ? entry->name.str : name, true, false);
  auto* conversion = dynamic_cast<idJointConversion*>(loaded);
  if (entry == nullptr || conversion == nullptr)
    return NULL_JOINT_CONVERSION;
  entry->ptr = conversion;
  entry->parent = conversion;
  entry->refType = REF_STRONG;
  conversion->SetHandle(handle);
  return handle;
}

idJointConversionHandle_t
idAnimationLocal::FindJointConversionHandle(const char* name) {
  const idJointConversionHandle_t handle =
      jointConversionHash.FindHandleByName(name);
  const auto* entry = EntryForHandle(jointConversionHash, handle);
  return entry != nullptr && entry->ptr != nullptr ? handle
                                                   : LoadJointConversion(name);
}

const char* idAnimationLocal::GetJointConversionName(
    idJointConversionHandle_t handle) const {
  const auto* entry = EntryForHandle(jointConversionHash, handle);
  return entry != nullptr ? entry->name.str : nullptr;
}

idCrcHandle_t idAnimationLocal::GetJointConversionCrc(
    idJointConversionHandle_t handle) const {
  const auto* entry = EntryForHandle(jointConversionHash, handle);
  return entry != nullptr && entry->ptr != nullptr ? entry->ptr->GetChecksum()
                                                   : NULL_CRC;
}

idJointIndex_t idAnimationLocal::GetJointIndex(
    idJointConversionHandle_t conversion, idJointHandle_t joint) const {
  const auto* entry = EntryForHandle(jointConversionHash, conversion);
  return joint.IsValid() && entry != nullptr && entry->ptr != nullptr
             ? entry->ptr->GetJointIndex(joint)
             : NULL_JOINT_INDEX;
}

idJointIndex_t idAnimationLocal::GetJointIndex(
    idJointConversionHandle_t conversion, const char* jointName) const {
  return GetJointIndex(conversion, FindJointHandle(jointName));
}

idJointIndex_t idAnimationLocal::GetJointIndex(const char* conversion,
                                                const char* jointName) {
  return GetJointIndex(FindJointConversionHandle(conversion), jointName);
}

idUserChannelIndex idAnimationLocal::GetUserChannelIndex(
    idJointConversionHandle_t conversion,
    idUserChannelHandle_t channel) const {
  const auto* entry = EntryForHandle(jointConversionHash, conversion);
  return channel.IsValid() && entry != nullptr && entry->ptr != nullptr
             ? entry->ptr->GetUserChannelIndex(channel)
             : NULL_USERCHANNEL_INDEX;
}

idUserChannelIndex idAnimationLocal::GetUserChannelIndex(
    idJointConversionHandle_t conversion, const char* channelName) const {
  return GetUserChannelIndex(conversion,
                             FindUserChannelHandle(channelName));
}

idUserChannelIndex idAnimationLocal::GetUserChannelIndex(
    const char* conversion, const char* channelName) {
  return GetUserChannelIndex(FindJointConversionHandle(conversion),
                             channelName);
}

const idCompressionGroup* idAnimationLocal::GetCompressionGroup(
    idJointConversionHandle_t conversion, int index) const {
  const auto* entry = EntryForHandle(jointConversionHash, conversion);
  if (entry == nullptr || entry->ptr == nullptr || index < 0 ||
      index >= entry->ptr->compressionGroups.Num())
    return nullptr;
  return entry->ptr->compressionGroups[index];
}

const idCompressionGroup* idAnimationLocal::GetCompressionGroup(
    idJointConversionHandle_t conversion, const char* name) const {
  const auto* entry = EntryForHandle(jointConversionHash, conversion);
  return entry != nullptr && entry->ptr != nullptr
             ? entry->ptr->GetCompressionGroup(name)
             : nullptr;
}

int idAnimationLocal::GetNumCompressionGroups(
    idJointConversionHandle_t conversion) const {
  const auto* entry = EntryForHandle(jointConversionHash, conversion);
  return entry != nullptr && entry->ptr != nullptr
             ? entry->ptr->compressionGroups.Num()
             : 0;
}

void idAnimationLocal::MakeDefaultJointConversion(
    const char* name, const idList<idStr, 5>& joints,
    const idList<idStr, 5>& channels) {
  idJointConversion::MakeDefault(name, joints, channels);
  idResource* existing = idJointConversion::resourceList.FindExisting(name, true);
  if (existing != nullptr)
    existing->LoadResource();
}

void idAnimationLocal::UnregisterJointConversion(
    idJointConversion* conversion) {
  if (conversion == nullptr)
    return;
  auto* entry = EntryForHandle(jointConversionHash, conversion->GetHandle());
  if (entry != nullptr && entry->ptr != nullptr) {
    entry->refType = REF_FREED;
    entry->ptr = nullptr;
    entry->parent = nullptr;
  }
}

idAliasHandle_t idAnimationLocal::RegisterAlias(const idDeclMD6* decl,
                                                 const char* aliasName) {
  if (aliasName == nullptr || *aliasName == '\0')
    return NULL_ALIAS;
  idAliasHandle_t handle = aliasHash.FindHandleByName(aliasName);
  auto* entry = EntryForHandle(aliasHash, handle);
  if (entry == nullptr) {
    auto* declarations = new idAliasDeclList;
    declarations->Append(decl);
    return aliasHash.Add(declarations, decl, aliasName);
  }
  if (decl == nullptr)
    return entry->handle;
  if (entry->ptr == nullptr)
    entry->ptr = new idAliasDeclList;
  for (int index = 0; index < entry->ptr->Num(); ++index) {
    if ((*entry->ptr)[index] == decl)
      return entry->handle;
    if ((*entry->ptr)[index] == nullptr) {
      (*entry->ptr)[index] = decl;
      return entry->handle;
    }
  }
  entry->ptr->Append(decl);
  entry->refType = REF_STRONG;
  return entry->handle;
}

void idAnimationLocal::UnregisterAlias(const idDeclMD6* decl,
                                       idAliasHandle_t handle) {
  auto* entry = EntryForHandle(aliasHash, handle);
  if (entry == nullptr || entry->ptr == nullptr)
    return;
  entry->ptr->RemoveFast(decl);
  if (entry->ptr->Num() == 0) {
    delete entry->ptr;
    entry->ptr = nullptr;
    entry->parent = nullptr;
    entry->refType = REF_FREED;
  }
}

void idAnimationLocal::UnregisterAliases(const idDeclMD6* decl) {
  for (int index = aliasHash.list.num - 1; index >= 0; --index)
    UnregisterAlias(decl, aliasHash.list[index].handle);
}

idAliasHandle_t
idAnimationLocal::FindAliasHandle(const char* aliasName) const {
  return aliasHash.FindHandleByName(aliasName);
}

idAliasHandle_t idAnimationLocal::FindAliasHandle(
    const idDeclMD6* decl, const char* aliasName) const {
  const idAliasHandle_t handle = FindAliasHandle(aliasName);
  const auto* entry = EntryForHandle(aliasHash, handle);
  if (entry == nullptr || entry->ptr == nullptr)
    return entry != nullptr ? handle : NULL_ALIAS;
  for (int index = 0; index < entry->ptr->Num(); ++index) {
    const idDeclMD6* owner = (*entry->ptr)[index];
    if (owner == decl || (decl != nullptr && decl->IsChildOf(owner)))
      return handle;
  }
  return NULL_ALIAS;
}

const char* idAnimationLocal::FindAliasName(idAliasHandle_t handle) const {
  const auto* entry = EntryForHandle(aliasHash, handle);
  return entry != nullptr ? entry->name.str : nullptr;
}

idAliasHandle_t idAnimationLocal::StrongLoadAlias(const idDeclMD6* decl,
                                                   const char* aliasName) {
  if (decl == nullptr)
    return NULL_ALIAS;
  const idAliasHandle_t handle = RegisterAlias(decl, aliasName);
  StrongLoadAlias(decl, handle);
  return handle;
}

const idMD6Alias* idAnimationLocal::StrongLoadAlias(
    const idDeclMD6* decl, idAliasHandle_t handle) {
  if (decl == nullptr)
    return nullptr;
  const auto* entry = EntryForHandle(aliasHash, handle);
  if (entry != nullptr)
    RegisterAlias(decl, entry->name.str);
  auto* alias = const_cast<idMD6Alias*>(decl->FindAlias(handle.value));
  if (alias != nullptr)
    alias->LoadAnims(decl);
  return alias;
}

idAliasHandle_t idAnimationLocal::AddStrongAliasRef(const char* aliasName) {
  if (aliasName == nullptr || *aliasName == '\0')
    return NULL_ALIAS;
  const idAliasHandle_t handle = RegisterAlias(nullptr, aliasName);
  for (int index = 0; index < strongAliasRefs.Num(); ++index)
    if (strongAliasRefs[index] == handle)
      return handle;
  strongAliasRefs.Append(handle);
  return handle;
}

void idAnimationLocal::GetAliasRefs(
    idAliasHandle_t handle, bool strongOnly,
    idList<const idDeclMD6*, 5>& declarations) const {
  declarations.Clear();
  const auto* entry = EntryForHandle(aliasHash, handle);
  if (entry == nullptr || entry->refType == REF_FREED || entry->ptr == nullptr)
    return;
  for (int index = 0; index < entry->ptr->Num(); ++index) {
    const idDeclMD6* decl = (*entry->ptr)[index];
    if (decl == nullptr)
      continue;
    const idMD6Alias* alias = decl->FindAlias(handle.value);
    if (strongOnly && (alias == nullptr || (alias->flags & 2u) == 0))
      continue;
    bool duplicate = false;
    for (int existing = 0; existing < declarations.Num(); ++existing)
      duplicate |= declarations[existing] == decl;
    if (!duplicate)
      declarations.Append(decl);
  }
}

void idAnimationLocal::FinalizeAliasRefs(idAliasHandle_t handle) {
  auto* entry = EntryForHandle(aliasHash, handle);
  if (entry == nullptr || entry->ptr == nullptr)
    return;
  for (int index = 0; index < entry->ptr->Num(); ++index) {
    const idDeclMD6* decl = (*entry->ptr)[index];
    auto* alias = decl != nullptr
                      ? const_cast<idMD6Alias*>(decl->FindAlias(handle.value))
                      : nullptr;
    if (alias == nullptr)
      continue;
    for (int mapping = 0; mapping < alias->mappings.Num(); ++mapping)
      alias->LoadAnimation(mapping);
  }
}

void idAnimationLocal::FinalizeAliasRefs() {
  for (int index = 0; index < strongAliasRefs.Num(); ++index)
    FinalizeAliasRefs(strongAliasRefs[index]);
  strongAliasRefs.ClearFree();
}

const char* idAnimationLocal::GetOriginJointName() const { return "origin"; }

idJointHandle_t idAnimationLocal::GetOriginJointHandle() const {
  return originJointHandle;
}

void idAnimationLocal::VerifyAnimations(const idCmdArgs*) {
  // Bounding-box verification is driven by the registered alias declarations.
  // The retail command is intentionally side-effect free when no verifier is
  // installed, which is the engine-core configuration reconstructed here.
}

idAnimationLocal animationLocal;
idAnimation* animation = &animationLocal;

