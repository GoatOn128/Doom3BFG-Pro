#pragma once

#include "engine/cm/collisionmodel.h"
#include "shared/idlib/text/str.h"

class idMaterial;

class idCollisionModelManager {
public:
  idCollisionModel* ModelFromTrm(const char* name,
                                 const idTraceModel& model,
                                 const idMaterial* material);
};

extern idCollisionModelManager* collisionModelManager;

class idMapFile {
public:
  static idStr GetMapModelName(const char* mapName,
                               const char* entityName);
};
