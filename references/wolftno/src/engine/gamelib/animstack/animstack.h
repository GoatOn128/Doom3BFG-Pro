#pragma once

#include "engine/gamelib/animstack/animator_base.h"
#include "engine/models/skeletalanimation/md6blockalloc.h"
#include "shared/idlib/containers/staticlist.h"

#include <cstddef>
#include <cstdint>

class idClip;
class idGameTimeManager;
class idJointAnimator;
class idParallelJobList;
class idSerializer;
class idTreeAnimator;

class idAnimStack_Util {
public:
  static void UpdateTree_r(const idMD6Skel* skeleton, int timeMSec,
                           int ticksPerSec, idMD6Node* node);
  static void UpdateTree(const idMD6Skel* skeleton, int timeMSec,
                         int ticksPerSec, idMD6Node* node);
  static void FreeTree_r(idMD6Allocator* allocator, idMD6Node* node);
  static void SerializeJointMods(idSerializer& serializer,
                                 idMD6Blend::jointMod_t* jointMods,
                                 int numMods);
  static void WriteTree_r(const idMD6Skel* skeleton, int timeMSec,
                          int ticksPerSec, const class idAnimStack& stack,
                          idMD6Node* node, idBitMsg& typesMessage,
                          idBitMsg& branchMessage, idBitMsg& leafMessage);
  static idMD6Node* ReadTree_r(const class idAnimStack& stack,
                               idMD6Allocator* allocator,
                               idBitMsg& typesMessage,
                               idBitMsg& branchMessage,
                               idBitMsg& leafMessage);
};

class idAnimStack {
public:
  enum animatorFlag_t {
    ANIMATORFLAG_INITIALIZED = 1,
    ANIMATORFLAG_ENABLED = 2,
    ANIMATORFLAG_CONTRIBUTED_LAST_SERIALIZE = 4,
    ANIMATORFLAG_SERIALIZE = 8,
    ANIMATORFLAG_CLIENT_AUTHORITATIVE_ENABLED = 16,
    ANIMATORFLAG_CLIENT_AUTHORITATIVE_CONTRIBUTED = 32,
  };

  struct syncNode_t {
    idMD6LeafPlay* node{};
    float alpha{};
    std::uint8_t padding[4];
  };

  struct syncGroup_t {
    syncGroup_t();
    syncGroup_t& operator=(const syncGroup_t& other);

    idStaticList<syncNode_t, 16> nodes;
    float fraction;
    float syncRate;
    int groupNumber;
    std::uint8_t padding[4];
  };

  idAnimStack();
  ~idAnimStack();

  void Init(idTreeAnimator* animator);
  void Init(idJointAnimator* animator);
  void Shutdown();

  void BlendStack(int currentTime, int previousTime, int gameMsPerFrame,
                  int ticksPerSec, idGameTimeManager* game,
                  idParallelJobList* parallelJobList) {
    BlendStackInternal(currentTime, previousTime, gameMsPerFrame,
                       ticksPerSec, game, parallelJobList, nullptr, nullptr,
                       nullptr, nullptr);
  }
  void BlendStackToRSTU(int currentTime, int previousTime,
                        int gameMsPerFrame, int ticksPerSec,
                        idGameTimeManager* game,
                        idParallelJobList* parallelJobList, float* localR,
                        float* localS, float* localT, float* localU) {
    BlendStackInternal(currentTime, previousTime, gameMsPerFrame,
                       ticksPerSec, game, parallelJobList, localR, localS,
                       localT, localU);
  }

  void AddAnimator(idAnimator_Base* animator);
  void RemoveAnimator(idAnimator_Base* animator);
  idAnimator_Base* GetAnimator(const char* name);
  int GetAnimatorIndex(const idAnimator_Base* animator);
  void SetAnimatorFlag(const idAnimator_Base* animator,
                       animatorFlag_t flag);
  void ClearAnimatorFlag(const idAnimator_Base* animator,
                         animatorFlag_t flag);
  bool IsAnimatorFlagSet(const idAnimator_Base* animator,
                         animatorFlag_t flag);
  idAnimator_Base* GetAnimatorByIndex(int index);
  idAnimator_Base* GetAnimatorBySerializeType(serializeType_t type);

  int GetNumAnimators() const { return blendStack.num; }
  idMD6Node* GetTree() { return tree; }
  const idMD6Node* GetTree() const { return tree; }
  idJointAnimator* GetJointAnimator() const { return jointAnimator; }

  void SerializeSnapshot(idSerializer& serializer, idClip* clip,
                         int curTime, idGameTimeManager* gameTimeManager,
                         idTreeAnimator* treeAnimator);
  void BlendSnapshot(int curTime, int gameMsPerFrame, int ticksPerSec,
                     float fraction, idGameTimeManager* game,
                     idParallelJobList* parallelJobList);
  void FreeSerializedAnimators();
  bool BlendedFromSnapshot() const { return blendedFromSnapshot; }
  const idList<idMD6Node*, 5>& GetNewSnapshotNodes() const {
    return usedSnapshotNodes;
  }
  void DisableSerialization() { disableSerialization = true; }
  void SetLocalAuthorative(bool local) { skipSerialization = local; }

