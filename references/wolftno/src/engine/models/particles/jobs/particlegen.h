#pragma once

#include "engine/models/particles/jobs/dustgen.h"

#include <cstddef>
#include <cstdint>

struct particleMeshDistTri_t {
  idVec3 vertices[3];
  float area{};
  std::uint8_t pad[8]{};

  float CaluclateArea();
};

struct particleMeshDist_t {
  particleMeshDist_t();
  ~particleMeshDist_t();
  void operator=(const particleMeshDist_t& other);
  void ConstructEmpty();
  idVec3 GenerateRandomPointOnSurface(idRandom2& random) const;
  void Construct(std::uint16_t numTriangles,
                 const particleMeshDistTri_t* triangles);

  particleMeshDistTri_t* triangles{};
  idBounds bounds;
  float totalArea{};
  std::uint16_t numTriangles{};
  std::uint16_t isCopy{};
};

struct prtLocation_t {
  idParticleParm loc[3];
  std::int32_t diversity{};

  void Clear();
};

struct particleRenderView_t {
  idVec3 viewOrg;
  idVec3 viewFwd;
  idVec3 viewLeft;
  idVec3 viewUp;
  std::int32_t renderTime{};
  std::int32_t deltaTime{};
  std::uint16_t atlasWidth{};
  std::uint16_t atlasHeight{};
  std::uint32_t pad{};
};

struct particleInput_t {
  const idParticleStage* stage{};
  const prtLocation_t* location{};
  const idLookupTable* tables{};
  const particleRenderView_t* view{};
  const idDrawVert* staticVerts{};
  const particleMeshDist_t* meshDistribution{};
  std::int32_t staticVertsSize{};
  std::int32_t totalParticles{};
  idMat3 modelAxis{};
  idMat3 globalAxis{};
  idVec3 globalOrigin{};
  idVec3 localVelocity{};
  idVec3 localViewLeft{};
  idVec3 localViewUp{};
  idVec3 localViewOrg{};
  float fade{};
  float alphaScaleOverride{};
  float shadow{};
  idVec3 distribScale{};
  float sizeScale{};
  idVec4 entityColor{};
  idMat3 stageAxis{};
  idVec3 wind{};
  const influenceSphere_t* influenceSpheres{};
  std::int32_t numInfluenceSpheres{};
  std::int32_t diversity{};
};

struct particleOutput_t {
  idTransparencyVert* verts{};
  float* quadDepth{};
};

struct modelParticleParms_t {
  std::int32_t stopTime{};
  float timeOffset{};
  std::int32_t diversity{};
  float coverage{};
  idVec4 color{};
  idVec4 wind{};
  idVec3 distribScale{};
  idVec3 origin{};
  idMat3 axis{};
  idVec3 scale{};
  idVec3 velocity{};
  float shadow{};
  float alphaScaleOverride{};
  std::uint8_t lod{};
  std::uint8_t pad[3]{};
};

struct effectParticleParms_t {
  std::int32_t startTime{};
  std::int32_t duration{};
  const idParticleStage* stage{};
  const idLookupTable* tables{};
  std::int32_t numTables{};
  std::int32_t diversity{};
  idVec3 origin{};
  idMat3 axis{};
  idVec3 velocity{};
  idVec3 wind{};
  float shadow{};
  std::int32_t currTime{};
  std::int32_t numParticles{};
  std::uint32_t color{};
};

struct deferredParticleGenParms_t {
  std::uint16_t numJobs{};
  bool mergedSurfaces{};
  std::uint8_t headerPad[5]{};
  const particleRenderView_t* renderView{};
  const effectParticleParms_t* effectParticleParms{};
  const modelParticleParms_t* modelParticleParms{};
  const idParticleStage* stage{};
  const idLookupTable* tables{};
  const idDrawVert* staticVerts{};
  const visibleInfluenceSpheres_t* visibleInfluenceSpheres{};
  prtLocation_t* location{};
  particleMeshDist_t meshDistribution{};
  std::uint16_t numEffectParticleParms{};
  std::uint16_t numModelParticleParms{};
  std::int32_t numTables{};
  std::int32_t numLocations{};
  std::int32_t staticVertsSize{};
  float deadTime{};
  std::int32_t maxVertsToGen{};
  idTransparencyVert* verts{};
  float* quadDepth{};
  int* quadsUsed{};
  std::uint32_t pad[1]{};
};

idMat3 ParticleStageAxis(const idParticleStage* stage, int diversity);
idVec3 ParticleOrigin(const particleInput_t* inputs, particleGen_t* gen,
                      idVec3& origin);
void ParticleLeftAndUp(const idParticleStage* const stage,
                       const idLookupTable* tables, particleGen_t* gen,
                       const idVec3& viewUp, const idVec3& viewLeft,
                       float extraRotation, idVec3& left, idVec3& up);
