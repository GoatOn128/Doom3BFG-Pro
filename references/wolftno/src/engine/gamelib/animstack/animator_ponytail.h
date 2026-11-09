#pragma once

#include "engine/decls/declholder.h"
#include "engine/gamelib/animstack/animator_base.h"
#include "engine/gamelib/animstack/coneconstraint.h"
#include "shared/idlib/containers/staticlist.h"

#include <cstddef>
#include <cstdint>

class idTreeAnimator;

struct jointTransform_t {
  idQuat msQuat;
  idVec3 msOrigin;
};

template <typename Type> class idSpring {
public:
  static const float K_MAX;

  __forceinline idSpring() {
    k = 1.0f;
    m = 1.0f;
    c = 2.0f;
    p0 = idVec3{0.0f, 0.0f, 0.0f};
    p1 = idVec3{0.0f, 0.0f, 0.0f};
    vel = idVec3{0.0f, 0.0f, 0.0f};
    maxSpeed = 0.0f;
    pMin = idVec3{0.0f, 0.0f, 0.0f};
    hasPMin = false;
    pMax = idVec3{0.0f, 0.0f, 0.0f};
    hasPMax = false;
  }

  void Update(float dt);
  void SetConstants(float springConstant, float damping);

  Type p0;
  Type p1;
  Type vel;
  float maxSpeed;
  Type pMin;
  Type pMax;
  bool hasPMax;
  bool hasPMin;
  bool fixDenormals;
  std::uint8_t recoveredPadding;
  float k;
  float c;
  float m;
  float restLength;
};

class idPonytailSegment {
public:
  idPonytailSegment() = default;
  __forceinline idPonytailSegment(
      const idVec3& localOriginValue,
      const idConeConstraint_Quat& constraint)
      : localOrigin(localOriginValue), localAxis(quat_identity),
        origin{0.0f, 0.0f, 0.0f}, coneConstraintQ(constraint), len(1.0f) {}

  idVec3 localOrigin;
  idQuat localAxis;
  idVec3 origin;
  idConeConstraint_Quat coneConstraintQ;
  float len;
};

class idAnimatorParms_Ponytail : public idAnimatorParms_Base {
public:
  idAnimatorParms_Ponytail(
      idAnimStack* stack, const char* animatorName,
      idMD6Blend::blendOp_t operation,
      idMD6Blend::originBlend_t originOperation,
      md6WeightGroup_t animatorWeightGroup,
      md6WeightGroup_t animatorFilterGroup, float animatorAlpha,
      const idDeclPonytail* ponytailDeclaration,
      idTreeAnimator* sourceAnimator)
      : idAnimatorParms_Base(stack, animatorName, operation, originOperation,
                             animatorWeightGroup, animatorFilterGroup,
                             animatorAlpha),
        declPonytail(ponytailDeclaration), parentAnimator(sourceAnimator) {}

  const idDeclPonytail* declPonytail;
  idTreeAnimator* parentAnimator;
};

class idAnimator_Ponytail : public idAnimator_Base {
public:
  idAnimator_Ponytail();
  ~idAnimator_Ponytail()
#if defined(WOLFTNO_RETAIL_SYNCED_DTOR_UNWIND_ABI)
      noexcept(false)
#endif
      override;

protected:
  bool InternalInit(const idAnimatorParms_Base& parms) override;
  void InternalShutdown(idAnimStack*) override {}
  void InternalPreBlendTree(const idAnimStack* stack, int curTime,
                            int ticksPerSec) override;
  void InternalPostBlendTree(const idAnimStack*, int) override {}
  void InternalStart(
      const idAnimStack*, int,
      idTypesafeNumber<int, gameTimeUnique_t>) override {}
  void InternalEnd(
      const idAnimStack*, int,
      idTypesafeNumber<int, gameTimeUnique_t>) override {}
  void InternalBlend(
      const idAnimStack*, int, float,
      idTypesafeNumber<int, gameTimeUnique_t>) override {}
  bool InternalIsContributing() const override;
  const idMD6Branch* InternalGetMergeBranch() const override;
  idMD6Branch* InternalGetMergeBranch() override;

public:
  const idDeclPonytail* declPonytail;
  idTreeAnimator* parentAnimator;
  idList<idJointIndex_t, 30> joints;
  idMD6Branch mergeBranch;
  idMD6LeafPause pauseLeaf;
  idMD6LeafPlay playLeaf;
  idMD6Branch lerpBranch;
  idList<idPonytailSegment, 30> segments;
  idSpring<idVec3> ptEndSpring;
  bool isInitialized;
  std::uint8_t recoveredTail[3];

