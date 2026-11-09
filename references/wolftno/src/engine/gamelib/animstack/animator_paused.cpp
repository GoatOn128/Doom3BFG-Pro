#define WOLFTNO_RETAIL_IDMEM_SINGLETON_ABI
#include "engine/gamelib/animstack/animator_paused.h"
#undef WOLFTNO_RETAIL_IDMEM_SINGLETON_ABI

#include <cstdint>

idAnimator_Paused::~idAnimator_Paused() = default;

const idMD6Anim* idAnimator_Paused::GetAnim() const { return anim; }

void idAnimator_Paused::InternalShutdown(idAnimStack*) { anim = nullptr; }

const idMD6Branch* idAnimator_Paused::InternalGetMergeBranch() const {
  return &mergeBranch;
}

idMD6Branch* idAnimator_Paused::InternalGetMergeBranch() {
  return &mergeBranch;
}

void idAnimator_Paused::SetAnim(const idMD6Anim* animation) {
  anim = animation;
  if (animation != nullptr) {
    leaf.Init(animation, leaf.frame, idMD6Leaf::WRAP_CLAMP, weightGroup);
    leaf.anim = anim;
  } else {
    leaf.anim = nullptr;
  }
}

void idAnimator_Paused::SetFrame(float frame) { leaf.frame = frame; }

float idAnimator_Paused::GetFrame() const { return leaf.frame; }

void idAnimator_Paused::SetNormalizedFrame(float normalizedFrame) {
  leaf.frame =
      static_cast<float>(leaf.anim->animData->numFrames - 1) *
      normalizedFrame;
}

bool idAnimator_Paused::InternalInit(const idAnimatorParms_Base& parms) {
  const auto& pauseParms =
      static_cast<const idAnimatorParms_Pause&>(parms);
  mergeBranch.right = &leaf;
  anim = pauseParms.anim;
  if (pauseParms.anim != nullptr) {
    leaf.Init(pauseParms.anim, leaf.frame, idMD6Leaf::WRAP_CLAMP,
              weightGroup);
    leaf.anim = anim;
  } else {
    leaf.anim = nullptr;
  }
  // Retail uses COMISS(normalized, 0) followed by JB, so unordered values
  // select the raw start frame along with ordinary negative values.
  if (!(pauseParms.normalizedStartFrame >= 0.0f)) {
    leaf.frame = pauseParms.startFrame;
  } else {
    leaf.frame =
        static_cast<float>(leaf.anim->animData->numFrames - 1) *
        pauseParms.normalizedStartFrame;
  }
  return true;
}

bool idAnimator_Paused::InternalIsContributing() const {
  return leaf.anim != nullptr;
}

idAnimator_Paused::idAnimator_Paused() : leaf() {
  mergeBranch.type = idMD6Node::NODE_BRANCH;
  mergeBranch.left = nullptr;
  mergeBranch.right = nullptr;
  *reinterpret_cast<std::uint64_t*>(&mergeBranch.leftTimeOverride) =
      UINT64_MAX;
  mergeBranch.filterGroup = MD6_WEIGHTGROUP_MAX;
  mergeBranch.op = idMD6Blend::BOP_MAX;
  mergeBranch.originBlend = idMD6Blend::ORIGINBLEND_BRANCH;
  *reinterpret_cast<std::uint64_t*>(&mergeBranch.currentAlpha) = 0;
  *reinterpret_cast<std::uint64_t*>(&mergeBranch.alphaRate) = 0;
  anim = nullptr;
}
