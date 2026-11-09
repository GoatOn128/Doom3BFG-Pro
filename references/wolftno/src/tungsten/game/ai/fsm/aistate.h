#pragma once

#include "tungsten/game/ai/fsm/aifsm.h"

#include <string>

class idAIStateTransition : public idStateTransition {
public:
  using aiTransCode_t = int;
  virtual ~idAIStateTransition() = default;
  aiTransCode_t Evaluate(idAIFSM* parentFSM, idAIState* state,
                         int currentTime);
  virtual aiTransCode_t EvaluateForAI(idAI2* ai, idAIFSM* parentFSM,
                                      idAIState* state, int currentTime) = 0;
  static const char* NameForTransitionCode(aiTransCode_t code);
};

class idAIStateTransitionStatic : public idAIStateTransition {
public:
  using transDest_t = idStateTransition::transDest_t;

  aiTransCode_t EvaluateForAI(idAI2*, idAIFSM*, idAIState*, int) override {
    return destinations.Num() > 0 ? destinations[0].transCode : 0;
  }
  void AddDestination(const char* stateType, aiTransCode_t code) {
    destinations.Append({stateType, code});
  }
  void InternalSave(std::string* output, const char* indent,
                    const idList<transDest_t, 5>* dests) const override;
  const idList<transDest_t, 5>& GetDestinations() const { return destinations; }

private:
  idList<transDest_t, 5> destinations;
};
