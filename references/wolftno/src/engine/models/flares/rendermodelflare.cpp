#include "engine/models/flares/rendermodelflare.h"

#include "engine/decls/declflare.h"
#include "engine/framework/coderesource.h"
#include "engine/models/serializer.h"
#include "engine/models/transparency/rendermodeltransparency.h"
#include "engine/models/renderworld.h"
#include "engine/renderer/autorenderparms.h"
#include "engine/renderer/declmaterial.h"
#include "engine/renderer/imagemanager.h"
#include "engine/renderer/jobs/render/renderocclusion.h"
#include "engine/renderer/render.h"
#include "engine/renderer/renderersystem.h"
#include "shared/idlib/csystems/cvarsystemlocal.h"
#include "shared/idlib/lib.h"
#include "shared/idlib/math/mathlib.h"
#include "shared/idlib/geometry/winding.h"

#include <algorithm>
#include <cmath>
#include <cstdint>
#include <cstdio>
#include <cstring>
#include <memory>

extern idCVar r_lockView;

idCVar r_skipFlares("r_skipFlares", "0", CVAR_INTEGER,
                    "1 = skip all flares", 0.0f, 1.0f);
idCVar r_skipSunFlares("r_skipSunFlares", "0", CVAR_INTEGER,
                       "1 = skip sun flares", 0.0f, 1.0f);
idCVar r_singleFlareModel("r_singleFlareModel", "-1", CVAR_INTEGER,
                          "suppress all but one flare");
idCVar r_debugFlareOcclusionQuad("r_debugFlareOcclusionQuad", "0",
                                 CVAR_INTEGER,
                                 "display occlusion quad debug");

idCodeResource<idMaterial> mtrFlareDepthOnly("flareDepthOnly");
idCodeResource<idDeclRenderParm> rp_flareOriginNDC("flareOriginNDC");

idVertexBuffer idRenderModelFlare::vertexBuffer[2];
idIndexBuffer idRenderModelFlare::indexBuffer;
idDrawVert* idRenderModelFlare::mappedVertices = nullptr;
int idRenderModelFlare::mappedBufferIndex = 0;
int idRenderModelFlare::vertOffset = 0;

idVertexBuffer idRenderModelFlareOcclusionQuad::vertexBuffer;
idIndexBuffer idRenderModelFlareOcclusionQuad::indexBuffer;
int idRenderModelFlareOcclusionQuad::vertOffset = 0;
idVec3 idRenderModelFlareOcclusionQuad::points[4] = {
    {0.0f, 1.0f, -1.0f}, {0.0f, -1.0f, -1.0f},
    {0.0f, -1.0f, 1.0f}, {0.0f, 1.0f, 1.0f}};

