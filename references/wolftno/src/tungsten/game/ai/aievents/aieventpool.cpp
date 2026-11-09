#include "tungsten/game/ai/aievents/aieventmanager.h"

#include <algorithm>

// DIA: idAIEventPool::AllocEvent
idAIEvent* idAIEventPool::AllocEvent() {
  idAIEvent* event = nullptr;
  if (freeEvents.Num() > 0) {
    event = freeEvents[freeEvents.Num() - 1];
    freeEvents.RemoveIndex(freeEvents.Num() - 1);
  } else {
    event = new idAIEvent();
    storage.Append(event);
  }
  ++allocatedCount;
  return event;
}

// DIA: idAIEventPool::Free
void idAIEventPool::Free() {
  for (int i = 0; i < storage.Num(); ++i) delete storage[i];
  storage.ClearFree();
  freeEvents.ClearFree();
  allocatedCount = 0;
}

// DIA: idAIEventPool::idAIEventPool
idAIEventPool::idAIEventPool() = default;

// DIA: idAIEventPool::idAIEventPool
idAIEventPool::idAIEventPool(int eventTypeValue) : eventType(eventTypeValue) {}

// DIA: idAIEventPool::~idAIEventPool
idAIEventPool::~idAIEventPool() { Free(); }

// DIA: `idAIEventPool::~idAIEventPool'::`1'::dtor$0
// DIA: `idAIEventPool::~idAIEventPool'::`1'::dtor$1

// DIA: idAIEventPool::`scalar deleting destructor'

// DIA: idAIEventPool::AllocInitialPool
void idAIEventPool::AllocInitialPool() {
  while (storage.Num() < std::max(0, initialPoolSize)) {
    idAIEvent* event = new idAIEvent();
    storage.Append(event);
    freeEvents.Append(event);
  }
}

// DIA: idAIEventPool::FreeEvent
void idAIEventPool::FreeEvent(idAIEvent* event) {
  if (event == nullptr) return;
  bool owned = false;
  for (int i = 0; i < storage.Num(); ++i) owned |= storage[i] == event;
  if (!owned) return;
  for (int i = 0; i < freeEvents.Num(); ++i)
    if (freeEvents[i] == event) return;
  freeEvents.Append(event);
  allocatedCount = std::max(0, allocatedCount - 1);
}
