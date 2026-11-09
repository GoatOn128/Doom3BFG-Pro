#include "engine/models/skeletalanimation/md6parsetree.h"

#include "shared/idlib/csystems/cvarsystemlocal.h"
#include "shared/idlib/lib.h"

#include <algorithm>
#include <cstring>

idCVar anim_showDecodes(
    "anim_showDecodes", "0", CVAR_BOOL,
    "show which animations and frames were queued for decoding.");

static void GetFrame(const idMD6Leaf* leaf, int timeMSec,
                     std::int16_t& frame, float& fraction,
                     std::uint32_t& loops, const int ticksPerSec) {
  loops = 0;
  float floatFrame = 0.0f;
  const idMD6AnimData* animData = leaf->anim->animData;
  const int numFrames = animData != nullptr ? animData->numFrames : 0;
  const int frameRate = leaf->GetFrameRate();
  if (leaf->type == idMD6Node::NODE_LEAF_PAUSE) {
    floatFrame = static_cast<const idMD6LeafPause*>(leaf)->frame;
  } else if (leaf->type == idMD6Node::NODE_LEAF_PLAY) {
    const int frameTime =
        static_cast<const idMD6LeafPlay*>(leaf)->GetFrameTime(timeMSec);
    loops = frameRate * frameTime / ticksPerSec / (numFrames - 1);
    floatFrame = loops == 0 || leaf->wrapMode != 0
                     ? (1.0f / static_cast<float>(ticksPerSec)) *
                           static_cast<float>(frameTime) *
                           static_cast<float>(frameRate)
                     : static_cast<float>(numFrames - 1);
  }

  if (leaf->wrapMode != 0) {
    if (leaf->wrapMode == 1) {
      const int integerFrame = static_cast<int>(floatFrame);
      fraction = floatFrame - static_cast<float>(integerFrame);
      frame = numFrames > 1
                  ? static_cast<std::int16_t>(integerFrame % (numFrames - 1))
                  : static_cast<std::int16_t>(integerFrame);
      return;
    }
    frame = 0;
    return;
  }
  if (floatFrame < 0.0f) {
    fraction = 0.0f;
    frame = 0;
  } else if (floatFrame < static_cast<float>(numFrames)) {
    const int integerFrame = static_cast<int>(floatFrame);
    fraction = floatFrame - static_cast<float>(integerFrame);
    frame = static_cast<std::int16_t>(integerFrame);
  } else {
    frame = static_cast<std::int16_t>(numFrames - 1);
    fraction = 0.0f;
  }
}

static md6AnimCommand_t* SetupCommandBlendInfo(md6AnimCommand_t* cmd,
                                               idMD6Branch* branch) {
  std::memset(cmd, 0, sizeof(*cmd));
  cmd->op = md6AnimCommand_t::OP_POP_AND_BLEND;
  cmd->blendInfo.op = branch->op;
  cmd->blendInfo.filterGroup = branch->filterGroup;
  cmd->blendInfo.originBlend = branch->originBlend;
  float currentAlpha = branch->currentAlpha;
  switch (branch->blendType) {
    case idMD6Branch::BLEND_LINEAR:
      break;
    case idMD6Branch::BLEND_EASEIN: {
      const float squared = currentAlpha * currentAlpha;
      currentAlpha = currentAlpha <= branch->targetAlpha
                         ? squared
                         : currentAlpha + currentAlpha - squared;
      break;
    }
    case idMD6Branch::BLEND_EASEOUT: {
      const float squared = currentAlpha * currentAlpha;
      currentAlpha = currentAlpha > branch->targetAlpha
                         ? squared
                         : currentAlpha + currentAlpha - squared;
      break;
    }
    case idMD6Branch::BLEND_EASEIN_EASEOUT: {
      const float doubled = currentAlpha + currentAlpha;
      currentAlpha = doubled >= 1.0f
                         ? ((doubled - 1.0f) + (doubled - 1.0f) -
                            (doubled - 1.0f) * (doubled - 1.0f) + 1.0f) *
                               0.5f
                         : (doubled * 0.5f) * doubled;
      break;
    }
    default:
      idLib::Warning("Unknown MD6 blend type");
      break;
  }
  cmd->blendInfo.currentAlpha = currentAlpha;
  return cmd + 1;
}

