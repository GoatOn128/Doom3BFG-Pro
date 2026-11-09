#include "engine/models/renderworld.h"

#include "engine/framework/console.h"
#include "engine/renderer/jobs/approximatelighting/approximatelighting.h"
#include "engine/renderer/jobs/render/renderocclusion.h"
#include "engine/renderer/jobs/render/renderwalkbsp.h"
#include "engine/renderer/render.h"
#include "engine/renderer/renderlightcommitted.h"
#include "engine/renderer/renderlocal.h"
#include "engine/renderer/rendermodelcommitted.h"
#include "engine/renderer/rendersystem_runtime.h"
#include "engine/renderer/renderworldcommitted.h"
#include "engine/renderer/triangles.h"
#include "shared/idlib/bv/box.h"
#include "shared/idlib/bv/sphere.h"
#include "shared/idlib/color.h"
#include "shared/idlib/csystems/cvarsystemlocal.h"
#include "shared/idlib/lib.h"
#include "shared/idlib/math/mathlib.h"
#include "shared/idlib/parallelism/paralleljoblist.h"
#include "shared/idlib/sys/sys_alloc.h"

#include <algorithm>
#include <cmath>
#include <cstdio>
#include <cstring>
#include <new>

extern idCVar r_dimShadowForceHighQuality;
extern idCVar r_showAmbientLighting;
extern idCVar r_showDynamicLightingSingleModel;
extern idCVar r_singleSelfShadow;

idCVar r_useDynamicLightingJobs("r_useDynamicLightingJobs", "1", 2,
                                "0 = off, 1 = threaded", 0.0f, 1.0f);
idCVar r_skipDynamicLighting("r_skipDynamicLighting", "0", 1, "");
idCVar r_showDynamicLighting(
    "r_showDynamicLighting", "0", 2,
    "Draw lines to lights for each dynamic model, 3 = draw bounds of problem models and lights affecting them");
idCVar r_dynamicLightingScale(
    "r_dynamicLightingScale", "1", 4,
    "Scales all approximated lighting on dynamic models");
idCVar r_directChannelScale("r_directChannelScale", "1", 4,
                            "Scales the direct light contribution");
idCVar r_ambientChannelScale("r_ambientChannelScale", "1", 4,
                             "Scales the ambient light contribution");
idCVar r_primeLightScale("r_primeLightScale", "1", 4,
                         "Scales prime light contribution");
idCVar r_dimShadowHeightTweak(
    "r_dimShadowHeightTweak", "0.9", 4,
    "This value is added to the true combined light z value to prevent long stretched shadows.  Set to 0 for true shadows.");
idCVar r_showLightingDebugPrint("r_showLightingDebugPrint", "1", 1,
                                "1 show warnings, 0 dont show warnings");
idCVar r_enableSelfShadowLessLocalLights(
    "r_enableSelfShadowLessLocalLights", "1", 2,
    "enable selfshadowless local light, 2 = force no selfshadows");

namespace {

constexpr int kMaxApproximateLightingParms = 1024;
constexpr int kMaxApproximateLightingLights = 8192;
constexpr int kMaxContributingLights = 16;
constexpr int kMaxBoundsAreas = 2048;
constexpr int kProblemLightCount = 128;
constexpr int kFrustumCullFront = 1;
constexpr int kFrustumCullCross = 3;

int warningThrottle = 0;

idVec3 BoundsCenter(const idBounds& bounds) {
  return {(bounds.b[0].x + bounds.b[1].x) * 0.5f,
          (bounds.b[0].y + bounds.b[1].y) * 0.5f,
          (bounds.b[0].z + bounds.b[1].z) * 0.5f};
}

idVec3 TransformPoint(const idRenderMatrix& matrix, const idVec3& point) {
  return {matrix.m[0] * point.x + matrix.m[1] * point.y +
              matrix.m[2] * point.z + matrix.m[3],
          matrix.m[4] * point.x + matrix.m[5] * point.y +
              matrix.m[6] * point.z + matrix.m[7],
          matrix.m[8] * point.x + matrix.m[9] * point.y +
              matrix.m[10] * point.z + matrix.m[11]};
}

bool BoundsContainsPoint(const idBounds& bounds, const idVec3& point) {
  return point.x >= bounds.b[0].x && point.x <= bounds.b[1].x &&
         point.y >= bounds.b[0].y && point.y <= bounds.b[1].y &&
         point.z >= bounds.b[0].z && point.z <= bounds.b[1].z;
}

idBox MakeOrientedBox(const idBounds& bounds, const idVec3& origin,
                      const idMat3& axis) {
  idBox box{};
  const idVec3 localCenter = BoundsCenter(bounds);
  box.center = origin + axis * localCenter;
  box.extents = (bounds.b[1] - bounds.b[0]) * 0.5f;
  box.axis = axis;
  return box;
}

bool ContainsIndex(const idList<idRenderIndex, 62>& list,
                   unsigned int index) {
  for (int i = 0; i < list.num; ++i) {
    if (list.list[i].index == index)
      return true;
  }
  return false;
}

bool ContainsArea(const idList<worldArea_t*, 62>& list,
                  const worldArea_t* area) {
  for (int i = 0; i < list.num; ++i) {
    if (list.list[i] == area)
      return true;
  }
  return false;
}

void RemoveIndexFast(idList<idRenderIndex, 62>& list, unsigned int index) {
  for (int i = 0; i < list.num; ++i) {
    if (list.list[i].index == index) {
      list.RemoveIndexFast(i);
      return;
    }
  }
}

void SetApproximateLightingScales(approximateLightingParms_t& parms) {
  parms.ambientChannelScale = r_ambientChannelScale.valueFloat;
  parms.directChannelScale = r_directChannelScale.valueFloat;
  parms.primeLightScale = r_primeLightScale.valueFloat;
  parms.dynamicLightingScale = r_dynamicLightingScale.valueFloat;
  parms.dimShadowHeightTweak = r_dimShadowHeightTweak.valueFloat;
}

} // namespace

