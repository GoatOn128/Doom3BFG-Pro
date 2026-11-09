#include "engine/animation/md6.h"

#include "engine/models/skeletalanimation/jobs/md6blend.h"
#include "engine/models/skeletalanimation/md6parsetree.h"
#include "engine/models/skeletalanimation/md6phasetrack.h"
#include "engine/models/skeletalanimation/md6util.h"
#include "shared/idlib/csystems/cvarsystemlocal.h"

#include <algorithm>
#include <array>
#include <cmath>
#include <cstdint>
#include <cstring>
#include <limits>
#include <vector>

idCVar anim_debugFilterLogic(
    "anim_debugFilterLogic", "0", CVAR_BOOL,
    "show the filtering decisions made by MD6 best-leaf nodes");

namespace {

constexpr float kSmallestNormal = 1.1754944e-38f;

// The recovered idSoundEmitterLocal::GetVolumeAdjustment line record aliases
// this compiland's tiny rate-scale accessor RVA.  GetRateScale is the typed
// MD6 mapping; sound-emitter code remains owned by the sound subsystem.
// Recovered idSearch_Binary records map to the stable_sort/lower-bound style
// searches below.  Their two scalar deleting destructors and ten exception
// cleanup records are represented by standard C++ comparator and RAII
// lifetimes.  The seven idList records map to the typed idList operations used
// by this file and shared/idlib/containers/list.h.

template <typename T, int Tag>
void SetRetailListDefaults(idList<T, Tag>& list) {
  list.ClearFree();
  list.granularity = 30;
  list.memTag = 0;
  list.listStatic = 0;
}

int AnimFrameRate(const idMD6Anim* animation) {
  return animation != nullptr && animation->animData != nullptr
             ? animation->animData->frameRate
             : 30;
}

int AnimFrameCount(const idMD6Anim* animation) {
  return animation != nullptr && animation->animData != nullptr
             ? animation->animData->numFrames
             : 0;
}

const idMD6PhaseTrack* PhaseTrack(const idMD6Anim* animation) {
  return animation != nullptr
             ? reinterpret_cast<const idMD6PhaseTrack*>(animation->phaseTrack)
             : nullptr;
}

float SquaredDistance(const float* lhs, const float* rhs, int dimensions) {
  float distance = 0.0f;
  for (int index = 0; index < dimensions; ++index) {
    const float delta = lhs[index] - rhs[index];
    distance += delta * delta;
  }
  return distance;
}

int PopulationCount(std::uint32_t value) {
  value -= (value >> 1U) & 0x55555555U;
  value = (value & 0x33333333U) + ((value >> 2U) & 0x33333333U);
  value = (value + (value >> 4U)) & 0x0F0F0F0FU;
  return static_cast<int>((value * 0x01010101U) >> 24U);
}

bool ApplyTagFilter(const idMD6TagFilter& filter, std::uint32_t desiredTag,
                    std::uint32_t defaultTag,
                    const idList<unsigned, 30>& tags,
                    std::array<bool, 256>& skip) {
  const std::array<bool, 256> original = skip;
  const auto apply = [&](std::uint32_t requested) {
    bool found = false;
    const std::uint32_t requestedMask = requested & filter.tagMask;
    for (int index = 0; index < tags.Num() && index < 256; ++index) {
      if (skip[index])
        continue;
      const bool reject = requestedMask != 0
                              ? (requestedMask & tags[index]) == 0
                              : (filter.tagMask & tags[index]) != 0;
      skip[index] = reject;
      found |= !reject;
    }
    return found;
  };
  if (apply(desiredTag))
    return true;
  skip = original;
  if (apply(defaultTag))
    return true;
  skip = original;
  return false;
}

std::uint32_t g_animTreeRandomSeed = 0;

struct WeightedLeaf {
  idMD6Node* node;
  float weight;
};

void GatherWeightedLeaves(idMD6Node* node, float weight,
                          std::vector<WeightedLeaf>& leaves) {
  if (node == nullptr || std::fabs(weight) < kSmallestNormal)
    return;
  if (node->type != idMD6Node::NODE_BRANCH &&
      node->type != idMD6Node::NODE_BLEND_BRANCH &&
      node->type != idMD6Node::NODE_BLENDA_BRANCH &&
      node->type != idMD6Node::NODE_FUSION_BRANCH) {
    leaves.push_back({node, weight});
    return;
  }
  auto* branch = static_cast<idMD6Branch*>(node);
  const float rightWeight = weight * branch->currentAlpha;
  GatherWeightedLeaves(branch->left, weight - rightWeight, leaves);
  GatherWeightedLeaves(branch->right, rightWeight, leaves);
}

int LeafDuration(const idMD6Node* node, int ticksPerSec) {
  if (node == nullptr ||
      (node->type != idMD6Node::NODE_LEAF_PLAY &&
       node->type != idMD6Node::NODE_LEAF_PAUSE))
    return 0;
  return static_cast<const idMD6Leaf*>(node)->GetLength(ticksPerSec);
}

} // namespace

idMD6TagFilter::idMD6TagFilter() {
  type = NODE_TAG_FILTER;
  tagGroup = nullptr;
  tagMask = 0;
  tagGroupIndex = 0;
  tagBias = 0;
}

void idMD6TagFilter::Init(std::uint8_t inputTagBias, unsigned inputTagMask,
                          std::uint8_t inputTagGroupIndex,
                          const idDeclAnimWebTagGroup* inputTagGroup) {
  tagMask = inputTagMask;
  tagGroupIndex = inputTagGroupIndex;
  tagGroup = inputTagGroup;
  tagBias = inputTagBias;
}

idMD6Leaf::idMD6Leaf(idMD6Node::nodeType_t nodeType) {
  type = nodeType;
  anim = nullptr;
  weightGroup = MD6_WEIGHTGROUP_MAX;
  wrapMode = WRAP_INVALID;
  initCounter = 0;
  pad = 0;
}

int idMD6Leaf::GetFrameRate() const { return AnimFrameRate(anim); }

