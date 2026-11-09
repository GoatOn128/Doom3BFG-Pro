#include "engine/renderer/render.h"

#include "engine/renderer/jobs/render/renderocclusion.h"
#include "shared/idlib/csystems/cvarsystemlocal.h"
#include "shared/idlib/lib.h"
#include "shared/idlib/sys/sys_alloc.h"

#include <cmath>
#include <cstdint>
#include <cstring>

extern idCVar r_jitter;
extern idCVar r_zfar;
extern idCVar r_znear;

namespace {

constexpr idRenderMatrix kIdentityMatrix = {
    {1.0f, 0.0f, 0.0f, 0.0f,
     0.0f, 1.0f, 0.0f, 0.0f,
     0.0f, 0.0f, 1.0f, 0.0f,
     0.0f, 0.0f, 0.0f, 1.0f}};

} // namespace

void idEnvBlendParms::RemoveRenderParm(const idDeclRenderParm* parm) {
  int index = 0;
  while (index < parms.num && parms.list[index] != parm) {
    ++index;
  }
  if (index >= parms.num) {
    return;
  }

  parms.RemoveIndex(index);
  values.RemoveIndex(index);
  blends.RemoveIndex(index);
}

void renderView_t::BuildProjectionMatrix(idRenderMatrix& projection,
                                         int jitterWidth,
                                         int jitterHeight) const {
  if (useExplicitProjectionMatrix) {
    std::memmove(&projection, &explicitProjectionMatrix,
                 sizeof(explicitProjectionMatrix));
    return;
  }

  if (fov_x <= 0.0f || fov_y <= 0.0f) {
    idLib::Error("idRenderWorldLocal::Render: bad FOVs: %f, %f", fov_x,
                 fov_y);
  }

  float zNear = r_znear.valueFloat;
  if (cramZNear) {
    zNear *= 0.25f;
  }

  float xOffset = 0.0f;
  float yOffset = 0.0f;
  if (jitterWidth > 0 && jitterHeight > 0) {
    constexpr float degreesToRadians = 0.017453292f;
    constexpr float maxRandInverse = 0.000030518509f;
    const float yExtent =
        std::tanf(fov_y * degreesToRadians * 0.5f) * zNear;
    const float xExtent =
        std::tanf(fov_x * degreesToRadians * 0.5f) * zNear;
    const float yStep = (yExtent - -yExtent) / static_cast<float>(jitterHeight);
    const float xStep = (xExtent - -xExtent) / static_cast<float>(jitterWidth);

    static std::uint32_t seed = 0;
    const std::uint32_t xSeed = seed * 1103515245u + 12345u;
    seed = xSeed * 1103515245u + 12345u;
    const float randomX =
        static_cast<float>((xSeed >> 16u) & 0x7FFFu) * maxRandInverse;
    const float randomY =
        static_cast<float>((seed >> 16u) & 0x7FFFu) * maxRandInverse;
    xOffset = randomX * xStep;
    yOffset = randomY * yStep;
  }

  idRenderMatrix::CreateProjectionMatrixFov(
      fov_x, fov_y, zNear, r_zfar.valueFloat, xOffset, yOffset, projection);
}

void renderView_t::BuildViewMatrix(idRenderMatrix& matrix) const {
  if (forceIdentityViewMatrix) {
    matrix = kIdentityMatrix;
    return;
  }
  idRenderMatrix::CreateViewMatrix(vieworg, viewaxis, matrix);
}

void idEnvBlendParms::Clear() {
  parms.ClearFree();
  values.ClearFree();
  blends.ClearFree();
}

void idEnvBlendParms::CopyFrom(const idEnvBlendParms& other) {
  parms.SetNum(other.parms.num);
  values.SetNum(other.values.num);
  blends.SetNum(other.blends.num);

  for (int index = 0; index < parms.num; ++index) {
    parms.list[index] = other.parms.list[index];
  }
  for (int index = 0; index < values.num; ++index) {
    values.list[index] = other.values.list[index];
  }
  for (int index = 0; index < blends.num; ++index) {
    blends.list[index] = other.blends.list[index];
  }
}