template <typename T>
T* idListArrayNew(int num, unsigned tag, bool zeroBuffer) {
  if (num <= 0)
    return nullptr;
  const char* location =
      zeroBuffer
          ? "w:\\build\\tech5\\shared\\idlib\\containers\\List.h(27) : tag"
          : "w:\\build\\tech5\\shared\\idlib\\containers\\List.h(29) : tag";
  T* result = static_cast<T*>(idMem::Singleton()->AllocWithLocation(
      location, sizeof(T) * static_cast<std::size_t>(num),
      static_cast<memTag_t>(tag), zeroBuffer, ALIGN_16, HEAP_DEFAULTHEAP));
  if (result == nullptr)
    return nullptr;
  for (int i = 0; i < num; ++i)
    result[i].index = static_cast<unsigned int>(-1);
  return result;
}

template <typename T>
T* idListArrayResize(T* oldPointer, int oldNum, int newNum, unsigned tag,
                     bool zeroBuffer) {
  T* replacement =
      newNum > 0 ? idListArrayNew<T>(newNum, tag, zeroBuffer) : nullptr;
  if (newNum > 0 && replacement == nullptr)
    return nullptr;
  const int copyCount = (std::min)(oldNum, newNum);
  for (int i = 0; i < copyCount; ++i)
    replacement[i] = oldPointer[i];
  idMem::Singleton()->Free(oldPointer, ALIGN_16);
  return replacement;
}

void idRenderWorldCommitted::BoundsInAreas_r(int nodeNum,
                                             const idBounds* bounds,
                                             int* areas, int* numAreas,
                                             int maxAreas) {
  while (nodeNum >= 0) {
    const areaNode_t& node = areaNodes[nodeNum];
    const planeSide_t side = bounds->PlaneSide(node.plane, 0.1f);
    if (side != PLANESIDE_FRONT) {
      if (side != PLANESIDE_BACK && node.children[0] != 0) {
        BoundsInAreas_r(node.children[0], bounds, areas, numAreas, maxAreas);
        if (*numAreas >= maxAreas)
          return;
      }
      nodeNum = node.children[1];
    } else {
      nodeNum = node.children[0];
    }
    if (nodeNum == 0)
      return;
  }

  const int areaNum = -1 - nodeNum;
  for (int i = 0; i < *numAreas; ++i) {
    if (areas[i] == areaNum)
      return;
  }
  if (*numAreas < maxAreas)
    areas[(*numAreas)++] = areaNum;
}

int idRenderWorldCommitted::GetNumAddAlwaysModels() const {
  int count = 0;
  for (idRenderModelCommitted* model = addAlwaysModels; model != nullptr;
       model = model->nextAddAlwaysModel)
    ++count;
  return count;
}

void idRenderWorldCommitted::BoundsInAreas(const idBounds* bounds, int* areas,
                                           int* numAreas, int maxAreas) {
  BoundsInAreas_r(0, bounds, areas, numAreas, maxAreas);
}

