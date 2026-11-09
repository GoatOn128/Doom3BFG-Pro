#include "engine/models/particles/rendermodelparticle.h"

#include "engine/framework/coderesource.h"
#include "engine/models/renderworld.h"
#include "engine/models/transparency/rendermodeltransparency.h"
#include "engine/renderer/autorenderparms.h"
#include "engine/renderer/declmaterial.h"
#include "engine/renderer/globalshadows.h"
#include "engine/renderer/imagemanager.h"
#include "engine/renderer/render.h"
#include "engine/renderer/render_tools.h"
#include "shared/idlib/csystems/cvarsystemlocal.h"
#include "shared/idlib/lib.h"
#include "shared/idlib/parallelism/paralleljoblist.h"
#include "shared/idlib/sys/sys_alloc.h"

#include <algorithm>
#include <cmath>
#include <cstdio>
#include <cstdint>
#include <new>

idCVar r_showParticleBounds("r_showParticleBounds", "0", CVAR_BOOL,
                             "draw particle bounds");
idCVar r_showParticleDistVol(
    "r_showParticleDistVol", "0", CVAR_BOOL,
    "visualize the distribution volume for each stage of the particle");
idCVar r_showParticleInfo(
    "r_showParticleInfo", "0", CVAR_INTEGER,
    "1 = show particle counts depth-tested, 2 = show particle counts non-depth-tested, 3 = dump particle info to console");
idCVar r_skipParticles(
    "r_skipParticles", "0", CVAR_INTEGER,
    "1 = skip all particle systems, 2 = skip all non-transsort particles, 3 = skip all transsort particles");
idCVar r_useParticleGenJobs("r_useParticleGenJobs", "1", CVAR_INTEGER,
                            "0 = off, 1 = threaded", 0.0f, 1.0f);
idCVar r_testParticleLOD("r_testParticleLOD", "0", CVAR_INTEGER,
                         "which lod level to apply", 0.0f, 3.0f);
idCVar r_particleBatchStages(
    "r_particleBatchStages", "2", CVAR_INTEGER,
    "1 = batch particle jobs, 2 = batch particle surfaces", 0.0f, 2.0f);

idCodeResource<idDeclRenderParm> rp_particleLightingEnable(
    "particleLightingEnable");
idCodeResource<idDeclRenderParm> rpParticleAlphaScale("ParticleAlphaScale");
idCodeResource<idDeclRenderParm> rpParticleScale("ParticleScale");
idCodeResource<idDeclRenderParm> rpParticleVel("ParticleVel");
idCodeResource<idDeclRenderParm> rpParticleLOD("ParticleLOD");

namespace {

constexpr memTag_t kParticleTag = static_cast<memTag_t>(0x4f);

template <typename Type>
Type* AllocateParticle(const char* location) {
  return static_cast<Type*>(idMem::Singleton()->AllocWithLocation(
      location, sizeof(Type), kParticleTag, false, ALIGN_16,
      HEAP_DEFAULTHEAP));
}

const idMat3& ActiveAxis(const idRenderModelParticle& model) {
  return (model.useDeferredPosition & 1U) != 0U ? model.deferredAxis
                                               : model.g.axis;
}

const idVec3& ActiveOrigin(const idRenderModelParticle& model) {
  return (model.useDeferredPosition & 1U) != 0U ? model.deferredOrigin
                                               : model.g.origin;
}

idVec3 TransformVector(const idMat3& axis, const idVec3& value) {
  return {value.x * axis[0].x + value.y * axis[0].y + value.z * axis[0].z,
          value.x * axis[1].x + value.y * axis[1].y + value.z * axis[1].z,
          value.x * axis[2].x + value.y * axis[2].y + value.z * axis[2].z};
}

float ClampUnit(float value) { return std::max(0.0f, std::min(value, 1.0f)); }

} // namespace

void idRenderModelParticle::DeclNameToModelName(const char* declName,
                                                 idStr& modelName) {
  modelName = declName;
  modelName.SetFileExtension("prt");
}