int idMD6Leaf::GetLength(int ticksPerSec) const {
  if (anim == nullptr)
    return 0;
  return static_cast<int>(static_cast<float>(AnimFrameCount(anim) - 1) *
                          static_cast<float>(ticksPerSec) /
                          static_cast<float>(GetFrameRate()));
}

void idMD6LeafPlay::Clear() {
  weightGroup = MD6_WEIGHTGROUP_MAX;
  wrapMode = WRAP_INVALID;
  rateScale = 1.0f;
  anim = nullptr;
  startTime = 0;
  initCounter = 0;
}

void idMD6LeafPlay::SetFrameTime(int gameTime, int targetTime) {
  startTime = static_cast<int>(static_cast<float>(gameTime) -
                               static_cast<float>(targetTime) / rateScale);
}

void idMD6LeafPlay::SetFloatFrame(int gameTime, int ticksPerSec,
                                  float targetFrame) {
  startTime = gameTime - static_cast<int>(
                             (static_cast<float>(ticksPerSec) * targetFrame /
                              static_cast<float>(GetFrameRate())) /
                             rateScale);
}

void idMD6LeafPlay::SetFrame(int gameTime, int ticksPerSec,
                             std::uint16_t targetFrame) {
  const int frameTicks = ticksPerSec * targetFrame / GetFrameRate();
  startTime = static_cast<int>(static_cast<float>(gameTime) -
                               static_cast<float>(frameTicks) / rateScale);
}

int idMD6LeafPlay::GetFrameTime(int gameTime) const {
  return gameTime >= startTime
             ? static_cast<int>(static_cast<float>(gameTime - startTime) *
                                rateScale)
             : 0;
}

int idMD6LeafPlay::GetFrameTicks(int gameTime) const {
  return GetFrameTime(gameTime);
}

float idMD6LeafPlay::GetNormalizedFrameTime(int gameTime,
                                             int ticksPerSec) const {
  if (anim == nullptr)
    return 0.0f;
  const float lastFrame = static_cast<float>(AnimFrameCount(anim)) - 1.0f;
  if (lastFrame <= 0.0f || ticksPerSec == 0)
    return 0.0f;
  const float frame = static_cast<float>(GetFrameRate() * GetFrameTime(gameTime)) /
                      static_cast<float>(ticksPerSec);
  return wrapMode != WRAP_CLAMP ? std::fmod(frame, lastFrame) / lastFrame
                                : std::min(frame / lastFrame, 1.0f);
}

void idMD6LeafPlay::SetRateScale(int gameTime, float newRateScale) {
  if (rateScale == newRateScale)
    return;
  const int currentTicks = GetFrameTime(gameTime);
  rateScale = newRateScale >= 0.0f ? newRateScale : 1.0f;
  if (std::fabs(rateScale) < kSmallestNormal)
    startTime = gameTime;
  else
    startTime = static_cast<int>(static_cast<float>(gameTime) -
                                 static_cast<float>(currentTicks) / rateScale);
}

int idMD6LeafPlay::GetAnimLength(int ticksPerSec,
                                 bool applyRateScale) const {
  int result = anim != nullptr
                   ? ticksPerSec * (AnimFrameCount(anim) - 1) / GetFrameRate()
                   : 0;
  if (applyRateScale && std::fabs(rateScale) >= kSmallestNormal)
    result = static_cast<int>((1.0f / rateScale) * result);
  return result;
}

int idMD6LeafPlay::GetAnimTicks(int ticksPerSec) const {
  return GetAnimLength(ticksPerSec, true);
}

void idMD6LeafPlay::Restart(int gameTime, int ticksPerSec,
                            std::uint16_t frame,
                            idMD6Leaf::wrapMode_t animationWrapMode) {
  ++initCounter;
  wrapMode = static_cast<std::uint8_t>(animationWrapMode);
  SetFrame(gameTime, ticksPerSec, frame);
}

void idMD6LeafPlay::Init(const idMD6Anim* animation, int gameTime,
                         int ticksPerSec, std::uint16_t frame,
                         float animationRateScale,
                         idMD6Leaf::wrapMode_t animationWrapMode,
                         md6WeightGroup_t animationWeightGroup) {
  weightGroup = static_cast<std::uint8_t>(animationWeightGroup);
  startTime = 0;
  rateScale = animationRateScale;
  anim = animation;
  initCounter = 1;
  wrapMode = static_cast<std::uint8_t>(animationWrapMode);
  SetFrame(gameTime, ticksPerSec, frame);
}

std::uint32_t idMD6LeafPlay::GetLoopCount(int gameTime,
                                          int ticksPerSec) const {
  if (anim == nullptr || wrapMode == WRAP_CLAMP || ticksPerSec == 0)
    return 0;
  const int lastFrame = AnimFrameCount(anim) - 1;
  if (lastFrame <= 0)
    return 0;
  return static_cast<std::uint32_t>(GetFrameTime(gameTime) * GetFrameRate() /
                                    ticksPerSec / lastFrame);
}

float idMD6LeafPlay::GetFloatFrame(int gameTime, int ticksPerSec) const {
  if (anim == nullptr || ticksPerSec == 0)
    return 0.0f;
  const float lastFrame = static_cast<float>(AnimFrameCount(anim) - 1);
  if (lastFrame <= 0.0f)
    return 0.0f;
  const float frame = static_cast<float>(GetFrameRate() * GetFrameTime(gameTime)) /
                      static_cast<float>(ticksPerSec);
  return wrapMode != WRAP_CLAMP ? std::fmod(frame, lastFrame)
                                : std::min(frame, lastFrame);
}

std::uint16_t idMD6LeafPlay::GetFrame(int gameTime, int ticksPerSec) const {
  if (anim == nullptr || ticksPerSec == 0)
    return 0;
  const unsigned lastFrame =
      static_cast<unsigned>(std::max(0, AnimFrameCount(anim) - 1));
  if (lastFrame == 0)
    return 0;
  const unsigned frame = static_cast<unsigned>(
      GetFrameTime(gameTime) * GetFrameRate() / ticksPerSec);
  return static_cast<std::uint16_t>(wrapMode != WRAP_CLAMP
                                        ? frame % lastFrame
                                        : std::min(frame, lastFrame));
}

