#define WOLFTNO_RETAIL_IDLIB_ERROR_RETURNS_ABI
#define WOLFTNO_RETAIL_IDMEM_SINGLETON_ABI
#define WOLFTNO_RETAIL_PVS_CONST_FILE_DATA_ABI
#define WOLFTNO_RETAIL_IDSTR_INLINE_FREEDATA_ABI
#include "engine/gamelib/pvs/pvs.h"

#include "engine/framework/fileresource.h"
#include "shared/idlib/filesystem/file.h"
#include "shared/idlib/lib.h"
#undef WOLFTNO_RETAIL_PVS_CONST_FILE_DATA_ABI
#undef WOLFTNO_RETAIL_IDMEM_SINGLETON_ABI
#undef WOLFTNO_RETAIL_IDLIB_ERROR_RETURNS_ABI
#undef WOLFTNO_RETAIL_IDSTR_INLINE_FREEDATA_ABI

#include <cstring>
#include <intrin.h>
#include <new>

void operator delete[](void* memory, memTag_t tag) noexcept;

__forceinline void* operator new(std::size_t, void* storage,
                                 memTag_t) noexcept {
  return storage;
}

__forceinline void operator delete(void* memory, void*,
                                   memTag_t tag) noexcept {
  operator delete[](memory, tag);
}

namespace {

constexpr int PVS_MAGIC = 55793232;

unsigned int Swap32(unsigned int value) {
  return (value >> 24) | ((value >> 8) & 0x0000FF00u) |
         ((value << 8) & 0x00FF0000u) | (value << 24);
}

unsigned short Swap16(unsigned short value) {
  return static_cast<unsigned short>((value >> 8) | (value << 8));
}

void DecompressAdd(unsigned char* destination, int numBits,
                   const unsigned char* source) {
  int outputBit = 0;
  while (outputBit < numBits) {
    const unsigned int code = *source++;
    if ((code & 0x80u) != 0u) {
      unsigned int skip = code & 0x3Fu;
      if ((code & 0x40u) != 0u) {
        skip |= static_cast<unsigned int>(*source++) << 6;
      }
      outputBit += static_cast<int>(skip) + 1;
      continue;
    }

    for (int literalBit = 0;
         literalBit < 7 && outputBit < numBits;
         ++literalBit, ++outputBit) {
      destination[outputBit >> 3] |=
          static_cast<unsigned char>(((code >> literalBit) & 1u)
                                     << (outputBit & 7));
    }
  }
}

}  // namespace

idPVS::idPVS()
    : dataStart(nullptr),
      planes(nullptr),
      vertices(nullptr),
      nodes(nullptr),
      portals(nullptr),
      areas(nullptr),
      areaOffsets(nullptr),
      areaPVS(nullptr),
      bare(false) {
  header.magic = PVS_MAGIC;
  header.numPlanes = 0;
  header.numVertices = 0;
  header.numNodes = 0;
  header.numPortals = 0;
  header.numAreas = 0;
  header.numPVSBytes = 0;
  for (pvsCurrent_t& current : currentPVS) {
    current.handle.i = -1;
    current.handle.h = 0;
    current.pvs = nullptr;
  }
}

idPVS::~idPVS() noexcept(false) { Free(); }

int idPVS::GetPVSAreas(const idBounds& bounds, int* outputAreas,
                       int maxAreas) const {
  if (header.numNodes <= 1) {
    outputAreas[0] = 0;
    return 1;
  }

  int stackNodes[128];
  int stackDepth = 0;
  int nodeIndex = 1;
  int numAreas = 0;
  for (;;) {
    int* stack = &stackNodes[0];
    stack += stackDepth;
    while (nodeIndex > 0) {
      const pvsNode_t& node = nodes[nodeIndex];
      *stack = node.children[1];
      const int side = bounds.PlaneSide(planes[node.planeNum], 0.1f);
      if (side == PLANESIDE_CROSS) {
        nodeIndex = node.children[0];
        ++stackDepth;
        ++stack;
      } else {
        nodeIndex = node.children[side];
      }
    }

    if (nodeIndex < 0) {
      const int area = -1 - nodeIndex;
      int index = 0;
      while (index < numAreas && outputAreas[index] != area) {
        ++index;
      }
      if (index >= numAreas && area >= 0) {
        outputAreas[numAreas++] = area;
        if (numAreas >= maxAreas) {
          return numAreas;
        }
      }
    }

    if (stackDepth == 0) {
      return numAreas;
    }
    nodeIndex = stackNodes[--stackDepth];
  }
}

void idPVS::FreePVS(pvsHandle_t handle) const {
  if (static_cast<unsigned int>(handle.i) > 7u ||
      handle.h != currentPVS[handle.i].handle.h) {
    idLib::Error("idPVS::FreePVS: invalid handle");
    __assume(false);
  }
  currentPVS[handle.i].handle.i = -1;
}

