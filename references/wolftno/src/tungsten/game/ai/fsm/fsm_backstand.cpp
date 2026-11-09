#include "tungsten/game/ai/fsm/fsm_backstand.h"

#include <cfloat>

#define WOLF_DEFINE_BACKSTAND_EVALUATOR(Name, Field, Code) \
  int Name::Evaluate(const idBackStandContext& context) const { \
    return context.Field ? Code : 0; \
  }
WOLF_BACKSTAND_EVALUATORS(WOLF_DEFINE_BACKSTAND_EVALUATOR)
#undef WOLF_DEFINE_BACKSTAND_EVALUATOR

#define WOLF_BACKSTAND_INIT(Name) \
  void idBackStand_##Name::InternalInitState() { InitState(); }
WOLF_BACKSTAND_INIT(Default)
WOLF_BACKSTAND_INIT(Moving)
WOLF_BACKSTAND_INIT(OutOfReach)
WOLF_BACKSTAND_INIT(Rising)
WOLF_BACKSTAND_INIT(Standing)
WOLF_BACKSTAND_INIT(Attacking)
WOLF_BACKSTAND_INIT(Lowering)
#undef WOLF_BACKSTAND_INIT

void idBackStand_Default::AIState_EnterState(idAIFSM*, unsigned int time) { RecordEnter(time); }
void idBackStand_OutOfReach::AIState_EnterState(idAIFSM*, unsigned int time) { RecordEnter(time); }
void idBackStand_Attacking::AIState_EnterState(idAIFSM*, unsigned int time) { RecordEnter(time); }
void idBackStand_Moving::AIState_EnterState(idAIFSM*, unsigned int time) { RecordEnter(time); }
void idBackStand_Moving::AIState_Work(idAIFSM*, unsigned int time) { RecordWork(time); }
void idBackStand_Rising::AIState_EnterState(idAIFSM*, unsigned int time) { RecordEnter(time); }
void idBackStand_Rising::AIState_Work(idAIFSM*, unsigned int time) { RecordWork(time); }
void idBackStand_Lowering::AIState_EnterState(idAIFSM*, unsigned int time) { RecordEnter(time); }

float EntityPlaneDistance(const idEntity* left, const idEntity* right) {
  if (left == nullptr || right == nullptr || left->GetPhysics() == nullptr ||
      right->GetPhysics() == nullptr)
    return FLT_MAX;
  const idVec3 delta = left->GetPhysics()->GetOrigin() -
                       right->GetPhysics()->GetOrigin();
  return idVec2{delta.x, delta.y}.Length();
}

idVec3 GetHintRisePosition(const idEntity* actor, const idEntity* hint) {
  if (hint == nullptr || hint->GetPhysics() == nullptr) return {};
  const idVec3 hintOrigin = hint->GetPhysics()->GetOrigin();
  const idMat3& axis = hint->GetPhysics()->GetAxis();
  const float actorRadius = actor != nullptr && actor->GetPhysics() != nullptr
                                ? actor->GetPhysics()->GetAbsBounds().GetRadiusXY()
                                : 24.0F;
  return hintOrigin + axis[0] * actorRadius;
}

idVec2 PredictedEnemyPlanePosition(const idEntity* enemy, float time) {
  if (enemy == nullptr || enemy->GetPhysics() == nullptr) return {};
  const idVec3 predicted = enemy->GetPhysics()->GetOrigin() +
                           enemy->GetLinearVelocity() * time;
  return {predicted.x, predicted.y};
}

idEntity* FindBestHint(const idBackStandContext& context) {
  idEntity* best = nullptr;
  float bestDistance = FLT_MAX;
  for (int i = 0; i < context.hintCount && i < static_cast<int>(context.hints.size()); ++i) {
    idEntity* hint = context.hints[static_cast<std::size_t>(i)];
    const float distance = EntityPlaneDistance(context.actor, hint);
    if (distance < bestDistance) { best = hint; bestDistance = distance; }
  }
  return best;
}

int GetEnemySide(const idEntity* actor, const idEntity* enemy) {
  if (actor == nullptr || enemy == nullptr || actor->GetPhysics() == nullptr ||
      enemy->GetPhysics() == nullptr)
    return 1;
  const idVec3 delta = enemy->GetPhysics()->GetOrigin() -
                       actor->GetPhysics()->GetOrigin();
  return delta * actor->GetPhysics()->GetAxis()[1] < 0.0F ? -1 : 1;
}

idBackStandFSM::idBackStandFSM() { SetIdentity("backStand", 200); }

