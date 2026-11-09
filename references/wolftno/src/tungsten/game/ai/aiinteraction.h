#pragma once

#include "shared/idlib/containers/list.h"
#include "shared/idlib/text/str.h"
#include "tungsten/game/ai/ai2_actions.h"

enum aiInteractionEventType_t {
  IEV_ACTIONSCRIPT,
  IEV_MAX,
};

class idDeclAIInteraction {
public:
  struct eventData_t {
    aiInteractionEventType_t type{IEV_MAX};
    int timeMS{0};
    int actorIndex{0};
    idStr actionScript;
  };

  struct declInfo_t {
    const char* typeName{"aiInteraction"};
  };

  idDeclAIInteraction();
  virtual ~idDeclAIInteraction();
  declInfo_t* GetDeclInfo();
  int AddEvent(const eventData_t& event) { return events.Append(event); }
  int NumEvents() const { return events.Num(); }
  const eventData_t& GetEvent(int index) const { return events[index]; }
  void SetName(const char* value) { name = value; }
  const idStr& GetName() const { return name; }

private:
  friend class idAIInteraction;
  static declInfo_t resourceList;
  idStr name;
  idList<eventData_t, 5> events;
};

class idAIInteraction final : public idEntity {
public:
  idAIInteraction();
  void Spawn();
  void BeginInteraction(idEntity* activator);
  void EndInteraction();
  void Think();
  void OnActivate(idEntity* activator) override;

  void SetInteractionDecl(const idDeclAIInteraction* value) {
    interactionDecl = value;
  }
  void AddTarget(idEntity* value) { targets.Append(value); }
  void SetCurrentTime(int value) { currentTime = value; }
  bool IsInteractionActive() const { return active; }
  int GetNextEvent() const { return nextEvent; }
  idEntity* GetActivator() const { return activator; }

private:
  const idDeclAIInteraction* interactionDecl{nullptr};
  int startTime{-1};
  int nextEvent{-1};
  idEntity* activator{nullptr};
  idList<idEntity*, 5> targets;
  int currentTime{0};
  bool active{false};
};
