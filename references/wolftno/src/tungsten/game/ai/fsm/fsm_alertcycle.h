#pragma once

#include "tungsten/game/ai/aimemory.h"
#include "tungsten/game/ai/fsm/aifsm.h"

struct idAlertCycleContext {
  bool executingScript{false};
  bool unhandledError{false};
  bool relaxed{false};
  bool doomscriptDone{false};
  bool hasEquippedWeapon{false};
  bool restart{false};
  bool canUseBackstand{false};
  bool inPain{false};
  bool hasActionScript{false};
  bool painFinished{false};
  bool unstuck{false};
  bool dyingFinished{false};
  bool shallReact{false};
  bool canUseCombatScenePoint{false};
  bool canUseSearchScenePoint{false};
  bool canUseRoamScenePoint{false};
  bool noEnemy{false};
  bool playerApproaching{false};
  unsigned int pendingReactions{0};
  unsigned int forbiddenReactions{0};
};

#define WOLF_ALERT_EVALUATORS(X) \
  X(AC_ExecutingScript, executingScript) X(AC_UnhandledError, unhandledError) \
  X(AC_IsRelaxed, relaxed) X(AC_DoomscriptDone, doomscriptDone) \
  X(AC_HasEquippedWeapon, hasEquippedWeapon) X(AC_Restart, restart) \
  X(AC_CanUseBackstand, canUseBackstand) X(AC_PainFinished, painFinished) \
  X(AC_Unstuck, unstuck) X(AC_DyingFinished, dyingFinished) \
  X(AC_Shall_FSMReact, shallReact) \
  X(AC_CanUseCombatScenePoint, canUseCombatScenePoint) \
  X(AC_CanUseSearchScenePoint, canUseSearchScenePoint) \
  X(AC_CanUseRoamScenePoint, canUseRoamScenePoint) X(AC_NoEnemy, noEnemy) \
  X(AC_PlayerApproaching, playerApproaching)

#define WOLF_DECLARE_ALERT_EVALUATOR(Name, Field) \
  class Name { public: int Evaluate(const idAlertCycleContext& context) const; };
WOLF_ALERT_EVALUATORS(WOLF_DECLARE_ALERT_EVALUATOR)
#undef WOLF_DECLARE_ALERT_EVALUATOR

class AC_NoActionScript {
public:
  int Evaluate(const idAlertCycleContext& context) const;
};

bool AC_InPain(const idAlertCycleContext& context);
bool FSM_SetupReaction(idAlertCycleContext& context, unsigned int reaction,
                       int currentTime);

class idAlertCycleState : public idAIState {
public:
  explicit idAlertCycleState(const char* name) : idAIState(name) {}
  void RecordEnter(unsigned int time) { ++enterCount; lastTime = time; }
  void RecordWork(unsigned int time) { ++workCount; lastTime = time; }
  void RecordExit(unsigned int time) { ++exitCount; lastTime = time; }
  int enterCount{0};
  int workCount{0};
  int exitCount{0};
  unsigned int lastTime{0};
};

#define WOLF_ALERT_STATES(X) \
  X(FSMReaction) X(Default) X(ExecuteDoomscript) X(ExecuteCodeMoveAction) \
  X(ActionScript) X(EquipWeapon) X(CombatScenePoint) X(SearchScenePoint) \
  X(RoamScenePoint) X(BackStand) X(Pain) X(Dying) X(Stuck) \
  X(UnhandledError) X(Dead)

#define WOLF_DECLARE_ALERT_STATE(Name) \
  class idAlertCycle_##Name : public idAlertCycleState { \
  public: \
    idAlertCycle_##Name() : idAlertCycleState(#Name) {} \
    void InternalInitState(); \
    void AIState_EnterState(idAIFSM*, unsigned int) override; \
    void AIState_Work(idAIFSM*, unsigned int) override; \
    void AIState_ExitState(idAIFSM*, unsigned int) override; \
  };
WOLF_ALERT_STATES(WOLF_DECLARE_ALERT_STATE)
#undef WOLF_DECLARE_ALERT_STATE

class idAlertCycleFSM : public idAIFSM {
public:
  idAlertCycleFSM();
  ~idAlertCycleFSM() override;
  void InitStates(const idFiniteStateMachineParams* params) override;

  idAlertCycle_Default& GetDefaultCycleState() { return stateDefault; }
  idAlertCycle_Pain& GetPainState() { return statePain; }
  idAlertCycle_Dead& GetDeadState() { return stateDead; }

private:
  idAlertCycle_FSMReaction stateFSMReaction;
  idAlertCycle_Default stateDefault;
  idAlertCycle_ExecuteDoomscript stateExecuteDoomscript;
  idAlertCycle_ExecuteCodeMoveAction stateExecuteCodeMoveAction;
  idAlertCycle_ActionScript stateActionScript;
  idAlertCycle_EquipWeapon stateEquipWeapon;
  idAlertCycle_CombatScenePoint stateCombatScenePoint;
  idAlertCycle_SearchScenePoint stateSearchScenePoint;
  idAlertCycle_RoamScenePoint stateRoamScenePoint;
  idAlertCycle_BackStand stateBackStand;
  idAlertCycle_Pain statePain;
  idAlertCycle_Dying stateDying;
  idAlertCycle_Stuck stateStuck;
  idAlertCycle_UnhandledError stateUnhandledError;
  idAlertCycle_Dead stateDead;
};

extern idCVar ai_showDeathInfo;
extern idCVar ai_painGoreTest;