void idRenderWorldCommitted::ShowApproximateLighting(
    idRenderModelCommitted* model,
    const approximateLightingParms_t* parms) {
  const int selectedModel = r_showDynamicLightingSingleModel.valueInteger;
  if (selectedModel >= 0 &&
      selectedModel != static_cast<int>(model->index.index >> 8))
    return;

  if (r_showAmbientLighting.valueInteger != 0) {
    ambientLighting.DebugDrawAmbientLightingSamples(committedDebugData,
                                                    parms->samplePoint);
  }
  if (r_showDynamicLighting.valueInteger == 0 ||
      r_showDynamicLighting.valueInteger == 3)
    return;

  committedDebugData->DebugBox(
      idColor::colorWhite,
      MakeOrientedBox(model->referenceBounds, model->r.origin, model->r.axis),
      1, false);
  const idBounds sampleBounds{
      {{parms->samplePoint.x - 2.0f, parms->samplePoint.y - 2.0f,
        parms->samplePoint.z - 2.0f},
       {parms->samplePoint.x + 2.0f, parms->samplePoint.y + 2.0f,
        parms->samplePoint.z + 2.0f}}};
  committedDebugData->DebugBounds(idColor::colorWhite, sampleBounds,
                                  vec3_origin, 1, false);

  int numContributions = 0;
  int numPrivatePrime = 0;
  for (; numContributions < kMaxContributingLights; ++numContributions) {
    const lightContribution_t& contribution =
        parms->contributions[numContributions];
    if (contribution.light == nullptr)
      break;
    if (contribution.light->r.lightClass == LIGHT_PRIVATE_PRIME)
      ++numPrivatePrime;

    const idVec4 contributionColor{contribution.color.x,
                                   contribution.color.y,
                                   contribution.color.z, 1.0f};
    committedDebugData->DebugLine(
        contributionColor, contribution.light->globalLightOrigin,
        parms->samplePoint, 3, false);

    const idVec3 localLight = TransformPoint(
        model->inverseModelMatrix, contribution.light->globalLightOrigin);
    idBounds expanded = model->referenceBounds;
    expanded.b[0] -= idVec3{4.0f, 4.0f, 4.0f};
    expanded.b[1] += idVec3{4.0f, 4.0f, 4.0f};
    if (BoundsContainsPoint(expanded, localLight)) {
      idVec3 rayDirection = BoundsCenter(expanded) - localLight;
      const float lengthSquared = rayDirection.LengthSqr();
      const float inverseLength =
          1.0f /
          std::sqrt((std::max)(lengthSquared, idMath::FLT_SMALLEST_NON_DENORMAL));
      rayDirection *= inverseLength;
      float scale1 = 0.0f;
      float scale2 = 0.0f;
      if (expanded.RayIntersection(localLight, rayDirection, scale1, scale2)) {
        const idVec3 globalHit = TransformPoint(
            model->modelMatrix, localLight + rayDirection * scale1);
        committedDebugData->DebugLine(
            idColor::colorOrange, contribution.light->globalLightOrigin,
            globalHit, 1, false);
        committedDebugData->DebugSphere(
            idColor::colorOrange, idSphere{globalHit, 2.0f}, 12, 1, false);
      }
    }
  }

  if (numContributions >= kMaxContributingLights) {
    idLib::Warning("model %s has more than MAX_CONTRIBUTING_LIGHTS (%d)",
                   model->name, kMaxContributingLights);
  }
  if (numPrivatePrime >= 2) {
    idLib::Warning(
        "model %s is affected by multiple LIGHT_PRIVATE_PRIME lights",
        model->name);
  }

  char label[256];
  std::snprintf(label, sizeof(label), "%u: %s", model->index.index >> 8,
                model->name);
  idVec3 labelPoint = parms->samplePoint;
  labelPoint.z += 8.0f;
  committedDebugData->DebugText(label, labelPoint, 0.2f,
                                idColor::colorWhite, 1, 3, false, false);
  std::snprintf(label, sizeof(label), "%u / %d", parms->numLights,
                numContributions);
  committedDebugData->DebugText(label, parms->samplePoint, 0.2f,
                                idColor::colorWhite, 1, 3, false, false);

  if (r_showDynamicLighting.valueInteger == 2) {
    for (unsigned int lightIndex = 0; lightIndex < parms->numLights;
         ++lightIndex) {
      const idRenderLightCommitted* light = parms->lights[lightIndex];
      bool contributed = false;
      for (int contributionIndex = 0;
           contributionIndex < kMaxContributingLights;
           ++contributionIndex) {
        if (parms->contributions[contributionIndex].light == light) {
          contributed = true;
          break;
        }
      }
      if (!contributed) {
        committedDebugData->DebugLine(
            {light->currentColor.x, light->currentColor.y,
             light->currentColor.z, 1.0f},
            light->globalLightOrigin, parms->samplePoint, 3, false);
      }
    }
  }
}

void idRenderWorldCommitted::ApproximateAmbientForBounds(
    const idBounds* bounds, approximateLighting_t* lighting) {
  lighting->Clear();
  lightContribution_t contributions[kMaxContributingLights]{};
  approximateLightingParms_t parms{};
  parms.lighting = lighting;
  parms.contributions = contributions;
  parms.ambientMap = ambientLighting.ambientMap;
  parms.shadowMap = globalShadows.GetShadowMap();
  parms.lights = nullptr;
  parms.samplePoint = BoundsCenter(*bounds);
  parms.sampleRadius = bounds->GetRadius();
  parms.lightGroupID = -1;
  parms.lightingModel = LM_PRIME_AND_CHANNEL;
  parms.noApproximateLighting = true;
  SetApproximateLightingScales(parms);
  ApproximateLightingJob(&parms);
}

