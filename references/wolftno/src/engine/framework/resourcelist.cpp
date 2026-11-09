#include "engine/framework/resourcelist.h"

#include "engine/framework/coderesource.h"
#include "engine/framework/common.h"
#include "shared/idlib/csystems/cvarsystemlocal.h"
#include "shared/idlib/hashing/hashing.h"
#include "shared/idlib/lib.h"
#include "shared/idlib/networking/bitmsg.h"
#include "shared/idlib/parallelism/thread.h"
#include "shared/idlib/sys/sys_time.h"
#include "shared/idlib/sys/sys_alloc.h"

#include <algorithm>
#include <cstdint>
#include <cstring>

idCVar resource_errorInGame(
    "resource_errorInGame", "0", CVAR_INTEGER,
    "0 - Nothing. 1 - Warning. 2 - Fatal Error - if loaded during gameplay");
idCVar resource_showLoads("resource_showLoads", "0", CVAR_INTEGER,
                          "1 = show all loads, 2 = show all finds");
idCVar resource_errorOnResolveFailure(
    "resource_errorOnResolveFailure", "1", CVAR_BOOL,
    "1 = errors on failure to resolve code resources, 0 = just warnings");
idCVar resource_errorOnClientAddNetResource(
    "resource_errorOnClientAddNetResource", "0", CVAR_BOOL,
    "Error on clients trying to add network resources");
idCVar resource_toolForceStaleChecks(
    "resource_toolForceStaleChecks", "1", CVAR_BOOL,
    "When idStudio is active, always perform stale checks on resources.");
idCVar resource_showStaleReloads(
    "resource_showStaleReloads", "0", CVAR_BOOL,
    "Prints to the console whenever a resource is stale-reloaded.");
idCVar resource_warnOnLoad(
    "resource_warnOnLoad", "", 0,
    "warn on load that sub string match on string (to find out who cause a resource to load)");
idCVar resource_trackLoadTimes(
    "resource_trackLoadTimes", "0", CVAR_BOOL,
    "1 = track the time spent on loading resources and basic statistics per resource list");

extern idCVar com_production;

idBaseCodeResource* idBaseCodeResource::codeResourceList = nullptr;
bool idBaseCodeResource::allocCodeResourcesHasBeenCalled = false;

int idResourceList::loadResourceRecursionLevel = 0;
idArray<idResource*, 32> idResourceList::recursiveLoads{};
int idResourceList::staleCount = 0;
bool idResourceList::inGameLoading = false;

namespace {

idResourceList* listOfResourceLists = nullptr;
idResourceLoadTimeTracker loadTimeTracker;
int breakHere = 0;

class ResourceListLock {
public:
  explicit ResourceListLock(idSysMutex* mutex_) : mutex(mutex_) {
    mutex->Lock(true);
  }
  ~ResourceListLock() { mutex->Unlock(); }

  idSysMutex* mutex;
};

std::uint32_t HashResourceName(const char* typeName, const char* canonicalName) {
  std::uint32_t hash = MurMur32_HashData(
      reinterpret_cast<std::uint8_t*>(const_cast<char*>(typeName)),
      static_cast<std::uint32_t>(idStr::Length(typeName)), 0x566F8AD7u);
  hash = MurMur32_HashData(
      reinterpret_cast<std::uint8_t*>(const_cast<char*>(canonicalName)),
      static_cast<std::uint32_t>(idStr::Length(canonicalName)), hash);
  return hash == 0 ? 1u : hash;
}

} // namespace

idSysMutex* idResourceList::Mutex() {
  static idSysMutex mutex;
  return &mutex;
}

idResourceList::idResourceList(const char* typeName)
    : resourceTypeName(typeName), nextResourceList(listOfResourceLists),
      resourceList(), resourceIDHash(256, 256) {
  resourceList.granularity = 0;
  resourceIDHash.memTag = TAG_HASHINDEX;
  listOfResourceLists = this;
}

const char* idResourceList::GetResourceTypeName() const {
  if (resourceTypeName == nullptr)
    idLib::FatalError("idResourceList::GetResourceTypeName: not set");
  return resourceTypeName;
}

