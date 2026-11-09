#pragma once

#include "engine/decls/animwebs/declanimweb.h"
#include "engine/gamelib/animstack/animator_channelweight.h"
#include "engine/gamelib/animstack/animweb/animwebeventhandler.h"
#include "shared/idlib/containers/staticlist.h"

#include <cstddef>
#include <cstdint>

class idAnimStack;
class idAnimWebBlendTree;
class idEventDef;
class idMD6Allocator;
class idRenderModelGui;

template <typename NodeHolder, typename NodeCache, typename NodeIndex,
          typename EdgeCache, typename Route>
class idDijkstra;

enum awPathResult_t : int {
  AWPATH_FAILED = 0,
  AWPATH_OK = 1,
  AWPATH_ALREADY_THERE = 2,
  AWPATH_ALREADY_AT_DESTINATION = AWPATH_ALREADY_THERE,
};

enum awCheckPathResult_t : int {
  AWCHECKPATH_OK = 0,
  AWCHECKPATH_STOP = 1,
  AWCHECKPATH_FAILED = 2,
};

enum interruptPath_t : int {
  INTR_PATH_NO = 0,
  INTR_PATH_YES = 1,
  INTR_PATH_IMMEDIATE = 2,
};

enum interruptBlend_t : int {
  INTR_BLEND_NO = 0,
  INTR_BLEND_YES = 1,
  INTR_BLEND_MULTI = 2,
};

enum invalidAnimWebHandle_t : int {
  INVALID_ANIMWEB_HANDLE = -1,
};

template <>
class idHandle<std::int16_t, invalidAnimWebHandle_t, -1> {
public:
  static constexpr invalidAnimWebHandle_t INVALID_VALUE =
      INVALID_ANIMWEB_HANDLE;

  constexpr idHandle() : value(-1) {}
  constexpr explicit idHandle(const std::int16_t& initial) : value(initial) {}
  bool operator==(const idHandle& other) const { return value == other.value; }
  bool operator!=(const idHandle& other) const { return value != other.value; }
  bool operator!() const { return value == -1; }
  bool IsValid() const { return value != -1; }
  void Release() { value = -1; }
  void Set(const std::int16_t& newValue) { value = newValue; }
  std::int16_t& Get() { return value; }
  const std::int16_t& Get() const { return value; }
  bool Equal(const std::int16_t& other) const { return value == other; }
  int MaxHandleValue() const { return 0x7FFF; }

  std::int16_t value;
};

using idAnimWebNodeIndex =
    idIndex<std::int16_t, idDeclAnimWeb::invalidNodeIndex_t>;
using idAnimWebModelIndex =
    idIndex<std::int16_t, idDeclAnimWeb::invalidModelIndex_t>;
using idAnimWebEdgeIndex =
    idIndex<std::int16_t, idDeclAnimWeb::invalidEdgeIndex_t>;
using idAnimWebHandle =
    idHandle<std::int16_t, invalidAnimWebHandle_t, -1>;

struct animBlendVarPair_t {
  explicit animBlendVarPair_t(float initial = 0.0f)
      : value(initial), index(-1), used(false), pad(0) {}
  float value;
  std::int16_t index;
  bool used;
  std::uint8_t pad;
};

class idAnimWebRoute {
public:
  static constexpr int MAX_PATH_LEN = 32;

  idAnimWebRoute() : path{}, num(0), cost(0x7FFFFFFF) {}
  void Clear() { num = 0; cost = 0x7FFFFFFF; }
  void Invert() {
    for (int left = 0, right = num - 1; left < right; ++left, --right) {
      const idAnimWebNodeIndex temporary = path[left];
      path[left] = path[right];
      path[right] = temporary;
    }
  }
  void AddNodeIndex(idAnimWebNodeIndex nodeIndex) {
    if (num < MAX_PATH_LEN) path[num++] = nodeIndex;
  }
  idAnimWebNodeIndex GetNodeIndex(std::int16_t index) const {
    return index >= 0 && index < num ? path[index] : idAnimWebNodeIndex{};
  }
  idAnimWebNodeIndex GetLastNodeIndex() const {
    return num > 0 ? path[num - 1] : idAnimWebNodeIndex{};
  }
  void RemoveNodeIndex(int index) {
    if (index < 0 || index >= num) return;
    for (int move = index + 1; move < num; ++move) path[move - 1] = path[move];
    --num;
  }
  int Num() const { return num; }
  int GetCost() const { return cost; }
  void SetCost(int value) { cost = value; }
  void AppendPath(const idAnimWebRoute& other);

