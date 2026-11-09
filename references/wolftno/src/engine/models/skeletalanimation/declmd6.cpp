#include "engine/models/skeletalanimation/declmd6.h"

#include "engine/models/skeletalanimation/md6morph.h"
#include "engine/models/skeletalanimation/md6util.h"
#include "engine/models/skeletalanimation/jointgroup.h"
#include "engine/models/skeletalanimation/taginfo.h"
#include "shared/idlib/csystems/cvarsystemlocal.h"
#include "shared/idlib/math/random.h"

#include <algorithm>
#include <cstdint>
#include <cstring>
#include <string>
#include <vector>

idCVar md6_ForceGenMorphVerts(
    "md6_ForceGenMorphVerts", "0", CVAR_BOOL,
    "force regeneration of MD6 morph vertices", nullptr);

namespace {
idAnimEventTransformCacheCallback transformCacheCallback = nullptr;
idAnimEventSoundPinCallback soundPinCallback = nullptr;
idDeclMD6RuntimeCallbacks declRuntimeCallbacks{};

struct validMeshKitSet_t {
  idList<idAtomicString, 5> kitNames[3];
};

struct idDeclMD6RecoveredState {
  struct meshKit_t {
    idStr name;
    idList<int, 19> meshIndices;
  };

  idAnimEvents animEvents;
  idList<const idDeclMD6*, 5> includes;
  idList<idVarArgs<6>, 19> userProps;
  idList<idAtomicString, 5> headTrackGroupNames;
  idList<validMeshKitSet_t, 5> validRandomMeshKits;
  idList<idMD6MorphSet, 5> morphSets;
  struct userChannelAliasOverride_t {
    idUserChannelIndex channel;
    idAliasHandle_t alias;
  };
  idList<userChannelAliasOverride_t, 5> userChannelAliasOverrides;
  idList<idAliasHandle_t, 5> userChannelAliasMap;
  idList<meshKit_t, 5> meshKitGroups[3];
  idStr meshKitDefaultOverride[3];
  idStr defaultSkinOverride;
  idList<idStr, 5> wrinkleMapRegionExpressions;
  idList<int, 5> skipUseHintMeshes;
  idJointGroupCollection jointGroups;
  idPropsCollection props;
};

idDeclMD6RecoveredState& State(idDeclMD6& declaration) {
  if (!declaration.recoveredState) {
    declaration.recoveredState =
        std::static_pointer_cast<void>(
            std::make_shared<idDeclMD6RecoveredState>());
  }
  return *static_cast<idDeclMD6RecoveredState*>(
      declaration.recoveredState.get());
}

const idDeclMD6RecoveredState& State(const idDeclMD6& declaration) {
  return State(const_cast<idDeclMD6&>(declaration));
}

std::vector<std::string>& AliasNames() {
  static std::vector<std::string> names;
  return names;
}

bool SameText(const char* lhs, const char* rhs) {
  return idStr::Icmp(lhs != nullptr ? lhs : "", rhs != nullptr ? rhs : "") ==
         0;
}

idAliasHandle_t GlobalAliasHandle(const char* aliasName, bool add) {
  auto& names = AliasNames();
  for (std::size_t index = 0; index < names.size(); ++index) {
    if (SameText(names[index].c_str(), aliasName))
      return idAliasHandle_t(static_cast<unsigned short>(index));
  }
  if (!add || names.size() >= 0xffffu)
    return NULL_ALIAS;
  names.emplace_back(aliasName != nullptr ? aliasName : "");
  return idAliasHandle_t(static_cast<unsigned short>(names.size() - 1));
}

template <typename T>
bool ContainsPointer(const idList<const T*, 5>& list, const T* value) {
  for (int index = 0; index < list.Num(); ++index) {
    if (list[index] == value)
      return true;
  }
  return false;
}

void AppendUniqueJoint(idList<const idCachedJoint*, 5>& joints,
                       const idCachedJoint* candidate) {
  if (candidate == nullptr || joints.IsFull())
    return;
  for (int index = 0; index < joints.Num(); ++index) {
    if (candidate->frameNum == joints[index]->frameNum ||
        candidate->jointIndex.value == joints[index]->jointIndex.value)
      return;
  }
  joints.Append(candidate);
}

int EventHashKey(const char* text) {
  std::uint32_t hash = 2166136261u;
  if (text != nullptr) {
    while (*text != '\0') {
      unsigned char character = static_cast<unsigned char>(*text++);
      if (character >= 'A' && character <= 'Z')
        character = static_cast<unsigned char>(character - 'A' + 'a');
      hash ^= character;
      hash *= 16777619u;
    }
  }
  return static_cast<int>(hash & 0x7fffffffu);
}

bool FrameIsInRange(int frame, int startFrame, int endFrame) {
  return startFrame <= endFrame
             ? frame >= startFrame && frame <= endFrame
             : frame >= startFrame || frame <= endFrame;
}
} // namespace

void SetDeclMD6RuntimeCallbacks(
    const idDeclMD6RuntimeCallbacks& callbacks) {
  declRuntimeCallbacks = callbacks;
}

template <> idVarArgs<6>::~idVarArgs() {
  Free();
}

template <> void idVarArgs<6>::Grow(std::size_t newArgSize) {
  if (newArgSize == 0)
    return;
  std::size_t required = static_cast<std::size_t>(argSize) + newArgSize;
  if (required <= buffSize)
    return;
  required = std::max(required, static_cast<std::size_t>(buffSize) * 2);
  auto* replacement = static_cast<std::uint8_t*>(
      idMem::Singleton()->AllocWithLocation(
          "w:\\build\\tech5\\shared\\idlib\\Heap.h(113) : tag", required,
          static_cast<memTag_t>(0x5c), false, ALIGN_16, HEAP_DEFAULTHEAP));
  if (args != nullptr) {
    std::memmove(replacement, args, argSize);
    idMem::Singleton()->Free(args, ALIGN_16);
  }
  args = replacement;
  buffSize = static_cast<std::uint16_t>(required);
}

