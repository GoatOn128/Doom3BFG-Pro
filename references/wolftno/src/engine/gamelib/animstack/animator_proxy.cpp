#define WOLFTNO_RETAIL_IDMEM_SINGLETON_ABI
#include "engine/gamelib/animstack/animator_proxy.h"
#undef WOLFTNO_RETAIL_IDMEM_SINGLETON_ABI

idAnimator_Proxy::~idAnimator_Proxy() = default;

const idMD6Branch* idAnimator_Proxy::InternalGetMergeBranch() const {
  return &mergeBranch;
}

idMD6Branch* idAnimator_Proxy::InternalGetMergeBranch() {
  return &mergeBranch;
}

idAnimator_Proxy::idAnimator_Proxy() {
  mergeBranch.left = nullptr;
  mergeBranch.right = nullptr;
  *reinterpret_cast<std::uint64_t*>(&mergeBranch.currentAlpha) = 0;
  *reinterpret_cast<std::uint64_t*>(&mergeBranch.alphaRate) = 0;
  mergeBranch.type = idMD6Node::NODE_BRANCH;
  *reinterpret_cast<std::uint64_t*>(&mergeBranch.leftTimeOverride) =
      0xffffffffffffffffull;
  mergeBranch.filterGroup = MD6_WEIGHTGROUP_MAX;
  mergeBranch.op = 11;
  mergeBranch.originBlend = 0;
}

void idAnimator_Proxy::SetTree(idAnimator_AnimWeb* web) {
  mergeBranch.right = reinterpret_cast<idMD6Node*>(web);
}

bool idAnimator_Proxy::InternalIsContributing() const {
  return mergeBranch.right != nullptr;
}
