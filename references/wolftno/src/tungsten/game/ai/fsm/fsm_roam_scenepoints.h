#pragma once

#include "tungsten/game/ai/fsm/aifsm.h"
#include "tungsten/game/ai/ai2_actions.h"

struct idRoamScenePointContext {
  bool shallIdle{false};
  bool shallMoveToScenePoint{false};
  bool shallFindScenePoint{false};
  bool shallStayAtScenePoint{false};
  bool shallLookAtScenePoint{false};
  bool enabled{true};
  idScenePoint* scenePoint{nullptr};
};

#define WOLF_ROAM_EVALUATORS(X) \
  X(RoamScenePoint_Shall_Idle, shallIdle) \
  X(RoamScenePoint_Shall_MoveToScenePoint, shallMoveToScenePoint) \
  X(RoamScenePoint_Shall_FindScenePoint, shallFindScenePoint) \
  X(RoamScenePoint_Shall_StayAtScenePoint, shallStayAtScenePoint) \
  X(RoamScenePoint_Shall_LookAtScenePoint, shallLookAtScenePoint)
#define WOLF_DECLARE_ROAM_EVALUATOR(Name, Field) \
  class Name { public: int Evaluate(const idRoamScenePointContext& context) const; };
WOLF_ROAM_EVALUATORS(WOLF_DECLARE_ROAM_EVALUATOR)
#undef WOLF_DECLARE_ROAM_EVALUATOR

class idRoamScenePointState : public idAIState {
public:
  explicit idRoamScenePointState(const char* name) : idAIState(name) {}
  void RecordEnter(unsigned int time) { ++entries; lastTime = time; }
  void RecordWork(unsigned int time) { ++works; lastTime = time; }
  void RecordExit(unsigned int time) { ++exits; lastTime = time; }
  int entries{0}, works{0}, exits{0};
  unsigned int lastTime{0};
};

#define WOLF_ROAM_STATES(X) X(Idle) X(Find) X(MoveTo) X(StayAt) \
  X(SharedWaitForAnimation) X(SharedFinished)
#define WOLF_DECLARE_ROAM_STATE(Name) \
  class idRoamScenePoint_##Name : public idRoamScenePointState { \
  public: \
    idRoamScenePoint_##Name() : idRoamScenePointState(#Name) {} \
    void InternalInitState(); \
    void AIState_EnterState(idAIFSM*, unsigned int) override; \
    void AIState_Work(idAIFSM*, unsigned int) override; \
    void AIState_ExitState(idAIFSM*, unsigned int) override; \
    void AIState_ReenterState(idAIFSM*, unsigned int, int) override; \
  };
WOLF_ROAM_STATES(WOLF_DECLARE_ROAM_STATE)
#undef WOLF_DECLARE_ROAM_STATE

class idRoamScenePointFSM : public idAIFSM {
public:
  idRoamScenePointFSM();
  ~idRoamScenePointFSM() override;
  void InitStates(const idFiniteStateMachineParams* params) override;
  bool CanUse(const idRoamScenePointContext& context, int currentTime) const;
  idRoamScenePoint_Find& GetFindState() { return stateFind; }
  idRoamScenePoint_MoveTo& GetMoveToState() { return stateMoveTo; }
  idRoamScenePoint_StayAt& GetStayAtState() { return stateStayAt; }
private:
#define WOLF_ROAM_MEMBER(Name) idRoamScenePoint_##Name state##Name;
  WOLF_ROAM_STATES(WOLF_ROAM_MEMBER)
#undef WOLF_ROAM_MEMBER
};
