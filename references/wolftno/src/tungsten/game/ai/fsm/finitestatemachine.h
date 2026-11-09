#pragma once

#include "tungsten/game/ai/fsm/aiaction.h"

#include <string>

class idFSMManager;

struct idFiniteStateMachineParams {
  bool startActive{true};
  idEntity* owner{nullptr};
  idFSMManager* manager{nullptr};
};

struct idFSMTransitionResult {
  idAIState* nextState{nullptr};
  const char* transitionType{nullptr};
  int transCode{0};
};

enum idFSMStatus {
  FSM_STATUS_DONE = 0,
  FSM_STATUS_RUNNING = 1,
  FSM_STATUS_ERROR = 2,
};

class idFiniteStateMachine {
public:
  idFiniteStateMachine();
  virtual ~idFiniteStateMachine();

  void ExitChildStates(idFiniteStateMachine* parentFSM, int currentTime);
  void SetNextState(const idFSMTransitionResult& result, int currentTime);
  void ForceUpdate();
  idFSMStatus GetStatus() const;
  void SetError(const char* transitionType, int transCode);
  bool HadError() const;
  bool IsInterruptible() const;
  void SetDefaultState(idAIState* state);
  virtual void Init(const idFiniteStateMachineParams* params);
  idAIState* FindState(const char* stateName) const;
  virtual bool InternalRespondsTo(const char* eventName) const;
  virtual void SetState(const idFSMTransitionResult& result,
                        unsigned int currentTime, bool forcingStack = false);
  void SetCurrentState(idAIState* newState, const char* transitionType,
                       int transCode, unsigned int currentTime);
  void TransitionState(const idFSMTransitionResult& result, int currentTime);
  void Save(std::string* output, const char* indent) const;
  bool PostEvent(int eventId, const void* eventData, unsigned int currentTime);
  virtual bool InternalCallEvent(const char* eventName);
  idEntity* GetOwner() const;
  void SetOwner(idEntity* value) { owner = value; }
  void AddState(idAIState* state);
  void Update(bool evaluate, int currentTime,
              idList<idState::transitionInfo_t, 5>* info = nullptr);
  void Restart(unsigned int currentTime);
  void Restart(idFiniteStateMachine* parentFSM, unsigned int currentTime);

  virtual void InitStates(const idFiniteStateMachineParams*) {}
  virtual void AppendDefaultTransitions(idAIState*) {}
  virtual idAIState* GetErrorStateBase() const { return nullptr; }
  virtual idAIState* GetDoneStateBase() const { return nullptr; }

  idAIState* GetCurrentState() const { return currentState; }
  idAIState* GetPreviousState() const { return previousState; }
  int GetStateCount() const { return states.Num(); }
  int GetLastTransitionCode() const { return lastTransitionCode; }
  int GetLastTransitionTime() const { return lastTransitionTime; }
  int GetNextUpdateTime() const { return nextUpdateTime; }
  int GetStateFrame() const { return stateFrame; }

protected:
  idList<idAIState*, 5> states;
  idAIState* currentState{nullptr};
  idAIState* previousState{nullptr};
  idAIState* defaultState{nullptr};
  idAIState* errorSourceState{nullptr};
  idEntity* owner{nullptr};
  idFSMManager* manager{nullptr};
  const char* errorTransitionType{nullptr};
  int errorTransCode{0};
  int lastTransitionCode{0};
  int lastTransitionTime{0};
  int nextUpdateTime{-1};
  int stateFrame{0};
  int eventId{-1};
  const void* eventData{nullptr};
  bool interruptible{true};
};

template <typename T>
T* idListArrayNew(int count) {
  return count > 0 ? new T[static_cast<std::size_t>(count)] : nullptr;
}

template <typename T>
T* idListArrayResize(T* oldData, int oldCount, int newCount) {
  T* result = idListArrayNew<T>(newCount);
  const int copyCount = oldCount < newCount ? oldCount : newCount;
  for (int i = 0; i < copyCount; ++i) result[i] = oldData[i];
  delete[] oldData;
  return result;
}

class idFSMManager {
public:
  class idFSMStack {
  public:
    bool Add(idFiniteStateMachine* fsm);
    int Num() const { return fsms.Num(); }
    idFiniteStateMachine* operator[](int index) const { return fsms[index]; }
  private:
    idList<idFiniteStateMachine*, 5> fsms;
  };

  void SetFSMStack(const idFSMStack& value);
  const idFSMStack& GetFSMStack() const { return stack; }

private:
  idFSMStack stack;
};

extern idCVar ai_oldFSMUpdate;