bool idPVS::InPVS(pvsHandle_t handle, const int* targetAreas,
                  int numTargetAreas) const {
  if (static_cast<unsigned int>(handle.i) > 7u ||
      handle.h != currentPVS[handle.i].handle.h) {
    idLib::Error("idPVS::InPVS: invalid handle");
    __assume(false);
  }

  for (int index = 0; index < numTargetAreas; ++index) {
    const int area = targetAreas[index];
    if (area >= 0 && area < header.numAreas &&
        (currentPVS[handle.i].pvs[area >> 3] &
         static_cast<unsigned char>(1 << (area & 7))) != 0) {
      return true;
    }
  }
  return false;
}

inline idFile_Memory* idFileResource::GetFile() const {
  idMem& memory = idMem::Singleton();
  void* const storage = memory.AllocWithLocation(
      "w:\\build\\tech5\\shared\\idlib\\Heap.h(95) : tag",
      sizeof(idFile_Memory), TAG_FILE_RESOURCE, false, ALIGN_16,
      HEAP_DEFAULTHEAP);
  idFile_Memory* file = nullptr;
  if (storage != nullptr) {
    file = ::new (storage, TAG_FILE_RESOURCE) idFile_Memory(name.str);
  }
  file->SetReadOnlyData(static_cast<const char*>(data), length);
  return file;
}

int idPVS::Alloc(bool bareAllocation) {
  int allocationSize;
  if (bareAllocation) {
    allocationSize =
        header.numPVSBytes +
        4 * (header.numAreas +
             2 * (header.numNodes + 2 * header.numPlanes));
  } else {
    allocationSize =
        header.numPVSBytes +
        4 * (header.numAreas + header.numVertices +
             2 * header.numVertices +
             2 * (header.numNodes + header.numAreas +
                  header.numPortals + 2 * header.numPlanes));
  }

  idMem& memory = idMem::Singleton();
  dataStart = static_cast<unsigned char*>(memory.AllocWithLocation(
      "w:\\build\\tech5\\engine\\gamelib\\pvs\\PVS.cpp(140) : TAG_PVS",
      static_cast<std::size_t>(allocationSize), TAG_PVS, false, ALIGN_16,
      HEAP_DEFAULTHEAP));
  std::memset(dataStart, 0, static_cast<std::size_t>(allocationSize));

  planes = reinterpret_cast<idPlane*>(dataStart);
  nodes = reinterpret_cast<pvsNode_t*>(dataStart + 16 * header.numPlanes);
  areaOffsets = reinterpret_cast<int*>(nodes + header.numNodes);
  areaPVS = reinterpret_cast<unsigned char*>(areaOffsets + header.numAreas);
  idVec3* const vertexStart =
      reinterpret_cast<idVec3*>(areaPVS + header.numPVSBytes);
  if (bareAllocation) {
    vertices = nullptr;
    portals = nullptr;
    areas = nullptr;
  } else {
    vertices = vertexStart;
    portals = reinterpret_cast<pvsPortal_t*>(vertices + header.numVertices);
    areas = reinterpret_cast<pvsArea_t*>(portals + header.numPortals);
  }

  const std::size_t currentPVSBytes = static_cast<std::size_t>(
      ((header.numAreas + 31) >> 3) & ~3);
  for (pvsCurrent_t& current : currentPVS) {
    current.handle.i = -1;
    current.handle.h = 0;
    idMem& currentMemory = idMem::Singleton();
    current.pvs = static_cast<unsigned char*>(currentMemory.AllocWithLocation(
        "w:\\build\\tech5\\shared\\idlib\\Heap.h(113) : tag",
        currentPVSBytes, TAG_PVS, false, ALIGN_16, HEAP_DEFAULTHEAP));
    std::memset(current.pvs, 0, currentPVSBytes);
  }
  return allocationSize;
}

void idPVS::Free() {
  if (dataStart != nullptr) {
    idMem& memory = idMem::Singleton();
    memory.Free(dataStart, ALIGN_16);
    dataStart = nullptr;
  }
  planes = nullptr;
  vertices = nullptr;
  nodes = nullptr;
  portals = nullptr;
  areas = nullptr;
  areaOffsets = nullptr;
  areaPVS = nullptr;
  for (pvsCurrent_t& current : currentPVS) {
    unsigned char* const currentData = current.pvs;
    current.handle.i = -1;
    current.handle.h = 0;
    idMem& memory = idMem::Singleton();
    memory.Free(currentData, ALIGN_16);
    current.pvs = nullptr;
  }
}

void idPVS::Default() {
  Free();
  header.magic = PVS_MAGIC;
  header.numPlanes = 0;
  header.numVertices = 0;
  header.numNodes = 0;
  header.numPortals = 0;
  header.numAreas = 1;
  header.numPVSBytes = 8;
  Alloc(false);
  areas[0].firstPortal = 0;
  areas[0].numPortals = 0;
  areaOffsets[0] = 0;
  std::memset(areaPVS, 0x7F, 8);
}