  idAnimWebNodeIndex path[MAX_PATH_LEN];
  int num;
  int cost;
};

class idAnimatorParms_AnimWeb : public idAnimatorParms_Base {
public:
  idAnimatorParms_AnimWeb(
      idAnimStack* animStack, const char* name,
      idMD6Blend::blendOp_t blendOp,
      idMD6Blend::originBlend_t originBlend,
      md6WeightGroup_t weightGroup, md6WeightGroup_t filterGroup,
      float alpha, const idDeclAnimWeb* declaration,
      const char* initialSubWeb, const char* initialState, int currentTime)
      : idAnimatorParms_Base(animStack, name, blendOp, originBlend,
                             weightGroup, filterGroup, alpha),
        webDecl(declaration), initialStateName(initialState),
        initialSubWebName(initialSubWeb), curTime(currentTime), activeLayers() {}
  ~idAnimatorParms_AnimWeb() = default;

  const idDeclAnimWeb* webDecl;
  idStr initialStateName;
  idStr initialSubWebName;
  int curTime;
  std::uint8_t padA4[4]{};
  idList<idAtomicString, 5> activeLayers;
};

class idAnimator_AnimWeb : public idAnimator_Base {
public:
  template <typename NodeHolder, typename NodeCache, typename NodeIndex,
            typename EdgeCache, typename Route>
  friend class idDijkstra;

  static constexpr int DEFAULT_EDGE_WEIGHT = 100;
  static const int FORCED_EDGE_INDEX;
  static constexpr int MAX_BLEND_TREES = 8;
  static constexpr int ANIMWEB_NUM_RANDOM_BLEND_VALS = 7;

  struct animInfo_t {
    animInfo_t() : curFrame(0), numFrames(0), frameRate(0) {}
    int curFrame;
    int numFrames;
    int frameRate;
  };

  enum animWebError_t : int {
    ANIMWEB_ERROR_NONE = 0,
    ANIMWEB_ERROR_NOPATH = 1,
  };

  class idModelAnimState {
  public:
    static constexpr int MAX_BLENDS = 2;
    static constexpr int MAX_RECENT = 3;

    idModelAnimState();
    ~idModelAnimState();
    void SetDecl(const idDeclAnimWeb* declaration, idAnimator_AnimWeb* web,
                 idAnimWebModelIndex modelIndex, int ticksPerSec);
    void Update(int time) {
      if (blending && branches[GetRootBranchIndex()] != nullptr) {
        branches[GetRootBranchIndex()]->UpdateCurrentAlpha(time, time, 1000);
      }
    }
    void UpdateCurrentAlpha(int time, int previousTime, int ticksPerSec) {
      for (idMD6Branch* branch : branches) {
        if (branch != nullptr)
          branch->UpdateCurrentAlpha(time, previousTime, ticksPerSec);
      }
    }
    void KillBlend() { blending = false; }
    void Free(idAnimator_AnimWeb* web);
    bool BeginNodePlaying(idAnimator_AnimWeb* web,
                          const idDeclAnimWebNode::idModelInfo* modelInfo,
                          idAnimWebNodeIndex nodeIndex, int curTime,
                          int ticksPerSec);
    __forceinline void ClearTree() { tree = nullptr; blending = false; }
    void StartBlend(idAnimator_AnimWeb* web,
                    const idDeclAnimWebNode::idModelInfo* modelInfo,
                    idAnimWebNodeIndex fromNodeIndex,
                    idAnimWebNodeIndex toNodeIndex,
                    const blendParms_t& blendParms, int curTime,
                    int ticksPerSec, unsigned int& longestAnimLength,
                    int& destFrame);
    const idMD6Node* GetTree() const { return tree; }
    idMD6Node* GetTree() { return tree; }
    idAnimWebBlendTree* GetBlendTree(idAnimWebNodeIndex nodeIndex);
    bool IsBlending() const { return blending; }
    int GetRootBranchIndex() const { return blending ? 1 : 0; }
    const idMD6Branch* GetBlendBranch(int index) const { return branches[index]; }
    idMD6Branch* GetBlendBranch(int index) { return branches[index]; }
    void SetDestFrame(idMD6Node* node, const blendParms_t& blendParms,
                      int curTime, int ticksPerSec, std::uint8_t phase,
                      unsigned int& longestAnimLength, int& destFrame) const;
    void GetPhase(const idMD6Node* node, int curTime, int ticksPerSec,
                  std::uint8_t& phase) const;
    void OffsetTreeStartTimes(
        const idMD6Node*, idTypesafeNumber<int, gameTimeUnique_t>) {}
    idAnimWebBlendTree* AllocBlendTree(
        idAnimWebNodeIndex nodeIndex,
        idAnimWebNodeIndex recentNodeIndex) const;
    void FreeBlendTree(const idAnimWebNodeIndex& nodeIndex) const;
    __forceinline void AddRecentNode(const idAnimWebNodeIndex& nodeIndex) {
      recentNodes[recentNodeStart] = nodeIndex;
      if (++recentNodeStart >= MAX_RECENT) recentNodeStart = 0;
    }

