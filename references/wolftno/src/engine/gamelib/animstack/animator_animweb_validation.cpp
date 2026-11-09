#define WOLFTNO_RETAIL_ANIMWEB_DECLMD6_LAYOUT_ABI
#define WOLFTNO_RETAIL_ANIMWEB_GETNAME_POINTER_ABI
#define WOLFTNO_RETAIL_ANIMATOR_ANIMWEB_INDEX_ABI
#define WOLFTNO_RETAIL_PONYTAIL_QUATERNION_MULTIPLY_ABI
#include "engine/gamelib/animstack/animweb/animator_animweb_abi.h"
#include "shared/idlib/math/random.h"

#define WIN32_LEAN_AND_MEAN
#include <Windows.h>

#include <cstdint>

#pragma function(memset)

extern "C" void* __cdecl memset(void* destination, int value,
                                 std::size_t count) {
  auto* output = static_cast<volatile std::uint8_t*>(destination);
  for (std::size_t index = 0; index < count; ++index) {
    output[index] = static_cast<std::uint8_t>(value);
  }
  return destination;
}

template <typename Type>
Type WrappedLerp(Type rangeMinimum, Type rangeMaximum, Type from, Type to,
                 Type fraction);

struct djScratch_t {
  std::uint16_t cost;
  std::int16_t prevIndex;
};

template <typename NodeHolder, typename NodeCache, typename NodeIndex,
          typename EdgeCache, typename Route>
class idDijkstra {
public:
  bool FindShortestPaths(const NodeHolder& nodeHolder, NodeIndex startNode,
                         const NodeIndex* destNodes, int numDestNodes,
                         const EdgeCache* edges, djScratch_t* scratchBuff,
                         int skipFlags, int requiredFlags);

  int lastStartNode;
};

