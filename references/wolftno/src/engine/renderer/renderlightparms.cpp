#include "engine/renderer/renderlightparms.h"

#include "engine/renderer/renderlocal.h"
#include "shared/idlib/lib.h"
#include "shared/idlib/math/mathlib.h"

#include <algorithm>
#include <cmath>
#include <cstring>

#if defined(WOLF_RENDERER_RENDERLIGHTPARMS_TEST_ACCESS)
extern idRenderSystemLocal* rendererRenderLightParmsTestLocal;
extern void RenderLightParmsTestCreateFromOriginAxis(
    const idVec3& origin, const idMat3& axis, idRenderMatrix& result);
extern bool RenderLightParmsTestInverse(const idRenderMatrix& input,
                                        idRenderMatrix& result);
extern void RenderLightParmsTestMultiply(const idRenderMatrix& left,
                                         const idRenderMatrix& right,
                                         idRenderMatrix& result);
extern void RenderLightParmsTestWarning(const char* message);
#define RENDER_LIGHT_LOCAL (*rendererRenderLightParmsTestLocal)
#define RENDER_LIGHT_CREATE_FROM_ORIGIN_AXIS(origin, axis, result) \
  RenderLightParmsTestCreateFromOriginAxis(origin, axis, result)
#define RENDER_LIGHT_INVERSE(input, result) \
  RenderLightParmsTestInverse(input, result)
#define RENDER_LIGHT_MULTIPLY(left, right, result) \
  RenderLightParmsTestMultiply(left, right, result)
#define RENDER_LIGHT_WARNING(message) RenderLightParmsTestWarning(message)
#else
#define RENDER_LIGHT_LOCAL tr
#define RENDER_LIGHT_CREATE_FROM_ORIGIN_AXIS(origin, axis, result) \
  idRenderMatrix::CreateFromOriginAxis(origin, axis, result)
#define RENDER_LIGHT_INVERSE(input, result) \
  idRenderMatrix::Inverse(input, result)
#define RENDER_LIGHT_MULTIPLY(left, right, result) \
  idRenderMatrix::Multiply(left, right, result)
#define RENDER_LIGHT_WARNING(message) idLib::Warning(message)
#endif

const idMaterial* idRenderLightParms::DefaultMaterial() const {
  if (lightClass == LIGHT_BLENDED_ONLY) {
    if (lightType == LIGHT_POINT)
      return RENDER_LIGHT_LOCAL.mtrDefaultPointBlendLight;
    if (lightType == LIGHT_PARALLEL)
      return RENDER_LIGHT_LOCAL.mtrDefaultParallelBlendLight;
    return RENDER_LIGHT_LOCAL.mtrDefaultProjectedBlendLight;
  }

  if (lightClass == LIGHT_SLOWLIGHT_DYNAMIC_ONLY ||
      lightClass == LIGHT_SLOWLIGHT_PRIVATE_ONLY) {
    if (lightType == LIGHT_POINT)
      return RENDER_LIGHT_LOCAL.mtrDefaultPointSlowLight;
    if (lightType == LIGHT_PARALLEL)
      return RENDER_LIGHT_LOCAL.mtrDefaultParallelSlowLight;
    return RENDER_LIGHT_LOCAL.mtrDefaultProjectedSlowLight;
  }

  if (lightType == LIGHT_POINT)
    return RENDER_LIGHT_LOCAL.mtrDefaultPointLight;
  if (lightType == LIGHT_PARALLEL)
    return RENDER_LIGHT_LOCAL.mtrDefaultParallelLight;
  return RENDER_LIGHT_LOCAL.mtrDefaultProjectedLight;
}

