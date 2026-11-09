#pragma once

#include "engine/decls/animwebs/animwebblendequation.h"
#include "engine/gamelib/animstack/animator_base.h"

#include <cstddef>
#include <cstdint>

class idAnimator_BlendEq : public idAnimator_Base {
public:
  class idNamedAnim {
  public:
    idNamedAnim()
        : anim(nullptr), wrapMode(idMD6Leaf::WRAP_LOOP), weightGroup(0),
          rate(1.0f) {}

    const idMD6Anim* anim;
    std::uint8_t wrapMode;
    std::uint8_t weightGroup;
    std::uint8_t padding[2];
    float rate;
  };

  idAnimator_BlendEq();
  ~idAnimator_BlendEq()
#if defined(WOLFTNO_RETAIL_SYNCED_DTOR_UNWIND_ABI)
      noexcept(false)
#endif
      override;

  void ParseBlendEquation(
      idAnimStack* animStack, int startTime,
      const idDeclAnimWebNode::idModelInfo& modelInfo);
  void SetNamedAnims(const idList<idNamedAnim, 5>& animations) {
    namedAnims = animations;
  }
  const idList<idNamedAnim, 5>& GetNamedAnims() const {
    return reinterpret_cast<const idList<idNamedAnim, 5>&>(namedAnims);
  }
  const char* GetBlendEquation() const { return blendEq.c_str(); }
  void SetBlendEquation(
      idAnimStack* animStack, int startTime, const char* equation,
      const idDeclAnimWebNode::idModelInfo& modelInfo);
  const idList<idAnimWebScalar, 5>& GetScalars() const {
    return reinterpret_cast<const idList<idAnimWebScalar, 5>&>(scalars);
  }
  void SetScalars(const idList<idAnimWebScalar, 5>& values) {
    scalars = values;
  }
  int NumScalars() const { return scalars.num; }
  __forceinline float* FindScalarPtr(const char* name) {
    for (int index = 0; index < scalars.num; ++index) {
      idAnimWebScalar& scalar = scalars.list[index];
      if (idStr::Icmp(scalar.name.str, name) != 0) {
        continue;
      }
      if (scalar.scalarIndex >= 0 && scalarMem != nullptr) {
        return &scalarMem->floats[scalar.scalarIndex];
      }
      return scalar.scalarPtr;
    }
    return nullptr;
  }
  __forceinline float FindScalarValue(const char* name) const {
    for (int index = 0; index < scalars.num; ++index) {
      const idAnimWebScalar& scalar = scalars.list[index];
      if (idStr::Icmp(scalar.name.str, name) == 0) {
        if (scalarMem == nullptr) {
          return 0.0f;
        }
#if defined(WOLFTNO_RETAIL_ANIMWEB_GETSCALAR_REFERENCE_ABI)
        return scalar.GetScalar(*scalarMem);
#else
        return scalar.GetScalar(scalarMem);
#endif
      }
    }
    return 0.0f;
  }
  const idList<idAnimWebScalarPair, 5>& GetScalarPairs() const {
    return reinterpret_cast<const idList<idAnimWebScalarPair, 5>&>(
        scalarPairs);
  }

protected:
  bool InternalInit(const idAnimatorParms_Base& parms) override;
  void InternalShutdown(idAnimStack* animStack) override;
  void InternalPreBlendTree(const idAnimStack* stack, int curTime,
                            int ticksPerSec) override;
  void InternalPostBlendTree(const idAnimStack*, int) override {}
  void InternalStart(
      const idAnimStack* stack, int curTime,
      idTypesafeNumber<int, gameTimeUnique_t> blendDurationTicks) override;
  void InternalEnd(
      const idAnimStack*, int,
      idTypesafeNumber<int, gameTimeUnique_t>) override {}
  void InternalBlend(
      const idAnimStack*, int, float,
      idTypesafeNumber<int, gameTimeUnique_t>) override {}
  bool InternalIsContributing() const override { return true; }
  const idMD6Branch* InternalGetMergeBranch() const override {
    return mergeBranch;
  }
  idMD6Branch* InternalGetMergeBranch() override { return mergeBranch; }

private:
  void Free(idMD6Allocator* allocator, idMD6Node* root);
  void CopyScalars(const idAnimStack* stack, int curTime,
                   std::uint8_t fieldFlags);

public:
  idStr blendEq;
  idMD6Branch* mergeBranch;
  idList<idAnimWebScalar, 33> scalars;
  idList<idAnimWebScalarPair, 33> scalarPairs;
  idScalarMemBlock* scalarMem;
  idList<idNamedAnim, 33> namedAnims;

