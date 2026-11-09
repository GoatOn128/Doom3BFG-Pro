#pragma once

#include "tungsten/game/ai/aievents/aievent.h"

#include <algorithm>

class idAIEventPool {
public:
  idAIEventPool();
  explicit idAIEventPool(int eventTypeValue);
  virtual ~idAIEventPool();
  idAIEvent* AllocEvent();
  void Free();
  void FreeEvent(idAIEvent* event);
  void AllocInitialPool();
  int GetNumAllocated() const { return allocatedCount; }
  int GetStorageCount() const { return storage.Num(); }
  int GetFreeCount() const { return freeEvents.Num(); }

  int eventType{0};
  int initialPoolSize{0};

private:
  idList<idAIEvent*, 5> storage;
  idList<idAIEvent*, 5> freeEvents;
  int allocatedCount{0};
};

template <typename T>
class idSort {
public:
  virtual ~idSort();
};

class idSort_AIEventPoolsByNumAllocated : public idSort<idAIEventPool*> {
public:
  ~idSort_AIEventPoolsByNumAllocated() override;
  bool operator()(const idAIEventPool* lhs, const idAIEventPool* rhs) const {
    return lhs->GetNumAllocated() < rhs->GetNumAllocated();
  }
};

template <typename T, typename Comparator>
class idSort_Quick {
public:
  void Sort(T* base, unsigned int count) {
    if (base != nullptr) std::sort(base, base + count, Comparator{});
  }
};

class idAIEventManager {
public:
  idAIEventManager();
  ~idAIEventManager();
  static int GetEventType(const idDeclAiEvent* declaration);
  idAIEvent* GetEvent(int handle) const;
  void RemoveEvent(int index);
  void RemoveEventByHandle(int handle);
  void RemoveReceiver(idEntity* receiver);
  void UpdateEvents(int currentTime);
  void RemoveEntityEvents(const idEntity* entity);
  void EnableEvents(const idEntity* entity, int eventClassFlags, bool enable);
  void MapShutdown();
  void SetEventInstigator(int handle, const idEntity* instigator);
  idAIEventPool* AddPool(const idDeclAiEvent* declaration);
  void Init();
  void AddReceiver(idEntity* receiver);
  idAIEvent* AllocEvent(const idDeclAiEvent* declaration);
  idAIEvent* AddEvent(const idDeclAiEvent* declaration, int currentTime,
                      int delay);
  int AddEvent(const idDeclAiEvent* declaration, int currentTime,
               const idEntity* originator, const idEntity* instigator,
               const idVec3& origin, int delay);

  int GetEventCount() const { return events.Num(); }
  int GetPoolCount() const { return pools.Num(); }
  int GetReceiverCount() const { return receivers.Num(); }

private:
  idList<idAIEvent*, 5> events;
  idList<idAIEventPool*, 5> pools;
  idList<idEntity*, 5> receivers;
};

extern idCVar aievents_show;
extern idCVar aievents_print;
