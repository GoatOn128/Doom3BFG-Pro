#include "tungsten/game/ai/ai2_core_support.h"

#include <algorithm>
#include <cmath>
#include <string>

namespace {

int FocusTime() {
  return gameLocal != nullptr ? gameLocal->GetGameTimeMs() : 0;
}

int FocusFrameTime() {
  return gameLocal != nullptr ? gameLocal->GetGameMsPerFrame() : 16;
}

idVec3 FocusOrigin(const idEntity* entity) {
  return entity != nullptr && entity->GetPhysics() != nullptr
             ? entity->GetPhysics()->GetOrigin()
             : idVec3{};
}

}  // namespace

idCVar ai_debugAimRotationTracker(
    "ai_debugAimRotationTracker", "0", 0,
    "show debugging output for the aim focus rotation tracker");
idCVar ai_debugLookRotationTracker(
    "ai_debugLookRotationTracker", "0", 0,
    "show debugging output for the look focus rotation tracker");

// DIA: idFocusInfo::Shutdown
void idFocusInfo::Shutdown(idAnimatedEntity* owner) {
  (void)owner;
  delete animators[0];
  delete animators[1];
  animators[0] = nullptr;
  animators[1] = nullptr;
  initialized = false;
}

// DIA: idFocusInfo::SetConstraints
void idFocusInfo::SetConstraints(const idRotationTrackerRange* constraints) {
  if (constraints != nullptr && initialized) aimTracker.SetConstraints(*constraints);
}

// DIA: idFocusInfo::AddWeaponFireKickback
void idFocusInfo::AddWeaponFireKickback(const idAnimatedEntity* owner,
                                        const idWeapon* weapon) {
  if (owner == nullptr || weapon == nullptr || !initialized) return;
  const float scale = 0.25F * static_cast<float>(wanderingAim.kickCount + 1);
  wanderingAim.kickback += {0.0F, scale, scale * 0.5F};
  ++wanderingAim.kickCount;
}

// DIA: idFocusInfo::SetEnableTracking
void idFocusInfo::SetEnableTracking(const idAnimatedEntity* owner, bool enable) {
  (void)owner;
  enableTracking = enable;
}

// DIA: idFocusInfo::SetDeclTrackingParms
void idFocusInfo::SetDeclTrackingParms(const idDeclTrackingParms* decl) {
  if (decl != nullptr) {
    declTrackingParms = decl;
    SetTrackingParmsIndex(trackingParmsIndex);
  }
}

// DIA: idAnimator_TrackerBase::ValidAsParent
bool idAnimator_TrackerBase::ValidAsParent(int curTime, int msPerFrame) const {
  return IsContributing() && curTime - lastValidTime < 2 * msPerFrame;
}

// DIA: idAimTracker::~idAimTracker
idAimTracker::~idAimTracker() = default;

// DIA: idFocusInfo::~idFocusInfo
idFocusInfo::~idFocusInfo() { Shutdown(nullptr); }

// DIA: `idFocusInfo::~idFocusInfo'::`1'::dtor$0

// DIA: idFocusInfo::SetTempFocusPoint
void idFocusInfo::SetTempFocusPoint(idAnimatedEntity* owner,
                                    const idVec3& focusPointValue,
                                    aiFocus_t focusTypeValue) {
  (void)owner;
  if (!initialized) return;
  idealFocusPoint = focusPointValue;
  focusType = focusTypeValue;
}

// DIA: idFocusInfo::SetYawOnlyThreshold
void idFocusInfo::SetYawOnlyThreshold(float angle) {
  aimTracker.SetYawOnlyThreshold(angle);
}

// DIA: idFocusInfo::SetLockTracking
void idFocusInfo::SetLockTracking(bool lock) {
  lockTracking = lock;
  for (idAnimator_TrackerBase* animator : animators)
    if (animator != nullptr) animator->lockTracking = lock;
}

// DIA: idFocusInfo::SetTrackingParmsIndex
void idFocusInfo::SetTrackingParmsIndex(int index) {
  trackingParmsIndex = std::max(0, index);
  if (declTrackingParms != nullptr && declTrackingParms->parms.Num() > 0)
    trackingParmsIndex =
        std::min(trackingParmsIndex, declTrackingParms->parms.Num() - 1);
}

// DIA: idAimTracker::`scalar deleting destructor'

// DIA: idFocusInfo::Init
void idFocusInfo::Init(idAnimatedEntity* owner, aiFoci_t fociTypeValue) {
  Shutdown(owner);
  ++initCount;
  fociType = fociTypeValue;
  if (fociType != AIFOCI_AIM && fociType != AIFOCI_LOOK) return;
  animators[0] = new idAnimator_TrackerBase();
  animators[1] = new idAnimator_TrackerBase();
  curAnimatorIndex = 1;
  initialized = true;
  if (headTrackGroups.Num() == 0) {
    idHeadTrackGroup group;
    group.name = fociType == AIFOCI_AIM ? "torso_track" : "head_track";
    group.blendTime = owner != nullptr ? owner->GetHeadTrackBlendTime() : 500;
    headTrackGroups.Append(group);
  }
  SetCurHeadTrackGroup(owner, defaultHeadTrackGroup >= 0
                                 ? defaultHeadTrackGroup
                                 : 0);
}