template <> void idVarArgs<6>::Copy(const idVarArgs& other) {
  if (this == &other)
    return;
  Free();
  numArgs = other.numArgs;
  argSize = other.argSize;
  buffSize = other.buffSize;
  std::memcpy(argOffsets, other.argOffsets, sizeof(argOffsets));
  std::memcpy(argTypes, other.argTypes, sizeof(argTypes));
  std::memcpy(argExTypes, other.argExTypes, sizeof(argExTypes));
  if (buffSize != 0) {
    args = static_cast<std::uint8_t*>(idMem::Singleton()->AllocWithLocation(
        "w:\\build\\tech5\\shared\\idlib\\Heap.h(113) : tag", buffSize,
        static_cast<memTag_t>(0x5c), false, ALIGN_16, HEAP_DEFAULTHEAP));
    if (argSize != 0)
      std::memmove(args, other.args, argSize);
  }
}

template <typename T>
void AddVarArgValue(idVarArgs<6>& target, const T& value, argTypes_t type,
                    std::uint8_t exType) {
  target.AddArg(&value, type, exType);
}

template <>
void idVarArgs<6>::AddArg(const void* value, std::uint8_t type,
                          std::uint8_t exType) {
  argTypes[numArgs] = type;
  argExTypes[numArgs] = exType;
  argOffsets[numArgs] = argSize;
  Grow(sizeof(value));
  std::memcpy(args + argOffsets[numArgs], &value, sizeof(value));
  argSize = static_cast<std::uint16_t>(argSize + sizeof(value));
  ++numArgs;
}

template <>
void idVarArgs<6>::AddArg(const char* value, std::uint8_t exType) {
  if (value == nullptr)
    value = "";
  argTypes[numArgs] = ARGTYPE_STRING;
  argExTypes[numArgs] = exType;
  argOffsets[numArgs] = argSize;
  const std::size_t length = std::strlen(value) + 1;
  Grow(length);
  std::memmove(args + argOffsets[numArgs], value, length);
  argSize = static_cast<std::uint16_t>(argSize + length);
  ++numArgs;
}

template <> void idVarArgs<6>::AddArg(float value, std::uint8_t exType) {
  argTypes[numArgs] = ARGTYPE_FLOAT;
  argExTypes[numArgs] = exType;
  argOffsets[numArgs] = argSize;
  Grow(sizeof(value));
  std::memcpy(args + argOffsets[numArgs], &value, sizeof(value));
  argSize = static_cast<std::uint16_t>(argSize + sizeof(value));
  ++numArgs;
}

template <> void idVarArgs<6>::AddArg(int value, std::uint8_t exType) {
  argTypes[numArgs] = ARGTYPE_INT;
  argExTypes[numArgs] = exType;
  argOffsets[numArgs] = argSize;
  Grow(sizeof(value));
  std::memcpy(args + argOffsets[numArgs], &value, sizeof(value));
  argSize = static_cast<std::uint16_t>(argSize + sizeof(value));
  ++numArgs;
}

template <> void idVarArgs<6>::AddArg(bool value, std::uint8_t exType) {
  argTypes[numArgs] = ARGTYPE_BOOL;
  argExTypes[numArgs] = exType;
  argOffsets[numArgs] = argSize;
  Grow(sizeof(value));
  args[argOffsets[numArgs]] = value ? 1 : 0;
  argSize = static_cast<std::uint16_t>(argSize + sizeof(value));
  ++numArgs;
}

template <>
void idVarArgs<6>::AddHandleArg(int value, std::uint8_t type,
                                std::uint8_t exType) {
  argTypes[numArgs] = type;
  argExTypes[numArgs] = exType;
  argOffsets[numArgs] = argSize;
  Grow(sizeof(value));
  std::memcpy(args + argOffsets[numArgs], &value, sizeof(value));
  argSize = static_cast<std::uint16_t>(argSize + sizeof(value));
  ++numArgs;
}

void SetAnimEventTransformCacheCallback(
    idAnimEventTransformCacheCallback callback) {
  transformCacheCallback = callback;
}

void SetAnimEventSoundPinCallback(idAnimEventSoundPinCallback callback) {
  soundPinCallback = callback;
}

idAnimEventId idAnimEvents::curEventId{};

idAnimEvents::idAnimEvents() : list(), hashIndex(1024, 1024) {}

idAnimEvents::~idAnimEvents() { Clear(); }

idAnimEventId idAnimEvents::GetNextEventId() {
  if (curEventId.value == 0xffffffffu)
    curEventId.value = 1;
  else
    ++curEventId.value;
  return curEventId;
}

void idAnimEvents::Clear() {
  hashIndex.InternalFree();
  list.ClearFree();
}

idAnimEvents::animEventInfo_t&
idAnimEvents::FindOrAdd(const char* animationName) {
  const int key = EventHashKey(animationName);
  for (int index = hashIndex.First(key); index >= 0;
       index = hashIndex.Next(index)) {
    if (index < list.Num() &&
        idStr::Icmp(list[index].animRef.str, animationName) == 0)
      return list[index];
  }
  animEventInfo_t* info = list.Alloc();
  info->animRef.Set(animationName != nullptr ? animationName : "");
  hashIndex.Add(key, list.Num() - 1);
  return *info;
}

const idAnimEvents::animEventInfo_t*
idAnimEvents::Find(const char* animationName) const {
  const int key = EventHashKey(animationName);
  for (int index = hashIndex.First(key); index >= 0;
       index = hashIndex.Next(index)) {
    if (index < list.Num() &&
        idStr::Icmp(list[index].animRef.str, animationName) == 0)
      return &list[index];
  }
  return nullptr;
}

