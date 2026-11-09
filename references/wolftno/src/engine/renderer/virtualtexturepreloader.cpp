#include "engine/renderer/virtualtexturepreloader.h"

#include "engine/decls/declvmtrpreload.h"
#include "engine/framework/streamfilecache.h"
#include "engine/renderer/declmaterial.h"
#include "engine/renderer/virtualtexture.h"
#include "shared/idlib/containers/heaparray.h"
#include "shared/idlib/containers/list.h"
#include "shared/idlib/lib.h"
#include "shared/idlib/sys/sys_time.h"

#include <algorithm>
#include <cstdint>
#include <cstring>
#include <new>
#include <utility>

extern bool forceIdListsToReallocateEveryAppend;

#if defined(WOLF_RENDERER_VIRTUALTEXTUREPRELOADER_TEST_ACCESS)
const idMaterial* WolfVirtualTexturePreloaderTestFindMaterial(
    const char* materialName);
bool WolfVirtualTexturePreloaderTestGetLists(
    const char* mapName, const char* spawnSpot,
    const idList<preloadMaterial_t>** sharedList,
    const idList<preloadMaterial_t>** spawnList);
#endif

template <typename T> class idSort {
public:
  virtual ~idSort() = default;
  virtual void Sort(T* base, unsigned int num) = 0;
};

template <typename T, typename Derived>
class idSort_Quick : public idSort<T> {
public:
  void Sort(T* base, unsigned int num) override;
};

class idSort_preloadMaterials
    : public idSort_Quick<idTmpPreloadMaterial_t,
                          idSort_preloadMaterials> {
public:
  ~idSort_preloadMaterials() override;

  int Compare(const idTmpPreloadMaterial_t& left,
              const idTmpPreloadMaterial_t& right) const {
    if (left.material->pageFlavor == ID_VTF_BAKE ||
        left.material->IsVMTRVirtualTexture()) {
      return static_cast<int>(left.material->pageSource) -
             static_cast<int>(right.material->pageSource);
    }
    if (right.material->pageFlavor == ID_VTF_BAKE ||
        right.material->IsVMTRVirtualTexture()) {
      return 1;
    }
    return 0;
  }
};

template <typename T, typename Derived>
void idSort_Quick<T, Derived>::Sort(T* base, unsigned int num) {
  if (num <= 1) {
    return;
  }

  constexpr std::int64_t maxLevels = 128;
  std::int64_t lo[maxLevels];
  std::int64_t hi[maxLevels];
  std::int64_t low = 0;
  std::int64_t high = num - 1;
  std::int64_t level = 0;

  for (;;) {
    std::int64_t left = low;
    std::int64_t right = high;
    if (right - left >= 4 && level < maxLevels - 1) {
      const std::int64_t pivotIndex = (left + right) / 2;
      std::swap(base[right], base[pivotIndex]);
      T& pivot = base[right--];

      do {
        while (static_cast<const Derived*>(this)->Compare(base[left], pivot) <
               0) {
          if (++left >= right) {
            break;
          }
        }
        while (static_cast<const Derived*>(this)->Compare(base[right], pivot) >
               0) {
          if (--right <= left) {
            break;
          }
        }
        if (left >= right) {
          break;
        }
        std::swap(base[left], base[right]);
      } while (++left < --right);

      while (static_cast<const Derived*>(this)->Compare(base[left], pivot) <=
                 0 &&
             left < high) {
        ++left;
      }
      while (static_cast<const Derived*>(this)->Compare(base[right], pivot) >=
                 0 &&
             low < right) {
        --right;
      }

      std::swap(pivot, base[left]);
      if (high - left <= right - low) {
        lo[level] = low;
        hi[level++] = right;
        low = left;
      } else {
        hi[level] = high;
        lo[level++] = left;
        high = right;
      }
    } else {
      for (; left < right; --right) {
        std::int64_t maximum = left;
        for (std::int64_t index = left + 1; index <= right; ++index) {
          if (static_cast<const Derived*>(this)->Compare(base[index],
                                                          base[maximum]) > 0) {
            maximum = index;
          }
        }
        std::swap(base[maximum], base[right]);
      }
      if (--level < 0) {
        return;
      }
      low = lo[level];
      high = hi[level];
    }
  }
}

idSort_preloadMaterials::~idSort_preloadMaterials() = default;

