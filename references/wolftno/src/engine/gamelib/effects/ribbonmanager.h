#pragma once

#include "engine/gamelib/effects/ribbonmodelmanager_abi.h"
#include "shared/idlib/math/vector.h"

#include <cstddef>

class idColor;
class idDeclRibbon;
class idRenderModelBeam;
class idRibbon;
class idSerializer;

class idRibbonManager {
public:
  idRibbonManager();
  ~idRibbonManager();

  void Init(const idDeclRibbon* ribbonDecl, idRenderModelBeam* beamModel,
            idRibbonModelManager* modelManager);
  void Shutdown();
  bool IsInitialized() { return isInitialized; }
  bool IsActive();
  void StartRibbon(const int spawnTime, const idVec3& spawnOrigin);
  void StopRibbon();
  bool UpdateRibbon(const int time, const idVec3& origin, const idMat3& axis,
                    const idVec3& velocity, const idVec4& color,
                    const idVec3& translate);
  int GetRemainingTime(const int currentTime) const;

  idRibbon* ribbonMustBeNullDontEverUse;
  ribbonHandle_t ribbonHandle;
  const idDeclRibbon* ribbonDecl;
  idRenderModelBeam* beamModel;
  idRibbonModelManager* modelManager;
  bool isInitialized;
};

static_assert(sizeof(idRibbonManager) == 0x38,
              "ribbon manager ABI drift");
static_assert(offsetof(idRibbonManager, ribbonHandle) == 0x8,
              "ribbon manager handle offset drift");
static_assert(offsetof(idRibbonManager, ribbonDecl) == 0x18,
              "ribbon manager declaration offset drift");
static_assert(offsetof(idRibbonManager, beamModel) == 0x20,
              "ribbon manager beam-model offset drift");
static_assert(offsetof(idRibbonManager, modelManager) == 0x28,
              "ribbon manager model-manager offset drift");
static_assert(offsetof(idRibbonManager, isInitialized) == 0x30,
              "ribbon manager initialized offset drift");
