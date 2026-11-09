#include "tungsten/game/ai/aivisibility.h"

#include <algorithm>

idCVar ai_showVisTrace("ai_showVisTrace", "0", 2,
                       "1 to show visibility traces, 2 only against player");

namespace {

constexpr int kVisibilityMask = 3;
constexpr int kVisibilityMaskWithBreakables = 8195;

idVec3 ProjectOntoPlane(idVec3 vector, const idVec3& normal) {
  vector -= normal * (vector * normal);
  vector.Normalize();
  return vector;
}

float FovScore(float dot, float cosineFov) {
  if (dot <= cosineFov || cosineFov >= 1.0F) return 0.0F;
  return (dot - cosineFov) / (1.0F - cosineFov);
}

int VisibilityMask(bool seeThroughBreakables) {
  return seeThroughBreakables ? kVisibilityMask
                              : kVisibilityMaskWithBreakables;
}

}  // namespace

// DIA: idAIVisibility::TraceVisible_Submit
void idAIVisibility::TraceVisible_Submit(const idVec3& fromPoint,
                                         const idVec3& toPoint,
                                         idSpawnId ignoreId,
                                         bool seeThroughBreakables,
                                         idClipQuery* clipQuery) {
  if (clipQuery == nullptr) return;
  clipQuery->index =
      world != nullptr
          ? world->SubmitTrace(fromPoint, toPoint,
                               VisibilityMask(seeThroughBreakables),
                               ignoreId.GetEntityNumber())
          : 0;
}

// DIA: idAIVisibility::ByteToFOVDot
float idAIVisibility::ByteToFOVDot(std::uint8_t fov) {
  return static_cast<float>(fov) * 0.0078431377F - 1.0F;
}

// DIA: idAIVisibility::CheckFOV
float idAIVisibility::CheckFOV(const idVec3& direction,
                               const idVec3& eyeForward, float cosineFov) {
  return FovScore(direction * eyeForward, cosineFov);
}

// DIA: idAIVisibility::CheckFOV
float idAIVisibility::CheckFOV(const idVec3& direction,
                               const idVec3& eyeForward,
                               float cosineHorizontalFov,
                               float cosineVerticalFov) {
  static const idVec3 worldUp{0.0F, 0.0F, 1.0F};
  idVec3 eyeRight = eyeForward.Cross(worldUp);
  if (eyeRight.Normalize() == 0.0F) eyeRight = {0.0F, 1.0F, 0.0F};

  const idVec3 verticalDirection = ProjectOntoPlane(direction, eyeRight);
  const float verticalDot = verticalDirection * eyeForward;
  if (verticalDot < cosineVerticalFov) return 0.0F;

  const idVec3 horizontalDirection = ProjectOntoPlane(direction, worldUp);
  const idVec3 horizontalForward = ProjectOntoPlane(eyeForward, worldUp);
  const float horizontalDot = horizontalDirection * horizontalForward;
  if (horizontalDot >= cosineHorizontalFov) {
    return FovScore(horizontalDot, cosineHorizontalFov);
  }
  return FovScore(verticalDot, cosineVerticalFov);
}

bool idAIVisibility::IsVisibleHit(const idAIGameState* gameState,
                                  idSpawnId destinationId,
                                  const trace_t& trace) {
  if (trace.fraction >= 1.0F) return true;
  if (!destinationId.IsValid()) return false;
  const int hitEntity = trace.contact.entityNum;
  if (hitEntity == destinationId.GetEntityNumber()) return true;
  return gameState != nullptr &&
         hitEntity == gameState->GetMaster(destinationId).GetEntityNumber();
}

// DIA: idAIVisibility::TraceVisible
bool idAIVisibility::TraceVisible(idAIGameState* gameState,
                                  const idVec3& fromPoint,
                                  const idVec3& toPoint, idSpawnId ignoreId,
                                  idSpawnId destinationId, trace_t* trace,
                                  bool seeThroughBreakables) {
  if (trace == nullptr) return false;
  if (world == nullptr) {
    trace->fraction = 0.0F;
    trace->endpos = fromPoint;
    return false;
  }
  world->Trace(fromPoint, toPoint, VisibilityMask(seeThroughBreakables),
               ignoreId.GetEntityNumber(), *trace);
  return IsVisibleHit(gameState, destinationId, *trace);
}

// DIA: idAIVisibility::FOVDotToByte
std::uint8_t idAIVisibility::FOVDotToByte(float fov) {
  const int encoded = static_cast<int>((fov + 1.0F) * 127.5F);
  return static_cast<std::uint8_t>(std::clamp(encoded, 0, 255));
}

// DIA: idAIVisibility::TraceVisible_Resolve
bool idAIVisibility::TraceVisible_Resolve(idAIGameState* gameState,
                                          idSpawnId destinationId,
                                          idClipQuery* clipQuery,
                                          trace_t* trace) {
  if (clipQuery == nullptr || trace == nullptr || world == nullptr ||
      clipQuery->index == 0 ||
      !world->ResolveTrace(clipQuery->index, *trace)) {
    if (trace != nullptr) trace->fraction = 0.0F;
    return false;
  }
  clipQuery->index = 0;
  return IsVisibleHit(gameState, destinationId, *trace);
}

// DIA: `dynamic initializer for 'ai_showVisTrace''

// DIA: `dynamic atexit destructor for 'ai_showVisTrace''