namespace {

constexpr int kFlareVertexCapacity = 1024;
constexpr int kFlareQuadCapacity = 256;
constexpr int kFlareSurfaceCapacity = 20;
constexpr float kSmallest = 1.17549435e-38f;
constexpr idRenderMatrix kFlipToOpenGL =
    {{0, -1, 0, 0, 0, 0, 1, 0, -1, 0, 0, 0, 0, 0, 0, 1}};

const idVec3& ActiveOrigin(const idRenderModel& model) {
  return (model.useDeferredPosition & 1U) != 0U ? model.deferredOrigin
                                                : model.g.origin;
}

const idMat3& ActiveAxis(const idRenderModel& model) {
  return (model.useDeferredPosition & 1U) != 0U ? model.deferredAxis
                                                : model.g.axis;
}

void HideSurfaces(idRenderModel& model) {
  for (int index = 0; index < model.surfaces.Num(); ++index) {
    idTriangles* const geometry = model.surfaces[index].geometry;
    if (geometry != nullptr) {
      geometry->numVerts = 0;
      geometry->numIndexes = 0;
    }
  }
}

std::uint8_t ClampByte(float value) {
  return static_cast<std::uint8_t>(
      std::clamp(static_cast<int>(value), 0, 255));
}

std::uint8_t PackDirection(float value) {
  return ClampByte((value + 1.0f) * 127.5f + 0.5f);
}

idVec2 ProjectPoint(const idRenderMatrix& matrix, const idVec3& point,
                    float xScale, float yScale) {
  const float x = matrix.m[0] * point.x + matrix.m[1] * point.y +
                  matrix.m[2] * point.z + matrix.m[3];
  const float y = matrix.m[4] * point.x + matrix.m[5] * point.y +
                  matrix.m[6] * point.z + matrix.m[7];
  const float w = matrix.m[12] * point.x + matrix.m[13] * point.y +
                  matrix.m[14] * point.z + matrix.m[15];
  const float inverseW = 1.0f / w;
  return {(x * inverseW + 1.0f) * 0.5f * xScale,
          (y * inverseW + 1.0f) * 0.5f * yScale};
}

int RoundedMultisampleCount() {
  const int samples = std::max(renderSystem->GetNumMultisamples(), 1);
  int rounded = 1;
  while (rounded < samples)
    rounded <<= 1;
  return rounded;
}

void BuildQuadIndexes(std::uint16_t* indexes, bool flareWinding) {
  for (int quad = 0; quad < kFlareQuadCapacity; ++quad) {
    const int vertex = quad * 4;
    const int index = quad * 6;
    indexes[index + 0] = static_cast<std::uint16_t>(vertex);
    if (flareWinding) {
      indexes[index + 1] = static_cast<std::uint16_t>(vertex + 2);
      indexes[index + 2] = static_cast<std::uint16_t>(vertex + 3);
      indexes[index + 3] = static_cast<std::uint16_t>(vertex);
      indexes[index + 4] = static_cast<std::uint16_t>(vertex + 3);
      indexes[index + 5] = static_cast<std::uint16_t>(vertex + 1);
    } else {
      indexes[index + 1] = static_cast<std::uint16_t>(vertex + 1);
      indexes[index + 2] = static_cast<std::uint16_t>(vertex + 2);
      indexes[index + 3] = static_cast<std::uint16_t>(vertex);
      indexes[index + 4] = static_cast<std::uint16_t>(vertex + 2);
      indexes[index + 5] = static_cast<std::uint16_t>(vertex + 3);
    }
  }
}

bool CenterInClip(const idVec3& source, const idRenderMatrix& model,
                  const idRenderMatrix& projection, idVec2& ndc) {
  idVec4 eye;
  idVec4 clip;
  idRenderMatrix::TransformModelToClip(source, model, projection, eye, clip);
  if (std::fabs(clip.w) <= kSmallest)
    return false;
  const float inverseW = 1.0f / clip.w;
  const float depth = clip.z * inverseW;
  if (depth < -1.0f || depth > 1.0f)
    return false;
  ndc = {clip.x * inverseW, clip.y * inverseW};
  return true;
}

} // namespace

int idRenderView::GetModelOcclusionResult(int modelIndex) const {
  if (occlusionState->modelVisibleFrameNumber[modelIndex] <
          occlusionState->renderFrameNumber ||
      occlusionState->modelVisibleFrameCount[modelIndex] <= 1) {
    return 0;
  }
  return occlusionState->modelOcclusionQueryResult[modelIndex];
}

void idRenderModelFlare::StartFrame() {
  mappedBufferIndex ^= 1;
  mappedVertices = static_cast<idDrawVert*>(
      vertexBuffer[mappedBufferIndex].MapBuffer(BM_WRITE_NOSYNC));
  vertOffset = 0;
}

void idRenderModelFlare::EndFrame() {
  vertexBuffer[mappedBufferIndex].UnmapBuffer();
}

void idRenderModelFlare::Shutdown() {
  if (!idLib::headless) {
    if (mappedVertices != nullptr && vertexBuffer[mappedBufferIndex].IsMapped())
      vertexBuffer[mappedBufferIndex].UnmapBuffer();
    for (idVertexBuffer& buffer : vertexBuffer)
      buffer.FreeBufferObject();
    indexBuffer.FreeBufferObject();
  }
  mappedVertices = nullptr;
  mappedBufferIndex = 0;
  vertOffset = 0;
}

