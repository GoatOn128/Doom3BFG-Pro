#define WOLFTNO_RETAIL_ANIMWEB_DECLMD6_LAYOUT_ABI
#define WOLFTNO_RETAIL_ANIMWEB_GETSCALAR_REFERENCE_ABI
#define WOLFTNO_RETAIL_ANIMWEB_SETSCALAR_REFERENCE_ABI
#define WOLFTNO_RETAIL_BLEND_EQ_MODELINFO_DTOR_ABI
#define WOLFTNO_RETAIL_IDLIST_FORCEINLINE_CLEARFREE_ABI
#define WOLFTNO_RETAIL_IDLIST_FORCEINLINE_CTOR_DTOR_ABI
#define WOLFTNO_RETAIL_IDMEM_SINGLETON_ABI
#define WOLFTNO_RETAIL_IDSTR_INLINE_FREEDATA_ABI
#define WOLFTNO_RETAIL_HASHINDEX_MEMBER_ABI
#define WOLFTNO_RETAIL_SYNCED_DTOR_UNWIND_ABI
#define WOLFTNO_RETAIL_SYNCED_LEAF_INIT_ABI
#include "engine/gamelib/animstack/animator_blendeq.h"

#include "engine/framework/timemanager.h"
#include "engine/gamelib/animstack/animstack.h"
#include "engine/models/skeletalanimation/jointanimator.h"
#include "shared/idlib/containers/hashindex.h"
#include "shared/idlib/lib.h"

#undef WOLFTNO_RETAIL_SYNCED_LEAF_INIT_ABI
#undef WOLFTNO_RETAIL_SYNCED_DTOR_UNWIND_ABI
#undef WOLFTNO_RETAIL_HASHINDEX_MEMBER_ABI
#undef WOLFTNO_RETAIL_IDSTR_INLINE_FREEDATA_ABI
#undef WOLFTNO_RETAIL_IDMEM_SINGLETON_ABI
#undef WOLFTNO_RETAIL_IDLIST_FORCEINLINE_CTOR_DTOR_ABI
#undef WOLFTNO_RETAIL_IDLIST_FORCEINLINE_CLEARFREE_ABI
#undef WOLFTNO_RETAIL_ANIMWEB_SETSCALAR_REFERENCE_ABI
#undef WOLFTNO_RETAIL_ANIMWEB_GETSCALAR_REFERENCE_ABI
#undef WOLFTNO_RETAIL_BLEND_EQ_MODELINFO_DTOR_ABI
#undef WOLFTNO_RETAIL_ANIMWEB_DECLMD6_LAYOUT_ABI

#include <cmath>
#include <cstdint>
#include <intrin.h>

class idEntity;
class idScenePoint {
public:
  virtual void ActivateTargets(idEntity* target);
};

extern bool forceIdListsToReallocateEveryAppend;

template <typename Type>
void* idListArrayResize(void* oldPointer, int oldCount, int newCount,
                        memTag_t tag, bool zeroBuffer);

template <>
void* idListArrayResize<idAnimWebScalarPair>(
    void* oldPointer, int oldCount, int newCount, memTag_t tag,
    bool zeroBuffer);

template <>
bool idList<idMD6Anim*, 5>::Resize(int newSize);

