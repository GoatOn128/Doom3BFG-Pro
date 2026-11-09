#include "engine/renderer/jobs/render/setupmodelmvp.h"

#include "engine/renderer/render.h"
#include "shared/idlib/math/mathlib.h"

#include <cmath>

namespace {
constexpr idRenderMatrix kIdentity =
    {{1, 0, 0, 0, 0, 1, 0, 0, 0, 0, 1, 0, 0, 0, 0, 1}};

// Bytes read from retail VA 0x1415BCD00 (renderMatrix_flipToOpenGL).
constexpr idRenderMatrix kFlipToOpenGL =
    {{0, -1, 0, 0, 0, 0, 1, 0, -1, 0, 0, 0, 0, 0, 0, 1}};
} // namespace

void SetupModelMVPMatrix(idRenderMatrix* mvpMatrix,
                         const idRenderModelParms* parameters,
                         const idRenderMatrix* modelMatrix,
                         const idRenderView* renderView, float zNear,
                         float zFar, float guiOriginOffset) {
  if (parameters->MvpIsIdentity()) {
    *mvpMatrix = kIdentity;
    return;
  }

  if (parameters->MvpIsOrthographic()) {
    mvpMatrix->m[0] = parameters->scale.x;
    mvpMatrix->m[1] = 0.0f;
    mvpMatrix->m[2] = 0.0f;
    mvpMatrix->m[3] = guiOriginOffset + parameters->origin.x;
    mvpMatrix->m[4] = 0.0f;
    mvpMatrix->m[5] = parameters->scale.y;
    mvpMatrix->m[6] = 0.0f;
    mvpMatrix->m[7] = parameters->origin.y;
    mvpMatrix->m[8] = 0.0f;
    mvpMatrix->m[9] = 0.0f;
    mvpMatrix->m[10] = 0.0f;
    mvpMatrix->m[11] = 0.0f;
    mvpMatrix->m[12] = 0.0f;
    mvpMatrix->m[13] = 0.0f;
    mvpMatrix->m[14] = 0.0f;
    mvpMatrix->m[15] = 1.0f;
    return;
  }

  if (renderView == nullptr) {
    *mvpMatrix = kIdentity;
    return;
  }

  if ((parameters->flags2 & 0x02u) != 0) {
    idRenderMatrix rotationOnlyView = renderView->viewMatrix;
    rotationOnlyView.m[3] = 0.0f;
    rotationOnlyView.m[7] = 0.0f;
    rotationOnlyView.m[11] = 0.0f;

    idVec4 eye{};
    idVec4 clip{};
    idRenderMatrix::TransformModelToClip(
        parameters->origin, rotationOnlyView, renderView->projectionMatrix,
        eye, clip);
    float projectedX = clip.x;
    float projectedY = clip.y;
    float projectedZ = clip.z;
    if (std::fabs(clip.w) > idMath::FLT_SMALLEST_NON_DENORMAL) {
      const float inverseW = 1.0f / clip.w;
      projectedX *= inverseW;
      projectedY *= inverseW;
      projectedZ *= inverseW;
    }

    idRenderMatrix projection{};
    projection.m[0] = parameters->scale.x /
                      static_cast<float>(renderView->windowWidth);
    projection.m[3] = projectedX;
    projection.m[5] = parameters->scale.y /
                      static_cast<float>(renderView->windowHeight);
    projection.m[7] = projectedY;
    projection.m[10] = 1.0f;
    projection.m[11] =
        (projectedZ < -1.0f || projectedZ > 1.0f) ? projectedZ : 0.99989998f;
    projection.m[15] = 1.0f;
    idRenderMatrix::Multiply(projection, kFlipToOpenGL, *mvpMatrix);
    return;
  }

  idRenderMatrix modelViewMatrix{};
  idRenderMatrix::Multiply(renderView->viewMatrix, *modelMatrix,
                           modelViewMatrix);

  const idRenderMatrix* projection = &renderView->projectionMatrix;
  idRenderMatrix scaledProjection{};
  if (parameters->fovScale != 1.0f && !renderView->r.inhibitModelFovScale) {
    float adjustedNear = zNear;
    if (renderView->r.cramZNear)
      adjustedNear *= 0.25f;
    const float tangent = std::tan(parameters->fovScale * renderView->r.fov_y *
                                   idMath::M_DEG2RAD * 0.5f);
    const float yMaximum = tangent * adjustedNear;
    const float xMaximum =
        (static_cast<float>(renderView->renderWidth) /
         static_cast<float>(renderView->renderHeight)) * yMaximum;
    idRenderMatrix::CreateProjectionMatrix(
        -xMaximum, xMaximum, -yMaximum, yMaximum, adjustedNear, zFar,
        scaledProjection);
    projection = &scaledProjection;
  }

  idRenderMatrix::Multiply(*projection, modelViewMatrix, *mvpMatrix);
  if (parameters->modelDepthHack > 0.0f) {
    mvpMatrix->m[8] *= parameters->modelDepthHack;
    mvpMatrix->m[9] *= parameters->modelDepthHack;
    mvpMatrix->m[10] *= parameters->modelDepthHack;
    mvpMatrix->m[11] *= parameters->modelDepthHack;
  }
}