namespace {

std::uint32_t randomState = 0x9E3779B9u;

std::uint32_t RandomU32() {
  randomState ^= randomState << 13;
  randomState ^= randomState >> 17;
  randomState ^= randomState << 5;
  return randomState;
}

float RandomFloat() {
  union {
    std::uint32_t bits;
    float value;
  } result{RandomU32()};
  return result.value;
}

std::uint32_t FloatBits(float value) {
  union {
    float value;
    std::uint32_t bits;
  } result{value};
  return result.bits;
}

void* MapRetailImage(const wchar_t* path) {
  const HANDLE file = CreateFileW(path, GENERIC_READ, FILE_SHARE_READ, nullptr,
                                  OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, nullptr);
  if (file == INVALID_HANDLE_VALUE) return nullptr;
  LARGE_INTEGER fileSize{};
  GetFileSizeEx(file, &fileSize);
  auto* bytes = static_cast<std::uint8_t*>(VirtualAlloc(
      nullptr, static_cast<std::size_t>(fileSize.QuadPart),
      MEM_RESERVE | MEM_COMMIT, PAGE_READWRITE));
  if (bytes == nullptr) {
    CloseHandle(file);
    return nullptr;
  }
  DWORD read = 0;
  const BOOL readOk = ReadFile(file, bytes,
                               static_cast<DWORD>(fileSize.QuadPart), &read,
                               nullptr);
  CloseHandle(file);
  if (!readOk || read != static_cast<DWORD>(fileSize.QuadPart)) return nullptr;
  const auto* dos = reinterpret_cast<const IMAGE_DOS_HEADER*>(bytes);
  const auto* nt = reinterpret_cast<const IMAGE_NT_HEADERS64*>(
      bytes + dos->e_lfanew);
  auto* image = static_cast<std::uint8_t*>(VirtualAlloc(
      reinterpret_cast<void*>(nt->OptionalHeader.ImageBase),
      nt->OptionalHeader.SizeOfImage, MEM_RESERVE | MEM_COMMIT,
      PAGE_EXECUTE_READWRITE));
  if (image == nullptr) return nullptr;
  for (std::size_t index = 0; index < nt->OptionalHeader.SizeOfHeaders;
       ++index) {
    image[index] = bytes[index];
  }
  const IMAGE_SECTION_HEADER* section = IMAGE_FIRST_SECTION(nt);
  for (unsigned index = 0; index < nt->FileHeader.NumberOfSections; ++index) {
    if (section[index].SizeOfRawData == 0) continue;
    for (std::size_t byte = 0; byte < section[index].SizeOfRawData; ++byte) {
      image[section[index].VirtualAddress + byte] =
          bytes[section[index].PointerToRawData + byte];
    }
  }
  FlushInstructionCache(GetCurrentProcess(), image,
                        nt->OptionalHeader.SizeOfImage);
  return image;
}

[[noreturn]] void Finish(const char* message, DWORD exitCode) {
  DWORD length = 0;
  while (message[length] != '\0') ++length;
  DWORD written = 0;
  WriteFile(GetStdHandle(STD_OUTPUT_HANDLE), message, length, &written,
            nullptr);
  ExitProcess(exitCode);
}

void Progress(const char* message) {
  DWORD length = 0;
  while (message[length] != '\0') ++length;
  DWORD written = 0;
  WriteFile(GetStdHandle(STD_OUTPUT_HANDLE), message, length, &written,
            nullptr);
}

void AppendHex(char*& output, std::uint32_t value) {
  static constexpr char digits[] = "0123456789ABCDEF";
  for (int shift = 28; shift >= 0; shift -= 4) {
    *output++ = digits[(value >> shift) & 15u];
  }
}

[[noreturn]] void ReportNormalize(const idQuat& input,
                                  const idQuat& production,
                                  const idQuat& retail, int iteration) {
  char buffer[256];
  char* output = buffer;
  const char prefix[] = "FAIL quaternion normalize ";
  for (char value : prefix) {
    if (value != '\0') *output++ = value;
  }
  AppendHex(output, static_cast<std::uint32_t>(iteration));
  const char labels[] = " input ";
  for (char value : labels) {
    if (value != '\0') *output++ = value;
  }
  for (int index = 0; index < 4; ++index) {
    AppendHex(output, FloatBits((&input.x)[index]));
    *output++ = ' ';
  }
  const char productionLabel[] = "production ";
  for (char value : productionLabel) {
    if (value != '\0') *output++ = value;
  }
  for (int index = 0; index < 4; ++index) {
    AppendHex(output, FloatBits((&production.x)[index]));
    *output++ = ' ';
  }
  const char retailLabel[] = "retail ";
  for (char value : retailLabel) {
    if (value != '\0') *output++ = value;
  }
  for (int index = 0; index < 4; ++index) {
    AppendHex(output, FloatBits((&retail.x)[index]));
    *output++ = ' ';
  }
  *output++ = '\r';
  *output++ = '\n';
  DWORD written = 0;
  WriteFile(GetStdHandle(STD_OUTPUT_HANDLE), buffer,
            static_cast<DWORD>(output - buffer), &written, nullptr);
  ExitProcess(1);
}

template <typename Function>
Function Retail(std::uintptr_t address) {
  return reinterpret_cast<Function>(address);
}

int __fastcall EdgeCost(const idAnimator_AnimWeb*, idAnimWebNodeIndex node,
                        idAnimWebEdgeIndex edge) {
  static bool first = true;
  if (first) {
    first = false;
    Progress("edge cost\r\n");
  }
  return 1 + ((static_cast<int>(static_cast<std::uint16_t>(node.value)) * 13 +
               static_cast<int>(static_cast<std::uint16_t>(edge.value)) * 7) &
              63);
}

} // namespace