void idRenderModelParticle::SetMeshDistribution(
    const particleMeshDist_t* mesh) {
  particleMeshDist = mesh;
  if (mesh == nullptr) {
    referenceBounds = particleDecl->bounds;
    return;
  }
  referenceBounds[0] = particleDecl->bounds[0] + mesh->bounds[0];
  referenceBounds[1] = particleDecl->bounds[1] + mesh->bounds[1];
}

void idRenderModelParticle::DrawDistributionVolumes() {
  idRenderDebug* renderDebug = GetRenderDebug();
  if (renderDebug == nullptr)
    return;

  const idLookupTable* tables = particleDecl->GetTables();
  const idMat3& axis = ActiveAxis(*this);
  const idVec3& modelOrigin = ActiveOrigin(*this);
  const idVec4 scaleParm = gameParmBlock.GetVector(rpParticleScale.Get());
  const idVec3 distributionScale{scaleParm.x, scaleParm.y, scaleParm.z};

  for (int stageIndex = 0; stageIndex < particleDecl->stages.Num();
       ++stageIndex) {
    const idParticleStage* stage = particleDecl->stages[stageIndex];
    const idPartImposterData* imposter = particleDecl->imposterData[stageIndex];
    const int locationCount = imposter->sumLocations + 1;
    for (int locationIndex = 0; locationIndex < locationCount;
         ++locationIndex) {
      const prtLocation_t* location =
          locationIndex == 0 ? nullptr : &imposter->locData[locationIndex - 1];
      idVec3 offset{
          stage->offset.offset[0].GetMaxParmVal(tables),
          stage->offset.offset[1].GetMaxParmVal(tables),
          stage->offset.offset[2].GetMaxParmVal(tables)};
      if (location != nullptr) {
        offset.x += location->loc[0].GetMaxParmVal(tables);
        offset.y += location->loc[1].GetMaxParmVal(tables);
        offset.z += location->loc[2].GetMaxParmVal(tables);
      }
      const idVec3 origin = modelOrigin + TransformVector(axis, offset);
      const idVec3 extents{
          stage->distribution.size[0].GetMaxParmVal(tables) *
              distributionScale.x,
          stage->distribution.size[1].GetMaxParmVal(tables) *
              distributionScale.y,
          stage->distribution.size[2].GetMaxParmVal(tables) *
              distributionScale.z};
      const float radius = std::sqrt(std::max(
          extents.x * extents.x + extents.y * extents.y +
              extents.z * extents.z,
          1.17549435e-38f));
      if (radius == 0.0f) {
        renderDebug->DebugPoint(idColor::colorRed, origin, 0, false);
        continue;
      }
      const idColor color = BrightColorForNumber(stageIndex);
      switch (stage->distribution.type) {
      case PDIST_RECT:
      case PDIST_RECT_SURFACE: {
        const idBox box{origin, extents, axis};
        renderDebug->DebugBox(color, box, 0, false);
        break;
      }
      case PDIST_CYLINDER:
      case PDIST_CYLINDER_SURFACE:
        renderDebug->DebugCone(color, origin, axis[2], radius, radius, 0,
                               false);
        break;
      case PDIST_SPHERE:
      case PDIST_SPHERE_SURFACE: {
        const idSphere sphere{origin, radius};
        renderDebug->DebugSphere(color, sphere, 12, 0, false);
        break;
      }
      default:
        break;
      }
    }
  }
}