    idMD6Node* tree;
    idMD6Branch* branches[MAX_BLENDS];
    idStaticList<idAnimWebBlendTree*, MAX_BLEND_TREES> blendTreeCache;
    idAnimWebNodeIndex recentNodes[MAX_RECENT];
    std::int16_t recentNodeStart;
    bool blending;
    std::uint8_t tailPadding[7];
  };

  struct forcedEdge_t {
    idAnimWebNodeIndex destNodeIndex;
    idAnimWebSubWebIndex destSubWebIndex;
    std::uint8_t pad04[4];
    blendParms_t blendParms;
  };

  idAnimator_AnimWeb();
  ~idAnimator_AnimWeb() override;

  priority_t GetStackPriority() const override { return PRIORITY_WEB; }
  void Free();
  void InitWeb(const idAnimatorParms_AnimWeb& parms);
  void Reset(const char* subWebName, idAnimWebStateIndex stateIndex,
             int curTime);
  void Reset(const char* subWebName, const char* stateName, int curTime);
  void PushSkipFlags(std::uint8_t flags) { skipFlags |= flags; }
  std::uint8_t PopSkipFlags() { const std::uint8_t value = skipFlags; skipFlags = 0; return value; }
  void PushRequiredFlags(std::uint8_t flags) { requiredFlags |= flags; }
  std::uint8_t PopRequiredFlags() { const std::uint8_t value = requiredFlags; requiredFlags = 0; return value; }

  awPathResult_t ChangeState(idAnimWebSubWebIndex subWebIndex,
                             idAnimWebStateIndex stateIndex,
                             interruptPath_t interruptPath,
                             interruptBlend_t interruptBlend, int flags);
  awPathResult_t ChangeState(const char* subWebName,
                             idAnimWebStateIndex stateIndex,
                             interruptPath_t interruptPath,
                             interruptBlend_t interruptBlend, int flags);
  awPathResult_t ChangeState(const char* subWebName, const char* stateName,
                             interruptPath_t interruptPath,
                             interruptBlend_t interruptBlend, int flags);

  awPathResult_t ChangeStateVia(
      idAnimWebSubWebIndex subWebIndex, idAnimWebStateIndex stateIndex,
      idAnimWebSubWebIndex viaSubWebIndex, idAnimWebStateIndex viaStateIndex,
      interruptPath_t interruptPath, interruptBlend_t interruptBlend,
      int flags);
  awPathResult_t ChangeStateVia(
      const char* subWebName, idAnimWebStateIndex stateIndex,
      const char* viaSubWebName, idAnimWebStateIndex viaStateIndex,
      interruptPath_t interruptPath, interruptBlend_t interruptBlend,
      int flags);
  awPathResult_t ChangeStateVia(
      const char* subWebName, const char* stateName,
      const char* viaSubWebName, const char* viaStateName,
      interruptPath_t interruptPath, interruptBlend_t interruptBlend,
      int flags);

  awPathResult_t ForceState(const char* subWebName, const char* stateName,
                            const blendParms_t& blendParms, int flags);
  awPathResult_t ForceState(const char* subWebName,
                            idAnimWebStateIndex stateIndex,
                            const blendParms_t& blendParms, int flags);
  awPathResult_t ForceState(idAnimWebSubWebIndex subWebIndex,
                            idAnimWebStateIndex stateIndex,
                            const blendParms_t& blendParms, int flags);

