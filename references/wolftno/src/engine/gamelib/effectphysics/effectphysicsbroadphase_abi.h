#pragma once

#include "engine/gamelib/effectphysics/effectphysicsbroadphase.h"

class idCollisionModel;
struct idJointMat;

class idCollisionModelManager {
public:
  bool GetStepMoveResult(trace_t* result, idCollisionQuery& query, bool peek);
  idCollisionQuery Motion(
      trace_t* result, const idVec3& start, const idVec3& end,
      const idRotation& rotation, const idBounds& globalBounds,
      const idTraceModel** const traceModels, int numTraceModels,
      const idMat3& traceModelAxis, int contentsMask,
      const idPositionedCollisionModel* models, int numModels,
      const char* userName);
  void DrawCollisionModel(idCollisionModel* model,
                          const idJointMat* modelJoints,
                          const unsigned char* sphereActives,
                          const idVec3& modelOrigin,
                          const idMat3& modelAxis,
                          const idVec3& viewOrigin,
                          const idMat3& viewAxis, float radius, float scale,
                          int lifeTime);
};

extern idCollisionModelManager* collisionModelManager;
