#include "tungsten/game/ai/aievents/aieventmanager.h"

idCVar aievents_show("aievents_show", "0", 0, "show active AI events");
idCVar aievents_print("aievents_print", "0", 0, "print AI event changes");

// DIA: idAIEventManager::GetEventType
int idAIEventManager::GetEventType(const idDeclAiEvent* declaration) {
  return declaration != nullptr ? declaration->eventClass : -1;
}

// DIA: idSort<idAIEventPool * __ptr64>::`scalar deleting destructor'
template <typename T>
idSort<T>::~idSort() = default;
template class idSort<idAIEventPool*>;

// DIA: idAIEventManager::GetEvent
idAIEvent* idAIEventManager::GetEvent(int handle) const {
  for (int i = 0; i < events.Num(); ++i)
    if (events[i] != nullptr && events[i]->GetHandle() == handle) return events[i];
  return nullptr;
}

// DIA: idSort_AIEventPoolsByNumAllocated::~idSort_AIEventPoolsByNumAllocated
idSort_AIEventPoolsByNumAllocated::~idSort_AIEventPoolsByNumAllocated() = default;

// DIA: idAIEventManager::RemoveEvent
void idAIEventManager::RemoveEvent(int index) {
  if (index < 0 || index >= events.Num()) return;
  idAIEvent* event = events[index];
  events.RemoveIndexFast(index);
  const int type = GetEventType(event != nullptr ? event->GetDeclaration() : nullptr);
  for (int i = 0; i < pools.Num(); ++i)
    if (pools[i] != nullptr && pools[i]->eventType == type) {
      pools[i]->FreeEvent(event);
      return;
    }
}

// DIA: idAIEventManager::RemoveEvent
void idAIEventManager::RemoveEventByHandle(int handle) {
  for (int i = events.Num() - 1; i >= 0; --i)
    if (events[i] != nullptr && events[i]->GetHandle() == handle) {
      RemoveEvent(i);
      return;
    }
}

// DIA: idAIEventManager::RemoveReceiver
void idAIEventManager::RemoveReceiver(idEntity* receiver) {
  receivers.RemoveFast(receiver);
}

// DIA: idSort_Quick<idAIEventPool * __ptr64,idSort_AIEventPoolsByNumAllocated>::Sort
template class idSort_Quick<idAIEventPool*, idSort_AIEventPoolsByNumAllocated>;

// DIA: idAIEventManager::idAIEventManager
idAIEventManager::idAIEventManager() = default;

// DIA: `idAIEventManager::idAIEventManager'::`1'::dtor$0
// DIA: `idAIEventManager::idAIEventManager'::`1'::dtor$1
// DIA: `idAIEventManager::idAIEventManager'::`1'::dtor$2

// DIA: idAIEventManager::UpdateEvents
void idAIEventManager::UpdateEvents(int currentTime) {
  for (int i = events.Num() - 1; i >= 0; --i)
    if (events[i] == nullptr || events[i]->Update(currentTime) ==
                                  idAIEvent::AIUPDATE_REMOVE)
      RemoveEvent(i);
  for (int i = receivers.Num() - 1; i >= 0; --i)
    if (receivers[i] == nullptr || receivers[i]->IsRemoved())
      receivers.RemoveIndexFast(i);
}

// DIA: idAIEventManager::RemoveEntityEvents
void idAIEventManager::RemoveEntityEvents(const idEntity* entity) {
  for (int i = events.Num() - 1; i >= 0; --i)
    if (events[i] != nullptr && events[i]->GetOriginator() == entity)
      RemoveEvent(i);
}

// DIA: idAIEventManager::EnableEvents
void idAIEventManager::EnableEvents(const idEntity* entity,
                                    int eventClassFlags, bool enable) {
  for (int i = 0; i < events.Num(); ++i) {
    idAIEvent* event = events[i];
    if (event != nullptr && event->GetOriginator() == entity &&
        (event->GetEventClass() & eventClassFlags) != 0)
      event->SetEnabled(enable);
  }
}

