#pragma once

#include "tungsten/game/ai/ai2_actions.h"

#include <cstdint>

class idAIScriptedAnimWeb : public idEntity {
public:
  struct animWebState_t {
    idStr destAnimRef;
    idStr viaAnimRef;
    idStr exitFastRef;
    std::int8_t randomState{-1};
  };

  struct animState_t {
    void SetcurrentState(int state);
    int GetRandomState();
    void GotoNextState();
    void SetAnimWebStates(idList<animWebState_t, 5>* states);

    void SetRandomSeed(std::uint32_t value) { randomSeed = value; }

    idList<animWebState_t, 5>* animWebStates{nullptr};
    idList<std::uint16_t, 59> randomStates;
    int currentState{-1};
    std::uint32_t randomSeed{0x12345678u};
  };

  void PostSpawn() override;
  void Spawn();
  void StartAIAction(idEntity* activator);
  void OnActivate(idEntity* activator) override;
  eventVoid* Event_SetScriptedAnimWebState(eventVoid* result, int state);

  void AddTarget(idEntity* target) { targets.Append(target); }
  int AddState(const animWebState_t& state) { return animWebStates.Append(state); }
  animState_t& GetStateInfo() { return stateInfo; }
  const animState_t& GetStateInfo() const { return stateInfo; }
  idList<animWebState_t, 5>& GetStates() { return animWebStates; }
  int GetValidationErrorCount() const { return validationErrors.Num(); }
  int GetNonAITargetCount() const { return nonAITargetCount; }
  const idStr& GetValidationError(int index) const {
    return validationErrors[index];
  }

private:
  bool ValidateAWebPath(idAI2* ai, const idStr& path);

  idList<animWebState_t, 5> animWebStates;
  animState_t stateInfo;
  idList<idEntity*, 5> targets;
  idList<idStr, 5> validationErrors;
  int nonAITargetCount{0};
};
