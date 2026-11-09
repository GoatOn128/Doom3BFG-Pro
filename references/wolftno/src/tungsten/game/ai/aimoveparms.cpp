#include "tungsten/game/ai/aimoveparms.h"

namespace {

idVec3 EntityOrigin(const idEntity* entity) {
  return entity != nullptr && entity->GetPhysics() != nullptr
             ? entity->GetPhysics()->GetOrigin()
             : idVec3{};
}

}  // namespace

// DIA: idAIMoveParms::GetRelevantGoalOrigin
const idVec3& idAIMoveParms::GetRelevantGoalOrigin(const idAI2* ai) const {
  (void)ai;
  if (goalEntity != nullptr && goalEntity->GetPhysics() != nullptr) {
    return goalEntity->GetPhysics()->GetOrigin();
  }
  return goalOrigin;
}

// DIA: idAIMoveParms::GetRelevantGoalDirection
idVec3 idAIMoveParms::GetRelevantGoalDirection(const idAI2* ai) const {
  if (ai == nullptr) return {1.0F, 0.0F, 0.0F};
  const idVec3 aiOrigin = EntityOrigin(ai);
  if (ai->GetMoveFocusEntity() != nullptr) {
    idVec3 direction = EntityOrigin(ai->GetMoveFocusEntity()) - aiOrigin;
    direction.Normalize();
    return direction;
  }
  if (ai->HasExplicitMoveFocusPoint()) {
    idVec3 direction = ai->GetExplicitMoveFocusPoint() - aiOrigin;
    direction.Normalize();
    return direction;
  }
  if (goalScenePoint != nullptr) {
    const idMat3* axis =
        static_cast<const idEntity*>(goalScenePoint)->GetPresentableAxis();
    if (axis != nullptr) return (*axis)[0];
  }
  const idMat3* axis = ai->GetPresentableAxis();
  return axis != nullptr ? (*axis)[0] : idVec3{1.0F, 0.0F, 0.0F};
}

// DIA: idAIMoveParms::idAIMoveParms
idAIMoveParms::idAIMoveParms() {
  goalOrientation.w = 1.0F;
}

// DIA: idAIMoveParms::Init
void idAIMoveParms::Init(
    aiMoveReason_t reason, const idVec3& start, const idVec3& goal,
    const idEntity* newGoalEntity, const idQuat& orientation,
    const idScenePoint* newGoalScenePoint, float newGoalTolerance,
    float newIntermediateTolerance, aiArrivalAction_t newArrivalAction,
    posture_t newArrivalPosture, int newFlags, idAnimWebIndex newSourceSubWeb,
    idAnimWebIndex newSourceState, idAnimWebIndex newDestinationSubWeb,
    idAnimWebIndex newDestinationState) {
  moveReason = reason;
  startOrigin = start;
  goalOrigin = goal;
  goalEntity = newGoalEntity;
  goalOrientation = orientation;
  goalScenePoint = const_cast<idScenePoint*>(newGoalScenePoint);
  goalTolerance = newGoalTolerance;
  intermediateTolerance = newIntermediateTolerance;
  arrivalAction = newArrivalAction;
  arrivalPosture = newArrivalPosture;
  flags = newFlags;
  sourceSubWeb = newSourceSubWeb;
  sourceState = newSourceState;
  destinationSubWeb = newDestinationSubWeb;
  destinationState = newDestinationState;
}
