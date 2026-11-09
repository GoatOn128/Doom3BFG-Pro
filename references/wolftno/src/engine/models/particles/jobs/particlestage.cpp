#include "engine/models/particles/jobs/particlegen.h"

#include "engine/framework/coderesource.h"
#include "engine/models/particles/jobs/staticparticlemodeldata.h"
#include "engine/renderer/declmaterial.h"
#include "engine/renderer/declrenderprog.h"

#include <algorithm>
#include <cmath>
#include <cstdint>
#include <cstring>
#include <immintrin.h>

namespace {

const __m128 vector_float_abs_mask = _mm_castsi128_ps(
    _mm_setr_epi32(0x7fffffff, 0x7fffffff, 0x7fffffff, 0x7fffffff));
const __m128 vector_float_sign_bit = _mm_castsi128_ps(
    _mm_setr_epi32(static_cast<int>(0x80000000u),
                   static_cast<int>(0x80000000u),
                   static_cast<int>(0x80000000u),
                   static_cast<int>(0x80000000u)));
const __m128 vector_float_last_sign_bit = _mm_castsi128_ps(
    _mm_setr_epi32(0, 0, 0, static_cast<int>(0x80000000u)));
const __m128 vector_float_first_sign_bit = _mm_castsi128_ps(
    _mm_setr_epi32(static_cast<int>(0x80000000u), 0, 0, 0));
const __m128 vector_float_clear_last = _mm_castsi128_ps(
    _mm_setr_epi32(-1, -1, -1, 0));
const __m128 DC_OFFSET_V128 =
    _mm_castsi128_ps(_mm_set1_epi32(0x26901d7d));

idCodeResource<idMaterial> mtrDefaultParticle("_defaultParticle");

void InitializeParticleParm(idParticleParm& parm) {
  parm.val0 = 0.0f;
  parm.val1 = 0.0f;
  parm.variance = 0.0f;
  parm.tableIdx = -1;
  parm.table2Idx = -1;
  parm.calcType = PARTICLE_CALC_NONE;
}

void SetParticleParmAsConstant(idParticleParm& parm, float value,
                               float variance) {
  parm.val0 = value;
  parm.val1 = value;
  parm.variance = variance;
  parm.calcType = PARTICLE_CALC_GENERIC;
}

void SetParticleParmAsIntegrate(idParticleParm& parm, float from, float to,
                                float variance) {
  parm.val0 = from;
  parm.val1 = to;
  parm.variance = variance;
  parm.calcType = PARTICLE_CALC_PARAMETRIC_INTEGRATE;
}

void SetParticleParmAsEval(idParticleParm& parm, float from, float to,
                           float variance) {
  parm.val0 = from;
  parm.val1 = to;
  parm.variance = variance;
  parm.calcType = PARTICLE_CALC_PARAMETRIC_EVAL;
}

void SetParticleParmAsMinMax(idParticleParm& parm, float minimum,
                             float maximum) {
  parm.val0 = minimum;
  parm.val1 = maximum;
  parm.variance = 0.0f;
  parm.calcType = PARTICLE_CALC_GENERIC;
}

void SetConstantRange(idParticleParm* parms, int count, float value) {
  for (int index = 0; index < count; ++index)
    SetParticleParmAsConstant(parms[index], value, 0.0f);
}

void SetIntegrateRange(idParticleParm* parms, int count) {
  for (int index = 0; index < count; ++index)
    SetParticleParmAsIntegrate(parms[index], 0.0f, 0.0f, 0.0f);
}

idMat3 IdentityParticleAxis() {
  idMat3 result{};
  result.mat[0].x = 1.0f;
  result.mat[1].y = 1.0f;
  result.mat[2].z = 1.0f;
  return result;
}

std::uint32_t NextBoundsRandom(std::uint32_t& seed) {
  seed = seed * 1664525u + 1013904223u;
  return seed;
}

} // namespace

