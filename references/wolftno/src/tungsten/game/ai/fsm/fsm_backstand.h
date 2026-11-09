#pragma once

#include "tungsten/game/ai/fsm/aifsm.h"

#include <array>

struct idBackStandContext {
  bool searchFinished{false};
  bool error{false};
  bool moveDone{false};
  bool targetLost{false};
  bool shouldReturn{false};
  bool shouldRise{false};
  bool shouldOutOfReach{false};
  bool shouldOutOfReachFromLedge{false};
  bool shouldMove{false};
  bool shouldAttack{false};
  bool shouldLower{false};
  bool shouldChangeHint{false};
  bool enabled{true};
  idEntity* actor{nullptr};
  idEntity* enemy{nullptr};
  idEntity* currentHint{nullptr};
  std::array<idEntity*, 16> hints{};
  int hintCount{0};
  float attackReach{96.0F};
  float standReach{192.0F};
};

#define WOLF_BACKSTAND_EVALUATORS(X) \
  X(SearchScenePoint_Shared_Finished, searchFinished, 452) \
  X(BACKSTAND_Error, error, 349) X(BACKSTAND_MoveDone, moveDone, 456) \
  X(BACKSTAND_TargetLost, targetLost, 456) \
  X(BACKSTAND_ShouldReturn, shouldReturn, 456) \
  X(BACKSTAND_ShouldRise, shouldRise, 456) \
  X(BACKSTAND_ShouldOutOfReach, shouldOutOfReach, 456) \
  X(BACKSTAND_ShouldOutOfReachFromLedge, shouldOutOfReachFromLedge, 456) \
  X(BACKSTAND_ShouldMove, shouldMove, 456) \
  X(BACKSTAND_ShouldAttack, shouldAttack, 456) \
  X(BACKSTAND_ShouldLower, shouldLower, 456) \
  X(BACKSTAND_ShouldChangeHint, shouldChangeHint, 456)
#define WOLF_DECLARE_BACKSTAND_EVALUATOR(Name, Field, Code) \
  class Name { public: int Evaluate(const idBackStandContext& context) const; };
WOLF_BACKSTAND_EVALUATORS(WOLF_DECLARE_BACKSTAND_EVALUATOR)
#undef WOLF_DECLARE_BACKSTAND_EVALUATOR

float EntityPlaneDistance(const idEntity* left, const idEntity* right);
idVec3 GetHintRisePosition(const idEntity* actor, const idEntity* hint);
idVec2 PredictedEnemyPlanePosition(const idEntity* enemy, float time);
idEntity* FindBestHint(const idBackStandContext& context);
int GetEnemySide(const idEntity* actor, const idEntity* enemy);

class idBackStandState : public idAIState {
public:
  explicit idBackStandState(const char* name) : idAIState(name) {}
  void RecordEnter(unsigned int time) { ++entries; lastTime = time; }
  void RecordWork(unsigned int time) { ++works; lastTime = time; }
  int entries{0};
  int works{0};
  unsigned int lastTime{0};
};

class idBackStand_Default : public idBackStandState {
public:
  idBackStand_Default() : idBackStandState("Default") {}
  void InternalInitState();
  void AIState_EnterState(idAIFSM*, unsigned int) override;
};
class idBackStand_Moving : public idBackStandState {
public:
  idBackStand_Moving() : idBackStandState("Moving") {}
  void InternalInitState();
  void AIState_EnterState(idAIFSM*, unsigned int) override;
  void AIState_Work(idAIFSM*, unsigned int) override;
};
class idBackStand_OutOfReach : public idBackStandState {
public:
  idBackStand_OutOfReach() : idBackStandState("OutOfReach") {}
  void InternalInitState();
  void AIState_EnterState(idAIFSM*, unsigned int) override;
};
class idBackStand_Rising : public idBackStandState {
public:
  idBackStand_Rising() : idBackStandState("Rising") {}
  void InternalInitState();
  void AIState_EnterState(idAIFSM*, unsigned int) override;
  void AIState_Work(idAIFSM*, unsigned int) override;
};
class idBackStand_Standing : public idBackStandState {
public:
  idBackStand_Standing() : idBackStandState("Standing") {}
  void InternalInitState();
};
class idBackStand_Attacking : public idBackStandState {
public:
  idBackStand_Attacking() : idBackStandState("Attacking") {}
  void InternalInitState();
  void AIState_EnterState(idAIFSM*, unsigned int) override;
};
class idBackStand_Lowering : public idBackStandState {
public:
  idBackStand_Lowering() : idBackStandState("Lowering") {}
  void InternalInitState();
  void AIState_EnterState(idAIFSM*, unsigned int) override;
};

class idBackStandFSM : public idAIFSM {
public:
  idBackStandFSM();
  ~idBackStandFSM() override;
  void InitStates(const idFiniteStateMachineParams* params) override;
  bool CanUse(const idBackStandContext& context, int currentTime) const;
  idBackStand_Default& GetDefaultBackStandState() { return stateDefault; }
  idBackStand_Moving& GetMovingState() { return stateMoving; }
  idBackStand_Rising& GetRisingState() { return stateRising; }

private:
  idBackStand_Default stateDefault;
  idBackStand_Moving stateMoving;
  idBackStand_OutOfReach stateOutOfReach;
  idBackStand_Rising stateRising;
  idBackStand_Standing stateStanding;
  idBackStand_Attacking stateAttacking;
  idBackStand_Lowering stateLowering;
};
