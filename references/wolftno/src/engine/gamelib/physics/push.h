#pragma once

#include "engine/gamelib/physics/physics.h"
#include "shared/idlib/containers/list.h"

class idPush {
public:
  explicit idPush(idClip* clip_) : clip(clip_), pushed() {}

  float ClipTranslationalPush(trace_t& results, idPhysics* pusher, int flags,
                              const idVec3& newOrigin,
                              const idVec3& translation);
  float ClipRotationalPush(trace_t& results, idPhysics* pusher, int flags,
                           const idMat3& newAxis,
                           const idRotation& rotation);
  float ClipPush(trace_t& results, idPhysics* pusher, int flags,
                 const idVec3& oldOrigin, const idMat3& oldAxis,
                 idVec3& newOrigin, idMat3& newAxis);

  void InitSavingPushedPhysicsObjectState();
  void RestorePushedPhysicsObjectState();
  void SetPushedOnPushedPhysicsObjects(int deltaTime);
  int GetNumPushedPhysicsObjects() const { return pushed.num; }
  idPhysics* GetPushedPhysicsObject(int index) const {
    return pushed.list[index];
  }

private:
  int GetPhysicsObjectsTouchingBounds(idPhysics** const physicsObjects,
                                      const idBounds& bounds, int flags,
                                      idPhysics* pusher);
  bool RotatePhysicsObjectToAxial(idPhysics* physics,
                                  const idVec3& rotationPoint,
                                  const idClipModel* clipModel);
  int RecursiveTryRotatePushPhysicsObject(trace_t& results, trace_t& trace,
                                          idPhysics* pusher, int flags,
                                          const idRotation& rotation,
                                          float& mass);
  int RecursiveTryTranslatePushPhysicsObject(trace_t& results, trace_t& trace,
                                             idPhysics* pusher, int flags,
                                             const idVec3& translation,
                                             float& mass);
  void SavePhysicsObjectState(idPhysics* physics);
  int TryRotatePushPhysicsObject(trace_t& results, idPhysics* check,
                                 idClipModel* clipModel, int flags,
                                 const idVec3& newOrigin,
                                 const idMat3& newAxis,
                                 const idRotation& rotation, float& mass);
  int TryTranslatePushPhysicsObject(trace_t& results, idPhysics* check,
                                    idClipModel* clipModel, int flags,
                                    const idVec3& newOrigin,
                                    const idVec3& translation, float& mass);

  idClip* clip;
  idList<idPhysics*, TAG_PUSH> pushed;
};

static_assert(sizeof(idPush) == 0x20, "idPush ABI drift");