// DIA: `idFocusInfo::Init'::`1'::dtor$0
// DIA: `idFocusInfo::Init'::`1'::dtor$1
// DIA: `idFocusInfo::Init'::`1'::dtor$2
// DIA: `idFocusInfo::Init'::`1'::dtor$3
// DIA: `idFocusInfo::Init'::`1'::dtor$4
// DIA: `idFocusInfo::Init'::`1'::dtor$5
// DIA: `idFocusInfo::Init'::`1'::dtor$6
// DIA: `idFocusInfo::Init'::`1'::dtor$7

// DIA: idFocusInfo::GetCurHeadTrackGroup
const idHeadTrackGroup* idFocusInfo::GetCurHeadTrackGroup(
    idAnimatedEntity* owner) const {
  (void)owner;
  int index = defaultHeadTrackGroup;
  if (initialized && animators[curAnimatorIndex] != nullptr)
    index = animators[curAnimatorIndex]->trackGroupIndex;
  return index >= 0 && index < headTrackGroups.Num()
             ? &headTrackGroups[index]
             : (headTrackGroups.Num() > 0 ? &headTrackGroups[0] : nullptr);
}

int idFocusInfo::ResolveBlendTime(idAnimatedEntity* owner, int requested) const {
  if (requested >= 0) return requested;
  const idHeadTrackGroup* group = GetCurHeadTrackGroup(owner);
  return group != nullptr ? group->blendTime : 500;
}

// DIA: idFocusInfo::SwapTrackers
void idFocusInfo::SwapTrackers(idAnimatedEntity* owner, int blendTime) {
  if (!initialized) return;
  const int resolved = ResolveBlendTime(owner, blendTime);
  const int now = FocusTime();
  animators[curAnimatorIndex]->End(now, resolved);
  curAnimatorIndex = curAnimatorIndex == 0 ? 1 : 0;
  animators[curAnimatorIndex]->Start(now, resolved);
}

// DIA: idFocusInfo::SetReferenceJointOverride
void idFocusInfo::SetReferenceJointOverride(idAnimatedEntity* owner,
                                            std::int16_t jointIndex) {
  if (!initialized ||
      animators[curAnimatorIndex]->referenceJointOverride == jointIndex)
    return;
  const int group = animators[curAnimatorIndex]->trackGroupIndex;
  SwapTrackers(owner, -1);
  animators[curAnimatorIndex]->trackGroupIndex = group;
  animators[curAnimatorIndex]->referenceJointOverride = jointIndex;
}

// DIA: idEntityPtr<idEntity const >::operator->
template <>
const idEntity* idEntityPtr<const idEntity>::operator->() const {
  return entity;
}

// DIA: idFocusInfo::idFocusInfo
idFocusInfo::idFocusInfo() = default;

// DIA: idFocusInfo::SetExplicitFocusPoint
void idFocusInfo::SetExplicitFocusPoint(idAnimatedEntity* owner,
                                        const idVec3& focusPointValue,
                                        bool keepInView, int duration,
                                        aiFocus_t focusTypeValue) {
  (void)owner;
  if (!initialized) return;
  idealFocusPoint = focusPointValue;
  hasExplicitFocusPoint = true;
  focusEntity = nullptr;
  focusType = focusTypeValue;
  keepFocusInView = keepInView;
  aimPoint = AIMPOINT_HEAD;
  focusTimeout = duration < 0 ? -1 : FocusTime() + duration;
}

// DIA: idFocusInfo::Clear
void idFocusInfo::Clear(idAnimatedEntity* owner) {
  (void)owner;
  hasExplicitFocusPoint = false;
  focusEntity = nullptr;
  keepFocusInView = false;
  aimPoint = AIMPOINT_EYELEVEL;
  focusTimeout = -1;
  focusType = AIFOCUS_NOTHING;
  offset = {};
}

// DIA: idFocusInfo::BlendOut
void idFocusInfo::BlendOut(idAnimatedEntity* owner, int blendTime) {
  if (initialized)
    animators[curAnimatorIndex]->End(FocusTime(),
                                     ResolveBlendTime(owner, blendTime));
}

// DIA: idFocusInfo::BlendIn
void idFocusInfo::BlendIn(idAnimatedEntity* owner, int blendTime) {
  if (initialized)
    animators[curAnimatorIndex]->Start(FocusTime(),
                                       ResolveBlendTime(owner, blendTime));
}