namespace {

constexpr int PRELOAD_COMMAND_MASK = 15;
constexpr int PRELOAD_PAGE_QUEUE_SIZE = 0x4000;
constexpr int PRELOAD_PAGE_QUEUE_MASK = PRELOAD_PAGE_QUEUE_SIZE - 1;

struct pageInfo_t {
  const idVirtualTexture* virtualTexture;
  unsigned int diskOffset;
  unsigned short diskLength;
  unsigned short level;
};

static_assert(sizeof(pageInfo_t) == 0x10, "preload page-info ABI drift");

const char* PreloadTypeName(preloadType_t type) {
  return type == PRELOAD_ALL ? "PRELOAD_ALL" : "PRELOAD_MATERIAL_LIST";
}

bool IsNonZeroMapping(const idVec4& mapping) {
  std::uint32_t bits[4];
  std::memcpy(bits, &mapping, sizeof(bits));
  return ((bits[0] | bits[1] | bits[2] | bits[3]) & 0x7FFFFFFFu) !=
         0;
}

void QueueCommand(idVirtualTexturePreloadThread& thread,
                  preloadCommand_t* command) {
  thread.preloadCommandQueue[thread.preloadCommandQueueEnd] = command;
  thread.preloadCommandQueueEnd =
      (thread.preloadCommandQueueEnd + 1) & PRELOAD_COMMAND_MASK;
}

} // namespace

idVirtualTexturePreloader virtualTexturePreloader;

idVirtualTexturePreloadThread::idVirtualTexturePreloadThread()
    : idSysThread(), preloadCommands{}, preloadCommandQueue{},
      preloadCommandQueueStart(0), preloadCommandQueueEnd(0), currentHandle(1),
      endCalled(false), cancelToTerminate(false) {
  for (preloadCommand_t& command : preloadCommands) {
    std::memset(&command, 0, sizeof(command));
    command.type = PRELOAD_INVALID;
    command.done = true;
  }
}

preloadCommand_t* idVirtualTexturePreloadThread::AllocCommand(
    preloadType_t type, int minLod, idVirtualTexture* virtualTexture) {
  preloadCommand_t* command = nullptr;
  for (preloadCommand_t& candidate : preloadCommands) {
    if (candidate.done) {
      command = &candidate;
      break;
    }
  }
  if (command == nullptr) {
    return nullptr;
  }

  command->handle = currentHandle++;
  command->minLod = minLod;
  command->type = type;
  command->numTextureRects = 0;
  command->texture = virtualTexture;
  command->totalTimeMSec = 0;
  std::memset(command->pagesLoaded, 0, sizeof(command->pagesLoaded));
  command->done = false;
  return command;
}

void idVirtualTexturePreloader::DestroyThread() {
  if (thread != nullptr) {
    delete thread;
  }
  thread = nullptr;
}

void idVirtualTexturePreloadThread::PrintCommandResult(
    const preloadCommand_t& command) const {
  idLib::Printf("%4.1f seconds for %s\n",
                static_cast<float>(command.totalTimeMSec) * 0.001F,
                PreloadTypeName(command.type));
  for (int source = 1; source < PAGESOURCE_TOTAL; ++source) {
    const idVirtualTexture& virtualTexture = virtualTextureSystem.vts[source];
    if (virtualTexture.pageFile != nullptr && command.pagesLoaded[source] != 0) {
      idLib::Printf("%4i pages preloaded on %s\n",
                    command.pagesLoaded[source], virtualTexture.baseName.data);
    }
  }
}

void idVirtualTexturePreloadThread::StopPreloading() {
  endCalled = true;
  WaitForThread();
  endCalled = false;
  for (preloadCommand_t& command : preloadCommands) {
    if (command.type != PRELOAD_INVALID) {
      PrintCommandResult(command);
      command.type = PRELOAD_INVALID;
    }
  }
}

void idVirtualTexturePreloader::SpawnThread() {
  idMem* memory = idMem::Singleton();
  void* storage = memory->AllocWithLocation(
      "w:\\build\\tech5\\shared\\idlib\\Heap.h(95) : tag",
      sizeof(idVirtualTexturePreloadThread), TAG_RENDERER, false, ALIGN_16,
      HEAP_DEFAULTHEAP);
  idVirtualTexturePreloadThread* preloadThread = nullptr;
  try {
    if (storage != nullptr) {
      preloadThread = ::new (storage) idVirtualTexturePreloadThread;
    }
  } catch (...) {
    memory->Free(storage, ALIGN_16);
    throw;
  }
  thread = preloadThread;
  thread->StartWorkerThread("VirtualTexturePreloader", CORE_1A, THREAD_NORMAL,
                            0x40000);
}

void idVirtualTexturePreloader::EndPreloading() {
  if (thread != nullptr && running) {
    idLib::Printf("================================================\n");
    idLib::Printf("idVirtualTexturePreloader::EndPreloading(): signaling background thread to stop\n");
    thread->StopPreloading();
    running = false;
  }
}

