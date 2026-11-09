#define WOLFTNO_RETAIL_ANIMWEB_DECLMD6_LAYOUT_ABI
#define WOLFTNO_RETAIL_ANIMWEB_GETNAME_POINTER_ABI
#define WOLFTNO_RETAIL_ANIMATOR_ANIMWEB_INDEX_ABI
#define WOLFTNO_RETAIL_ANIMWEB_GETSCALAR_REFERENCE_ABI
#define WOLFTNO_RETAIL_ANIMWEB_RESTART_FRAME_INT_ABI
#define WOLFTNO_RETAIL_ANIMWEB_SCALARMEM_PRIVATE_FREE_ABI
#define WOLFTNO_RETAIL_CHANNEL_BLENDPARMS_ABI
#define WOLFTNO_RETAIL_CVAR_INLINE_FREEDATA_ABI
#define WOLFTNO_RETAIL_FIND_ALIAS_REFERENCE_ABI
#define WOLFTNO_RETAIL_IDMEM_SINGLETON_ABI
#define WOLFTNO_RETAIL_IDLIST_NOINLINE_ALLOC_ABI
#define WOLFTNO_RETAIL_IDLIST_NOINLINE_APPEND_ABI
#define WOLFTNO_RETAIL_IDLIST_NOINLINE_RESIZE_ABI
#define WOLFTNO_RETAIL_IDLIST_EXACT_GROWTH_ABI
#define WOLFTNO_RETAIL_IDSTR_INLINE_FREEDATA_ABI
#define WOLFTNO_RETAIL_IDLIB_ERROR_RETURNS_ABI
#define WOLFTNO_RETAIL_PONYTAIL_QUATERNION_MULTIPLY_ABI
#define WOLFTNO_RETAIL_SYNCED_LEAF_INIT_ABI
#include "engine/gamelib/animstack/animweb/animator_animweb_abi.h"

#include "engine/framework/gametimemanager.h"
#include "engine/gamelib/animstack/animstack.h"
#define WOLFTNO_RETAIL_ANIMWEB_BLENDTREE_CONST_PREBLEND_ABI
#include "engine/gamelib/animstack/animweb/animwebblendtree.h"
#undef WOLFTNO_RETAIL_ANIMWEB_BLENDTREE_CONST_PREBLEND_ABI
#include "engine/models/skeletalanimation/animation.h"
#include "engine/models/skeletalanimation/jointanimator.h"
#include "engine/models/skeletalanimation/md6phasetrack.h"
#include "shared/idlib/csystems/cvarsystemlocal.h"
#include "shared/idlib/lib.h"
#include "shared/idlib/math/mathlib.h"
#include "shared/idlib/math/random.h"
#include "shared/idlib/sys/sys_time.h"

#undef WOLFTNO_RETAIL_SYNCED_LEAF_INIT_ABI
#undef WOLFTNO_RETAIL_PONYTAIL_QUATERNION_MULTIPLY_ABI
#undef WOLFTNO_RETAIL_IDLIB_ERROR_RETURNS_ABI
#undef WOLFTNO_RETAIL_IDSTR_INLINE_FREEDATA_ABI
#undef WOLFTNO_RETAIL_IDMEM_SINGLETON_ABI
#undef WOLFTNO_RETAIL_IDLIST_NOINLINE_RESIZE_ABI
#undef WOLFTNO_RETAIL_IDLIST_EXACT_GROWTH_ABI
#undef WOLFTNO_RETAIL_IDLIST_NOINLINE_APPEND_ABI
#undef WOLFTNO_RETAIL_IDLIST_NOINLINE_ALLOC_ABI
#undef WOLFTNO_RETAIL_FIND_ALIAS_REFERENCE_ABI
#undef WOLFTNO_RETAIL_CVAR_INLINE_FREEDATA_ABI
#undef WOLFTNO_RETAIL_CHANNEL_BLENDPARMS_ABI
#undef WOLFTNO_RETAIL_ANIMWEB_SCALARMEM_PRIVATE_FREE_ABI
#undef WOLFTNO_RETAIL_ANIMWEB_RESTART_FRAME_INT_ABI
#undef WOLFTNO_RETAIL_ANIMWEB_GETSCALAR_REFERENCE_ABI
#undef WOLFTNO_RETAIL_ANIMATOR_ANIMWEB_INDEX_ABI
#undef WOLFTNO_RETAIL_ANIMWEB_GETNAME_POINTER_ABI
#undef WOLFTNO_RETAIL_ANIMWEB_DECLMD6_LAYOUT_ABI

#include <algorithm>
#include <cstdarg>
#include <cmath>
#include <cstdint>
#include <cstring>
#include <limits>
#include <malloc.h>
#include <new>
#include <intrin.h>
#include <xmmintrin.h>

extern bool forceIdListsToReallocateEveryAppend;

idCVar anim_autoWebTrace(
    "anim_autoWebTrace", "0", 0,
    "1 = automatically perform a PIX trace when the source and dest paths match",
    static_cast<argCompletionRef_t>(nullptr));
idCVar anim_sourcePath("anim_sourcePath", "hands_relaxed/idle", 0,
                       "source path for anim web timing trace",
                       static_cast<argCompletionRef_t>(nullptr));
idCVar anim_destPath("anim_destPath", "hands_actionScript/lean_over_rail", 0,
                     "dest path for anim web timing trace",
                     static_cast<argCompletionRef_t>(nullptr));
idCVar anim_verifyPaths(
    "anim_verifyPaths", "0", 4,
    "1 = verify paths for every change state at time of request\n, 2 = verify and assert on invalid paths",
    static_cast<argCompletionRef_t>(nullptr));
idCVar anim_webTimeThreshold("anim_webTimeThreshold", "1.0", 4,
                             "warn if a path exceeds this threshold",
                             static_cast<argCompletionRef_t>(nullptr));

const int idAnimator_AnimWeb::FORCED_EDGE_INDEX = 0x7FFF;
std::int16_t idAnimator_AnimWeb::curAnimWebHandle = 0;

bool operator!=(const idAtomicString& atomic, const char* const text) {
  if (text == nullptr)
    return atomic.str != idAtomicString::nullAtomicString;
  return atomic.str != text && idStr::Cmp(atomic.str, text) != 0;
}

struct djScratch_t {
  std::uint16_t cost;
  std::int16_t prevIndex;
};

template <typename NodeType, typename PriorityType, PriorityType MIN_PRIORITY>
class idBinaryHeap {
public:
  struct idHeapNode {
    NodeType node;
    PriorityType priority;
  };

  __forceinline explicit idBinaryHeap(int capacity)
      : nodes(nullptr), curSize(0), initialSize(capacity), numNodes(0),
        ordered(true), externalBuffer(false) {
    Resize(capacity);
    nodes[0].priority = MIN_PRIORITY;
  }

  __forceinline idBinaryHeap(void* buffer, const int capacity)
      : nodes(static_cast<idHeapNode*>(buffer)), curSize(capacity - 1),
        initialSize(capacity), numNodes(0), ordered(true),
        externalBuffer(true) {
    nodes[0].priority = MIN_PRIORITY;
  }

  ~idBinaryHeap() {
    if (externalBuffer) {
      nodes = nullptr;
    } else if (nodes != nullptr) {
      idMem::Singleton().Free(nodes, ALIGN_16);
      nodes = nullptr;
    }
  }

  __forceinline void Push(NodeType node, PriorityType priority) {
    if (numNodes == curSize) Resize(curSize * 2);
    int index = ++numNodes;
    while (nodes[index / 2].priority > priority) {
      nodes[index] = nodes[index / 2];
      index /= 2;
    }
    nodes[index].node = node;
    nodes[index].priority = priority;
  }

  __forceinline bool Pop(NodeType& node, PriorityType& priority) {
    if (numNodes == 0) return false;
    node = nodes[1].node;
    priority = nodes[1].priority;
    const idHeapNode last = nodes[numNodes--];
    int index = 1;
    while (index * 2 <= numNodes) {
      int child = index * 2;
      if (child != numNodes &&
          nodes[child + 1].priority < nodes[child].priority)
        ++child;
      if (nodes[child].priority >= last.priority) break;
      nodes[index] = nodes[child];
      index = child;
    }
    nodes[index] = last;
    return true;
  }

private:
  void Resize(int newSize) {
    idMem& memory = idMem::Singleton();
    idHeapNode* const replacement = static_cast<idHeapNode*>(
        memory.AllocWithLocation(
            "w:\\build\\tech5\\shared\\idlib\\Heap.h(76) : TAG_NEW",
            sizeof(idHeapNode) * static_cast<std::size_t>(newSize + 1),
            TAG_NEW, false, ALIGN_16, HEAP_DEFAULTHEAP));
    if (replacement != nullptr) {
      for (int index = 0; index <= newSize; ++index)
        replacement[index].priority = MIN_PRIORITY;
    }
    for (int index = 0; index <= numNodes; ++index)
      replacement[index] = nodes[index];
    memory.Free(nodes, ALIGN_16);
    nodes = replacement;
    curSize = newSize;
  }

  idHeapNode* nodes;
  int curSize;
  int initialSize;
  int numNodes;
  bool ordered;
  bool externalBuffer;
};

template <typename NodeHolder, typename NodeCache, typename NodeIndex,
          typename EdgeCache, typename Route>
class idDijkstra {
public:
  bool FindShortestPaths(const NodeHolder& nodeHolder,
                         const NodeIndex startNode,
                         const NodeIndex* destNodes, int numDestNodes,
                         const EdgeCache* edges, djScratch_t* scratchBuff,
                         int skipFlags, int requiredFlags) {
    const idDeclAnimWeb* const declaration = nodeHolder.decl;
    std::memset(scratchBuff, 0xFF,
                sizeof(djScratch_t) * declaration->nodes.num);
    int pathableNodeCount = 0;
    for (int listIndex = 0;
         listIndex < nodeHolder.pathableSubWebNodeLists.num;
         ++listIndex) {
      const idList<idAnimWebNodeIndex, 32>& nodeList =
          *nodeHolder.pathableSubWebNodeLists.list[listIndex];
      pathableNodeCount += nodeList.num;
      for (int nodeIndex = 0; nodeIndex < nodeList.num; ++nodeIndex)
        scratchBuff[nodeList.list[nodeIndex].value].cost &= 0x7FFFu;
    }
    scratchBuff[startNode.value].cost = 0;
    lastStartNode = -1;

    const int heapBufferCount = pathableNodeCount + 2;
    void* const heapBuffer =
        _alloca(sizeof(idBinaryHeap<int, int,
                                   std::numeric_limits<int>::min()>::idHeapNode) *
                static_cast<std::size_t>(heapBufferCount));
    idBinaryHeap<int, int, std::numeric_limits<int>::min()> queue(
        heapBuffer, heapBufferCount);
    queue.Push(startNode.value, 0);
    int destinationsFound = 0;
    int visitedNodes = 0;
    int queuedNode = 0;
    int queuedCost = 0;
    while (queue.Pop(queuedNode, queuedCost)) {
      (void)queuedCost;
      const std::int16_t nodeIndex = static_cast<std::int16_t>(queuedNode);
      const std::uint16_t currentCost = scratchBuff[nodeIndex].cost;
      if ((currentCost & 0x8000u) != 0) continue;
      for (int destinationIndex = 0; destinationIndex < numDestNodes;
           ++destinationIndex) {
        if (destNodes[destinationIndex].value == nodeIndex &&
            ++destinationsFound == numDestNodes) {
          lastStartNode = startNode.value;
          return true;
        }
      }

      scratchBuff[nodeIndex].cost = currentCost | 0x8000u;
      const NodeCache& node = declaration->nodeCache.list[nodeIndex];
      if ((node.flags & skipFlags) == 0 &&
          (node.flags & requiredFlags) == requiredFlags) {
        const std::int16_t* const edgeIndices =
            declaration->edgeIndexCache + node.edgeIndexOffset;
        for (int localEdge = 0; localEdge < node.numEdges; ++localEdge) {
          const int edgeIndex = edgeIndices[localEdge];
          const int destination = edges[edgeIndex].destNodeIndex.value;
          if ((scratchBuff[destination].cost & 0x8000u) != 0) continue;
          const float scale = edges[edgeIndex].weightScale * 0.0625f;
          const int edgeCost = nodeHolder.InternalGetEdgeCost(
              NodeIndex(nodeIndex), idAnimWebEdgeIndex(
                                       static_cast<std::int16_t>(edgeIndex)));
          const int totalCost =
              static_cast<int>(edgeCost * scale) + (currentCost & 0x7FFFu);
          if (static_cast<int>(scratchBuff[destination].cost & 0x7FFFu) <=
              totalCost)
            continue;
          scratchBuff[destination].cost = static_cast<std::uint16_t>(
              (scratchBuff[destination].cost & 0x8000u) |
              (totalCost & 0x7FFF));
          scratchBuff[destination].prevIndex = nodeIndex;
          queue.Push(destination, totalCost);
        }
      }
      if (++visitedNodes >= declaration->nodes.num) break;
    }
    lastStartNode = startNode.value;
    return true;
  }

  int lastStartNode;
};