namespace {

constexpr float kSmallestNormalFloat = 1.1754944e-38f;

int AnimationFrameCount(const idMD6Anim* animation) {
  return animation->animData != nullptr ? animation->animData->numFrames : 0;
}

template <typename NodePointer>
bool ResizePointerList(idList<NodePointer, 5>& values, int newSize) {
  return reinterpret_cast<idList<idMD6Anim*, 5>&>(values)
      .ResizeForPort(newSize);
}

template <typename NodePointer>
void AppendPointer(idList<NodePointer, 5>& values, NodePointer value) {
  if (values.list == nullptr) {
    const int initialSize = values.granularity > 0 ? values.granularity : 1;
    ResizePointerList(values, initialSize);
  }
  if (values.num == values.size) {
    int newSize;
    if (forceIdListsToReallocateEveryAppend) {
      newSize = values.size + 1;
    } else if (values.granularity != 0) {
      newSize = values.granularity + values.size -
                (values.granularity + values.size) % values.granularity;
    } else if (values.size <= 0x3FFFFFFF) {
      newSize = values.size * 2;
      if (newSize == 0) {
        newSize = 1;
      }
    } else {
      newSize = 0x7FFFFFFF;
    }
    ResizePointerList(values, newSize);
  }
  if (values.num < values.size) {
    values.list[values.num++] = value;
  }
}

bool ContainsNode(const idList<idMD6Node*, 5>& values,
                  const idMD6Node* node) {
  for (int index = 0; index < values.num; ++index) {
    if (values.list[index] == node) {
      return true;
    }
  }
  return false;
}

bool ContainsNode(const idList<const idMD6Node*, 5>& values,
                  const idMD6Node* node) {
  for (int index = 0; index < values.num; ++index) {
    if (values.list[index] == node) {
      return true;
    }
  }
  return false;
}

void ClearScalarPairsRetail(idList<idAnimWebScalarPair, 33>& pairs) {
  if (pairs.size < 0) {
    if (pairs.listStatic == 0 || pairs.listStatic == 2) {
      if (pairs.list != nullptr) {
        idMem::Singleton()->Free(pairs.list, ALIGN_16);
      }
      pairs.list = nullptr;
      pairs.size = 0;
    }
    pairs.num = 0;
  }
  pairs.num = pairs.size > 0 ? 0 : pairs.size;
}

}  // namespace

idAnimator_BlendEq::idAnimator_BlendEq()
    : blendEq(), mergeBranch(nullptr), scalars(), scalarPairs(),
      scalarMem(nullptr), namedAnims() {}

idAnimator_BlendEq::~idAnimator_BlendEq() noexcept(false) {
  scalarMem = nullptr;
}

