#pragma once

#include "tungsten/game/ai/fsm/aifsm.h"
#include "tungsten/game/ai/ai2_actions.h"

struct idSearchScenePointContext {
  bool shallMoveToScenePoint{false};
  bool shallMoveToPos{false};
  bool shallPerformSearch{false};
  bool shallFindScenePoint{false};
  bool shallStayAtScenePoint{false};
  bool shallLookAtScenePoint{false};
  bool enabled{true};
  int searchAction{0};
  idScenePoint* scenePoint{nullptr};
  idVec3 searchPosition{};
};

int GetSearchAction(const idSearchScenePointContext& context);
#define WOLF_SEARCH_EVALUATORS(X) \
  X(SearchScenePoint_Shall_MoveToScenePoint, shallMoveToScenePoint) \
  X(SearchScenePoint_Shall_MoveToPos, shallMoveToPos) \
  X(SearchScenePoint_Shall_PerformSearch, shallPerformSearch) \
  X(SearchScenePoint_Shall_FindScenePoint, shallFindScenePoint) \
  X(SearchScenePoint_Shall_StayAtScenePoint, shallStayAtScenePoint) \
  X(SearchScenePoint_Shall_LookAtScenePoint, shallLookAtScenePoint)
#define WOLF_DECLARE_SEARCH_EVALUATOR(Name, Field) \
  class Name { public: int Evaluate(const idSearchScenePointContext& context) const; };
WOLF_SEARCH_EVALUATORS(WOLF_DECLARE_SEARCH_EVALUATOR)
#undef WOLF_DECLARE_SEARCH_EVALUATOR

class idSearchScenePointState : public idAIState {
public:
  explicit idSearchScenePointState(const char* name) : idAIState(name) {}
  void RecordEnter(unsigned int time) { ++entries; lastTime = time; }
  void RecordWork(unsigned int time) { ++works; lastTime = time; }
  void RecordExit(unsigned int time) { ++exits; lastTime = time; }
  int entries{0}, works{0}, exits{0};
  unsigned int lastTime{0};
};

#define WOLF_SEARCH_STATES(X) X(Find) X(MoveTo) X(StayAt) X(MoveToPos) \
  X(PerformSearchAnimation)
#define WOLF_DECLARE_SEARCH_STATE(Name) \
  class idSearchScenePoint_##Name : public idSearchScenePointState { \
  public: \
    idSearchScenePoint_##Name() : idSearchScenePointState(#Name) {} \
    void InternalInitState(); \
    void AIState_EnterState(idAIFSM*, unsigned int) override; \
    void AIState_Work(idAIFSM*, unsigned int) override; \
    void AIState_ExitState(idAIFSM*, unsigned int) override; \
    void AIState_ReenterState(idAIFSM*, unsigned int, int) override; \
  };
WOLF_SEARCH_STATES(WOLF_DECLARE_SEARCH_STATE)
#undef WOLF_DECLARE_SEARCH_STATE

class idSearchScenePointFSM : public idAIFSM {
public:
  idSearchScenePointFSM();
  ~idSearchScenePointFSM() override;
  void InitStates(const idFiniteStateMachineParams* params) override;
  bool CanUse(const idSearchScenePointContext& context, int currentTime) const;
  idSearchScenePoint_Find& GetFindState() { return stateFind; }
  idSearchScenePoint_MoveToPos& GetMoveToPosState() { return stateMoveToPos; }
  idSearchScenePoint_PerformSearchAnimation& GetSearchAnimationState() {
    return statePerformSearchAnimation;
  }
private:
#define WOLF_SEARCH_MEMBER(Name) idSearchScenePoint_##Name state##Name;
  WOLF_SEARCH_STATES(WOLF_SEARCH_MEMBER)
#undef WOLF_SEARCH_MEMBER
};