namespace {

constexpr float kSmallestNormal = 1.17549435e-38f;
char kNoDeclSubWebName[1] = {};
char kNoNodeSubWebName[1] = {};

void DestroyBlendTree(idAnimWebBlendTree* treeValue);

bool IsBranchType(std::uint8_t type) {
  return type == idMD6Node::NODE_BRANCH ||
         type == idMD6Node::NODE_BLEND_BRANCH ||
         type == idMD6Node::NODE_BLENDA_BRANCH ||
         type == idMD6Node::NODE_FUSION_BRANCH;
}

const idMD6Node* BestLeafNode(const idMD6Node* node) {
  if (node == nullptr || node->type != idMD6Node::NODE_BEST_LEAF) return node;
  const auto* best = static_cast<const idMD6BestLeaf*>(node);
  if (best->leafList.num <= 0) return nullptr;
  const int index = best->bestLeafIndex < best->leafList.num
                        ? best->bestLeafIndex
                        : 0;
  return best->leafList.list[index];
}

idMD6Node* BestLeafNode(idMD6Node* node) {
  return const_cast<idMD6Node*>(BestLeafNode(
      static_cast<const idMD6Node*>(node)));
}

const idMD6Alias* StrongLoadAlias(const idDeclMD6* declaration,
                                  std::uint16_t handle) {
  const idAliasHandle_t aliasHandle(handle);
  using Function = const idMD6Alias*(__fastcall*)(
      idAnimation*, const idDeclMD6*, const idAliasHandle_t&);
  Function* const vtable = *reinterpret_cast<Function**>(animation);
  return vtable[35](animation, declaration, aliasHandle);
}

const idMD6Anim* ResolveAnimation(const idMD6AnimProps& properties,
                                  const idDeclMD6* declaration) {
  if (properties.aliasHandle == 0xFFFFu) return properties.anim;
  const idMD6Alias* const alias =
      StrongLoadAlias(declaration, properties.aliasHandle);
  return alias != nullptr ? alias->mappings.list[0].anim : nullptr;
}

float InverseLength(const idQuat& quaternion) {
  const __m128 ySquared =
      _mm_mul_ss(_mm_set_ss(quaternion.y), _mm_set_ss(quaternion.y));
  const __m128 xSquared =
      _mm_mul_ss(_mm_set_ss(quaternion.x), _mm_set_ss(quaternion.x));
  const __m128 zSquared =
      _mm_mul_ss(_mm_set_ss(quaternion.z), _mm_set_ss(quaternion.z));
  const __m128 wSquared =
      _mm_mul_ss(_mm_set_ss(quaternion.w), _mm_set_ss(quaternion.w));
  const __m128 squared = _mm_add_ss(
      _mm_add_ss(_mm_add_ss(ySquared, xSquared), zSquared), wSquared);
  const __m128 length = _mm_sqrt_ss(
      _mm_max_ss(squared, _mm_set_ss(kSmallestNormal)));
  return _mm_cvtss_f32(_mm_div_ss(_mm_set_ss(1.0f), length));
}

idQuat AnimationDeltaRotation(const idMD6Anim* animationValue) {
  if (animationValue == nullptr || animationValue->animData == nullptr)
    return quat_identity;
  const auto* quaternions =
      reinterpret_cast<const idQuat*>(animationValue->animData);
  idQuat inverseStart = quaternions[2];
  const float inverseLength = InverseLength(inverseStart);
  inverseStart.x = -inverseStart.x * inverseLength;
  inverseStart.y = -inverseStart.y * inverseLength;
  inverseStart.z = -inverseStart.z * inverseLength;
  inverseStart.w = quaternions[2].w;
  return quaternions[5] * inverseStart;
}

idVec3 AnimationDeltaTranslation(const idMD6Anim* animationValue) {
  if (animationValue == nullptr || animationValue->animData == nullptr)
    return vec3_origin;
  const auto* start = reinterpret_cast<const idVec3*>(
      &animationValue->animData->startDelta[8]);
  const auto* end = reinterpret_cast<const idVec3*>(
      &animationValue->animData->endDelta[8]);
  return *end - *start;
}

int EventNumber(const idEventDef& eventDefinition) {
  return *reinterpret_cast<const int*>(
      reinterpret_cast<const std::uint8_t*>(&eventDefinition) + 0x2C);
}

idAnimWebNodeIndex FindNodeIndex(const idDeclAnimWeb* declaration,
                                 idAnimWebSubWebIndex subWebIndex,
                                 idAnimWebStateIndex stateIndex) {
  if (declaration == nullptr) return idAnimWebNodeIndex{};
  return declaration->FindNodeIndex(subWebIndex, stateIndex);
}

bool FindRouteToNodes(const idAnimator_AnimWeb& constantWeb,
                      idAnimWebNodeIndex startNodeIndex,
                      const idAnimWebNodeIndex* destinationNodes,
                      int destinationCount,
                      idAnimWebSubWebIndex destinationSubWebIndex,
                      idAnimWebStateIndex destinationStateIndex,
                      idAnimWebSubWebIndex otherSubWebIndex,
                      bool rejectNegativeStart,
                      bool emptyNegativeOtherSubWeb,
                      idAnimWebRoute& outputRoute) {
  if (constantWeb.decl == nullptr) return false;
  if (rejectNegativeStart && startNodeIndex.value < 0) return false;
  idAnimator_AnimWeb& web =
      const_cast<idAnimator_AnimWeb&>(constantWeb);
  web.pathableSubWebNodeLists.num = 0;
  const idDeclAnimWebNode* const startNode =
      startNodeIndex.value == -1
          ? nullptr
          : web.decl->nodes.list[startNodeIndex.value];
  const std::int16_t startSubWebIndex = startNode->subWebIndex.value;
  for (int index = 0; index < web.decl->subWebs.num; ++index) {
    const idDeclAnimWeb::idSubWebInfo& subWeb = web.decl->subWebs.list[index];
    if (!subWeb.isHub && index != startSubWebIndex &&
        index != destinationSubWebIndex.value &&
        index != otherSubWebIndex.value)
      continue;
    const auto* const nodeList = reinterpret_cast<
        const idList<idAnimWebNodeIndex, 32>*>(&subWeb.nodes);
    web.pathableSubWebNodeLists.Append(nodeList);
  }

  const std::uint8_t skipFlags = web.skipFlags;
  const std::uint8_t requiredFlags = web.requiredFlags;
  web.skipFlags = 0;
  web.requiredFlags = 0;
  auto* const scratch = static_cast<djScratch_t*>(
      _alloca(sizeof(djScratch_t) *
              static_cast<std::size_t>(web.decl->nodes.num)));
  idDijkstra<idAnimator_AnimWeb, idDeclAnimWeb::idNodeCache,
             idAnimWebNodeIndex, idDeclAnimWeb::idEdgeCache,
             idAnimWebRoute>
      dijkstra;
  const int startTicks = static_cast<int>(Sys_GetClockTicks());
  const bool foundPaths = dijkstra.FindShortestPaths(
      web, startNodeIndex, destinationNodes, destinationCount,
      web.decl->edgeCache.list, scratch, skipFlags, requiredFlags);
  const float elapsedMilliseconds =
      static_cast<float>(
          static_cast<int>(Sys_GetClockTicks()) - startTicks) /
      (static_cast<float>(static_cast<int>(Sys_ClockTicksPerSecond())) *
       0.001f);
  if (elapsedMilliseconds > anim_webTimeThreshold.valueFloat) {
    const idDeclAnimWebNode* const diagnosticStartNode =
        web.decl->nodes.list[startNodeIndex.value];
    idLib::Printf(
        "animWeb '%s' - slow path (%.8f ms) from '%s:%s' to '%s:%s' other '%s'\n",
        web.decl->name.str,
        elapsedMilliseconds,
        web.decl->GetSubWebName(diagnosticStartNode->subWebIndex),
        web.decl->GetStateName(diagnosticStartNode->states.stateIndex[0]),
        web.decl->GetSubWebName(destinationSubWebIndex),
        web.decl->GetStateName(destinationStateIndex),
        emptyNegativeOtherSubWeb && otherSubWebIndex.value < 0
            ? ""
            : web.decl->GetSubWebName(otherSubWebIndex));
  }
  if (!foundPaths)
    return false;

  int bestRouteIndex = -1;
  int bestCost = std::numeric_limits<int>::max();
  idAnimWebRoute candidateRoutes[8];
  for (int destinationIndex = 0; destinationIndex < destinationCount;
       ++destinationIndex) {
    idAnimWebRoute& route = candidateRoutes[destinationIndex];
    route.num = 0;
    route.cost = std::numeric_limits<int>::max();
    if (dijkstra.lastStartNode != startNodeIndex.value ||
        destinationNodes[destinationIndex].value == startNodeIndex.value)
      continue;
    std::int16_t cursor = destinationNodes[destinationIndex].value;
    route.path[route.num++].value = cursor;
    route.cost = scratch[cursor].cost;
    while (route.num < idAnimWebRoute::MAX_PATH_LEN) {
      cursor = scratch[cursor].prevIndex;
      if (cursor < 0) {
        route.num = 0;
        route.cost = std::numeric_limits<int>::max();
        break;
      }
      route.path[route.num++].value = cursor;
      if (cursor == startNodeIndex.value) break;
    }
    if (route.num < idAnimWebRoute::MAX_PATH_LEN &&
        route.path[route.num - 1].value != startNodeIndex.value) {
      route.num = 0;
      route.cost = std::numeric_limits<int>::max();
      continue;
    }
    route.Invert();
    if (route.cost < bestCost) {
      bestCost = route.cost;
      bestRouteIndex = destinationIndex;
    }
  }
  if (bestRouteIndex < 0) return false;
  outputRoute.AppendPath(candidateRoutes[bestRouteIndex]);
  return true;
}

} // namespace

int idRandom2::RandomInt(int maximum) {
  if (maximum == 0) return 0;
  seed = 1664525u * seed + 1013904223u;
  return static_cast<int>((seed >> 10) & 0x7FFFu) % maximum;
}

namespace {

__forceinline float OrderedMultiply(float left, float right) {
  return _mm_cvtss_f32(
      _mm_mul_ss(_mm_set_ss(left), _mm_set_ss(right)));
}

__forceinline float OrderedAdd(float left, float right) {
  return _mm_cvtss_f32(
      _mm_add_ss(_mm_set_ss(left), _mm_set_ss(right)));
}

__forceinline float OrderedSubtract(float left, float right) {
  return _mm_cvtss_f32(
      _mm_sub_ss(_mm_set_ss(left), _mm_set_ss(right)));
}

} // namespace

idQuat idQuat::operator*(const idQuat& rhs) const {
  idQuat result;
  result.x = OrderedSubtract(
      OrderedAdd(
          OrderedAdd(OrderedMultiply(x, rhs.w),
                     OrderedMultiply(rhs.x, w)),
          OrderedMultiply(y, rhs.z)),
      OrderedMultiply(z, rhs.y));
  result.y = OrderedSubtract(
      OrderedAdd(
          OrderedAdd(OrderedMultiply(y, rhs.w),
                     OrderedMultiply(rhs.y, w)),
          OrderedMultiply(z, rhs.x)),
      OrderedMultiply(rhs.z, x));
  result.w = OrderedSubtract(
      OrderedSubtract(
          OrderedSubtract(OrderedMultiply(rhs.w, w),
                          OrderedMultiply(x, rhs.x)),
          OrderedMultiply(rhs.y, y)),
      OrderedMultiply(z, rhs.z));
  result.z = OrderedSubtract(
      OrderedAdd(
          OrderedAdd(OrderedMultiply(z, rhs.w),
                     OrderedMultiply(rhs.z, w)),
          OrderedMultiply(rhs.y, x)),
      OrderedMultiply(y, rhs.x));
  return result;
}

idQuat& idQuat::Normalize() {
  const float inverse = InverseLength(*this);
  w *= inverse;
  x *= inverse;
  y *= inverse;
  z *= inverse;
  return *this;
}

void idAnimWebRoute::AppendPath(const idAnimWebRoute& other) {
  int sourceIndex = 0;
  if (num > 0 && path[num - 1].value == other.path[0].value) {
    const std::int16_t duplicate = path[num - 1].value;
    do {
      ++sourceIndex;
    } while (other.path[sourceIndex].value == duplicate);
  }
  while (sourceIndex < other.num && num < MAX_PATH_LEN) {
    path[num++] = other.path[sourceIndex++];
  }
  cost += other.cost;
}

void idAnimator_AnimWeb::DebugPrint(const char* format, ...) const {
  std::va_list arguments;
  va_start(arguments, format);
  if (debug) idLib::VPrintf(format, arguments);
  va_end(arguments);
}

const idMD6Branch* idAnimator_AnimWeb::InternalGetMergeBranch() const {
  return mergeBranch;
}

void idAnimator_AnimWeb::ClearEvent(animWebEvent_t eventType) {
  eventHandler.ClearEvent(eventType);
}

void idAnimator_AnimWeb::ClearEvents() {
  eventHandler.ClearEvents();
}

bool idAnimator_AnimWeb::IsAnyEventPending() const {
  return eventHandler.IsAnyEventPending();
}

bool idAnimator_AnimWeb::DecomposeAnimRef(const char* animRef,
                                          idStr& subWebName,
                                          idStr& stateName) const {
  idStr webName;
  idAnimWebPath::SplitPath(animRef, webName, subWebName, stateName);
  return InternalRemapNames(subWebName, stateName);
}

idAnimWebPath idAnimator_AnimWeb::GetDestWebPath(bool includeWebName) const {
  idAnimWebPath result;
  result.SetPath(includeWebName ? decl->name.str : "",
                 destNodeSubWebName.str, destNodeStateName.str);
  return result;
}

void idAnimator_AnimWeb::SetEvent(animWebEvent_t eventType,
                                  idAnimWebSubWebIndex subWebIndex,
                                  idAnimWebStateIndex stateIndex,
                                  const idEventDef& eventDef,
                                  idAnimWebEvent::priority_t priority) {
  eventHandler.SetEvent(*this, subWebIndex, stateIndex, eventType,
                        EventNumber(eventDef), priority);
}

void idAnimator_AnimWeb::TriggerEvent(const idDeclAnimWebNode* node,
                                      animWebEvent_t eventType,
                                      const idAnimWebEvent& event,
                                      int eventParm) {
  InternalTriggerEvent(node, eventType, handle, event.eventNum, eventParm);
}

idAnimWebBlendTree*
idAnimator_AnimWeb::idModelAnimState::GetBlendTree(
    idAnimWebNodeIndex nodeIndex) {
  for (int index = 0; index < blendTreeCache.num; ++index) {
    if (blendTreeCache.list[index]->nodeIndex.value == nodeIndex.value)
      return blendTreeCache.list[index];
  }
  return nullptr;
}

void idAnimator_AnimWeb::idModelAnimState::FreeBlendTree(
    const idAnimWebNodeIndex& nodeIndex) const {
  auto& cache = const_cast<idStaticList<idAnimWebBlendTree*,
                                        MAX_BLEND_TREES>&>(blendTreeCache);
  for (int index = 0; index < cache.num; ++index) {
    idAnimWebBlendTree* treeValue = cache.list[index];
    if (treeValue->nodeIndex.value != nodeIndex.value) continue;
    DestroyBlendTree(treeValue);
    cache.RemoveIndexFast(index);
    return;
  }
}

bool idAnimator_AnimWeb::IsTransitioning() const {
  return curEdgeNodeIndex.value >= 0 && curEdgeIndex >= 0;
}

bool idAnimator_AnimWeb::InternalIsContributing() const {
  const idMD6Node* const treeValue = modelAnimStates.list[0].tree;
  return treeValue != nullptr && treeValue->IsValid();
}

void idAnimator_AnimWeb::InternalUpdateBlendEvents(
    const idAnimStack* stack, int curTime, int ticksPerSec) {
  (void)stack;
  if (decl == nullptr || blendEventMask == 0 || curNodeIndex.value == -1)
    return;
  const idDeclAnimWebNode* const current =
      decl->nodes.list[curNodeIndex.value];
  if (current == nullptr) return;
  decl->GetSubWebName(current->subWebIndex);
  decl->GetStateName(current->states.stateIndex[0]);
  for (int localEdgeIndex = 0; localEdgeIndex < current->edges.num;
       ++localEdgeIndex) {
    const int edgeIndex = current->edges.list[localEdgeIndex];
    const idAnimWebNodeIndex destination(
        decl->edgeCache.list[edgeIndex].destNodeIndex.value);
    if (destination.value == -1) continue;
    const idDeclAnimWebNode* const destinationNode =
        decl->nodes.list[destination.value];
    if (destinationNode == nullptr ||
        (destinationNode->customFlags & blendEventMask) == 0)
      continue;
    decl->GetSubWebName(destinationNode->subWebIndex);
    decl->GetStateName(destinationNode->states.stateIndex[0]);
    const idDeclAnimWebEdge& edge = decl->edges.list[edgeIndex];
    if (InternalShouldStartNextNode(
            curNodeIndex, destination, curTime, ticksPerSec,
            edge.blendParms, idAnimWebModelIndex(0), false, nullptr)) {
      InternalTriggerBlendEvent(destinationNode->customFlags,
                                idAnimWebModelIndex(0), curNodeIndex,
                                destination, curTime);
    }
  }
}

const idMD6Leaf* idAnimator_AnimWeb::FindFirstLeaf(
    const idMD6Node* treeValue, int curTime, bool stillPlaying) const {
  const idMD6Node* stack[32];
  int stackIndex = 0;
  stack[0] = treeValue;
  while (stackIndex >= 0) {
    const idMD6Node* node = stack[stackIndex--];
    if (IsBranchType(node->type)) {
      const auto* branch = static_cast<const idMD6Branch*>(node);
      if (branch->left != nullptr &&
          (!stillPlaying || !branch->IsBlendFinished()))
        stack[++stackIndex] = branch->left;
      if (branch->right != nullptr &&
          (!stillPlaying || !branch->IsBlendFinished()))
        stack[++stackIndex] = branch->right;
      continue;
    }
    if (node->type == idMD6Node::NODE_LEAF_PLAY) {
      const auto* leaf = static_cast<const idMD6LeafPlay*>(node);
      if (leaf->IsPlaying(curTime, timeManager.gameTimePerSecond.value) ||
          !stillPlaying)
        return leaf;
      continue;
    }
    if (node->type == idMD6Node::NODE_LEAF_PAUSE) {
      if (!stillPlaying) return static_cast<const idMD6Leaf*>(node);
      continue;
    }
    if (node->type == idMD6Node::NODE_BEST_LEAF) {
      const idMD6Node* best = BestLeafNode(node);
      if (best != nullptr) stack[++stackIndex] = best;
      continue;
    }
    return nullptr;
  }
  return nullptr;
}

const idMD6Leaf* idAnimator_AnimWeb::FindFirstLeafForNode(
    idAnimWebModelIndex modelIndex, idAnimWebNodeIndex nodeIndex,
    int curTime, bool stillPlaying) const {
  const idAnimWebBlendTree* const blendTree =
      modelAnimStates.list[modelIndex.value].GetBlendTree(nodeIndex);
  return blendTree != nullptr
             ? FindFirstLeaf(blendTree->tree, curTime, stillPlaying)
             : nullptr;
}

void idAnimator_AnimWeb::CacheCurNode() {
  if (decl != nullptr && curNodeIndex.value >= 0 &&
      decl->nodes.list[curNodeIndex.value] != nullptr) {
    const idDeclAnimWebNode* const node = decl->nodes.list[curNodeIndex.value];
    curNodeStateName.Set(decl->GetStateName(node->states.stateIndex[0]));
    curNodeSubWebName.Set(decl->GetSubWebName(node->subWebIndex));
  } else {
    curNodeStateName.Set("");
    curNodeSubWebName.Set("");
  }
}

void idAnimator_AnimWeb::CacheDestNode() {
  destNodeStateName.Set("");
  destNodeSubWebName.Set("");
  const idAnimWebNodeIndex destination = curPath.GetLastNodeIndex();
  if (decl == nullptr || destination.value < 0) return;
  const idDeclAnimWebNode* const node = decl->nodes.list[destination.value];
  const char* subWebName = "";
  if (node != nullptr) {
    destNodeStateName.Set(decl->GetStateName(node->states.stateIndex[0]));
    subWebName = decl->GetSubWebName(node->subWebIndex);
  } else {
    destNodeStateName.Set("");
  }
  destNodeSubWebName.Set(subWebName);
}

bool idAnimator_AnimWeb::GetCurEdgeInfo(
    idAnimWebSubWebIndex& subWebIndex, idAnimWebNodeIndex& nodeIndex,
    blendParms_t& blendParameters) const {
  if (curEdgeNodeIndex.value < 0 || curEdgeIndex < 0 || decl == nullptr)
    return false;
  if (curEdgeIndex == FORCED_EDGE_INDEX) {
    subWebIndex = forcedEdge.destSubWebIndex;
    nodeIndex = forcedEdge.destNodeIndex;
    blendParameters = forcedEdge.blendParms;
    return true;
  }
  const idDeclAnimWebNode* const node =
      decl->nodes.list[curEdgeNodeIndex.value];
  if (node == nullptr || node->edges.num <= 0) return false;
  const int edgeIndex = node->edges.list[curEdgeIndex];
  if (edgeIndex < 0) return false;
  subWebIndex.value = decl->edges.list[edgeIndex].destSubWebIndex.value;
  blendParameters = decl->edges.list[edgeIndex].blendParms;
  nodeIndex.value = decl->edgeCache.list[edgeIndex].destNodeIndex.value;
  return true;
}

