#pragma once

#include "tungsten/game/ai/aievents/aievent.h"

class idTrigger_EventListener : public idEntity {};

class idAIEventSphere : public idAIEvent {
public:
  idAIEventSphere();
  float GetRadius() const;
  bool InternalIsTouching(idEntity* entity, int currentTime) const;
  void InternalDrawDebug(int level, int currentTime, int duration);
  int GetDebugDrawCount() const { return debugDrawCount; }

private:
  int debugDrawCount{0};
};

class idAIEventProjectedSphere : public idAIEventSphere {
public:
  aiEventUpdateResult_t InternalUpdateAttached(int currentTime) override;
  void SetProjectionVelocity(const idVec3& value) { simulationVelocity = value; }
  bool HasProjection() const { return hasProjection; }
  const idVec3& GetProjectedOrigin() const { return projectedOrigin; }

private:
  idVec3 simulationPosition{};
  idVec3 simulationVelocity{};
  idVec3 projectedOrigin{};
  int lastSimulationTime{-1};
  int bounces{0};
  bool hasProjection{false};
};

class idAIEventSound : public idAIEventSphere {
public:
  idAIEventSound();
  bool InternalIsTouching(idEntity* entity, int currentTime) const;
  float InternalGetIntensity(const idEntity* entity) const override;
  void SetOcclusion(float value) { occlusion = std::clamp(value, 0.0F, 1.0F); }

private:
  float occlusion{0.0F};
};