  void Pause(idTypesafeNumber<int, gameTimeUnique_t> curTime);
  void Unpause(idTypesafeNumber<int, gameTimeUnique_t> curTime);
  bool IsPaused() const { return pauseTime.value >= 0; }
  idTypesafeNumber<int, gameTimeUnique_t> GetPauseTime() const {
    return pauseTime;
  }

  idMD6Allocator* GetAllocator() { return &md6Allocator; }
  const idMD6Allocator* GetAllocator() const { return &md6Allocator; }
  std::uint8_t* GetTypesMemory() { return typesMemory; }
  const std::uint8_t* GetTypesMemory() const { return typesMemory; }
  std::uint8_t* GetBranchMemory() { return branchMemory; }
  const std::uint8_t* GetBranchMemory() const { return branchMemory; }
  std::uint8_t* GetLeafMemory() { return leafMemory; }
  const std::uint8_t* GetLeafMemory() const { return leafMemory; }

private:
  void UpdateSynchronization(int currentTime, int previousTime,
                             int gameMsPerFrame, int ticksPerSec);
  void PreSynchronizeNode_r(idList<syncGroup_t, 5>& groups,
                            idMD6Node* node, float branchAlpha);
  void AddNodeToSyncGroups(idList<syncGroup_t, 5>& groups,
                           idMD6LeafPlay* node, float alpha);
  void BlendStackInternal(int currentTime, int previousTime,
                          int gameMsPerFrame, int ticksPerSec,
                          idGameTimeManager* game,
                          idParallelJobList* parallelJobList, float* localR,
                          float* localS, float* localT, float* localU);
  bool ShutdownBlendStackIndex(int index);

public:
  idTreeAnimator* treeAnimator;
  idJointAnimator* jointAnimator;
  idMD6BlockAlloc md6Allocator;
  idList<idAnimator_Base*, 25> blendStack;
  idList<std::uint8_t, 25> flags;
  idMD6Node* tree;
  idTypesafeNumber<int, gameTimeUnique_t> pauseTime;
  bool skipSerialization;
  std::uint8_t typesMemory[17];
  std::uint8_t branchMemory[112];
  std::uint8_t leafMemory[272];
  bool serializedNewTree;
  std::uint8_t recoveredPadding317;
  int prevSerializedTime;
  int curSerializedTime;
  bool createdFromSnapshot;
  bool blendedFromSnapshot;
  bool disableSerialization;
  std::uint8_t recoveredPadding323[5];
  idList<idMD6Node*, 5> usedSnapshotNodes;
  idList<idMD6Node*, 5> rememberedNodes;
};

static_assert(offsetof(idAnimStack, treeAnimator) == 0x00,
              "idAnimStack::treeAnimator ABI drift");
static_assert(offsetof(idAnimStack, jointAnimator) == 0x08,
              "idAnimStack::jointAnimator ABI drift");
static_assert(offsetof(idAnimStack, md6Allocator) == 0x10,
              "idAnimStack::md6Allocator ABI drift");
static_assert(offsetof(idAnimStack, blendStack) == 0x148,
              "idAnimStack::blendStack ABI drift");
static_assert(offsetof(idAnimStack, flags) == 0x160,
              "idAnimStack::flags ABI drift");
static_assert(offsetof(idAnimStack, tree) == 0x178,
              "idAnimStack::tree ABI drift");
static_assert(offsetof(idAnimStack, pauseTime) == 0x180,
              "idAnimStack::pauseTime ABI drift");
static_assert(offsetof(idAnimStack, typesMemory) == 0x185,
              "idAnimStack::typesMemory ABI drift");
static_assert(offsetof(idAnimStack, branchMemory) == 0x196,
              "idAnimStack::branchMemory ABI drift");
static_assert(offsetof(idAnimStack, leafMemory) == 0x206,
              "idAnimStack::leafMemory ABI drift");
static_assert(offsetof(idAnimStack, serializedNewTree) == 0x316,
              "idAnimStack::serializedNewTree ABI drift");
static_assert(offsetof(idAnimStack, usedSnapshotNodes) == 0x328,
              "idAnimStack::usedSnapshotNodes ABI drift");
static_assert(offsetof(idAnimStack, rememberedNodes) == 0x340,
              "idAnimStack::rememberedNodes ABI drift");
static_assert(sizeof(idAnimStack::syncNode_t) == 0x10,
              "idAnimStack::syncNode_t ABI drift");
static_assert(sizeof(idAnimStack::syncGroup_t) == 0x128,
              "idAnimStack::syncGroup_t ABI drift");
static_assert(sizeof(idAnimStack) == 0x358, "idAnimStack ABI drift");