void idRenderModelFlareOcclusionQuad::Shutdown() {
  vertexBuffer.FreeBufferObject();
  indexBuffer.FreeBufferObject();
  vertOffset = 0;
}

void idRenderModelFlareOcclusionQuad::StartFrame() {
  vertOffset = 0;
}

void idRenderModelFlare::Init() {
  for (idVertexBuffer& buffer : vertexBuffer)
    buffer.AllocBufferObject(nullptr, 0x8000, 31, BU_DYNAMIC);
  auto indexes = std::make_unique<std::uint16_t[]>(1536);
  BuildQuadIndexes(indexes.get(), true);
  indexBuffer.AllocBufferObject(indexes.get(), 3072, BU_DEFAULT);
  mappedVertices = nullptr;
  mappedBufferIndex = 0;
  vertOffset = 0;
}

void idRenderModelFlareOcclusionQuad::Init() {
  vertexBuffer.AllocBufferObject(nullptr, 0x8000, 31, BU_DYNAMIC);
  auto indexes = std::make_unique<std::uint16_t[]>(1536);
  BuildQuadIndexes(indexes.get(), false);
  indexBuffer.AllocBufferObject(indexes.get(), 3072, BU_DEFAULT);

  auto* vertices =
      static_cast<idDrawVert*>(vertexBuffer.MapBuffer(BM_WRITE));
  if (vertices != nullptr) {
    const std::uint8_t normal[4] = {PackDirection(0.0f),
                                    PackDirection(1.0f),
                                    PackDirection(0.0f), 0};
    const std::uint8_t tangent[4] = {PackDirection(1.0f),
                                     PackDirection(0.0f),
                                     PackDirection(0.0f), 128};
    for (int quad = 0; quad < kFlareQuadCapacity; ++quad) {
      idDrawVert* const verts = vertices + quad * 4;
      std::memset(verts, 0, sizeof(idDrawVert) * 4);
      for (int corner = 0; corner < 4; ++corner) {
        verts[corner].xyz = points[corner];
        std::memcpy(verts[corner].normal, normal, sizeof(normal));
        std::memcpy(verts[corner].tangent, tangent, sizeof(tangent));
        std::memset(verts[corner].color, 0xff, 4);
      }
      verts[0].st = {0.0f, 0.0f};
      verts[1].st = {1.0f, 0.0f};
      verts[2].st = {1.0f, 1.0f};
      verts[3].st = {0.0f, 1.0f};
    }
  }
  vertexBuffer.UnmapBuffer();
  vertOffset = 0;
}

idRenderModelFlareOcclusionQuad::idRenderModelFlareOcclusionQuad()
    : isSunFlare(false), recoveredOcclusionPadding{}, screenSpaceArea{},
      currentIdx(0) {
  renderModelType = RENDERMODEL_FLAREOCCLUSIONQUAD;
  SetName("_flareOcclusionQuad");
  g.flags0 |= 0x80U;
  g.flags1 |= 0x01U;
  g.flags2 |= 0x10U;
  g.modelDepthSort = static_cast<modelDepthSort_t>(2);

  idRenderModelSurface surface{};
  surface.material = mtrFlareDepthOnly.Get();
  surface.geometry = new idTriangles{};
  surface.geometry->numVerts = 4;
  surface.geometry->numIndexes = 6;
  surface.geometry->vertexBuffer.Reference(vertexBuffer);
  surface.geometry->indexBuffer.Reference(indexBuffer);
  surface.geometry->bounds.Clear();
  for (const idVec3& point : points)
    surface.geometry->bounds.AddPoint(point);
  AddSurface(surface);
  FinishSurfaces();
  g.flags2 |= 0x80U;
}