static md6AnimCommand_t* SetupCommandFrameInfo(
    const idMD6Model* model, md6AnimCommand_t* cmd, idMD6Leaf* leaf,
    const int lastTimeMSec, const int timeMSec, idBounds& frameBounds,
    idBounds& translatedBounds, idBounds& normalizedBounds,
    md6BlendOutputs_t* blendOutputs, const std::uint16_t animatorFlags,
    const int ticksPerSec) {
  (void)blendOutputs;
  if (cmd == nullptr || model == nullptr || leaf == nullptr)
    return nullptr;

  std::memset(cmd, 0, sizeof(*cmd));
  cmd->op = md6AnimCommand_t::OP_DECODE_AND_PUSH;
  cmd->frameInfo.weightGroup = leaf->weightGroup;
  const idMD6Anim* anim = leaf->anim;
  if (anim != nullptr) {
    std::int16_t frame;
    float frameFraction;
    std::uint32_t loops;
    GetFrame(leaf, timeMSec, frame, frameFraction, loops, ticksPerSec);
    cmd->animName = anim->name.str;
    if (anim_showDecodes.GetInteger() != 0)
      idLib::Printf("frame %d (%g), anim %s\n", frame, frameFraction,
                    anim->name.str);
    if (leaf->type == idMD6Node::NODE_LEAF_PLAY &&
        static_cast<idMD6LeafPlay*>(leaf)->startTime < timeMSec) {
      std::int16_t lastFrame;
      float lastFrameFraction;
      std::uint32_t lastLoops;
      GetFrame(leaf, lastTimeMSec, lastFrame, lastFrameFraction, lastLoops,
               ticksPerSec);
      if (loops > lastLoops)
        cmd->frameInfo.flags |= 4u;
    }
    cmd->frameInfo.frameFraction = frameFraction;
    cmd->frameInfo.frame = frame;
    cmd->frameInfo.flags |= (animatorFlags & 1u) != 0 ? 8u : 0u;
    if (!anim->AnimIsOkForModel(model)) {
      idLib::Warning("Model '%s' cannot use animation '%s'", model->name.str,
                     leaf->anim->name.str);
      return nullptr;
    }

    const idMD6AnimData* animData = anim->animData;
    const auto* frameSetTable =
        reinterpret_cast<const std::uint8_t*>(animData) +
        animData->frameSetTblOffset;
    const auto* frameSetOffsets = reinterpret_cast<const std::uint32_t*>(
        reinterpret_cast<const std::uint8_t*>(animData) +
        animData->frameSetOffsetTblOffset);
    const std::uint8_t frameSetIndex = frameSetTable[frame];
    const std::uint32_t frameSetOffset = frameSetOffsets[frameSetIndex] * 16u;
    const auto* frameSet = reinterpret_cast<const frameSetData_t*>(
        reinterpret_cast<const std::uint8_t*>(animData) + frameSetOffset);
    cmd->frameInfo.animData = animData;
    cmd->frameInfo.animInfoSize = animData->size;
    cmd->frameInfo.frameSetData = frameSet;
    cmd->frameInfo.frameSetSize = static_cast<std::uint16_t>(
        animData->nextSize + frameSetOffsets[frameSetIndex + 1] * 16u -
        frameSetOffset);
    cmd->frameInfo.frameSetFrame =
        static_cast<std::int16_t>(frame - frameSet->frameStart);
    cmd->frameInfo.originFrameSetData = nullptr;
    cmd->frameInfo.originFrameSetFrame = cmd->frameInfo.frameSetFrame;
    cmd->frameInfo.originFrame = frame;
    cmd->frameInfo.originFrameFraction = frameFraction;

    if ((animatorFlags & 1u) != 0 && (animData->flags & 0x1eu) != 0) {
      cmd->frameInfo.flags |= 0x20u;
      if ((animatorFlags & 2u) != 0) {
        cmd->frameInfo.flags |= 0x40u;
        std::uint32_t originLoops;
        GetFrame(leaf, 2 * timeMSec - lastTimeMSec,
                 cmd->frameInfo.originFrame,
                 cmd->frameInfo.originFrameFraction, originLoops,
                 ticksPerSec);
        const int originFrame = cmd->frameInfo.originFrame;
        if (originFrame < frameSet->frameStart ||
            originFrame >= frameSet->frameStart + frameSet->frameRange) {
          const std::uint8_t originSetIndex = frameSetTable[originFrame];
          const std::uint32_t originOffset =
              frameSetOffsets[originSetIndex] * 16u;
          const auto* originSet = reinterpret_cast<const frameSetData_t*>(
              reinterpret_cast<const std::uint8_t*>(animData) + originOffset);
          cmd->frameInfo.originFrameSetSize = static_cast<std::uint16_t>(
              animData->nextSize +
              frameSetOffsets[originSetIndex + 1] * 16u - originOffset);
          cmd->frameInfo.originFrameSetData = originSet;
          cmd->frameInfo.originFrameSetFrame =
              static_cast<std::int16_t>(originFrame - originSet->frameStart);
        }
      } else {
        std::uint32_t originLoops;
        GetFrame(leaf, lastTimeMSec, cmd->frameInfo.originFrame,
                 cmd->frameInfo.originFrameFraction, originLoops,
                 ticksPerSec);
        if (originLoops != 0 ||
            cmd->frameInfo.originFrame != cmd->frameInfo.frame ||
            cmd->frameInfo.frame != 0 ||
            cmd->frameInfo.frameFraction != 0.0f) {
          const int originFrame = cmd->frameInfo.originFrame;
          if (originFrame < frameSet->frameStart ||
              originFrame >= frameSet->frameStart + frameSet->frameRange) {
            const std::uint8_t originSetIndex = frameSetTable[originFrame];
            const std::uint32_t originOffset =
                frameSetOffsets[originSetIndex] * 16u;
            const auto* originSet = reinterpret_cast<const frameSetData_t*>(
                reinterpret_cast<const std::uint8_t*>(animData) +
                originOffset);
            cmd->frameInfo.originFrameSetSize = static_cast<std::uint16_t>(
                animData->nextSize +
                frameSetOffsets[originSetIndex + 1] * 16u - originOffset);
            cmd->frameInfo.originFrameSetData = originSet;
            cmd->frameInfo.originFrameSetFrame = static_cast<std::int16_t>(
                originFrame - originSet->frameStart);
          }
        } else {
          cmd->frameInfo.flags |= 0x40u;
          GetFrame(leaf, 2 * timeMSec - lastTimeMSec,
                   cmd->frameInfo.originFrame,
                   cmd->frameInfo.originFrameFraction, originLoops,
                   ticksPerSec);
        }
      }
    }

    if ((animData->flags & 0x21u) == 0) {
      for (int axis = 0; axis < 3; ++axis) {
        translatedBounds[0][axis] = std::min(
            translatedBounds[0][axis],
            static_cast<float>(anim->translatedBounds.b[0][axis]));
        translatedBounds[1][axis] = std::max(
            translatedBounds[1][axis],
            static_cast<float>(anim->translatedBounds.b[1][axis]));
        normalizedBounds[0][axis] = std::min(
            normalizedBounds[0][axis],
            static_cast<float>(anim->normalizedBounds.b[0][axis]));
        normalizedBounds[1][axis] = std::max(
            normalizedBounds[1][axis],
            static_cast<float>(anim->normalizedBounds.b[1][axis]));
        const idBoundsShort& commandBounds =
            (animatorFlags & 1u) != 0 && (animData->flags & 0x0eu) != 0
                ? anim->normalizedBounds
                : anim->translatedBounds;
        frameBounds[0][axis] = std::min(
            frameBounds[0][axis],
            static_cast<float>(commandBounds.b[0][axis]));
        frameBounds[1][axis] = std::max(
            frameBounds[1][axis],
            static_cast<float>(commandBounds.b[1][axis]));
      }
    }
  }

  if (leaf->type == idMD6Node::NODE_LEAF_PAUSE) {
    auto* pause = static_cast<idMD6LeafPause*>(leaf);
    idList<idMD6Blend::jointMod_t, 30>& animMods =
        pause->animMods[pause->currentDeferred & 1];
    cmd->frameInfo.animModsSize =
        static_cast<std::uint16_t>(animMods.Num() << 6);
    if (cmd->frameInfo.animModsSize != 0)
      cmd->frameInfo.animMods = animMods.list;
    cmd->frameInfo.flags |= static_cast<std::uint16_t>(pause->flags);
    if ((pause->flags & 0x10) != 0) {
      frameBounds = pause->bounds;
      translatedBounds = pause->bounds;
      normalizedBounds = pause->bounds;
    } else {
      frameBounds.AddBounds(pause->bounds);
      translatedBounds.AddBounds(pause->bounds);
      normalizedBounds.AddBounds(pause->bounds);
    }
    if ((pause->flags & 0x80u) != 0)
      cmd->op = md6AnimCommand_t::OP_DECODE_FACECOMPOSITE_AND_BLEND;
  }
  return cmd + 1;
}