  awPathResult_t ForceAndChangeState(
      const char* forcedSubWebName, idAnimWebStateIndex forcedStateIndex,
      const char* destSubWebName, idAnimWebStateIndex destStateIndex,
      const blendParms_t& blendParms, int flags);
  awPathResult_t ForceAndChangeState(
      idAnimWebSubWebIndex forcedSubWebIndex,
      idAnimWebStateIndex forcedStateIndex,
      idAnimWebSubWebIndex destSubWebIndex,
      idAnimWebStateIndex destStateIndex,
      const blendParms_t& blendParms, int flags);

  bool FindPath(idAnimWebNodeIndex fromNodeIndex,
                const idDeclAnimWeb::stateList_t& targetStates,
                idAnimWebSubWebIndex targetSubWebIndex,
                idAnimWebSubWebIndex excludedSubWebIndex,
                idAnimWebRoute& route) const;
  bool FindPath(idAnimWebNodeIndex fromNodeIndex,
                idAnimWebStateIndex targetStateIndex,
                idAnimWebSubWebIndex targetSubWebIndex,
                idAnimWebSubWebIndex excludedSubWebIndex,
                idAnimWebRoute& route) const;
  void CancelPath() { curPath.Clear(); pathIndex = -1; }
  idAnimWebNodeIndex FindIntermediateNode(idAnimWebNodeIndex,
                                          idAnimWebNodeIndex destination,
                                          int) const { return destination; }
  void StartPath(int curTime);
  int GetEdgeCost(idAnimWebNodeIndex nodeIndex,
                  idAnimWebEdgeIndex edgeIndex) const {
    return InternalGetEdgeCost(nodeIndex, edgeIndex);
  }
  void SetCurNode(idAnimWebNodeIndex nodeIndex, int curTime);
  void BeginNodesPlaying(idAnimWebNodeIndex nodeIndex, int curTime);

  void SetEvent(animWebEvent_t eventType,
                idAnimWebSubWebIndex subWebIndex,
                idAnimWebStateIndex stateIndex,
                const idEventDef& eventDef,
                idAnimWebEvent::priority_t priority);
  void SetEvent(animWebEvent_t eventType, const char* subWebName,
                const char* stateName,
                idAnimWebSubWebIndex* subWebIndex,
                idAnimWebStateIndex* stateIndex,
                const idEventDef& eventDef,
                idAnimWebEvent::priority_t priority);
  void ClearEvent(animWebEvent_t eventType);
  void ClearEvents();
  void TriggerEvent(const idDeclAnimWebNode* node,
                    animWebEvent_t eventType,
                    const idAnimWebEvent& event, int eventParm);
  bool IsEventPending(animWebEvent_t eventType) const {
    return eventHandler.IsEventPending(eventType);
  }
  idAnimWebSubWebIndex GetEventSubwebIndex(animWebEvent_t eventType) const {
    return eventHandler.events[eventType].swi;
  }
  idAnimWebStateIndex GetEventStateIndex(animWebEvent_t eventType) const {
    return eventHandler.events[eventType].si;
  }
  bool IsAnyEventPending() const;
  void ClearEventsOnStateChanges(bool clear) {
    clearEventsOnStateChanges = clear;
  }

  void AddBlendVariable(const char* name, animBlendVarPair_t& pair,
                        idAnimWebScalarPair::fieldFlag_t flags) {
    pair.index = static_cast<std::int16_t>(
        AddBlendVariable(name, &pair.value, flags));
    pair.used = pair.index >= 0;
  }
  int AddBlendVariable(const char* name, float* value,
                       idAnimWebScalarPair::fieldFlag_t flags);
  bool UsesAnyScalarIndices(const idList<int, 5>& indices,
                            const idDeclAnimWebNode* node) const;
  bool UsesAnyScalarIndices(const idList<int, 5>& indices) const;
  const idList<idAnimWebScalar, 5>& GetConstScalars() const {
    return reinterpret_cast<const idList<idAnimWebScalar, 5>&>(scalars);
  }
  idList<idAnimWebScalar, 33>& GetNonConstScalars() { return scalars; }
  int NumScalars() const { return scalars.num; }
  bool FindScalar(const char* name, float& value) const;
  bool SetScalar(const char* name, float value);
  bool SetScalarPtr(const char* name, float* value);
  void SetScalarFlags(const char* name,
                      idAnimWebScalarPair::fieldFlag_t flags);
  const idScalarMemBlock& GetConstScalarMem() const { return scalarMem; }
  idScalarMemBlock& GetNonConstSCalarMem() { return scalarMem; }
  float GetScalarForScalarPair(const idAnimWebScalarPair& pair) const;
  void SetDontTouchMergeBranchAlpha(bool value) {
    dontTouchMergeBranchAlpha = value;
  }

