#include "tungsten/game/ai/aiinteraction.h"

#include <stdexcept>

idDeclAIInteraction::declInfo_t idDeclAIInteraction::resourceList{};

// DIA: idAIInteraction::idAIInteraction
idAIInteraction::idAIInteraction() = default;

// DIA: idAIInteraction::Spawn
void idAIInteraction::Spawn() {
  active = false;
  if (interactionDecl == nullptr) return;
  int previousTime = 0;
  for (int index = 0; index < interactionDecl->events.Num(); ++index) {
    const idDeclAIInteraction::eventData_t& event =
        interactionDecl->events[index];
    if (event.type == IEV_MAX) {
      throw std::invalid_argument("AI interaction event has invalid type");
    }
    if (event.timeMS < previousTime) {
      throw std::invalid_argument("AI interaction events are out of order");
    }
    previousTime = event.timeMS;
  }
}

// DIA: idAIInteraction::BeginInteraction
void idAIInteraction::BeginInteraction(idEntity* newActivator) {
  if (interactionDecl == nullptr) return;
  nextEvent = 0;
  startTime = currentTime;
  activator = newActivator;
  active = true;
  for (int index = 0; index < targets.Num(); ++index) {
    if (idAI2* ai = dynamic_cast<idAI2*>(targets[index])) {
      ai->SetCurrentInteraction(this);
    }
  }
}

// DIA: idAIInteraction::EndInteraction
void idAIInteraction::EndInteraction() {
  for (int index = 0; index < targets.Num(); ++index) {
    if (idAI2* ai = dynamic_cast<idAI2*>(targets[index])) {
      if (ai->GetCurrentInteraction() == this) {
        ai->SetCurrentInteraction(nullptr);
      }
    }
  }
  activator = nullptr;
  startTime = -1;
  nextEvent = -1;
  active = false;
}

// DIA: idAIInteraction::Think
void idAIInteraction::Think() {
  if (!active || interactionDecl == nullptr ||
      interactionDecl->events.Num() == 0) {
    active = false;
    return;
  }
  const int elapsed = currentTime - startTime;
  while (nextEvent >= 0 && nextEvent < interactionDecl->events.Num() &&
         interactionDecl->events[nextEvent].timeMS <= elapsed) {
    const idDeclAIInteraction::eventData_t& event =
        interactionDecl->events[nextEvent];
    if (event.type == IEV_ACTIONSCRIPT && event.actorIndex >= 0 &&
        event.actorIndex < targets.Num()) {
      if (idAI2* ai = dynamic_cast<idAI2*>(targets[event.actorIndex])) {
        if (!ai->IsDeadState()) {
          ai->StartInteractionAction(event.actionScript.data, this, activator,
                                     currentTime);
        }
      }
    }
    ++nextEvent;
  }
  if (nextEvent >= interactionDecl->events.Num()) EndInteraction();
}

// DIA: idAIInteraction::OnActivate
void idAIInteraction::OnActivate(idEntity* newActivator) {
  BeginInteraction(newActivator);
}

// DIA: idDeclAIInteraction::idDeclAIInteraction
idDeclAIInteraction::idDeclAIInteraction() = default;

// DIA: idDeclAIInteraction::GetDeclInfo
idDeclAIInteraction::declInfo_t* idDeclAIInteraction::GetDeclInfo() {
  return &resourceList;
}

// DIA: idDeclAIInteraction::`scalar deleting destructor'
idDeclAIInteraction::~idDeclAIInteraction() = default;
