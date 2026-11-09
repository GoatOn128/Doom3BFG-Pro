#include "tungsten/game/ai/cover/aicover.h"

#include <cmath>

// DIA: idAICover::operator=
idAICover& idAICover::operator=(const idAICover& other) {
  if (this != &other) {
    aas = other.aas;
    ownerEntity = other.ownerEntity;
    spawnId = other.spawnId;
    coverIndex = other.coverIndex;
  }
  return *this;
}

// DIA: idAICover::Init
void idAICover::Init(const idEntity* entity, const idAAS2* newAAS,
                     std::int16_t index) {
  aas = newAAS;
  ownerEntity = entity;
  spawnId = entity != nullptr ? entity->GetEntityNumber() : 0x1FFF;
  coverIndex = index;
}

// DIA: idCoverActions::Init
void idCoverActions::Init(const aas2Cover_t* cover) {
  actionMask = 0;
  if (cover == nullptr) return;
  if ((cover->flags & 0x20u) != 0) actionMask |= 0x01u;
  if ((cover->flags & 0x140u) != 0) actionMask |= 0x02u;
  if ((cover->flags & 0x100u) != 0) actionMask |= 0x04u;
  if ((cover->flags & 0x280u) != 0) actionMask |= 0x08u;
  if ((cover->flags & 0x200u) != 0) actionMask |= 0x10u;
}

// DIA: idCoverBlocker::~idCoverBlocker
idCoverBlocker::~idCoverBlocker() {
  blockers.RemoveFast(this);
}

// DIA: idAICover::idAICover
idAICover::idAICover() = default;

// DIA: idAICover::idAICover
idAICover::idAICover(const idAAS2* newAAS, std::int16_t index)
    : aas(newAAS), coverIndex(index) {}

// DIA: idAICover::GetCoverPoint
aas2Cover_t* idAICover::GetCoverPoint() const {
  if (aas == nullptr || coverIndex < 0) return nullptr;
  if (ownerEntity == nullptr) return aas->GetCoverPoint(coverIndex);
  if (ownerEntity->IsRemoved()) return nullptr;
  const auto* provider = dynamic_cast<const idDynamicCoverProvider*>(ownerEntity);
  return provider != nullptr ? provider->GetDynamicCoverPoint(aas, coverIndex)
                             : nullptr;
}

// DIA: idAICover::Clear
void idAICover::Clear() {
  aas = nullptr;
  ownerEntity = nullptr;
  spawnId = 0x1FFF;
  coverIndex = -1;
}

// DIA: idAICover::IsValid
bool idAICover::IsValid() const {
  if (ownerEntity != nullptr) {
    const aas2Cover_t* cover = GetCoverPoint();
    return !ownerEntity->IsRemoved() && cover != nullptr &&
           (cover->flags & 0x2000u) == 0;
  }
  return aas != nullptr && coverIndex > 0;
}

// DIA: idAICover::GetCoverPosture
int idAICover::GetCoverPosture() const {
  const aas2Cover_t* cover = GetCoverPoint();
  return cover != nullptr ? 1 + ((cover->flags & 1u) != 0) : 1;
}

// DIA: idAICover::GetOrigin
const idVec3& idAICover::GetOrigin() const {
  const aas2Cover_t* cover = GetCoverPoint();
  static const idVec3 zero{};
  return cover != nullptr ? cover->origin : zero;
}

// DIA: idAICover::GetAxis
void idAICover::GetAxis(idMat3& result) const {
  const aas2Cover_t* cover = GetCoverPoint();
  if (cover == nullptr || aas == nullptr) {
    result[0] = {1.0F, 0.0F, 0.0F};
    result[1] = {0.0F, 1.0F, 0.0F};
    result[2] = {0.0F, 0.0F, 1.0F};
    return;
  }
  result[0] = cover->dir.LengthSqr() > 0.0F ? cover->dir : cover->normal;
  result[0].Normalize();
  result[2] = aas->GetGravityDirection();
  if (result[2].Normalize() == 0.0F) result[2] = {0.0F, 0.0F, -1.0F};
  result[1] = result[2].Cross(result[0]);
  if (result[1].Normalize() == 0.0F) result[1] = {0.0F, 1.0F, 0.0F};
}

// DIA: idAICover::Unreserve
void idAICover::Unreserve() {
  aas2Cover_t* cover = GetCoverPoint();
  if (cover != nullptr) cover->reservedBy = 0x1FFF;
}

// DIA: idAICover::GetCoverActions
void idAICover::GetCoverActions(idCoverActions* actions) const {
  if (actions != nullptr) actions->Init(GetCoverPoint());
}

// DIA: idAICover::IsPointInArc
bool idAICover::IsPointInArc(const idVec3& point, float arcCosine) const {
  const aas2Cover_t* cover = GetCoverPoint();
  const idVec3 origin = cover != nullptr ? cover->origin : idVec3{};
  const idVec3 direction =
      cover != nullptr
          ? (cover->dir.LengthSqr() > 0.0F ? cover->dir : cover->normal)
          : idVec3{};
  const float dx = point.x - origin.x;
  const float dy = point.y - origin.y;
  const float horizontalSquared = dx * dx + dy * dy;
  if (cover != nullptr &&
      (horizontalSquared < cover->minRange * cover->minRange ||
       (cover->maxRange != 0.0F &&
        horizontalSquared > cover->maxRange * cover->maxRange)))
    return false;
  if (horizontalSquared <= 0.0F) return false;
  const float inverseDistance = 1.0F / std::sqrt(horizontalSquared);
  const float facing = (dx * direction.x + dy * direction.y) * inverseDistance;
  const float dz = point.z - origin.z;
  return facing >= arcCosine && dz * dz <= horizontalSquared;
}

// DIA: idCoverBlocker::Spawn
void idCoverBlocker::Spawn() {
  for (int i = 0; i < blockers.Num(); ++i)
    if (blockers[i] == this) return;
  blockers.Append(this);
}