bool idVirtualTexturePreloader::MaterialPreloadIsDone(int handle) {
  if (handle == 0) {
    return true;
  }
  for (const preloadCommand_t& command : thread->preloadCommands) {
    if (command.handle == handle) {
      if (command.done) {
        thread->PrintCommandResult(command);
      }
      return command.done;
    }
  }
  return true;
}

int idVirtualTexturePreloadThread::Run() {
  while (preloadCommandQueueStart != preloadCommandQueueEnd) {
    preloadCommand_t* command =
        preloadCommandQueue[preloadCommandQueueStart];
    const int startTime = static_cast<int>(Sys_Milliseconds());

    idHeapArray<pageInfo_t> pageQueue;
    pageQueue.Alloc(PRELOAD_PAGE_QUEUE_SIZE);
    int queueStart = 0;
    int queueEnd = 0;
    int numQueuedPages[PAGESOURCE_TOTAL]{};
    pageSource_t activePageSources[PAGESOURCE_TOTAL]{};
    int numActivePageSources = 0;

    if (command->type == PRELOAD_ALL) {
      for (int source = 1; source < PAGESOURCE_TOTAL; ++source) {
        idVirtualTexture& virtualTexture = virtualTextureSystem.vts[source];
        if (virtualTexture.pageFile == nullptr ||
            virtualTexture.header.subRoot.finerDiskLength[0] == 0) {
          continue;
        }

        bool alreadyQueued = false;
        for (int index = 0; index < queueEnd; ++index) {
          if (idStr::Icmp(pageQueue.buffer[index].virtualTexture
                              ->pageFilePath.data,
                          virtualTexture.pageFilePath.data) == 0) {
            alreadyQueued = true;
            break;
          }
        }
        if (alreadyQueued) {
          continue;
        }

        pageInfo_t& root = pageQueue.buffer[queueEnd];
        root.virtualTexture = &virtualTexture;
        root.diskOffset =
            virtualTexture.header.subRoot.finerDiskOffset[0];
        root.diskLength =
            virtualTexture.header.subRoot.finerDiskLength[0];
        root.level =
            static_cast<unsigned short>(virtualTexture.header.numLevels - 1);
        queueEnd = (queueEnd + 1) & PRELOAD_PAGE_QUEUE_MASK;
        ++numQueuedPages[virtualTexture.pageSource];
        activePageSources[numActivePageSources++] = virtualTexture.pageSource;
      }
    } else if (command->type == PRELOAD_MATERIAL_LIST &&
               command->texture != nullptr &&
               command->texture->header.subRoot.finerDiskLength[0] != 0) {
      idVirtualTexture& virtualTexture = *command->texture;
      pageInfo_t& root = pageQueue.buffer[0];
      root.virtualTexture = &virtualTexture;
      root.diskOffset = virtualTexture.header.subRoot.finerDiskOffset[0];
      root.diskLength = virtualTexture.header.subRoot.finerDiskLength[0];
      root.level =
          static_cast<unsigned short>(virtualTexture.header.numLevels - 1);
      queueEnd = 1;
      ++numQueuedPages[virtualTexture.pageSource];
      activePageSources[numActivePageSources++] = virtualTexture.pageSource;
    }

    const int pageSourceTimeSlice = numActivePageSources > 0
                                        ? std::max(400 / numActivePageSources,
                                                   50)
                                        : 0;

    while (!endCalled && !cancelToTerminate && queueStart != queueEnd) {
      streamFileCache->NewFrame();
      int queueIndex = queueStart;
      while (!endCalled) {
        pageInfo_t& page = pageQueue.buffer[queueIndex];
        int minimumPagesLoaded = 0x7FFFFFFF;
        for (int index = 0; index < numActivePageSources; ++index) {
          const pageSource_t source = activePageSources[index];
          if (numQueuedPages[source] != 0) {
            minimumPagesLoaded =
                std::min(minimumPagesLoaded, command->pagesLoaded[source]);
          }
        }

        const idVirtualTexture& virtualTexture = *page.virtualTexture;
        const pageSource_t source = virtualTexture.pageSource;
        if (command->pagesLoaded[source] <=
            minimumPagesLoaded + pageSourceTimeSlice) {
          idFile* pageFile = virtualTexture.pageFileInstalled;
          if (pageFile == nullptr ||
              page.level < virtualTexture.minMipLevelInstalled) {
            pageFile = virtualTexture.pageFile;
          }

          pageHeader_t pageHeader{};
          const std::int64_t pageOffset =
              static_cast<std::int64_t>(page.diskOffset) *
              virtualTexture.header.diskOffsetScale;
          const int persistence = 64 << (page.level + 4);
          if (streamFileCache->ReadCachedData(
                  pageFile, pageOffset, sizeof(pageHeader_t), &pageHeader,
                  false, page.level, persistence, PFC_TEXTURE) ==
              sizeof(pageHeader_t)) {
            ++command->pagesLoaded[source];

            int minimumLevel = command->minLod;
            if (command->type == PRELOAD_ALL &&
                virtualTexture.flavor == ID_VTF_VMTR) {
              minimumLevel = virtualTexture.header.numLevels - 5;
            }

            if (page.level > minimumLevel) {
              for (int quadrant = 0; quadrant < 4; ++quadrant) {
                if (pageHeader.finerDiskLength[quadrant] == 0) {
                  continue;
                }

                if (command->type == PRELOAD_MATERIAL_LIST) {
                  const int childLevel = page.level - 1;
                  const int childX =
                      ((quadrant & 1) | (2 * pageHeader.x)) << childLevel;
                  const int childY = (((quadrant >> 1) & 1) |
                                      (2 * pageHeader.y))
                                     << childLevel;
                  const int levelMask = ~((1 << childLevel) - 1);
                  bool intersects = false;
                  for (int rectangle = 0;
                       rectangle < command->numTextureRects; ++rectangle) {
                    const preloadTextureRect_t& bounds =
                        command->textureRects[rectangle];
                    if (childLevel >= command->textureMinLod[rectangle] &&
                        childX >= (levelMask & bounds.x1) &&
                        childY >= (levelMask & bounds.y1) &&
                        childX <= (levelMask & bounds.x2) &&
                        childY <= (levelMask & bounds.y2)) {
                      intersects = true;
                      break;
                    }
                  }
                  if (!intersects) {
                    continue;
                  }
                }

                const int nextQueueEnd =
                    (queueEnd + 1) & PRELOAD_PAGE_QUEUE_MASK;
                if (nextQueueEnd == queueStart) {
                  break;
                }
                pageInfo_t& child = pageQueue.buffer[queueEnd];
                child.virtualTexture = page.virtualTexture;
                child.diskOffset = pageHeader.finerDiskOffset[quadrant];
                child.diskLength = pageHeader.finerDiskLength[quadrant];
                child.level = static_cast<unsigned short>(page.level - 1);
                queueEnd = nextQueueEnd;
                ++numQueuedPages[source];
              }
            }

            --numQueuedPages[source];
            int moveIndex = queueIndex;
            while (moveIndex != queueStart) {
              const int previous =
                  (moveIndex - 1) & PRELOAD_PAGE_QUEUE_MASK;
              pageQueue.buffer[moveIndex] = pageQueue.buffer[previous];
              moveIndex = previous;
            }
            queueStart = (queueStart + 1) & PRELOAD_PAGE_QUEUE_MASK;
          }
        }

        queueIndex = (queueIndex + 1) & PRELOAD_PAGE_QUEUE_MASK;
        if (queueIndex == queueEnd) {
          break;
        }
      }
      streamFileCache->SignalWork();
      Sys_Sleep(static_cast<unsigned long>(8));
    }

    command->totalTimeMSec =
        static_cast<int>(Sys_Milliseconds()) - startTime;
    command->done = true;
    preloadCommandQueueStart =
        (preloadCommandQueueStart + 1) & PRELOAD_COMMAND_MASK;
  }
  return 0;
}