int idMD6LeafPlay::GetClampTime(int, int ticksPerSec) const {
  return startTime + GetAnimLength(ticksPerSec, true);
}

bool idMD6LeafPlay::IsPlaying(int gameTime, int ticksPerSec) const {
  if (anim == nullptr || anim->animData == nullptr)
    return false;
  if (wrapMode != WRAP_CLAMP)
    return true;
  if ((anim->animData->flags & 0x1eU) != 0) {
    return gameTime - startTime <
           GetAnimLength(ticksPerSec, true) +
               ticksPerSec / std::max(1, GetFrameRate());
  }
  return GetFrame(gameTime, ticksPerSec) < AnimFrameCount(anim) - 1;
}

bool idMD6LeafPlay::IsPlaying(int gameTime, int ticksPerSec,
                              bool clampIgnoreExtraFrame) const {
  if (anim == nullptr)
    return false;
  if (wrapMode != WRAP_CLAMP)
    return true;
  if (clampIgnoreExtraFrame || anim->animData == nullptr ||
      (anim->animData->flags & 0x1eU) == 0)
    return GetFrame(gameTime, ticksPerSec) < AnimFrameCount(anim) - 1;
  return gameTime - startTime <
         GetAnimLength(ticksPerSec, true) +
             ticksPerSec / std::max(1, GetFrameRate());
}

bool idMD6LeafPlay::IsPlaying(int gameTime, int ticksPerSec,
                              idMD6Leaf::wrapMode_t animationWrapMode) const {
  if (anim == nullptr)
    return false;
  if (animationWrapMode != WRAP_CLAMP)
    return true;
  if (anim->animData != nullptr && (anim->animData->flags & 0x1eU) != 0)
    return gameTime - startTime <
           GetAnimLength(ticksPerSec, true) +
               ticksPerSec / std::max(1, GetFrameRate());
  return GetFrame(gameTime, ticksPerSec) < AnimFrameCount(anim) - 1;
}

idMD6LeafPause::idMD6LeafPause() : idMD6Leaf(NODE_LEAF_PAUSE) {
  SetRetailListDefaults(animMods[0]);
  SetRetailListDefaults(animMods[1]);
  Clear();
}

idMD6LeafPause::~idMD6LeafPause() = default;

void idMD6LeafPause::Clear() {
  weightGroup = MD6_WEIGHTGROUP_MAX;
  wrapMode = WRAP_INVALID;
  anim = nullptr;
  flags = 0;
  frame = 0.0f;
}

void idMD6LeafPause::Init(const idMD6Anim* animation, float animationFrame,
                          idMD6Leaf::wrapMode_t animationWrapMode,
                          md6WeightGroup_t animationWeightGroup) {
  frame = animationFrame;
  anim = animation;
  flags = 0;
  wrapMode = static_cast<std::uint8_t>(animationWrapMode);
  weightGroup = static_cast<std::uint8_t>(animationWeightGroup);
}

void idMD6LeafPause::SetUseStackTop(bool set) {
  flags = set ? static_cast<std::int16_t>(flags | 1)
              : static_cast<std::int16_t>(flags & ~1);
}

void idMD6LeafPause::SetClearNonModWeights(bool set) {
  flags = set ? static_cast<std::int16_t>(flags | 2)
              : static_cast<std::int16_t>(flags & ~2);
}

void idMD6LeafPause::SetOverrideFrameBounds(bool set) {
  flags = set ? static_cast<std::int16_t>(flags | 0x10)
              : static_cast<std::int16_t>(flags & ~0x10);
}

void idMD6LeafPause::SetNumAnimMods(int count) {
  if (count == animMods[0].Num())
    return;
  const idMD6Blend::jointMod_t empty{};
  animMods[0].SetNum(count, empty);
  animMods[1].SetNum(count, empty);
}

void idMD6Branch::Clear() {
  left = nullptr;
  right = nullptr;
  filterGroup = 0;
  op = 0;
  currentAlpha = 0.0f;
  targetAlpha = 0.0f;
  alphaRate = 0.0f;
  blendType = BLEND_LINEAR;
}

bool idMD6Branch::IsBlendFinished() const {
  return alphaRate == 0.0f || currentAlpha == targetAlpha;
}

bool idMD6Branch::IsChildBlendedOut(const idMD6Node* child) const {
  if (child == nullptr)
    return true;
  if ((targetAlpha > 0.0f && targetAlpha < 1.0f) || filterGroup != 0)
    return false;
  if (left == child)
    return currentAlpha >= 1.0f;
  return right == child && currentAlpha <= 0.0f;
}

bool idMD6Branch::IsBlendingOut() const {
  return currentAlpha > 0.0f && targetAlpha == 0.0f && alphaRate != 0.0f;
}

bool idMD6Branch::IsBlendingIn() const {
  return currentAlpha < 1.0f && targetAlpha >= 1.0f && alphaRate != 0.0f;
}

void idMD6Branch::Init(idMD6Node* leftNode, idMD6Node* rightNode,
                       idMD6Blend::blendOp_t blendOp,
                       idMD6Blend::originBlend_t animationOriginBlend,
                       float startAlpha, float endAlpha, float blendRate,
                       md6WeightGroup_t animationFilterGroup,
                       idMD6Branch::blendType_t animationBlendType) {
  filterGroup = static_cast<std::uint8_t>(animationFilterGroup);
  op = static_cast<std::uint8_t>(blendOp);
  currentAlpha = startAlpha;
  targetAlpha = endAlpha;
  originBlend = static_cast<std::uint8_t>(animationOriginBlend);
  left = leftNode;
  alphaRate = blendRate;
  blendType = animationBlendType;
  right = rightNode;
}

void idMD6Branch::UpdateCurrentAlpha(int timeMSec, int lastTimeMSec,
                                     int ticksPerSec) {
  const int elapsed = timeMSec - lastTimeMSec;
  if (elapsed <= 0 || ticksPerSec == 0)
    return;
  const float difference = targetAlpha - currentAlpha;
  const float elapsedSeconds =
      static_cast<float>(elapsed) / static_cast<float>(ticksPerSec);
  if (difference > 0.0f) {
    const float value = elapsedSeconds * alphaRate + currentAlpha;
    currentAlpha = value <= targetAlpha - 0.000001f ? value : targetAlpha;
  } else if (difference < 0.0f) {
    const float value = currentAlpha - elapsedSeconds * alphaRate;
    currentAlpha = targetAlpha + 0.000001f <= value ? value : targetAlpha;
  }
}