  int NumModels() const { return modelDecls.num; }
  bool GetNodeAnimLength(int curTime, idAnimWebModelIndex modelIndex,
                         idAnimWebSubWebIndex subWebIndex,
                         idAnimWebStateIndex stateIndex,
                         idMD6Leaf::wrapMode_t wrapMode,
                         bool ignoreAdditive, animInfo_t& info) const;
  bool GetNodeAnimLength(int curTime, idAnimWebModelIndex modelIndex,
                         idAnimWebNodeIndex nodeIndex,
                         idMD6Leaf::wrapMode_t wrapMode,
                         bool ignoreAdditive, animInfo_t& info) const;
  bool GetNodeAnimLength(int curTime, idAnimWebModelIndex modelIndex,
                         idAnimWebNodeIndex nodeIndex,
                         animInfo_t& info) const {
    return GetNodeAnimLength(curTime, modelIndex, nodeIndex,
                             idMD6Leaf::WRAP_INVALID, false, info);
  }
  bool GetNodeAnimLength(int curTime, idAnimWebModelIndex modelIndex,
                         idAnimWebSubWebIndex subWebIndex,
                         idAnimWebStateIndex stateIndex,
                         animInfo_t& info) const {
    return GetNodeAnimLength(curTime, modelIndex, subWebIndex, stateIndex,
                             idMD6Leaf::WRAP_INVALID, false, info);
  }
  bool GetNodeAnimLength(int curTime, idAnimWebModelIndex modelIndex,
                         const char* subWebName, const char* stateName,
                         animInfo_t& info) const;
  bool GetNodeTotalMovement(idAnimWebModelIndex modelIndex,
                            idAnimWebSubWebIndex subWebIndex,
                            idAnimWebStateIndex stateIndex,
                            idVec3& translation, idQuat& rotation) const;

  __forceinline const idDeclAnimWeb* GetDecl() const { return decl; }
  void SetDecl(const idDeclAnimWeb* declaration,
               const idDeclMD6* baseModel);
  void SetDeclMD6(idAnimWebModelIndex modelIndex,
                  const idDeclMD6* model) {
    modelDecls.list[modelIndex.value] = model;
  }
  const idDeclMD6* GetDeclMD6(idAnimWebModelIndex modelIndex) const {
    return modelDecls.list[modelIndex.value];
  }
  int GetNumModelAnimStates() const { return modelAnimStates.num; }
  const idModelAnimState& GetModelAnimState(int index) const {
    return modelAnimStates.list[index];
  }
  idModelAnimState& GetModelAnimState(int index) {
    return modelAnimStates.list[index];
  }
  const idAnimWebHandle& GetHandle() const { return handle; }
  idAnimWebNodeIndex GetCurNodeIndex() const { return curNodeIndex; }
  idAnimWebNodeIndex GetPrevNodeIndex() const { return prevNodeIndex; }
  idAnimWebNodeIndex GetDestNodeIndex() const {
    return curPath.num > 0 ? curPath.path[curPath.num - 1]
                           : idAnimWebNodeIndex{};
  }
  idAnimWebNodeIndex GetTargetNodeIndex() const { return GetDestNodeIndex(); }
  int GetNodeStartTime() const { return nodeStartTime; }
  int GetPreveNodeStartTime() const { return prevNodeStartTime; }
  int GetNodeStartFrame() const { return nodeStartFrame; }
  void SetDebug(bool value) { debug = value; }
  bool GetDebug() const { return debug; }
  idMD6Node* GetTree(idAnimWebModelIndex modelIndex) {
    return modelAnimStates.list[modelIndex.value].tree;
  }
  const idMD6Node* GetTree(idAnimWebModelIndex modelIndex) const {
    return modelAnimStates.list[modelIndex.value].tree;
  }
  idMD6Allocator* GetAllocator() const { return md6Allocator; }
  void SetAllocator(idMD6Allocator* allocator) { md6Allocator = allocator; }