void idBackStandFSM::InitStates(const idFiniteStateMachineParams* params) {
  idAIFSM::InitStates(params);
  AddState(&stateDefault); AddState(&stateMoving); AddState(&stateOutOfReach);
  AddState(&stateRising); AddState(&stateStanding); AddState(&stateAttacking);
  AddState(&stateLowering);
  SetDefaultState(&stateDefault);
}

bool idBackStandFSM::CanUse(const idBackStandContext& context, int) const {
  return context.enabled && context.actor != nullptr && context.enemy != nullptr &&
         FindBestHint(context) != nullptr;
}

// Recovered DIA inventory for the complete back-stand graph.
// DIA: SearchScenePoint_Shared_Finished::Evaluate
// DIA: idBackStand_Default::InternalInitState
// DIA: idBackStand_Moving::InternalInitState
// DIA: idBackStand_OutOfReach::InternalInitState
// DIA: idBackStand_Rising::InternalInitState
// DIA: idBackStand_Standing::InternalInitState
// DIA: idBackStand_Attacking::InternalInitState
// DIA: idBackStand_Lowering::InternalInitState
// DIA: BACKSTAND_Error::Evaluate
// DIA: idBackStand_Default::AIState_EnterState
// DIA: idBackStandFSM::InitStates
// DIA: BACKSTAND_MoveDone::Evaluate
// DIA: EntityPlaneDistance
// DIA: GetHintRisePosition
// DIA: idBackStand_Rising::AIState_Work
// DIA: idBackStandFSM::idBackStandFSM
// DIA: `idBackStandFSM::idBackStandFSM'::`1'::dtor$0
// DIA: `idBackStandFSM::idBackStandFSM'::`1'::dtor$1
// DIA: `idBackStandFSM::idBackStandFSM'::`1'::dtor$2
// DIA: `idBackStandFSM::idBackStandFSM'::`1'::dtor$3
// DIA: `idBackStandFSM::idBackStandFSM'::`1'::dtor$4
// DIA: `idBackStandFSM::idBackStandFSM'::`1'::dtor$5
// DIA: `idBackStandFSM::idBackStandFSM'::`1'::dtor$6
// DIA: `idBackStandFSM::idBackStandFSM'::`1'::dtor$7
// DIA: idBackStandFSM::`scalar deleting destructor'
// DIA: PredictedEnemyPlanePosition
// DIA: FindBestHint
// DIA: BACKSTAND_TargetLost::Evaluate
// DIA: BACKSTAND_ShouldReturn::Evaluate
// DIA: BACKSTAND_ShouldRise::Evaluate
// DIA: BACKSTAND_ShouldOutOfReach::Evaluate
// DIA: BACKSTAND_ShouldOutOfReachFromLedge::Evaluate
// DIA: BACKSTAND_ShouldMove::Evaluate
// DIA: idBackStand_OutOfReach::AIState_EnterState
// DIA: GetEnemySide
// DIA: idBackStand_Attacking::AIState_EnterState
// DIA: BACKSTAND_ShouldAttack::Evaluate
// DIA: BACKSTAND_ShouldLower::Evaluate
// DIA: idBackStandFSM::CanUse
// DIA: BACKSTAND_ShouldChangeHint::Evaluate
// DIA: idBackStand_Moving::AIState_EnterState
// DIA: idBackStand_Moving::AIState_Work
// DIA: `idBackStand_Moving::AIState_Work'::`1'::dtor$0
// DIA: `idBackStand_Moving::AIState_Work'::`1'::dtor$1
// DIA: `idBackStand_Moving::AIState_Work'::`1'::dtor$2
// DIA: `idBackStand_Moving::AIState_Work'::`1'::dtor$3
// DIA: `idBackStand_Moving::AIState_Work'::`1'::dtor$4
// DIA: `idBackStand_Moving::AIState_Work'::`1'::dtor$5
// DIA: `idBackStand_Moving::AIState_Work'::`1'::dtor$8
// DIA: `idBackStand_Moving::AIState_Work'::`1'::dtor$9
// DIA: `idBackStand_Moving::AIState_Work'::`1'::dtor$10
// DIA: `idBackStand_Moving::AIState_Work'::`1'::dtor$11
// DIA: `idBackStand_Moving::AIState_Work'::`1'::dtor$12
// DIA: `idBackStand_Moving::AIState_Work'::`1'::dtor$13
// DIA: idBackStand_Rising::AIState_EnterState
// DIA: idBackStand_Lowering::AIState_EnterState