bool idMD6Node::IsValid() const {
  if (this == nullptr)
    return false;
  if (type == NODE_LEAF_PAUSE || type == NODE_LEAF_PLAY)
    return static_cast<const idMD6Leaf*>(this)->weightGroup !=
           MD6_WEIGHTGROUP_MAX;
  if (type == NODE_BEST_LEAF)
    return static_cast<const idMD6BestLeaf*>(this)->leafList.Num() > 0;
  if (type != NODE_BRANCH && type != NODE_BLEND_BRANCH &&
      type != NODE_BLENDA_BRANCH && type != NODE_FUSION_BRANCH)
    return false;
  const auto* branch = static_cast<const idMD6Branch*>(this);
  return branch->left != nullptr && branch->right != nullptr;
}

void idMD6BlendBranch_Base::InitCoordinateNodePairs(
    int dimensions, const idList<float, 5>& coordinates,
    const idList<idMD6Node*, 5>& animations, std::uint8_t flags) {
  const int count = std::min(animations.Num(),
                             dimensions > 0 ? coordinates.Num() / dimensions
                                            : 0);
  std::vector<int> order(static_cast<std::size_t>(count));
  for (int index = 0; index < count; ++index)
    order[static_cast<std::size_t>(index)] = index;
  if (dimensions == 1 || (flags & 1U) != 0) {
    std::stable_sort(order.begin(), order.end(), [&](int lhs, int rhs) {
      for (int axis = 0; axis < dimensions; ++axis) {
        const float a = coordinates[lhs * dimensions + axis];
        const float b = coordinates[rhs * dimensions + axis];
        if (a != b)
          return a < b;
      }
      return false;
    });
  }

  coordinateList.SetNum(count * dimensions, 0.0f);
  animationList.SetNum(count, nullptr);
  for (int output = 0; output < count; ++output) {
    const int input = order[static_cast<std::size_t>(output)];
    for (int axis = 0; axis < dimensions; ++axis)
      coordinateList[output * dimensions + axis] =
          coordinates[input * dimensions + axis];
    animationList[output] = animations[input];
  }

  if (dimensions == 1 && (flags & 1U) != 0 && count > 1) {
    const float first = coordinateList[0];
    const float last = coordinateList[count - 1];
    const float extent = std::max(std::fabs(first), std::fabs(last));
    if (std::fabs(first) != std::fabs(last)) {
      if (std::fabs(first) < extent) {
        coordinateList.Insert(-extent, 0);
        animationList.Insert(animationList[count - 1], 0);
      } else {
        coordinateList.Append(extent);
        animationList.Append(animationList[0]);
      }
    }
  }
}

void idMD6BlendAdditiveBranch::FindAnimation(
    idMD6Node** rightAnimation, float* alpha) const {
  if (rightAnimation == nullptr || alpha == nullptr)
    return;
  *rightAnimation = baseAnimation;
  *alpha = 0.0f;
  if (animationList.Num() <= 0)
    return;
  int nearest = 0;
  float nearestDistance = std::fabs(coordinateList[0] - currentCoordinate);
  for (int index = 1; index < animationList.Num(); ++index) {
    const float distance =
        std::fabs(coordinateList[index] - currentCoordinate);
    if (distance < nearestDistance) {
      nearest = index;
      nearestDistance = distance;
    }
  }
  *rightAnimation = animationList[nearest];
  const float coordinate = coordinateList[nearest];
  float numerator = 0.0f;
  if (coordinate < 0.0f)
    numerator = currentCoordinate <= coordinate ? coordinate
                                                 : std::min(currentCoordinate, 0.0f);
  else
    numerator = currentCoordinate >= coordinate ? coordinate
                                                 : std::max(currentCoordinate, 0.0f);
  *alpha = std::fabs(coordinate) >= kSmallestNormal ? numerator / coordinate
                                                    : 0.0f;
}

void idMD6BlendAdditiveBranch::Update(bool forceUpdate) {
  if (DispatchMD6ParseTreeBranchUpdate(this, forceUpdate, 0, 0, nullptr))
    return;
  if (!forceUpdate &&
      std::fabs(currentCoordinate - activeCoordinate) < kSmallestNormal)
    return;
  activeCoordinate = currentCoordinate;
  if (animationList.Num() <= 0) {
    currentAlpha = 0.0f;
    left = baseAnimation;
    right = baseAnimation;
    return;
  }
  FindAnimation(&right, &currentAlpha);
  left = baseAnimation;
}

void idMD6BlendAdditiveBranch::Init(
    idMD6Node* base, const idList<float, 5>& coordinates,
    const idList<idMD6Node*, 5>& animations, float coordinate,
    md6WeightGroup_t weightGroup) {
  baseAnimation = base;
  currentCoordinate = coordinate;
  InitCoordinateNodePairs(1, coordinates, animations, 0);
  activeCoordinate = currentCoordinate;
  Update(true);
  targetAlpha = currentAlpha;
  filterGroup = static_cast<std::uint8_t>(weightGroup);
  op = idMD6Blend::BOP_BLENDA;
  originBlend = idMD6Blend::ORIGINBLEND_BRANCH;
  alphaRate = 0.0f;
  blendType = BLEND_LINEAR;
}

