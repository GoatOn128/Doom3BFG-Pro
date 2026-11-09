#include "tungsten/game/ai/ai2_core_support.h"

#include <unordered_map>

namespace {

struct actorWaterEffectsObservation_t {
  int count{0};
  int physId{-1};
  int bodyId{-1};
};

struct actorModelObservation_t {
  idRenderModel* model{nullptr};
};

struct actorPresentationSources_t {
  idAnimStack* animStack{nullptr};
  const idDeclFX* fxDeclaration{nullptr};
};

std::unordered_map<const idActor*, actorWaterEffectsObservation_t>
    actorWaterEffectsObservations;
std::unordered_map<const idActor*, actorModelObservation_t>
    actorModelObservations;
std::unordered_map<const idActor*, actorPresentationSources_t>
    actorPresentationSources;

}  // namespace

damageDirection_t idActor::GetDamageDirection(
    const idMat3& axis, const idVec3& direction,
    int numberOfDirections) {
  const float forward = axis[0] * direction;
  const float side = axis[1] * direction;
  if (numberOfDirections <= 4) {
    if (forward >= 0.70710677F) return DAMAGEDIR_BACK;
    if (forward <= -0.70710677F) return DAMAGEDIR_FRONT;
    if (side <= -0.70710677F) return DAMAGEDIR_LEFT;
    return DAMAGEDIR_RIGHT;
  }
  if (forward >= 0.92387950F) return DAMAGEDIR_BACK;
  if (forward <= -0.92387950F) return DAMAGEDIR_FRONT;
  if (side <= -0.92387950F) return DAMAGEDIR_LEFT;
  if (side >= 0.92387950F) return DAMAGEDIR_RIGHT;
  if (forward >= 0.0F)
    return side < 0.0F ? DAMAGEDIR_BACKLEFT : DAMAGEDIR_BACKRIGHT;
  return side < 0.0F ? DAMAGEDIR_FRONTLEFT : DAMAGEDIR_FRONTRIGHT;
}

// Header-emitted actor support used by the recovered AI translation units.
walkState_t idActor::GetWalkState() const { return walkState; }

void idActor::SetWalkState(walkState_t state) { walkState = state; }

void idActor::SetModel(idRenderModel* model) {
  actorModelObservations[this].model = model;
}

void idActor::UpdateAttachments() { ++actorUpdateAttachmentsCount; }

void idActor::UpdateEntityAttachmentPos() {
  ++actorUpdateEntityAttachmentPosCount;
}

idRenderModel* idActor::GetActorModelForTest() const {
  return actorModelObservations[this].model;
}

idAnimStack* idActor::GetAnimStack() {
  return actorPresentationSources[this].animStack;
}

const idDeclFX* idActor::GetFXDecl() const {
  return actorPresentationSources[this].fxDeclaration;
}

void idActor::SetAnimStackForTest(idAnimStack* value) {
  actorPresentationSources[this].animStack = value;
}

void idActor::SetFXDeclForTest(const idDeclFX* value) {
  actorPresentationSources[this].fxDeclaration = value;
}

// DIA: idActor::Teleport
void idActor::Teleport(const idVec3& origin, const idAngles& angles) {
  lastTeleportOrigin = origin;
  lastTeleportAngles = angles;
  ++teleportCount;
}

void idActor::ApplyWaterEffects(int physId, int bodyId) {
  actorWaterEffectsObservation_t& observation =
      actorWaterEffectsObservations[this];
  ++observation.count;
  observation.physId = physId;
  observation.bodyId = bodyId;
}

int idActor::GetActorWaterEffectsCountForTest() const {
  return actorWaterEffectsObservations[this].count;
}

int idActor::GetActorWaterEffectsPhysIdForTest() const {
  return actorWaterEffectsObservations[this].physId;
}

int idActor::GetActorWaterEffectsBodyIdForTest() const {
  return actorWaterEffectsObservations[this].bodyId;
}

idRecoveredValue idActor::SaveDamageGroup(...) { return {}; }

idDamageGroup* idActor::GetDamageGroupFromJoint(
    const idIndex<short, invalidJointIndex_t> joint) {
  for (std::size_t index = 0; index < damageGroupJointIndices.size(); ++index) {
    if (damageGroupJointIndices[index] == joint.value) {
      return GetDamageGroupFromName(damageGroupJointNames[index].c_str());
    }
  }
  return nullptr;
}

