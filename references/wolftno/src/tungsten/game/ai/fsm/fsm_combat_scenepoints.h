#pragma once

#include "tungsten/game/ai/fsm/aifsm.h"
#include "tungsten/game/ai/ai2_actions.h"

struct idCombatScenePointContext {
  bool shallMoveToScenePoint{false};
  bool shallStrafe{false};
  bool shallIdle{false};
  bool shallReload{false};
  bool shallLookAtScenePoint{false};
  bool shallStayAtScenePoint{false};
  bool shallEnterTurret{false};
  bool pounceDamaged{false};
  bool shallFindScenePoint{false};
  bool shallThrowGrenade{false};
  bool shallTacticalAttack{false};
  bool shallTacticalCover{false};
  bool shallPounce{false};
  bool shallChangeStance{false};
  bool shallMelee{false};
  bool shallGrapple{false};
  bool shallOutOfReach{false};
  bool shallTesla{false};
  bool finished{false};
  bool shallClose{false};
  bool shallApplyRestriction{false};
  bool shallDodge{false};
  bool shallDodgeGrenade{false};
  bool enabled{true};
  int stance{1};
  int bestCoverAction{0};
  idScenePoint_Combat* scenePoint{nullptr};
};

#define WOLF_COMBAT_EVALUATORS(X) \
  X(CombatScenePoint_Shall_MoveToScenePoint, shallMoveToScenePoint) \
  X(CombatScenePoint_Shall_Strafe, shallStrafe) \
  X(CombatScenePoint_Shall_Idle, shallIdle) \
  X(CombatScenePoint_Shall_Reload, shallReload) \
  X(CombatScenePoint_Shall_LookAtScenePoint, shallLookAtScenePoint) \
  X(CombatScenePoint_Shall_StayAtScenePoint, shallStayAtScenePoint) \
  X(CombatScenePoint_Shall_Enter_Turret, shallEnterTurret) \
  X(CombatScenePoint_Pounce_Damaged, pounceDamaged) \
  X(CombatScenePoint_Shall_FindScenePoint, shallFindScenePoint) \
  X(CombatScenePoint_Shall_ThrowGrenade, shallThrowGrenade) \
  X(CombatScenePoint_Shall_TacticalAttack, shallTacticalAttack) \
  X(CombatScenePoint_Shall_TacticalCover, shallTacticalCover) \
  X(CombatScenePoint_Shall_Pounce, shallPounce) \
  X(CombatScenePoint_Shall_ChangeStance, shallChangeStance) \
  X(CombatScenePoint_Shall_Melee, shallMelee) \
  X(CombatScenePoint_Shall_Grapple, shallGrapple) \
  X(CombatScenePoint_Shall_OutOfReach, shallOutOfReach) \
  X(CombatScenePoint_Shall_Tesla, shallTesla) \
  X(CombatScenePoint_Shared_Finished, finished) \
  X(CombatScenePoint_Shall_Close, shallClose) \
  X(CombatScenePoint_Shall_ApplyRestriction, shallApplyRestriction) \
  X(CombatScenePoint_Shall_Dodge, shallDodge) \
  X(CombatScenePoint_Shall_DodgeGrenade, shallDodgeGrenade)
#define WOLF_DECLARE_COMBAT_EVALUATOR(Name, Field) \
  class Name { public: int Evaluate(const idCombatScenePointContext& context) const; };
WOLF_COMBAT_EVALUATORS(WOLF_DECLARE_COMBAT_EVALUATOR)
#undef WOLF_DECLARE_COMBAT_EVALUATOR

void SetStance(idCombatScenePointContext& context, int stance);
int GetBestCoverAction(const idCombatScenePointContext& context);

class idPlayerControlled {
public:
  idPlayer* GetPlayerUser() const;
  void SetPlayerUser(idPlayer* value) { player = value; }
private:
  idPlayer* player{nullptr};
};

class idAIEditable {
public:
  class idAIActionSettings {
  public:
    bool HasSquadLeader() const;
    idAI2* GetSquadLeader() const;
    void SetSquadLeader(idAI2* value) { squadLeader = value; }
  private:
    idAI2* squadLeader{nullptr};
  };
};

class idCombatScenePointState : public idAIState {
public:
  explicit idCombatScenePointState(const char* name) : idAIState(name) {}
  void RecordEnter(unsigned int time) { ++entries; lastTime = time; }
  void RecordWork(unsigned int time) { ++works; lastTime = time; }
  void RecordExit(unsigned int time) { ++exits; lastTime = time; }
  int entries{0}, works{0}, exits{0};
  unsigned int lastTime{0};
  bool animationDone{false};
  bool meleeBlocked{false};
  bool hidden{false};
};

#define WOLF_COMBAT_STATES(X) \
  X(Find) X(MoveTo) X(StayAt) X(Strafe) X(ThrowGrenade) X(TacticalAttack) \
  X(TacticalCover) X(UseTurret) X(Close) X(ApplyRestriction) \
  X(EnterScenePoint) X(Reload) X(Pounce) X(Grapple) X(SharedFinished) \
  X(Idle) X(Melee) X(OutOfReach) X(SharedWaitForAnimation) X(Tesla) \
  X(Dodge) X(DodgeGrenade)

#define WOLF_DECLARE_COMBAT_STATE(Name) \
  class idCombatScenePoint_##Name : public idCombatScenePointState { \
  public: \
    idCombatScenePoint_##Name() : idCombatScenePointState(#Name) {} \
    void InternalInitState(); \
    void AIState_EnterState(idAIFSM*, unsigned int) override; \
    void AIState_Work(idAIFSM*, unsigned int) override; \
    void AIState_ExitState(idAIFSM*, unsigned int) override; \
    void AIState_ReenterState(idAIFSM*, unsigned int, int) override; \
    bool FSMEvent_AnimationDone(); \
    bool FSMEvent_MeleeBlocked(); \
    bool FSMEvent_Hide(); \
  };
WOLF_COMBAT_STATES(WOLF_DECLARE_COMBAT_STATE)
#undef WOLF_DECLARE_COMBAT_STATE

class idCombatScenePointFSM : public idAIFSM {
public:
  idCombatScenePointFSM();
  ~idCombatScenePointFSM() override;
  void InitStates(const idFiniteStateMachineParams* params) override;
  bool CanUse(const idCombatScenePointContext& context, int currentTime) const;
  idCombatScenePoint_Find& GetFindState() { return stateFind; }
  idCombatScenePoint_MoveTo& GetMoveToState() { return stateMoveTo; }
  idCombatScenePoint_StayAt& GetStayAtState() { return stateStayAt; }

private:
#define WOLF_COMBAT_MEMBER(Name) idCombatScenePoint_##Name state##Name;
  WOLF_COMBAT_STATES(WOLF_COMBAT_MEMBER)
#undef WOLF_COMBAT_MEMBER
};