// DIA: idFocusInfo::ShowTrackingInfo
void idFocusInfo::ShowTrackingInfo(idAnimatedEntity* owner, const char* label,
                                   const idVec3& eyePosition,
                                   const idVec3& debugOffset) {
  (void)owner;
  (void)eyePosition;
  (void)debugOffset;
  std::string text = label != nullptr ? label : "focus";
  text += enableTracking ? ": tracking" : ": disabled";
  text += lockTracking ? " (locked)" : "";
  debugText = text.c_str();
}

// DIA: `idFocusInfo::ShowTrackingInfo'::`1'::dtor$0

// DIA: idFocusInfo::Update
void idFocusInfo::Update(idAnimatedEntity* owner,
                         idAnimator_TrackerBase* parentTracker) {
  if (!initialized || owner == nullptr) return;
  lastParentTracker = parentTracker;
  ++updateCount;
  const int now = FocusTime();
  if (focusTimeout >= 0 && now >= focusTimeout) Clear(owner);
  if (focusType == AIFOCUS_ENTITY && focusEntity != nullptr) {
    if (const auto* animated = dynamic_cast<const idAnimatedEntity*>(focusEntity))
      idealFocusPoint = animated->GetAimPoint();
    else
      idealFocusPoint = FocusOrigin(focusEntity);
  }
  const bool parentValid = parentTracker == nullptr ||
      parentTracker->ValidAsParent(now, FocusFrameTime());
  idAnimator_TrackerBase* animator = animators[curAnimatorIndex];
  if (!HasActiveFocus() || !enableTracking || !parentValid) {
    if (animator->IsContributing()) BlendOut(owner, -1);
    return;
  }
  if (!animator->IsContributing()) BlendIn(owner, -1);
  if (lockTracking) return;
  idVec3 destination = idealFocusPoint + offset + wanderingAim.kickback;
  idVec3 direction = destination - FocusOrigin(owner);
  if (direction.LengthSqr() < 0.000001F) direction = {1.0F, 0.0F, 0.0F};
  idFocusTrackingParms parms;
  if (declTrackingParms != nullptr && declTrackingParms->parms.Num() > 0)
    parms = declTrackingParms->parms[trackingParmsIndex];
  aimTracker.Update(aimTracker.GetTrackedDirection(), direction, parms.rate,
                    FocusFrameTime() * 0.001F, keepFocusInView, true);
  animator->trackedPoint = FocusOrigin(owner) +
      aimTracker.GetTrackedDirection() * std::max(32.0F, direction.Length());
  animator->lastValidTime = now;
  wanderingAim.kickback *= 0.75F;
}

// DIA: idFocusInfo::SetCurHeadTrackGroup
void idFocusInfo::SetCurHeadTrackGroup(idAnimatedEntity* owner,
                                       int trackGroupIndex) {
  if (!initialized || trackGroupIndex < 0 ||
      trackGroupIndex >= headTrackGroups.Num() ||
      animators[curAnimatorIndex]->trackGroupIndex == trackGroupIndex)
    return;
  SwapTrackers(owner, -1);
  animators[curAnimatorIndex]->referenceJointOverride = -1;
  animators[curAnimatorIndex]->trackGroupIndex = trackGroupIndex;
}

// DIA: idFocusInfo::SetFocusEntity
void idFocusInfo::SetFocusEntity(idAnimatedEntity* owner,
                                 const idEntity* entity, bool keepInView,
                                 aimPoint_t point, int duration) {
  if (!initialized) return;
  if (entity == owner) {
    Clear(owner);
    return;
  }
  hasExplicitFocusPoint = false;
  focusEntity = const_cast<idEntity*>(entity);
  keepFocusInView = keepInView;
  focusType = entity != nullptr ? AIFOCUS_ENTITY : AIFOCUS_NOTHING;
  aimPoint = point;
  if (const auto* animatedEntity =
          dynamic_cast<const idAnimatedEntity*>(entity)) {
    idealFocusPoint = animatedEntity->GetAimPoint();
  }
  focusTimeout = duration < 0 ? -1 : FocusTime() + duration;
}

bool idFocusInfo::HasActiveFocus() const {
  return focusType != AIFOCUS_NOTHING &&
         (hasExplicitFocusPoint || focusEntity != nullptr ||
          idealFocusPoint.LengthSqr() > 0.0F);
}

// Header-emitted focus accessor used by AI2.
bool idFocusInfo::HasFocus() {
  return focusEntity != nullptr || hasExplicitFocusPoint;
}

// DIA: `dynamic initializer for 'ai_debugAimRotationTracker''
// DIA: `dynamic initializer for 'ai_debugLookRotationTracker''
// DIA: `dynamic atexit destructor for 'ai_debugAimRotationTracker''
// DIA: `dynamic atexit destructor for 'ai_debugLookRotationTracker''
