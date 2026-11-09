#pragma once

#include "shared/idlib/math/vector.h"

class idDeclEnv;
class idDeclRenderParm;
class idDeclTable;
class idRenderModel;

class idViewCallbacks {
public:
  virtual ~idViewCallbacks() = default;
  virtual const char* GetName() = 0;
  virtual int GetViewID() = 0;
  virtual void GetPosition(idVec3& position, idMat3& axis) = 0;
  virtual void GetViewPosition(idVec3& position, idMat3& axis) = 0;
  virtual void ApplyScreenShake(float magnitude, float scale,
                                const idAngles& angles,
                                const idVec3& offset) = 0;
  virtual void ApplyControllerShake(int highMagnitude, int lowMagnitude,
                                    float highDuration, float lowDuration,
                                    const idDeclTable* amplitudeTable) = 0;
  virtual void ApplyDynEnvOverride(const idDeclEnv* environment,
                                   float fadeTime) = 0;
  virtual void ApplyDynEnvParmOverride(const idDeclRenderParm* parameter,
                                       const idVec4& value, bool clear,
                                       float fadeTime) = 0;
  virtual void ApplyRadialBlur(const idVec3& position, float magnitude,
                               float radius, float duration,
                               float falloff, float scale) = 0;
  virtual int AddScreenParticle(idRenderModel* model, float angle) = 0;
  virtual void RemoveScreenParticle(int handle, int fadeTime) = 0;
};