idRenderModelFlare::idRenderModelFlare(const idDeclFlare* declFlare)
    : flareDecl(declFlare), occlusionQuadModel(nullptr), isSunFlare(false),
      recoveredFlarePadding{}, lockedViewState{} {
  renderModelType = RENDERMODEL_FLARE;
  lockedViewState.quadColor = {1.0f, 1.0f, 1.0f, 1.0f};
  lockedViewState.entityColor = {1.0f, 1.0f, 1.0f, 1.0f};
  lockedViewState.entityColorScale = 1.0f;
  lockedViewState.occlusionRatio = 1.0f;
  lockedViewState.fade = 1.0f;
  lockedViewState.viewAngle = 1.0f;
  SetName(declFlare != nullptr ? declFlare->GetName() : "<null flare>");
  g.flags0 |= 0x80U;
  g.modelDepthSort = MODEL_DEPTH_SORT_LAST;
  g.flags1 |= 0xa1U;

  for (int index = 0; index < kFlareSurfaceCapacity; ++index) {
    idRenderModelSurface surface{};
    surface.geometry = new idTriangles{};
    surface.geometry->indexBuffer.Reference(indexBuffer);
    surface.geometry->vertexBuffer.Reference(vertexBuffer[0]);
    AddSurface(surface);
  }
}

float idRenderModelFlareOcclusionQuad::CalcScreenSpaceAreaForView(
    const idRenderView* currentView, const idRenderView*,
    idRenderModelUpdateTools* updateTools) {
  float resolutionX = 1.0f;
  float resolutionY = 1.0f;
  updateTools->GetCurrentResolutionScale(&resolutionX, &resolutionY);
  const float xScale =
      static_cast<float>(currentView->renderWidth) * resolutionX - 1.0f;
  const float yScale =
      static_cast<float>(currentView->renderHeight) * resolutionY - 1.0f;
  idRenderMatrix mvp{};
  idVec2 centerNdc{};

  if (!isSunFlare) {
    idMat3 axis = ActiveAxis(*this);
    if ((g.flags1 & 0x40U) != 0U)
      axis = (currentView->r.vieworg - ActiveOrigin(*this)).ToMat3();
    idRenderMatrix model;
    idRenderMatrix modelView;
    idRenderMatrix::CreateFromOriginAxisScale(ActiveOrigin(*this), axis,
                                              g.scale, model);
    idRenderMatrix::Multiply(currentView->viewMatrix, model, modelView);
    if (!CenterInClip(vec3_origin, modelView, currentView->projectionMatrix,
                      centerNdc)) {
      return 0.0f;
    }
    idRenderMatrix::Multiply(currentView->projectionMatrix, modelView, mvp);
  } else {
    idRenderMatrix directionView = currentView->viewMatrix;
    directionView.m[3] = 0.0f;
    directionView.m[7] = 0.0f;
    directionView.m[11] = 0.0f;
    if (!CenterInClip(ActiveOrigin(*this), directionView,
                      currentView->projectionMatrix, centerNdc)) {
      return 0.0f;
    }
    idRenderMatrix ortho{};
    ortho.m[0] = g.scale.x / static_cast<float>(currentView->windowWidth);
    ortho.m[3] = centerNdc.x;
    ortho.m[5] = g.scale.y / static_cast<float>(currentView->windowHeight);
    ortho.m[7] = centerNdc.y;
    ortho.m[10] = 1.0f;
    std::uint32_t almostOneBits = 0x3f7ffff2U;
    std::memcpy(&ortho.m[11], &almostOneBits, sizeof(almostOneBits));
    ortho.m[15] = 1.0f;
    idRenderMatrix::Multiply(ortho, kFlipToOpenGL, mvp);
  }

  const idVec2 point0 = ProjectPoint(mvp, points[0], xScale, yScale);
  const idVec2 point1 = ProjectPoint(mvp, points[1], xScale, yScale);
  const idVec2 point3 = ProjectPoint(mvp, points[3], xScale, yScale);
  return static_cast<float>(RoundedMultisampleCount()) *
         std::fabs(point1.x - point0.x) *
         std::fabs(point3.y - point0.y);
}