void idParticleStage::operator=(const idParticleStage& src) {
  hidden = src.hidden;
  systemProperties.material = src.systemProperties.material;
  atlasScaleBias = src.atlasScaleBias;
  alphaBlended = src.alphaBlended;
  isTransparencySorted = src.isTransparencySorted;
  usesTransSortAtlas = src.usesTransSortAtlas;
  hasEmissivePass = src.hasEmissivePass;
  staticData = nullptr;
  systemProperties.totalParticles = src.systemProperties.totalParticles;
  systemProperties.diversity = src.systemProperties.diversity;
  systemProperties.cycles = src.systemProperties.cycles;
  cycleMsec = src.cycleMsec;
  bunchTime = src.bunchTime;
  systemProperties.particleLife = src.systemProperties.particleLife;
  maxParticleLife = src.maxParticleLife;
  maxDeadTime = src.maxDeadTime;
  systemProperties.timeOffset = src.systemProperties.timeOffset;
  systemProperties.deadTime = src.systemProperties.deadTime;
  systemProperties.useSysTime = src.systemProperties.useSysTime;
  systemProperties.spawnBunching = src.systemProperties.spawnBunching;
  systemProperties.emissionTime = src.systemProperties.emissionTime;
  systemProperties.textureFlipS = src.systemProperties.textureFlipS;
  systemProperties.textureFlipT = src.systemProperties.textureFlipT;
  systemProperties.windBias = src.systemProperties.windBias;
  systemProperties.sortType = src.systemProperties.sortType;
  systemProperties.boundsExpansion = src.systemProperties.boundsExpansion;
  systemProperties.randomOnCycle = src.systemProperties.randomOnCycle;
  systemProperties.lightingType = src.systemProperties.lightingType;
  bounds = src.bounds;
  distribution = src.distribution;
  orientation = src.orientation;
  direction = src.direction;
  speed = src.speed;
  acceleration = src.acceleration;
  gravity = src.gravity;
  friction = src.friction;
  offset = src.offset;
  spawnLocation = src.spawnLocation;
  colorAttributes = src.colorAttributes;
  rotation = src.rotation;
  initialRotation = src.initialRotation;
  pivot = src.pivot;
  size = src.size;
  texAnimation = src.texAnimation;
  customPath = src.customPath;
  genericParm = src.genericParm;
  lodParms = src.lodParms;
}

void idParticleStage::SetMaterial(const idMaterial* material) {
  systemProperties.material = material;
  if (material == nullptr)
    return;

  atlasScaleBias = material->atlasScaleBias;
  isTransparencySorted = material->MaterialIsTransparencySorted();
  usesTransSortAtlas = material->MaterialUsesTransSortAtlas();
  hasEmissivePass = material->GetProg(MPR_ADD) != nullptr;
  const idDeclRenderProg* stageProgram = material->GetProg(MPR_STAGE);
  if (isTransparencySorted) {
    alphaBlended = true;
  } else if (stageProgram != nullptr) {
    alphaBlended = (stageProgram->data.glState & 0x38u) != 8u;
    const materialCoverage_t coverage = material->Coverage();
    if (coverage == MC_OPAQUE || coverage == MC_PERFORATED ||
        coverage == MC_PERFORATED_FOR_MODELFADE) {
      alphaBlended = true;
    }
  }
}

void idParticleStage::SetStaticMesh(
    const idStaticParticleModelData* staticMesh) {
  staticData = staticMesh;
}

const idDrawVert* idParticleStage::GetStaticVerts() const {
  return staticData != nullptr ? staticData->staticVerts : nullptr;
}

int idParticleStage::GetNumStaticVerts() const {
  return staticData != nullptr ? staticData->numStaticVerts : 0;
}

int idParticleStage::NumVertsPerParticle() const {
  if (staticData != nullptr)
    return staticData->numStaticVerts;
  if (orientation.type == POR_TRAIL)
    return 4 * (orientation.numTrails + 1);
  return 4;
}