int idRenderModelParticle::EstimateVertAllocation(
    const idParticleStage* stage, int renderTime, float timeOffset,
    int stopTime, float deadTime, int lodLevel) {
  if (stage->hidden || stage->cycleMsec == 0)
    return 0;
  const int lifeMsec = static_cast<int>(stage->maxParticleLife * 1000.0f);
  const int totalDuration =
      static_cast<int>(deadTime * 1000.0f +
                       stage->maxParticleLife * 1000.0f);
  if (totalDuration <= 0)
    return 0;

  const int stageTime = static_cast<int>(
      (static_cast<float>(renderTime) * 0.001f -
       (timeOffset + stage->systemProperties.timeOffset)) *
      1000.0f);
  const int bunchMsec = static_cast<int>(
      stage->systemProperties.spawnBunching * stage->bunchTime * 1000.0f);
  const float lodFraction =
      ClampUnit(static_cast<float>(lodLevel) * stage->lodParms.lerpAmount);
  const int particleCount = static_cast<int>(
      (static_cast<float>(stage->lodParms.totalParticles) -
       static_cast<float>(stage->systemProperties.totalParticles)) *
          lodFraction +
      static_cast<float>(stage->systemProperties.totalParticles));

  unsigned int result = 0;
  int bunchOffset = 0;
  for (int particle = 0; particle < particleCount; ++particle) {
    const int age =
        stageTime - bunchOffset / stage->systemProperties.totalParticles;
    if (age >= 0 &&
        (stage->systemProperties.cycles == 0 ||
         age / totalDuration < stage->systemProperties.cycles)) {
      const int cycleAge = age % totalDuration;
      if ((stopTime == 0 || renderTime - cycleAge < stopTime) &&
          cycleAge >= 0 && cycleAge <= lifeMsec) {
        result += stage->NumVertsPerParticle();
      }
    }
    bunchOffset += bunchMsec;
  }
  return static_cast<int>(result);
}

idRenderModelParticle::idRenderModelParticle(const idDeclParticle* decl)
    : particleDecl(decl) {
  renderModelType = RENDERMODEL_PARTICLE;
  particleMeshDist = nullptr;
  SetName(decl->name.str);
  referenceBounds = decl->bounds;
  g.flags0 |= 0x80U;
  g.flags1 |= 0x01U;
  g.flags2 |= 0x20U;
  g.flags4 |= 0x04U;
  g.modelDepthSort = MODEL_DEPTH_SORT_AFTER_WORLD;
  SetPreventApproximateLightingCalculation(true);

  const int numStages = decl->stages.Num();
  for (int stageIndex = 0; stageIndex < numStages; ++stageIndex) {
    const idParticleStage* stage = decl->stages[stageIndex];
    if (stage->systemProperties.material == nullptr) {
      idLib::Warning("idDeclParticle '%s' stage %d does not have a valid material",
                     decl->name.str, stageIndex);
    }
    idRenderModelSurface surface{};
    surface.material = stage->systemProperties.material;
    surface.geometry = new idTriangles{};
    surface.geometry->bounds = stage->bounds;
    surface.geometry->vertexMask = 3167;
    AddSurface(surface);
  }
  for (idList<stageCounts_t, 5>& counts : stageCounts)
    counts.SetNum(numStages);
  genParms.SetNum(numStages);

  particleRenderView = AllocateParticle<particleRenderView_t>(
      "w:\\build\\tech5\\engine\\models\\particles\\RenderModelParticle.cpp(102) : TAG_PARTICLE");
  modelParms = AllocateParticle<modelParticleParms_t>(
      "w:\\build\\tech5\\engine\\models\\particles\\RenderModelParticle.cpp(103) : TAG_PARTICLE");
  lightingFullParmBlock.SetParm(rp_particleLightingEnable.Get(),
                                idVec4{1.0f, 0.0f, 0.0f, 0.0f});
  lightingAverageParmBlock.SetParm(rp_particleLightingEnable.Get(),
                                   idVec4{0.0f, 1.0f, 0.0f, 0.0f});
}

idRenderModelParticle::~idRenderModelParticle() {
  idMem* memory = idMem::Singleton();
  memory->Free(particleRenderView, ALIGN_16);
  memory->Free(modelParms, ALIGN_16);
  particleRenderView = nullptr;
  modelParms = nullptr;
  for (idList<stageCounts_t, 5>& counts : stageCounts)
    counts.ClearFree();
}