int idPVS::GetPVSArea(const idVec3& point) const {
  if (header.numNodes <= 1 || nodes == nullptr || planes == nullptr) {
    return 0;
  }

  int nodeIndex = 1;
  for (;;) {
    const pvsNode_t& node = nodes[nodeIndex];
    const idPlane& plane = planes[node.planeNum];
    const float distance = point.y * plane.b + point.x * plane.a +
                           point.z * plane.c + plane.d;
    nodeIndex = node.children[distance >= -0.0f ? 0 : 1];
    if (nodeIndex < 0) {
      return -1 - nodeIndex;
    }
    if (nodeIndex == 0) {
      return 0;
    }
  }
}

pvsHandle_t idPVS::DecodePVS(const int* sourceAreas,
                             int numSourceAreas) const {
  unsigned int hash = 0;
  for (int index = 0; index < numSourceAreas; ++index) {
    hash ^= static_cast<unsigned int>(sourceAreas[index]);
  }

  int slot = 0;
  while (currentPVS[slot].handle.i != -1) {
    ++slot;
    if (slot >= 8) {
      idLib::Error("idPVS::AllocPVS: no free PVS left", slot,
                   sourceAreas);
      __debugbreak();
    }
  }
  currentPVS[slot].handle.h = hash;
  currentPVS[slot].handle.i = slot;
  const pvsHandle_t handle = currentPVS[slot].handle;

  const int byteCount = ((header.numAreas + 31) >> 3) & ~3;
  std::memset(currentPVS[slot].pvs, 0,
              static_cast<std::size_t>(byteCount));
  const int numBits = 8 * byteCount;
  DecompressAdd(currentPVS[slot].pvs, numBits,
                areaPVS + areaOffsets[sourceAreas[0]]);
  for (int index = 1; index < numSourceAreas; ++index) {
    DecompressAdd(currentPVS[slot].pvs, numBits,
                  areaPVS + areaOffsets[sourceAreas[index]]);
  }
  return handle;
}

void idPVS::SwapHeader() {
  int* const values = &header.magic;
  for (int index = 0; index < 7; ++index) {
    values[index] = static_cast<int>(
        Swap32(static_cast<unsigned int>(values[index])));
  }
}

void idPVS::SwapData() {
  if (planes != nullptr) {
    for (int index = 0; index < header.numPlanes; ++index) {
      unsigned int* const values =
          reinterpret_cast<unsigned int*>(&planes[index]);
      for (int component = 0; component < 4; ++component) {
        values[component] = Swap32(values[component]);
      }
    }
  }
  if (vertices != nullptr) {
    for (int index = 0; index < header.numVertices; ++index) {
      unsigned int* const values =
          reinterpret_cast<unsigned int*>(&vertices[index]);
      for (int component = 0; component < 3; ++component) {
        values[component] = Swap32(values[component]);
      }
    }
  }
  if (nodes != nullptr) {
    for (int index = 0; index < header.numNodes; ++index) {
      nodes[index].planeNum = Swap32(nodes[index].planeNum);
      nodes[index].children[0] = static_cast<short>(Swap16(
          static_cast<unsigned short>(nodes[index].children[0])));
      nodes[index].children[1] = static_cast<short>(Swap16(
          static_cast<unsigned short>(nodes[index].children[1])));
    }
  }
  if (portals != nullptr) {
    for (int index = 0; index < header.numPortals; ++index) {
      portals[index].firstVertex = static_cast<int>(Swap32(
          static_cast<unsigned int>(portals[index].firstVertex)));
      portals[index].numVertices = static_cast<int>(Swap32(
          static_cast<unsigned int>(portals[index].numVertices)));
    }
  }
  if (areas != nullptr) {
    for (int index = 0; index < header.numAreas; ++index) {
      areas[index].firstPortal = static_cast<int>(Swap32(
          static_cast<unsigned int>(areas[index].firstPortal)));
      areas[index].numPortals = static_cast<int>(Swap32(
          static_cast<unsigned int>(areas[index].numPortals)));
    }
  }
  if (areaOffsets != nullptr) {
    for (int index = 0; index < header.numAreas; ++index) {
      areaOffsets[index] = static_cast<int>(Swap32(
          static_cast<unsigned int>(areaOffsets[index])));
    }
  }
}

void idPVS::LoadResource() {
  Free();
  idStr name(pvsName);
  name.SetFileExtension("pvs");
  idResource* const loaded =
      idFileResource::resourceList.Load(name.c_str(), true, false);
  idFileResource* const resource =
      static_cast<idFileResource*>(loaded);
  if (resource != nullptr && resource->GetLength() > 0) {
    idFile_Memory* const file = resource->GetFile();
    if (file != nullptr) {
      if (file->Length() > 0) {
        file->Read(&header, sizeof(header));
        SwapHeader();
        if (header.magic == PVS_MAGIC) {
          const int size = Alloc(bare);
          file->Read(planes, static_cast<std::size_t>(size));
          SwapData();
          delete file;
          delete resource;
          return;
        }
        idLib::Warning("%s is not a PVS file or wrong version",
                       name.c_str());
        delete file;
        Default();
        return;
      }
      delete file;
    }
  }
  idLib::Warning("failed to open %s", name.c_str());
  Default();
}
