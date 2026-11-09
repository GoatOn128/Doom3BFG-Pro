#pragma once

#include "engine/decls/decl.h"
#include "shared/idlib/containers/staticlist.h"

class idResourceLoadTimeTracker {
public:
  struct resourceListStats {
    resourceListStats() : numLoads(0), totalTime(0), list(nullptr) {}
    int numLoads;
    std::int64_t totalTime;
    idResourceList* list;
  };

  struct activeResourceStats {
    activeResourceStats()
        : startTime(0), totalTime(0), activeResource(nullptr),
          listStatsIndex(-1) {}
    void Setup(idResource* resource) {
      startTime = 0;
      totalTime = 0;
      activeResource = resource;
      listStatsIndex = -1;
    }
    void Start(std::int64_t now) { startTime = now; }
    void Pause(std::int64_t now) {
      totalTime += now - startTime;
      startTime = 0;
    }
    std::int64_t GetTotal() const { return totalTime; }

    std::int64_t startTime;
    std::int64_t totalTime;
    idResource* activeResource;
    int listStatsIndex;
  };

  idResourceLoadTimeTracker() = default;
  ~idResourceLoadTimeTracker() = default;
  void BeginLoad(idResource* resource);
  void EndLoad(idResource* resource);
  void PrintStats(float minimumMilliseconds);
  void PrintStatsToJSonFile(const char* filename, bool append);

  idStaticList<resourceListStats, 128> resourceLists;
  idStaticList<activeResourceStats, 64> resourceStack;
};

void ResourceLoadTrackerBeginLoad(idResource* resource);
void ResourceLoadTrackerEndLoad(idResource* resource);

static_assert(sizeof(idResourceLoadTimeTracker::resourceListStats) == 0x18,
              "resource-list stats ABI drift");
static_assert(sizeof(idResourceLoadTimeTracker::activeResourceStats) == 0x20,
              "active-resource stats ABI drift");
static_assert(offsetof(idResourceLoadTimeTracker, resourceStack) == 0xC18,
              "resource load tracker ABI drift");
static_assert(sizeof(idResourceLoadTimeTracker) == 0x1430,
              "resource load tracker size drift");
