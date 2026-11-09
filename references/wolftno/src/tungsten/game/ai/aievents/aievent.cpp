#include "tungsten/game/ai/aievents/aievent.h"

#include <algorithm>

idDeclAiEvent::declInfo_t idDeclAiEvent::resourceList;
int idDeclAiEvent::loadedCount = 0;
int idAIEvent::numEvents = 0;

// DIA: idTrajectory::~idTrajectory
idTrajectory::~idTrajectory() = default;

// DIA: idAIEvent::GetIntensity
float idAIEvent::GetIntensity(const idEntity* entity) const {
  return InternalGetIntensity(entity);
}

// DIA: idDeclAiEvent::idDeclAiEvent
idDeclAiEvent::idDeclAiEvent() {
  bounds.b[0] = {1.0e30F, 1.0e30F, 1.0e30F};
  bounds.b[1] = {-1.0e30F, -1.0e30F, -1.0e30F};
}

// DIA: idDeclAiEvent::GetDeclInfo
idDeclAiEvent::declInfo_t* idDeclAiEvent::GetDeclInfo() {
  return &resourceList;
}

// DIA: idAIEvent::GetDuration
int idAIEvent::GetDuration() const {
  return eventDecl != nullptr ? eventDecl->duration : 0;
}

// DIA: idAIEvent::GetPriority
int idAIEvent::GetPriority() const {
  return eventDecl != nullptr ? eventDecl->priority : 0;
}

// DIA: idAIEvent::GetInstigatorStimulus
int idAIEvent::GetInstigatorStimulus() const {
  return eventDecl != nullptr ? eventDecl->instigatorStimulus : 0;
}

// DIA: idAIEvent::GetEventClass
idAIEvent::aiEventClass_t idAIEvent::GetEventClass() const {
  return static_cast<aiEventClass_t>(eventDecl != nullptr
                                         ? eventDecl->eventClass
                                         : 0);
}

// DIA: idAIEvent::UpdateAttached
idAIEvent::aiEventUpdateResult_t idAIEvent::UpdateAttached(int currentTime) {
  if (originator == nullptr || originator->IsRemoved()) return AIUPDATE_REMOVE;
  return InternalUpdateAttached(currentTime);
}

// DIA: idAIEvent::Update
idAIEvent::aiEventUpdateResult_t idAIEvent::Update(int currentTime) {
  if (eventDecl != nullptr && eventDecl->attached &&
      UpdateAttached(currentTime) == AIUPDATE_REMOVE)
    return AIUPDATE_REMOVE;
  return InternalUpdate(currentTime);
}

// DIA: idDeclAiEvent::LoadAllDecls
void idDeclAiEvent::LoadAllDecls() {
  loadedCount = 0;
  for (int i = 0; i < resourceList.declarations.Num(); ++i) {
    idDeclAiEvent* declaration = resourceList.declarations[i];
    if (declaration == nullptr) continue;
    declaration->loaded = true;
    ++loadedCount;
  }
}

void idDeclAiEvent::Register(idDeclAiEvent* declaration) {
  if (declaration == nullptr) return;
  for (int i = 0; i < resourceList.declarations.Num(); ++i)
    if (resourceList.declarations[i] == declaration) return;
  resourceList.declarations.Append(declaration);
}

int idDeclAiEvent::GetLoadedCount() { return loadedCount; }

// DIA: idAIEvent::idAIEvent
idAIEvent::idAIEvent() : handle(numEvents++) {}

// DIA: idAIEvent::Init
void idAIEvent::Init(const idDeclAiEvent* declaration,
                     const idEntity* eventOriginator,
                     const idEntity* eventInstigator,
                     const idVec3& eventOrigin) {
  eventDecl = declaration;
  originator = eventOriginator;
  instigator = eventInstigator;
  origin = eventOrigin;
  stimulusOrigin = eventOrigin;
  startTime = -1;
}

// DIA: idAIEvent::InternalUpdateAttached
idAIEvent::aiEventUpdateResult_t idAIEvent::InternalUpdateAttached(
    int currentTime) {
  (void)currentTime;
  if (originator == nullptr || originator->GetPhysics() == nullptr)
    return AIUPDATE_REMOVE;
  origin = originator->GetPhysics()->GetOrigin();
  stimulusOrigin = origin;
  return AIUPDATE_KEEP;
}

float idAIEvent::InternalGetIntensity(const idEntity* entity) const {
  if (!enabled || eventDecl == nullptr) return 0.0F;
  float result = eventDecl->volume * eventDecl->intensityScale;
  if (entity != nullptr && entity->GetPhysics() != nullptr &&
      eventDecl->distance > 0.0F) {
    const float distance = (entity->GetPhysics()->GetOrigin() - origin).Length();
    result *= std::clamp(1.0F - distance / eventDecl->distance, 0.0F, 1.0F);
  }
  return std::max(0.0F, result);
}

idAIEvent::aiEventUpdateResult_t idAIEvent::InternalUpdate(int currentTime) {
  if (!enabled) return AIUPDATE_REMOVE;
  if (startTime < 0) startTime = currentTime;
  return GetDuration() > 0 && currentTime - startTime >= GetDuration()
             ? AIUPDATE_REMOVE
             : AIUPDATE_KEEP;
}