bool idRenderModelParticle::UpdateInView(
    const idRenderView* currentView, const idRenderView* nextView,
    idRenderModelUpdateTools* updateTools) {
  if (r_skipParticles.valueInteger != 0) {
    for (int surfaceIndex = 0; surfaceIndex < surfaces.Num(); ++surfaceIndex) {
      surfaces[surfaceIndex].geometry->numVerts = 0;
      surfaces[surfaceIndex].geometry->numIndexes = 0;
    }
    return false;
  }

  const int useParticleJobs = r_useParticleGenJobs.valueInteger;
  particleRenderView->viewOrg = nextView->r.vieworg;
  particleRenderView->viewFwd = nextView->r.viewaxis[0];
  particleRenderView->viewLeft = nextView->r.viewaxis[1];
  particleRenderView->viewUp = nextView->r.viewaxis[2];
  particleRenderView->renderTime = currentView->r.renderTime;
  particleRenderView->deltaTime = 0;
  particleRenderView->atlasWidth = static_cast<std::uint16_t>(
      globalImages->transSortImageAtlas->ATLAS_WIDTH);
  particleRenderView->atlasHeight = static_cast<std::uint16_t>(
      globalImages->transSortImageAtlas->ATLAS_HEIGHT);

  const float diversity = gameParmBlock.GetFloat(rp_Diversity.Get());
  modelParms->stopTime =
      static_cast<int>(gameParmBlock.GetFloat(rp_TimeStop.Get()) * 1000.0f);
  modelParms->timeOffset = gameParmBlock.GetFloat(rp_TimeOffset.Get());
  modelParms->color = gameParmBlock.GetVector(rp_Color.Get());
  modelParms->coverage = gameParmBlock.GetFloat(rp_coverage.Get());
  modelParms->wind = gameParmBlock.GetVector(rp_Wind.Get());
  const idVec4 distributionScale =
      gameParmBlock.GetVector(rpParticleScale.Get());
  modelParms->distribScale = {distributionScale.x, distributionScale.y,
                              distributionScale.z};
  const bool sampleShadows = gameParmBlock.GetFloat(rp_NoShadows.Get()) == 0.0f;
  const idVec4 velocity = gameParmBlock.GetVector(rpParticleVel.Get());
  modelParms->velocity = TransformVector(
      ActiveAxis(*this), {velocity.x, velocity.y, velocity.z});
  int lod = r_testParticleLOD.valueInteger;
  if (lod <= 0)
    lod = static_cast<int>(gameParmBlock.GetFloat(rpParticleLOD.Get()));
  modelParms->lod = static_cast<std::uint8_t>(lod);
  modelParms->alphaScaleOverride =
      gameParmBlock.GetFloat(rpParticleAlphaScale.Get());

  const idVec3 distance = currentView->r.vieworg - ActiveOrigin(*this);
  const float distanceSquared = distance.x * distance.x + distance.y * distance.y +
                                distance.z * distance.z;
  if (g.maxVisibleRange > 0.0f && g.fadeVisibilityOver > 0.0f) {
    const float fadeSquared = g.fadeVisibilityOver * g.fadeVisibilityOver;
    const float visibleSquared = g.maxVisibleRange * g.maxVisibleRange;
    modelParms->coverage *= ClampUnit(
        1.0f - (distanceSquared - fadeSquared) /
                   (visibleSquared - fadeSquared));
  } else if (g.fadeVisibilityOver < 0.0f) {
    const float fadeSquared = g.fadeVisibilityOver * g.fadeVisibilityOver;
    modelParms->coverage *=
        ClampUnit((distanceSquared + fadeSquared) / -fadeSquared);
  }

  modelParms->shadow = sampleShadows
                           ? static_cast<const shadowMapCPU_t*>(
                                 updateTools->globalShadowMap)
                                 ->ShadowSample(modelParms->origin)
                           : 1.0f;
  modelParms->origin = ActiveOrigin(*this);
  modelParms->axis = ActiveAxis(*this);
  modelParms->scale = g.scale;
  modelParms->diversity = static_cast<int>(diversity * 32767.0f);

  idList<stageCounts_t, 5>& currentCounts =
      stageCounts[idRenderModelTransparency::mappedBufferIndex];
  const int previousBuffer =
      (idRenderModelTransparency::mappedBufferIndex + 2) % 3;
  idList<stageCounts_t, 5>& previousCounts = stageCounts[previousBuffer];
  const bool mergeSurfaces = particleDecl->mergeSurfaces &&
                             r_particleBatchStages.valueInteger == 2 &&
                             useParticleJobs != 2;

  if (r_showParticleBounds.valueInteger != 0) {
    idRenderDebug* debug = GetRenderDebug();
    if (debug != nullptr) {
      const idVec3 localCenter{
          (particleDecl->bounds[0].x + particleDecl->bounds[1].x) * 0.5f,
          (particleDecl->bounds[0].y + particleDecl->bounds[1].y) * 0.5f,
          (particleDecl->bounds[0].z + particleDecl->bounds[1].z) * 0.5f};
      const idVec3 center =
          ActiveOrigin(*this) + TransformVector(ActiveAxis(*this), localCenter);
      char label[256];
      std::snprintf(label, sizeof(label), "%d:%s", GetIndex(),
                    particleDecl->name.str);
      debug->DebugText(label, center, 1.0f, idColor::colorRed, 1, 0,
                       true, false);
      idBounds transformed;
      idVec3 boundsScale{g.scale.x * modelParms->distribScale.x,
                         g.scale.y * modelParms->distribScale.y,
                         g.scale.z * modelParms->distribScale.z};
      transformed.FromTransformedBounds(particleDecl->bounds,
                                        ActiveOrigin(*this),
                                        ActiveAxis(*this), boundsScale);
      debug->DebugBounds(idVec4{1.0f, 0.0f, 0.0f, 1.0f}, transformed,
                         vec3_origin, 0, false);
    }
  }
  if (r_showParticleDistVol.valueInteger != 0)
    DrawDistributionVolumes();

  if (updateTools->pc.showStats || r_showParticleInfo.valueInteger != 0) {
    int totalVerts = 0;
    for (int stageIndex = 0; stageIndex < previousCounts.Num(); ++stageIndex)
      totalVerts += previousCounts[stageIndex].vertCount;
    updateTools->pc.c_numParticleVertices += totalVerts;
    if (r_showParticleInfo.valueInteger != 0) {
      idRenderDebug* debug = GetRenderDebug();
      if (debug != nullptr) {
        const idVec3 localCenter{
            (particleDecl->bounds[0].x + particleDecl->bounds[1].x) * 0.5f,
            (particleDecl->bounds[0].y + particleDecl->bounds[1].y) * 0.5f,
            (particleDecl->bounds[0].z + particleDecl->bounds[1].z) * 0.5f};
        const idVec3 center = ActiveOrigin(*this) +
                              TransformVector(ActiveAxis(*this), localCenter);
        char text[256];
        std::snprintf(text, sizeof(text),
                      "%s : total verts %d total indices %d LOD %d",
                      particleDecl->name.str, totalVerts,
                      6 * (totalVerts >> 2), modelParms->lod);
        debug->DebugText(text, center, 1.0f, idColor::colorRed, 1, 0,
                         r_showParticleInfo.valueInteger == 1, false);
      }
    }
    if (r_showParticleInfo.valueInteger == 3) {
      idLib::Printf("%s : total verts %d total indices %d LOD %d\n",
                    particleDecl->name.str, totalVerts,
                    6 * (totalVerts >> 2), modelParms->lod);
    }
  }

  bool preventApproximateLighting = true;
  bool surfaceParmChanged = false;
  for (int stageIndex = 0; stageIndex < particleDecl->stages.Num();
       ++stageIndex) {
    if (stageIndex >= surfaces.Num())
      continue;
    idRenderModelSurface& stageSurface = surfaces[stageIndex];
    stageSurface.geometry->numVerts = 0;
    stageSurface.geometry->numIndexes = 0;
    stageSurface.material = nullptr;
    idRenderModelSurface& outputSurface =
        mergeSurfaces ? surfaces[0] : stageSurface;
    const idParticleStage* stage = particleDecl->stages[stageIndex];
    if (stage->systemProperties.material == nullptr ||
        stage->isTransparencySorted || stage->cycleMsec == 0 ||
        stage->hidden) {
      continue;
    }

    idParmBlock* oldParms = stageSurface.parms;
    if (stage->systemProperties.lightingType == PLIGHTING_TYPE_FULL) {
      stageSurface.referenceMask |= 0x20U;
      stageSurface.parms = &lightingFullParmBlock;
      preventApproximateLighting = false;
    } else if (stage->systemProperties.lightingType ==
               PLIGHTING_TYPE_AVERAGE) {
      stageSurface.referenceMask |= 0x20U;
      stageSurface.parms = &lightingAverageParmBlock;
      preventApproximateLighting = false;
    } else {
      stageSurface.parms = nullptr;
    }
    surfaceParmChanged |= oldParms != stageSurface.parms;

    const int vertCount = previousCounts[stageIndex].vertCount;
    outputSurface.material = stage->systemProperties.material;
    outputSurface.geometry->numVerts += vertCount;
    outputSurface.geometry->numIndexes += 6 * (vertCount >> 2);
    idBounds stageBounds;
    if (stage->distribution.type == PDIST_MESH_SURFACE &&
        particleMeshDist != nullptr) {
      stageBounds[0] = stage->bounds[0] + particleMeshDist->bounds[0];
      stageBounds[1] = stage->bounds[1] + particleMeshDist->bounds[1];
    } else {
      stageBounds[0] = stage->bounds[0] - modelParms->distribScale;
      stageBounds[1] = stage->bounds[1] + modelParms->distribScale;
    }

    if (mergeSurfaces && stageIndex != 0) {
      outputSurface.geometry->bounds.AddPoint(stageBounds[0]);
      outputSurface.geometry->bounds.AddPoint(stageBounds[1]);
    } else {
      outputSurface.geometry->bounds = stageBounds;
      outputSurface.geometry->vertexBuffer.Reference(
          idRenderModelTransparency::vertexBufferUnsorted[previousBuffer]);
      outputSurface.geometry->indexBuffer.Reference(
          idRenderModelTransparency::indexBufferUnsorted,
          2 * previousCounts[stageIndex].indexOffset,
          2 * outputSurface.geometry->numIndexes);
    }
  }
  if (mergeSurfaces && particleDecl->stages.Num() != 0 &&
      surfaces.Num() != 0) {
    idRenderModelSurface& surface = surfaces[0];
    surface.geometry->vertexBuffer.Reference(
        idRenderModelTransparency::vertexBufferUnsorted[previousBuffer]);
    surface.geometry->indexBuffer.Reference(
        idRenderModelTransparency::indexBufferUnsorted,
        2 * previousCounts[0].indexOffset,
        2 * surface.geometry->numIndexes);
  }
  if (preventApproximateLighting !=
          SetPreventApproximateLightingCalculation(
              preventApproximateLighting) ||
      surfaceParmChanged) {
    CommitThisFrame();
  }

  const visibleInfluenceSpheres_t* visibleSpheres =
      updateTools->transparencyModel->visibleInfluenceSpheres;
  idRandom2 deadTimeRandom{static_cast<std::uint32_t>(modelParms->diversity)};
  const bool batchJobs = r_particleBatchStages.valueInteger != 0 &&
                         useParticleJobs != 2;
  int acceptedStages = 0;
  int mergedQuads = 0;

  for (int stageIndex = 0; stageIndex < particleDecl->stages.Num();
       ++stageIndex) {
    if (stageIndex >= surfaces.Num())
      continue;
    currentCounts[stageIndex] = {};
    const idParticleStage* stage = particleDecl->stages[stageIndex];
    if (stage->systemProperties.material == nullptr)
      continue;
    const float deadTime = stage->systemProperties.deadTime.Compute(
        particleDecl->GetTables(), 1.0f, deadTimeRandom);
    const int estimatedVerts = EstimateVertAllocation(
        stage, particleRenderView->renderTime, modelParms->timeOffset,
        modelParms->stopTime, deadTime, modelParms->lod);
    if (estimatedVerts == 0)
      continue;

    deferredParticleGenParms_t& parms = genParms[acceptedStages];
    const idPartImposterData* imposter = particleDecl->imposterData[stageIndex];
    parms.numLocations = imposter->sumLocations;
    const int maxVerts =
        std::min((imposter->sumLocations + 1) * estimatedVerts, 512);
    const int numQuads = maxVerts >> 2;
    parms.numJobs = 1;
    parms.location = imposter->locData;
    parms.mergedSurfaces = mergeSurfaces;
    parms.renderView = particleRenderView;
    parms.effectParticleParms = nullptr;
    parms.modelParticleParms = modelParms;
    parms.stage = stage;
    parms.tables = particleDecl->GetTables();
    parms.staticVerts = stage->GetStaticVerts();
    parms.numEffectParticleParms = 0;
    parms.numModelParticleParms = 1;
    parms.numTables = particleDecl->tables.Num();
    parms.staticVertsSize = stage->GetNumStaticVerts();
    parms.deadTime = deadTime;
    parms.visibleInfluenceSpheres = visibleSpheres;
    mergedQuads += mergeSurfaces ? numQuads : 0;
    parms.maxVertsToGen = 4 * (mergeSurfaces ? mergedQuads : numQuads);
    parms.verts = nullptr;
    parms.quadDepth = nullptr;
    parms.quadsUsed = nullptr;
    if (particleMeshDist != nullptr)
      parms.meshDistribution = *particleMeshDist;
    else
      parms.meshDistribution.ConstructEmpty();

    bool allocated = mergeSurfaces;
    if (stage->systemProperties.material->MaterialIsTransparencySorted()) {
      if (r_skipParticles.valueInteger == 1 ||
          r_skipParticles.valueInteger == 3) {
        allocated = false;
      } else if (!mergeSurfaces) {
        allocated = idRenderModelTransparency::AllocateQuadSegment(
            numQuads, parms.verts, parms.quadDepth, parms.quadsUsed,
            stage->hasEmissivePass);
      }
    } else {
      if (r_skipParticles.valueInteger == 1 ||
          r_skipParticles.valueInteger == 2) {
        allocated = false;
      } else if (!mergeSurfaces) {
        allocated = idRenderModelTransparency::AllocateUnsortedQuadSegment(
            numQuads, parms.verts, currentCounts[stageIndex].indexOffset,
            currentCounts[stageIndex].vertCount);
      }
    }
    if (allocated)
      ++acceptedStages;
  }

  if (acceptedStages <= 0)
    return true;

  int jobsToDispatch = acceptedStages;
  if (batchJobs) {
    genParms[0].numJobs = static_cast<std::uint16_t>(acceptedStages);
    jobsToDispatch = 1;
    if (mergeSurfaces) {
      const idParticleStage* firstStage = particleDecl->stages[0];
      bool allocated;
      if (firstStage->systemProperties.material
              ->MaterialIsTransparencySorted()) {
        allocated = idRenderModelTransparency::AllocateQuadSegment(
            mergedQuads, genParms[0].verts, genParms[0].quadDepth,
            genParms[0].quadsUsed, firstStage->hasEmissivePass);
      } else {
        allocated = idRenderModelTransparency::AllocateUnsortedQuadSegment(
            mergedQuads, genParms[0].verts, currentCounts[0].indexOffset,
            currentCounts[0].vertCount);
      }
      if (!allocated)
        jobsToDispatch = 0;
      for (int parmIndex = 1; parmIndex < acceptedStages; ++parmIndex) {
        genParms[parmIndex].verts = genParms[0].verts;
        genParms[parmIndex].quadDepth = genParms[0].quadDepth;
      }
    }
  }

  for (int jobIndex = 0; jobIndex < jobsToDispatch; ++jobIndex) {
    deferredParticleGenParms_t* parms = &genParms[jobIndex];
    if (useParticleJobs == 1)
      updateTools->renderModelJobList->AddJob(
          reinterpret_cast<jobRun_t>(ParticleGenJob), parms);
    else
      ParticleGenJob(parms);
  }
  return true;
}