void idRenderWorldCommitted::ApproximateLightingForModel(
    const idRenderView* view, idRenderModelCommitted* model, int renderTime,
    idParallelJobList* jobList) {
  if (!model->needsApproximatedLighting || (model->r.flags1 & 0x20u) != 0 ||
      model->references.num == 0)
    return;

  model->approximateState =
      (model->approximateState & ~3u) |
      ((model->approximateState ^ (model->approximateState >> 1u)) & 1u);
  if (r_skipDynamicLighting.valueInteger != 0) {
    approximateLighting_t& lighting =
        model->approximateLighting[model->currentApprox];
    lighting.Clear();
    for (idVec4& channel : lighting.channelLighting)
      channel = {0.5f, 0.5f, 0.5f, 1.0f};
    return;
  }

  idRenderModelCommitted* master = model;
  if (model->r.groupMasterIndex != -1 && (model->r.flags4 & 0x08u) != 0) {
    idRenderModelCommitted* candidate =
        committedRenderModels.list[model->r.groupMasterIndex];
    if (candidate != nullptr) {
      master = candidate;
    } else {
      idLib::Warning(
          "ApproximateLightingForModel: model had a groupMasterIndex pointing at a removed model",
          0, model, renderTime);
    }
  }

  const int frameNumber = view->occlusionState->renderFrameNumber;
  bool visible = view->occlusionState
                     ->modelVisibleFrameNumber[model->index.index >> 8] >=
                 frameNumber;
  for (int i = 0; !visible && i < model->references.num; ++i) {
    visible = view->occlusionState
                  ->worldAreaVisibleFrameNumber[model->references.list[i]
                                                    ->areaNum] >= frameNumber;
  }
  if (!visible)
    return;

  const int oldNumParms = numApproximateLightingParms;
  const int oldNumWanted = numWantedApproximateLightingParms;
  const int oldNumLights = numApproximateLightingLights;
  ++numWantedApproximateLightingParms;
  if (oldNumParms >= kMaxApproximateLightingParms ||
      oldNumLights >= kMaxApproximateLightingLights)
    return;

  const idRenderLightCommitted** gatheredLights =
      &approximateLightingLights[oldNumLights];
  const int remainingLights = kMaxApproximateLightingLights - oldNumLights;
  int numLights = 0;
  for (int areaIndex = 0; areaIndex < master->references.num; ++areaIndex) {
    worldArea_t* area = master->references.list[areaIndex];
    const idList<idRenderIndex, 62>* refs = nullptr;
    if (isComboMap) {
      refs = (master->r.flags3 & 0x01u) != 0
                 ? &area->approximatedLightPrivateLightRefs
                 : &area->approximatedLightRefs;
    } else {
      refs = &area->drawLightRefs;
    }
    for (int refIndex = 0;
         refIndex < refs->num && numLights < remainingLights; ++refIndex) {
      idRenderLightCommitted* light =
          committedRenderLights.list[refs->list[refIndex].index >> 8];
      bool duplicate = false;
      for (int i = 0; i < numLights; ++i) {
        if (gatheredLights[i] == light) {
          duplicate = true;
          break;
        }
      }
      if (!duplicate)
        gatheredLights[numLights++] = light;
    }
  }
  numApproximateLightingLights = (oldNumLights + numLights + 3) & ~3;

  if (remainingLights < kProblemLightCount && numLights == remainingLights)
    return;

  if (numLights >= kProblemLightCount && isComboMap) {
    if (r_showDynamicLighting.valueInteger == 3) {
      committedDebugData->DebugBounds(idColor::colorRed,
                                      model->globalReferenceBounds,
                                      vec3_origin, 0, false);
      committedDebugData->DebugText(
          model->name, BoundsCenter(model->globalReferenceBounds), 0.2f,
          idColor::colorRed, 1, 1, false, false);
      for (int i = 0; i < numLights; ++i) {
        const idRenderLightCommitted* light = gatheredLights[i];
        committedDebugData->DebugLine(idColor::colorWhite,
                                      light->globalLightOrigin,
                                      model->GetLightingPoint(), 3, false);
        committedDebugData->DebugBounds(idColor::colorYellow,
                                        light->globalReferenceBounds,
                                        vec3_origin, 0, false);
      }
    } else {
      idLib::WarningIf(
          r_showLightingDebugPrint.valueInteger != 0,
          "Approximate lighting affected by more than %d lights for model %s",
          kProblemLightCount, model->name);
    }
  }

  approximateLightingParms_t& parms =
      approximateLightingParms[numApproximateLightingParms++];
  parms = {};
  parms.lightingModel =
      (master->r.flags3 & 0x01u) != 0 &&
              (master->r.flags3 & 0x02u) == 0
          ? LM_LOCAL_LIGHTS
          : LM_PRIME_AND_CHANNEL;
  parms.lights = gatheredLights;
  parms.numLights = static_cast<unsigned int>(numLights);
  const int targetLighting =
      parms.lightingModel == LM_LOCAL_LIGHTS ? model->currentApprox
                                             : (model->currentApprox ^ 1u);
  parms.lighting = &model->approximateLighting[targetLighting];
  parms.ambientMap = ambientLighting.ambientMap;
  parms.shadowMap = globalShadows.GetShadowMap();
  parms.samplePoint = master->GetLightingPoint();
  parms.sampleRadius = master->GetLightingRadius();
  parms.lightGroupID = master->r.lightGroupID;
  parms.noAmbient = (master->r.flags3 & 0x20u) != 0;
  parms.noApproximateLighting =
      !isComboMap && (model->r.flags3 & 0x40u) == 0;
  parms.privateLightOnly = (master->r.flags3 & 0x01u) != 0;
  parms.noSelfShadow =
      parms.lightingModel == LM_LOCAL_LIGHTS &&
      (((master->r.flags3 & 0x04u) != 0 &&
        r_enableSelfShadowLessLocalLights.valueInteger != 0) ||
       r_enableSelfShadowLessLocalLights.valueInteger == 2 ||
       (r_singleSelfShadow.valueInteger != -1 &&
        r_singleSelfShadow.valueInteger !=
            static_cast<int>(model->index.index >> 8)) ||
       !model->mayCastShadows ||
       static_cast<signed char>(model->r.flags0) < 0 ||
       model->r.castDimShadows == DIMSHADOW_OFF);
  parms.lowQualityDimShadows = true;
  if (r_dimShadowForceHighQuality.valueInteger > 0 ||
      (r_dimShadowForceHighQuality.valueInteger == 0 &&
       model->r.castDimShadows == DIMSHADOW_HIGH_QUALITY &&
       (model->r.flags0 & 0x01u) != 0)) {
    parms.lowQualityDimShadows = false;
  }
  SetApproximateLightingScales(parms);

  const bool useJob = parms.lightingModel != LM_LOCAL_LIGHTS &&
                      r_showDynamicLighting.valueInteger == 0 &&
                      r_showAmbientLighting.valueInteger == 0 && isComboMap &&
                      r_useDynamicLightingJobs.valueInteger == 1;
  if (parms.lightingModel != LM_LOCAL_LIGHTS)
    model->queuedApprox = 1;
  if (useJob) {
    jobList->AddJob(reinterpret_cast<jobRun_t>(ApproximateLightingJob),
                    &parms);
  } else {
    lightContribution_t contributions[kMaxContributingLights]{};
    parms.contributions = contributions;
    ApproximateLightingJob(&parms);
    ShowApproximateLighting(model, &parms);
    numApproximateLightingParms = oldNumParms;
    numWantedApproximateLightingParms = oldNumWanted;
    numApproximateLightingLights = oldNumLights;
    parms.contributions = nullptr;
  }
}