void InsertEventSorted(idList<const idMD6AnimEvent*, 5>& events,
                       const idMD6AnimEvent* event) {
  if (event == nullptr || events.IsFull())
    return;
  int index = 0;
  while (index < events.Num()) {
    const idMD6AnimEvent* current = events[index];
    if (event->GetFrameNum() < current->GetFrameNum() ||
        (event->GetFrameNum() == current->GetFrameNum() &&
         event->GetRow() < current->GetRow()))
      break;
    ++index;
  }
  events.Insert(event, index);
}

void MergeEvents(const idList<const idMD6AnimEvent*, 5>& newEvents,
                 idList<const idMD6AnimEvent*, 5>& events) {
  for (int newIndex = 0; newIndex < newEvents.Num(); ++newIndex) {
    const idMD6AnimEvent* candidate = newEvents[newIndex];
    bool overloaded = false;
    for (int existing = 0; existing < events.Num(); ++existing) {
      const idMD6AnimEvent* current = events[existing];
      if (current->GetFrameNum() == candidate->GetFrameNum() &&
          current->GetRow() == candidate->GetRow()) {
        overloaded = true;
        break;
      }
    }
    if (!overloaded)
      InsertEventSorted(events, candidate);
  }
}

bool idAnimEvents::FindAnimEvents(
    const idAtomicString& animationName, int startFrame, int endFrame,
    idList<const idMD6AnimEvent*, 5>& events) const {
  events.Clear();
  const animEventInfo_t* info = Find(animationName.str);
  if (info == nullptr)
    return false;
  for (int index = 0; index < info->events.Num() && !events.IsFull(); ++index) {
    const idMD6AnimEvent& event = info->events[index];
    if (FrameIsInRange(event.GetFrameNum(), startFrame, endFrame))
      InsertEventSorted(events, &event);
  }
  return events.Num() > 0;
}

bool idAnimEvents::FindAnimEvents(
    const idAtomicString& animationName, int startFrame, int endFrame,
    int eventNum, idList<const idMD6AnimEvent*, 5>& events) const {
  events.Clear();
  const animEventInfo_t* info = Find(animationName.str);
  if (info == nullptr)
    return false;
  for (int index = 0; index < info->events.Num() && !events.IsFull(); ++index) {
    const idMD6AnimEvent& event = info->events[index];
    if (event.GetEventNum() == eventNum &&
        FrameIsInRange(event.GetFrameNum(), startFrame, endFrame))
      InsertEventSorted(events, &event);
  }
  return events.Num() > 0;
}

bool idAnimEvents::animEventInfo_t::GetCachedJoints(
    int eventNum, idList<const idCachedJoint*, 5>& joints) const {
  joints.Clear();
  for (int index = 0; index < events.Num() && !joints.IsFull(); ++index) {
    const idMD6AnimEvent& event = events[index];
    if (event.GetEventNum() == eventNum && event.GetCachedJoint() != nullptr)
      joints.Append(event.GetCachedJoint());
  }
  return joints.Num() > 0;
}

bool idAnimEvents::GetCachedJoints(
    int eventNum, const idMD6Anim* animation,
    idList<const idCachedJoint*, 5>& joints) const {
  joints.Clear();
  const animEventInfo_t* info =
      animation != nullptr ? Find(animation->name.str) : nullptr;
  return info != nullptr && info->GetCachedJoints(eventNum, joints);
}

void idAnimEvents::Parse(const idDeclMD6* declaration, idParser& source,
                         int& loadErrors) {
  idToken token;
  if (!source.ExpectTokenType(TT_PUNCTUATION, 46, token))
    return;
  while (source.ReadToken(token)) {
    if (token.type == TT_PUNCTUATION && token.subtype == 47)
      break;
    if (token.Icmp("anim") != 0) {
      source.Error("Unknown token '%s' in anim events block", token.data);
      continue;
    }
    if (!source.ExpectTokenType(TT_STRING, 0, token))
      break;
    idStr animationName(token.data);
    animationName.ToLower();
    animEventInfo_t& info = FindOrAdd(animationName.data);
    if (!source.ExpectTokenType(TT_PUNCTUATION, 46, token))
      break;
    while (source.ReadToken(token)) {
      if (token.type == TT_PUNCTUATION && token.subtype == 47)
        break;
      if (token.Icmp("event") != 0) {
        source.Error("Unknown token '%s' in animation event list",
                     token.data);
        continue;
      }
      idMD6AnimEvent* event = info.events.Alloc();
      if (event == nullptr ||
          !event->Parse(declaration, source, loadErrors))
        info.events.RemoveIndex(info.events.Num() - 1);
    }
  }
}

bool idAnimEvents::CacheTransforms(const idDeclMD6* declaration) {
  if (transformCacheCallback == nullptr)
    return true;
  for (int infoIndex = 0; infoIndex < list.Num(); ++infoIndex) {
    animEventInfo_t& info = list[infoIndex];
    for (int eventIndex = 0; eventIndex < info.events.Num(); ++eventIndex) {
      if (!transformCacheCallback(declaration, info.animRef.str,
                                  info.events[eventIndex]))
        return false;
    }
  }
  return true;
}

void idAnimEvents::PinStreamedSound(int eventNum, int maxFrame) const {
  if (soundPinCallback == nullptr)
    return;
  for (int infoIndex = 0; infoIndex < list.Num(); ++infoIndex) {
    const animEventInfo_t& info = list[infoIndex];
    for (int eventIndex = 0; eventIndex < info.events.Num(); ++eventIndex) {
      const idMD6AnimEvent& event = info.events[eventIndex];
      if (event.GetEventNum() != eventNum || event.GetFrameNum() > maxFrame ||
          event.GetArgs().GetArgType(0) != ARGTYPE_POINTER)
        continue;
      const void* resource = nullptr;
      const void* defaultResource = nullptr;
      if (event.GetArgs().GetArg(0, resource, defaultResource) &&
          resource != nullptr)
        soundPinCallback(resource);
    }
  }
}