void idMD6BlendBranch::FindAnimation(bool isSorted,
                                     idMD6Node** leftAnimation,
                                     idMD6Node** rightAnimation,
                                     float* alpha) {
  if (leftAnimation == nullptr || rightAnimation == nullptr || alpha == nullptr)
    return;
  *leftAnimation = nullptr;
  *rightAnimation = nullptr;
  *alpha = 0.0f;
  const int count = animationList.Num();
  if (count <= 0)
    return;
  activeIndices.SetNum(2, 0);
  if (count == 1) {
    *leftAnimation = *rightAnimation = animationList[0];
    return;
  }
  const float target = currentCoordinate.Num() > 0 ? currentCoordinate[0]
                                                   : 0.0f;
  int lower = -1;
  int upper = -1;
  float lowerDelta = std::numeric_limits<float>::max();
  float upperDelta = std::numeric_limits<float>::max();
  if (isSorted) {
    upper = 0;
    while (upper < count && coordinateList[upper * numDimensions] < target)
      ++upper;
    if (upper >= count)
      lower = upper = count - 1;
    else if (upper == 0)
      lower = upper = 0;
    else
      lower = upper - 1;
  } else {
    for (int index = 0; index < count; ++index) {
      const float delta = target - coordinateList[index * numDimensions];
      if (delta >= 0.0f && delta < lowerDelta) {
        lowerDelta = delta;
        lower = index;
      }
      if (delta < 0.0f && -delta < upperDelta) {
        upperDelta = -delta;
        upper = index;
      }
    }
    if (lower < 0)
      lower = upper;
    if (upper < 0)
      upper = lower;
  }
  if (lower < 0 || upper < 0)
    lower = upper = 0;
  const float lowerCoordinate = coordinateList[lower * numDimensions];
  const float upperCoordinate = coordinateList[upper * numDimensions];
  const float range = upperCoordinate - lowerCoordinate;
  *alpha = range > 0.0f
               ? (std::clamp(target, lowerCoordinate, upperCoordinate) -
                  lowerCoordinate) /
                     range
               : 0.0f;
  *leftAnimation = animationList[lower];
  *rightAnimation = animationList[upper];
  activeIndices[0] = lower;
  activeIndices[1] = upper;
}

void idMD6BlendBranch::Solve_Gaussian(idArray<float, 6>& values,
                                      idArray<float, 36>& matrix,
                                      idArray<float, 6>& solution) const {
  const int count = std::min<int>(numDimensions, 6);
  std::array<int, 6> rows{0, 1, 2, 3, 4, 5};
  for (int column = 0; column + 1 < count; ++column) {
    int pivot = column;
    float pivotMagnitude =
        std::fabs(matrix[rows[column] * count + column]);
    for (int row = column + 1; row < count; ++row) {
      const float magnitude = std::fabs(matrix[rows[row] * count + column]);
      if (magnitude > pivotMagnitude) {
        pivot = row;
        pivotMagnitude = magnitude;
      }
    }
    std::swap(rows[column], rows[pivot]);
    const int pivotRow = rows[column];
    const float pivotValue = matrix[pivotRow * count + column];
    const float inverse = pivotMagnitude >= kSmallestNormal
                              ? 1.0f / pivotValue
                              : 0.0f;
    for (int row = column + 1; row < count; ++row) {
      const int targetRow = rows[row];
      const float factor = matrix[targetRow * count + column] * inverse;
      matrix[targetRow * count + column] = 0.0f;
      for (int inner = column + 1; inner < count; ++inner)
        matrix[targetRow * count + inner] -=
            factor * matrix[pivotRow * count + inner];
      values[targetRow] -= factor * values[pivotRow];
    }
  }
  for (int row = count - 1; row >= 0; --row) {
    const int sourceRow = rows[row];
    float value = values[sourceRow];
    for (int column = row + 1; column < count; ++column)
      value -= matrix[sourceRow * count + column] * solution[column];
    const float divisor = matrix[sourceRow * count + row];
    solution[row] = std::fabs(divisor) >= kSmallestNormal
                        ? value / divisor
                        : 0.0f;
  }
}

void idMD6BlendBranch::UpdateNormal(
    const idArray<float, 6>& point0, const idArray<float, 6>& point1,
    const idArray<float, 6>& inputNormal,
    idArray<float, 6>& outputNormal) const {
  const int count = std::min<int>(numDimensions, 6);
  float deltaLength = 0.0f;
  float projection = 0.0f;
  for (int axis = 0; axis < count; ++axis) {
    const float delta = point1[axis] - point0[axis];
    deltaLength += delta * delta;
    projection += inputNormal[axis] * delta;
  }
  if (deltaLength >= kSmallestNormal) {
    projection /= deltaLength;
    for (int axis = 0; axis < count; ++axis)
      outputNormal[axis] = inputNormal[axis] -
                           projection * (point1[axis] - point0[axis]);
  } else {
    for (int axis = 0; axis < count; ++axis)
      outputNormal[axis] = inputNormal[axis];
  }
}

bool idMD6BlendBranch::BuildPointList(idList<int, 5>& indices,
                                      idArray<float, 6>& normal,
                                      bool isFillerPoint) const {
  const int count = animationList.Num();
  if (count <= 0)
    return true;
  int best = -1;
  float bestDistance = std::numeric_limits<float>::max();
  for (int candidate = 0; candidate < count; ++candidate) {
    bool used = false;
    for (int index = 0; index < indices.Num(); ++index)
      used |= indices[index] == candidate;
    if (used)
      continue;
    const float distance = SquaredDistance(
        &coordinateList[candidate * numDimensions], currentCoordinate.list,
        numDimensions);
    if (distance < bestDistance) {
      best = candidate;
      bestDistance = distance;
    }
  }
  if (best < 0)
    return true;
  if (!isFillerPoint && bestDistance < kSmallestNormal) {
    indices.Append(best);
    return true;
  }
  indices.Append(best);
  if (indices.Num() == 2) {
    for (int axis = 0; axis < numDimensions && axis < 6; ++axis)
      normal[axis] = coordinateList[indices[1] * numDimensions + axis] -
                     coordinateList[indices[0] * numDimensions + axis];
  }
  return indices.Num() >= numDimensions + 1;
}