void idRenderWorldCommitted::PostCommitData(const idRenderView* renderView,
                                            int renderTime) {
  for (int i = 0; i < committedRenderModels.num; ++i) {
    if (postCommitModels.ptr[i]) {
      postCommitModels.ptr[i] = false;
      idRenderModelCommitted* model = committedRenderModels.list[i];
      model->PostCommit();
      ++tr.pc.c_modelCommits;
      ++tr.pc.c_modelTypeCommits[model->renderModelType];
    }
  }
  for (int i = 0; i < committedRenderLights.num; ++i) {
    if (postCommitLights.ptr[i]) {
      postCommitLights.ptr[i] = false;
      committedRenderLights.list[i]->PostCommit();
      ++tr.pc.c_lightCommits;
    }
  }

  rendererJobList->Wait();
  console->AddJobListTimeRange(RANGE_JBL_RNDR, &idColor::colorMagenta,
                               rendererJobList, 0);
  numApproximateLightingParms = 0;
  numWantedApproximateLightingParms = 0;
  numApproximateLightingLights = 0;

  idRenderModelCommitted* lastModel = nullptr;
  idRenderModelCommitted* lastFittedModel = nullptr;
  idRenderModelCommitted* lastLocalLightModel = nullptr;
  for (idRenderModelCommitted* model = approxLightModels; model != nullptr;
       model = model->nextApproxLightModel) {
    ApproximateLightingForModel(renderView, model, renderTime,
                                rendererJobList);
    ++tr.pc.c_approxLightModels;
    lastModel = model;
    if (numApproximateLightingParms == numWantedApproximateLightingParms)
      lastFittedModel = model;
    if ((model->r.flags3 & 0x01u) != 0)
      lastLocalLightModel = model;
  }

  if (numApproximateLightingParms != numWantedApproximateLightingParms &&
      lastFittedModel != nullptr &&
      (lastFittedModel->r.flags3 & 0x01u) == 0) {
    if (lastLocalLightModel != nullptr) {
      lastModel->nextApproxLightModel =
          lastLocalLightModel->nextApproxLightModel;
      lastLocalLightModel->nextApproxLightModel =
          lastFittedModel->nextApproxLightModel;
    } else {
      lastModel->nextApproxLightModel = approxLightModels;
      approxLightModels = lastFittedModel->nextApproxLightModel;
    }
    lastFittedModel->nextApproxLightModel = nullptr;
  }

  tr.pc.c_approxLightModelsWant = numWantedApproximateLightingParms;
  if (numApproximateLightingParms == numWantedApproximateLightingParms) {
    warningThrottle = 0;
  } else {
    if (warningThrottle % 500 == 0) {
      char suffix[96]{};
      if (warningThrottle != 0) {
        std::snprintf(suffix, sizeof(suffix),
                      "(continuously the last %d frames)", warningThrottle);
      }
      idLib::Warning(
          "Too many approximated light models, tried lighting %d models but only %d fit in the buffer%s",
          numWantedApproximateLightingParms, numApproximateLightingParms,
          suffix);
    }
    ++warningThrottle;
  }
  rendererJobList->Submit(nullptr, -1);
}