// DIA: idAIEventManager::MapShutdown
void idAIEventManager::MapShutdown() {
  events.Clear();
  for (int i = 0; i < pools.Num(); ++i) delete pools[i];
  pools.ClearFree();
  receivers.ClearFree();
}

// DIA: idAIEventManager::~idAIEventManager
idAIEventManager::~idAIEventManager() { MapShutdown(); }

// DIA: `idAIEventManager::~idAIEventManager'::`1'::dtor$0
// DIA: `idAIEventManager::~idAIEventManager'::`1'::dtor$1
// DIA: `idAIEventManager::~idAIEventManager'::`1'::dtor$2
// DIA: `idAIEventManager::~idAIEventManager'::`1'::dtor$3

// DIA: idAIEventManager::SetEventInstigator
void idAIEventManager::SetEventInstigator(int handle,
                                          const idEntity* instigator) {
  idAIEvent* event = GetEvent(handle);
  if (event != nullptr) event->SetInstigator(instigator);
}

// DIA: idAIEventManager::AddPool
idAIEventPool* idAIEventManager::AddPool(const idDeclAiEvent* declaration) {
  if (declaration == nullptr || declaration->initialPoolSize <= 0) return nullptr;
  const int type = GetEventType(declaration);
  for (int i = 0; i < pools.Num(); ++i)
    if (pools[i] != nullptr && pools[i]->eventType == type) {
      pools[i]->initialPoolSize += declaration->initialPoolSize;
      return pools[i];
    }
  idAIEventPool* pool = new idAIEventPool(type);
  pool->initialPoolSize = declaration->initialPoolSize;
  pools.Append(pool);
  return pool;
}

// DIA: idAIEventManager::Init
void idAIEventManager::Init() {
  for (int i = 0; i < pools.Num(); ++i)
    if (pools[i] != nullptr) pools[i]->AllocInitialPool();
  idSort_Quick<idAIEventPool*, idSort_AIEventPoolsByNumAllocated> sorter;
  sorter.Sort(pools.list, static_cast<unsigned int>(pools.Num()));
}

// DIA: `idAIEventManager::Init'::`1'::dtor$0

// DIA: idAIEventManager::AddReceiver
void idAIEventManager::AddReceiver(idEntity* receiver) {
  if (receiver == nullptr) return;
  for (int i = 0; i < receivers.Num(); ++i)
    if (receivers[i] == receiver) return;
  receivers.Append(receiver);
}

// DIA: idAIEventManager::AllocEvent
idAIEvent* idAIEventManager::AllocEvent(const idDeclAiEvent* declaration) {
  if (declaration == nullptr) return nullptr;
  const int type = GetEventType(declaration);
  idAIEventPool* pool = nullptr;
  for (int i = 0; i < pools.Num(); ++i)
    if (pools[i] != nullptr && pools[i]->eventType == type) pool = pools[i];
  if (pool == nullptr) pool = AddPool(declaration);
  return pool != nullptr ? pool->AllocEvent() : nullptr;
}

// DIA: idAIEventManager::AddEvent
idAIEvent* idAIEventManager::AddEvent(const idDeclAiEvent* declaration,
                                      int currentTime, int delay) {
  idAIEvent* event = AllocEvent(declaration);
  if (event == nullptr) return nullptr;
  event->Init(declaration, nullptr, nullptr, {});
  event->SetStartTime(currentTime + std::max(0, delay));
  events.Append(event);
  return event;
}

// DIA: idAIEventManager::AddEvent
int idAIEventManager::AddEvent(const idDeclAiEvent* declaration,
                               int currentTime, const idEntity* originator,
                               const idEntity* instigator,
                               const idVec3& origin, int delay) {
  idAIEvent* event = AddEvent(declaration, currentTime, delay);
  if (event == nullptr) return -1;
  event->Init(declaration, originator, instigator, origin);
  event->SetStartTime(currentTime + std::max(0, delay));
  return event->GetHandle();
}

// DIA: `dynamic initializer for 'aievents_show''
// DIA: `dynamic initializer for 'aievents_print''
// DIA: `dynamic atexit destructor for 'aievents_show''
// DIA: `dynamic atexit destructor for 'aievents_print''
