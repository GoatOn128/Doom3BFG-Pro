#include "tungsten/game/ai/cover/formationcover.h"

// DIA: idFormationCover::idFormationCover
idFormationCover::idFormationCover() : idDynamicCover(1) {}

// DIA: idFormationCover::UpdateUsable
void idFormationCover::UpdateUsable(const idEntity* owner, const idAAS2* aas,
                                    unsigned int currentTime) {
  if (owner == nullptr || owner->IsRemoved()) {
    MarkAllUnusable();
    SetUpdated(false);
    return;
  }
  if (IsUpdated()) {
    InternalUpdateCurrent(owner, aas, static_cast<int>(currentTime));
    const coverInfo_t* info = GetCoverPoint(0);
    SetUpdated(info != nullptr && (info->cover.flags & 0x2000u) == 0);
  } else {
    MarkAllUnusable();
    InternalUpdateUsable(owner, aas, static_cast<int>(currentTime));
    const coverInfo_t* info = GetCoverPoint(0);
    SetUpdated(info != nullptr && (info->cover.flags & 0x2000u) == 0);
  }
}

// DIA: idFormationCoverMgr::AllocCover
idDynamicCover* idFormationCoverMgr::AllocCover(const idAAS2*) {
  return new idFormationCover();
}

// DIA: idFormationCover::InternalUpdateCurrent
void idFormationCover::InternalUpdateCurrent(const idEntity* owner,
                                             const idAAS2* aas,
                                             int currentTime) {
  (void)currentTime;
  coverInfo_t* info = GetCoverPoint(0);
  if (info == nullptr || owner == nullptr || owner->GetPhysics() == nullptr ||
      aas == nullptr) {
    MarkAllUnusable();
    return;
  }
  const idMat3& axis = owner->GetPhysics()->GetAxis();
  info->cover.origin = owner->GetPhysics()->GetOrigin() - axis[0] * 32.0F;
  info->cover.dir = -axis[0];
  info->cover.areaNum = aas->PointAreaNum(0, info->cover.origin);
  if (info->cover.areaNum == 0)
    info->cover.flags |= 0x2000u;
  else
    info->cover.flags &= ~0x2000u;
}

// DIA: idFormationCover::InternalUpdateUsable
void idFormationCover::InternalUpdateUsable(const idEntity* owner,
                                            const idAAS2* aas,
                                            int currentTime) {
  coverInfo_t* info = GetCoverPoint(0);
  if (info == nullptr || info->cover.usableTime > currentTime ||
      GetClipForAAS(aas) == nullptr)
    return;
  InternalUpdateCurrent(owner, aas, currentTime);
  if ((info->cover.flags & 0x2000u) == 0) {
    info->cover.flags |= 0x20u;
    info->contentsQuery = 0;
    info->translationQuery = 0;
  }
}