idResourceList* idResourceList::ForTypeName(const char* typeName) {
  for (idResourceList* list = listOfResourceLists; list != nullptr;
       list = list->nextResourceList) {
    if (idStr::Icmp(list->GetResourceTypeName(), typeName) == 0)
      return list;
  }
  idLib::Printf("Unknown resource type '%s'\n", typeName);
  return nullptr;
}

idResourceList* idResourceList::GetNextResourceList() {
  return listOfResourceLists;
}

int idResourceList::GenerateResourceIDForName(const char* canonicalName) const {
  return static_cast<int>(HashResourceName(GetResourceTypeName(), canonicalName));
}

bool idResourceList::ReloadResourceIfStale(idResource* resource) {
  const char* const previousError = resource->resourceError;
  resource->SetResourceError(nullptr);
  if (resource->ReloadIfStale()) {
    if (resource_showStaleReloads.valueInteger != 0) {
      idLib::Printf("reloaded stale resource (%s) : '%s'\n",
                    resource->GetResourceTypeName(), resource->name.str);
    }
    return true;
  }
  resource->SetResourceError(previousError);
  return false;
}

bool idResourceList::CanPerformStaleCheck(const idResource* resource) {
  if (common->IsToolActive() && resource_toolForceStaleChecks.valueInteger != 0) {
    resource->staleCount = staleCount;
    return true;
  }
  if (resource->staleCount < staleCount) {
    resource->staleCount = staleCount;
    return true;
  }
  return false;
}

int idResourceList::GetResourceIndexForID(
    int resourceID, const char* furtherQualifyByName) const {
  if (resourceID == 0)
    return -1;
  for (int index = resourceIDHash.First(resourceID); index != -1;
       index = resourceIDHash.Next(index)) {
    idResource* const resource = resourceList[index];
    if (resource->resourceID != resourceID)
      continue;
    if (furtherQualifyByName == nullptr)
      return index;
    if (resource->name.str == nullptr ||
        idStr::Cmp(resource->name.str, furtherQualifyByName) == 0) {
      return index;
    }
    idLib::Warning(
        "GetResourceIndexForID: Resource [%s] %s had a resource hash collision with %s, and must be renamed",
        GetResourceTypeName(), resource->name.str, furtherQualifyByName);
  }
  return -1;
}

const idResource* idResourceList::GetResourceByIDInternal(int resourceID) const {
  if (resourceID == 0)
    return nullptr;
  for (int index = resourceIDHash.First(resourceID); index != -1;
       index = resourceIDHash.Next(index)) {
    if (resourceList[index]->resourceID == resourceID)
      return resourceList[index];
  }
  return nullptr;
}

const idResource* idResourceList::GetResourceByID(int resourceID) {
  return GetResourceByIDInternal(resourceID);
}

const idResource* idResourceList::ReadResourceFromMsg(idBitMsg& message) {
  const unsigned int resourceID =
      static_cast<unsigned int>(message.ReadBits(32));
  const idResource* const resource = GetResourceByID(resourceID);
  if (resourceID != 0 && resource == nullptr)
    idLib::Error("Invalid network resource ID %i", resourceID);
  return resource;
}

void idResourceList::WriteResourceToMsg(const idResource* resource,
                                        idBitMsg& message) {
  message.WriteBits(resource != nullptr ? resource->resourceID : 0, 32);
}

idResource* idResourceList::Lookup(const char* canonicalName) const {
  ResourceListLock lock(Mutex());
  const int resourceID = static_cast<int>(
      HashResourceName(GetResourceTypeName(), canonicalName));
  const int index = GetResourceIndexForID(resourceID, canonicalName);
  return index == -1 ? nullptr : resourceList[index];
}