idDamageGroup* idActor::GetDamageGroupFromName(
    const char* damageGroupName) {
  if (damageGroupName == nullptr) return nullptr;
  for (idDamageGroup* damageGroup : registeredDamageGroups) {
    if (damageGroup != nullptr &&
        idStr::Icmp(damageGroup->groupName.data, damageGroupName) == 0) {
      return damageGroup;
    }
  }
  return nullptr;
}

void idActor::RegisterDamageGroupForActor(idDamageGroup* damageGroup) {
  if (damageGroup != nullptr) registeredDamageGroups.push_back(damageGroup);
}

void idActor::ClearDamageGroupsForActor() {
  registeredDamageGroups.clear();
}

void idActor::SetDamageGroupForJointForTest(
    const short joint, const char* damageGroupName) {
  for (std::size_t index = 0; index < damageGroupJointIndices.size(); ++index) {
    if (damageGroupJointIndices[index] == joint) {
      damageGroupJointNames[index] =
          damageGroupName != nullptr ? damageGroupName : "";
      return;
    }
  }
  damageGroupJointIndices.push_back(joint);
  damageGroupJointNames.emplace_back(
      damageGroupName != nullptr ? damageGroupName : "");
}

void idActor::SetLastDamageInfo(
    const idEntity* inflictor,
    const idDeclDamage* damageDefinition) {
  lastDamageInflictor = inflictor;
  lastDamageDefinition = damageDefinition;
}

const idAnimator_AF* idActor::GetAF() const {
  return &articulatedFigureProperties;
}

// DIA: idActor::GetEyeOffset
const idVec3 idActor::GetEyeOffset() const {
  return eyeOffset;
}

bool idActor::ExpandBoundsByJoints(idBounds& bounds,
                                   const int jointGroup) const {
  lastJointBoundsGroup = jointGroup;
  if (!jointBoundsAvailable) return false;
  bounds = jointBounds;
  return true;
}

// DIA: idActor::GetViewTransform
void idActor::GetViewTransform(idVec3& origin, idMat3& axis) const {
  idPhysics* const gravityPhysics = GetPhysics();
  idPhysics* const originPhysics = GetPhysics();
  const float eyeHeightAlongGravity = -GetEyeOffset().z;
  const idVec3& gravityNormal = gravityPhysics->GetGravityNormal();
  const idVec3& physicsOrigin = originPhysics->GetOrigin();
  origin = physicsOrigin + gravityNormal * eyeHeightAlongGravity;
  axis = *GetPresentableAxis();
}

// DIA: idActor::GetEyePos
void idActor::GetEyePos(idVec3* eyePosition) const {
  const idVec3 origin =
      GetPhysics() != nullptr ? GetPhysics()->GetOrigin() : idVec3{};
  *eyePosition = origin + GetEyeOffset();
}

void idActor::GetLastEyePos(idVec3* eyePosition) const {
  *eyePosition = lastEyePosition;
}

void idActor::GetEyeAxis(idMat3* outputEyeAxis) const {
  *outputEyeAxis = eyeAxis;
}

void idActor::SetEyeAxis(const idMat3* inputEyeAxis) {
  eyeAxis = *inputEyeAxis;
}

const idInventoryCollection* idActor::GetInventory() const {
  return &inventory;
}

idAttachmentCollection* idActor::GetAttachments() { return &attachments; }

const idAttachmentCollection* idActor::GetAttachments() const {
  return &attachments;
}

const idList<idEntityPtr<idEntity>, 5>*
idActor::GetTriggerTouchList() const {
  return &touching;
}

idStrId idActor::GetDisplayName() const { return displayName; }

int idActor::GetPosture() const { return posture; }

leanState_t idActor::GetLeanState() const { return leanState; }

void idActor::SetLeanState(leanState_t newLeanState) {
  leanState = newLeanState;
}

weaponReadyState_t idActor::GetWeaponReadyState() const {
  return weaponReadyState;
}

void idActor::SetWeaponReadyState(weaponReadyState_t state) {
  weaponReadyState = state;
}