void idMD6BlendBranch::FindBarycentric(
    idList<idPair<float, idMD6Node*>, 5>& leaves) {
  leaves.Clear();
  const int dimensions = std::min<int>(numDimensions, 6);
  if (dimensions <= 0 || animationList.Num() <= 0)
    return;

  std::vector<int> candidates(static_cast<std::size_t>(animationList.Num()));
  for (int index = 0; index < animationList.Num(); ++index)
    candidates[static_cast<std::size_t>(index)] = index;
  std::stable_sort(candidates.begin(), candidates.end(), [&](int lhs, int rhs) {
    return SquaredDistance(&coordinateList[lhs * dimensions],
                           currentCoordinate.list, dimensions) <
           SquaredDistance(&coordinateList[rhs * dimensions],
                           currentCoordinate.list, dimensions);
  });

  idList<int, 5> selected;
  selected.Append(candidates[0]);
  for (std::size_t candidateIndex = 1;
       candidateIndex < candidates.size() && selected.Num() < dimensions + 1;
       ++candidateIndex) {
    const int candidate = candidates[candidateIndex];
    idArray<float, 36> matrix{};
    const float* base = &coordinateList[selected[0] * dimensions];
    for (int column = 0; column < selected.Num(); ++column) {
      const float* point =
          &coordinateList[(column + 1 < selected.Num()
                               ? selected[column + 1]
                               : candidate) *
                          dimensions];
      for (int row = 0; row < dimensions; ++row)
        matrix[row * dimensions + column] = point[row] - base[row];
    }
    float magnitude = 0.0f;
    for (int row = 0; row < dimensions; ++row)
      magnitude += matrix[row * dimensions + selected.Num() - 1] *
                   matrix[row * dimensions + selected.Num() - 1];
    if (magnitude >= kSmallestNormal)
      selected.Append(candidate);
  }
  while (selected.Num() < dimensions + 1)
    selected.Append(selected[selected.Num() - 1]);

  activeIndices.SetNum(selected.Num(), 0);
  for (int index = 0; index < selected.Num(); ++index)
    activeIndices[index] = selected[index];
  idArray<float, 6> values{};
  idArray<float, 36> matrix{};
  idArray<float, 6> solution{};
  const float* base = &coordinateList[selected[dimensions] * dimensions];
  for (int row = 0; row < dimensions; ++row) {
    values[row] = currentCoordinate[row] - base[row];
    for (int column = 0; column < dimensions; ++column)
      matrix[row * dimensions + column] =
          coordinateList[selected[column] * dimensions + row] - base[row];
  }
  Solve_Gaussian(values, matrix, solution);
  float sum = 0.0f;
  for (int index = 0; index < dimensions; ++index) {
    const float weight = std::clamp(solution[index], 0.0f, 1.0f);
    sum += weight;
    leaves.Append({weight, animationList[selected[index]]});
  }
  leaves.Append({1.0f - sum, animationList[selected[dimensions]]});
  std::stable_sort(leaves.list, leaves.list + leaves.Num(),
                   [](const auto& lhs, const auto& rhs) {
                     return lhs.first > rhs.first;
                   });
}

void idMD6BlendBranch::Update(bool forceUpdate, bool isSorted) {
  if (DispatchMD6ParseTreeBranchUpdate(this, forceUpdate,
                                       isSorted ? 1 : 0, 0, nullptr))
    return;
  if (!forceUpdate) {
    bool changed = false;
    for (int index = 0; index < numDimensions; ++index) {
      if (index >= activeCoordinate.Num() ||
          index >= currentCoordinate.Num() ||
          std::fabs(activeCoordinate[index] - currentCoordinate[index]) >=
              kSmallestNormal) {
        changed = true;
        break;
      }
    }
    if (!changed)
      return;
  }
  activeCoordinate = currentCoordinate;
  if (animationList.Num() <= 0) {
    currentAlpha = 0.0f;
    left = nullptr;
    right = nullptr;
    return;
  }
  if (animationList.Num() == 1) {
    currentAlpha = 0.0f;
    left = right = animationList[0];
    return;
  }
  if (numDimensions == 1) {
    FindAnimation(isSorted, &left, &right, &currentAlpha);
    return;
  }

  idList<idPair<float, idMD6Node*>, 5> leaves;
  FindBarycentric(leaves);
  if (leaves.Num() <= 0) {
    currentAlpha = 0.0f;
    left = right = animationList[0];
    return;
  }
  idMD6Branch* branch = this;
  float remaining = 1.0f;
  for (int index = 0; index < numDimensions && index < leaves.Num(); ++index) {
    const float weight = leaves[index].first;
    branch->currentAlpha = std::fabs(remaining) >= kSmallestNormal
                               ? std::max(0.0f, (remaining - weight) / remaining)
                               : 0.0f;
    branch->left = leaves[index].second;
    remaining *= branch->currentAlpha;
    if (index < numDimensions - 1)
      branch = &branchList[index];
  }
  branch->right =
      leaves[std::min(static_cast<int>(numDimensions), leaves.Num() - 1)]
          .second;
}

void idMD6BlendBranch::Init(
    std::uint8_t dimensions,
    const idList<idUserChannelIndex, 30>& channels,
    const idList<float, 5>& coordinates,
    const idList<idMD6Node*, 5>& animations,
    const idList<float, 5>& coordinate, md6WeightGroup_t weightGroup,
    std::uint8_t flags) {
  numDimensions = dimensions;
  userChannelIndices = channels;
  currentCoordinate.SetNum(coordinate.Num(), 0.0f);
  for (int index = 0; index < coordinate.Num(); ++index)
    currentCoordinate[index] = coordinate[index];
  branchList.SetNum(std::max(0, static_cast<int>(numDimensions) - 1));
  idMD6Branch* next = nullptr;
  for (int index = branchList.Num() - 1; index >= 0; --index) {
    idMD6Branch& branch = branchList[index];
    branch.type = NODE_BRANCH;
    branch.left = nullptr;
    branch.right = next;
    branch.filterGroup = static_cast<std::uint8_t>(weightGroup);
    branch.op = idMD6Blend::BOP_LERP;
    branch.originBlend = idMD6Blend::ORIGINBLEND_BRANCH;
    branch.currentAlpha = branch.targetAlpha = branch.alphaRate = 0.0f;
    branch.blendType = BLEND_LINEAR;
    next = &branch;
  }
  if (next != nullptr)
    right = next;
  isAngleWrapped = (flags & 1U) != 0;
  InitCoordinateNodePairs(numDimensions, coordinates, animations, flags);
  Update(true, true);
  targetAlpha = currentAlpha;
  filterGroup = static_cast<std::uint8_t>(weightGroup);
  op = idMD6Blend::BOP_BLEND;
  originBlend = idMD6Blend::ORIGINBLEND_BRANCH;
  alphaRate = 0.0f;
  blendType = BLEND_LINEAR;
}