idResource* idResourceList::FindExisting(const char* name,
                                         bool skipStaleCheck) const {
  const char* canonicalName = name;
  idStr canonicalConversion;
  if (!idStr::IsNameCanonical(name)) {
    canonicalConversion = name;
    canonicalConversion.MakeNameCanonical();
    canonicalName = canonicalConversion.data;
  }

  idResource* const resource = Lookup(canonicalName);
  if (resource == nullptr || !idLib::IsMainThread())
    return resource;

  if (resource_showLoads.valueInteger == 2) {
    for (int i = 0; i < loadResourceRecursionLevel; ++i)
      idLib::Printf(" ");
    idLib::Printf("idResourceList::Load( %s : %s )\n",
                  GetResourceTypeName(), canonicalName);
  }

  if (resource->insideRecursiveLoad)
    ++breakHere;

  bool performLoad = false;
  if (resource->buildForceLoad && resource->loadedResource) {
    performLoad = true;
    resource->buildForceLoad = false;
  }
  if (resource->loadDeferred &&
      (loadResourceRecursionLevel == 0 || resource->staticResource)) {
    resource->loadDeferred = false;
    resource->buildForceLoad = false;
    resource->loadedResource = true;
    performLoad = true;
  }

  if (performLoad) {
    recursiveLoads.ptr[loadResourceRecursionLevel++] = resource;
    try {
      resource->Load();
    } catch (...) {
      --loadResourceRecursionLevel;
      throw;
    }
    if (idLib::IsMainThread())
      --loadResourceRecursionLevel;
  }

  if (!skipStaleCheck && idLib::production == PROD_DEVELOPMENT &&
      resource->loadedResource && CanPerformStaleCheck(resource) &&
      dynamic_cast<idDecl*>(resource) == nullptr) {
    ReloadResourceIfStale(resource);
  }
  if (resourceLoadCallback != nullptr && resource->loadedResource &&
      !idLib::dontTrack) {
    resourceLoadCallback(resource);
  }
  return resource;
}

void idResourceList::ClearStaleCheckCountIfExisting(const char* name) {
  idResource* const resource = FindExisting(name, true);
  if (resource != nullptr)
    resource->staleCount = 0;
}

idResource* idResourceList::Index(int index) const {
  ResourceListLock lock(Mutex());
  if (index >= resourceList.num)
    idLib::FatalError("Resource index exceeds current count");
  return resourceList[index];
}

void idHashIndex::RemoveIndexFast(const int key, const int index) {
  int highestIndex = 0;
  int highestKey = 0;
  for (int bucket = 0; bucket < hashSize; ++bucket) {
    for (int current = hash[bucket]; current != -1;
         current = indexChain[current]) {
      if (current > highestIndex) {
        highestIndex = current;
        highestKey = bucket;
      }
    }
  }
  Remove(key, index);
  if (highestIndex > index) {
    Remove(highestKey, highestIndex);
    Add(highestKey, index);
  }
}

void idResourceList::Remove_Locked(idResource* resource) {
  if (resource->resourceID == 0)
    return;
  const int index =
      GetResourceIndexForID(resource->resourceID, resource->name.str);
  if (index == -1)
    idLib::FatalError("Remove_Locked: Resource wasn't found by ID");
  resourceIDHash.RemoveIndexFast(resource->resourceID & resourceIDHash.hashMask,
                                 index);
  resourceList.RemoveIndexFast(index);
  resource->resourceID = 0;
}

void idResourceList::Remove(idResource* resource) {
  ResourceListLock lock(Mutex());
  Remove_Locked(resource);
}

bool idResourceList::ReloadStaleResources() const {
  if (idLib::production != PROD_DEVELOPMENT)
    return false;
  bool reloaded = false;
  for (int index = 0; index < resourceList.num; ++index) {
    idResource* resource;
    {
      ResourceListLock lock(Mutex());
      if (index >= resourceList.num)
        idLib::FatalError("Resource index exceeds current count");
      resource = resourceList[index];
    }
    if (resource->loadedResource && ReloadResourceIfStale(resource)) {
      idLib::Printf("reloaded %s\n", resource->name.str);
      reloaded = true;
    }
  }
  return reloaded;
}

void idResourceList::MarkAllStaticResources() {
  ResourceListLock lock(Mutex());
  for (idResourceList* list = listOfResourceLists; list != nullptr;
       list = list->nextResourceList) {
    for (int index = list->resourceList.num - 1; index >= 0; --index)
      list->resourceList[index]->staticResource = true;
  }
}

