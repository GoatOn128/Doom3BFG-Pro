#pragma once

#define WOLFTNO_RETAIL_PHYSICS_ACTOR_GRAVITY_SLOT_ABI
#define WOLFTNO_RETAIL_PHYSICS_ACTOR_CONST_ABI
#define WOLFTNO_RETAIL_TRACE_ASSIGN_ABI
#include "engine/gamelib/physics/physics_dynamicbase.h"
#undef WOLFTNO_RETAIL_TRACE_ASSIGN_ABI
#undef WOLFTNO_RETAIL_PHYSICS_ACTOR_CONST_ABI
#undef WOLFTNO_RETAIL_PHYSICS_ACTOR_GRAVITY_SLOT_ABI

#include <cstddef>
#include <cstdint>

class idPhysics_Actor : public idPhysics_DynamicBase {
public:
  enum actorClipModel_t : int {
    ACTOR_CLIPMODEL_STANDING = 0,
    ACTOR_CLIPMODEL_CROUCHING = 1,
  };

  idPhysics_Actor();
  ~idPhysics_Actor() override;

  void SetClipModel(idClipModel* model, float density, int id,
                    bool freeOld) override;
  idClipModel* GetClipModel(int id) const override;
  int GetNumClipModels() const override;
  void SetMass(float mass_, int id) override;
  float GetMass(int id) const override;
  void SetContents(int contents, int id) override;
  int GetContents(int id) const override;
  void SetClipMask(int mask, int id) override;
  int GetClipMask(int id) const override;
  const idBounds& GetBounds(int id) const override;
  const idBounds& GetAbsBounds(int id) const override;
  const idVec3& GetOrigin(int id) const override;
  const idMat3& GetAxis(int id) const override;
  void SetGravity(const idVec3& gravity) override;
  bool IsPushable(int id) const override;
  void ClipTranslation(trace_t& results, const idVec3& translation,
                       const idClipModel* model) const override;
  void ClipRotation(trace_t& results, const idRotation& rotation,
                    const idClipModel* model) const override;
  int ClipContents(const idClipModel* model, int id) const override;
  void DisableClip() override;
  void EnableClip() override;
  void UnlinkClip() override;
  void LinkClip() override;
  bool EvaluateContacts() override;

  virtual void DisableClip(actorClipModel_t model);
  virtual void EnableClip(actorClipModel_t model);
  virtual void LinkClip(const idVec3& origin, const idMat3& axis);
  const idMat3& GetGravityAxis() const override;

  float GetMasterDeltaYaw() const;
  void SetClipModelAxis();
  void RememberHistorySample();
  idVec3 GetOriginHistory(int time) const;
  idVec3 GetVelocityHistory(int time) const;
  idVec3 GetYawHistory(int time) const;

  static constexpr int HISTORY_SAMPLE_RATE = 128;
  static constexpr int HALF_HISTORY_SAMPLE_RATE = 64;
  static constexpr int HISTORY_SAMPLE_RATE_MASK = 127;
  static constexpr int NOT_HISTORY_SAMPLE_RATE_MASK = ~127;
  static constexpr int NUM_HISTORIES = 16;
  static constexpr int NUM_HISTORIES_MASK = 15;
  static constexpr int MAX_TIME_BACK = 1664;
  static const float SAMPLE_RATE_TO_VELOCITY;

public:
  idClipModel* clipModels[2];
  int clipMasks[2];
  idMat3 clipModelAxis;
  float mass;
  float invMass;
  bool hasMaster;
  std::uint8_t masterPadding[3];
  float masterYaw;
  float masterDeltaYaw;
  bool wasHistoryInitialized;
  std::uint8_t historyPadding[3];
  int lastHistorySaveTime;
  idVec3 originHistory[16];
  float yawHistory[16];

protected:
  static int nextHistoryOffset;
  static int nextHistoryOffsetIncr;

  friend class PhysicsAIValidationAccess;
  friend class PhysicsActorValidationAccess;
};

static_assert(sizeof(idPhysics_Actor) == 0x1F0,
              "idPhysics_Actor ABI drift");
static_assert(offsetof(idPhysics_Actor, clipModels) == 0x98,
              "idPhysics_Actor clip-model offset drift");
static_assert(offsetof(idPhysics_Actor, clipMasks) == 0xA8,
              "idPhysics_Actor clip-mask offset drift");
static_assert(offsetof(idPhysics_Actor, clipModelAxis) == 0xB0,
              "idPhysics_Actor clip-axis offset drift");
static_assert(offsetof(idPhysics_Actor, mass) == 0xD4,
              "idPhysics_Actor mass offset drift");
static_assert(offsetof(idPhysics_Actor, invMass) == 0xD8,
              "idPhysics_Actor inverse-mass offset drift");
static_assert(offsetof(idPhysics_Actor, hasMaster) == 0xDC,
              "idPhysics_Actor master flag offset drift");
static_assert(offsetof(idPhysics_Actor, originHistory) == 0xF0,
              "idPhysics_Actor origin-history offset drift");
static_assert(offsetof(idPhysics_Actor, yawHistory) == 0x1B0,
              "idPhysics_Actor yaw-history offset drift");