int idRenderWorldCommitted::GetNumCommittedModels() const {
  int count = 0;
  for (int i = 0; i < committedRenderModels.num; ++i)
    count += committedRenderModels.list[i] != nullptr ? 1 : 0;
  return count;
}

int idRenderWorldCommitted::GetNumCommittedLights() const {
  int count = 0;
  for (int i = 0; i < committedRenderLights.num; ++i)
    count += committedRenderLights.list[i] != nullptr ? 1 : 0;
  return count;
}

void idRenderWorldCommitted::ApproximateLightingForBounds(
    const idBounds* bounds, approximateLighting_t* lighting) {
  int areas[kMaxBoundsAreas];
  int numAreas = 0;
  BoundsInAreas(bounds, areas, &numAreas, kMaxBoundsAreas);

  const idRenderLightCommitted* lights[kMaxBoundsAreas];
  int numLights = 0;
  for (int areaIndex = 0; areaIndex < numAreas; ++areaIndex) {
    worldArea_t& area = worldAreas[areas[areaIndex]];
    const idList<idRenderIndex, 62>& refs =
        isComboMap ? area.approximatedLightRefs : area.drawLightRefs;
    for (int refIndex = 0;
         refIndex < refs.num && numLights < kMaxBoundsAreas; ++refIndex) {
      idRenderLightCommitted* light =
          committedRenderLights.list[refs.list[refIndex].index >> 8];
      bool duplicate = false;
      for (int i = 0; i < numLights; ++i) {
        if (lights[i] == light) {
          duplicate = true;
          break;
        }
      }
      if (!duplicate)
        lights[numLights++] = light;
    }
  }

  lighting->Clear();
  lightContribution_t contributions[kMaxContributingLights]{};
  approximateLightingParms_t parms{};
  parms.lighting = lighting;
  parms.contributions = contributions;
  parms.ambientMap = ambientLighting.ambientMap;
  parms.shadowMap = globalShadows.GetShadowMap();
  parms.lights = lights;
  parms.numLights = static_cast<unsigned int>(numLights);
  parms.samplePoint = BoundsCenter(*bounds);
  parms.sampleRadius = bounds->GetRadius();
  parms.lightGroupID = -1;
  parms.lightingModel = LM_PRIME_AND_CHANNEL;
  parms.noApproximateLighting = !isComboMap;
  SetApproximateLightingScales(parms);
  ApproximateLightingJob(&parms);
}

void idRenderWorldCommitted::SetBinaryModelState(const char* binaryModelName,
                                                 bool isVisible,
                                                 bool makingDefinition) {
  idStr name(binaryModelName);
  const int hash = idStr::Hash(name.data);
  int index = binaryModelHash.First(hash);
  while (index != -1 && !(name == binaryModelNames.list[index]))
    index = binaryModelHash.Next(index);

  if (index == -1) {
    if (!makingDefinition)
      idLib::Error("BinaryModelName '%s' was not defined", binaryModelName);
    index = binaryModelNames.Append(name);
    binaryModelState.Append(isVisible);
    binaryModelHash.Add(hash, index);
  } else {
    if (makingDefinition && std::strstr(name.data, "world") == nullptr) {
      idLib::Error("BinaryModelName '%s' was defined twice", binaryModelName);
    }
    binaryModelState.list[index] = isVisible;
  }
}

