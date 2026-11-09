#pragma once

#include "tungsten/game/ai/fsm/finitestatemachine.h"

#include <array>

class idAI2;

enum idAIFSMEventType {
  FSM_EVENT_NORMAL = 0,
  FSM_EVENT_ACTION = 1,
};

struct idAIFSMEvent {
  idAIFSMEventType type{FSM_EVENT_NORMAL};
  const char* name{nullptr};
};

struct idAIFSMEventResult {
  bool handled{false};
  idAIAction* action{nullptr};
};

class idAIFSM : public idFiniteStateMachine {
public:
  idAIFSM() = default;
  using idFiniteStateMachine::InternalCallEvent;
  using idFiniteStateMachine::InternalRespondsTo;
  idAI2* GetAI() const;
  void AppendDefaultTransitions(idAIState* state) override;
  void InitStates(const idFiniteStateMachineParams* params) override;
  idAIStateError* GetErrorState();
  idAIStateDone* GetDoneState();
  idAIFSMEventResult InternalCallEvent(const idAIFSMEvent& event,
                                       const idEventArgs* args = nullptr);
  bool InternalRespondsTo(const idAIFSMEvent& event) const;

  void SetAI(idAI2* value) { SetOwner(reinterpret_cast<idEntity*>(value)); }
  void SetActionManager(idAIActionManager* value) { actionManager = value; }
  void SetActionCallerState(idAIState* value) { actionCallerState = value; }
  void SetMoveDone(bool value) { moveDone = value; }
  bool IsMoveDone() const { return moveDone; }
  void SetLastWorkTransCode(int value) { lastWorkTransCode = value; }
  void SetLastFSMEventTransCode(int value) { lastFSMEventTransCode = value; }
  const char* GetName() const { return fsmName; }
  int GetTypeNum() const { return typeNum; }
  void SetIdentity(const char* value, int type) {
    fsmName = value != nullptr ? value : "";
    typeNum = type;
  }

  idAIState* GetErrorStateBase() const override {
    return const_cast<idAIStateError*>(&errorState);
  }
  idAIState* GetDoneStateBase() const override {
    return const_cast<idAIStateDone*>(&doneState);
  }

private:
  idAIActionManager* actionManager{nullptr};
  idAIState* actionCallerState{nullptr};
  idAIStateError errorState;
  idAIStateDone doneState;
  int lastWorkTransCode{0};
  int lastFSMEventTransCode{0};
  bool moveDone{false};
  const char* fsmName{"aifsm"};
  int typeNum{0};

  friend class idStateTransition_StateError;
  friend class Shared_WorkTransCode;
  friend class Shared_FSMEventTransCode;
};

class idStateTransition_UnhandledChildError {
public:
  int Evaluate(const idAIState& state) const;
};
class idStateTransition_StateError {
public:
  int Evaluate(const idAIFSM& parentFSM) const;
};
class Shared_CanUse { public: int Evaluate() const; };
class Shared_DefaultUse { public: int Evaluate() const; };
class Shared_ChildFinished { public: int Evaluate(const idAIState& state) const; };
class Shared_MoveDone { public: int Evaluate(const idAIFSM& parentFSM) const; };
class Shared_WorkTransCode { public: int Evaluate(const idAIFSM& parentFSM) const; };
class Shared_FSMEventTransCode { public: int Evaluate(const idAIFSM& parentFSM) const; };