const char* idAnimator_AnimWeb::GetCurSubWebName() const {
  if (decl == nullptr) return kNoDeclSubWebName;
  if (curNodeIndex.value == -1 ||
      decl->nodes.list[curNodeIndex.value] == nullptr)
    return kNoNodeSubWebName;
  return decl->GetSubWebName(decl->nodes.list[curNodeIndex.value]->subWebIndex);
}

idAnimWebSubWebIndex idAnimator_AnimWeb::GetCurSubWebIndex() const {
  if (decl != nullptr && curNodeIndex.value >= 0 &&
      decl->nodes.list[curNodeIndex.value] != nullptr)
    return idAnimWebSubWebIndex(
        decl->nodes.list[curNodeIndex.value]->subWebIndex.value);
  return idAnimWebSubWebIndex{};
}

const char* idAnimator_AnimWeb::GetCurStateName() const {
  if (decl == nullptr || curNodeIndex.value == -1 ||
      decl->nodes.list[curNodeIndex.value] == nullptr) return "<NULL>";
  return decl->GetStateName(
      decl->nodes.list[curNodeIndex.value]->states.stateIndex[0]);
}

idAnimWebPath idAnimator_AnimWeb::GetCurWebPath(bool includeWebName) const {
  idAnimWebPath result;
  result.SetPath(includeWebName ? decl->name.str : "", GetCurSubWebName(),
                 GetCurStateName());
  return result;
}

idAnimWebStateIndex idAnimator_AnimWeb::GetCurStateIndex() const {
  if (decl != nullptr && curNodeIndex.value >= 0 &&
      decl->nodes.list[curNodeIndex.value] != nullptr)
    return idAnimWebStateIndex(
        decl->nodes.list[curNodeIndex.value]->states.stateIndex[0].value);
  return idAnimWebStateIndex{};
}

bool idAnimator_AnimWeb::GetNodeAnimLength(
    int curTime, idAnimWebModelIndex modelIndex,
    idAnimWebNodeIndex nodeIndex, idMD6Leaf::wrapMode_t wrapMode,
    bool ignoreAdditive, animInfo_t& animInfo) const {
  if (decl == nullptr) return false;
  const idAnimWebBlendTree* const blendTree =
      modelAnimStates.list[modelIndex.value].GetBlendTree(nodeIndex);
  if (blendTree != nullptr) {
    return blendTree->GetAnimLength(
        curTime, timeManager.gameTimePerSecond.value, wrapMode,
        ignoreAdditive, animInfo.curFrame, animInfo.numFrames,
        animInfo.frameRate);
  }
  animInfo.curFrame = 0;
  if (nodeIndex.value == -1) return false;
  const idDeclAnimWebNode* const node = decl->nodes.list[nodeIndex.value];
  if (node == nullptr) return false;
  const idDeclMD6* const model = modelDecls.list[modelIndex.value];
  if (model == nullptr) return false;
  const idDeclAnimWebNode::idModelInfo* const modelInfo =
      node->FindModelInfo(idAnimWebModelIndex(modelIndex.value));
  if (modelInfo == nullptr) return false;

  int longestLength = -1;
  for (int index = 0; index < modelInfo->animProps.num; ++index) {
    const idMD6AnimProps& properties = modelInfo->animProps.list[index];
    if (wrapMode != idMD6Leaf::WRAP_INVALID &&
        static_cast<int>(properties.wrap) != static_cast<int>(wrapMode))
      continue;
    const idMD6Anim* const anim = ResolveAnimation(properties, model);
    if (anim == nullptr || anim->animData == nullptr ||
        (ignoreAdditive && (anim->animData->flags & 1u) != 0))
      continue;
    const int length =
        1000 * anim->animData->numFrames / anim->animData->frameRate;
    if (length > longestLength) {
      animInfo.numFrames = anim->animData->numFrames;
      const idMD6AnimData* const currentData = anim->animData;
      animInfo.frameRate = currentData != nullptr ? currentData->frameRate : 30;
      longestLength = length;
    }
  }
  if (longestLength >= 0) return true;
  idStr nodeName;
  node->GetName(decl, nodeName);
  idLib::Warning(
      "idAnimator_AnimWeb::GetNodeAnimLength - no valid animations at node '%s' in web '%s'",
      nodeName.data, decl->name.str);
  return false;
}

bool idAnimator_AnimWeb::GetNodeAnimLength(
    int curTime, idAnimWebModelIndex modelIndex,
    idAnimWebSubWebIndex subWebIndex, idAnimWebStateIndex stateIndex,
    idMD6Leaf::wrapMode_t wrapMode, bool ignoreAdditive,
    animInfo_t& animInfo) const {
  if (decl == nullptr) return false;
  return GetNodeAnimLength(curTime, modelIndex,
                           FindNodeIndex(decl, subWebIndex, stateIndex),
                           wrapMode, ignoreAdditive, animInfo);
}

bool idAnimator_AnimWeb::GetNodeTotalMovement(
    idAnimWebModelIndex modelIndex, idAnimWebSubWebIndex subWebIndex,
    idAnimWebStateIndex stateIndex, idVec3& translation,
    idQuat& rotation) const {
  if (decl == nullptr) return false;
  const idAnimWebNodeIndex nodeIndex =
      FindNodeIndex(decl, subWebIndex, stateIndex);
  const idAnimWebBlendTree* const blendTree =
      modelAnimStates.list[modelIndex.value].GetBlendTree(nodeIndex);
  if (blendTree != nullptr) {
    for (int index = 0; index < blendTree->leaves.num; ++index) {
      const idMD6LeafPlay* const leaf = blendTree->leaves.list[index];
      if (leaf == nullptr || (leaf->anim->animData->flags & 1u) != 0)
        continue;
      translation = AnimationDeltaTranslation(leaf->anim);
      rotation = AnimationDeltaRotation(leaf->anim);
      return true;
    }
  }
  if (nodeIndex.value == -1) return false;
  const idDeclAnimWebNode* const node = decl->nodes.list[nodeIndex.value];
  if (node == nullptr) return false;
  const idDeclMD6* const model = modelDecls.list[modelIndex.value];
  if (model == nullptr) return false;
  const idDeclAnimWebNode::idModelInfo* const modelInfo =
      node->FindModelInfo(idAnimWebModelIndex(modelIndex.value));
  if (modelInfo == nullptr) return false;
  for (int index = 0; index < modelInfo->animProps.num; ++index) {
    const idMD6Anim* const anim =
        ResolveAnimation(modelInfo->animProps.list[index], model);
    if (anim == nullptr || anim->animData == nullptr ||
        (anim->animData->flags & 1u) != 0)
      continue;
    translation = AnimationDeltaTranslation(anim);
    rotation = AnimationDeltaRotation(anim);
    return true;
  }
  return false;
}

namespace {

template <typename Visitor>
bool VisitTreeLeaves(const idMD6Node* root, Visitor&& visitor) {
  const idMD6Node* stack[32];
  int stackIndex = 0;
  stack[0] = root;
  while (stackIndex >= 0) {
    const idMD6Node* node = stack[stackIndex--];
    if (IsBranchType(node->type)) {
      const auto* branch = static_cast<const idMD6Branch*>(node);
      if (branch->left != nullptr) stack[++stackIndex] = branch->left;
      if (branch->right != nullptr) stack[++stackIndex] = branch->right;
      continue;
    }
    if (node->type == idMD6Node::NODE_BEST_LEAF) {
      const idMD6Node* best = BestLeafNode(node);
      if (best != nullptr) stack[++stackIndex] = best;
      continue;
    }
    if (node->type != idMD6Node::NODE_LEAF_PLAY &&
        node->type != idMD6Node::NODE_LEAF_PAUSE) return false;
    if (!visitor(static_cast<const idMD6Leaf*>(node))) return true;
  }
  return true;
}

idAnimWebBlendTree* AllocateBlendTree(idAnimWebNodeIndex nodeIndex) {
  idMem& memory = idMem::Singleton();
  void* const storage = memory.AllocWithLocation(
      "w:\\build\\tech5\\shared\\idlib\\Heap.h(53) : TAG_NEW",
      sizeof(idAnimWebBlendTree), TAG_NEW, false, ALIGN_16,
      HEAP_DEFAULTHEAP);
  return storage != nullptr ? ::new (storage) idAnimWebBlendTree(nodeIndex)
                            : nullptr;
}

void DestroyBlendTree(idAnimWebBlendTree* treeValue) {
  if (treeValue == nullptr) return;
  treeValue->~idAnimWebBlendTree();
  idMem::Singleton().Free(treeValue, ALIGN_16);
}

} // namespace

template <typename Type>
Type WrappedLerp(const Type rangeMinimum, const Type rangeMaximum,
                 Type from, Type to, Type fraction) {
  const Type difference = to - from;
  const Type directDistance = std::fabs(difference);
  const Type wrappedDistance =
      (rangeMaximum - rangeMinimum) - directDistance;
  if (wrappedDistance >= directDistance) return difference * fraction + from;
  if (from > to) {
    std::swap(from, to);
    fraction = Type(1) - fraction;
  }
  const Type split = (from - rangeMinimum) / wrappedDistance;
  if (split < fraction) {
    return ((Type(1) - fraction) / (Type(1) - split)) *
               (to - rangeMaximum) +
           rangeMaximum;
  }
  return (rangeMinimum - from) * (fraction / split) + from;
}

template float WrappedLerp<float>(const float, const float, float, float,
                                  float);

void idAnimator_AnimWeb::idModelAnimState::Free(idAnimator_AnimWeb* web) {
  for (idMD6Branch*& branch : branches) {
    if (branch != nullptr) {
      idMD6Allocator::FreeTyped(web->md6Allocator, branch);
      branch = nullptr;
    }
  }
  for (int index = 0; index < blendTreeCache.num; ++index)
    DestroyBlendTree(blendTreeCache.list[index]);
  blendTreeCache.ClearFree();
  tree = nullptr;
  blending = false;
}

bool idAnimator_AnimWeb::idModelAnimState::BeginNodePlaying(
    idAnimator_AnimWeb* web,
    const idDeclAnimWebNode::idModelInfo* modelInfo,
    idAnimWebNodeIndex nodeIndex, int curTime, int ticksPerSec) {
  if (modelInfo == nullptr) {
    tree = nullptr;
    blending = false;
    return true;
  }
  idAnimWebBlendTree* const blendTree = GetBlendTree(nodeIndex);
  if (blendTree == nullptr) return false;
  blendTree->BeginPlaying(web, modelInfo->animProps.list,
                          modelInfo->animProps.num, curTime, ticksPerSec);
  tree = blendTree->tree;
  blending = false;
  recentNodes[recentNodeStart] = nodeIndex;
  recentNodeStart = (recentNodeStart + 1) % MAX_RECENT;
  return true;
}

void idAnimator_AnimWeb::idModelAnimState::SetDecl(
    const idDeclAnimWeb* declaration, idAnimator_AnimWeb* web,
    idAnimWebModelIndex modelIndex, int) {
  Free(web);
  for (idMD6Branch*& branch : branches) {
    branch = static_cast<idMD6Branch*>(
        web->md6Allocator->Alloc(idMD6Node::NODE_BRANCH));
  }

  const auto preloadNode = [&](const idDeclAnimWebNode* node) {
    if (node == nullptr) return;
    const idDeclAnimWebNode::idModelInfo* const modelInfo =
        node->FindModelInfo(idAnimWebModelIndex(modelIndex.value));
    if (modelInfo == nullptr) return;
    const idDeclMD6* const model = web->modelDecls.list[modelIndex.value];
    for (int propertyIndex = 0;
         propertyIndex < modelInfo->animProps.num; ++propertyIndex) {
      const idMD6AnimProps& property = modelInfo->animProps.list[propertyIndex];
      if (property.aliasHandle != 0xFFFFu)
        StrongLoadAlias(model, property.aliasHandle);
    }
  };

  if (declaration->layers.num != 0) {
    for (int activeIndex = 0;
         activeIndex < web->initialActiveSubWebs.num; ++activeIndex) {
      const int subWebIndex =
          web->initialActiveSubWebs.list[activeIndex].value;
      if (subWebIndex < 0 || subWebIndex >= declaration->subWebs.num)
        continue;
      const auto& nodes = declaration->subWebs.list[subWebIndex].nodes;
      for (int nodeIndex = 0; nodeIndex < nodes.num; ++nodeIndex) {
        const int index = nodes.list[nodeIndex].value;
        if (index != -1)
          preloadNode(declaration->nodes.list[index]);
      }
    }
  } else {
    for (int nodeIndex = 0; nodeIndex < declaration->nodes.num; ++nodeIndex) {
      const std::int16_t narrowedIndex =
          static_cast<std::int16_t>(nodeIndex);
      if (narrowedIndex != -1)
        preloadNode(declaration->nodes.list[narrowedIndex]);
    }
  }
}

void idAnimator_AnimWeb::idModelAnimState::SetDestFrame(
    idMD6Node* node, const blendParms_t& blendParameters,
    int curTime, int ticksPerSec, std::uint8_t phase,
    unsigned int& longestAnimLength, int& destFrame) const {
  if (IsBranchType(node->type)) {
    auto* branch = static_cast<idMD6Branch*>(node);
    SetDestFrame(branch->left, blendParameters, curTime, ticksPerSec,
                 phase, longestAnimLength, destFrame);
    SetDestFrame(branch->right, blendParameters, curTime, ticksPerSec,
                 phase, longestAnimLength, destFrame);
    return;
  }
  if (node->type == idMD6Node::NODE_BEST_LEAF) {
    const auto* best = static_cast<const idMD6BestLeaf*>(node);
    if (best->leafList.num <= 0) return;
    const int index = best->bestLeafIndex < best->leafList.num
                          ? best->bestLeafIndex
                          : 0;
    idMD6Node* const selected = best->leafList.list[index];
    if (selected != nullptr)
      SetDestFrame(selected, blendParameters, curTime, ticksPerSec,
                   phase, longestAnimLength, destFrame);
    return;
  }
  if (node->type == idMD6Node::NODE_LEAF_PLAY) {
    auto* leaf = static_cast<idMD6LeafPlay*>(node);
    std::int16_t first;
    std::int16_t duration;
    blendParameters.GetDestFrames(leaf->anim, first, duration);
    const auto* const phaseTrack =
        reinterpret_cast<const idMD6PhaseTrack*>(leaf->anim->phaseTrack);
    if ((blendParameters.parms.flags & 0x10u) != 0 &&
        phaseTrack->IsValid(leaf->anim)) {
      std::int16_t phaseFrame = first;
      if (phaseTrack->GetFrame(phase, phaseFrame)) first = phaseFrame;
    }
    leaf->SetFrame(curTime, ticksPerSec, static_cast<std::uint16_t>(first));
    const unsigned int length =
        static_cast<unsigned int>(leaf->GetAnimLength(ticksPerSec, true));
    if (length > longestAnimLength) {
      destFrame = first;
      longestAnimLength = length;
    }
    return;
  }
  if (node->type == idMD6Node::NODE_LEAF_PAUSE) {
    auto* leaf = static_cast<idMD6LeafPause*>(node);
    std::int16_t first;
    std::int16_t duration;
    blendParameters.GetDestFrames(leaf->anim, first, duration);
    leaf->frame =
        static_cast<float>(first) / leaf->anim->animData->numFrames;
  }
}

void idAnimator_AnimWeb::idModelAnimState::GetPhase(
    const idMD6Node* node, int curTime, int ticksPerSec,
    std::uint8_t& phase) const {
  if (IsBranchType(node->type)) {
    const auto* branch = static_cast<const idMD6Branch*>(node);
    std::uint8_t leftPhase;
    std::uint8_t rightPhase;
    GetPhase(branch->left, curTime, ticksPerSec, leftPhase);
    GetPhase(branch->right, curTime, ticksPerSec, rightPhase);
    const float blendFraction = branch->currentAlpha -
                                std::floor(branch->currentAlpha);
    const float blended = WrappedLerp<float>(
        0.0f, 256.0f, static_cast<float>(leftPhase),
        static_cast<float>(rightPhase), blendFraction);
    const int result = static_cast<int>(std::fmod(blended, 255.0f));
    phase = static_cast<std::uint8_t>(std::clamp(result, 0, 255));
    return;
  }
  if (node->type == idMD6Node::NODE_BEST_LEAF) {
    const auto* best = static_cast<const idMD6BestLeaf*>(node);
    if (best->leafList.num <= 0) return;
    const int index = best->bestLeafIndex < best->leafList.num
                          ? best->bestLeafIndex
                          : 0;
    const idMD6Node* const selected = best->leafList.list[index];
    if (selected != nullptr)
      GetPhase(selected, curTime, ticksPerSec, phase);
    return;
  }
  if (node->type == idMD6Node::NODE_LEAF_PLAY) {
    const auto* leaf = static_cast<const idMD6LeafPlay*>(node);
    const auto* const phaseTrack =
        reinterpret_cast<const idMD6PhaseTrack*>(leaf->anim->phaseTrack);
    if (phaseTrack->IsValid(leaf->anim)) {
      phaseTrack->GetPhase(leaf->GetFrame(curTime, ticksPerSec), phase);
    }
    return;
  }
  if (node->type == idMD6Node::NODE_LEAF_PAUSE) {
    const auto* leaf = static_cast<const idMD6LeafPause*>(node);
    const auto* const phaseTrack =
        reinterpret_cast<const idMD6PhaseTrack*>(leaf->anim->phaseTrack);
    if (phaseTrack->IsValid(leaf->anim)) {
      phaseTrack->GetPhase(static_cast<int>(leaf->frame), phase);
    }
  }
}