void idAnimator_BlendEq::CopyScalars(const idAnimStack*, int curTime,
                                     std::uint8_t fieldFlags) {
  if (scalarMem == nullptr) {
    return;
  }

  for (int index = 0; index < scalarPairs.num; ++index) {
    idAnimWebScalarPair& pair = scalarPairs.list[index];
    if ((pair.fieldFlags & fieldFlags) == 0) {
      continue;
    }

    idMD6Node* const node = pair.node;
    const std::uint8_t type = node->type;
    if (type == idMD6Node::NODE_BLEND_BRANCH ||
        type == idMD6Node::NODE_FUSION_BRANCH) {
      if (pair.fieldType == 4) {
        static_cast<idMD6BlendBranch*>(node)
            ->currentCoordinate.list[pair.coordinateIndex] =
            scalars.list[pair.scalarIndex].GetScalar(*scalarMem);
      }
      continue;
    }

    if (type == idMD6Node::NODE_BLENDA_BRANCH) {
      if (pair.fieldType == 4) {
        static_cast<idMD6BlendAdditiveBranch*>(node)->currentCoordinate =
            scalars.list[pair.scalarIndex].GetScalar(*scalarMem);
      }
      continue;
    }

    if (type == idMD6Node::NODE_BRANCH) {
      if (pair.fieldType == 0) {
        idMD6Branch* const branch = static_cast<idMD6Branch*>(node);
        branch->currentAlpha =
            scalars.list[pair.scalarIndex].GetScalar(*scalarMem);
        branch->targetAlpha =
            scalars.list[pair.scalarIndex].GetScalar(*scalarMem);
        branch->alphaRate = 0.0f;
      }
      continue;
    }

    if (type == idMD6Node::NODE_BEST_LEAF) {
      if (pair.fieldType == 0 && (pair.fieldFlags & 0x10u) != 0u) {
        const float value = std::fabs(
            scalars.list[pair.scalarIndex].GetScalar(*scalarMem));
        if (value >= kSmallestNormalFloat) {
          static_cast<idMD6BestLeaf*>(node)->desiredTag |=
              1u << (pair.data1 & 31u);
        }
      }
      continue;
    }

    if (type == idMD6Node::NODE_LEAF_PLAY) {
      idMD6LeafPlay* const leaf = static_cast<idMD6LeafPlay*>(node);
      if (pair.fieldType == 1) {
        leaf->SetRateScale(
            curTime, scalars.list[pair.scalarIndex].GetScalar(*scalarMem));
      } else if (pair.fieldType == 2) {
        int animationIndex = static_cast<int>(
            scalars.list[pair.scalarIndex].GetScalar(*scalarMem));
        if ((pair.fieldFlags & 4u) != 0u) {
          animationIndex %= static_cast<int>(pair.data1);
        }
        if ((pair.fieldFlags & 8u) != 0u) {
          animationIndex += pair.data2;
        }
        if (animationIndex < 0 || animationIndex >= namedAnims.num) {
          idLib::Warning(
              "Anim array index out of range! Index = %d ( scalar %s ), "
              "range = [0,%d)",
              animationIndex, scalars.list[pair.scalarIndex].name.str,
              namedAnims.num);
        } else {
          const idNamedAnim& indexed = namedAnims.list[animationIndex];
          if (indexed.anim != nullptr) {
            leaf->anim = indexed.anim;
            leaf->SetRateScale(curTime, indexed.rate);
            leaf->wrapMode = indexed.wrapMode;
            const int frameCount = AnimationFrameCount(indexed.anim);
            const int ticksPerSecond = timeManager.gameTimePerSecond.value;
            if (leaf->GetFrame(curTime, ticksPerSecond) >= frameCount) {
              const char* const animationName = leaf->anim->name.str;
              idLib::Warning(
                  "Leaf '%s' current frame is out of range for indexed anim "
                  "'%s'! Cur = %u, range = [0, %u)",
                  animationName, animationName,
                  leaf->GetFrame(curTime, ticksPerSecond), frameCount);
            }
          }
        }
      }
      continue;
    }

    if (type == idMD6Node::NODE_LEAF_PAUSE) {
      idMD6LeafPause* const leaf = static_cast<idMD6LeafPause*>(node);
      if (pair.fieldType == 3) {
        leaf->frame = scalars.list[pair.scalarIndex].GetScalar(*scalarMem);
      } else if (pair.fieldType == 2) {
        int animationIndex = static_cast<int>(
            scalars.list[pair.scalarIndex].GetScalar(*scalarMem));
        if ((pair.fieldFlags & 4u) != 0u) {
          animationIndex %= static_cast<int>(pair.data1);
        }
        if ((pair.fieldFlags & 8u) != 0u) {
          animationIndex += pair.data2;
        }
        if (animationIndex < 0 || animationIndex >= namedAnims.num) {
          idLib::Warning(
              "Anim array index out of range! Index = %d ( scalar %s ), "
              "range = [0,%d)",
              animationIndex, scalars.list[pair.scalarIndex].name.str,
              namedAnims.num);
        } else {
          const idNamedAnim& indexed = namedAnims.list[animationIndex];
          if (indexed.anim != nullptr) {
            leaf->anim = indexed.anim;
            leaf->wrapMode = indexed.wrapMode;
            const int frameCount = AnimationFrameCount(indexed.anim);
            if (leaf->frame >= static_cast<float>(frameCount)) {
              const char* const animationName = indexed.anim->name.str;
              idLib::Warning(
                  "Leaf '%s' current frame is out of range for indexed anim "
                  "'%s'! Cur = %f, range = [0, %u)",
                  animationName, animationName, leaf->frame, frameCount);
            }
          }
        }
      }
    }
  }
}

void idAnimator_BlendEq::InternalStart(
    const idAnimStack* stack, int curTime,
    idTypesafeNumber<int, gameTimeUnique_t>) {
  CopyScalars(stack, curTime, 2u);
}

void idAnimator_BlendEq::InternalPreBlendTree(const idAnimStack* stack,
                                              int curTime, int) {
  CopyScalars(stack, curTime, 1u);
}