int idVirtualTexturePreloadThread::AddCommand(
    preloadType_t type, const preloadMaterial_t* const materials,
    int numMaterials, int minLod) {
  preloadCommand_t* command = nullptr;
  if (type == PRELOAD_ALL) {
    command = AllocCommand(PRELOAD_ALL, minLod, nullptr);
  } else {
    idList<idTmpPreloadMaterial_t, 5> materialsToLoad;
    if (numMaterials > 0) {
      materialsToLoad.ResizeForPort(numMaterials);
    }

    for (int index = 0; index < numMaterials; ++index) {
      const char* materialName = materials[index].material.str;
#if defined(WOLF_RENDERER_VIRTUALTEXTUREPRELOADER_TEST_ACCESS)
      const idMaterial* material =
          WolfVirtualTexturePreloaderTestFindMaterial(materialName);
#else
      char remappedName[512];
      const char* remapped = idMaterial::resourceList.RemapResourceName(
          remappedName, sizeof(remappedName), materialName);
      const idMaterial* material = static_cast<const idMaterial*>(
          static_cast<idResourceList&>(idMaterial::resourceList)
              .FindExisting(remapped, false));
#endif
      if (material == nullptr) {
        idLib::Warning(
            "idVirtualTexturePreloadThread::AddCommand: can't load material %s\n",
            materialName);
        continue;
      }

      if (forceIdListsToReallocateEveryAppend) {
        materialsToLoad.ResizeForPort(materialsToLoad.size + 1);
      }
      const idTmpPreloadMaterial_t temporary{
          material, materials[index].minLod};
      materialsToLoad.Append(temporary);
    }

    idSort_preloadMaterials sorter;
    sorter.Sort(materialsToLoad.list,
                static_cast<unsigned int>(materialsToLoad.num));

    for (int index = 0; index < materialsToLoad.num; ++index) {
      const idTmpPreloadMaterial_t& preloadMaterial = materialsToLoad[index];
      const idMaterial& material = *preloadMaterial.material;
      const idVec4 mapping = material.virtualMapping;
      if (!IsNonZeroMapping(mapping)) {
        continue;
      }

      idVirtualTexture* virtualTexture =
          &virtualTextureSystem.vts[material.pageSource];
      if (command != nullptr && command->texture != virtualTexture) {
        QueueCommand(*this, command);
        command = nullptr;
      }
      if (command == nullptr) {
        command = AllocCommand(type, minLod, virtualTexture);
        if (command == nullptr) {
          break;
        }
      }

      const int virtualTexelsWide = virtualTexture->VIRTUAL_TEXELS_WIDE;
      const int mappingWidth =
          static_cast<int>(virtualTexelsWide * mapping.x);
      const int mappingHeight =
          static_cast<int>(virtualTexelsWide * mapping.y);
      const int mappingX = static_cast<int>(virtualTexelsWide * mapping.z);
      const int mappingY = static_cast<int>(virtualTexelsWide * mapping.w);
      preloadTextureRect_t& rectangle =
          command->textureRects[command->numTextureRects];
      rectangle.x1 = static_cast<unsigned short>(mappingX / 120);
      rectangle.y1 = static_cast<unsigned short>(mappingY / 120);
      rectangle.x2 =
          static_cast<unsigned short>((mappingWidth + mappingX) / 120);
      rectangle.y2 =
          static_cast<unsigned short>((mappingY + mappingHeight) / 120);
      command->textureMinLod[command->numTextureRects++] =
          static_cast<std::uint8_t>(preloadMaterial.minLod);
      command->minLod = std::min(command->minLod, preloadMaterial.minLod);
    }
  }

  if (command != nullptr) {
    QueueCommand(*this, command);
  }
  SignalWork();
  return currentHandle - 1;
}

