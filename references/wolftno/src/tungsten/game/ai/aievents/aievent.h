#pragma once

#include "tungsten/game/game_runtime.h"
#include "shared/idlib/containers/list.h"
#include "shared/idlib/text/str.h"

class idTrajectory {
public:
  virtual ~idTrajectory();
};

class idDeclAiEvent {
public:
  struct declInfo_t {
    idList<idDeclAiEvent*, 5> declarations;
  };

  idDeclAiEvent();
  declInfo_t* GetDeclInfo();
  static void LoadAllDecls();
  static void Register(idDeclAiEvent* declaration);
  static int GetLoadedCount();

  idStr name;
  int initialPoolSize{8};
  int duration{0};
  int priority{0};
  int instigatorStimulus{0};
  int eventClass{0};
  bool attached{false};
  bool loaded{false};
  float lookAheadTime{0.0F};
  float volume{1.0F};
  float intensityScale{1.0F};
  float distance{0.0F};
  int voiceMessage{7};
  int delayedMessage{30};
  float arcWidthDegrees{0.0F};
  float heightMin{0.0F};
  float heightMax{0.0F};
  idBounds bounds{};

private:
  static declInfo_t resourceList;
  static int loadedCount;
};

class idAIEvent {
public:
  enum aiEventClass_t : int {};
  enum aiEventUpdateResult_t { AIUPDATE_KEEP, AIUPDATE_REMOVE };

  idAIEvent();
  virtual ~idAIEvent() = default;
  virtual float InternalGetIntensity(const idEntity* entity) const;
  virtual aiEventUpdateResult_t InternalUpdate(int currentTime);
  virtual aiEventUpdateResult_t InternalUpdateAttached(int currentTime);

  float GetIntensity(const idEntity* entity) const;
  int GetDuration() const;
  int GetPriority() const;
  int GetInstigatorStimulus() const;
  aiEventClass_t GetEventClass() const;
  aiEventUpdateResult_t UpdateAttached(int currentTime);
  aiEventUpdateResult_t Update(int currentTime);
  void Init(const idDeclAiEvent* declaration, const idEntity* eventOriginator,
            const idEntity* eventInstigator, const idVec3& eventOrigin);

  int GetHandle() const { return handle; }
  const idVec3& GetOrigin() const { return origin; }
  const idEntity* GetOriginator() const { return originator; }
  const idEntity* GetInstigator() const { return instigator; }
  const idDeclAiEvent* GetDeclaration() const { return eventDecl; }
  bool IsEnabled() const { return enabled; }
  int GetStartTime() const { return startTime; }
  void SetEnabled(bool value) { enabled = value; }
  void SetStartTime(int value) { startTime = value; }
  void SetInstigator(const idEntity* value) { instigator = value; }
  void SetTrackedOrigin(const idVec3& value) {
    origin = value;
    stimulusOrigin = value;
  }
  void ConfigureEventForTest(const idEntity* eventOriginator,
                             const idEntity* eventInstigator,
                             const idVec3& eventOrigin) {
    originator = eventOriginator;
    instigator = eventInstigator;
    origin = eventOrigin;
  }
  void SetHandleForTest(int value) { handle = value; }

private:
  const idDeclAiEvent* eventDecl{nullptr};
  bool enabled{true};
  const idEntity* originator{nullptr};
  const idEntity* instigator{nullptr};
  idVec3 origin{};
  idVec3 stimulusOrigin{};
  int startTime{-1};
  int handle{-1};
  static int numEvents;
};
