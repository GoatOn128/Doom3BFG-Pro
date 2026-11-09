#include "tungsten/game/ai/aievents/aieventprojectile.h"

namespace {

const idBounds& EmptyBounds() {
  static const idBounds bounds = [] {
    idBounds value{};
    value.b[0] = {1.0e30F, 1.0e30F, 1.0e30F};
    value.b[1] = {-1.0e30F, -1.0e30F, -1.0e30F};
    return value;
  }();
  return bounds;
}

}  // namespace

// DIA: idAIEventTrace::GetBounds
const idBounds& idAIEventTrace::GetBounds() const {
  const idDeclAiEvent* declaration = GetDeclaration();
  return declaration != nullptr ? declaration->bounds : EmptyBounds();
}

// DIA: idAIEventTrace::GetDistance
float idAIEventTrace::GetDistance() const {
  const idDeclAiEvent* declaration = GetDeclaration();
  return declaration != nullptr ? declaration->distance : 0.0F;
}

// DIA: idAIEventTrace::idAIEventTrace
idAIEventTrace::idAIEventTrace() = default;

// DIA: idAIEventTrace::InternalDrawDebug
void idAIEventTrace::InternalDrawDebug(int level, int currentTime,
                                       int duration) {
  (void)currentTime;
  lastDebugDuration =
      duration > 0
          ? duration
          : (gameLocal != nullptr ? gameLocal->GetGameMsPerFrame() : 16);
  const float tracedDistance = GetDistance() * lastTraceResult.fraction;
  lastDebugEnd = GetOrigin() + direction * tracedDistance;
  lastDebugCenter = GetOrigin() + direction * (tracedDistance * 0.5F);
  debugPrimitiveCount = level > 1 ? 3 : 2;
}

// DIA: idAIEventTrace::InternalIsTouching
bool idAIEventTrace::InternalIsTouching(const idEntity* entity,
                                        int currentTime) {
  if (currentTime != lastTraceFrame && direction.LengthSqr() > 0.0F) {
    const idVec3 end = GetOrigin() + direction * GetDistance();
    lastTraceResult = traceQuery != nullptr
                          ? traceQuery(GetOrigin(), end, GetBounds(),
                                       GetOriginator())
                          : idAIEventTraceResult{};
    lastTraceFrame = currentTime;
  }
  return entity != nullptr && lastTraceResult.fraction != 1.0F &&
         lastTraceResult.entityNumber == entity->GetEntityNumber();
}