static md6AnimCommand_t* ParseTree_r(
    const idMD6Model* model, const int lastTimeMSec, const int timeMSec,
    const int ticksPerSec, idMD6Branch* branch, md6AnimCommand_t* cmd,
    idBounds& frameBounds, idBounds& translatedBounds,
    idBounds& normalizedBounds, md6BlendOutputs_t* blendOutputs,
    const std::uint16_t animatorFlags) {
  branch->UpdateCurrentAlpha(timeMSec, lastTimeMSec, ticksPerSec);
  const int leftTime = branch->leftTimeOverride >= 0
                           ? branch->leftTimeOverride
                           : timeMSec;
  const int rightTime = branch->rightTimeOverride >= 0
                            ? branch->rightTimeOverride
                            : timeMSec;
  if (branch->type == idMD6Node::NODE_FUSION_BRANCH) {
    static_cast<idMD6FusionBranch*>(branch)->Update(
        false, timeMSec, ticksPerSec, model->skeleton);
  } else if (branch->type == idMD6Node::NODE_BLEND_BRANCH) {
    static_cast<idMD6BlendBranch*>(branch)->Update(false, true);
  } else if (branch->type == idMD6Node::NODE_BLENDA_BRANCH) {
    static_cast<idMD6BlendAdditiveBranch*>(branch)->Update(false);
  }

  md6AnimCommand_t* next = cmd;
  md6AnimCommand_t* leftStart = nullptr;
  const std::uint8_t leftOp = branch->op;
  if (branch->filterGroup != 0 ||
      ((leftOp == 1 || leftOp == 2 || leftOp == 7 || leftOp == 9 ||
        leftOp == 10) && branch->currentAlpha != 1.0f) ||
      ((leftOp == 3 || leftOp == 5) && branch->currentAlpha != 0.0f) ||
      leftOp == 4 || leftOp == 8 || leftOp == 6) {
    leftStart = cmd;
  }

  idMD6Node* left = branch->left;
  if (left->type == idMD6Node::NODE_BEST_LEAF) {
    auto* bestLeaf = static_cast<idMD6BestLeaf*>(left);
    if (bestLeaf->leafList.num != 0) {
      std::uint16_t index = bestLeaf->bestLeafIndex;
      if (index >= bestLeaf->leafList.num)
        index = 0;
      left = bestLeaf->leafList.list[index];
    } else {
      left = nullptr;
    }
  }
  md6AnimCommand_t* leftEnd =
      left->type == idMD6Node::NODE_LEAF_PAUSE ||
              left->type == idMD6Node::NODE_LEAF_PLAY
          ? SetupCommandFrameInfo(model, leftStart,
                                  static_cast<idMD6Leaf*>(left),
                                  lastTimeMSec, leftTime, frameBounds,
                                  translatedBounds, normalizedBounds,
                                  blendOutputs, animatorFlags, ticksPerSec)
          : ParseTree_r(model, lastTimeMSec, leftTime, ticksPerSec,
                        static_cast<idMD6Branch*>(left), leftStart,
                        frameBounds, translatedBounds, normalizedBounds,
                        blendOutputs, animatorFlags);
  if (leftEnd != nullptr)
    next = leftEnd;

  md6AnimCommand_t* rightStart = nullptr;
  const std::uint8_t rightOp = branch->op;
  if (branch->filterGroup != 0 ||
      ((rightOp == 1 || rightOp == 2 || rightOp == 7 || rightOp == 9 ||
        rightOp == 10 || rightOp == 4 || rightOp == 6 || rightOp == 8) &&
       branch->currentAlpha != 0.0f) ||
      rightOp == 3 || rightOp == 5) {
    rightStart = next;
  }

  idMD6Node* right = branch->right;
  if (right->type == idMD6Node::NODE_BEST_LEAF) {
    auto* bestLeaf = static_cast<idMD6BestLeaf*>(right);
    if (bestLeaf->leafList.num != 0) {
      std::uint16_t index = bestLeaf->bestLeafIndex;
      if (index >= bestLeaf->leafList.num)
        index = 0;
      right = bestLeaf->leafList.list[index];
    } else {
      right = nullptr;
    }
  }
  md6AnimCommand_t* rightEnd =
      right->type == idMD6Node::NODE_LEAF_PAUSE ||
              right->type == idMD6Node::NODE_LEAF_PLAY
          ? SetupCommandFrameInfo(model, rightStart,
                                  static_cast<idMD6Leaf*>(right),
                                  lastTimeMSec, rightTime, frameBounds,
                                  translatedBounds, normalizedBounds,
                                  blendOutputs, animatorFlags, ticksPerSec)
          : ParseTree_r(model, lastTimeMSec, rightTime, ticksPerSec,
                        static_cast<idMD6Branch*>(right), rightStart,
                        frameBounds, translatedBounds, normalizedBounds,
                        blendOutputs, animatorFlags);
  if (rightEnd != nullptr)
    next = rightEnd;
  return leftEnd != nullptr &&
                 leftEnd->op !=
                     md6AnimCommand_t::OP_DECODE_FACECOMPOSITE_AND_BLEND &&
                 rightEnd != nullptr
             ? SetupCommandBlendInfo(next, branch)
             : next;
}