idParticleStage::idParticleStage() {
  std::memset(this, 0, sizeof(*this));

  auto initializeRange = [](idParticleParm* parms, int count) {
    for (int index = 0; index < count; ++index)
      InitializeParticleParm(parms[index]);
  };

  initializeRange(&systemProperties.particleLife, 1);
  initializeRange(&systemProperties.deadTime, 1);
  initializeRange(&systemProperties.windBias, 1);
  initializeRange(distribution.size, 3);
  initializeRange(&orientation.depthOffset, 1);
  initializeRange(speed.speed, 3);
  initializeRange(acceleration.acceleration, 3);
  initializeRange(&gravity.gravity, 1);
  initializeRange(friction.friction, 3);
  initializeRange(offset.offset, 3);
  initializeRange(spawnLocation.spawnLocation, 3);
  initializeRange(colorAttributes.baseColor, 4);
  initializeRange(&colorAttributes.brightness, 1);
  initializeRange(rotation.rotation, 3);
  initializeRange(initialRotation.initialAngle, 3);
  initializeRange(size.size, 3);
  initializeRange(&size.aspectRatio, 1);
  initializeRange(&texAnimation.rate, 1);
  initializeRange(customPath.parms, 5);
  initializeRange(genericParm.genericParm, 4);

  bounds.Clear();
  atlasScaleBias = {1.0f, 1.0f, 0.0f, 0.0f};
  systemProperties.randomOnCycle = true;
  systemProperties.lightingType = PLIGHTING_TYPE_OFF;
  distribution.type = PDIST_RECT;
  distribution.random = true;
  orientation.type = POR_VIEW;
  orientation.viewFade = 1.0f;
  orientation.aimedSafeQuadAlign = 1.0f;
  direction.type = PDIR_CONE;
  direction.coneAxis = IdentityParticleAxis();
  colorAttributes.softParticleAlphaScale = 1.0f;
  colorAttributes.entityColorBlendVal = 1.0f;
  rotation.allowRotDirOverride = true;
  texAnimation.type = PANIM_TYPE_SINGLE_CYCLE;
  texAnimation.useFrameBlending = true;
  customPath.type = PPATH_STANDARD;
  customPath.useRandomSpawn = true;
  lodParms.sizeScale = 1.0f;
  lodParms.totalParticles = 1;
}