void idAnimator_AnimWeb::Free() {
  scalars.ClearFree();
  for (int index = 0; index < modelAnimStates.num; ++index)
    modelAnimStates.list[index].Free(this);
  destStateIndices.numStates = 0;
  nodeStartFrame = 0;
  decl = nullptr;
  curEdgeNodeIndex.value = -1;
  curEdgeIndex = -1;
  destSubWebIndex.value = -1;
  pathIndex = -1;
  nodeStartTime = -1;
  prevNodeStartTime = -1;
  curNodeIndex.value = -1;
  prevNodeIndex.value = -1;
  curPath.cost = std::numeric_limits<int>::max();
  curPath.num = 0;
  forcedEdge.destNodeIndex.value = -1;
  forcedEdge.destSubWebIndex.value = -1;
  handle.value = -1;
  CacheCurNode();
  CacheDestNode();
}

awPathResult_t idAnimator_AnimWeb::ForceState(
    idAnimWebSubWebIndex subWebIndex, idAnimWebStateIndex stateIndex,
    const blendParms_t& blendParameters, int newBlendEventMask) {
  if (decl == nullptr) return AWPATH_FAILED;
  curEdgeNodeIndex = curNodeIndex;
  if (stateIndex.value < 0 || stateIndex.value >= decl->states.num) {
    idLib::Warning("Id %d, '%s', is not a valid destination state in web %s",
                   stateIndex.value,
                   decl->GetStateName(stateIndex),
                   decl->name.str);
    return AWPATH_FAILED;
  }
  const idAnimWebNodeIndex destination =
      FindNodeIndex(decl, subWebIndex, stateIndex);
  if (destination.value < 0) {
    idLib::Warning("(%s) Sub-web '%s' does not contain a node with state '%s'",
                   decl->name.str,
                    decl->GetSubWebName(subWebIndex),
                   decl->states.list[stateIndex.value].name.str);
    return AWPATH_FAILED;
  }
  if (destination.value == curNodeIndex.value) {
    destStateIndices.numStates = 0;
    viaStateIndices.numStates = 0;
    destSubWebIndex.value = -1;
    viaSubWebIndex.value = -1;
    otherSubWebIndex.value = -1;
    curEdgeNodeIndex.value = -1;
    curEdgeIndex = -1;
    curPath.cost = std::numeric_limits<int>::max();
    curPath.num = 0;
    pathIndex = -1;
    forcedEdge.destNodeIndex.value = -1;
    forcedEdge.destSubWebIndex.value = -1;
    if (debug) {
      const idDeclAnimWebNode* const currentNode =
          curNodeIndex.value == -1 ? nullptr
                                   : decl->nodes.list[curNodeIndex.value];
      idLib::Printf(
          "('%s') ForceState from %s/%s to %s/%s, already there\n",
          decl->name.str, decl->GetSubWebName(currentNode->subWebIndex),
          decl->GetStateName(currentNode->states.stateIndex[0]),
          decl->GetSubWebName(subWebIndex),
          decl->GetStateName(stateIndex));
    }
    return AWPATH_ALREADY_THERE;
  }
  forcedEdge.destNodeIndex = destination;
  forcedEdge.destSubWebIndex = subWebIndex;
  forcedEdge.blendParms = blendParameters;
  blendEventMask = newBlendEventMask;
  destStateIndices.numStates = 0;
  viaStateIndices.numStates = 0;
  interruptPath = INTR_PATH_YES;
  interruptBlend = INTR_BLEND_YES;
  skipForcedEdgeAutoPath = false;
  destSubWebIndex.value = -1;
  viaSubWebIndex.value = -1;
  otherSubWebIndex.value = -1;
  if (clearEventsOnStateChanges) eventHandler.ClearEvents();
  if (debug) {
    const idDeclAnimWebNode* const currentNode =
        curNodeIndex.value == -1 ? nullptr
                                 : decl->nodes.list[curNodeIndex.value];
    idLib::Printf("('%s') ForceState from %s/%s to %s/%s\n",
                  decl->name.str,
                  decl->GetSubWebName(currentNode->subWebIndex),
                  decl->GetStateName(currentNode->states.stateIndex[0]),
                  decl->GetSubWebName(subWebIndex),
                  decl->GetStateName(stateIndex));
  }
  return AWPATH_OK;
}

awPathResult_t idAnimator_AnimWeb::ForceState(
    const char* subWebName, const char* stateName,
    const blendParms_t& blendParameters, int newBlendEventMask) {
  if (decl == nullptr) return AWPATH_FAILED;
  const idAnimWebSubWebIndex subWeb = decl->GetSubWebIndexForName(subWebName);
  if (subWeb.value < 0) {
    idLib::Warning(
        "idAnimator_AnimWeb::ForceState - animWeb '%s' does not have a sub-web '%s'",
        decl->name.str, subWebName);
    return AWPATH_FAILED;
  }
  const idAnimWebStateIndex state = decl->GetStateIndexForName(stateName);
  if (state.value < 0) {
    idLib::Warning(
        "idAnimator_AnimWeb::ForceState - animWeb '%s' does not have a state '%s'",
        decl->name.str, stateName);
    return AWPATH_FAILED;
  }
  return ForceState(idAnimWebSubWebIndex(subWeb.value),
                    idAnimWebStateIndex(state.value), blendParameters,
                    newBlendEventMask);
}

awPathResult_t idAnimator_AnimWeb::ForceState(
    const char* subWebName, idAnimWebStateIndex stateIndex,
    const blendParms_t& blendParameters, int newBlendEventMask) {
  if (decl == nullptr) return AWPATH_FAILED;
  const idAnimWebSubWebIndex subWeb = decl->GetSubWebIndexForName(subWebName);
  if (subWeb.value < 0) {
    idLib::Warning(
        "idAnimator_AnimWeb::ForceState - animWeb '%s' does not have a sub-web '%s'",
        decl->name.str, subWebName);
    return AWPATH_FAILED;
  }
  return ForceState(idAnimWebSubWebIndex(subWeb.value), stateIndex,
                    blendParameters, newBlendEventMask);
}

void idAnimator_AnimWeb::InternalShutdown(idAnimStack* stack) {
  channelWeightAnimator.Shutdown(stack);
  if (mergeBranch != nullptr) {
    idMD6Allocator::FreeTyped(&stack->md6Allocator, mergeBranch);
    mergeBranch = nullptr;
  }
  Free();
}

namespace {

const idMD6Leaf* FindLeafWithMatchingAnim(
    const idMD6Node* treeValue, int curTime,
    const idMD6Anim* animationValue, bool) {
  const idMD6Leaf* result = nullptr;
  VisitTreeLeaves(treeValue, [&](const idMD6Leaf* leaf) {
    if (leaf->type == idMD6Node::NODE_LEAF_PLAY)
      static_cast<const idMD6LeafPlay*>(leaf)->IsPlaying(
          curTime, timeManager.gameTimePerSecond.value);
    if (leaf->anim == animationValue) {
      result = leaf;
      return false;
    }
    return true;
  });
  return result;
}

const idMD6Leaf* FindLongestLeaf(
    const idMD6Node* treeValue, int curTime, int ticksPerSec,
    bool) {
  const idMD6Leaf* result = nullptr;
  int longestClampTime = std::numeric_limits<int>::min();
  VisitTreeLeaves(treeValue, [&](const idMD6Leaf* leaf) {
    if (leaf->type == idMD6Node::NODE_LEAF_PAUSE) {
      result = leaf;
      return false;
    }
    const int clampTime =
        static_cast<const idMD6LeafPlay*>(leaf)->GetClampTime(curTime,
                                                              ticksPerSec);
    if (clampTime > longestClampTime) {
      longestClampTime = clampTime;
      result = leaf;
    }
    return true;
  });
  return result;
}

} // namespace

const idMD6Leaf* idAnimator_AnimWeb::FindBestLeaf(
    idAnimWebNodeIndex nodeIndex, idAnimWebModelIndex modelIndex,
    const idMD6Anim* animationValue, int curTime, bool& matchedAnim) const {
  matchedAnim = false;
  if (nodeIndex.value < 0 ||
      decl->nodes.list[nodeIndex.value] == nullptr) return nullptr;
  const idAnimWebBlendTree* const blendTree =
      modelAnimStates.list[modelIndex.value].GetBlendTree(nodeIndex);
  if (blendTree == nullptr) return nullptr;
  if (animationValue != nullptr) {
    const idMD6Leaf* const matching = FindLeafWithMatchingAnim(
        blendTree->tree, curTime, animationValue, false);
    if (matching != nullptr) {
      matchedAnim = true;
      return matching;
    }
  }
  return FindFirstLeaf(blendTree->tree, curTime, false);
}

bool idAnimator_AnimWeb::InternalShouldStartNextNode(
    idAnimWebNodeIndex currentNodeIndex,
    idAnimWebNodeIndex nextNodeIndex, int curTime, int ticksPerSec,
    const blendParms_t& blendParameters, idAnimWebModelIndex modelIndex,
    bool showWarnings, animWebEvent_t* eventType) {
  if (decl == nullptr) return false;
  if (eventType != nullptr) *eventType = ANIMWEB_EVENT_NONE;

  const idDeclAnimWebNode* const currentNode =
      currentNodeIndex.value == -1
          ? nullptr
          : decl->nodes.list[currentNodeIndex.value];
  const idDeclAnimWebNode* const nextNode =
      nextNodeIndex.value == -1 ? nullptr
                                : decl->nodes.list[nextNodeIndex.value];
  const idAnimWebBlendTree* const currentTree =
      modelAnimStates.list[modelIndex.value].GetBlendTree(currentNodeIndex);
  idAnimWebBlendTree* const nextTree =
      modelAnimStates.list[modelIndex.value].GetBlendTree(nextNodeIndex);
  if (currentTree == nullptr) {
    idStr nodeName;
    currentNode->GetName(decl, nodeName);
    idLib::Error(
        "Warning: node '%s' in animWeb '%s' is missing cur blend tree for model '%s'.",
        nodeName.data, decl->name.str,
        decl->models.list[modelIndex.value]->name.str);
  }
  if (nextTree == nullptr) {
    idStr nodeName;
    nextNode->GetName(decl, nodeName);
    idLib::Error(
        "Warning: node '%s' in animWeb '%s' is missing next blend tree for model '%s'.",
        nodeName.data, decl->name.str,
        decl->models.list[modelIndex.value]->name.str);
  }

  const idMD6Leaf* sourceLeaf =
      blendParameters.parms.srcAnim != nullptr
          ? FindLeafWithMatchingAnim(currentTree->tree, curTime,
                                     blendParameters.parms.srcAnim, false)
          : FindLongestLeaf(currentTree->tree, curTime, ticksPerSec, false);
  if (blendParameters.parms.srcAnim == nullptr && sourceLeaf == nullptr) {
    idLib::Warning(
        "animWeb '%s': no explicit animations defined in blend from node '%s' to node '%s', but src node has no leaves.",
        decl->name.str,
        decl->GetStateName(currentNode->states.stateIndex[0]),
        decl->GetStateName(nextNode->states.stateIndex[0]));
    return false;
  }

  nextTree->CopyScalars(this, curTime, ticksPerSec, 3u);
  const idDeclMD6* const model = decl->models.list[modelIndex.value];
  idAnimStack_Util::UpdateTree(model->model->skeleton, curTime,
                               ticksPerSec, nextTree->tree);
  const idMD6Leaf* destinationLeaf =
      blendParameters.parms.destAnim != nullptr
          ? FindLeafWithMatchingAnim(nextTree->tree, curTime,
                                     blendParameters.parms.destAnim, false)
          : FindLongestLeaf(nextTree->tree, curTime, ticksPerSec, false);
  if (blendParameters.parms.destAnim == nullptr &&
      destinationLeaf == nullptr) {
    idLib::Warning(
        "animWeb '%s': no explicit animations defined in blend from node '%s' to node '%s', but dest node has no leaves.",
        decl->name.str,
        decl->GetStateName(currentNode->states.stateIndex[0]),
        decl->GetStateName(nextNode->states.stateIndex[0]));
    return false;
  }

  if (sourceLeaf == nullptr || destinationLeaf == nullptr) {
    idLib::Warning(
        "(%d) animweb '%s': could not find blend from '%s:%s' to '%s:%s', at time %d",
        gametimeManager.gameTimeManager->GetGameFrame(), decl->name.str,
        decl->GetSubWebName(currentNode->subWebIndex),
        decl->GetStateName(currentNode->states.stateIndex[0]),
        decl->GetSubWebName(nextNode->subWebIndex),
        decl->GetStateName(nextNode->states.stateIndex[0]), curTime);
    return false;
  }

  std::int16_t sourceFirstFrame;
  std::int16_t sourceLastFrame;
  blendParameters.GetSourceFrames(sourceLeaf->anim, sourceFirstFrame,
                                  sourceLastFrame);
  if (sourceLeaf->type == idMD6Node::NODE_LEAF_PAUSE) {
    const float frame = static_cast<const idMD6LeafPause*>(sourceLeaf)->frame;
    if (frame >= sourceFirstFrame && frame <= sourceLastFrame) {
      std::int16_t destinationFrame;
      std::int16_t destinationDuration;
      blendParameters.GetDestFrames(destinationLeaf->anim, destinationFrame,
                                    destinationDuration);
      DebugPrint(
          "(%d) animweb '%s': found blend (sff:%d, slf:%d df:%d d:%d) from '%s/%s' to '%s/%s', curFrame %f at time %d\n",
          gametimeManager.gameTimeManager->GetGameFrame(), decl->name.str,
          sourceFirstFrame, sourceLastFrame, destinationFrame,
          destinationDuration, decl->GetSubWebName(currentNode->subWebIndex),
          decl->GetStateName(currentNode->states.stateIndex[0]),
          decl->GetSubWebName(nextNode->subWebIndex),
          decl->GetStateName(nextNode->states.stateIndex[0]), frame, curTime);
      return true;
    }
    return false;
  }
  if (sourceLeaf->type != idMD6Node::NODE_LEAF_PLAY) return false;
  const auto* play = static_cast<const idMD6LeafPlay*>(sourceLeaf);
  const int frame = static_cast<std::uint16_t>(
      play->GetFrame(curTime, timeManager.gameTimePerSecond.value));
  if (frame >= sourceFirstFrame && frame <= sourceLastFrame) {
    std::int16_t destinationFrame;
    std::int16_t destinationDuration;
    blendParameters.GetDestFrames(destinationLeaf->anim, destinationFrame,
                                  destinationDuration);
    DebugPrint(
        "(%d) animweb '%s': found blend (sff:%d, slf:%d df:%d d:%d) from '%s/%s' to '%s/%s', curFrame %d at time %d\n",
        gametimeManager.gameTimeManager->GetGameFrame(), decl->name.str,
        sourceFirstFrame, sourceLastFrame, destinationFrame,
        destinationDuration, decl->GetSubWebName(currentNode->subWebIndex),
        decl->GetStateName(currentNode->states.stateIndex[0]),
        decl->GetSubWebName(nextNode->subWebIndex),
        decl->GetStateName(nextNode->states.stateIndex[0]),
        static_cast<std::uint16_t>(
            play->GetFrame(curTime, timeManager.gameTimePerSecond.value)),
        curTime);
    return true;
  }

  if (eventType != nullptr) {
    const int frameRate = play->anim->animData != nullptr
                              ? play->anim->animData->frameRate
                              : 30;
    const int gameHz =
        gametimeManager.gameTimeManager->GetGameHz(GAMETIME_SCALED);
    if (frame * gameHz / frameRate -
            gameHz * sourceFirstFrame / frameRate ==
        1)
      *eventType = ANIMWEB_EVENT_7;
  }

  if (showWarnings && play->wrapMode == idMD6Leaf::WRAP_CLAMP &&
      frame > sourceLastFrame) {
    idStr currentNodeName;
    currentNode->GetName(decl, currentNodeName);
    idStr nextNodeName;
    nextNode->GetName(decl, nextNodeName);
    idLib::Warning(
        "animWeb '%s': clamped anim '%s' is at frame %d and already past the src blend window (%d,%d) to '%s'",
        decl->name.str, currentNodeName.data, frame, sourceFirstFrame,
        sourceLastFrame, nextNodeName.data);
  }
  if (play->anim == nullptr) {
    idLib::Warning(
        "animWeb '%s': trying to blend from '%s' to '%s', but source leaf has no animation.",
        decl->name.str,
        decl->GetStateName(currentNode->states.stateIndex[0]),
        decl->GetStateName(nextNode->states.stateIndex[0]));
    return false;
  }
  const int frameCount = play->anim->animData != nullptr
                             ? play->anim->animData->numFrames
                             : 0;
  const int terminalFrame =
      play->wrapMode == idMD6Leaf::WRAP_LOOP ? frameCount - 1 : frameCount;
  if (sourceFirstFrame < terminalFrame) return false;
  idStr currentNodeName;
  currentNode->GetName(decl, currentNodeName);
  idStr nextNodeName;
  nextNode->GetName(decl, nextNodeName);
  idLib::Warning("animWeb '%s': blend from '%s' to '%s' is out of range",
                 decl->name.str, currentNodeName.data, nextNodeName.data);
  return !play->IsPlaying(curTime, timeManager.gameTimePerSecond.value);
}