bool idRenderModelFlare::UpdateInView(
    const idRenderView* currentView, const idRenderView*,
    idRenderModelUpdateTools*) {
  if (r_skipFlares.valueInteger != 0 ||
      (isSunFlare && r_skipSunFlares.valueInteger != 0) ||
      (r_singleFlareModel.valueInteger >= 0 &&
       r_singleFlareModel.valueInteger != GetIndex())) {
    HideSurfaces(*this);
    return false;
  }
  if (flareDecl == nullptr || flareDecl->subflares.Num() > surfaces.Num())
    return false;

  float fade = 1.0f;
  float occlusionRatio = 1.0f;
  float viewAngle = 1.0f;
  float viewDistance = 0.0f;
  float entityColorScale = 1.0f;
  idVec4 entityColor{1.0f, 1.0f, 1.0f, 1.0f};
  idVec4 quadColor{1.0f, 1.0f, 1.0f, 1.0f};

  if (r_lockView.valueInteger != 0) {
    fade = lockedViewState.fade;
    if (fade <= 0.0f) {
      HideSurfaces(*this);
      return false;
    }
    entityColorScale = lockedViewState.entityColorScale;
    viewAngle = lockedViewState.viewAngle;
    entityColor = lockedViewState.entityColor;
    quadColor = lockedViewState.quadColor;
    occlusionRatio = lockedViewState.occlusionRatio;
    viewDistance = lockedViewState.viewDist;
  } else {
    if (occlusionQuadModel != nullptr) {
      occlusionRatio = 0.0f;
      const float area = occlusionQuadModel->screenSpaceArea[
          occlusionQuadModel->currentIdx ^ 1];
      const int result = currentView->GetModelOcclusionResult(
          occlusionQuadModel->GetIndex());
      if (result > 1 && area > kSmallest)
        occlusionRatio =
            std::clamp(static_cast<float>(result) / area, 0.0f, 1.0f);
      quadColor = occlusionQuadModel->gameParmBlock.GetVector(rp_Color.Get());
    }

    const idVec3 delta = currentView->r.vieworg - ActiveOrigin(*this);
    const float distanceSquared = delta.x * delta.x + delta.y * delta.y +
                                  delta.z * delta.z;
    const float inverseDistance =
        1.0f / std::sqrt(std::max(distanceSquared, kSmallest));
    const idVec3 viewVector = delta * inverseDistance;
    viewDistance = distanceSquared * inverseDistance;
    float distanceFade = 1.0f;
    if (!isSunFlare) {
      const float fadeRange = g.maxVisibleRange - g.fadeVisibilityOver;
      if (fadeRange > kSmallest) {
        distanceFade = std::clamp(
            1.0f - (viewDistance - g.fadeVisibilityOver) / fadeRange,
            0.0f, 1.0f);
      }
    }
    fade = distanceFade * occlusionRatio;
    if (fade <= 0.0f) {
      HideSurfaces(*this);
      lockedViewState.fade = 0.0f;
      return false;
    }

    const idVec3 angleVector =
        flareDecl->useAltViewVecCalc ? viewVector : currentView->r.viewaxis[0];
    viewAngle = std::fabs(ActiveAxis(*this)[0] * angleVector);
    entityColor = gameParmBlock.GetVector(rp_Color.Get());
    fade *= gameParmBlock.GetFloat(rp_coverage.Get());
    entityColorScale = gameParmBlock.GetFloat(rp_lightScale.Get());
    entityColor.x *= quadColor.x;
    entityColor.y *= quadColor.y;
    entityColor.z *= quadColor.z;
    entityColor.w *= quadColor.w;
    if (flareDecl->viewAngleFade > kSmallest)
      fade *= std::min(viewAngle / flareDecl->viewAngleFade, 1.0f);
    if (1.0f - flareDecl->invViewAngleFade > kSmallest) {
      fade *= std::max(viewAngle - flareDecl->invViewAngleFade, 0.0f) /
              (1.0f - flareDecl->invViewAngleFade);
    }
    lockedViewState.quadColor = quadColor;
    lockedViewState.entityColor = entityColor;
    lockedViewState.entityColorScale = entityColorScale;
    lockedViewState.occlusionRatio = occlusionRatio;
    lockedViewState.fade = fade;
    lockedViewState.viewAngle = viewAngle;
    lockedViewState.viewDist = viewDistance;
  }

  idRenderMatrix viewMatrix = currentView->viewMatrix;
  if (isSunFlare) {
    viewMatrix.m[3] = 0.0f;
    viewMatrix.m[7] = 0.0f;
    viewMatrix.m[11] = 0.0f;
  }
  idVec4 eye;
  idVec4 clip;
  idRenderMatrix::TransformModelToClip(ActiveOrigin(*this), viewMatrix,
                                       currentView->projectionMatrix, eye,
                                       clip);
  idVec2 ndc{clip.x, clip.y};
  if (std::fabs(clip.w) > kSmallest) {
    ndc.x /= clip.w;
    ndc.y /= clip.w;
  }
  gameParmBlock.SetVec4(rp_flareOriginNDC.Get(),
                        {(ndc.x * 0.5f) + 0.5f,
                         (ndc.y * 0.5f) + 0.5f, 0.0f, 0.0f});

  const idVec4 partialScreen =
      currentView->r.parmBlock.GetVector(rp_partialScreen.Get());
  const float inverseScreenX = 1.0f / partialScreen.z;
  const float inverseScreenY = 1.0f / partialScreen.w;
  const float aspectScale =
      (currentView->projectionMatrix.m[5] /
       currentView->projectionMatrix.m[0]) *
      0.5625f;
  const float brightness = flareDecl->brightness;

  for (int surfaceIndex = 0; surfaceIndex < surfaces.Num(); ++surfaceIndex) {
    idRenderModelSurface& surface = surfaces[surfaceIndex];
    idTriangles* const geometry = surface.geometry;
    if (surfaceIndex >= flareDecl->subflares.Num()) {
      geometry->numVerts = 0;
      geometry->numIndexes = 0;
      continue;
    }
    if (vertOffset + 4 > kFlareVertexCapacity)
      continue;
    const idDeclFlare::subFlare_t& subflare =
        flareDecl->subflares[surfaceIndex];
    if (subflare.mtr == nullptr)
      continue;

    surface.material = subflare.mtr;
    geometry->numVerts = 4;
    geometry->numIndexes = 6;
    geometry->vertexBuffer.Reference(vertexBuffer[mappedBufferIndex]);
    geometry->indexBuffer.Reference(indexBuffer, 12 * (vertOffset >> 2), 12);

    idDrawVert localVerts[4]{};
    const std::uint8_t red = ClampByte(
        subflare.color.x * brightness * entityColor.x * entityColorScale *
        255.0f);
    const std::uint8_t green = ClampByte(
        subflare.color.y * brightness * entityColor.y * entityColorScale *
        255.0f);
    const std::uint8_t blue = ClampByte(
        subflare.color.z * brightness * entityColor.z * entityColorScale *
        255.0f);
    const std::uint8_t alpha = ClampByte(
        subflare.color.w * fade * entityColor.w * 255.0f);
    for (idDrawVert& vertex : localVerts) {
      vertex.color[0] = red;
      vertex.color[1] = green;
      vertex.color[2] = blue;
      vertex.color[3] = alpha;
    }

    const idVec4 atlas = subflare.mtr->atlasScaleBias;
    float atlasInsetX = 0.0f;
    float atlasInsetY = 0.0f;
    if (subflare.mtr->MaterialUsesTransSortAtlas() && globalImages != nullptr &&
        globalImages->transSortImageAtlas != nullptr) {
      atlasInsetX = 0.5f /
                    (static_cast<float>(
                         globalImages->transSortImageAtlas->ATLAS_WIDTH) *
                     atlas.x);
      atlasInsetY = 0.5f /
                    (static_cast<float>(
                         globalImages->transSortImageAtlas->ATLAS_HEIGHT) *
                     atlas.y);
    }
    localVerts[0].st = {atlas.x * atlasInsetX + atlas.z,
                        atlas.y * atlasInsetY + atlas.w};
    localVerts[1].st = {(1.0f - atlasInsetX) * atlas.x + atlas.z,
                        localVerts[0].st.y};
    localVerts[2].st = {localVerts[0].st.x,
                        (1.0f - atlasInsetY) * atlas.y + atlas.w};
    localVerts[3].st = {localVerts[1].st.x, localVerts[2].st.y};

    const float occlusionScaleX =
        std::max(occlusionRatio, subflare.minScaleX);
    const float occlusionScaleY =
        std::max(occlusionRatio, subflare.minScaleY);
    float coneScale = 1.0f;
    if (std::fabs(1.0f - subflare.coneAngle) > kSmallest) {
      coneScale = std::max(viewAngle - subflare.coneAngle, 0.0f) /
                  (1.0f - subflare.coneAngle);
    }
    const float viewScaleX =
        (subflare.viewScaleX - 1.0f) * coneScale + 1.0f;
    const float viewScaleY =
        (subflare.viewScaleY - 1.0f) * coneScale + 1.0f;
    float distanceScale = 1.0f;
    if (std::fabs(subflare.scaleDistance) > kSmallest) {
      distanceScale = std::max(
          (subflare.scaleDistance - viewDistance) / subflare.scaleDistance,
          0.0f);
    }
    const float maximumScaleX =
        (subflare.maxDistScaleX - 1.0f) * distanceScale + 1.0f;
    const float maximumScaleY =
        (subflare.maxDistScaleY - 1.0f) * distanceScale + 1.0f;
    const float halfWidth = maximumScaleX * viewScaleX * occlusionScaleX *
                            subflare.width * inverseScreenX;
    const float halfHeight = maximumScaleY * viewScaleY * occlusionScaleY *
                             subflare.height * inverseScreenY * aspectScale;
    const float reflectX = subflare.reflectX ? -1.0f : 1.0f;
    const float reflectY = subflare.reflectY ? -1.0f : 1.0f;
    const float centerX = ndc.x - reflectX * ndc.x * subflare.position;
    const float centerY = ndc.y - reflectY * ndc.y * subflare.position;
    localVerts[0].xyz = {centerX - halfWidth, centerY + halfHeight, 0.0f};
    localVerts[1].xyz = {centerX + halfWidth, centerY + halfHeight, 0.0f};
    localVerts[2].xyz = {centerX - halfWidth, centerY - halfHeight, 0.0f};
    localVerts[3].xyz = {centerX + halfWidth, centerY - halfHeight, 0.0f};
    std::memcpy(mappedVertices + vertOffset, localVerts, sizeof(localVerts));
    vertOffset += 4;
  }
  return true;
}