template <>
__declspec(noinline) bool
idList<idAnimWebScalarPair, 33>::Resize(int newSize) {
  if (newSize <= 0) {
    ClearFree();
    return true;
  }
  if (newSize == size || listStatic != 0) {
    return true;
  }
  if (size > 0 || newSize == granularity) {
    reinterpret_cast<idScenePoint*>(this)->idScenePoint::ActivateTargets(
        reinterpret_cast<idEntity*>(static_cast<std::intptr_t>(newSize)));
  }
  list = static_cast<idAnimWebScalarPair*>(
      idListArrayResize<idAnimWebScalarPair>(
          list, size, newSize, static_cast<memTag_t>(memTag), false));
  if (list == nullptr) {
    return false;
  }
  size = newSize;
  if (num > newSize) {
    num = newSize;
  }
  return true;
}

template <>
__forceinline idList<idPair<idStr, float>, 5>::AllocResult
idList<idPair<idStr, float>, 5>::Alloc() {
  const int required = num + 1;
  if (required > size) {
    int newSize;
    if (forceIdListsToReallocateEveryAppend) {
      newSize = required;
    } else if (granularity != 0) {
      newSize = num + granularity - (num + granularity) % granularity;
    } else {
      newSize = 1;
      while (newSize < required) {
        newSize *= 2;
      }
    }
    ResizeForPort(newSize);
  }
  if (num >= size) {
    return &list[size - 1];
  }
  return &list[num++];
}

void idAnimator_BlendEq::Free(idMD6Allocator* allocator,
                              idMD6Node* root) {
  if (root == nullptr) {
    return;
  }

  idList<idMD6Node*, 5> nodesToDelete;
  {
    idList<idMD6Node*, 5> nodesToFollow;
    AppendPointer(nodesToFollow, root);
    while (nodesToFollow.num > 0) {
      idMD6Node* const node = nodesToFollow.list[--nodesToFollow.num];
      if (!ContainsNode(nodesToDelete, node)) {
        AppendPointer(nodesToDelete, node);
      }

      if (node->type == idMD6Node::NODE_BLENDA_BRANCH) {
        idMD6BlendAdditiveBranch* const additive =
            static_cast<idMD6BlendAdditiveBranch*>(node);
        AppendPointer(nodesToFollow, additive->baseAnimation);
        for (int index = 0; index < additive->animationList.num; ++index) {
          AppendPointer(nodesToFollow, additive->animationList.list[index]);
        }
      } else if (node->type == idMD6Node::NODE_BLEND_BRANCH ||
                 node->type == idMD6Node::NODE_FUSION_BRANCH) {
        idMD6BlendBranch_Base* const blend =
            static_cast<idMD6BlendBranch_Base*>(node);
        for (int index = 0; index < blend->animationList.num; ++index) {
          AppendPointer(nodesToFollow, blend->animationList.list[index]);
        }
      } else if (node->type == idMD6Node::NODE_BEST_LEAF) {
        idMD6BestLeaf* const bestLeaf = static_cast<idMD6BestLeaf*>(node);
        for (int index = 0; index < bestLeaf->leafList.num; ++index) {
          AppendPointer(nodesToFollow, bestLeaf->leafList.list[index]);
        }
      } else if (node->type == idMD6Node::NODE_BRANCH) {
        idMD6Branch* const branch = static_cast<idMD6Branch*>(node);
        if (branch->left != nullptr) {
          AppendPointer(nodesToFollow, branch->left);
        }
        if (branch->right != nullptr) {
          AppendPointer(nodesToFollow, branch->right);
        }
      }
    }
  }

  for (int index = 0; index < nodesToDelete.num; ++index) {
    idMD6Allocator::FreeTyped(allocator, nodesToDelete.list[index]);
    nodesToDelete.list[index] = nullptr;
  }
  allocator->Size();
}

