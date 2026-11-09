#include "engine/models/skeletalanimation/eyeinfocollection.h"

#include "engine/models/skeletalanimation/md6util.h"
#include "shared/idlib/sys/sys_alloc.h"
#include "shared/idlib/text/parser.h"

#include <algorithm>
#include <new>

extern bool forceIdListsToReallocateEveryAppend;

const char* idEyeInfo::eyeNames[idEyeInfo::EYE_MAX] = {"leftEye",
                                                       "rightEye"};
const char* idEyeInfoCollection::DEFAULT_NAME = "default";

namespace {

constexpr eyeInfoHandle_t NULL_EYE_INFO{0xffff};

} // namespace

int idEyeInfoCollection::FindEyeInfoIndex(const char* name) const {
  for (int index = 0; index < eyeInfos.Num(); ++index) {
    if (idStr::Icmp(eyeInfos[index].name.data, name) == 0)
      return index;
  }
  return -1;
}

idEyeInfo::idEyeInfo()
    : name(), handle{0xffff}, basisJointIndex(-1), minEyePitch(-16.0f),
      maxEyePitch(16.0f), minEyeYaw(-23.0f), maxEyeYaw(23.0f),
      lidMinPitch(-15.0f), lidMaxPitch(15.0f), lidMinYaw(-21.0f),
      lidMaxYaw(21.0f), perEyeInfo{} {}

void idEyeInfo::Parse(const idDeclMD6* declmd6, idParser& source,
                      int& loadErrors) {
  (void)loadErrors;
  idToken token;
  source.ExpectTokenType(TT_STRING, 0, token);
  name = token;
  source.ExpectTokenType(TT_PUNCTUATION, 46, token);

  while (source.ReadToken(token)) {
    if (token.type == TT_PUNCTUATION && token.subtype == 47)
      break;
    if (token == "basisJoint") {
      source.ExpectTokenType(TT_STRING, 0, token);
      basisJointIndex =
          declmd6 != nullptr && declmd6->model != nullptr &&
                  declmd6->model->skeleton != nullptr
              ? declmd6->model->skeleton->GetJointIndex(token.data)
              : idJointIndex_t(-1);
    } else if (token == "minEyePitch") {
      minEyePitch = source.ParseFloat();
    } else if (token == "maxEyePitch") {
      maxEyePitch = source.ParseFloat();
    } else if (token == "minEyeYaw") {
      minEyeYaw = source.ParseFloat();
    } else if (token == "maxEyeYaw") {
      maxEyeYaw = source.ParseFloat();
    } else if (token == "lidMinPitch") {
      lidMinPitch = source.ParseFloat();
    } else if (token == "lidMaxPitch") {
      lidMaxPitch = source.ParseFloat();
    } else if (token == "lidMinYaw") {
      lidMinYaw = source.ParseFloat();
    } else if (token == "lidMaxYaw") {
      lidMaxYaw = source.ParseFloat();
    } else if (token.type == TT_NAME) {
      int eye = 0;
      while (eye < EYE_MAX && idStr::Icmp(token.data, eyeNames[eye]) != 0)
        ++eye;
      if (eye >= EYE_MAX) {
        source.Error("Unknown token '%s' in modeldef '%s' eyeInfo '%s'",
                     token.data, declmd6->name.str, name.data);
        continue;
      }

      source.ExpectTokenType(TT_PUNCTUATION, 46, token);
      while (source.ReadToken(token)) {
        if (token.type == TT_PUNCTUATION && token.subtype == 47)
          break;
        perEyeInfo_t& info = perEyeInfo[eye];
        if (token == "focusScale")
          info.focusScale = source.ParseFloat();
        else if (token == "wanderRate")
          info.wanderRate = source.ParseFloat();
        else if (token == "maxWanderPitch")
          info.maxWanderPitch = source.ParseFloat();
        else if (token == "maxWanderYaw")
          info.maxWanderYaw = source.ParseFloat();
        else if (token == "jitterScale")
          info.jitterScale = source.ParseFloat();
        else if (token == "slerpScale")
          info.slerpScale = source.ParseFloat();
        else if (token == "angleOffset")
          source.Parse1DMatrix(3, &info.angleOffset.pitch);
        else
          source.Error("Unknown token '%s' in modeldef '%s' eyeInfo '%s'",
                       token.data, declmd6->name.str, name.data);
      }
    } else {
      source.Error("Unknown token '%s' in modeldef '%s' eyeInfo '%s'",
                   token.data, declmd6->name.str, name.data);
    }
  }
}