  friend struct AnimatorPonytailValidationAccess;
};

template <>
bool idList<idPonytailSegment, 30>::Resize(int newSize);
template <>
bool idList<jointTransform_t, 5>::Resize(int newSize);

static_assert(sizeof(jointTransform_t) == 0x1c,
              "ponytail joint transform ABI drift");
static_assert(offsetof(jointTransform_t, msOrigin) == 0x10,
              "ponytail joint-transform origin offset drift");
static_assert(sizeof(idSpring<idVec3>) == 0x54,
              "ponytail spring ABI drift");
static_assert(offsetof(idSpring<idVec3>, maxSpeed) == 0x24,
              "ponytail spring maximum-speed offset drift");
static_assert(offsetof(idSpring<idVec3>, pMax) == 0x34,
              "ponytail spring maximum-position offset drift");
static_assert(offsetof(idSpring<idVec3>, hasPMax) == 0x40,
              "ponytail spring maximum flag offset drift");
static_assert(offsetof(idSpring<idVec3>, hasPMin) == 0x41,
              "ponytail spring minimum flag offset drift");
static_assert(offsetof(idSpring<idVec3>, fixDenormals) == 0x42,
              "ponytail spring denormal flag offset drift");
static_assert(offsetof(idSpring<idVec3>, k) == 0x44,
              "ponytail spring constant offset drift");
static_assert(offsetof(idSpring<idVec3>, restLength) == 0x50,
              "ponytail spring rest-length offset drift");
static_assert(sizeof(idPonytailSegment) == 0x58,
              "ponytail segment ABI drift");
static_assert(offsetof(idPonytailSegment, localAxis) == 0x0c,
              "ponytail segment local-axis offset drift");
static_assert(offsetof(idPonytailSegment, origin) == 0x1c,
              "ponytail segment origin offset drift");
static_assert(offsetof(idPonytailSegment, coneConstraintQ) == 0x28,
              "ponytail segment constraint offset drift");
static_assert(offsetof(idPonytailSegment, len) == 0x54,
              "ponytail segment length offset drift");
static_assert(sizeof(idAnimatorParms_Ponytail) == 0x58,
              "ponytail animator parameters ABI drift");
static_assert(offsetof(idAnimatorParms_Ponytail, declPonytail) == 0x48,
              "ponytail declaration parameter offset drift");
static_assert(offsetof(idAnimatorParms_Ponytail, parentAnimator) == 0x50,
              "ponytail parent parameter offset drift");
static_assert(offsetof(idAnimator_Ponytail, declPonytail) == 0x50,
              "ponytail declaration offset drift");
static_assert(offsetof(idAnimator_Ponytail, parentAnimator) == 0x58,
              "ponytail parent animator offset drift");
static_assert(offsetof(idAnimator_Ponytail, joints) == 0x60,
              "ponytail joint-list offset drift");
static_assert(offsetof(idAnimator_Ponytail, mergeBranch) == 0x78,
              "ponytail merge-branch offset drift");
static_assert(offsetof(idAnimator_Ponytail, pauseLeaf) == 0xb0,
              "ponytail pause-leaf offset drift");
static_assert(offsetof(idAnimator_Ponytail, playLeaf) == 0x120,
              "ponytail play-leaf offset drift");
static_assert(offsetof(idAnimator_Ponytail, lerpBranch) == 0x148,
              "ponytail lerp-branch offset drift");
static_assert(offsetof(idAnimator_Ponytail, segments) == 0x180,
              "ponytail segment-list offset drift");
static_assert(offsetof(idAnimator_Ponytail, ptEndSpring) == 0x198,
              "ponytail spring offset drift");
static_assert(offsetof(idAnimator_Ponytail, isInitialized) == 0x1ec,
              "ponytail initialization flag offset drift");
static_assert(sizeof(idAnimator_Ponytail) == 0x1f0,
              "ponytail animator ABI drift");