void EnumerateTreeNodes_r(const idMD6Node* node,
                          idList<const idMD6Node*, 5>& nodes) {
  AppendPointer(nodes, node);
  if (node->type == idMD6Node::NODE_BLENDA_BRANCH) {
    const idMD6BlendAdditiveBranch* const additive =
        static_cast<const idMD6BlendAdditiveBranch*>(node);
    EnumerateTreeNodes_r(additive->baseAnimation, nodes);
    for (int index = 0; index < additive->animationList.num; ++index) {
      const idMD6Node* const child = additive->animationList.list[index];
      if (child != nullptr) {
        EnumerateTreeNodes_r(child, nodes);
      }
    }
  } else if (node->type == idMD6Node::NODE_BLEND_BRANCH ||
             node->type == idMD6Node::NODE_FUSION_BRANCH) {
    const idMD6BlendBranch_Base* const blend =
        static_cast<const idMD6BlendBranch_Base*>(node);
    for (int index = 0; index < blend->animationList.num; ++index) {
      const idMD6Node* const child = blend->animationList.list[index];
      if (child != nullptr) {
        EnumerateTreeNodes_r(child, nodes);
      }
    }
  } else if (node->type == idMD6Node::NODE_BEST_LEAF) {
    const idMD6BestLeaf* const bestLeaf =
        static_cast<const idMD6BestLeaf*>(node);
    for (int index = 0; index < bestLeaf->leafList.num; ++index) {
      EnumerateTreeNodes_r(bestLeaf->leafList.list[index], nodes);
    }
  } else if (node->type == idMD6Node::NODE_BRANCH) {
    const idMD6Branch* const branch = static_cast<const idMD6Branch*>(node);
    if (branch->left != nullptr) {
      EnumerateTreeNodes_r(branch->left, nodes);
    }
    if (branch->right != nullptr) {
      EnumerateTreeNodes_r(branch->right, nodes);
    }
  }
}

void FreeUnreferencedLeaves(
    idMD6Allocator* allocator, const idMD6Node* tree,
    idList<idAnimWebBlendEquation::existingNode_t, 5>& existingNodes) {
  idList<const idMD6Node*, 5> treeNodes;
  if (existingNodes.num > 0) {
    ResizePointerList(treeNodes, existingNodes.num);
    treeNodes.num = treeNodes.size > 0 ? 0 : treeNodes.size;
  }
  if (tree != nullptr) {
    EnumerateTreeNodes_r(tree, treeNodes);
  }

  for (int index = existingNodes.num - 1; index >= 0; --index) {
    if (!ContainsNode(treeNodes, existingNodes.list[index].node)) {
      idMD6Allocator::FreeTyped(allocator,
                                existingNodes.list[index].node);
      existingNodes.RemoveIndexFast(index);
    }
  }
}

void idAnimator_BlendEq::ParseBlendEquation(
    idAnimStack* animStack, int startTime,
    const idDeclAnimWebNode::idModelInfo& modelInfo) {
  if (mergeBranch == nullptr) {
    return;
  }

  idMD6Allocator* const allocator = animStack->GetAllocator();
  if (mergeBranch->right != nullptr) {
    Free(allocator, mergeBranch->right);
    mergeBranch->right = nullptr;
  }
  ClearScalarPairsRetail(scalarPairs);
  if (blendEq.IsEmpty()) {
    return;
  }

  const idMD6Model* const model = animStack->jointAnimator->decl->model;
  idList<idAnimWebBlendEquation::existingNode_t, 5> existingNodes;
  existingNodes.SetNum(namedAnims.num);
  for (int index = 0; index < namedAnims.num; ++index) {
    idMD6LeafPlay* const leaf = static_cast<idMD6LeafPlay*>(
        allocator->Alloc(idMD6Node::NODE_LEAF_PLAY));
    const idNamedAnim& named = namedAnims.list[index];
    if (named.anim == nullptr) {
      idLib::Error(
          "idAnimator_BlendEq::ParseBlendEquation() - Anim %d was not "
          "strong loaded.",
          index);
      __debugbreak();
    }
    leaf->Init(named.anim, 0, 0, 0, named.rate,
               static_cast<idMD6Leaf::wrapMode_t>(named.wrapMode),
               static_cast<md6WeightGroup_t>(named.weightGroup));
    idStr::snPrintf(existingNodes.list[index].name.str, 8, "anim%d", index);
    existingNodes.list[index].node = leaf;
  }

  if (scalars.num > scalarPairs.size) {
    scalarPairs.ResizeForPort(scalars.num);
  }
  ClearScalarPairsRetail(scalarPairs);

  idMD6Node* tree = nullptr;
  idAnimWebBlendEquation parser;
  const bool parsed = parser.Parse(
      modelInfo, model, allocator, startTime, 16, blendEq.c_str(), scalars,
      existingNodes, tree, scalarPairs);
  FreeUnreferencedLeaves(allocator, tree, existingNodes);
  if (parsed) {
    mergeBranch->right = tree;
  } else {
    Free(allocator, tree);
    mergeBranch->right = nullptr;
  }
}