void idAnimator_AnimWeb::SelectRandomBlendVals(
    const idDeclAnimWebNode* currentNode,
    const idDeclAnimWebNode* nextNode) {
  if (nextNode == nullptr) return;
  auto* random = reinterpret_cast<idRandom2*>(
      reinterpret_cast<std::uint8_t*>(animation) + 8);
  if (currentNode == nullptr) {
    for (int index = 0; index < ANIMWEB_NUM_RANDOM_BLEND_VALS; ++index) {
      int value = random->RandomInt(index + 2);
      if (static_cast<float>(value) == randomBlendVals[index])
        value = (value + 1) % (index + 2);
      randomBlendVals[index] = static_cast<float>(value);
    }
    return;
  }

  idList<int, 5> excludedScalars;
  idList<int, 5> usedScalars;
  for (int modelIndex = 0; modelIndex < modelAnimStates.num; ++modelIndex) {
    idModelAnimState& state = modelAnimStates.list[modelIndex];
    const idAnimWebBlendTree* const currentTree =
        state.GetBlendTree(idAnimWebNodeIndex(currentNode->nodeIndex.value));
    if (currentTree != nullptr) {
      currentTree->GetUsedScalarIndices(this, excludedScalars, 0, 7);
      const idAnimWebBlendTree* const nextTree =
          state.GetBlendTree(idAnimWebNodeIndex(nextNode->nodeIndex.value));
      if (nextTree != nullptr)
        nextTree->GetUsedScalarIndices(this, usedScalars, 0, 7);
    }
  }
  for (int usedIndex = 0; usedIndex < usedScalars.num; ++usedIndex) {
    const int scalarIndex = usedScalars.list[usedIndex];
    bool excluded = false;
    for (int excludedIndex = 0; excludedIndex < excludedScalars.num;
         ++excludedIndex) {
      if (excludedScalars.list[excludedIndex] != scalarIndex) continue;
      excluded = true;
      break;
    }
    if (excluded || scalarIndex < 0 || scalarIndex >= 7) continue;
    int value = random->RandomInt(scalarIndex + 2);
    if (static_cast<float>(value) == randomBlendVals[scalarIndex])
      value = (value + 1) % (scalarIndex + 2);
    randomBlendVals[scalarIndex] = static_cast<float>(value);
  }
}

void idAnimator_AnimWeb::BeginNodesPlaying(idAnimWebNodeIndex nodeIndex,
                                            int curTime) {
  if (decl == nullptr) return;
  const idDeclAnimWebNode* const node =
      nodeIndex.value == -1 ? nullptr : decl->nodes.list[nodeIndex.value];
  for (int modelIndex = 0; modelIndex < modelAnimStates.num; ++modelIndex) {
    const idDeclAnimWebNode::idModelInfo* const modelInfo =
        node->FindModelInfo(idAnimWebModelIndex(
            static_cast<std::int16_t>(modelIndex)));
    if (modelInfo != nullptr) {
      modelAnimStates.list[modelIndex].BeginNodePlaying(
          this, modelInfo, nodeIndex, curTime,
          timeManager.gameTimePerSecond.value);
    } else if (modelIndex == 0) {
      idStr nodeName;
      node->GetName(decl, nodeName);
      idLib::Error("animWeb '%s' node '%s' missing modelinfo.",
                   decl->name.str, nodeName.data);
      return;
    } else {
      modelAnimStates.list[modelIndex].tree = nullptr;
      modelAnimStates.list[modelIndex].blending = false;
    }
  }
}

idAnimator_AnimWeb::idModelAnimState::~idModelAnimState() {
  for (int index = 0; index < blendTreeCache.num; ++index) {
    DestroyBlendTree(blendTreeCache.list[index]);
    blendTreeCache.list[index] = nullptr;
  }
  blendTreeCache.num = 0;
}

void idAnimator_AnimWeb::idModelAnimState::StartBlend(
    idAnimator_AnimWeb* web,
    const idDeclAnimWebNode::idModelInfo* modelInfo,
    idAnimWebNodeIndex currentNodeIndex,
    idAnimWebNodeIndex nextNodeIndex,
    const blendParms_t& blendParameters, int curTime, int ticksPerSec,
    unsigned int& longestAnimLength, int& destFrame) {
  longestAnimLength = 0;
  destFrame = 0;
  idMD6Node* const previousTree = tree;
  idMD6Branch* blendBranch =
      previousTree == branches[0] ? branches[1] : branches[0];
  idMD6Branch* otherBranch = blendBranch == branches[0] ? branches[1]
                                                         : branches[0];
  if (otherBranch->left == blendBranch) otherBranch->left = blendBranch->right;
  if (!BeginNodePlaying(web, modelInfo, nextNodeIndex, curTime, ticksPerSec))
    return;
  idAnimWebBlendTree* const currentTree = GetBlendTree(currentNodeIndex);
  idAnimWebBlendTree* const nextTree = GetBlendTree(nextNodeIndex);
  if (currentTree == nullptr) {
    tree = nextTree->tree;
    return;
  }
  if (nextTree == nullptr) {
    tree = currentTree->tree;
    return;
  }
  idMD6Node* sourceTree = currentTree->tree;
  if (previousTree == branches[0] || previousTree == branches[1])
    sourceTree = previousTree;
  idMD6Node* const destinationTree = nextTree->tree;
  std::uint8_t phase = 0;
  if ((blendParameters.parms.flags & 0x10u) != 0)
    GetPhase(sourceTree, curTime, ticksPerSec, phase);
  SetDestFrame(destinationTree, blendParameters, curTime, ticksPerSec,
               phase, longestAnimLength, destFrame);
  if (blendParameters.GetDurationMS() <= 0) {
    blendBranch->Init(sourceTree, destinationTree, idMD6Blend::BOP_LERP,
                      static_cast<idMD6Blend::originBlend_t>(
                          blendParameters.parms.originBlend),
                      1.0f, 1.0f, 0.0f, MD6_WEIGHTGROUP_ALL,
                      idMD6Branch::BLEND_LINEAR);
  } else {
    blendBranch->Init(sourceTree, destinationTree, idMD6Blend::BOP_LERP,
                      static_cast<idMD6Blend::originBlend_t>(
                          blendParameters.parms.originBlend),
                      0.0f, 1.0f,
                      1000.0f / blendParameters.GetDurationMS(),
                      MD6_WEIGHTGROUP_ALL,
                      static_cast<idMD6Branch::blendType_t>(
                          blendParameters.parms.blendType));
  }
  tree = blendBranch;
  blending = true;
}

void idAnimator_AnimWeb::InternalStartBlend(
    const idDeclAnimWebNode* currentNode,
    const idDeclAnimWebNode* nextNode, int) {
  SelectRandomBlendVals(currentNode, nextNode);
}

idAnimator_AnimWeb::idModelAnimState::idModelAnimState()
    : tree(nullptr), branches{nullptr, nullptr}, blendTreeCache(),
      recentNodes{idAnimWebNodeIndex(), idAnimWebNodeIndex(),
                  idAnimWebNodeIndex()},
      recentNodeStart(0), blending(false) {}

idAnimWebBlendTree*
idAnimator_AnimWeb::idModelAnimState::AllocBlendTree(
    idAnimWebNodeIndex nodeIndex, idAnimWebNodeIndex keepNodeIndex) const {
  auto& cache = const_cast<idStaticList<idAnimWebBlendTree*,
                                        MAX_BLEND_TREES>&>(blendTreeCache);
  int existingIndex = -1;
  int replacementIndex = -1;
  int smallestLeafCount = std::numeric_limits<int>::max();
  for (int index = 0; index < cache.num; ++index) {
    idAnimWebBlendTree* const blendTree = cache.list[index];
    if (blendTree->nodeIndex.value == nodeIndex.value)
      existingIndex = index;
    if (blendTree->nodeIndex.value == keepNodeIndex.value) continue;
    if (blendTree->leaves.num < smallestLeafCount) {
      int recentIndex = 0;
      for (; recentIndex < MAX_RECENT; ++recentIndex) {
        if (recentNodes[(recentNodeStart + recentIndex) % MAX_RECENT].value ==
            blendTree->nodeIndex.value)
          break;
      }
      if (recentIndex >= MAX_RECENT) {
        smallestLeafCount = blendTree->leaves.num;
        replacementIndex = index;
      }
    }
  }
  if (existingIndex >= 0) return cache.list[existingIndex];
  idAnimWebBlendTree* const blendTree = AllocateBlendTree(nodeIndex);
  if (blendTree == nullptr) return nullptr;
  if (cache.num == cache.size) {
    DestroyBlendTree(cache.list[replacementIndex]);
    cache.list[replacementIndex] = blendTree;
  } else {
    cache.Append(blendTree);
  }
  return blendTree;
}

idAnimWebBlendTree* idAnimator_AnimWeb::AllocBlendTrees(
    idAnimWebNodeIndex nodeIndex, idAnimWebNodeIndex keepNodeIndex) {
  if (decl == nullptr) return nullptr;
  if (nodeIndex.value == -1 ||
      decl->nodes.list[nodeIndex.value] == nullptr) {
    idLib::Error("Node with index %d was not found in animweb '%s'",
                 nodeIndex.value, decl->name.str);
    __debugbreak();
  }
  const idDeclAnimWebNode* const node = decl->nodes.list[nodeIndex.value];
  idAnimWebBlendTree* primaryTree = nullptr;
  for (int modelIndex = 0; modelIndex < modelAnimStates.num; ++modelIndex) {
    const idDeclAnimWebNode::idModelInfo* const modelInfo =
        node->FindModelInfo(idAnimWebModelIndex(
            static_cast<std::int16_t>(modelIndex)));
    if (modelInfo == nullptr) continue;
    idModelAnimState& modelState = modelAnimStates.list[modelIndex];
    if (modelState.GetBlendTree(nodeIndex) != nullptr) continue;
    idAnimWebBlendTree* const blendTree =
        modelState.AllocBlendTree(nodeIndex, keepNodeIndex);
    if (modelIndex == 0) primaryTree = blendTree;
    if (!blendTree->Init(this, node,
                         idAnimWebModelIndex(static_cast<std::int16_t>(modelIndex)),
                         modelInfo->blendEq.str, *modelInfo, 960)) {
      modelState.FreeBlendTree(nodeIndex);
      idStr nodeName;
      node->GetName(decl, nodeName);
      idLib::Warning("Failed to initialize node '%s' in animWeb '%s'",
                     nodeName.data, decl->name.str);
    }
  }
  return primaryTree;
}

bool idAnimator_AnimWeb::FindPath(
    idAnimWebNodeIndex fromNodeIndex,
    const idDeclAnimWeb::stateList_t& targetStates,
    idAnimWebSubWebIndex targetSubWebIndex,
    idAnimWebSubWebIndex excludedSubWebIndex,
    idAnimWebRoute& route) const {
  if (decl == nullptr) return false;
  idStaticList<idAnimWebNodeIndex, 8> destinationNodes;
  decl->FindNodeIndices(targetSubWebIndex, targetStates, destinationNodes);
  return FindRouteToNodes(*this, fromNodeIndex, destinationNodes.list,
                          destinationNodes.num, targetSubWebIndex,
                          idAnimWebStateIndex(
                              targetStates.stateIndex[0].value),
                          excludedSubWebIndex, false, false, route);
}

bool idAnimator_AnimWeb::FindPath(
    idAnimWebNodeIndex fromNodeIndex,
    idAnimWebStateIndex targetStateIndex,
    idAnimWebSubWebIndex targetSubWebIndex,
    idAnimWebSubWebIndex excludedSubWebIndex,
    idAnimWebRoute& route) const {
  idDeclAnimWeb::stateList_t states;
  states.stateIndex[0].value = targetStateIndex.value;
  states.numStates = 1;
  if (decl == nullptr) return false;
  idStaticList<idAnimWebNodeIndex, 8> destinationNodes;
  decl->FindNodeIndices(targetSubWebIndex, states, destinationNodes);
  return FindRouteToNodes(*this, fromNodeIndex, destinationNodes.list,
                          destinationNodes.num, targetSubWebIndex,
                          targetStateIndex, excludedSubWebIndex,
                          true, true, route);
}

void idAnimator_AnimWeb::StartPath(int curTime) {
  const idAnimWebNodeIndex startingNode = curNodeIndex;
  if (startingNode.value < 0) return;

  idDeclAnimWeb::stateList_t targetStates = destStateIndices;
  const idAnimWebSubWebIndex targetSubWeb = destSubWebIndex;
  destSubWebIndex.value = -1;
  destStateIndices.numStates = 0;
  curEdgeNodeIndex.value = -1;
  curEdgeIndex = -1;
  curPath.cost = std::numeric_limits<int>::max();
  curPath.num = 0;
  idAnimWebNodeIndex pathStart = startingNode;
  bool selectFirstEdge = true;

  if (forcedEdge.destNodeIndex.value >= 0) {
    curEdgeNodeIndex = startingNode;
    curEdgeIndex = FORCED_EDGE_INDEX;
    curPath.path[curPath.num++] = startingNode;
    curPath.path[curPath.num++] = forcedEdge.destNodeIndex;
    curPath.cost = 1;
    pathIndex = 0;
    CacheDestNode();
    if (targetStates.numStates == 0) {
      selectFirstEdge = false;
      goto path_ready;
    }
    pathStart = forcedEdge.destNodeIndex;
  }

  InternalStartPath();
  if (viaStateIndices.numStates != 0 && viaSubWebIndex.value >= 0) {
    const idAnimWebSubWebIndex firstExcluded =
        otherSubWebIndex.value < 0 ? targetSubWeb : otherSubWebIndex;
    if (!FindPath(pathStart, viaStateIndices, viaSubWebIndex,
                  firstExcluded, curPath)) {
      const idDeclAnimWebNode* const node = decl->nodes.list[pathStart.value];
      if (otherSubWebIndex.value < 0) {
        idLib::Warning(
            "animweb '%s' could not find path from state '%s::%s' to desired state '%s::%s' through state '%s::%s.",
            decl->name.str, decl->GetSubWebName(node->subWebIndex),
            decl->GetStateName(node->states.stateIndex[0]),
            decl->GetSubWebName(targetSubWeb),
            decl->GetStateName(targetStates.stateIndex[0]),
            decl->GetSubWebName(viaSubWebIndex),
            decl->GetStateName(viaStateIndices.stateIndex[0]));
      } else {
        idLib::Warning(
            "animweb '%s' could not find path from state '%s::%s' to desired state '%s::%s' through state '%s::%s using other subweb '%s'.",
            decl->name.str, decl->GetSubWebName(node->subWebIndex),
            decl->GetStateName(node->states.stateIndex[0]),
            decl->GetSubWebName(targetSubWeb),
            decl->GetStateName(targetStates.stateIndex[0]),
            decl->GetSubWebName(viaSubWebIndex),
            decl->GetStateName(viaStateIndices.stateIndex[0]),
            decl->GetSubWebName(otherSubWebIndex));
      }
      error = ANIMWEB_ERROR_NOPATH;
      return;
    }
    const idAnimWebNodeIndex viaNode = curPath.GetLastNodeIndex();
    const idAnimWebSubWebIndex secondExcluded =
        otherSubWebIndex.value < 0 ? viaSubWebIndex : otherSubWebIndex;
    if (!FindPath(viaNode, targetStates, targetSubWeb,
                  secondExcluded, curPath)) {
      const idDeclAnimWebNode* const node = decl->nodes.list[pathStart.value];
      if (otherSubWebIndex.value < 0) {
        idLib::Warning(
            "animweb '%s' could not find path from state '%s::%s' to desired state '%s::%s' through state '%s::%s.",
            decl->name.str, decl->GetSubWebName(node->subWebIndex),
            decl->GetStateName(node->states.stateIndex[0]),
            decl->GetSubWebName(targetSubWeb),
            decl->GetStateName(targetStates.stateIndex[0]),
            decl->GetSubWebName(viaSubWebIndex),
            decl->GetStateName(viaStateIndices.stateIndex[0]));
      } else {
        idLib::Warning(
            "animweb '%s' could not find path from state '%s::%s' to desired state '%s::%s' through state '%s::%s using other subweb '%s'.",
            decl->name.str, decl->GetSubWebName(node->subWebIndex),
            decl->GetStateName(node->states.stateIndex[0]),
            decl->GetSubWebName(targetSubWeb),
            decl->GetStateName(targetStates.stateIndex[0]),
            decl->GetSubWebName(viaSubWebIndex),
            decl->GetStateName(viaStateIndices.stateIndex[0]),
            decl->GetSubWebName(otherSubWebIndex));
      }
      error = ANIMWEB_ERROR_NOPATH;
      return;
    }
  } else if (!FindPath(pathStart, targetStates, targetSubWeb,
                       otherSubWebIndex, curPath)) {
    const idDeclAnimWebNode* const node = decl->nodes.list[pathStart.value];
    if (node->states.stateIndex[0].value != targetStates.stateIndex[0].value ||
        node->subWebIndex.value != targetSubWeb.value) {
      if (otherSubWebIndex.value < 0) {
        idLib::Warning(
            "animweb '%s' could not find path from state '%s::%s' to desired state '%s::%s'.",
            decl->name.str, decl->GetSubWebName(node->subWebIndex),
            decl->GetStateName(node->states.stateIndex[0]),
            decl->GetSubWebName(targetSubWeb),
            decl->GetStateName(targetStates.stateIndex[0]));
      } else {
        idLib::Warning(
            "animweb '%s' could not find path from state '%s::%s' to desired state '%s::%s' through other subweb '%s'.",
            decl->name.str, decl->GetSubWebName(node->subWebIndex),
            decl->GetStateName(node->states.stateIndex[0]),
            decl->GetSubWebName(targetSubWeb),
            decl->GetStateName(targetStates.stateIndex[0]),
            decl->GetSubWebName(otherSubWebIndex));
      }
      error = ANIMWEB_ERROR_NOPATH;
    }
    return;
  }
  pathIndex = 0;
  CacheDestNode();

path_ready:
  if (debug) {
    const idDeclAnimWebNode* const startNode = decl->nodes.list[pathStart.value];
    if (viaStateIndices.numStates != 0 && viaSubWebIndex.value >= 0) {
      DebugPrint(
          "(%d) animWeb '%s', path from '%s::%s' to '%s::%s' through '%s::%s' at time %d:\n",
          gametimeManager.gameTimeManager->GetGameFrame(), decl->name.str,
          decl->GetSubWebName(startNode->subWebIndex),
          decl->GetStateName(startNode->states.stateIndex[0]),
          decl->GetSubWebName(targetSubWeb),
          decl->GetStateName(targetStates.stateIndex[0]),
          decl->GetSubWebName(viaSubWebIndex),
          decl->GetStateName(viaStateIndices.stateIndex[0]), curTime);
    } else {
      DebugPrint(
          "(%d) animWeb '%s', path from '%s::%s' to '%s::%s' at time %d:\n",
          gametimeManager.gameTimeManager->GetGameFrame(), decl->name.str,
          decl->GetSubWebName(startNode->subWebIndex),
          decl->GetStateName(startNode->states.stateIndex[0]),
          decl->GetSubWebName(targetSubWeb),
          decl->GetStateName(targetStates.stateIndex[0]), curTime);
    }
    for (int index = 0; index < curPath.num; ++index) {
      const idDeclAnimWebNode* const node =
          decl->nodes.list[curPath.path[index].value];
      DebugPrint("  %d: %s::%s\n", index,
                 decl->GetSubWebName(node->subWebIndex),
                 decl->GetStateName(node->states.stateIndex[0]));
    }
  }
  if (!selectFirstEdge) return;
  const idAnimWebNodeIndex nextNode = curPath.GetNodeIndex(pathIndex + 1);
  if (curEdgeIndex == FORCED_EDGE_INDEX) return;
  const idDeclAnimWebNode* const currentNode =
      decl->nodes.list[pathStart.value];
  for (int localEdgeIndex = 0; localEdgeIndex < currentNode->edges.num;
       ++localEdgeIndex) {
    const int edgeIndex = currentNode->edges.list[localEdgeIndex];
    if (decl->edgeCache.list[edgeIndex].destNodeIndex.value !=
        nextNode.value)
      continue;
    curEdgeNodeIndex = pathStart;
    curEdgeIndex = static_cast<std::int16_t>(localEdgeIndex);
    return;
  }
}