bool idRenderModelFlareOcclusionQuad::UpdateInView(
    const idRenderView* currentView, const idRenderView* nextView,
    idRenderModelUpdateTools* updateTools) {
  if (r_skipFlares.valueInteger != 0)
    return false;
  if (r_singleFlareModel.valueInteger >= 0 &&
      r_singleFlareModel.valueInteger != GetIndex()) {
    return false;
  }
  currentIdx ^= 1;
  screenSpaceArea[currentIdx] =
      CalcScreenSpaceAreaForView(currentView, nextView, updateTools);
  if (surfaces.Num() > 0 && surfaces[0].geometry != nullptr) {
    surfaces[0].geometry->indexBuffer.Reference(
        indexBuffer, 12 * (vertOffset >> 2), 12);
    vertOffset += 4;
  }

  const int debugMode = r_debugFlareOcclusionQuad.valueInteger;
  if (debugMode == 0)
    return false;

  idVec4 debugColor{1.0f, 0.0f, 1.0f, 1.0f};
  if (debugMode == 2) {
    float ratio = 0.0f;
    const float previousArea = screenSpaceArea[currentIdx ^ 1];
    const int samples = currentView->GetModelOcclusionResult(GetIndex());
    if (samples > 1 && previousArea > 0.0f)
      ratio = std::clamp(static_cast<float>(samples) / previousArea,
                         0.0f, 1.0f);
    debugColor = {ratio, ratio, ratio, 1.0f};
  } else if (debugMode == 3) {
    const float angle =
        std::fabs(ActiveAxis(*this)[0] * currentView->r.viewaxis[0]);
    debugColor = {angle, angle, angle, 1.0f};
  }

  const idVec3& origin = ActiveOrigin(*this);
  idMat3 axis;
  if ((g.flags1 & 0x40U) != 0U)
    axis = (currentView->r.vieworg - origin).ToMat3();
  else
    axis = ActiveAxis(*this);

  idWinding winding(4);
  idRenderDebug* const renderDebug = GetRenderDebug();
  for (int pointIndex = 0; pointIndex < 4; ++pointIndex) {
    const idVec3 scaled{points[pointIndex].x * g.scale.x,
                        points[pointIndex].y * g.scale.y,
                        points[pointIndex].z * g.scale.z};
    const idVec3 worldPoint = origin + axis * scaled;
    if (renderDebug != nullptr) {
      char label[16];
      std::snprintf(label, sizeof(label), "v%d", pointIndex);
      renderDebug->DebugText(label, worldPoint, 1.0f,
                             idColor::colorCyan, 1, 0, true, false);
    }
    winding.AddPoint(
        {worldPoint.x, worldPoint.y, worldPoint.z, 0.0f, 0.0f});
  }
  if (renderDebug != nullptr) {
    char label[32];
    std::snprintf(label, sizeof(label), "index%d", GetIndex());
    renderDebug->DebugText(label, origin, 1.0f, idColor::colorCyan,
                           1, 0, true, false);
    renderDebug->DebugFilledPolygon(debugColor, winding, 0, true);
  }
  return false;
}