void idParticleStage::Default(const idLookupTable* tables) {
  (void)tables;
  const idMaterial* material = mtrDefaultParticle.Get();
  systemProperties.material = material;
  staticData = nullptr;
  atlasScaleBias = material->atlasScaleBias;
  isTransparencySorted = material->MaterialIsTransparencySorted();
  usesTransSortAtlas = material->MaterialUsesTransSortAtlas();
  alphaBlended = false;
  hasEmissivePass = material->GetProg(MPR_ADD) != nullptr;

  systemProperties.totalParticles = 20;
  systemProperties.cycles = 0;
  systemProperties.diversity = 0;
  SetParticleParmAsConstant(systemProperties.particleLife, 1.5f, 0.0f);
  systemProperties.timeOffset = 0.0f;
  SetParticleParmAsConstant(systemProperties.deadTime, 0.0f, 0.0f);
  systemProperties.useSysTime = 0.0f;
  systemProperties.spawnBunching = 1.0f;
  systemProperties.emissionTime = 0.0f;
  systemProperties.textureFlipS = PTEXTURE_FLIP_NONE;
  systemProperties.textureFlipT = PTEXTURE_FLIP_NONE;
  SetParticleParmAsConstant(systemProperties.windBias, 0.0f, 0.0f);
  systemProperties.sortType = PSORT_TYPE_NONE;
  systemProperties.boundsExpansion = 0.0f;
  systemProperties.randomOnCycle = true;
  systemProperties.lightingType = PLIGHTING_TYPE_OFF;

  distribution.type = PDIST_RECT;
  SetConstantRange(distribution.size, 3, 0.0f);
  distribution.random = true;

  orientation.type = POR_VIEW;
  orientation.numTrails = 0;
  orientation.segmentLength = 0.0f;
  orientation.viewFade = 1.0f;
  orientation.orientToVelOnly = false;
  orientation.world = false;
  orientation.aimedSafeQuad = false;
  orientation.aimedSafeQuadAlign = 1.0f;
  SetParticleParmAsConstant(orientation.depthOffset, 0.0f, 0.0f);

  direction.type = PDIR_CONE;
  direction.parms[0] = 90.0f;
  direction.parms[1] = 0.0f;
  direction.parms[2] = 0.0f;
  direction.parms[3] = 0.0f;
  direction.angleOffsetStart[0] = 0.0f;
  direction.angleOffsetStart[1] = 0.0f;
  direction.angleOffsetRange[0] = 0.0f;
  direction.angleOffsetRange[1] = 0.0f;
  direction.coneAxis = IdentityParticleAxis();

  SetIntegrateRange(speed.speed, 3);
  SetConstantRange(acceleration.acceleration, 3, 0.0f);
  acceleration.world = false;
  SetParticleParmAsConstant(gravity.gravity, 0.0f, 0.0f);
  gravity.world = false;
  SetConstantRange(friction.friction, 3, 0.0f);
  SetConstantRange(offset.offset, 3, 0.0f);
  SetConstantRange(spawnLocation.spawnLocation, 3, 0.0f);

  SetConstantRange(colorAttributes.baseColor, 4, 1.0f);
  colorAttributes.fadeColor = {0.0f, 0.0f, 0.0f, 0.0f};
  colorAttributes.fadeInFraction = 0.1f;
  colorAttributes.fadeOutFraction = 0.25f;
  colorAttributes.fadeIndexFraction = 0.0f;
  colorAttributes.softParticleAlphaScale = 1.0f;
  SetParticleParmAsConstant(colorAttributes.brightness, 1.0f, 0.0f);
  colorAttributes.useGlobalShadows = false;
  colorAttributes.minShadowVal = 0.2f;
  colorAttributes.entityColorBlendVal = 1.0f;

  SetIntegrateRange(rotation.rotation, 3);
  rotation.allowRotDirOverride = true;
  for (idParticleParm& parm : initialRotation.initialAngle)
    SetParticleParmAsMinMax(parm, -360.0f, 360.0f);
  pivot.pivotOffset = {0.0f, 0.0f};
  for (idParticleParm& parm : size.size)
    SetParticleParmAsEval(parm, 4.0f, 4.0f, 0.0f);
  SetParticleParmAsEval(size.aspectRatio, 1.0f, 1.0f, 0.0f);

  texAnimation.type = PANIM_TYPE_SINGLE_CYCLE;
  texAnimation.numColumns = 1;
  texAnimation.numRows = 1;
  SetParticleParmAsConstant(texAnimation.rate, 0.0f, 0.0f);
  texAnimation.startFrame = 0;
  texAnimation.useRandomRow = false;
  texAnimation.useFrameBlending = true;

  customPath.type = PPATH_STANDARD;
  customPath.useRandomSpawn = true;
  SetConstantRange(customPath.parms, 5, 0.0f);
  SetConstantRange(genericParm.genericParm, 4, 0.0f);
  lodParms.sizeScale = 1.0f;
  lodParms.lerpAmount = 0.0f;
  lodParms.radius = 0;
  lodParms.totalParticles = 1;

  maxParticleLife = 1.5f;
  maxDeadTime = 0.0f;
  cycleMsec = 1500;
  bunchTime = systemProperties.emissionTime <= 0.0f
                  ? maxParticleLife
                  : systemProperties.emissionTime;
}