void idResourceList::FreeAllDynamicResources() {
  idLib::PrintfIf(resource_showLoads.valueInteger >= 1,
                  "FreeAllDynamicResources... \n");
  ResourceListLock lock(Mutex());
  for (idResourceList* list = listOfResourceLists; list != nullptr;
       list = list->nextResourceList) {
    idLib::PrintfIf(resource_showLoads.valueInteger >= 1, " Res List: %s\n",
                    list->GetResourceTypeName());
    for (int index = list->resourceList.num - 1; index >= 0; --index) {
      idResource* const resource = list->resourceList[index];
      if (!resource->staticResource) {
        list->Remove_Locked(resource);
        if (idLib::production >= PROD_PRODUCTION)
          delete resource;
      }
    }
  }
}

void idResourceList::BuildGameForceAllResourcesToReload() {
  ResourceListLock lock(Mutex());
  for (idResourceList* list = listOfResourceLists; list != nullptr;
       list = list->nextResourceList) {
    for (int index = 0; index < list->resourceList.num; ++index)
      list->resourceList[index]->buildForceLoad = true;
  }
}

void idResourceList::GetLoadedResources(idList<idResource*, 99>& output) const {
  ResourceListLock lock(Mutex());
  output.ClearFree();
  for (int index = 0; index < resourceList.num; ++index)
    output.Append(resourceList[index]);
  std::sort(output.list, output.list + output.num,
            [](const idResource* left, const idResource* right) {
              return idStr::Cmp(left->name.str, right->name.str) < 0;
            });
}

void idResourceList::Add_Locked(idResource* resource) {
  resource->resourceID = static_cast<int>(
      HashResourceName(GetResourceTypeName(), resource->name.str));
  if (GetResourceByID(resource->resourceID) != nullptr &&
      com_production.valueInteger != 0) {
    idLib::FatalError(
        "Resource [%s] %s had a resource hash collision, and must be renamed",
        GetResourceTypeName(), resource->name.str);
  }
  const int index = resourceList.Append(resource);
  resourceIDHash.Add(resource->resourceID & resourceIDHash.hashMask, index);
}

void idResourceList::Add(idResource* resource) {
  ResourceListLock lock(Mutex());
  if (resource->resourceID != 0)
    idLib::FatalError("Resource with a set link added to a list");
  if (resource->name.str == "unnamed" ||
      idStr::Cmp(resource->name.str, "unnamed") == 0) {
    idLib::FatalError("idResourceList::Add called on UNNAMED_RESOURCE");
  }
  idResource* const existing = Lookup(resource->name.str);
  if (existing != nullptr)
    Remove_Locked(existing);
  Add_Locked(resource);
  resource->resourceListPtr = this;
}

idResource* idResourceList::Load(const char* name, bool makeDefault,
                                 bool skipStaleCheck) {
  const bool pushedWarningInfo = idLib::PushWarningInfo(GetResourceTypeName(), name);
  try {
    if (!resource_warnOnLoad.valueString.IsEmpty() &&
        idStr::Find(name, resource_warnOnLoad.valueString.data, false, 0, -1) != -1) {
      idLib::Warning("Warn on Load");
    }

    idResource* resource = FindExisting(name, skipStaleCheck);
    if (resource != nullptr) {
      if (!makeDefault && resource->resourceError != nullptr)
        resource = nullptr;
      if (pushedWarningInfo)
        idLib::PopWarningInfo();
      return resource;
    }

    if (resource_showLoads.valueInteger >= 1) {
      for (int i = 0; i < loadResourceRecursionLevel; ++i)
        idLib::Printf(" ");
      idLib::Printf("%s::LoadResource( %s )\n", GetResourceTypeName(), name);
    }
    if (idLib::production == PROD_LOADED && strstr(name, ".bswf") == nullptr) {
      idLib::Warning("Resource loaded in production mode: %s : %s",
                     GetResourceTypeName(), name);
    }
    if (common->IsInGame() && !common->IsToolActive() && !inGameLoading &&
        idLib::production != PROD_BUILDING) {
      if (resource_errorInGame.valueInteger == 1) {
        idLib::Warning(
            "resource_errorInGame enabled: Resource loaded in game: %s : %s",
            GetResourceTypeName(), name);
      } else if (resource_errorInGame.valueInteger == 2) {
        idLib::FatalError(
            "resource_errorInGame enabled: Resource loaded in game: %s : %s",
            GetResourceTypeName(), name);
      }
    }

    resource = Alloc(name);
    Add(resource);
    resource->loadedResource = true;
    if (loadResourceRecursionLevel <= 0 ||
        idLib::production != PROD_PRODUCTION || inGameLoading) {
      resource->insideRecursiveLoad = true;
      recursiveLoads.ptr[loadResourceRecursionLevel++] = resource;
      try {
        resource->Load();
        if (idLib::production == PROD_DEVELOPMENT && !skipStaleCheck)
          ReloadResourceIfStale(resource);
      } catch (...) {
        --loadResourceRecursionLevel;
        throw;
      }
      --loadResourceRecursionLevel;
      resource->insideRecursiveLoad = false;
      if (!makeDefault && resource->resourceError != nullptr)
        resource = nullptr;
      else if (resourceLoadCallback != nullptr && !idLib::dontTrack)
        resourceLoadCallback(resource);
    } else {
      resource->loadDeferred = true;
    }
    if (pushedWarningInfo)
      idLib::PopWarningInfo();
    return resource;
  } catch (...) {
    if (pushedWarningInfo)
      idLib::PopWarningInfo();
    throw;
  }
}