void idVirtualTexturePreloader::BeginPreloading() {
  idLib::Printf(
      "idVirtualTexturePreloader::BeginPreloading(): signaling background thread to start\n");
  thread->AddCommand(PRELOAD_ALL, nullptr, 0, 0);
  running = true;
}

int idVirtualTexturePreloader::PreloadVMTRsForMap(
    const char* const mapName, const char* spawnSpot) {
  if (mapName == nullptr) {
    return 0;
  }
#if defined(WOLF_RENDERER_VIRTUALTEXTUREPRELOADER_TEST_ACCESS)
  const idList<preloadMaterial_t>* sharedList = nullptr;
  const idList<preloadMaterial_t>* spawnList = nullptr;
  if (!WolfVirtualTexturePreloaderTestGetLists(
          mapName, spawnSpot, &sharedList, &spawnList)) {
    return 0;
  }

  int handle = 0;
  if (sharedList != nullptr && sharedList->num > 0) {
    handle = thread->AddCommand(PRELOAD_MATERIAL_LIST, sharedList->list,
                                sharedList->num, 16);
  }
  if (spawnList != nullptr && spawnList->num > 0) {
    return thread->AddCommand(PRELOAD_MATERIAL_LIST, spawnList->list,
                              spawnList->num, 16);
  }
  return handle;
#else
  const idDecl* found = idDeclVmtrPreload::resourceList.FindWithInheritance(
      mapName, false, true);
  const auto* preload = static_cast<const idDeclVmtrPreload*>(found);
  if (preload == nullptr) {
    return 0;
  }

  int handle = 0;
  if (preload->sharedList.num > 0) {
    handle = thread->AddCommand(PRELOAD_MATERIAL_LIST,
                                preload->sharedList.list,
                                preload->sharedList.num, 16);
  }
  const idList<preloadMaterial_t>& spawnList =
      preload->GetListForSpawnspot(spawnSpot);
  if (spawnList.num > 0) {
    return thread->AddCommand(PRELOAD_MATERIAL_LIST, spawnList.list,
                              spawnList.num, 16);
  }
  return handle;
#endif
}