  bool GetCurEdgeInfo(idAnimWebSubWebIndex& subWebIndex,
                      idAnimWebNodeIndex& nodeIndex,
                      blendParms_t& blendParms) const;
  void SetCurEdge(idAnimWebNodeIndex nodeIndex, std::int16_t edgeIndex) {
    curEdgeNodeIndex = nodeIndex;
    curEdgeIndex = edgeIndex;
  }
  __forceinline void ClearCurEdge() {
    curEdgeNodeIndex.value = -1;
    curEdgeIndex = -1;
  }
  const char* GetCurSubWebName() const;
  const char* GetCurStateName() const;
  const char* GetDestStateName() const { return destNodeStateName.str; }
  const char* GetDestSubWebName() const { return destNodeSubWebName.str; }
  idAnimWebPath GetCurWebPath(bool includeWebName) const;
  idAnimWebPath GetDestWebPath(bool includeWebName) const;
  const idAnimWebSubWebIndex& GetDestSubWebIndex() const {
    return destSubWebIndex;
  }
  const idAnimWebStateIndex& GetDestStateIndex() const {
    return reinterpret_cast<const idAnimWebStateIndex&>(
        destStateIndices.stateIndex[0]);
  }
  const idAnimWebSubWebIndex& GetViaSubWebIndex() const {
    return viaSubWebIndex;
  }
  const idAnimWebStateIndex& GetViaStateIndex() const {
    return reinterpret_cast<const idAnimWebStateIndex&>(
        viaStateIndices.stateIndex[0]);
  }
  idAnimWebSubWebIndex GetCurSubWebIndex() const;
  idAnimWebStateIndex GetCurStateIndex() const;
  bool GetCurStateIndices(idDeclAnimWeb::stateList_t& states) const;
  const idAnimWebRoute& GetRoute() const { return curPath; }
  idAnimWebRoute& GetRoute() { return curPath; }
  int NumNodes() const { return decl != nullptr ? decl->nodes.num : 0; }
  const idDeclAnimWebNode* GetNode(idAnimWebNodeIndex index) const {
    return decl != nullptr && index.value >= 0 && index.value < decl->nodes.num
               ? decl->nodes.list[index.value]
               : nullptr;
  }
  const idDeclAnimWeb::idNodeCache* GetNodeCache(
      idAnimWebNodeIndex index) const {
    return decl != nullptr && index.value >= 0 && index.value < decl->nodeCache.num
               ? &decl->nodeCache.list[index.value]
               : nullptr;
  }
  const idAnimWebEdgeIndex* GetEdgeIndexCache() const {
    return reinterpret_cast<const idAnimWebEdgeIndex*>(decl->edgeIndexCache);
  }
  __forceinline int NumPathableSubwebs() const {
    return pathableSubWebNodeLists.num;
  }
  __forceinline const idList<idAnimWebNodeIndex, 32>& GetPathableSubwebNodeList(
      int index) const { return *pathableSubWebNodeLists.list[index]; }
  bool IsBlending() const {
    return modelAnimStates.num > 0 && modelAnimStates.list[0].blending;
  }
  bool IsTransitioning() const;
  bool PathPending() const { return pathIndex >= 0 && pathIndex < curPath.num; }
  bool IsPathing() const { return curPath.num > 0; }
  bool IsUsingForcedEdge() const { return forcedEdge.destNodeIndex.value >= 0; }
  void RemoveNodeFromPath(int index) { curPath.RemoveNodeIndex(index); }
  int GetBlendEventMask() const { return blendEventMask; }
  void ForceAnimDeltaModeOnBlendStart() {
    forceAnimDeltaModeOnBlendStart = true;
  }
  std::size_t Size() const;
  bool DecomposeAnimRef(const char* animRef, idStr& subWebName,
                        idStr& stateName) const;
  void SetError(animWebError_t value) { error = value; }
  animWebError_t GetError() const { return error; }
  const idAnimator_ChannelWeight& GetChannelWeightAnimator() const {
    return channelWeightAnimator;
  }
  idAnimator_ChannelWeight& GetChannelWeightAnimator() {
    return channelWeightAnimator;
  }
  int DebugDrawNode(idRenderModelGui*, idMD6Node*, float, float) { return 0; }
  void DebugDrawNodes(idRenderModelGui*) {}
  int DebugPrintNode(idMD6Node*, float) { return 0; }
  void DebugPrintNodes() {}
  const idMD6Leaf* FindFirstLeafForNode(idAnimWebModelIndex modelIndex,
                                        idAnimWebNodeIndex nodeIndex,
                                        int curTime,
                                        bool stillPlaying) const;

protected:
  void Update(int curTime, int ticksPerSec);
  bool InternalInit(const idAnimatorParms_Base& parms) override;
  void InternalShutdown(idAnimStack* stack) override;
  void InternalPreBlendTree(const idAnimStack* stack, int curTime,
                            int ticksPerSec) override;
  void InternalPostBlendTree(const idAnimStack*, int) override {}
  void InternalStart(const idAnimStack*, int,
                     idTypesafeNumber<int, gameTimeUnique_t>) override {}
  void InternalEnd(const idAnimStack*, int,
                   idTypesafeNumber<int, gameTimeUnique_t>) override {}
  void InternalBlend(const idAnimStack*, int, float,
                     idTypesafeNumber<int, gameTimeUnique_t>) override {}
  bool InternalIsContributing() const override;
  const idMD6Branch* InternalGetMergeBranch() const override;