void idResourceLoadTimeTracker::BeginLoad(idResource* resource) {
  const std::int64_t now = static_cast<std::int64_t>(Sys_Microseconds());
  if (resourceStack.num > 0)
    resourceStack[resourceStack.num - 1].Pause(now);
  activeResourceStats* const active = resourceStack.Alloc();
  active->Setup(resource);
  idResourceList* const list = resource->GetResourceList();
  for (int index = 0; index < resourceLists.num; ++index) {
    if (resourceLists[index].list == list)
      active->listStatsIndex = index;
  }
  if (active->listStatsIndex == -1) {
    resourceLists.Alloc()->list = list;
    active->listStatsIndex = resourceLists.num - 1;
  }
  resourceStack[resourceStack.num - 1].Start(now);
}

void idResourceLoadTimeTracker::EndLoad(idResource*) {
  const std::int64_t now = static_cast<std::int64_t>(Sys_Microseconds());
  activeResourceStats* const active = &resourceStack[resourceStack.num - 1];
  if (resourceStack.num > 0)
    --resourceStack.num;
  active->Pause(now);
  resourceLists[active->listStatsIndex].totalTime += active->totalTime;
  ++resourceLists[active->listStatsIndex].numLoads;
  if (resourceStack.num > 0)
    resourceStack[resourceStack.num - 1].Start(now);
}

void ResourceLoadTrackerBeginLoad(idResource* resource) {
  if (resource_trackLoadTimes.valueInteger != 0)
    loadTimeTracker.BeginLoad(resource);
}

void ResourceLoadTrackerEndLoad(idResource* resource) {
  if (resource_trackLoadTimes.valueInteger != 0)
    loadTimeTracker.EndLoad(resource);
}

void idBaseCodeResource::AllocateCodeResources() {
  allocCodeResourcesHasBeenCalled = true;
  for (idBaseCodeResource* code = codeResourceList; code != nullptr;
       code = code->next) {
    idStr canonical(code->name);
    canonical.MakeNameCanonical();
    code->r = code->rl->Lookup(canonical.data);
    if (code->r == nullptr) {
      code->r = code->rl->Alloc(canonical.data);
      code->rl->Add(code->r);
      code->r->staticResource = true;
      code->r->loadDeferred = true;
    }
  }
}

void idBaseCodeResource::LoadCodeResources(const char* ofType) {
  for (idBaseCodeResource* code = codeResourceList; code != nullptr;
       code = code->next) {
    if (!code->r->loadDeferred)
      continue;
    if (ofType != nullptr &&
        idStr::Icmp(code->rl->GetResourceTypeName(), ofType) != 0) {
      continue;
    }
    code->r->loadedResource = true;
    code->r = code->rl->Load(code->name, false, false);
    if (code->r == nullptr) {
      if (resource_errorOnResolveFailure.valueInteger != 0) {
        idLib::FatalError("ResolveCodeResources: Resource name not found for %s:%s",
                          code->rl->GetResourceTypeName(), code->name);
      }
      idLib::Warning("ResolveCodeResources: Resource name not found for %s:%s",
                     code->rl->GetResourceTypeName(), code->name);
    }
  }
}
