#include "tungsten/game/ai/cover/dynamiccover.h"

namespace {

const idBounds kCover24{{{-12.0F, -12.0F, 0.0F}, {12.0F, 12.0F, 96.0F}}};
const idBounds kCover32{{{-16.0F, -16.0F, 0.0F}, {16.0F, 16.0F, 96.0F}}};
const idBounds kCover48{{{-24.0F, -24.0F, 0.0F}, {24.0F, 24.0F, 96.0F}}};
const idClipModel kClip24{kCover24, "_cover24"};
const idClipModel kClip32{kCover32, "_cover32"};
const idClipModel kClip48{kCover48, "_cover48"};

}  // namespace

// DIA: idInventoryCollection::GetInventoryCount
int idInventoryCollection::GetInventoryCount() const { return inventory.Num(); }

// DIA: idList<idAICover,5>::IsFull
template bool idList<idAICover, 5>::IsFull() const;

// DIA: idBounds::operator==
bool idBounds::operator==(const idBounds& other) const {
  return b[0] == other.b[0] && b[1] == other.b[1];
}

// DIA: idDynamicCover::GetClipForAAS
const idClipModel* idDynamicCover::GetClipForAAS(const idAAS2* aas) {
  if (aas == nullptr) return nullptr;
  const idBounds& bounds = aas->DefaultSearchBounds();
  if (bounds == kCover24) return &kClip24;
  if (bounds == kCover32) return &kClip32;
  if (bounds == kCover48) return &kClip48;
  return nullptr;
}

// DIA: idDynamicCover::GetPropNameForAAS
const char* idDynamicCover::GetPropNameForAAS(const idAAS2* aas) {
  const idClipModel* clip = GetClipForAAS(aas);
  return clip != nullptr ? clip->name : "_info";
}

// DIA: idDynamicCover::GetCoverPoint
idDynamicCover::coverInfo_t* idDynamicCover::GetCoverPoint(int index) {
  return index >= 0 && index < coverInfo.Num() ? &coverInfo[index] : nullptr;
}

const idDynamicCover::coverInfo_t* idDynamicCover::GetCoverPoint(int index) const {
  return index >= 0 && index < coverInfo.Num() ? &coverInfo[index] : nullptr;
}

// DIA: idDynamicCover::AppendUsableCover
bool idDynamicCover::AppendUsableCover(int coverIndex) {
  if (coverIndex < 0) return false;
  for (int i = 0; i < usableCover.Num(); ++i)
    if (usableCover[i] == coverIndex) return false;
  usableCover.Append(coverIndex);
  return true;
}

aas2Cover_t* idDynamicCover::GetDynamicCoverPoint(const idAAS2*,
                                                   std::int16_t index) const {
  const coverInfo_t* info = GetCoverPoint(index);
  return info != nullptr ? const_cast<aas2Cover_t*>(&info->cover) : nullptr;
}

// DIA: idDynamicCover::MarkAllUsable
void idDynamicCover::MarkAllUsable() {
  for (int i = 0; i < coverInfo.Num(); ++i) coverInfo[i].cover.flags &= ~0x2000u;
}

// DIA: idDynamicCover::GetReserver
const idEntity* idDynamicCover::GetReserver(int index) const {
  const coverInfo_t* info = GetCoverPoint(index);
  return info != nullptr && info->reserver != nullptr &&
                 !info->reserver->IsRemoved()
             ? info->reserver
             : nullptr;
}

// DIA: idDynamicCover::Reserve
void idDynamicCover::Reserve(int index, const idEntity* entity) {
  coverInfo_t* info = GetCoverPoint(index);
  if (info == nullptr) return;
  info->reserver = entity;
  info->cover.reservedBy =
      static_cast<std::uint16_t>(entity != nullptr ? entity->GetEntityNumber()
                                                   : 0x1FFF);
}

// DIA: idDynamicCover::Unreserve
void idDynamicCover::Unreserve(int index) {
  coverInfo_t* info = GetCoverPoint(index);
  if (info == nullptr) return;
  info->reserver = nullptr;
  info->cover.reservedBy = 0x1FFF;
}