  friend struct AnimatorBlendEqValidationAccess;
};

class idAnimatorParms_BlendEq : public idAnimatorParms_Base {
public:
  idAnimatorParms_BlendEq(
      idAnimStack* stack, const char* animatorName,
      idMD6Blend::blendOp_t operation,
      idMD6Blend::originBlend_t originOperation,
      md6WeightGroup_t animatorWeightGroup,
      md6WeightGroup_t animatorFilterGroup, float animatorAlpha,
      const char* equation, idScalarMemBlock* scalarMemory)
      : idAnimatorParms_Base(stack, animatorName, operation, originOperation,
                             animatorWeightGroup, animatorFilterGroup,
                             animatorAlpha),
        blendEq(equation), scalars(), scalarMem(scalarMemory), anims() {}

  idStr blendEq;
  idList<idAnimWebScalar, 5> scalars;
  idScalarMemBlock* scalarMem;
  idList<idAnimator_BlendEq::idNamedAnim, 5> anims;
};

void EnumerateTreeNodes_r(const idMD6Node* node,
                          idList<const idMD6Node*, 5>& nodes);
void FreeUnreferencedLeaves(
    idMD6Allocator* allocator, const idMD6Node* tree,
    idList<idAnimWebBlendEquation::existingNode_t, 5>& existingNodes);

template <>
bool idList<idAnimWebScalarPair, 33>::Resize(int newSize);
template <>
bool idList<idAnimWebBlendEquation::existingNode_t, 5>::Resize(int newSize);
template <>
bool idList<idPair<idStr, float>, 5>::Resize(int newSize);
template <>
idList<idPair<idStr, float>, 5>::AllocResult
idList<idPair<idStr, float>, 5>::Alloc();

static_assert(sizeof(idAnimator_BlendEq::idNamedAnim) == 0x10,
              "blend-equation named-animation ABI drift");
static_assert(offsetof(idAnimator_BlendEq::idNamedAnim, anim) == 0x00,
              "blend-equation named-animation pointer offset drift");
static_assert(offsetof(idAnimator_BlendEq::idNamedAnim, wrapMode) == 0x08,
              "blend-equation named-animation wrap offset drift");
static_assert(offsetof(idAnimator_BlendEq::idNamedAnim, weightGroup) == 0x09,
              "blend-equation named-animation weight offset drift");
static_assert(offsetof(idAnimator_BlendEq::idNamedAnim, rate) == 0x0C,
              "blend-equation named-animation rate offset drift");
static_assert(sizeof(idAnimatorParms_BlendEq) == 0xA8,
              "blend-equation animator-parameter ABI drift");
static_assert(offsetof(idAnimatorParms_BlendEq, blendEq) == 0x48,
              "blend-equation parameter string offset drift");
static_assert(offsetof(idAnimatorParms_BlendEq, scalars) == 0x70,
              "blend-equation parameter scalar-list offset drift");
static_assert(offsetof(idAnimatorParms_BlendEq, scalarMem) == 0x88,
              "blend-equation parameter scalar-memory offset drift");
static_assert(offsetof(idAnimatorParms_BlendEq, anims) == 0x90,
              "blend-equation parameter animation-list offset drift");
static_assert(sizeof(idAnimator_BlendEq) == 0xD0,
              "blend-equation animator ABI drift");
static_assert(offsetof(idAnimator_BlendEq, blendEq) == 0x50,
              "blend-equation string offset drift");
static_assert(offsetof(idAnimator_BlendEq, mergeBranch) == 0x78,
              "blend-equation merge-branch offset drift");
static_assert(offsetof(idAnimator_BlendEq, scalars) == 0x80,
              "blend-equation scalar-list offset drift");
static_assert(offsetof(idAnimator_BlendEq, scalarPairs) == 0x98,
              "blend-equation scalar-pair-list offset drift");
static_assert(offsetof(idAnimator_BlendEq, scalarMem) == 0xB0,
              "blend-equation scalar-memory offset drift");
static_assert(offsetof(idAnimator_BlendEq, namedAnims) == 0xB8,
              "blend-equation named-animation-list offset drift");