  virtual void InternalInitWeb();
  virtual void InternalUpdate(int) {}
  virtual idDeclAnimWebNode::animDelta_t
  InternalGetCurrentAnimDeltaMode() const {
    return idDeclAnimWebNode::ANIMDELTA_DEFAULT;
  }
  virtual void InternalSetAnimDelta(idAnimWebModelIndex,
                                    const idDeclAnimWebNode*) {}
  virtual int InternalGetEdgeCost(idAnimWebNodeIndex,
                                  idAnimWebEdgeIndex) const {
    return DEFAULT_EDGE_WEIGHT;
  }
  virtual void InternalStartPath() {}
  virtual void InternalUpdateScalars(int) {}
  virtual void InternalStartBlend(const idDeclAnimWebNode* fromNode,
                                  const idDeclAnimWebNode* toNode,
                                  int curTime);
  virtual void InternalFinishBlend(const idDeclAnimWebNode*,
                                   const idDeclAnimWebNode*, int) {}
  virtual awCheckPathResult_t CheckNextNode(const idDeclAnimWebNode*,
                                            const idDeclAnimWebNode*) {
    return AWCHECKPATH_OK;
  }
  const idMD6Leaf* FindBestLeaf(idAnimWebNodeIndex nodeIndex,
                                idAnimWebModelIndex modelIndex,
                                const idMD6Anim* sourceAnim,
                                int curTime, bool& found) const;
  virtual bool InternalShouldStartNextNode(
      idAnimWebNodeIndex fromNodeIndex, idAnimWebNodeIndex toNodeIndex,
      int curTime, int ticksPerSec, const blendParms_t& blendParms,
      idAnimWebModelIndex modelIndex, bool force,
      animWebEvent_t* eventType);
  virtual void InternalTriggerEvent(const idDeclAnimWebNode*,
                                    animWebEvent_t,
                                    const idAnimWebHandle&, int, int) {}
  virtual void InternalTriggerBlendEvent(int, idAnimWebModelIndex,
                                         idAnimWebNodeIndex,
                                         idAnimWebNodeIndex, int) const {}
  virtual bool InternalRemapNames(idStr&, idStr&) const { return true; }
  void DebugPrint(const char* format, ...) const;
  idAnimWebBlendTree* AllocBlendTrees(idAnimWebNodeIndex nodeIndex,
                                      idAnimWebNodeIndex recentNodeIndex);

public:
  const idDeclAnimWeb* decl;
  idMD6Allocator* md6Allocator;
  idList<idAnimWebSubWebIndex, 5> initialActiveSubWebs;
  idList<idModelAnimState, 33> modelAnimStates;
  idAnimWebNodeIndex curNodeIndex;
  idAnimWebNodeIndex prevNodeIndex;
  idAnimWebSubWebIndex destSubWebIndex;
  idDeclAnimWeb::stateList_t destStateIndices;
  idAnimWebSubWebIndex viaSubWebIndex;
  idDeclAnimWeb::stateList_t viaStateIndices;
  idAnimWebSubWebIndex otherSubWebIndex;
  idAnimWebNodeIndex curEdgeNodeIndex;
  std::int16_t curEdgeIndex;
  std::uint8_t padB2[6];
  idStaticList<const idList<idAnimWebNodeIndex, 32>*, 16>
      pathableSubWebNodeLists;
  std::int16_t pathIndex;
  std::uint8_t pad152[2];
  idAnimWebRoute curPath;
  interruptPath_t interruptPath;
  interruptBlend_t interruptBlend;
  int nodeStartTime;
  int prevNodeStartTime;
  int nodeStartFrame;
  idList<idAnimWebScalar, 33> scalars;
  idScalarMemBlock scalarMem;
  int blendEventMask;
  int declChangeId;
  idAtomicString curNodeStateName;
  idAtomicString curNodeSubWebName;
  idAtomicString destNodeStateName;
  idAtomicString destNodeSubWebName;
  idAnimWebHandle handle;
  std::uint8_t pad202[6];
  idAnimWebEventHandler_Zion eventHandler;
  bool clearEventsOnStateChanges;
  std::uint8_t skipFlags;
  std::uint8_t requiredFlags;
  std::uint8_t pad283[5];
  forcedEdge_t forcedEdge;
  float randomBlendVals[ANIMWEB_NUM_RANDOM_BLEND_VALS];
  std::uint8_t pad2CC[4];
  idMD6Branch* mergeBranch;
  idAnimator_ChannelWeight channelWeightAnimator;
  idList<const idDeclMD6*, 33> modelDecls;
  animWebError_t error;
  bool debug;
  bool forceAnimDeltaModeOnBlendStart;
  bool skipForcedEdgeAutoPath;
  bool dontTouchMergeBranchAlpha;

private:
  void EndPath() { curPath.Clear(); pathIndex = -1; }
  const idMD6Leaf* FindFirstLeaf(const idMD6Node* tree, int curTime,
                                 bool stillPlaying) const;
  void Reinitialize(int curTime);
  void CacheCurNode();
  void CacheDestNode();
  void SelectRandomBlendVals(const idDeclAnimWebNode* fromNode,
                             const idDeclAnimWebNode* toNode);
  virtual void InternalUpdateBlendEvents(const idAnimStack* stack,
                                         int curTime, int ticksPerSec);

