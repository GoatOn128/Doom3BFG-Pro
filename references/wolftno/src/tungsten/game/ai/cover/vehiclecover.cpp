#include "tungsten/game/ai/cover/vehiclecover.h"

#include "tungsten/game/ai/cover/aicovermanager.h"

#include <algorithm>

namespace {

constexpr int kVehicleCoverCount = 8;
constexpr std::uint32_t kInvalidCover = 0x2000u;
constexpr std::uint32_t kVehicleCoverFlags[kVehicleCoverCount] = {
    0x20u, 0x20u, 0x40u, 0x40u, 0x80u, 0x80u, 0x100u, 0x100u};

idVec3 VehicleCoverDirection(const idMat3& axis, int index) {
  static constexpr float kDiagonal = 0.70710678F;
  static constexpr float kDirections[kVehicleCoverCount][2] = {
      {1.0F, 0.0F},        {kDiagonal, kDiagonal}, {0.0F, 1.0F},
      {-kDiagonal, kDiagonal}, {-1.0F, 0.0F},      {-kDiagonal, -kDiagonal},
      {0.0F, -1.0F},       {kDiagonal, -kDiagonal}};
  idVec3 direction = axis[0] * kDirections[index][0] +
                     axis[1] * kDirections[index][1];
  direction.z = 0.0F;
  if (direction.Normalize() == 0.0F)
    direction = {kDirections[index][0], kDirections[index][1], 0.0F};
  return direction;
}

idVec3 VehicleCoverOrigin(const idPhysics& ownerPhysics,
                          const idVec3& direction) {
  const idBounds& bounds = ownerPhysics.GetAbsBounds();
  const idVec3 center = (bounds[0] + bounds[1]) * 0.5F;
  const float halfX = (bounds[1].x - bounds[0].x) * 0.5F;
  const float halfY = (bounds[1].y - bounds[0].y) * 0.5F;
  const float clearance = std::max(halfX, halfY) + 16.0F;
  idVec3 origin = center + direction * clearance;
  origin.z = bounds[0].z;
  return origin;
}

}  // namespace

// DIA: idVehicleCover::idVehicleCover
idVehicleCover::idVehicleCover() : idDynamicCover(kVehicleCoverCount) {}

// DIA: idVehicleCoverMgr::FreeCover
void idVehicleCoverMgr::FreeCover(idDynamicCover* cover) { delete cover; }

// DIA: idVehicleCover::`scalar deleting destructor'

// DIA: idVehicleCover::UpdateUsable
void idVehicleCover::UpdateUsable(const idEntity* owner, const idAAS2* aas,
                                  unsigned int currentTime) {
  if (owner == nullptr || owner->IsRemoved() ||
      (ai_forceVehicleCoverFlag.GetInteger() == 0 &&
       !owner->ProvidesDynamicCover()))
    return;

  const idPhysics* ownerPhysics = owner->GetPhysics();
  if (ownerPhysics == nullptr ||
      owner->GetLinearVelocity().LengthSqr() > 0.001F) {
    MarkAllUnusable();
    SetUpdated(false);
    return;
  }

  if (IsUpdated()) {
    InternalUpdateCurrent(owner, aas, static_cast<int>(currentTime));
  } else {
    MarkAllUnusable();
    InternalUpdateUsable(owner, aas, static_cast<int>(currentTime));
    SetUpdated(true);
  }
}

// DIA: idVehicleCoverMgr::AllocCover
idDynamicCover* idVehicleCoverMgr::AllocCover(const idAAS2*) {
  return new idVehicleCover();
}

// DIA: idVehicleCover::InternalUpdateCurrent
void idVehicleCover::InternalUpdateCurrent(const idEntity* owner,
                                           const idAAS2* aas,
                                           int currentTime) {
  const idPhysics* ownerPhysics =
      owner != nullptr ? owner->GetPhysics() : nullptr;
  if (ownerPhysics == nullptr || aas == nullptr ||
      GetClipForAAS(aas) == nullptr) {
    MarkAllUnusable();
    return;
  }

  for (int i = 0; i < GetNumCover(); ++i) {
    coverInfo_t* info = GetCoverPoint(i);
    if (info == nullptr) continue;
    info->contentsQuery = 0;
    info->translationQuery = 0;
    if (info->cover.usableTime > currentTime) {
      info->cover.flags |= kInvalidCover;
      continue;
    }

    const idVec3 direction = VehicleCoverDirection(ownerPhysics->GetAxis(), i);
    idVec3 origin = VehicleCoverOrigin(*ownerPhysics, direction);
    const idBounds searchBounds = aas->DefaultSearchBounds();
    const std::uint16_t area = aas->PointReachableAreaNum(
        0, origin, searchBounds, 2, 1);
    if (area == 0) {
      info->cover.areaNum = 0;
      info->cover.flags |= kInvalidCover;
      info->cover.valid = false;
      continue;
    }
    aas->PushPointIntoAreaNum(area, origin);
    info->cover.origin = origin;
    info->cover.dir = direction;
    info->cover.normal = -direction;
    info->cover.areaNum = area;
    info->cover.flags = static_cast<std::uint16_t>(kVehicleCoverFlags[i]);
    info->cover.valid = true;
  }
}

// DIA: idVehicleCover::InternalUpdateUsable
void idVehicleCover::InternalUpdateUsable(const idEntity* owner,
                                          const idAAS2* aas,
                                          int currentTime) {
  InternalUpdateCurrent(owner, aas, currentTime);
}
