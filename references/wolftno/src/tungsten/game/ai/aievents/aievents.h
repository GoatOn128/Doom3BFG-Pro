#pragma once

#include "tungsten/game/ai/aievents/aieventprojectile.h"
#include "tungsten/game/ai/ai2_actions.h"

class idAIEventWedge : public idAIEvent {
public:
  idAIEventWedge();
  ~idAIEventWedge() override;
  aiEventUpdateResult_t InternalUpdateAttached(int currentTime) override;
  void Cache(int currentTime);
  bool InternalIsTouching(idEntity* entity, int currentTime);
  void InternalDrawDebug(int level, int currentTime, int duration);

  float GetRadius() const { return radius; }
  void SetRadius(float value) { radius = value; }
  const idVec3& GetDirection() const { return direction; }
  void SetDirection(const idVec3& value) {
    direction = value;
    lastRefresh = -1;
  }
  int GetLastRefresh() const { return lastRefresh; }
  const idPlane& GetPlane(int index) const { return planes[index]; }
  int GetDebugPrimitiveCount() const { return debugPrimitiveCount; }
  int GetDebugDuration() const { return debugDuration; }

protected:
  idVec3 direction{};
  float radius{0.0F};

private:
  int lastRefresh{-1};
  idMat3 axis{};
  idPlane planes[4]{};
  int debugPrimitiveCount{0};
  int debugDuration{0};
};

class idVehicle : public idEntity {
public:
  void SetDriver(idEntity* value) { driver = value; }
  idEntity* GetDriver() const { return driver; }
  void SetVelocity(const idVec3& value) { velocity = value; }
  const idVec3& GetVelocity() const { return velocity; }
  idVec3 GetLinearVelocity() const override { return velocity; }
  void SetProvidesDynamicCover(bool value) { provideDynamicCover = value; }
  bool ProvidesDynamicCover() const override { return provideDynamicCover; }

private:
  idEntity* driver{nullptr};
  idVec3 velocity{};
  bool provideDynamicCover{false};
};

class idAIEvent_Vehicle : public idAIEventWedge {
public:
  idAIEvent_Vehicle();
  aiEventUpdateResult_t InternalUpdateAttached(int currentTime) override;
};

class idAIEvent_PlayerAim : public idAIEventTrace {
public:
  idAIEvent_PlayerAim();
  float GetDistance() const;
  aiEventUpdateResult_t InternalUpdateAttached(int currentTime) override;
};