void idAnimator_AnimWeb::SetCurNode(idAnimWebNodeIndex nodeIndex,
                                    int curTime) {
  prevNodeIndex.value = -1;
  curEdgeNodeIndex.value = -1;
  curEdgeIndex = -1;
  if (nodeIndex.value >= 0) {
    curNodeIndex = nodeIndex;
    AllocBlendTrees(nodeIndex, idAnimWebNodeIndex());
    const idDeclAnimWebNode* const startNode =
        decl->nodes.list[curNodeIndex.value];
    InternalStartBlend(nullptr, startNode, curTime);
    BeginNodesPlaying(nodeIndex, curTime);
    destStateIndices.numStates = 0;
    destSubWebIndex.value = -1;
    const idDeclAnimWebNode* const node =
        nodeIndex.value == -1 ? nullptr : decl->nodes.list[nodeIndex.value];
    if (node != nullptr) {
      for (int modelIndex = 0; modelIndex < modelAnimStates.num;
           ++modelIndex) {
        InternalSetAnimDelta(idAnimWebModelIndex(
                                 static_cast<std::int16_t>(modelIndex)),
                             nodeIndex.value == -1
                                 ? nullptr
                                 : decl->nodes.list[nodeIndex.value]);
      }
      if ((node->flags & 0x10u) != 0) {
        mergeBranch->targetAlpha = 0.0f;
        mergeBranch->currentAlpha = 0.0f;
      } else {
        mergeBranch->targetAlpha = 1.0f;
        mergeBranch->currentAlpha = 1.0f;
      }
    }
  } else {
    curNodeIndex.value = -1;
    destSubWebIndex.value = -1;
    destStateIndices.numStates = 0;
    for (int modelIndex = 0; modelIndex < modelAnimStates.num;
         ++modelIndex) {
      modelAnimStates.list[modelIndex].tree = nullptr;
      modelAnimStates.list[modelIndex].blending = false;
    }
  }
  CacheCurNode();
}

int idAnimator_AnimWeb::AddBlendVariable(
    const char* name, float* value,
    idAnimWebScalarPair::fieldFlag_t flags) {
  for (int index = 0; index < scalars.num; ++index) {
    idAnimWebScalar& scalar = scalars.list[index];
    if (idStr::Cmp(scalar.name.str, name) != 0) continue;
    if (scalar.scalarIndex < 0 && scalar.scalarPtr == nullptr) {
      if (flags == idAnimWebScalarPair::FF_NO_OVERRIDE)
        flags = static_cast<idAnimWebScalarPair::fieldFlag_t>(scalar.flags);
      scalar.Init(name, static_cast<std::uint8_t>(flags), value);
    }
    return index;
  }
  idAnimWebScalar* const scalar = &scalars.Alloc();
  scalar->Init(name, static_cast<std::uint8_t>(flags), value);
  return scalars.num - 1;
}

void idAnimator_AnimWeb::InternalInitWeb() {
  static const char* const names[ANIMWEB_NUM_RANDOM_BLEND_VALS] = {
      "_random2", "_random3", "_random4", "_random5",
      "_random6", "_random7", "_random8"};
  for (int index = 0; index < ANIMWEB_NUM_RANDOM_BLEND_VALS; ++index)
    AddBlendVariable(names[index], &randomBlendVals[index],
                     idAnimWebScalarPair::FF_NO_OVERRIDE);
}

idAnimator_AnimWeb::idAnimator_AnimWeb()
    : decl(nullptr),
      md6Allocator(nullptr),
      initialActiveSubWebs(),
      modelAnimStates(),
      curNodeIndex(),
      prevNodeIndex(),
      destSubWebIndex(),
      destStateIndices(),
      viaSubWebIndex(),
      viaStateIndices(),
      otherSubWebIndex(),
      curEdgeNodeIndex(),
      curEdgeIndex(-1),
      pathableSubWebNodeLists(),
      pathIndex(-1),
      curPath(),
      interruptPath(INTR_PATH_NO),
      interruptBlend(INTR_BLEND_NO),
      nodeStartTime(-1),
      prevNodeStartTime(-1),
      nodeStartFrame(0),
      scalars(),
      scalarMem(),
      blendEventMask(0),
      declChangeId(-1),
      curNodeStateName(),
      curNodeSubWebName(),
      destNodeStateName(),
      destNodeSubWebName(),
      handle(),
      eventHandler(),
      clearEventsOnStateChanges(true),
      skipFlags(2),
      requiredFlags(0),
      randomBlendVals{},
      mergeBranch(nullptr),
      channelWeightAnimator(),
      modelDecls(),
      error(ANIMWEB_ERROR_NONE),
      debug(false),
      forceAnimDeltaModeOnBlendStart(false),
      skipForcedEdgeAutoPath(false),
      dontTouchMergeBranchAlpha(false) {
  curPath.num = 0;
  curPath.cost = std::numeric_limits<int>::max();
  for (idAnimWebNodeIndex& index : curPath.path) index.value = -1;
  std::memset(pathableSubWebNodeLists.list, 0,
              sizeof(*pathableSubWebNodeLists.list) *
                  pathableSubWebNodeLists.size);
}

idAnimator_AnimWeb::~idAnimator_AnimWeb() {
  Free();
  md6Allocator = nullptr;
  mergeBranch = nullptr;
}

void idAnimator_AnimWeb::Reset(const char* initialSubWebName,
                               const char* initialStateName,
                               int curTime) {
  if (decl == nullptr) return;
  const idAnimWebNodeIndex rawNode =
      decl->GetNodeIndexForName(initialSubWebName, initialStateName);
  idAnimWebNodeIndex nodeIndex(rawNode.value);
  if (nodeIndex.value < 0) {
    idLib::Warning("(%s) Initial node '%s' does not exist!",
                   decl->name.str, initialStateName);
    curTime = 0;
  }
  SetCurNode(nodeIndex, curTime);
  eventHandler.ClearEvents();
}

void idAnimator_AnimWeb::Reset(const char* initialSubWebName,
                               idAnimWebStateIndex initialStateIndex,
                               int curTime) {
  if (decl == nullptr) return;
  const idAnimWebSubWebIndex rawSubWeb =
      decl->GetSubWebIndexForName(initialSubWebName);
  const idAnimWebNodeIndex nodeIndex = FindNodeIndex(
      decl, idAnimWebSubWebIndex(rawSubWeb.value), initialStateIndex);
  if (nodeIndex.value < 0) {
    const char* stateName = decl->GetStateName(initialStateIndex);
    idLib::Warning(
        "(%s) Initial node '%s' does not exist in subweb '%s'!",
        decl->name.str, stateName != nullptr ? stateName : "<unknown>",
        initialSubWebName);
    curTime = 0;
  }
  SetCurNode(nodeIndex, curTime);
  eventHandler.ClearEvents();
}

awPathResult_t idAnimator_AnimWeb::ChangeState(
    idAnimWebSubWebIndex subWebIndex, idAnimWebStateIndex stateIndex,
    interruptPath_t newInterruptPath,
    interruptBlend_t newInterruptBlend, int newBlendEventMask) {
  if (decl == nullptr) return AWPATH_FAILED;
  if (stateIndex.value < 0 || stateIndex.value >= decl->states.num) {
    idLib::Warning("Id %d, '%s', is not a valid destination state in web %s",
                   stateIndex.value, decl->GetStateName(stateIndex),
                   decl->name.str);
    return AWPATH_FAILED;
  }
  const idAnimWebNodeIndex destination =
      FindNodeIndex(decl, subWebIndex, stateIndex);
  if (destination.value < 0) {
    idLib::Warning("Sub-web '%s' does not contain a node with state '%s'. %s",
                   decl->GetSubWebName(subWebIndex),
                   decl->states.list[stateIndex.value].name.str,
                   decl->name.str);
    return AWPATH_FAILED;
  }
  if (destination.value == curNodeIndex.value) {
    destStateIndices.numStates = 0;
    destSubWebIndex.value = -1;
    viaStateIndices.numStates = 0;
    viaSubWebIndex.value = -1;
    otherSubWebIndex.value = -1;
    curEdgeNodeIndex.value = -1;
    curEdgeIndex = -1;
    curPath.cost = std::numeric_limits<int>::max();
    curPath.num = 0;
    pathIndex = -1;
    forcedEdge.destNodeIndex.value = -1;
    forcedEdge.destSubWebIndex.value = -1;
    if (debug) {
      const idDeclAnimWebNode* const currentNode =
          curNodeIndex.value == -1 ? nullptr
                                   : decl->nodes.list[curNodeIndex.value];
      idLib::Printf(
          "('%s') ChangeState from %s/%s to %s(%d)/%s(%d), already there\n",
          decl->name.str, decl->GetSubWebName(currentNode->subWebIndex),
          decl->GetStateName(currentNode->states.stateIndex[0]),
          decl->GetSubWebName(subWebIndex), subWebIndex.value,
          decl->GetStateName(stateIndex), stateIndex.value);
    }
    return AWPATH_ALREADY_THERE;
  }
  blendEventMask = newBlendEventMask;
  interruptPath = newInterruptPath;
  interruptBlend = newInterruptBlend;
  destStateIndices.numStates = 1;
  destStateIndices.stateIndex[0].value = stateIndex.value;
  destSubWebIndex = subWebIndex;
  viaStateIndices.numStates = 0;
  viaSubWebIndex.value = -1;
  otherSubWebIndex.value = -1;
  forcedEdge.destNodeIndex.value = -1;
  forcedEdge.destSubWebIndex.value = -1;
  if (clearEventsOnStateChanges) eventHandler.ClearEvents();
  if (anim_verifyPaths.valueInteger > 0) {
    idAnimWebRoute verificationRoute;
    verificationRoute.num = 0;
    verificationRoute.cost = std::numeric_limits<int>::max();
    if (!FindPath(curNodeIndex, stateIndex, subWebIndex,
                  idAnimWebSubWebIndex(), verificationRoute)) {
      const idDeclAnimWebNode* const startNode =
          curNodeIndex.value == -1 ? nullptr
                                   : decl->nodes.list[curNodeIndex.value];
      const idDeclAnimWebNode* const destinationNode =
          destination.value == -1 ? nullptr
                                  : decl->nodes.list[destination.value];
      if (startNode != nullptr && destinationNode != nullptr) {
        idStr startNodeName;
        startNode->GetName(decl, startNodeName);
        idStr destinationNodeName;
        destinationNode->GetName(decl, destinationNodeName);
        idLib::Warning("('%s') No path exists from '%s' to '%s'",
                       decl->name.str, startNodeName.data,
                       destinationNodeName.data);
      }
    }
  }
  if (debug) {
    const idDeclAnimWebNode* const currentNode =
        curNodeIndex.value == -1 ? nullptr
                                 : decl->nodes.list[curNodeIndex.value];
    idLib::Printf(
        "('%s') ChangeState from %s/%s to %s(%d)/%s(%d)\n",
        decl->name.str, decl->GetSubWebName(currentNode->subWebIndex),
        decl->GetStateName(currentNode->states.stateIndex[0]),
        decl->GetSubWebName(subWebIndex), subWebIndex.value,
        decl->GetStateName(stateIndex), stateIndex.value);
  }
  return AWPATH_OK;
}