void idParticleStage::CalculateBounds(const idLookupTable* tables,
                                      const prtLocation_t* locations,
                                      int numLocations) {
  bounds.Clear();

  particleMeshDist_t mesh;
  mesh.ConstructEmpty();
  prtLocation_t empty{};
  empty.Clear();

  particleInput_t inputs{};
  inputs.stage = this;
  inputs.tables = tables;
  inputs.meshDistribution = &mesh;
  inputs.totalParticles = systemProperties.totalParticles;
  inputs.modelAxis = IdentityParticleAxis();
  inputs.globalAxis = IdentityParticleAxis();
  inputs.localViewLeft = {0.0f, 1.0f, 0.0f};
  inputs.localViewUp = {0.0f, 0.0f, 1.0f};
  inputs.fade = 1.0f;
  inputs.alphaScaleOverride = 1.0f;
  inputs.shadow = 1.0f;
  inputs.distribScale = {1.0f, 1.0f, 1.0f};
  inputs.sizeScale = 1.0f;
  inputs.entityColor = {1.0f, 1.0f, 1.0f, 1.0f};

  const int maxLifeMsec = static_cast<int>(maxParticleLife * 1000.0f);
  std::uint32_t diversitySeed = 0;
  for (int locationIndex = 0; locationIndex <= numLocations;
       ++locationIndex) {
    inputs.location = locationIndex == 0 ? &empty : &locations[locationIndex - 1];
    for (int sample = 0; sample < 100; ++sample) {
      const std::uint32_t sampleSeed = diversitySeed;
      const int diversity = static_cast<int>(
          (NextBoundsRandom(diversitySeed) >> 10) & 0x7fffu);
      inputs.stageAxis = ParticleStageAxis(this, diversity);
      for (int ageMsec = 0; ageMsec < maxLifeMsec; ageMsec += 16) {
        if (ageMsec + 16 > maxLifeMsec)
          ageMsec = maxLifeMsec - 1;
        particleGen_t gen{};
        gen.index = systemProperties.totalParticles;
        gen.cycleAge = ageMsec * 0.001f;
        gen.totalAge = gen.cycleAge;
        gen.particleLife = maxParticleLife;
        gen.frac = maxLifeMsec != 0
                       ? static_cast<float>(ageMsec) / maxLifeMsec
                       : 0.0f;
        gen.parmVal = gen.frac;
        gen.random.seed = sampleSeed;
        gen.originalRandom = gen.random;
        gen.vertColor = {1.0f, 1.0f, 1.0f, 1.0f};
        idVec3 origin{};
        ParticleOrigin(&inputs, &gen, origin);
        bounds.AddPoint(origin);
      }
    }
  }

  idVec3 maxSize{};
  idRandom2 random{diversitySeed};
  for (float fraction = 0.0f; fraction <= 1.0f; fraction += 0.015625f) {
    float sizeX = size.size[0].Compute(tables, fraction, random);
    float sizeY;
    float sizeZ;
    if (staticData != nullptr) {
      sizeY = size.size[1].Compute(tables, fraction, random);
      sizeZ = size.size[2].Compute(tables, fraction, random);
    } else {
      const float aspect = size.aspectRatio.Compute(tables, fraction, random);
      if (orientation.type == POR_AIMED) {
        sizeY = aspect * orientation.segmentLength;
      } else {
        if (aspect > 1.0f)
          sizeX *= aspect;
        sizeY = sizeX;
      }
      sizeZ = sizeX;
    }
    maxSize.x = std::max(maxSize.x, sizeX);
    maxSize.y = std::max(maxSize.y, sizeY);
    maxSize.z = std::max(maxSize.z, sizeZ);
  }

  const idVec3 expansion = {
      maxSize.x + 8.0f + systemProperties.boundsExpansion,
      maxSize.y + 8.0f + systemProperties.boundsExpansion,
      maxSize.z + 8.0f + systemProperties.boundsExpansion};
  bounds[0] -= expansion;
  bounds[1] += expansion;
}