int ParseTree(const idMD6Model* model, const int lastTimeMSec,
              const int timeMSec, const int ticksPerSec, idMD6Node* tree,
              md6AnimCommand_t* cmds, const int maxCmds,
              idBounds& frameBounds, idBounds& translatedBounds,
              idBounds& normalizedBounds, md6BlendOutputs_t* blendOutputs,
              const std::uint16_t animatorFlags) {
  (void)maxCmds;
  frameBounds[0] = {1.0e30f, 1.0e30f, 1.0e30f};
  frameBounds[1] = {-1.0e30f, -1.0e30f, -1.0e30f};
  if (blendOutputs != nullptr)
    blendOutputs->fudged = false;

  int numCommands = 0;
  if (tree != nullptr && tree->IsValid()) {
    idMD6Leaf* leaf = nullptr;
    if (tree->type == idMD6Node::NODE_LEAF_PAUSE ||
        tree->type == idMD6Node::NODE_LEAF_PLAY) {
      leaf = static_cast<idMD6Leaf*>(tree);
    } else if (tree->type != idMD6Node::NODE_BEST_LEAF) {
      numCommands = static_cast<int>(
          ParseTree_r(model, lastTimeMSec, timeMSec, ticksPerSec,
                      static_cast<idMD6Branch*>(tree), cmds, frameBounds,
                      translatedBounds, normalizedBounds, blendOutputs,
                      animatorFlags) -
          cmds);
    } else {
      auto* bestLeaf = static_cast<idMD6BestLeaf*>(tree);
      if (bestLeaf->leafList.num != 0) {
        std::uint16_t index = bestLeaf->bestLeafIndex;
        if (index >= bestLeaf->leafList.num)
          index = 0;
        leaf = static_cast<idMD6Leaf*>(bestLeaf->leafList.list[index]);
      }
    }
    if (leaf != nullptr &&
        SetupCommandFrameInfo(model, cmds, leaf, lastTimeMSec, timeMSec,
                              frameBounds, translatedBounds, normalizedBounds,
                              blendOutputs, animatorFlags,
                              ticksPerSec) != nullptr) {
      numCommands = 1;
    }
  } else {
    numCommands = 1;
    std::memset(cmds, 0, sizeof(*cmds));
    cmds->op = md6AnimCommand_t::OP_DECODE_AND_PUSH;
  }

  int stackDepth = 0;
  int maxStackDepth = 0;
  for (int index = 0; index < numCommands; ++index) {
    if (cmds[index].op == md6AnimCommand_t::OP_DECODE_AND_PUSH)
      ++stackDepth;
    else if (cmds[index].op == md6AnimCommand_t::OP_POP_AND_BLEND)
      --stackDepth;
    maxStackDepth = std::max(maxStackDepth, stackDepth);
  }
  (void)maxStackDepth;

  std::memset(&cmds[numCommands], 0, sizeof(cmds[numCommands]));
  cmds[numCommands].op = md6AnimCommand_t::OP_END;
  return numCommands + 1;
}