// DIA: idDynamicCover::GetUsableTime
int idDynamicCover::GetUsableTime(int index) const {
  const coverInfo_t* info = GetCoverPoint(index);
  return info != nullptr ? info->cover.usableTime : -1;
}

// DIA: idDynamicCover::SetUsableTime
void idDynamicCover::SetUsableTime(int index, int time) {
  coverInfo_t* info = GetCoverPoint(index);
  if (info != nullptr) info->cover.usableTime = time;
}

// DIA: idDynamicCover::MarkAllUnusable
void idDynamicCover::MarkAllUnusable() {
  for (int i = 0; i < coverInfo.Num(); ++i) {
    coverInfo[i].cover.flags |= 0x2000u;
    coverInfo[i].cover.reservedBy = 0x1FFF;
    coverInfo[i].reserver = nullptr;
    coverInfo[i].contentsQuery = 0;
    coverInfo[i].translationQuery = 0;
  }
}

// DIA: idDynamicCover::~idDynamicCover
idDynamicCover::~idDynamicCover() { MarkAllUnusable(); }

// DIA: idList<idDynamicCover::coverInfo_t,5>::Resize
template bool idList<idDynamicCover::coverInfo_t, 5>::Resize(int newSize);

// DIA: idList<idDynamicCover::coverInfo_t,5>::Append
template int idList<idDynamicCover::coverInfo_t, 5>::Append(
    const idDynamicCover::coverInfo_t& value);

// DIA: idDynamicCover::idDynamicCover
idDynamicCover::idDynamicCover(int coverCount) {
  for (int i = 0; i < coverCount; ++i) {
    const int index = coverInfo.Append(coverInfo_t{});
    coverInfo_t& info = coverInfo[index];
    info.cover.origin = {};
    info.cover.normal = {};
    info.cover.dir = {};
    info.cover.areaNum = -1;
    info.cover.flags = 0;
    info.cover.reservedBy = 0x1FFF;
    info.cover.numTouching = 0;
    info.cover.firstTouching = 0;
    info.cover.durationSec = 0.0F;
    info.cover.minRange = 0.0F;
    info.cover.maxRange = 0.0F;
    info.cover.usableTime = -1;
    info.cover.valid = false;
    info.contentsQuery = 0;
    info.translationQuery = 0;
    info.reserver = nullptr;
  }
}

// DIA: `idDynamicCover::idDynamicCover'::`1'::dtor$0
// DIA: `idDynamicCover::idDynamicCover'::`1'::dtor$1

// DIA: idList<idAICover,5>::Alloc
template idAICover* idList<idAICover, 5>::Alloc();

// DIA: idDynamicCover::InternalAppendUsableCover
int idDynamicCover::InternalAppendUsableCover(
    const idEntity* owner, const idAAS2* aas,
    idList<idAICover, 5>* output) const {
  if (output == nullptr) return 0;
  int appended = 0;
  for (int i = 0; i < coverInfo.Num() && !output->IsFull(); ++i) {
    if ((coverInfo[i].cover.flags & 0x2000u) != 0) continue;
    output->Alloc()->Init(owner, aas, static_cast<std::int16_t>(i));
    ++appended;
  }
  return appended;
}

void idDynamicCover::UpdateUsable(const idEntity* owner, const idAAS2* aas,
                                  unsigned int currentTime) {
  (void)owner;
  (void)aas;
  updated = true;
  for (int i = 0; i < coverInfo.Num(); ++i)
    if ((coverInfo[i].cover.flags & 0x2000u) == 0)
      coverInfo[i].cover.usableTime = static_cast<int>(currentTime);
}

// DIA: `dynamic initializer for 'ai_debugDynamicCover''
idCVar ai_debugDynamicCover("ai_debugDynamicCover", "0", 2,
                            "show dynamic cover collision checks");

// DIA: `dynamic atexit destructor for 'ai_debugDynamicCover''