void idEnvBlendParms::SetFloat(const idDeclRenderParm* parm, float value,
                               float blend) {
  parmValue_t parmValue;
  parmValue.value[0] = value;
  parmValue.value[1] = value;
  parmValue.value[2] = value;
  parmValue.value[3] = value;

  int index = 0;
  while (index < parms.num && parms.list[index] != parm) {
    ++index;
  }

  if (index >= parms.num) {
    parms.Append(parm);
    values.Append(parmValue);
    blends.Append(blend);
    return;
  }

  values.list[index] = parmValue;
  blends.list[index] = blend;
}

idRenderView::idRenderView() {
  viewIndex = 0;
  envBlend = {};

  projectionMatrix = kIdentityMatrix;
  inverseProjectionMatrix = kIdentityMatrix;
  viewMatrix = kIdentityMatrix;
  inverseViewMatrix = kIdentityMatrix;
  worldSpaceMVPMatrix = kIdentityMatrix;
  worldSpaceInverseMVPMatrix = kIdentityMatrix;
  viewSpaceModelMatrix = kIdentityMatrix;
  viewSpaceInverseModelMatrix = kIdentityMatrix;
  viewSpaceMVPMatrix = kIdentityMatrix;

  windowHeight = 0;
  renderWidth = 0;
  renderHeight = 0;
  feedbackWidth = 0;
  feedbackHeight = 0;
  viewport.Clear();
  scissorRect.Clear();
  testScissor.Clear();
  firstOcclusionQueryInPreviousFrame = 1;

  occlusionState = static_cast<occlusionState_t*>(
      idMem::Singleton()->AllocWithLocation(
          "w:\\build\\tech5\\engine\\renderer\\RenderView.cpp(179) : TAG_RENDERWORLD",
          sizeof(occlusionState_t), TAG_RENDERWORLD, true, ALIGN_16,
          HEAP_DEFAULTHEAP));
  std::memset(occlusionState, 0, sizeof(*occlusionState));
  occlusionState->renderFrameNumber = 1;
}

idRenderView::~idRenderView() noexcept(false) {
  idMem::Singleton()->Free(occlusionState, ALIGN_16);
}

void idRenderView::CommitParameters(int newWindowWidth, int newWindowHeight,
                                    int newRenderWidth, int newRenderHeight) {
  r = g;
  m_pModelEnvOverride = nullptr;

  int jitterWidth = 0;
  int jitterHeight = 0;
  if (r_jitter.valueInteger != 0) {
    jitterWidth = newRenderWidth;
    jitterHeight = newRenderHeight;
  }
  r.BuildProjectionMatrix(projectionMatrix, jitterWidth, jitterHeight);
  if (!idRenderMatrix::Inverse(projectionMatrix, inverseProjectionMatrix)) {
    idLib::Error("idRenderView: Projection Matrix Invert failed!\n");
  }

  r.BuildViewMatrix(viewMatrix);
  idRenderMatrix::InverseByTranspose(viewMatrix, inverseViewMatrix);
  idRenderMatrix::Multiply(projectionMatrix, viewMatrix, worldSpaceMVPMatrix);
  if (!idRenderMatrix::Inverse(worldSpaceMVPMatrix,
                               worldSpaceInverseMVPMatrix)) {
    idLib::Error("idRenderView: MVP Matrix Invert failed!\n");
  }

  std::memmove(&viewSpaceModelMatrix, &inverseViewMatrix,
               sizeof(viewSpaceModelMatrix));
  std::memmove(&viewSpaceInverseModelMatrix, &viewMatrix,
               sizeof(viewSpaceInverseModelMatrix));
  std::memmove(&viewSpaceMVPMatrix, &projectionMatrix,
               sizeof(viewSpaceMVPMatrix));

  viewport.x1 = 0;
  viewport.y1 = 0;
  viewport.x2 = newRenderWidth - 1;
  viewport.y2 = newRenderHeight - 1;
  windowWidth = newWindowWidth;
  windowHeight = newWindowHeight;
  renderWidth = newRenderWidth;
  renderHeight = newRenderHeight;
  feedbackWidth = 160;
  feedbackHeight = 120;
  scissorRect = viewport;
}