idMD6BestLeaf::idMD6BestLeaf() {
  type = NODE_BEST_LEAF;
  SetRetailListDefaults(leafList);
  SetRetailListDefaults(tagList);
  SetRetailListDefaults(filterList);
  tagGroup = nullptr;
  desiredTag = 0;
  activeTag = 0;
  defaultTag = 0;
  bestLeafIndex = 0xffffU;
  tagBias = 0;
  tagGroupIndex = 0;
}

void idMD6BestLeaf::Init(
    std::uint8_t inputTagBias, unsigned inputTagMask,
    std::uint8_t inputTagGroupIndex,
    const idList<idMD6Node*, 30>& leaves,
    const idList<unsigned, 30>& tags,
    const idList<idMD6Filter*, 30>& filters, unsigned inputDefaultTag,
    const idDeclAnimWebTagGroup* inputTagGroup) {
  leafList = leaves;
  tagList = tags;
  filterList = filters;
  tagBias = inputTagBias;
  tagGroupIndex = inputTagGroupIndex;
  tagGroup = inputTagGroup;
  desiredTag = 0;
  activeTag = 0;
  defaultTag = inputDefaultTag;
  bestLeafIndex = 0xffffU;
  tagGroupFilter.Init(inputTagBias, inputTagMask, inputTagGroupIndex,
                      inputTagGroup);
}

void idMD6BestLeaf::Update() {
  const int count = std::min({leafList.Num(), tagList.Num(), 256});
  if (count > 0 && bestLeafIndex != 0xffffU && desiredTag == activeTag) {
    desiredTag = 0;
    return;
  }
  std::array<bool, 256> skip{};
  for (int index = filterList.Num() - 1; index >= 0; --index) {
    const idMD6Filter* filter = filterList[index];
    if (filter != nullptr && filter->type == NODE_TAG_FILTER)
      ApplyTagFilter(*static_cast<const idMD6TagFilter*>(filter), desiredTag,
                     defaultTag, tagList, skip);
  }
  ApplyTagFilter(tagGroupFilter, desiredTag, defaultTag, tagList, skip);

  std::vector<std::uint8_t> bestIndices;
  unsigned bestScore = 0;
  for (int index = 0; index < count; ++index) {
    if (skip[index])
      continue;
    const unsigned score = static_cast<unsigned>(
        100 * PopulationCount(~(tagList[index] ^ desiredTag)) +
        PopulationCount(~(defaultTag ^ tagList[index])));
    if (score > bestScore) {
      bestScore = score;
      bestIndices.clear();
    }
    if (score == bestScore)
      bestIndices.push_back(static_cast<std::uint8_t>(index));
  }
  activeTag = desiredTag;
  desiredTag = 0;
  if (bestIndices.empty()) {
    bestLeafIndex = 0xffffU;
    return;
  }
  g_animTreeRandomSeed =
      1664525U * g_animTreeRandomSeed + 1013904223U;
  const unsigned randomValue = (g_animTreeRandomSeed >> 10U) & 0x7fffU;
  bestLeafIndex = bestIndices[randomValue % bestIndices.size()];
  debugFilterLogicHelper = anim_debugFilterLogic.GetInteger() != 0 ? 1 : 0;
}

void idMD6FusionBranch::Init(
    std::uint8_t dimensions,
    const idList<idUserChannelIndex, 30>& channels,
    const idList<float, 5>& coordinates,
    const idList<idMD6Node*, 5>& animations,
    const idList<float, 5>& coordinate, md6WeightGroup_t weightGroup,
    std::uint8_t flags) {
  idMD6BlendBranch::Init(dimensions, channels, coordinates, animations,
                         coordinate, weightGroup, flags);
  op = idMD6Blend::BOP_FUSION;
  phaseToFrameLookupData.SetNum(animationList.Num() * 256, 0xffffU);
  for (int animationIndex = 0; animationIndex < animationList.Num();
       ++animationIndex) {
    idMD6Node* node = animationList[animationIndex];
    const idMD6Anim* animation =
        node != nullptr &&
                (node->type == NODE_LEAF_PLAY || node->type == NODE_LEAF_PAUSE)
            ? static_cast<idMD6Leaf*>(node)->anim
            : nullptr;
    const idMD6PhaseTrack* track = PhaseTrack(animation);
    if (animation == nullptr || track == nullptr || !track->IsValid(animation))
      continue;
    auto* lookup = &phaseToFrameLookupData[animationIndex * 256];
    std::array<unsigned, 256> entries{};
    for (int frame = 0; frame < track->NumFrames(); ++frame) {
      std::uint8_t phase = 0;
      track->GetPhase(frame, phase);
      lookup[phase] = static_cast<std::uint16_t>(
          (frame + entries[phase] * lookup[phase]) / (entries[phase] + 1));
      ++entries[phase];
    }
    int firstPhase = -1;
    int previousPhase = -1;
    for (int phase = 0; phase < 256; ++phase) {
      if (lookup[phase] == 0xffffU)
        continue;
      if (firstPhase < 0)
        firstPhase = phase;
      if (previousPhase >= 0 && phase - previousPhase > 1) {
        int endFrame = lookup[phase];
        const int previousFrame = lookup[previousPhase];
        if (std::abs(endFrame - previousFrame) >
            std::abs(endFrame + track->NumFrames() - previousFrame))
          endFrame += track->NumFrames();
        for (int fill = previousPhase + 1; fill < phase; ++fill) {
          int frame = previousFrame +
                      (endFrame - previousFrame) * (fill - previousPhase) /
                          (phase - previousPhase);
          while (frame >= track->NumFrames())
            frame -= track->NumFrames();
          lookup[fill] = static_cast<std::uint16_t>(frame);
        }
      }
      previousPhase = phase;
    }
    if (firstPhase >= 0 && previousPhase >= 0) {
      int endFrame = lookup[firstPhase];
      const int previousFrame = lookup[previousPhase];
      if (std::abs(endFrame - previousFrame) >
          std::abs(endFrame + track->NumFrames() - previousFrame))
        endFrame += track->NumFrames();
      const int range = firstPhase + 256 - previousPhase;
      for (int offset = 1; offset < range; ++offset) {
        int frame = previousFrame + (endFrame - previousFrame) * offset / range;
        while (frame >= track->NumFrames())
          frame -= track->NumFrames();
        lookup[(previousPhase + offset) & 255] =
            static_cast<std::uint16_t>(frame);
      }
    }
  }
}