void idRenderModelFlare::SerializeSnapshot(idSerializer* serializer,
                                           bool serializeParmBlock) {
  idRenderModel::SerializeSnapshot(serializer, serializeParmBlock);
  if (serializer->writing) {
    idDeclFlare::resourceList.WriteResourceToMsg(flareDecl, serializer->msg);
    serializer->msg->WriteBits(isSunFlare, 1);
  } else {
    flareDecl = static_cast<const idDeclFlare*>(
        idDeclFlare::resourceList.ReadResourceFromMsg(serializer->msg));
    isSunFlare = serializer->msg->ReadBits(1) == 1;
  }
}

void idRenderModelFlareOcclusionQuad::SerializeSnapshot(
    idSerializer* serializer, bool serializeParmBlock) {
  idRenderModel::SerializeSnapshot(serializer, serializeParmBlock);
  if (serializer->writing)
    serializer->msg->WriteBits(isSunFlare, 1);
  else
    isSunFlare = serializer->msg->ReadBits(1) == 1;
}

void idRenderModelFlare::Save(idFile* file) const {
  idRenderModel::Save(file);
  file->WriteString(flareDecl != nullptr ? flareDecl->GetName() : "");
  file->Write(&isSunFlare, 1);
}

bool idRenderModelFlare::Load(idFile* file) {
  if (!idRenderModel::Load(file))
    return false;
  idStr flareName;
  if (file->ReadString(flareName) == 0)
    return false;
  flareDecl = flareName.IsEmpty()
                  ? nullptr
                  : static_cast<const idDeclFlare*>(
                        idDeclFlare::resourceList.FindWithInheritance(
                            flareName.data, true, true));
  return file->Read(&isSunFlare, 1) != 0;
}

void idRenderModelFlareOcclusionQuad::Save(idFile* file) const {
  idRenderModel::Save(file);
  file->Write(&isSunFlare, 1);
  file->WriteBigArray(&currentIdx, 1);
  file->WriteBigArray(screenSpaceArea, 2);
}

bool idRenderModelFlareOcclusionQuad::Load(idFile* file) {
  if (!idRenderModel::Load(file) || file->Read(&isSunFlare, 1) == 0)
    return false;
  return file->ReadBigArray(&currentIdx, 1) != 0 &&
         file->ReadBigArray(screenSpaceArea, 2) != 0;
}
