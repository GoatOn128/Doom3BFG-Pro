#include "tungsten/game/ai/cover/dynamiccovermgr.h"

void idDynamicCoverMgr::FreeCover(idDynamicCover* cover) { delete cover; }

idDynamicCover* idDynamicCoverMgr::AllocCover(const idAAS2*) {
  return new idDynamicCover();
}

int idDynamicCoverMgr::FindAAS(const idAAS2* aas) const {
  for (int i = 0; i < aasList.Num(); ++i)
    if (aasList[i] == aas) return i;
  return -1;
}

// DIA: idDynamicCoverMgr::Shutdown
void idDynamicCoverMgr::Shutdown() {
  for (int i = 0; i < coverSets.Num(); ++i) FreeCover(coverSets[i]);
  coverSets.ClearFree();
  aasList.ClearFree();
  initialized = false;
}

// DIA: idDynamicCoverMgr::NumCoverPoints
int idDynamicCoverMgr::NumCoverPoints(const idAAS2* aas) const {
  const int index = FindAAS(aas);
  return index >= 0 && coverSets[index] != nullptr
             ? coverSets[index]->GetNumCover()
             : 0;
}

// DIA: idDynamicCoverMgr::GetCoverPoint
aas2Cover_t* idDynamicCoverMgr::GetCoverPoint(const idAAS2* aas,
                                              int coverIndex) const {
  const int index = FindAAS(aas);
  if (index < 0 || coverSets[index] == nullptr) return nullptr;
  idDynamicCover::coverInfo_t* info = coverSets[index]->GetCoverPoint(coverIndex);
  return info != nullptr ? &info->cover : nullptr;
}

// DIA: idDynamicCoverMgr::GetReserver
const idEntity* idDynamicCoverMgr::GetReserver(const idAAS2* aas,
                                                int coverIndex) const {
  const int index = FindAAS(aas);
  return index >= 0 && coverSets[index] != nullptr
             ? coverSets[index]->GetReserver(coverIndex)
             : nullptr;
}

// DIA: idDynamicCoverMgr::Reserve
void idDynamicCoverMgr::Reserve(const idAAS2* aas, int coverIndex,
                                const idEntity* entity) {
  const int index = FindAAS(aas);
  if (index >= 0 && coverSets[index] != nullptr)
    coverSets[index]->Reserve(coverIndex, entity);
}

// DIA: idDynamicCoverMgr::Unreserve
void idDynamicCoverMgr::Unreserve(const idAAS2* aas, int coverIndex) {
  const int index = FindAAS(aas);
  if (index >= 0 && coverSets[index] != nullptr)
    coverSets[index]->Unreserve(coverIndex);
}

// DIA: idDynamicCoverMgr::GetUsableTime
int idDynamicCoverMgr::GetUsableTime(const idAAS2* aas, int coverIndex) const {
  const int index = FindAAS(aas);
  return index >= 0 && coverSets[index] != nullptr
             ? coverSets[index]->GetUsableTime(coverIndex)
             : 0;
}

// DIA: idDynamicCoverMgr::SetUsableTime
void idDynamicCoverMgr::SetUsableTime(const idAAS2* aas, int coverIndex,
                                      int time) {
  const int index = FindAAS(aas);
  if (index >= 0 && coverSets[index] != nullptr)
    coverSets[index]->SetUsableTime(coverIndex, time);
}

// DIA: idDynamicCoverMgr::idDynamicCoverMgr
idDynamicCoverMgr::idDynamicCoverMgr() { managers.Append(this); }

// DIA: idDynamicCoverMgr::~idDynamicCoverMgr
idDynamicCoverMgr::~idDynamicCoverMgr() {
  Shutdown();
  managers.RemoveFast(this);
}

// DIA: idDynamicCoverMgr::`scalar deleting destructor'

// DIA: idDynamicCoverMgr::Init
void idDynamicCoverMgr::Init() {
  Shutdown();
  const int count = gameLocal != nullptr ? gameLocal->NumAAS() : 0;
  for (int i = 0; i < count; ++i) {
    idAAS2* aas = gameLocal->GetAAS(i);
    aasList.Append(aas);
    coverSets.Append(AllocCover(aas));
  }
  initialized = true;
}

// DIA: idDynamicCoverMgr::UpdateUsable
void idDynamicCoverMgr::UpdateUsable(const idEntity* owner,
                                     unsigned int currentTime) {
  if (!initialized) Init();
  for (int i = 0; i < coverSets.Num(); ++i)
    if (coverSets[i] != nullptr)
      coverSets[i]->UpdateUsable(owner, aasList[i], currentTime);
}

// DIA: idStaticList<idAICover,128>::~idStaticList<idAICover,128>
template idStaticList<idAICover, 128>::~idStaticList();

// DIA: idDynamicCoverMgr::ShowDynamicCover
void idDynamicCoverMgr::ShowDynamicCover(const idVec3* point, float distance) {
  lastShownCount = 0;
  if (point == nullptr || distance < 0.0F) return;
  const float distanceSquared = distance * distance;
  idStaticList<idAICover, 128> visibleCover;
  for (int m = 0; m < managers.Num(); ++m) {
    const idDynamicCoverMgr* manager = managers[m];
    if (manager == nullptr) continue;
    for (int a = 0; a < manager->coverSets.Num(); ++a) {
      const idDynamicCover* set = manager->coverSets[a];
      if (set == nullptr) continue;
      for (int c = 0; c < set->GetNumCover(); ++c) {
        const idDynamicCover::coverInfo_t* info = set->GetCoverPoint(c);
        if (info == nullptr ||
            (info->cover.origin - *point).LengthSqr() > distanceSquared)
          continue;
        idAICover* cover = visibleCover.Alloc();
        if (cover == nullptr) break;
        cover->Init(set, manager->aasList[a], static_cast<std::int16_t>(c));
        ++lastShownCount;
      }
    }
  }
}

// DIA: `idDynamicCoverMgr::ShowDynamicCover'::`1'::dtor$1
// DIA: `idDynamicCoverMgr::ShowDynamicCover'::`1'::dtor$0
// DIA: `idDynamicCoverMgr::ShowDynamicCover'::`1'::dtor$3

// DIA: `dynamic initializer for 'ai_showDynamicCover''
idCVar ai_showDynamicCover("ai_showDynamicCover", "0", 4,
                           "draw nearby dynamic cover");

// DIA: `dynamic atexit destructor for 'ai_showDynamicCover''