void idAnimWebPath::SplitPath(const char* path, idStr& webName,
                              idStr& subWebName, idStr& stateName) {
  webName.Empty();
  subWebName.Empty();
  stateName.Empty();
  if (path == nullptr || *path == '\0')
    return;

  const int length = static_cast<int>(std::strlen(path));
  if (path[length - 1] == '/') {
    webName = path;
    return;
  }

  int lastSlash = -1;
  for (int index = length - 1; index >= 0; --index) {
    if (path[index] == '/') {
      lastSlash = index;
      break;
    }
  }
  if (lastSlash < 0) {
    stateName = path;
    return;
  }

  int previousSlash = -1;
  for (int index = lastSlash - 1; index >= 0; --index) {
    if (path[index] == '/') {
      previousSlash = index;
      break;
    }
  }
  if (previousSlash >= 0) {
    webName.Append(path, previousSlash);
    subWebName.Append(path + previousSlash + 1,
                      lastSlash - previousSlash - 1);
  } else {
    subWebName.Append(path, lastSlash);
  }
  stateName.Append(path + lastSlash + 1, length - lastSlash - 1);
}

void idAnimWebPath::GetWebName(idStr& webName) const {
  idStr subWebName;
  idStr stateName;
  SplitPath(data, webName, subWebName, stateName);
}

void idAnimWebPath::GetWebPathNames(idStr& webName, idStr& subWebName,
                                    idStr& stateName) const {
  SplitPath(data, webName, subWebName, stateName);
}

void idAnimWebPath::SetPath(const char* webName, const char* subWebName,
                            const char* stateName) {
  Empty();
  if (webName != nullptr && *webName != '\0') {
    Append(webName);
    Append('/');
  }
  if (subWebName != nullptr && *subWebName != '\0') {
    Append(subWebName);
    Append('/');
  }
  Append(stateName != nullptr ? stateName : "");
}

const char* idDeclMD6::DefaultDefinition() const { return "{}"; }

idDeclInfo* idDeclMD6::GetDeclInfo() const {
  return declRuntimeCallbacks.getDeclInfo != nullptr
             ? declRuntimeCallbacks.getDeclInfo()
             : nullptr;
}

idAliasHandle_t idDeclMD6::StrongLoadAlias(const char* aliasName) const {
  return GlobalAliasHandle(aliasName, true);
}

bool idDeclMD6::GetJointsForAnimTime(idJointMat* joints,
                                     const idMD6Anim* anim, int timeMS,
                                     const idVec3& offset,
                                     bool removeOriginOffset) const {
  return model != nullptr && declRuntimeCallbacks.getJointsForAnimTime != nullptr
             ? declRuntimeCallbacks.getJointsForAnimTime(
                   model, joints, anim, timeMS, offset, removeOriginOffset)
             : false;
}

idAliasHandle_t idDeclMD6::FindAliasHandle(const char* aliasName) const {
  return GlobalAliasHandle(aliasName, false);
}

const char* idDeclMD6::FindAliasName(idAliasHandle_t aliasHandle) const {
  const auto& names = AliasNames();
  return aliasHandle.IsValid() && aliasHandle.value < names.size()
             ? names[aliasHandle.value].c_str()
             : nullptr;
}

const idMD6Alias* idDeclMD6::FindAlias(idAliasHandle_t aliasHandle,
                                       bool includeInherited,
                                       bool includeIncludes) const {
  if (!aliasHandle.IsValid())
    return nullptr;
  for (int index = aliases.Num() - 1; index >= 0; --index) {
    if (aliases[index].handle == aliasHandle.value)
      return &aliases[index].alias;
  }
  if (includeInherited && parentDecl != nullptr) {
    const idMD6Alias* inherited =
        parentDecl->FindAlias(aliasHandle, true, true);
    if (inherited != nullptr)
      return inherited;
  }
  if (includeIncludes) {
    const auto& includes = State(*this).includes;
    for (int index = 0; index < includes.Num(); ++index) {
      const idDeclMD6* declaration = includes[index];
      const idMD6Alias* included = declaration != nullptr
                                       ? declaration->FindAlias(
                                             aliasHandle, false, false)
                                       : nullptr;
      if (included != nullptr)
        return included;
    }
  }
  return nullptr;
}

const idMD6Alias* idDeclMD6::FindAlias(const char* aliasName,
                                       bool includeInherited,
                                       bool includeIncludes) const {
  const idAliasHandle_t handle = FindAliasHandle(aliasName);
  return handle.IsValid()
             ? FindAlias(handle, includeInherited, includeIncludes)
             : nullptr;
}

const idMD6Anim* idDeclMD6::AnimForAlias(idAliasHandle_t aliasHandle,
                                         bool load) const {
  if (!aliasHandle.IsValid())
    return nullptr;
  const idMD6Alias* alias = FindAlias(aliasHandle, true, true);
  if (alias != nullptr && alias->mappings.Num() > 0)
    return alias->mappings[0].anim;
  const char* aliasName = FindAliasName(aliasHandle);
  return load && aliasName != nullptr && declRuntimeCallbacks.loadAnimation
             ? declRuntimeCallbacks.loadAnimation(aliasName)
             : nullptr;
}