void idRenderLightParms::ComputeSpotLightProjectionMatrix(
    idRenderMatrix& localProject) const {
  const float targetLengthSqr = target.x * target.x + target.y * target.y +
                                target.z * target.z;
  const float inverseTargetLength =
      1.0f /
      std::sqrt(std::fmax(targetLengthSqr,
                          idMath::FLT_SMALLEST_NON_DENORMAL));
  const idVec3 targetNormal = target * inverseTargetLength;

  const float targetLength = inverseTargetLength * targetLengthSqr;
  const float rightScale =
      (targetLength * 0.5f) /
      (right.x * right.x + right.y * right.y + right.z * right.z);
  const float upScale =
      (targetLength * -0.5f) /
      (up.x * up.x + up.y * up.y + up.z * up.z);

  localProject.m[0] = right.x * rightScale;
  localProject.m[1] = right.y * rightScale;
  localProject.m[2] = right.z * rightScale;
  localProject.m[3] = 0.0f;
  localProject.m[4] = up.x * upScale;
  localProject.m[5] = up.y * upScale;
  localProject.m[6] = up.z * upScale;
  localProject.m[7] = 0.0f;
  localProject.m[12] = targetNormal.x;
  localProject.m[13] = targetNormal.y;
  localProject.m[14] = targetNormal.z;
  localProject.m[15] = 0.0f;

  const float startDistance =
      std::fmax(targetNormal.x * start.x + targetNormal.y * start.y +
                    targetNormal.z * start.z,
                8.0f);
  const float endDistance =
      std::fmax(targetNormal.x * end.x + targetNormal.y * end.y +
                    targetNormal.z * end.z,
                16.0f);
  const float depthScale = (endDistance + startDistance) / endDistance;
  localProject.m[8] = targetNormal.x * depthScale;
  localProject.m[9] = targetNormal.y * depthScale;
  localProject.m[10] = targetNormal.z * depthScale;
  localProject.m[11] = -(depthScale * startDistance);

  const float targetW = target.x * localProject.m[12] +
                        target.y * localProject.m[13] +
                        target.z * localProject.m[14] + localProject.m[15];
  const float projectedX =
      (target.x * localProject.m[0] + target.y * localProject.m[1] +
       target.z * localProject.m[2] + localProject.m[3]) /
      targetW;
  const float projectedY =
      (target.x * localProject.m[4] + target.y * localProject.m[5] +
       target.z * localProject.m[6] + localProject.m[7]) /
      targetW;

  const float offsetX = 0.5f - projectedX;
  localProject.m[0] += localProject.m[12] * offsetX;
  localProject.m[1] += localProject.m[13] * offsetX;
  localProject.m[2] += localProject.m[14] * offsetX;
  localProject.m[3] += localProject.m[15] * offsetX;

  const float offsetY = 0.5f - projectedY;
  localProject.m[4] += localProject.m[12] * offsetY;
  localProject.m[5] += localProject.m[13] * offsetY;
  localProject.m[6] += localProject.m[14] * offsetY;
  localProject.m[7] += localProject.m[15] * offsetY;
}

void idRenderLightParms::ComputeWorldToLightMatrix(
    idRenderMatrix& baseLightProject,
    idRenderMatrix& inverseBaseLightProject) const {
  idRenderMatrix localProject;
  if (lightType == LIGHT_SPOT) {
    ComputeSpotLightProjectionMatrix(localProject);
  } else {
    std::memset(&localProject, 0, sizeof(localProject));
    localProject.m[3] = 0.5f;
    localProject.m[7] = 0.5f;
    localProject.m[11] = 0.5f;
    localProject.m[0] = 0.5f / lightRadius.x;
    localProject.m[5] = 0.5f / lightRadius.y;
    localProject.m[10] = 0.5f / lightRadius.z;
    localProject.m[15] = 1.0f;
  }

  idRenderMatrix lightMatrix;
  idRenderMatrix inverseLightMatrix;
  RENDER_LIGHT_CREATE_FROM_ORIGIN_AXIS(origin, axis, lightMatrix);
  if (!RENDER_LIGHT_INVERSE(lightMatrix, inverseLightMatrix))
    RENDER_LIGHT_WARNING("lightMatrix invert failed");
  RENDER_LIGHT_MULTIPLY(localProject, inverseLightMatrix, baseLightProject);
  if (!RENDER_LIGHT_INVERSE(baseLightProject, inverseBaseLightProject))
    RENDER_LIGHT_WARNING("baseLightProject invert failed");
}

void idRenderLightParms::ComputeLightGlobalOrigin(idVec3& globalOrigin) const {
  if (lightType == LIGHT_PARALLEL) {
    const float lengthSqr = lightCenter.x * lightCenter.x +
                            lightCenter.y * lightCenter.y +
                            lightCenter.z * lightCenter.z;
    const float inverseLength =
        1.0f /
        std::sqrt(std::fmax(lengthSqr,
                            idMath::FLT_SMALLEST_NON_DENORMAL));
    idVec3 direction = lightCenter * inverseLength;
    if (inverseLength * lengthSqr == 0.0f)
      direction.z = 1.0f;
    globalOrigin = origin + direction * 100000.0f;
    return;
  }

  if (lightType == LIGHT_SPOT) {
    globalOrigin = origin;
    return;
  }

  globalOrigin.x =
      (lightCenter.y * axis[1].x + lightCenter.x * axis[0].x) +
      lightCenter.z * axis[2].x + origin.x;
  globalOrigin.y =
      (lightCenter.x * axis[0].y + lightCenter.y * axis[1].y) +
      lightCenter.z * axis[2].y + origin.y;
  globalOrigin.z =
      (lightCenter.x * axis[0].z + lightCenter.y * axis[1].z) +
      lightCenter.z * axis[2].z + origin.z;
}

#undef RENDER_LIGHT_LOCAL
#undef RENDER_LIGHT_CREATE_FROM_ORIGIN_AXIS
#undef RENDER_LIGHT_INVERSE
#undef RENDER_LIGHT_MULTIPLY
#undef RENDER_LIGHT_WARNING