void idMD6FusionBranch::Update(bool forceUpdate, int gameTime,
                               int ticksPerSec,
                               const idMD6Skel* skeleton) {
  if (DispatchMD6ParseTreeBranchUpdate(this, forceUpdate, gameTime,
                                       ticksPerSec, skeleton))
    return;
  std::vector<WeightedLeaf> leaves;
  GatherWeightedLeaves(this, 1.0f, leaves);
  if (leaves.empty()) {
    idMD6BlendBranch::Update(forceUpdate, userChannelIndices.Num() <= 0);
    return;
  }
  float weightedDuration = 0.0f;
  float largestWeight = -1.0f;
  idMD6Node* master = nullptr;
  for (const WeightedLeaf& leaf : leaves) {
    weightedDuration += LeafDuration(leaf.node, ticksPerSec) * leaf.weight;
    if (leaf.weight > largestWeight) {
      largestWeight = leaf.weight;
      master = leaf.node;
    }
  }
  if (master == nullptr || weightedDuration < kSmallestNormal) {
    idMD6BlendBranch::Update(forceUpdate, userChannelIndices.Num() <= 0);
    return;
  }
  if (master->type == NODE_LEAF_PLAY) {
    auto* play = static_cast<idMD6LeafPlay*>(master);
    play->SetRateScale(gameTime,
                       static_cast<float>(play->GetLength(ticksPerSec)) /
                           weightedDuration);
  }
  const auto* masterLeaf = static_cast<idMD6Leaf*>(master);
  const idMD6PhaseTrack* masterTrack = PhaseTrack(masterLeaf->anim);
  if (masterTrack == nullptr || !masterTrack->IsValid(masterLeaf->anim)) {
    idMD6BlendBranch::Update(forceUpdate, userChannelIndices.Num() <= 0);
    return;
  }
  const float masterFrame = master->type == NODE_LEAF_PLAY
                                ? static_cast<idMD6LeafPlay*>(master)
                                      ->GetFloatFrame(gameTime, ticksPerSec)
                                : static_cast<idMD6LeafPause*>(master)->frame;
  const int lowerFrame = static_cast<int>(std::floor(masterFrame));
  std::uint8_t lowerPhase = 0;
  std::uint8_t upperPhase = 0;
  masterTrack->GetPhase(lowerFrame, lowerPhase);
  masterTrack->GetPhase(
      (lowerFrame + 1) % std::max(1, static_cast<int>(masterTrack->NumFrames()) - 1),
      upperPhase);
  float upper = static_cast<float>(upperPhase);
  if (upper < lowerPhase)
    upper += 256.0f;
  const float phase = std::fmod(
      static_cast<float>(lowerPhase) +
          (upper - lowerPhase) * (masterFrame - std::floor(masterFrame)),
      255.0f);

  std::array<float, 272> userChannels{};
  for (int index = 0; index < animationList.Num(); ++index) {
    idMD6Node* node = animationList[index];
    if (node == nullptr || node == master ||
        (node->type != NODE_LEAF_PLAY && node->type != NODE_LEAF_PAUSE))
      continue;
    auto* leaf = static_cast<idMD6Leaf*>(node);
    const idMD6PhaseTrack* track = PhaseTrack(leaf->anim);
    if (track == nullptr || !track->IsValid(leaf->anim) ||
        phaseToFrameLookupData.Num() < (index + 1) * 256)
      continue;
    const auto* lookup = &phaseToFrameLookupData[index * 256];
    const int phaseIndex = static_cast<int>(phase) & 255;
    const int nextPhase = (phaseIndex + 1) & 255;
    const float fraction = phase - std::floor(phase);
    float frame = lookup[phaseIndex] +
                  (lookup[nextPhase] - lookup[phaseIndex]) * fraction;
    const float lastFrame = std::max(1.0f, track->NumFrames() - 1.0f);
    frame = std::fmod(frame + lastFrame, lastFrame);
    if (node->type == NODE_LEAF_PLAY) {
      auto* play = static_cast<idMD6LeafPlay*>(node);
      play->SetRateScale(gameTime,
                         static_cast<float>(play->GetLength(ticksPerSec)) /
                             weightedDuration);
      const float current = play->GetFloatFrame(gameTime, ticksPerSec);
      if (current > frame && current - frame > lastFrame * 0.5f)
        frame += lastFrame;
      frame = std::fmod(current + (frame - current) * 0.1f, lastFrame);
      play->SetFloatFrame(gameTime, ticksPerSec, frame);
    } else {
      static_cast<idMD6LeafPause*>(node)->frame = frame;
    }
    if (skeleton != nullptr && skeleton->data != nullptr &&
        userChannelIndices.Num() > 0 && leaf->anim != nullptr &&
        leaf->anim->animData != nullptr) {
      idMD6Blend::DecodeUserChannelsForAnimFrame(
          skeleton->data, leaf->anim->animData, frame, userChannels.data());
      for (int axis = 0; axis < numDimensions; ++axis) {
        const int channel = userChannelIndices[axis].value;
        if (channel >= 0 && channel < static_cast<int>(userChannels.size()))
          coordinateList[index * numDimensions + axis] =
              userChannels[static_cast<std::size_t>(channel)];
      }
    }
  }
  idMD6BlendBranch::Update(forceUpdate && master->type == NODE_LEAF_PLAY,
                           userChannelIndices.Num() <= 0);
}