idAliasHandle_t idDeclMD6::AddAlias(const idMD6Alias& inAlias,
                                    bool overload) {
  const auto inheritedEqual = [&inAlias](const idMD6Alias* other) {
    if (other == nullptr || inAlias.mappings.Num() != other->mappings.Num() ||
        !SameText(inAlias.name.str, other->name.str))
      return false;
    for (int index = 0; index < inAlias.mappings.Num(); ++index) {
      if (!SameText(inAlias.mappings[index].animRef.str,
                    other->mappings[index].animRef.str))
        return false;
    }
    return true;
  };

  if (!overload) {
    if (parentDecl != nullptr) {
      const idMD6Alias* inherited =
          parentDecl->FindAlias(inAlias.name.str, true, true);
      if (inheritedEqual(inherited))
        return idAliasHandle_t(inherited->aliasHandle);
    }
    const auto& includes = State(*this).includes;
    for (int index = 0; index < includes.Num(); ++index) {
      const idMD6Alias* inherited =
          includes[index] != nullptr
              ? includes[index]->FindAlias(inAlias.name.str, true, true)
              : nullptr;
      if (inheritedEqual(inherited))
        return idAliasHandle_t(inherited->aliasHandle);
    }
  }

  const idAliasHandle_t handle = GlobalAliasHandle(inAlias.name.str, true);
  aliasEntry_t entry;
  entry.handle = handle.value;
  entry.alias = inAlias;
  entry.alias.aliasHandle = handle.value;
  aliases.Append(entry);
  return handle;
}

void idDeclMD6::GetAliases(idList<const idMD6Alias*, 5>& list,
                           bool includeParent,
                           bool includeIncludes) const {
  const auto appendUnique = [&list](const idMD6Alias* alias) {
    if (alias == nullptr)
      return;
    for (int index = 0; index < list.Num(); ++index) {
      if (SameText(list[index]->name.str, alias->name.str))
        return;
    }
    list.Append(alias);
  };
  for (int index = 0; index < aliases.Num(); ++index)
    appendUnique(&aliases[index].alias);

  if (includeParent && parentDecl != nullptr) {
    idList<const idMD6Alias*, 5> inherited;
    parentDecl->GetAliases(inherited, true, true);
    for (int index = 0; index < inherited.Num(); ++index)
      appendUnique(inherited[index]);
  }
  if (includeIncludes) {
    const auto& includes = State(*this).includes;
    for (int includeIndex = 0; includeIndex < includes.Num(); ++includeIndex) {
      if (includes[includeIndex] == nullptr)
        continue;
      idList<const idMD6Alias*, 5> included;
      includes[includeIndex]->GetAliases(included, false, false);
      for (int aliasIndex = 0; aliasIndex < included.Num(); ++aliasIndex)
        appendUnique(included[aliasIndex]);
    }
  }
}

void idDeclMD6::IncludeAnimDataFrom(const idDeclMD6* includeDecl) {
  if (includeDecl == nullptr)
    return;
  auto& includes = State(*this).includes;
  const auto append = [this, &includes](const idDeclMD6* declaration) {
    if (declaration != nullptr && declaration != this &&
        declaration != parentDecl && !ContainsPointer(includes, declaration))
      includes.Append(declaration);
  };
  append(includeDecl);
  append(includeDecl->parentDecl);
  const auto& nested = State(*includeDecl).includes;
  for (int index = 0; index < nested.Num(); ++index)
    append(nested[index]);
}

bool idDeclMD6::IsChildOf(const idDeclMD6* candidate) const {
  for (const idDeclMD6* current = parentDecl; current != nullptr;
       current = current->parentDecl) {
    if (current == candidate)
      return true;
  }
  const auto& includes = State(*this).includes;
  for (int index = 0; index < includes.Num(); ++index) {
    for (const idDeclMD6* current = includes[index]; current != nullptr;
         current = current->parentDecl) {
      if (current == candidate)
        return true;
    }
  }
  return false;
}

int idDeclMD6::NumJoints() const {
  return model != nullptr && model->skeleton != nullptr
             ? model->skeleton->NumJoints()
             : 0;
}

const char* idDeclMD6::GetJointName(idJointIndex_t index) const {
  if (model == nullptr || model->skeleton == nullptr || index.value < 0 ||
      index.value >= model->skeleton->NumJoints())
    return nullptr;
  return model->skeleton->GetJointName(index.value);
}

void idDeclMD6::GetJointList(const char* jointNames,
                             idList<idJointIndex_t, 5>& jointList) const {
  if (model == nullptr || model->skeleton == nullptr || jointNames == nullptr)
    return;
  model->skeleton->GetJointList(model, jointNames, jointList);
}

int idDeclMD6::GetUserChannelWeightGroup() const {
  for (const idDeclMD6* declaration = this; declaration != nullptr;
       declaration = declaration->parentDecl) {
    if (declaration->userChannelWeightGroupOverride >= 0 &&
        declaration->userChannelWeightGroupOverride < MD6_WEIGHTGROUP_MAX)
      return declaration->userChannelWeightGroupOverride;
  }
  return MD6_WEIGHTGROUP_ALL;
}

const idVarArgs<6>* idDeclMD6::FindUserProp(const char* propName) const {
  const auto& props = State(*this).userProps;
  for (int index = 0; index < props.Num(); ++index) {
    const char* storedName = nullptr;
    if (props[index].GetArgType(0) == ARGTYPE_STRING)
      props[index].GetArg(0, storedName, static_cast<const char*>(nullptr));
    if (SameText(propName, storedName))
      return &props[index];
  }
  return nullptr;
}

