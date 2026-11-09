#pragma once

#include "engine/models/particles/declparticle.h"
#include "engine/models/particles/jobs/particlegen.h"
#include "engine/models/rendermodel.h"
#include "engine/renderer/bufferobject.h"

#include <cstddef>
#include <cstdint>

class idMaterial;

struct particleTrailParms_t {
  idVec3 startPos;
  idVec3 dir;
  int startTime{};
  int endTime{};
  int lastTime{};
  const idDeclParticle* decl{};
};

struct tracerParms_t {
  int startTime{};
  int lifeTime{};
  float speed{};
  idVec3 origin;
  idVec3 dir;
  float length{};
  float height{};
  const idMaterial* mat{};
};

struct decalParms_t {
  const idMaterial* mat{};
  int startTime{};
  int lifeTime{};
  int fadeInEndTime{};
  int fadeOutStartTime{};
};

struct decalVerts_t {
  idDrawVert verts[4];
};

class idRenderModelEffects : public idRenderModel {
public:
  struct sortedParticleStage_t {
    const idParticleStage* stage{};
    int first{};
    int num{};
  };

  struct deferredStage_t {
    const idMaterial* mtr{};
    int vertCount{};
    int indexOffset{};
  };

  idRenderModelEffects();

  static void Init();
  static void Shutdown();
  static void StartFrame();
  static void EndFrame();

  void AddDecal(const idMaterial* material, const idDrawVert& vertex0,
                const idDrawVert& vertex1, const idDrawVert& vertex2,
                const idDrawVert& vertex3, int startTime, int lifeTime,
                int fadeInEndTime, int fadeOutStartTime);
  void AddTracer(const idMaterial* material, const idVec3& origin,
                 const idVec3& direction, const idVec3& maximumDistance,
                 float speed, float length, float height, bool ensureVisual,
                 float insuredFraction, float lifeTime);
  bool AddParticles(const idDeclParticle* particle, int systemStartTime,
                    int gameMillisecondsPerFrame, float diversity,
                    const idVec3& origin, const idMat3& axis,
                    const idVec3& velocity, const std::uint32_t& color,
                    bool forceInitialTime);
  void Update(int newTime, int gameMillisecondsPerFrame);
  void ResetForEdit();

  effectParticleParms_t* particles{};
  int particleRange[2]{};
  particleTrailParms_t* particleTrails{};
  int particleTrailRange[2]{};
  tracerParms_t* tracers{};
  int tracerRange[2]{};
  decalParms_t* decals{};
  decalVerts_t* decalVerts{};
  int decalRange[2]{};
  int gameTime{};
  int deltaTime{};
  int latchedTime{};
  int lastLatchedParticleRange[2]{};
  int latchedParticleRange[2]{};
  int latchedTracerRange[2]{};
  int latchedDecalRange[2]{};
  sortedParticleStage_t sortedParticleStages[1024]{};
  int numSortedParticleStages{};
  particleRenderView_t* particleRenderView{};
  deferredParticleGenParms_t* particleGenParms{};
  idList<deferredStage_t, 5> deferredStages[3];
  idTriangles* triangles{};

  static idVertexBuffer vertexBuffer[2];
  static idIndexBuffer indexBuffer;
  static idDrawVert* mappedVerts;
  static int mappedBufferIndex;
  static int vertOffset;
  static int indexOffset;

protected:
  ~idRenderModelEffects() override;

private:
  bool CommitSubclass() override;
  bool UpdateInView(const idRenderView* currentView,
                    const idRenderView* nextView,
                    idRenderModelUpdateTools* updateTools) override;
  void SortEffectParticles();
  static int EstimateQuadAllocation(const idParticleStage* stage,
                                    const effectParticleParms_t* particle,
                                    int renderTime);
};

static_assert(sizeof(particleTrailParms_t) == 48,
              "particleTrailParms_t ABI drift");
static_assert(sizeof(tracerParms_t) == 56, "tracerParms_t ABI drift");
static_assert(sizeof(decalParms_t) == 24, "decalParms_t ABI drift");
static_assert(sizeof(decalVerts_t) == 128, "decalVerts_t ABI drift");
static_assert(sizeof(idRenderModelEffects::sortedParticleStage_t) == 16,
              "sorted particle stage ABI drift");
static_assert(sizeof(idRenderModelEffects::deferredStage_t) == 16,
              "deferred particle stage ABI drift");
static_assert(offsetof(idRenderModelEffects, particles) == 2176,
              "effects particles offset drift");
static_assert(offsetof(idRenderModelEffects, sortedParticleStages) == 2296,
              "effects sorted stages offset drift");
static_assert(offsetof(idRenderModelEffects, particleRenderView) == 18688,
              "effects particle view offset drift");
static_assert(offsetof(idRenderModelEffects, deferredStages) == 18704,
              "effects deferred stages offset drift");
static_assert(offsetof(idRenderModelEffects, triangles) == 18776,
              "effects triangles offset drift");
static_assert(sizeof(idRenderModelEffects) == 18784,
              "idRenderModelEffects ABI drift");