awPathResult_t idAnimator_AnimWeb::ChangeStateVia(
    idAnimWebSubWebIndex subWebIndex, idAnimWebStateIndex stateIndex,
    idAnimWebSubWebIndex requestedViaSubWebIndex,
    idAnimWebStateIndex requestedViaStateIndex,
    interruptPath_t newInterruptPath,
    interruptBlend_t newInterruptBlend, int newBlendEventMask) {
  if (decl == nullptr) return AWPATH_FAILED;
  const idDeclAnimWebNode* const current =
      curNodeIndex.value == -1 ? nullptr
                               : decl->nodes.list[curNodeIndex.value];
  if (current != nullptr &&
      current->subWebIndex.value == requestedViaSubWebIndex.value &&
      current->states.stateIndex[0].value == requestedViaStateIndex.value) {
    return ChangeState(subWebIndex, stateIndex, newInterruptPath,
                       newInterruptBlend, newBlendEventMask);
  }
  if (stateIndex.value < 0 || stateIndex.value >= decl->states.num) {
    idLib::Warning("Id %d, '%s', is not a valid destination state in web %s",
                   stateIndex.value, decl->GetStateName(stateIndex),
                   decl->name.str);
    return AWPATH_FAILED;
  }
  if (requestedViaStateIndex.value < 0 ||
      requestedViaStateIndex.value >= decl->states.num) {
    idLib::Warning("Id %d, '%s', is not a valid destination state in web %s",
                   requestedViaStateIndex.value,
                   decl->GetStateName(requestedViaStateIndex),
                   decl->name.str);
    return AWPATH_FAILED;
  }
  const idAnimWebNodeIndex viaNode = FindNodeIndex(
      decl, requestedViaSubWebIndex, requestedViaStateIndex);
  if (viaNode.value < 0) {
    idLib::Warning("(%s) Sub-web '%s' does not contain a node with state '%s'",
                   decl->name.str,
                   decl->GetSubWebName(requestedViaSubWebIndex),
                   decl->GetStateName(requestedViaStateIndex));
    return AWPATH_FAILED;
  }
  const idAnimWebNodeIndex destination =
      FindNodeIndex(decl, subWebIndex, stateIndex);
  if (destination.value < 0) {
    idLib::Warning("(%s) Sub-web '%s' does not contain a node with state '%s'",
                   decl->name.str, decl->GetSubWebName(subWebIndex),
                   decl->GetStateName(stateIndex));
    return AWPATH_FAILED;
  }
  blendEventMask = newBlendEventMask;
  destStateIndices.numStates = 1;
  destStateIndices.stateIndex[0].value = stateIndex.value;
  destSubWebIndex = subWebIndex;
  viaStateIndices.numStates = 1;
  viaStateIndices.stateIndex[0].value = requestedViaStateIndex.value;
  viaSubWebIndex = requestedViaSubWebIndex;
  interruptPath = newInterruptPath;
  interruptBlend = newInterruptBlend;
  otherSubWebIndex.value = -1;
  forcedEdge.destNodeIndex.value = -1;
  forcedEdge.destSubWebIndex.value = -1;
  if (clearEventsOnStateChanges) eventHandler.ClearEvents();
  if (anim_verifyPaths.valueInteger > 0) {
    idAnimWebRoute verificationRoute;
    verificationRoute.num = 0;
    verificationRoute.cost = std::numeric_limits<int>::max();
    const bool first = FindPath(curNodeIndex, requestedViaStateIndex,
                                requestedViaSubWebIndex, subWebIndex,
                                verificationRoute);
    const bool second = FindPath(viaNode, stateIndex, subWebIndex,
                                 idAnimWebSubWebIndex(), verificationRoute);
    if (!first || !second) {
      const idDeclAnimWebNode* const startNode =
          curNodeIndex.value == -1 ? nullptr
                                   : decl->nodes.list[curNodeIndex.value];
      const idDeclAnimWebNode* const viaPathNode =
          viaNode.value == -1 ? nullptr : decl->nodes.list[viaNode.value];
      const idDeclAnimWebNode* const destinationNode =
          destination.value == -1 ? nullptr
                                  : decl->nodes.list[destination.value];
      if (startNode != nullptr && viaPathNode != nullptr &&
          destinationNode != nullptr) {
        idStr startNodeName;
        startNode->GetName(decl, startNodeName);
        idStr viaNodeName;
        viaPathNode->GetName(decl, viaNodeName);
        idStr destinationNodeName;
        destinationNode->GetName(decl, destinationNodeName);
        if (!first && !second) {
          idLib::Warning(
              "(%s) No path (%s -> %s -> %s) exists from '%s' to '%s' or '%s' to '%s'",
              decl->name.str, startNodeName.data, viaNodeName.data,
              destinationNodeName.data, startNodeName.data,
              viaNodeName.data, viaNodeName.data, destinationNodeName.data);
        } else if (!first) {
          idLib::Warning(
              "(%s) No path (%s -> %s -> %s) exists from '%s' to '%s'",
              decl->name.str, startNodeName.data, viaNodeName.data,
              destinationNodeName.data, startNodeName.data,
              viaNodeName.data);
        } else {
          idLib::Warning(
              "(%s) No path (%s -> %s -> %s) exists from '%s' to '%s'",
              decl->name.str, startNodeName.data, viaNodeName.data,
              destinationNodeName.data, viaNodeName.data,
              destinationNodeName.data);
        }
      }
    }
  }
  if (debug) {
    const idDeclAnimWebNode* const currentNode =
        curNodeIndex.value == -1 ? nullptr
                                 : decl->nodes.list[curNodeIndex.value];
    idLib::Printf(
        "animWeb '%s' ChangeStateVia from %s/%s to %s/%s, thru %s/%s\n",
        decl->name.str, decl->GetSubWebName(currentNode->subWebIndex),
        decl->GetStateName(currentNode->states.stateIndex[0]),
        decl->GetSubWebName(subWebIndex), decl->GetStateName(stateIndex),
        decl->GetSubWebName(requestedViaSubWebIndex),
        decl->GetStateName(requestedViaStateIndex));
  }
  return AWPATH_OK;
}

awPathResult_t idAnimator_AnimWeb::ForceAndChangeState(
    idAnimWebSubWebIndex forcedSubWebIndex,
    idAnimWebStateIndex forcedStateIndex,
    idAnimWebSubWebIndex destinationSubWebIndex,
    idAnimWebStateIndex destinationStateIndex,
    const blendParms_t& blendParameters, int newBlendEventMask) {
  if (decl == nullptr) return AWPATH_FAILED;
  curEdgeNodeIndex = curNodeIndex;
  if (forcedStateIndex.value < 0 ||
      forcedStateIndex.value >= decl->states.num) {
    idLib::Warning("Id %d, '%s', is not a valid via state in web %s",
                   forcedStateIndex.value,
                   decl->GetStateName(forcedStateIndex), decl->name.str);
    return AWPATH_FAILED;
  }
  const idAnimWebNodeIndex forcedDestination =
      FindNodeIndex(decl, forcedSubWebIndex, forcedStateIndex);
  if (forcedDestination.value < 0) {
    idLib::Warning("(%s) Sub-web '%s' does not contain a node with state '%s'",
                   decl->name.str, decl->GetSubWebName(destSubWebIndex),
                   decl->states.list[forcedStateIndex.value].name.str);
    return AWPATH_FAILED;
  }
  if (destinationStateIndex.value < 0 ||
      destinationStateIndex.value >= decl->states.num) {
    idLib::Warning("Id %d, '%s', is not a valid destination state in web %s",
                   destinationStateIndex.value,
                   decl->GetStateName(destinationStateIndex), decl->name.str);
    return AWPATH_FAILED;
  }
  const idAnimWebNodeIndex destination =
      FindNodeIndex(decl, destinationSubWebIndex, destinationStateIndex);
  if (destination.value < 0) {
    idLib::Warning("(%s) Sub-web '%s' does not contain a node with state '%s'",
                   decl->name.str, decl->GetSubWebName(destSubWebIndex),
                   decl->states.list[destinationStateIndex.value].name.str);
    return AWPATH_FAILED;
  }
  blendEventMask = newBlendEventMask;
  forcedEdge.destNodeIndex = forcedDestination;
  forcedEdge.destSubWebIndex = forcedSubWebIndex;
  forcedEdge.blendParms = blendParameters;
  destStateIndices.numStates = 1;
  destStateIndices.stateIndex[0].value = destinationStateIndex.value;
  destSubWebIndex = destinationSubWebIndex;
  viaStateIndices.numStates = 0;
  viaSubWebIndex.value = -1;
  otherSubWebIndex.value = -1;
  interruptPath = INTR_PATH_YES;
  interruptBlend = INTR_BLEND_YES;
  skipForcedEdgeAutoPath = false;
  if (clearEventsOnStateChanges) eventHandler.ClearEvents();
  if (anim_verifyPaths.valueInteger > 0 &&
      destination.value != forcedDestination.value) {
    idAnimWebRoute verificationRoute;
    verificationRoute.num = 0;
    verificationRoute.cost = std::numeric_limits<int>::max();
    if (!FindPath(forcedDestination, destinationStateIndex,
                  destinationSubWebIndex, idAnimWebSubWebIndex(),
                  verificationRoute)) {
      const idDeclAnimWebNode* const startNode =
          curNodeIndex.value == -1 ? nullptr
                                   : decl->nodes.list[curNodeIndex.value];
      const idDeclAnimWebNode* const destinationNode =
          destination.value == -1 ? nullptr
                                  : decl->nodes.list[destination.value];
      if (startNode != nullptr && destinationNode != nullptr) {
        idStr startNodeName;
        startNode->GetName(decl, startNodeName);
        idStr destinationNodeName;
        destinationNode->GetName(decl, destinationNodeName);
        idLib::Warning("(%s) No path exists from '%s' to '%s'",
                       decl->name.str, startNodeName.data,
                       destinationNodeName.data);
      }
    }
  }
  if (debug) {
    const idDeclAnimWebNode* const currentNode =
        curNodeIndex.value == -1 ? nullptr
                                 : decl->nodes.list[curNodeIndex.value];
    idLib::Printf(
        "('%s') ForceAndChangeState from %s/%s to %s/%s via %s/%s\n",
        decl->name.str, decl->GetSubWebName(currentNode->subWebIndex),
        decl->GetStateName(currentNode->states.stateIndex[0]),
        decl->GetSubWebName(forcedSubWebIndex),
        decl->GetStateName(forcedStateIndex),
        decl->GetSubWebName(destinationSubWebIndex),
        decl->GetStateName(destinationStateIndex));
  }
  return AWPATH_OK;
}

awPathResult_t idAnimator_AnimWeb::ForceAndChangeState(
    const char* forcedSubWebName, idAnimWebStateIndex forcedStateIndex,
    const char* destinationSubWebName,
    idAnimWebStateIndex destinationStateIndex,
    const blendParms_t& blendParameters, int flags) {
  if (decl == nullptr) return AWPATH_FAILED;
  const idAnimWebSubWebIndex forcedSubWeb =
      decl->GetSubWebIndexForName(forcedSubWebName);
  const idAnimWebSubWebIndex destinationSubWeb =
      decl->GetSubWebIndexForName(destinationSubWebName);
  if (forcedSubWeb.value < 0) {
    idLib::Warning(
        "idAnimator_AnimWeb::ForceAndChangeState - animWeb '%s' does not have a sub-web '%s'",
        decl->name.str, forcedSubWebName);
    return AWPATH_FAILED;
  }
  if (destinationSubWeb.value < 0) {
    idLib::Warning(
        "idAnimator_AnimWeb::ForceAndChangeState - animWeb '%s' does not have a sub-web '%s'",
        decl->name.str, destinationSubWebName);
    return AWPATH_FAILED;
  }
  return ForceAndChangeState(
      idAnimWebSubWebIndex(forcedSubWeb.value), forcedStateIndex,
      idAnimWebSubWebIndex(forcedSubWeb.value), destinationStateIndex,
      blendParameters, flags);
}

awPathResult_t idAnimator_AnimWeb::ChangeState(
    const char* subWebName, idAnimWebStateIndex stateIndex,
    interruptPath_t newInterruptPath,
    interruptBlend_t newInterruptBlend, int flags) {
  if (decl == nullptr) return AWPATH_FAILED;
  const idAnimWebSubWebIndex subWeb = decl->GetSubWebIndexForName(subWebName);
  if (subWeb.value < 0) {
    idLib::Warning(
        "idAnimator_AnimWeb::ChangeState - animWeb '%s' does not have a sub-web '%s'",
        decl->name.str, subWebName);
    return AWPATH_FAILED;
  }
  return ChangeState(idAnimWebSubWebIndex(subWeb.value), stateIndex,
                     newInterruptPath, newInterruptBlend, flags);
}

awPathResult_t idAnimator_AnimWeb::ChangeState(
    const char* subWebName, const char* stateName,
    interruptPath_t newInterruptPath,
    interruptBlend_t newInterruptBlend, int flags) {
  if (decl == nullptr) return AWPATH_FAILED;
  const idAnimWebStateIndex state = decl->GetStateIndexForName(stateName);
  if (state.value < 0) {
    idLib::Warning(
        "idAnimator_AnimWeb::ChangeState - animWeb '%s' does not have a state '%s'",
        decl->name.str, stateName);
    return AWPATH_FAILED;
  }
  return ChangeState(subWebName, idAnimWebStateIndex(state.value),
                     newInterruptPath, newInterruptBlend, flags);
}

awPathResult_t idAnimator_AnimWeb::ChangeStateVia(
    const char* subWebName, idAnimWebStateIndex stateIndex,
    const char* requestedViaSubWebName,
    idAnimWebStateIndex requestedViaStateIndex,
    interruptPath_t newInterruptPath,
    interruptBlend_t newInterruptBlend, int flags) {
  if (decl == nullptr) return AWPATH_FAILED;
  const idAnimWebSubWebIndex subWeb = decl->GetSubWebIndexForName(subWebName);
  if (subWeb.value < 0) {
    idLib::Warning(
        "idAnimator_AnimWeb::ChangeStateVia - animWeb '%s' does not have a sub-web '%s'",
        decl->name.str, subWebName);
    return AWPATH_FAILED;
  }
  const idAnimWebSubWebIndex viaSubWeb =
      decl->GetSubWebIndexForName(requestedViaSubWebName);
  if (viaSubWeb.value < 0) {
    idLib::Warning(
        "idAnimator_AnimWeb::ChangeStateVia - animWeb '%s' does not have a sub-web '%s'",
        decl->name.str, requestedViaSubWebName);
    return AWPATH_FAILED;
  }
  return ChangeStateVia(
      idAnimWebSubWebIndex(subWeb.value), stateIndex,
      idAnimWebSubWebIndex(viaSubWeb.value), requestedViaStateIndex,
      newInterruptPath, newInterruptBlend, flags);
}

awPathResult_t idAnimator_AnimWeb::ChangeStateVia(
    const char* subWebName, const char* stateName,
    const char* requestedViaSubWebName, const char* requestedViaStateName,
    interruptPath_t newInterruptPath,
    interruptBlend_t newInterruptBlend, int flags) {
  if (decl == nullptr) return AWPATH_FAILED;
  const idAnimWebStateIndex state = decl->GetStateIndexForName(stateName);
  if (state.value < 0) {
    idLib::Warning(
        "idAnimator_AnimWeb::ChangeStateVia - animWeb '%s' does not have a state '%s'",
        decl->name.str, stateName);
    return AWPATH_FAILED;
  }
  const idAnimWebStateIndex viaState =
      decl->GetStateIndexForName(requestedViaStateName);
  if (viaState.value < 0) {
    idLib::Warning(
        "idAnimator_AnimWeb::ChangeStateVia - animWeb '%s' does not have a state '%s'",
        decl->name.str, requestedViaStateName);
    return AWPATH_FAILED;
  }
  return ChangeStateVia(subWebName, idAnimWebStateIndex(state.value),
                        requestedViaSubWebName,
                        idAnimWebStateIndex(viaState.value),
                        newInterruptPath, newInterruptBlend, flags);
}

void idAnimator_AnimWeb::SetDecl(const idDeclAnimWeb* declaration,
                                 const idDeclMD6* baseModel) {
  decl = declaration;
  modelAnimStates.ClearFree();
  modelAnimStates.SetNum(decl->models.num);
  modelDecls.SetNum(decl->models.num);
  for (int modelIndex = 0; modelIndex < modelAnimStates.num; ++modelIndex) {
    modelDecls.list[modelIndex] = decl->models.list[modelIndex];
    if (modelIndex == 0 && baseModel != nullptr)
      modelDecls.list[0] = baseModel;
    modelAnimStates.list[modelIndex].SetDecl(
        decl, this,
        idAnimWebModelIndex(static_cast<std::int16_t>(modelIndex)), 960);
  }
  scalars.ResizeForPort(scalars.num);
  declChangeId = decl->changeId;
  if (decl->nodes.num <= 0) {
    idLib::Error("AnimWeb '%s' has no nodes. It may be defaulted.",
                 decl->name.str);
    __debugbreak();
  }
}

void idAnimator_AnimWeb::InitWeb(const idAnimatorParms_AnimWeb& parms) {
  Free();
  const idDeclMD6* const baseModel =
      parms.animStack->jointAnimator->decl;
  handle.value = curAnimWebHandle++;
  if (curAnimWebHandle == -1) curAnimWebHandle = 0;
  md6Allocator = &parms.animStack->md6Allocator;
  eventHandler.ClearEvents();
  InternalInitWeb();
  const idList<idAnimWebScalar, 5>& declarationScalars =
      parms.webDecl->GetScalars();
  for (int index = 0; index < declarationScalars.num; ++index) {
    const idAnimWebScalar& scalar = declarationScalars.list[index];
    AddBlendVariable(scalar.name.str, scalar.scalarPtr,
                     idAnimWebScalarPair::FF_NO_OVERRIDE);
  }
  idAnimWebBlendEquation::AllocateUndeclaredScalars(scalars, scalarMem);
  SetDecl(parms.webDecl, baseModel);
  const idAnimWebNodeIndex rawNode = parms.webDecl->GetNodeIndexForName(
      parms.initialSubWebName.data, parms.initialStateName.data);
  idAnimWebNodeIndex nodeIndex(rawNode.value);
  int startTime = parms.curTime;
  if (nodeIndex.value < 0) {
    idLib::Warning("(%s) Initial node '%s/%s' does not exist!",
                   decl->name.str, parms.initialSubWebName.data,
                   parms.initialStateName.data);
    startTime = 0;
  }
  SetCurNode(nodeIndex, startTime);
  md6Allocator->Condense();
}

bool idAnimator_AnimWeb::InternalInit(const idAnimatorParms_Base& parms) {
  const auto& webParms =
      static_cast<const idAnimatorParms_AnimWeb&>(parms);
  mergeBranch = static_cast<idMD6Branch*>(
      webParms.animStack->md6Allocator.Alloc(idMD6Node::NODE_BRANCH));
  InitWeb(webParms);
  return true;
}

void idAnimator_AnimWeb::Reinitialize(int curTime) {
  SetDecl(decl, nullptr);
  const idAnimWebNodeIndex rawCurrent = decl->GetNodeIndexForName(
      curNodeSubWebName.str, curNodeStateName.str);
  if (rawCurrent.value < 0) {
    SetCurNode(idAnimWebNodeIndex(), 0);
    return;
  }
  SetCurNode(idAnimWebNodeIndex(rawCurrent.value), curTime);
  if (!(destNodeStateName != "")) return;
  const idAnimWebNodeIndex rawDestination = decl->GetNodeIndexForName(
      destNodeSubWebName.str, destNodeStateName.str);
  if (rawDestination.value < 0) return;
  const idDeclAnimWebNode* const node =
      decl->nodes.list[rawDestination.value];
  ChangeState(decl->GetSubWebName(node->subWebIndex),
              idAnimWebStateIndex(node->states.stateIndex[0].value),
              INTR_PATH_YES, INTR_BLEND_YES, 0);
}