const idEyeInfo*
idEyeInfoCollection::GetEyeInfo(const eyeInfoHandle_t& handle) const {
  for (int index = 0; index < eyeInfos.Num(); ++index) {
    if (eyeInfos[index].handle == handle)
      return &eyeInfos[index];
  }
  return nullptr;
}

eyeInfoHandle_t
idEyeInfoCollection::FindEyeInfoHandle(const char* name) const {
  const int index = FindEyeInfoIndex(name);
  return index >= 0 ? eyeInfos[index].handle : NULL_EYE_INFO;
}

void idEyeInfoCollection::Free() {
  eyeInfos.ClearFree();
  curHandle = 0;
}

template <>
bool idList<idEyeInfo, 5>::Resize(int newSize) {
  if (newSize <= 0) {
    ClearFree();
    return true;
  }
  if (newSize == size || listStatic != 0)
    return true;

  idMem* memory = idMem::Singleton();
  auto* replacement = static_cast<idEyeInfo*>(memory->AllocWithLocation(
      "w:\\build\\tech5\\shared\\idlib\\containers\\List.h(29) : tag",
      sizeof(idEyeInfo) * static_cast<std::size_t>(newSize),
      static_cast<memTag_t>(memTag), false, ALIGN_16, HEAP_DEFAULTHEAP));
  if (replacement == nullptr)
    return false;
  for (int index = 0; index < newSize; ++index)
    ::new (static_cast<void*>(&replacement[index])) idEyeInfo;
  const int copyCount = std::min(num, newSize);
  for (int index = 0; index < copyCount; ++index)
    replacement[index] = list[index];
  if ((listStatic == 0 || listStatic == 2) && list != nullptr) {
    for (int index = 0; index < size; ++index)
      list[index].~idEyeInfo();
    memory->Free(list, ALIGN_16);
  }
  list = replacement;
  size = newSize;
  if (num > newSize)
    num = newSize;
  listStatic = 0;
  return true;
}

idEyeInfoCollection::idEyeInfoCollection() : curHandle(0), eyeInfos() {
  eyeInfos.ClearFree();
}

idEyeInfoCollection::~idEyeInfoCollection() = default;

template <>
int idList<idEyeInfo, 5>::Append(const idEyeInfo& object) {
  if (list == nullptr) {
    const int initialSize = granularity > 0 ? granularity : 1;
    if (!Resize(initialSize))
      return -1;
  }
  if (num == size) {
    int newSize;
    if (forceIdListsToReallocateEveryAppend) {
      newSize = size + 1;
    } else if (granularity != 0) {
      newSize = size + granularity - (size + granularity) % granularity;
    } else if (size <= 0x3fffffff) {
      newSize = size * 2;
      if (newSize == 0)
        newSize = 1;
    } else {
      newSize = 0x7fffffff;
    }
    if (!Resize(newSize))
      return -1;
  }
  if (num >= size)
    return num - 1;
  list[num] = object;
  return num++;
}

eyeInfoHandle_t idEyeInfoCollection::AddEyeInfo(const idEyeInfo& info) {
  eyeInfos.Append(info);
  const eyeInfoHandle_t handle{curHandle++};
  eyeInfos[eyeInfos.Num() - 1].handle = handle;
  return handle;
}

void idEyeInfoCollection::DuplicateInherited(
    const idDeclMD6* declmd6,
    const idEyeInfoCollection* parentCollection) {
  (void)declmd6;
  for (int index = 0; index < parentCollection->eyeInfos.Num(); ++index) {
    const idEyeInfo& inherited = parentCollection->eyeInfos[index];
    if (FindEyeInfoIndex(inherited.name.data) < 0)
      AddEyeInfo(inherited);
  }
}

void idEyeInfoCollection::Parse(const idDeclMD6* decl, idParser& source,
                                int& loadErrors) {
  const int expectedCount = source.ParseInt();
  if (expectedCount > eyeInfos.size)
    eyeInfos.ResizeForPort(expectedCount);
  idToken token;
  source.ExpectTokenType(TT_PUNCTUATION, 46, token);
  while (source.ReadToken(token)) {
    if (token.type == TT_PUNCTUATION && token.subtype == 47)
      break;
    if (token == "eyeInfo") {
      idEyeInfo info;
      info.Parse(decl, source, loadErrors);
      AddEyeInfo(info);
    } else {
      source.Error("Unknown token '%s' in model def '%s' in eye info collection.",
                   token.data, decl->name.str);
    }
  }
  eyeInfos.ResizeForPort(eyeInfos.Num());
}