void idRenderWorldCommitted::AddModelRefToArea(
    idRenderModelCommitted* model, worldArea_t* area) {
  if (isComboMap || !ContainsIndex(area->modelRefs, model->index.index))
    area->modelRefs.Append(model->index);
  if (isComboMap || !ContainsArea(model->references, area))
    model->references.Append(area);
  ++tr.pc.c_modelReferences;
}

void idRenderWorldCommitted::AddLightRefToArea(
    idRenderLightCommitted* light, worldArea_t* area) {
  const lightClass_t lightClass = light->r.lightClass;
  const bool drawLight = lightClass == LIGHT_SLOWLIGHT_DYNAMIC_ONLY ||
                         lightClass == LIGHT_SLOWLIGHT_PRIVATE_ONLY ||
                         lightClass == LIGHT_BLENDED_ONLY ||
                         lightClass == LIGHT_PRELIGHT_ONLY;
  const bool privateLight = lightClass == LIGHT_PRIVATE_DYNAMIC_ONLY ||
                            lightClass == LIGHT_PRIVATE_PRIME;
  if (isComboMap || !ContainsArea(light->references, area))
    light->references.Append(area);

  idList<idRenderIndex, 62>* refs = nullptr;
  if (drawLight || !isComboMap)
    refs = &area->drawLightRefs;
  else if (privateLight)
    refs = &area->approximatedLightPrivateLightRefs;
  else
    refs = &area->approximatedLightRefs;
  if (isComboMap || !ContainsIndex(*refs, light->index.index))
    refs->Append(light->index);
  ++tr.pc.c_lightReferences;
}

void idRenderWorldCommitted::PushFrustumIntoTree_r(
    idRenderModelCommitted* model, idRenderLightCommitted* light,
    const frustumCorners_t* corners, int nodeNum) {
  while (nodeNum >= 0) {
    areaNode_t& node = areaNodes[nodeNum];
    const int cull =
        idRenderMatrix::CullFrustumCornersToPlane(*corners, node.plane);
    if (cull == kFrustumCullCross) {
      if (node.children[0] != 0)
        PushFrustumIntoTree_r(model, light, corners, node.children[0]);
    } else if (cull == kFrustumCullFront) {
      nodeNum = node.children[0];
      if (nodeNum == 0)
        return;
      continue;
    }
    nodeNum = node.children[1];
    if (nodeNum == 0)
      return;
  }
  worldArea_t* area = &worldAreas[-1 - nodeNum];
  if (model != nullptr)
    AddModelRefToArea(model, area);
  else
    AddLightRefToArea(light, area);
}

void idRenderWorldCommitted::LinkLightIntoTree(
    idRenderLightCommitted* light, const idRenderMatrix& frustumTransform,
    const idBounds& frustumBounds) {
  if (areaNodes == nullptr)
    return;
  frustumCorners_t corners{};
  idRenderMatrix::GetFrustumCorners(corners, frustumTransform, frustumBounds);
  PushFrustumIntoTree_r(nullptr, light, &corners, 0);
}

void idRenderWorldCommitted::UnlinkLightFromTree(
    idRenderLightCommitted* light) {
  for (int i = 0; i < light->references.num; ++i) {
    worldArea_t* area = light->references.list[i];
    RemoveIndexFast(area->drawLightRefs, light->index.index);
    RemoveIndexFast(area->approximatedLightRefs, light->index.index);
    RemoveIndexFast(area->approximatedLightPrivateLightRefs,
                    light->index.index);
  }
  light->references.Clear();
}

void idRenderWorldCommitted::LinkModelIntoTree(
    idRenderModelCommitted* model, const idRenderMatrix& frustumTransform,
    const idBounds& frustumBounds) {
  if (areaNodes == nullptr)
    return;
  frustumCorners_t corners{};
  idRenderMatrix::GetFrustumCorners(corners, frustumTransform, frustumBounds);
  PushFrustumIntoTree_r(model, nullptr, &corners, 0);
}

void idRenderWorldCommitted::UnlinkModelFromTree(
    idRenderModelCommitted* model) {
  for (int i = 0; i < model->references.num; ++i)
    RemoveIndexFast(model->references.list[i]->modelRefs, model->index.index);
  model->references.Clear();
}