void idDeclMD6::ReadUserProp(idParser& source, idVarArgs<6>& prop) {
  prop.ClearArgs();
  idToken token;
  if (!source.ReadToken(token))
    return;
  prop.AddArg(token.data);
  if (!source.ExpectTokenType(TT_PUNCTUATION, 46, token))
    return;
  while (prop.GetNumArgs() < 6 && source.ReadToken(token)) {
    if (token.type == TT_PUNCTUATION && token.subtype == 47)
      break;
    if (token.Icmp("bool") == 0) {
      prop.AddArg(source.ParseBool());
    } else if (token.Icmp("int") == 0) {
      prop.AddArg(source.ParseInt());
    } else if (token.Icmp("float") == 0) {
      prop.AddArg(source.ParseFloat());
    } else if (token.Icmp("vec3") == 0) {
      idVec3 value{};
      source.Parse1DMatrix(3, &value.x);
      prop.AddArg(value);
    } else if (token.Icmp("vec4") == 0) {
      idVec4 value{};
      source.Parse1DMatrix(4, &value.x);
      prop.AddArg(value);
    } else if (token.Icmp("quat") == 0) {
      idQuat value{};
      source.Parse1DMatrix(4, &value.x);
      prop.AddArg(value);
    } else if (token.Icmp("angles") == 0) {
      idAngles value{};
      source.Parse1DMatrix(3, &value.pitch);
      prop.AddArg(value);
    } else if (token.Icmp("string") == 0) {
      if (source.ReadToken(token))
        prop.AddArg(token.data);
    } else {
      source.Error("Unknown user property type '%s'", token.data);
      source.SkipRestOfLine();
    }
  }
}

int idDeclMD6::GetHeadTrackGroupIndex(const char* groupName) const {
  const auto& groups = State(*this).headTrackGroupNames;
  for (int index = 0; index < groups.Num(); ++index) {
    if (SameText(groups[index].str, groupName))
      return index;
  }
  return -1;
}

bool idDeclMD6::GetRandomValidMeshKits(
    idAtomicString (&meshKits)[3], idRandom2& random) const {
  const idDeclMD6* declaration = this;
  while (declaration != nullptr &&
         State(*declaration).validRandomMeshKits.Num() == 0)
    declaration = declaration->parentDecl;
  if (declaration == nullptr)
    return false;

  const auto& sets = State(*declaration).validRandomMeshKits;
  random.seed = 1664525u * random.seed + 1013904223u;
  const int setIndex =
      static_cast<int>((random.seed >> 10u) & 0x7fffu) % sets.Num();
  for (int category = 0; category < 3; ++category) {
    const auto& names = sets[setIndex].kitNames[category];
    if (names.Num() <= 0)
      continue;
    random.seed = 1664525u * random.seed + 1013904223u;
    const int nameIndex =
        static_cast<int>((random.seed >> 10u) & 0x7fffu) % names.Num();
    meshKits[category] = names[nameIndex];
  }
  return true;
}

int idDeclMD6::NumMeshKits(int component) const {
  return component >= 0 && component < 3
             ? State(*this).meshKitGroups[component].Num()
             : 0;
}

const char* idDeclMD6::GetMeshKitName(int component, int kitIndex) const {
  if (component < 0 || component >= 3)
    return nullptr;
  const auto& kits = State(*this).meshKitGroups[component];
  return kitIndex >= 0 && kitIndex < kits.Num() ? kits[kitIndex].name.data
                                                : nullptr;
}

const idList<int, 19>*
idDeclMD6::GetMeshKitMeshes(int component, int kitIndex) const {
  if (component < 0 || component >= 3)
    return nullptr;
  const auto& kits = State(*this).meshKitGroups[component];
  return kitIndex >= 0 && kitIndex < kits.Num()
             ? &kits[kitIndex].meshIndices
             : nullptr;
}

void idDeclMD6::AddMeshKit(int component, const char* kitName,
                           const idList<int, 19>& meshIndices) {
  if (component < 0 || component >= 3)
    return;
  auto& kits = State(*this).meshKitGroups[component];
  for (int index = 0; index < kits.Num(); ++index) {
    if (SameText(kits[index].name.data, kitName)) {
      kits[index].meshIndices = meshIndices;
      return;
    }
  }
  idDeclMD6RecoveredState::meshKit_t kit;
  kit.name = kitName != nullptr ? kitName : "";
  kit.meshIndices = meshIndices;
  kits.Append(kit);
}

const char* idDeclMD6::GetMeshKitDefaultOverride(int component) const {
  return component >= 0 && component < 3
             ? State(*this).meshKitDefaultOverride[component].data
             : "";
}

void idDeclMD6::SetMeshKitDefaultOverride(int component,
                                          const char* kitName) {
  if (component >= 0 && component < 3)
    State(*this).meshKitDefaultOverride[component] =
        kitName != nullptr ? kitName : "";
}

const char* idDeclMD6::GetDefaultSkinOverride() const {
  return State(*this).defaultSkinOverride.data;
}

void idDeclMD6::SetDefaultSkinOverride(const char* skinName) {
  State(*this).defaultSkinOverride = skinName != nullptr ? skinName : "";
}

int idDeclMD6::NumWrinkleMapRegionExpressions() const {
  return State(*this).wrinkleMapRegionExpressions.Num();
}

const char* idDeclMD6::GetWrinkleMapRegionExpression(int index) const {
  const auto& expressions = State(*this).wrinkleMapRegionExpressions;
  return index >= 0 && index < expressions.Num() ? expressions[index].data
                                                  : nullptr;
}

void idDeclMD6::AddWrinkleMapRegionExpression(const char* expression) {
  State(*this).wrinkleMapRegionExpressions.Append(
      idStr(expression != nullptr ? expression : ""));
}

int idDeclMD6::NumSkipUseHintMeshes() const {
  return State(*this).skipUseHintMeshes.Num();
}

int idDeclMD6::GetSkipUseHintMesh(int index) const {
  const auto& meshes = State(*this).skipUseHintMeshes;
  return index >= 0 && index < meshes.Num() ? meshes[index] : -1;
}

void idDeclMD6::AddSkipUseHintMesh(int meshIndex) {
  State(*this).skipUseHintMeshes.Append(meshIndex);
}

idJointGroupCollection* idDeclMD6::GetJointGroupCollection() const {
  return const_cast<idJointGroupCollection*>(&State(*this).jointGroups);
}