void idAnimator_BlendEq::InternalShutdown(idAnimStack* animStack) {
  if (mergeBranch != nullptr) {
    idMD6Allocator* const allocator = animStack->GetAllocator();
    Free(allocator, mergeBranch->right);
    mergeBranch->right = nullptr;
    idMD6Allocator::FreeTyped(allocator, mergeBranch);
    mergeBranch = nullptr;
  }
}

void idAnimator_BlendEq::SetBlendEquation(
    idAnimStack* animStack, int startTime, const char* equation,
    const idDeclAnimWebNode::idModelInfo& modelInfo) {
  blendEq = equation;
  ParseBlendEquation(animStack, startTime, modelInfo);
  if (scalarMem == nullptr) {
    return;
  }

  idList<idPair<idStr, float>, 5> currentScalarValues;
  idHashIndex currentScalarValuesHash(1024, 1024);
  for (int index = 0; index < scalars.num; ++index) {
    const idAnimWebScalar& scalar = scalars.list[index];
    if (scalar.scalarIndex < 0 && scalar.scalarPtr == nullptr) {
      continue;
    }
    idPair<idStr, float>* const value = currentScalarValues.Alloc();
    value->first = scalar.name.str;
    value->second = scalar.GetScalar(*scalarMem);
    currentScalarValuesHash.Add(idStr::Hash(scalar.name.str),
                                currentScalarValues.num - 1);
  }

  idAnimWebBlendEquation::AllocateUndeclaredScalars(scalars, *scalarMem);
  for (int index = 0; index < scalars.num; ++index) {
    idAnimWebScalar& scalar = scalars.list[index];
    const int hash = idStr::Hash(scalar.name.str);
    for (int valueIndex = currentScalarValuesHash.First(hash);
         valueIndex != -1;
         valueIndex = currentScalarValuesHash.Next(valueIndex)) {
      if (idStr::Icmp(scalar.name.str,
                      currentScalarValues.list[valueIndex].first.c_str()) ==
          0) {
        scalar.SetScalar(*scalarMem,
                         currentScalarValues.list[valueIndex].second);
        break;
      }
    }
  }
}

bool idAnimator_BlendEq::InternalInit(const idAnimatorParms_Base& baseParms) {
  const idAnimatorParms_BlendEq& parms =
      static_cast<const idAnimatorParms_BlendEq&>(baseParms);
  idMD6Allocator* const allocator = parms.animStack->GetAllocator();
  mergeBranch = static_cast<idMD6Branch*>(
      allocator->Alloc(idMD6Node::NODE_BRANCH));
  mergeBranch->left = nullptr;
  mergeBranch->right = nullptr;
  scalars = parms.scalars;
  scalarMem = parms.scalarMem;
  namedAnims = parms.anims;

  idDeclAnimWebNode::idModelInfo dummyModelInfo;
  SetBlendEquation(parms.animStack, 0, parms.blendEq.c_str(),
                   dummyModelInfo);
  return true;
}
