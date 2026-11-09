#pragma once

#include "tungsten/game/ai/aievents/aievent.h"

struct idAIEventTraceResult {
  float fraction{1.0F};
  int entityNumber{-1};
};

class idAIEventTrace : public idAIEvent {
public:
  using TraceQuery = idAIEventTraceResult (*)(const idVec3& start,
                                               const idVec3& end,
                                               const idBounds& bounds,
                                               const idEntity* ignoredEntity);

  idAIEventTrace();
  const idBounds& GetBounds() const;
  virtual float GetDistance() const;
  void InternalDrawDebug(int level, int currentTime, int duration);
  bool InternalIsTouching(const idEntity* entity, int currentTime);

  const idVec3& GetDirection() const { return direction; }
  void SetDirection(const idVec3& value) { direction = value; }
  const idAIEventTraceResult& GetLastTraceResult() const {
    return lastTraceResult;
  }
  const idVec3& GetLastDebugEnd() const { return lastDebugEnd; }
  const idVec3& GetLastDebugCenter() const { return lastDebugCenter; }
  int GetLastDebugDuration() const { return lastDebugDuration; }
  int GetDebugPrimitiveCount() const { return debugPrimitiveCount; }
  static void SetTraceQuery(TraceQuery value) { traceQuery = value; }

protected:
  idVec3 direction{};

private:
  int lastTraceFrame{-1};
  idAIEventTraceResult lastTraceResult{};
  idVec3 lastDebugEnd{};
  idVec3 lastDebugCenter{};
  int lastDebugDuration{0};
  int debugPrimitiveCount{0};
  inline static TraceQuery traceQuery{nullptr};
};

class idAIEventProjectile : public idAIEventTrace {
public:
  idAIEventProjectile();
  aiEventUpdateResult_t InternalUpdateAttached(int currentTime) override;
  float GetDistance() const override;
  void SetVelocity(const idVec3& value) { velocity = value; }
  const idVec3& GetVelocity() const { return velocity; }

private:
  idVec3 velocity{};
};