idPropsCollection* idDeclMD6::GetPropsCollection() const {
  return const_cast<idPropsCollection*>(&State(*this).props);
}

void idDeclMD6::PinStreamedSound(int eventNum, int maxFrame) const {
  State(*this).animEvents.PinStreamedSound(eventNum, maxFrame);
  for (const idDeclMD6* declaration = parentDecl; declaration != nullptr;
       declaration = declaration->parentDecl)
    State(*declaration).animEvents.PinStreamedSound(eventNum, maxFrame);
  const auto& includes = State(*this).includes;
  for (int index = 0; index < includes.Num(); ++index) {
    for (const idDeclMD6* declaration = includes[index]; declaration != nullptr;
         declaration = declaration->parentDecl)
      State(*declaration).animEvents.PinStreamedSound(eventNum, maxFrame);
  }
}

bool idDeclMD6::FindAnimEvents(
    const idMD6Anim* anim, int startFrame, int endFrame,
    idList<const idMD6AnimEvent*, 5>& events) const {
  events.Clear();
  if (anim == nullptr)
    return false;
  State(*this).animEvents.FindAnimEvents(anim->name, startFrame, endFrame,
                                         events);
  const auto mergeDeclaration = [&](const idDeclMD6* declaration) {
    idList<const idMD6AnimEvent*, 5> current;
    State(*declaration)
        .animEvents.FindAnimEvents(anim->name, startFrame, endFrame, current);
    MergeEvents(current, events);
  };
  for (const idDeclMD6* declaration = parentDecl; declaration != nullptr;
       declaration = declaration->parentDecl)
    mergeDeclaration(declaration);
  const auto& includes = State(*this).includes;
  for (int index = 0; index < includes.Num(); ++index) {
    for (const idDeclMD6* declaration = includes[index]; declaration != nullptr;
         declaration = declaration->parentDecl)
      mergeDeclaration(declaration);
  }
  return events.Num() != 0;
}

bool idDeclMD6::FindAnimEvents(
    const idMD6Anim* anim, int startFrame, int endFrame, int eventNum,
    idList<const idMD6AnimEvent*, 5>& events) const {
  events.Clear();
  if (anim == nullptr)
    return false;
  State(*this).animEvents.FindAnimEvents(anim->name, startFrame, endFrame,
                                         eventNum, events);
  const auto mergeDeclaration = [&](const idDeclMD6* declaration) {
    idList<const idMD6AnimEvent*, 5> current;
    State(*declaration)
        .animEvents.FindAnimEvents(anim->name, startFrame, endFrame, eventNum,
                                   current);
    MergeEvents(current, events);
  };
  for (const idDeclMD6* declaration = parentDecl; declaration != nullptr;
       declaration = declaration->parentDecl)
    mergeDeclaration(declaration);
  const auto& includes = State(*this).includes;
  for (int index = 0; index < includes.Num(); ++index) {
    for (const idDeclMD6* declaration = includes[index]; declaration != nullptr;
         declaration = declaration->parentDecl)
      mergeDeclaration(declaration);
  }
  return events.Num() != 0;
}

bool idDeclMD6::GetCachedJoints(
    int eventNum, const idMD6Anim* anim,
    idList<const idCachedJoint*, 5>& joints) const {
  joints.Clear();
  if (anim == nullptr)
    return false;
  idList<const idCachedJoint*, 5> current;
  State(*this).animEvents.GetCachedJoints(eventNum, anim, current);
  for (int index = 0; index < current.Num(); ++index)
    AppendUniqueJoint(joints, current[index]);
  const auto appendDeclaration = [&](const idDeclMD6* declaration) {
    current.Clear();
    State(*declaration).animEvents.GetCachedJoints(eventNum, anim, current);
    for (int index = 0; index < current.Num(); ++index)
      AppendUniqueJoint(joints, current[index]);
  };
  for (const idDeclMD6* declaration = parentDecl; declaration != nullptr;
       declaration = declaration->parentDecl)
    appendDeclaration(declaration);
  const auto& includes = State(*this).includes;
  for (int index = 0; index < includes.Num(); ++index) {
    for (const idDeclMD6* declaration = includes[index]; declaration != nullptr;
         declaration = declaration->parentDecl)
      appendDeclaration(declaration);
  }
  return joints.Num() != 0;
}

int idDeclMD6::NumMorphSets() const { return State(*this).morphSets.Num(); }

const idMD6MorphSet* idDeclMD6::GetMorphSet(int index) const {
  const auto& sets = State(*this).morphSets;
  return index >= 0 && index < sets.Num() ? &sets[index] : nullptr;
}

bool idDeclMD6::HasMorphSets() const { return NumMorphSets() > 0; }

idAnimEvents& idDeclMD6::AnimEvents() { return State(*this).animEvents; }

const idAnimEvents& idDeclMD6::AnimEvents() const {
  return State(*this).animEvents;
}

void idDeclMD6::AddUserProp(const idVarArgs<6>& prop) {
  State(*this).userProps.Append(prop);
}

void idDeclMD6::AddHeadTrackGroupName(const char* groupName) {
  State(*this).headTrackGroupNames.Append(idAtomicString(groupName));
}

void idDeclMD6::AddValidMeshKitSet(
    const idList<idAtomicString, 5> (&kits)[3]) {
  validMeshKitSet_t set;
  for (int index = 0; index < 3; ++index)
    set.kitNames[index] = kits[index];
  State(*this).validRandomMeshKits.Append(set);
}

void idDeclMD6::SetMorphSets(const idList<idMD6MorphSet, 5>& sets) {
  State(*this).morphSets = sets;
}