void ParticleTexCoords(const idParticleStage* stage,
                       const idLookupTable* tables, particleGen_t* gen,
                       const idVec2& sMinMax, const idVec2& tMinMax,
                       int atlasWidth, int atlasHeight,
                       idTransparencyVert* verts);
int GenParticleStage(const deferredParticleGenParms_t* parms, int index,
                     int vertOffset, const prtLocation_t* location);
int GenEffectStage(const deferredParticleGenParms_t* parms, int index,
                   int vertOffset, const prtLocation_t* location);
void ParticleGenJob(const deferredParticleGenParms_t* allParms);

static_assert(sizeof(particleMeshDistTri_t) == 48,
              "particleMeshDistTri_t ABI drift");
static_assert(offsetof(particleMeshDistTri_t, vertices) == 0,
              "particle mesh triangle vertices offset drift");
static_assert(offsetof(particleMeshDistTri_t, area) == 36,
              "particle mesh triangle area offset drift");
static_assert(offsetof(particleMeshDistTri_t, pad) == 40,
              "particle mesh triangle pad offset drift");
static_assert(sizeof(particleMeshDist_t) == 40,
              "particleMeshDist_t ABI drift");
static_assert(offsetof(particleMeshDist_t, triangles) == 0,
              "particle mesh triangles offset drift");
static_assert(offsetof(particleMeshDist_t, bounds) == 8,
              "particle mesh bounds offset drift");
static_assert(offsetof(particleMeshDist_t, totalArea) == 32,
              "particle mesh total area offset drift");
static_assert(offsetof(particleMeshDist_t, numTriangles) == 36,
              "particle mesh triangle count offset drift");
static_assert(offsetof(particleMeshDist_t, isCopy) == 38,
              "particle mesh ownership offset drift");
static_assert(sizeof(prtLocation_t) == 64, "prtLocation_t ABI drift");
static_assert(offsetof(prtLocation_t, loc) == 0,
              "particle location parms offset drift");
static_assert(offsetof(prtLocation_t, diversity) == 60,
              "particle location diversity offset drift");
static_assert(sizeof(particleRenderView_t) == 64,
              "particleRenderView_t ABI drift");
static_assert(offsetof(particleRenderView_t, renderTime) == 48,
              "particle view time offset drift");
static_assert(offsetof(particleRenderView_t, atlasWidth) == 56,
              "particle view atlas width offset drift");
static_assert(sizeof(particleInput_t) == 296, "particleInput_t ABI drift");
static_assert(offsetof(particleInput_t, staticVertsSize) == 48,
              "particle input static size offset drift");
static_assert(offsetof(particleInput_t, modelAxis) == 56,
              "particle input model axis offset drift");
static_assert(offsetof(particleInput_t, globalOrigin) == 128,
              "particle input global origin offset drift");
static_assert(offsetof(particleInput_t, localViewOrg) == 176,
              "particle input local view origin offset drift");
static_assert(offsetof(particleInput_t, entityColor) == 216,
              "particle input entity color offset drift");
static_assert(offsetof(particleInput_t, stageAxis) == 232,
              "particle input stage axis offset drift");
static_assert(offsetof(particleInput_t, influenceSpheres) == 280,
              "particle input influence sphere offset drift");
static_assert(offsetof(particleInput_t, diversity) == 292,
              "particle input diversity offset drift");
static_assert(sizeof(particleOutput_t) == 16, "particleOutput_t ABI drift");
static_assert(sizeof(modelParticleParms_t) == 144,
              "modelParticleParms_t ABI drift");
static_assert(offsetof(modelParticleParms_t, axis) == 72,
              "model particle axis offset drift");
static_assert(offsetof(modelParticleParms_t, alphaScaleOverride) == 136,
              "model particle alpha override offset drift");
static_assert(offsetof(modelParticleParms_t, lod) == 140,
              "model particle lod offset drift");
static_assert(sizeof(effectParticleParms_t) == 120,
              "effectParticleParms_t ABI drift");
static_assert(offsetof(effectParticleParms_t, axis) == 44,
              "effect particle axis offset drift");
static_assert(offsetof(effectParticleParms_t, currTime) == 108,
              "effect particle current time offset drift");
static_assert(offsetof(effectParticleParms_t, color) == 116,
              "effect particle color offset drift");
static_assert(sizeof(deferredParticleGenParms_t) == 168,
              "deferredParticleGenParms_t ABI drift");
static_assert(offsetof(deferredParticleGenParms_t, meshDistribution) == 72,
              "deferred particle mesh offset drift");
static_assert(offsetof(deferredParticleGenParms_t, verts) == 136,
              "deferred particle verts offset drift");
static_assert(offsetof(deferredParticleGenParms_t, quadDepth) == 144,
              "deferred particle depth offset drift");
static_assert(offsetof(deferredParticleGenParms_t, quadsUsed) == 152,
              "deferred particle quads-used offset drift");