void idAnimator_AnimWeb::Update(int curTime, int ticksPerSec) {
  if (declChangeId != decl->changeId) Reinitialize(curTime);
  InternalUpdateScalars(curTime);
  const bool wasTransitioning = IsTransitioning();

  for (int modelIndex = 0; modelIndex < modelAnimStates.num; ++modelIndex) {
    idModelAnimState& state = modelAnimStates.list[modelIndex];
    if (idAnimWebBlendTree* treeValue = state.GetBlendTree(prevNodeIndex))
      treeValue->PreBlendTree(this, curTime,
                              timeManager.gameTimePerSecond.value);
    idAnimWebBlendTree* const currentBlendTree =
        state.GetBlendTree(curNodeIndex);
    if (currentBlendTree != nullptr)
      currentBlendTree->PreBlendTree(this, curTime,
                                     timeManager.gameTimePerSecond.value);
    if (wasTransitioning) {
      const idAnimWebNodeIndex nextNode =
          curPath.GetNodeIndex(pathIndex + 1);
      if (idAnimWebBlendTree* nextBlendTree = state.GetBlendTree(nextNode))
        nextBlendTree->PreBlendTree(this, curTime,
                                    timeManager.gameTimePerSecond.value);
      if (mergeBranch->IsBlendFinished() &&
          mergeBranch->targetAlpha == 0.0f) {
        const int previousTime = gametimeManager.gameTimeManager->
            GetPreviousGameMs(GAMETIME_SCALED);
        const int currentTime =
            gametimeManager.gameTimeManager->GetGameMs(GAMETIME_SCALED);
        if (state.tree != nullptr) {
          const std::uint8_t type = state.tree->type;
          if (IsBranchType(type)) {
          static_cast<idMD6Branch*>(state.tree)->UpdateCurrentAlpha(
              currentTime, previousTime, timeManager.gameTimePerSecond.value);
          }
        }
      }
    }

    const bool wasBlending = state.blending;
    if (state.tree != nullptr && IsBranchType(state.tree->type) &&
        state.blending &&
        static_cast<idMD6Branch*>(state.tree)->IsBlendFinished()) {
      state.blending = false;
      state.tree = static_cast<idMD6Branch*>(state.tree)->right;
      for (idMD6Branch* branch : state.branches) {
        branch->left = nullptr;
        branch->right = nullptr;
      }
    }
    if (wasBlending && !state.blending) {
      const idDeclAnimWebNode* const currentNode =
          curNodeIndex.value == -1 ? nullptr
                                   : decl->nodes.list[curNodeIndex.value];
      if (currentNode != nullptr) {
        InternalSetAnimDelta(
            idAnimWebModelIndex(static_cast<std::int16_t>(modelIndex)),
            currentNode);
      }
      if (modelIndex == 0) {
        const idDeclAnimWebNode* const previousNode =
            prevNodeIndex.value == -1 ? nullptr
                                      : decl->nodes.list[prevNodeIndex.value];
        eventHandler.TriggerEventType(*this, previousNode,
                                      ANIMWEB_EVENT_END_BLEND_OUT, -1);
        eventHandler.TriggerEventType(*this, currentNode,
                                      ANIMWEB_EVENT_END_BLEND_IN, -1);
        if (pathIndex == curPath.num - 1)
          eventHandler.TriggerEventType(*this, currentNode,
                                        ANIMWEB_EVENT_DEST_END_BLEND_IN, -1);
        InternalFinishBlend(previousNode, currentNode, curTime);
      }
    }
    if (modelIndex == 0 && !IsTransitioning()) {
      const idDeclAnimWebNode* const currentNode =
          curNodeIndex.value == -1 ? nullptr
                                   : decl->nodes.list[curNodeIndex.value];
      bool allClamped = currentBlendTree == nullptr ||
                        currentBlendTree->leaves.num == 0;
      if (currentBlendTree != nullptr && currentBlendTree->leaves.num > 0) {
        allClamped = true;
        for (int leafIndex = 0;
             leafIndex < currentBlendTree->leaves.num; ++leafIndex) {
          const idMD6LeafPlay* const leaf =
              currentBlendTree->leaves.list[leafIndex];
          if (leaf->wrapMode != idMD6Leaf::WRAP_CLAMP ||
              leaf->IsPlaying(curTime,
                              timeManager.gameTimePerSecond.value)) {
            allClamped = false;
            break;
          }
        }
      }
      if (allClamped)
        eventHandler.TriggerEventType(*this, currentNode,
                                      ANIMWEB_EVENT_DEST_CLAMPED, -1);
    }
  }

  if ((!wasTransitioning || interruptPath > INTR_PATH_NO) &&
      (destStateIndices.numStates != 0 ||
       (!skipForcedEdgeAutoPath && forcedEdge.destNodeIndex.value >= 0))) {
    StartPath(curTime);
  }

  if (!IsTransitioning()) {
    bool found = false;
    const idMD6Leaf* const best = FindBestLeaf(
        curNodeIndex, idAnimWebModelIndex(0), nullptr, curTime, found);
    if (best == nullptr || best->type != idMD6Node::NODE_LEAF_PLAY)
      return;
    const auto* const leaf = static_cast<const idMD6LeafPlay*>(best);
    if (leaf->anim == nullptr) return;
    const int frameRate = leaf->anim->animData != nullptr
                              ? leaf->anim->animData->frameRate
                              : 30;
    const int numFrames = leaf->anim->animData != nullptr
                              ? leaf->anim->animData->numFrames
                              : 0;
    const int gameHz =
        gametimeManager.gameTimeManager->GetGameHz(GAMETIME_SCALED);
    const int elapsedFrames =
        gameHz * leaf->GetFrameTime(curTime) /
        timeManager.gameTimePerSecond.value;
    const int finalFrameTime = gameHz * (numFrames - 1) / frameRate;
    if (finalFrameTime - elapsedFrames == 1 ||
        (elapsedFrames >= finalFrameTime &&
         pathIndex == curPath.num - 1)) {
      const idDeclAnimWebNode* const currentNode =
          curNodeIndex.value == -1 ? nullptr
                                   : decl->nodes.list[curNodeIndex.value];
      eventHandler.TriggerEventType(*this, currentNode,
                                    ANIMWEB_EVENT_DEST_CLAMPED, -1);
    }
    return;
  }

  if (modelAnimStates.list[0].blending) {
    if (interruptBlend == INTR_BLEND_NO) return;
    if (interruptBlend < INTR_BLEND_MULTI) {
      for (int modelIndex = 0; modelIndex < modelAnimStates.num;
           ++modelIndex) {
        idModelAnimState& state = modelAnimStates.list[modelIndex];
        if (state.tree != nullptr && IsBranchType(state.tree->type)) {
          state.blending = false;
          state.tree = static_cast<idMD6Branch*>(state.tree)->right;
        }
      }
    }
  }

  int checkedPathIndex = pathIndex;
  bool skippedNodes = false;
  bool stopAtNextNode = false;
  while (checkedPathIndex < curPath.num - 1) {
    const idDeclAnimWebNode* const currentNode =
        curNodeIndex.value == -1 ? nullptr
                                 : decl->nodes.list[curNodeIndex.value];
    const idAnimWebNodeIndex checkIndex =
        curPath.GetNodeIndex(
            static_cast<std::int16_t>(checkedPathIndex + 1));
    const idDeclAnimWebNode* const checkNode =
        checkIndex.value == -1 ? nullptr
                               : decl->nodes.list[checkIndex.value];
    const awCheckPathResult_t check = CheckNextNode(currentNode, checkNode);
    if (check == AWCHECKPATH_STOP) {
      stopAtNextNode = true;
      break;
    }
    if (check == AWCHECKPATH_FAILED) {
      ++checkedPathIndex;
      skippedNodes = true;
      continue;
    }
    break;
  }
  if (stopAtNextNode) return;

  idAnimWebNodeIndex skippedDestination;
  bool foundSkippedEdge = false;
  if (skippedNodes) {
    const idDeclAnimWebNode* const currentNode =
        curNodeIndex.value == -1 ? nullptr
                                 : decl->nodes.list[curNodeIndex.value];
    skippedDestination = curPath.GetNodeIndex(
        static_cast<std::int16_t>(checkedPathIndex + 1));
    for (int localEdge = 0; localEdge < currentNode->edges.num;
         ++localEdge) {
      const int edgeIndex = currentNode->edges.list[localEdge];
      if (decl->edgeCache.list[edgeIndex].destNodeIndex.value !=
          skippedDestination.value)
        continue;
      curEdgeNodeIndex = curNodeIndex;
      curEdgeIndex = static_cast<std::int16_t>(localEdge);
      pathIndex = static_cast<std::int16_t>(checkedPathIndex);
      foundSkippedEdge = true;
      break;
    }
  }

  idAnimWebNodeIndex pathDestination = curPath.GetNodeIndex(pathIndex + 1);
  idAnimWebSubWebIndex edgeSubWeb;
  idAnimWebNodeIndex edgeDestination;
  blendParms_t blendParameters;
  if (!GetCurEdgeInfo(edgeSubWeb, edgeDestination, blendParameters)) return;
  if (skippedNodes && !foundSkippedEdge && skippedDestination.value != -1) {
    const idDeclAnimWebNode* const skippedNode =
        decl->nodes.list[skippedDestination.value];
    if (skippedNode != nullptr) {
      forcedEdge.blendParms = blendParameters;
      forcedEdge.destSubWebIndex.value = skippedNode->subWebIndex.value;
      forcedEdge.destNodeIndex = skippedDestination;
      curEdgeIndex = FORCED_EDGE_INDEX;
      skipForcedEdgeAutoPath = true;
      if (!GetCurEdgeInfo(edgeSubWeb, edgeDestination, blendParameters))
        return;
      pathIndex = static_cast<std::int16_t>(checkedPathIndex);
      pathDestination = curPath.GetNodeIndex(pathIndex + 1);
    }
  }

  AllocBlendTrees(pathDestination, curNodeIndex);
  animWebEvent_t eventToSignal = ANIMWEB_EVENT_NONE;
  if (interruptPath != INTR_PATH_IMMEDIATE) {
    if (!InternalShouldStartNextNode(
            curNodeIndex, pathDestination, curTime, ticksPerSec,
            blendParameters, idAnimWebModelIndex(0), true,
            &eventToSignal)) {
      if (eventToSignal == ANIMWEB_EVENT_NONE) return;
      const idDeclAnimWebNode* const currentNode =
          decl->nodes.list[curNodeIndex.value];
      eventHandler.TriggerEventType(*this, currentNode, eventToSignal, -1);
      return;
    }
  } else {
    interruptPath = INTR_PATH_YES;
  }

  const idDeclAnimWebNode* const previousNode =
      curNodeIndex.value == -1 ? nullptr
                               : decl->nodes.list[curNodeIndex.value];
  const idDeclAnimWebNode* const nextNode =
      edgeDestination.value == -1 ? nullptr
                                  : decl->nodes.list[edgeDestination.value];
  eventHandler.TriggerEventType(*this, previousNode,
                                ANIMWEB_EVENT_START_BLEND_OUT, -1);
  eventHandler.TriggerEventType(*this, nextNode,
                                ANIMWEB_EVENT_START_BLEND_IN, 0);
  if (pathIndex + 1 == curPath.num - 1)
    eventHandler.TriggerEventType(*this, nextNode,
                                  ANIMWEB_EVENT_DEST_START_BLEND_IN, -1);
  interruptBlend = INTR_BLEND_NO;
  InternalStartBlend(previousNode, nextNode, curTime);

  unsigned int longestAnimationLength = 0;
  int destinationFrame = 0;
  for (int modelIndex = 0; modelIndex < modelAnimStates.num; ++modelIndex) {
    const idDeclAnimWebNode::idModelInfo* const modelInfo =
        nextNode->FindModelInfo(idAnimWebModelIndex(
            static_cast<std::int16_t>(modelIndex)));
    if (modelInfo != nullptr) {
      modelAnimStates.list[modelIndex].StartBlend(
          this, modelInfo, curNodeIndex, edgeDestination, blendParameters,
          curTime, timeManager.gameTimePerSecond.value,
          longestAnimationLength, destinationFrame);
      if (nextNode->animDelta >= previousNode->animDelta ||
          forceAnimDeltaModeOnBlendStart) {
        forceAnimDeltaModeOnBlendStart = false;
        InternalSetAnimDelta(
            idAnimWebModelIndex(static_cast<std::int16_t>(modelIndex)),
            nextNode);
      }
    } else if (modelIndex == 0) {
      idStr nodeName;
      previousNode->GetName(decl, nodeName);
      idLib::Error("animWeb '%s' node '%s' missing modelinfo.",
                   decl->name.str, nodeName.data);
      break;
    }
  }
  prevNodeStartTime = nodeStartTime;
  nodeStartTime = curTime;
  nodeStartFrame = destinationFrame;
  ++pathIndex;
  const idDeclAnimWebNode* const pathDestinationNode =
      pathDestination.value == -1
          ? nullptr
          : decl->nodes.list[pathDestination.value];
  DebugPrint(
      "(%d) animWeb '%s': blending state '%s/%s' to new state '%s/%s' at time %d\n",
      gametimeManager.gameTimeManager->GetGameFrame(), decl->name.str,
      decl->GetSubWebName(previousNode->subWebIndex),
      decl->GetStateName(previousNode->states.stateIndex[0]),
      decl->GetSubWebName(pathDestinationNode->subWebIndex),
      decl->GetStateName(pathDestinationNode->states.stateIndex[0]),
      curTime);
  prevNodeIndex = curNodeIndex;
  curNodeIndex = pathDestination;
  CacheCurNode();

  if (!dontTouchMergeBranchAlpha) {
    if (blendParameters.GetDurationMS() <= 0) {
      const float alpha = (nextNode->flags & 0x10u) != 0 ? 0.0f : 1.0f;
      mergeBranch->targetAlpha = alpha;
      mergeBranch->currentAlpha = alpha;
    } else {
      mergeBranch->alphaRate =
          static_cast<float>(timeManager.gameTimePerSecond.value) /
          blendParameters.GetDurationMS();
      mergeBranch->targetAlpha =
          (nextNode->flags & 0x10u) != 0 ? 0.0f : 1.0f;
    }
  }
  for (int modelIndex = 0; modelIndex < modelAnimStates.num; ++modelIndex) {
    if (idAnimWebBlendTree* treeValue =
            modelAnimStates.list[modelIndex].GetBlendTree(curNodeIndex))
      treeValue->PreBlendTree(this, curTime,
                              timeManager.gameTimePerSecond.value);
  }
  forcedEdge.destNodeIndex.value = -1;
  forcedEdge.destSubWebIndex.value = -1;

  const idAnimWebNodeIndex nextRouteNode =
      curPath.GetNodeIndex(pathIndex + 1);
  if (nextRouteNode.value >= 0) {
    curEdgeNodeIndex.value = -1;
    curEdgeIndex = -1;
    if (curNodeIndex.value != -1) {
      const idDeclAnimWebNode* const currentNode =
          decl->nodes.list[curNodeIndex.value];
      if (currentNode != nullptr) {
        for (int localEdge = 0; localEdge < currentNode->edges.num;
             ++localEdge) {
          const int edgeIndex = currentNode->edges.list[localEdge];
          if (decl->edgeCache.list[edgeIndex].destNodeIndex.value !=
              nextRouteNode.value)
            continue;
          curEdgeNodeIndex = curNodeIndex;
          curEdgeIndex = static_cast<std::int16_t>(localEdge);
          break;
        }
        if (IsTransitioning()) {
          InternalUpdate(curTime);
          return;
        }
        idStr currentNodeName;
        currentNode->GetName(decl, currentNodeName);
        idStr nextNodeName;
        decl->nodes.list[nextRouteNode.value]->GetName(decl, nextNodeName);
        idLib::Error(
            "No edge was found from node '%s' to '%s' while traversing the animWeb '%s'",
            currentNodeName.data, nextNodeName.data, decl->name.str);
      }
    }
    idLib::Error("animWeb '%s': Current node is node valid.",
                 decl->name.str);
    __debugbreak();
  }
  curEdgeNodeIndex.value = -1;
  curEdgeIndex = -1;
  curPath.cost = std::numeric_limits<int>::max();
  curPath.num = 0;
  pathIndex = -1;
  forcedEdge.destNodeIndex.value = -1;
  forcedEdge.destSubWebIndex.value = -1;
}

void idAnimator_AnimWeb::InternalPreBlendTree(
    const idAnimStack* stack, int curTime, int ticksPerSec) {
  InternalUpdateBlendEvents(stack, curTime, ticksPerSec);
  Update(curTime, ticksPerSec);
  if (mergeBranch != nullptr) {
    mergeBranch->right = modelAnimStates.list[0].tree;
    mergeBranch->left = nullptr;
  }
}