idRenderWorldCommitted::idRenderWorldCommitted() {
  mapName = nullptr;
  isComboMap = false;
  isToolsWorld = false;
  hasWorldGeometry = false;
  hasWorldSurfaceOffsets = false;
  areaNodes = nullptr;
  areaNodeBounds = nullptr;
  areaNodeOcclusionIndex = nullptr;
  numAreaNodes = 0;
  worldAreas = nullptr;
  worldAreaSubspaceBounds = nullptr;
  worldAreaGeometryBounds = nullptr;
  numWorldAreas = 0;
  binaryModelNames.Clear();
  binaryModelHash.Clear();
  binaryModelState.Clear();
  dynamicEnvironments.Clear();
  testEnv = nullptr;
  SetBinaryModelState("world", true, true);
  committedRenderModels.SetNum(8192, nullptr);
  committedRenderLights.SetNum(8192, nullptr);
  addAlwaysModels = nullptr;
  dimShadowModels = nullptr;
  approxLightModels = nullptr;
  occlusionBaseModel = nullptr;
  transparencyRenderModel = nullptr;
  std::memset(&postCommitModels, 0, sizeof(postCommitModels));
  std::memset(&postCommitLights, 0, sizeof(postCommitLights));
  approximateLightingParms = static_cast<approximateLightingParms_t*>(
      idMem::Singleton()->AllocWithLocation(
          "w:\\build\\tech5\\engine\\renderer\\RenderWorldCommitted.cpp(75) : TAG_RENDERWORLD",
          sizeof(approximateLightingParms_t) * kMaxApproximateLightingParms,
          TAG_RENDERWORLD, false, ALIGN_128, HEAP_DEFAULTHEAP));
  numApproximateLightingParms = 0;
  numWantedApproximateLightingParms = 0;
  approximateLightingLights =
      static_cast<const idRenderLightCommitted**>(
          idMem::Singleton()->AllocWithLocation(
              "w:\\build\\tech5\\engine\\renderer\\RenderWorldCommitted.cpp(79) : TAG_RENDERWORLD",
              sizeof(idRenderLightCommitted*) * kMaxApproximateLightingLights,
              TAG_RENDERWORLD, false, ALIGN_128, HEAP_DEFAULTHEAP));
  numApproximateLightingLights = 0;
  assetRemappings = nullptr;
  committedDebugData = new idRenderDebugLocal;
  rendererJobList = parallelJobManager->AllocJobList(
      JOBLIST_RENDERER, JOBLIST_PRIORITY_MEDIUM, 1024, 16,
      &idColor::colorMagenta);
}

idRenderWorldCommitted::~idRenderWorldCommitted() {
  if (rendererJobList != nullptr) {
    parallelJobManager->FreeJobList(rendererJobList);
    rendererJobList = nullptr;
  }
  for (int areaIndex = 0; areaIndex < numWorldAreas; ++areaIndex) {
    worldArea_t& area = worldAreas[areaIndex];
    if (area.approximatedLightRefs.num != 0)
      idLib::Error("FreeWorld: unexpected remaining approximatedLightRefs");
    if (area.approximatedLightPrivateLightRefs.num != 0) {
      idLib::Error(
          "FreeWorld: unexpected remaining approximatedLightPrivateLightRefs");
    }
    if (area.drawLightRefs.num != 0)
      idLib::Error("FreeWorld: unexpected remaining drawLightRefs");
    if (area.modelRefs.num != 0)
      idLib::Error("FreeWorld: unexpected remaining modelRefs");
    for (int surfaceIndex = 0; surfaceIndex < area.worldSurfaces.num;
         ++surfaceIndex) {
      idRenderModelSurface& surface = area.worldSurfaces.list[surfaceIndex];
      if (!surface.geometryIsReference) {
        delete surface.geometry;
        surface.geometry = nullptr;
      }
    }
    area.worldSurfaces.ClearFree();
  }
  delete[] worldAreas;
  worldAreas = nullptr;
  delete[] worldAreaSubspaceBounds;
  worldAreaSubspaceBounds = nullptr;
  delete[] worldAreaGeometryBounds;
  worldAreaGeometryBounds = nullptr;
  numWorldAreas = 0;
  delete[] areaNodes;
  areaNodes = nullptr;
  delete[] areaNodeBounds;
  areaNodeBounds = nullptr;
  delete[] areaNodeOcclusionIndex;
  areaNodeOcclusionIndex = nullptr;
  numAreaNodes = 0;
  idMem::Singleton()->Free(approximateLightingParms, ALIGN_128);
  approximateLightingParms = nullptr;
  idMem::Singleton()->Free(
      const_cast<idRenderLightCommitted**>(approximateLightingLights),
      ALIGN_128);
  approximateLightingLights = nullptr;
  delete committedDebugData;
  committedDebugData = nullptr;
}

template idRenderIndex* idListArrayNew<idRenderIndex>(int, unsigned, bool);
template idRenderIndex* idListArrayResize<idRenderIndex>(
    idRenderIndex*, int, int, unsigned, bool);
template int idList<idRenderIndex, 62>::Append(const idRenderIndex&);
template idStaticList<idStr, 1024>::~idStaticList();