void idDeclMD6::SetUserChannelAliasOverride(idUserChannelIndex channel,
                                            idAliasHandle_t alias) {
  auto& overrides = State(*this).userChannelAliasOverrides;
  for (int index = 0; index < overrides.Num(); ++index) {
    if (overrides[index].channel.value == channel.value) {
      overrides[index].alias = alias;
      return;
    }
  }
  idDeclMD6RecoveredState::userChannelAliasOverride_t value;
  value.channel = channel;
  value.alias = alias;
  overrides.Append(value);
}

idAliasHandle_t
idDeclMD6::GetUserChannelAlias(idUserChannelIndex channel) const {
  const auto& map = State(*this).userChannelAliasMap;
  return channel.value >= 0 && channel.value < map.Num() ? map[channel.value]
                                                         : NULL_ALIAS;
}

void idDeclMD6::InitUserChannelToAnimationAliasMap() {
  auto& state = State(*this);
  const int parentCount =
      parentDecl != nullptr
          ? State(*parentDecl).userChannelAliasMap.Num()
          : 0;
  int channelCount =
      model != nullptr && model->skeleton != nullptr &&
              declRuntimeCallbacks.getNumUserChannels != nullptr
          ? declRuntimeCallbacks.getNumUserChannels(model->skeleton)
          : parentCount;
  for (int index = 0; index < state.userChannelAliasOverrides.Num(); ++index)
    channelCount = std::max(
        channelCount,
        static_cast<int>(state.userChannelAliasOverrides[index].channel.value) +
            1);
  state.userChannelAliasMap.SetNum(channelCount, NULL_ALIAS);
  for (int channel = 0; channel < channelCount; ++channel) {
    idAliasHandle_t alias =
        channel < parentCount
            ? State(*parentDecl).userChannelAliasMap[channel]
            : NULL_ALIAS;
    for (int index = 0; index < state.userChannelAliasOverrides.Num();
         ++index) {
      if (state.userChannelAliasOverrides[index].channel.value == channel) {
        alias = state.userChannelAliasOverrides[index].alias;
        break;
      }
    }
    state.userChannelAliasMap[channel] = alias;
  }
}

void idDeclMD6::InitHeadTrackGroups() {
  if (declRuntimeCallbacks.initHeadTrackGroups != nullptr)
    declRuntimeCallbacks.initHeadTrackGroups(this);
}

void idDeclMD6::Parse(idParser& source) {
  FreeData();
  idToken token;
  if (!source.ExpectTokenType(TT_PUNCTUATION, 46, token))
    return;
  int loadErrors = 0;
  while (source.ReadToken(token)) {
    if (token.type == TT_PUNCTUATION && token.subtype == 47)
      break;
    if (token.Icmp("parent") == 0 || token.Icmp("inherit") == 0) {
      if (source.ReadToken(token) &&
          declRuntimeCallbacks.findDeclaration != nullptr)
        parentDecl = declRuntimeCallbacks.findDeclaration(token.data);
    } else if (token.Icmp("includeAnimDataFrom") == 0) {
      if (source.ReadToken(token) &&
          declRuntimeCallbacks.findDeclaration != nullptr)
        IncludeAnimDataFrom(
            declRuntimeCallbacks.findDeclaration(token.data));
    } else if (token.Icmp("model") == 0) {
      if (source.ReadToken(token) && declRuntimeCallbacks.loadModel != nullptr)
        model = declRuntimeCallbacks.loadModel(token.data);
    } else if (token.Icmp("userChannelWeightGroup") == 0) {
      userChannelWeightGroupOverride = source.ParseInt();
    } else if (token.Icmp("userProp") == 0) {
      idVarArgs<6> prop;
      ReadUserProp(source, prop);
      AddUserProp(prop);
    } else if (token.Icmp("headTrackGroup") == 0) {
      if (source.ReadToken(token))
        AddHeadTrackGroupName(token.data);
    } else if (token.Icmp("animEvents") == 0 || token.Icmp("events") == 0) {
      AnimEvents().Parse(this, source, loadErrors);
    } else if (token.Icmp("alias") == 0 ||
               token.Icmp("overloadAlias") == 0) {
      const bool overload = token.Icmp("overloadAlias") == 0;
      idMD6Alias alias;
      if (!source.ExpectTokenType(TT_PUNCTUATION, 46, token))
        continue;
      while (source.ReadToken(token)) {
        if (token.type == TT_PUNCTUATION && token.subtype == 47)
          break;
        if (token.Icmp("name") == 0) {
          if (source.ReadToken(token))
            alias.name.Set(token.data);
        } else if (token.Icmp("anim") == 0) {
          if (source.ReadToken(token)) {
            idMD6AliasEntry mapping;
            mapping.animRef.Set(token.data);
            mapping.anim = nullptr;
            alias.mappings.Append(mapping);
          }
        } else if (token.Icmp("flags") == 0) {
          if (!source.ExpectTokenType(TT_PUNCTUATION, 46, token))
            continue;
          while (source.ReadToken(token)) {
            if (token.type == TT_PUNCTUATION && token.subtype == 47)
              break;
            if (token.Icmp("forceLoad") == 0)
              alias.flags |= 1u;
          }
        } else {
          source.Error("Unknown alias token '%s'", token.data);
        }
      }
      if (alias.name.str != nullptr && *alias.name.str != '\0')
        AddAlias(alias, overload);
    } else {
      source.Error("Unknown token '%s' in md6 declaration", token.data);
      if (source.PeekTokenType(TT_PUNCTUATION, 46, token))
        source.SkipBracedSection(true);
      else
        source.SkipRestOfLine();
    }
  }
  InitUserChannelToAnimationAliasMap();
  AnimEvents().CacheTransforms(this);
  InitHeadTrackGroups();
}

void idDeclMD6::FreeData() {
  model = nullptr;
  parentDecl = nullptr;
  aliases.ClearFree();
  userChannelWeightGroupOverride = MD6_WEIGHTGROUP_MAX;
  recoveredState.reset();
}
