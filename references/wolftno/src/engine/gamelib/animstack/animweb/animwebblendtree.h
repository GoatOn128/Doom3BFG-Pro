#pragma once

#include "engine/decls/animwebs/animwebblendequation.h"
#include "engine/gamelib/animstack/animweb/animator_animweb_abi.h"
#include "shared/idlib/containers/staticlist.h"

#include <cstddef>
#include <cstdint>

using idAnimWebBlendTreeNodeIndex =
    idIndex<std::int16_t, idDeclAnimWeb::invalidNodeIndex_t>;
using idAnimWebBlendTreeModelIndex =
    idIndex<std::int16_t, idDeclAnimWeb::invalidModelIndex_t>;

class idAnimWebBlendTree {
public:
  explicit idAnimWebBlendTree(idAnimWebBlendTreeNodeIndex nodeIndex);
  ~idAnimWebBlendTree();

  bool Init(idAnimator_AnimWeb* web, const idDeclAnimWebNode* node,
            idAnimWebBlendTreeModelIndex modelIndex,
            const char* blendEquation,
            const idDeclAnimWebNode::idModelInfo& modelInfo,
            int ticksPerSec);
  void Free();
  void BeginPlaying(idAnimator_AnimWeb* web,
                    const idMD6AnimProps* animProps, int numAnimProps,
                    int curTime, int ticksPerSec);
#if defined(WOLFTNO_RETAIL_ANIMWEB_BLENDTREE_CONST_PREBLEND_ABI)
  void PreBlendTree(const idAnimator_AnimWeb* web, int curTime,
                    int ticksPerSec) const;
#else
  void PreBlendTree(const idAnimator_AnimWeb* web, int curTime,
                    int ticksPerSec);
#endif
  bool GetAnimLength(int curTime, int ticksPerSec,
                     idMD6Leaf::wrapMode_t wrapMode, bool ignoreAdditive,
                     int& curFrame, int& numFrames, int& frameRate) const;
  void GetUsedScalarIndices(const idAnimator_AnimWeb* web,
                            idList<int, 5>& usedScalars, int minIndex,
                            int maxIndex) const;
  void CopyScalars(const idAnimator_AnimWeb* web, int curTime,
                   int ticksPerSec, std::uint8_t fieldFlags) const;

  idMD6Node* GetTree() { return tree; }
  const idMD6Node* GetTree() const { return tree; }
  idAnimWebBlendTreeNodeIndex GetNodeIndex() const { return nodeIndex; }
  void SetNodeIndex(idAnimWebBlendTreeNodeIndex value) { nodeIndex = value; }
  int GetModelIndex() const { return modelIndex; }
  const idMD6LeafPlay* GetLeaf(int index) const { return leaves[index]; }
  int NumLeaves() const { return leaves.Num(); }

  idMD6Node* tree;
  idAnimWebBlendTreeNodeIndex nodeIndex;
  std::int16_t modelIndex;
  std::uint8_t recoveredPad0C[4];
  idAnimator_AnimWeb* web;
  idList<idAnimWebScalarPair, 33> scalarPairs;
  idList<const idMD6LeafPlay*, 33> leaves;

private:
  void EnumerateTree_r(idMD6Node* node,
                       idStaticList<const idMD6Node*, 32>& nodes);
  void StartTreePlaying_r(idAnimator_AnimWeb* web,
                          const idDeclMD6* md6Decl,
                          const idMD6AnimProps* animProps,
                          int numAnimProps, int curTime, int ticksPerSec,
                          idMD6Node* node);
};

static_assert(sizeof(idAnimWebBlendTree) == 0x48,
              "AnimWeb blend-tree ABI drift");
static_assert(offsetof(idAnimWebBlendTree, tree) == 0x00,
              "AnimWeb blend-tree root offset drift");
static_assert(offsetof(idAnimWebBlendTree, nodeIndex) == 0x08,
              "AnimWeb blend-tree node-index offset drift");
static_assert(offsetof(idAnimWebBlendTree, modelIndex) == 0x0A,
              "AnimWeb blend-tree model-index offset drift");
static_assert(offsetof(idAnimWebBlendTree, web) == 0x10,
              "AnimWeb blend-tree web offset drift");
static_assert(offsetof(idAnimWebBlendTree, scalarPairs) == 0x18,
              "AnimWeb blend-tree scalar-pair offset drift");
static_assert(offsetof(idAnimWebBlendTree, leaves) == 0x30,
              "AnimWeb blend-tree leaf-list offset drift");