  static std::int16_t curAnimWebHandle;
};

static_assert(sizeof(idAnimWebRoute) == 0x48,
              "AnimWeb route ABI drift");
static_assert(sizeof(idAnimatorParms_AnimWeb) == 0xC0,
              "AnimWeb parameter ABI drift");
static_assert(sizeof(idAnimator_AnimWeb::animInfo_t) == 0x0C,
              "AnimWeb animation-info ABI drift");
static_assert(sizeof(idAnimator_AnimWeb::idModelAnimState) == 0x80,
              "AnimWeb model-state ABI drift");
static_assert(offsetof(idAnimator_AnimWeb::idModelAnimState,
                       blendTreeCache) == 0x18,
              "AnimWeb model-state cache offset drift");
static_assert(sizeof(idAnimator_AnimWeb::forcedEdge_t) == 0x28,
              "AnimWeb forced-edge ABI drift");
static_assert(sizeof(idAnimator_AnimWeb) == 0x3A8,
              "AnimWeb animator ABI drift");
static_assert(offsetof(idAnimator_AnimWeb, decl) == 0x50,
              "AnimWeb declaration offset drift");
static_assert(offsetof(idAnimator_AnimWeb, md6Allocator) == 0x58,
              "AnimWeb allocator offset drift");
static_assert(offsetof(idAnimator_AnimWeb, initialActiveSubWebs) == 0x60,
              "AnimWeb initial-subweb offset drift");
static_assert(offsetof(idAnimator_AnimWeb, modelAnimStates) == 0x78,
              "AnimWeb model-state offset drift");
static_assert(offsetof(idAnimator_AnimWeb, curPath) == 0x154,
              "AnimWeb route offset drift");
static_assert(offsetof(idAnimator_AnimWeb, scalars) == 0x1B0,
              "AnimWeb scalar-list offset drift");
static_assert(offsetof(idAnimator_AnimWeb, scalarMem) == 0x1C8,
              "AnimWeb scalar-memory offset drift");
static_assert(offsetof(idAnimator_AnimWeb, eventHandler) == 0x208,
              "AnimWeb event-handler offset drift");
static_assert(offsetof(idAnimator_AnimWeb, forcedEdge) == 0x288,
              "AnimWeb forced-edge offset drift");
static_assert(offsetof(idAnimator_AnimWeb, mergeBranch) == 0x2D0,
              "AnimWeb merge-branch offset drift");
static_assert(offsetof(idAnimator_AnimWeb, channelWeightAnimator) == 0x2D8,
              "AnimWeb channel-weight offset drift");
static_assert(offsetof(idAnimator_AnimWeb, modelDecls) == 0x388,
              "AnimWeb model-list offset drift");