extern "C" [[noreturn]] void WINAPI ValidationEntry() {
  Progress("validation entry\r\n");
  if (MapRetailImage(L"retail\\WolfNewOrder_x64.exe.unpacked.exe") == nullptr) {
    Finish("FAIL map\r\n", 1);
  }
  Progress("mapped retail\r\n");
  auto* retailSmallestNonDenorm =
      reinterpret_cast<float*>(0x141DBF310ull);
  for (int index = 0; index < 4; ++index) {
    retailSmallestNonDenorm[index] = 1.175494351e-38f;
  }

  const auto retailIsTransitioning = Retail<bool (*)(
      const idAnimator_AnimWeb*)>(0x1401A5FB0ull);
  alignas(idAnimator_AnimWeb) std::uint8_t webBytes[sizeof(idAnimator_AnimWeb)];
  auto* web = reinterpret_cast<idAnimator_AnimWeb*>(webBytes);
  for (int iteration = 0; iteration < 100000; ++iteration) {
    web->curEdgeNodeIndex.value = static_cast<std::int16_t>(RandomU32());
    web->curEdgeIndex = static_cast<std::int16_t>(RandomU32());
    if (web->IsTransitioning() != retailIsTransitioning(web)) {
      Finish("FAIL IsTransitioning\r\n", 1);
    }
  }
  Progress("transition passed\r\n");

  const auto retailRandomInt =
      Retail<int (*)(idRandom2*, int)>(0x1401A5750ull);
  for (int iteration = 0; iteration < 100000; ++iteration) {
    idRandom2 production{RandomU32()};
    idRandom2 retail = production;
    const int maximum = static_cast<int>(RandomU32());
    const int productionValue = production.RandomInt(maximum);
    const int retailValue = retailRandomInt(&retail, maximum);
    if (productionValue != retailValue || production.seed != retail.seed) {
      Finish("FAIL RandomInt\r\n", 1);
    }
  }
  Progress("random passed\r\n");

  const auto retailMultiply = Retail<idQuat* (*)(
      const idQuat*, idQuat*, const idQuat&)>(0x1401A5880ull);
  const auto retailNormalize =
      Retail<idQuat* (*)(idQuat*)>(0x1401A59C0ull);
  for (int iteration = 0; iteration < 100000; ++iteration) {
    idQuat left;
    left.x = RandomFloat();
    left.y = RandomFloat();
    left.z = RandomFloat();
    left.w = RandomFloat();
    idQuat right;
    right.x = RandomFloat();
    right.y = RandomFloat();
    right.z = RandomFloat();
    right.w = RandomFloat();
    const idQuat productionProduct = left * right;
    idQuat retailProduct;
    retailMultiply(&left, &retailProduct, right);
    const auto* productionProductBytes =
        reinterpret_cast<const std::uint8_t*>(&productionProduct);
    const auto* retailProductBytes =
        reinterpret_cast<const std::uint8_t*>(&retailProduct);
    for (std::size_t byte = 0; byte < sizeof(idQuat); ++byte) {
      if (productionProductBytes[byte] != retailProductBytes[byte]) {
        ReportNormalize(left, productionProduct, retailProduct, iteration);
      }
    }

    idQuat productionNormalized = left;
    idQuat retailNormalized = left;
    productionNormalized.Normalize();
    retailNormalize(&retailNormalized);
    const auto* productionNormalizedBytes =
        reinterpret_cast<const std::uint8_t*>(&productionNormalized);
    const auto* retailNormalizedBytes =
        reinterpret_cast<const std::uint8_t*>(&retailNormalized);
    for (std::size_t byte = 0; byte < sizeof(idQuat); ++byte) {
      if (productionNormalizedBytes[byte] != retailNormalizedBytes[byte]) {
        ReportNormalize(left, productionNormalized, retailNormalized,
                        iteration);
      }
    }
  }
  Progress("quat passed\r\n");

  const auto retailWrappedLerp = Retail<float (*)(
      float, float, float, float, float)>(0x1401A5A80ull);
  for (int iteration = 0; iteration < 100000; ++iteration) {
    const float minimum = RandomFloat();
    const float maximum = RandomFloat();
    const float from = RandomFloat();
    const float to = RandomFloat();
    const float fraction = RandomFloat();
    const float productionValue =
        WrappedLerp<float>(minimum, maximum, from, to, fraction);
    const float retailValue =
        retailWrappedLerp(minimum, maximum, from, to, fraction);
    if (FloatBits(productionValue) != FloatBits(retailValue)) {
      Finish("FAIL WrappedLerp\r\n", 1);
    }
  }
  Progress("lerp passed\r\n");

  const auto retailAppend = Retail<void (*)(
      idAnimWebRoute*, const idAnimWebRoute&)>(0x1401A5F30ull);
  for (int iteration = 0; iteration < 100000; ++iteration) {
    alignas(idAnimWebRoute)
        std::uint8_t productionStorage[sizeof(idAnimWebRoute)];
    alignas(idAnimWebRoute) std::uint8_t retailStorage[sizeof(idAnimWebRoute)];
    alignas(idAnimWebRoute) std::uint8_t otherStorage[sizeof(idAnimWebRoute)];
    auto* production =
        reinterpret_cast<idAnimWebRoute*>(productionStorage);
    auto* retail = reinterpret_cast<idAnimWebRoute*>(retailStorage);
    auto* other = reinterpret_cast<idAnimWebRoute*>(otherStorage);
    production->num = static_cast<std::int16_t>(RandomU32() % 33);
    production->cost = static_cast<int>(RandomU32());
    other->num = static_cast<std::int16_t>(1 + RandomU32() % 32);
    other->cost = static_cast<int>(RandomU32());
    for (int index = 0; index < idAnimWebRoute::MAX_PATH_LEN; ++index) {
      production->path[index].value = static_cast<std::int16_t>(RandomU32());
      other->path[index].value = static_cast<std::int16_t>(RandomU32());
    }
    for (std::size_t byte = 0; byte < sizeof(*production); ++byte) {
      retailStorage[byte] = productionStorage[byte];
    }
    production->AppendPath(*other);
    retailAppend(retail, *other);
    const auto* productionBytes =
        reinterpret_cast<const std::uint8_t*>(production);
    const auto* retailBytes = reinterpret_cast<const std::uint8_t*>(retail);
    for (std::size_t byte = 0; byte < sizeof(*production); ++byte) {
      if (productionBytes[byte] != retailBytes[byte]) {
        Finish("FAIL AppendPath\r\n", 1);
      }
    }
  }
  Progress("append passed\r\n");

  using Dijkstra =
      idDijkstra<idAnimator_AnimWeb, idDeclAnimWeb::idNodeCache,
                 idAnimWebNodeIndex, idDeclAnimWeb::idEdgeCache,
                 idAnimWebRoute>;
  const auto retailFindShortestPaths = Retail<bool (*)(
      Dijkstra*, const idAnimator_AnimWeb&, idAnimWebNodeIndex,
      const idAnimWebNodeIndex*, int, const idDeclAnimWeb::idEdgeCache*,
      djScratch_t*, int, int)>(0x1401A9CF0ull);
  Progress("dijkstra address\r\n");
  void* fakeVtable[64]{};
  fakeVtable[25] = reinterpret_cast<void*>(&EdgeCost);
  alignas(idDeclAnimWeb) std::uint8_t declarationBytes[sizeof(idDeclAnimWeb)]{};
  auto* declaration = reinterpret_cast<idDeclAnimWeb*>(declarationBytes);
  idDeclAnimWebNode* nodePointers[24]{};
  idDeclAnimWeb::idNodeCache nodeCache[24]{};
  idDeclAnimWeb::idEdgeCache edgeCache[72]{};
  std::int16_t edgeIndices[72]{};
  idAnimWebNodeIndex pathableNodes[24]{};
  Progress("dijkstra arrays\r\n");
  alignas(idList<idAnimWebNodeIndex, 32>)
      std::uint8_t pathableListBytes[sizeof(idList<idAnimWebNodeIndex, 32>)]{};
  auto* pathableList =
      reinterpret_cast<idList<idAnimWebNodeIndex, 32>*>(pathableListBytes);
  pathableList->list = pathableNodes;
  pathableList->size = 24;
  pathableList->num = 0;
  *reinterpret_cast<void***>(web) = fakeVtable;
  web->decl = declaration;
  web->pathableSubWebNodeLists.num = 1;
  web->pathableSubWebNodeLists.list[0] = pathableList;
  declaration->nodes.list = nodePointers;
  declaration->nodes.size = 24;
  declaration->nodeCache.list = nodeCache;
  declaration->nodeCache.size = 24;
  declaration->edgeCache.list = edgeCache;
  declaration->edgeCache.size = 72;
  declaration->edgeIndexCache = edgeIndices;
  Progress("dijkstra configured\r\n");
  for (int iteration = 0; iteration < 100000; ++iteration) {
    const int nodeCount = 2 + static_cast<int>(RandomU32() % 23);
    int edgeCount = 0;
    for (int nodeIndex = 0; nodeIndex < nodeCount; ++nodeIndex) {
      pathableNodes[nodeIndex].value = static_cast<std::int16_t>(nodeIndex);
      nodeCache[nodeIndex].flags = static_cast<std::uint8_t>(RandomU32());
      nodeCache[nodeIndex].edgeIndexOffset = edgeCount;
      const int maximumEdges = nodeCount - nodeIndex - 1;
      const int localEdgeCount = maximumEdges == 0
                                     ? 0
                                     : 1 + static_cast<int>(
                                               RandomU32() %
                                               (maximumEdges < 3
                                                    ? maximumEdges
                                                    : 3));
      nodeCache[nodeIndex].numEdges =
          static_cast<std::uint16_t>(localEdgeCount);
      for (int localEdge = 0; localEdge < localEdgeCount; ++localEdge) {
        edgeIndices[edgeCount] = static_cast<std::int16_t>(edgeCount);
        edgeCache[edgeCount].destNodeIndex.value =
            static_cast<std::int16_t>(
                nodeIndex + 1 + RandomU32() % maximumEdges);
        edgeCache[edgeCount].weightScale =
            static_cast<std::uint8_t>(RandomU32());
        edgeCache[edgeCount].flags = static_cast<std::uint8_t>(RandomU32());
        ++edgeCount;
      }
    }
    declaration->nodes.num = nodeCount;
    declaration->nodeCache.num = nodeCount;
    declaration->edgeCache.num = edgeCount;
    pathableList->num = nodeCount;

    const idAnimWebNodeIndex startNode(
        static_cast<std::int16_t>(RandomU32() % nodeCount));
    idAnimWebNodeIndex destinations[4];
    const int destinationCount = 1 + static_cast<int>(RandomU32() % 4);
    for (int index = 0; index < destinationCount; ++index) {
      destinations[index].value =
          static_cast<std::int16_t>(RandomU32() % nodeCount);
    }
    const int skipFlags = static_cast<int>(RandomU32() & 0xFFu);
    const int requiredFlags = static_cast<int>(RandomU32() & 0xFFu);
    djScratch_t productionScratch[24];
    djScratch_t retailScratch[24];
    Dijkstra productionDijkstra{};
    Dijkstra retailDijkstra{};
    if (iteration == 0) Progress("dijkstra production\r\n");
    const bool productionResult = productionDijkstra.FindShortestPaths(
        *web, startNode, destinations, destinationCount, edgeCache,
        productionScratch, skipFlags, requiredFlags);
    if (iteration == 0) Progress("dijkstra retail\r\n");
    const bool retailResult = retailFindShortestPaths(
        &retailDijkstra, *web, startNode, destinations, destinationCount,
        edgeCache, retailScratch, skipFlags, requiredFlags);
    if (iteration == 0) Progress("dijkstra compare\r\n");
    if (productionResult != retailResult ||
        productionDijkstra.lastStartNode != retailDijkstra.lastStartNode) {
      Finish("FAIL FindShortestPaths result\r\n", 1);
    }
    const auto* productionBytes =
        reinterpret_cast<const std::uint8_t*>(productionScratch);
    const auto* retailBytes =
        reinterpret_cast<const std::uint8_t*>(retailScratch);
    for (std::size_t byte = 0;
         byte < sizeof(djScratch_t) * static_cast<std::size_t>(nodeCount);
         ++byte) {
      if (productionBytes[byte] != retailBytes[byte]) {
        Finish("FAIL FindShortestPaths scratch\r\n", 1);
      }
    }
  }

  Finish("animator_animweb differential validation passed\r\n", 0);
}
