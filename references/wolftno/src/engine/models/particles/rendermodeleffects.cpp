#include "engine/models/particles/rendermodeleffects.h"

#include "engine/models/transparency/rendermodeltransparency.h"
#include "engine/renderer/globalshadows.h"
#include "engine/renderer/imagemanager.h"
#include "engine/renderer/render.h"
#include "shared/idlib/csystems/cvarsystemlocal.h"
#include "shared/idlib/lib.h"
#include "shared/idlib/parallelism/paralleljoblist.h"
#include "shared/idlib/sys/sys_alloc.h"

#include <algorithm>
#include <cmath>
#include <cstring>
#include <new>
#include <vector>

extern idCVar r_skipDecals;
extern idCVar r_useParticleGenJobs;

idCVar r_skipEffectParticles(
    "r_skipEffectParticles", "0", CVAR_INTEGER,
    "1 = skip all particle, 2 = skip all non-transsort particles, 3 = skip all transsort particles",
    0.0f, 3.0f);
idCVar r_skipTracers("r_skipTracers", "0", CVAR_BOOL, "skip tracers");
idCVar r_particleTrail_timeStep(
    "r_particleTrail_timeStep", "0", CVAR_INTEGER,
    "particle trail time step in MS (non-zero value uses cvar)");

idVertexBuffer idRenderModelEffects::vertexBuffer[2];
idIndexBuffer idRenderModelEffects::indexBuffer;
idDrawVert* idRenderModelEffects::mappedVerts = nullptr;
int idRenderModelEffects::mappedBufferIndex = 0;
int idRenderModelEffects::vertOffset = 0;
int idRenderModelEffects::indexOffset = 0;

namespace {

constexpr int kParticleCapacity = 4096;
constexpr int kTrailCapacity = 32;
constexpr int kTracerCapacity = 128;
constexpr int kDecalCapacity = 1024;
constexpr memTag_t kEffectParticleTag = static_cast<memTag_t>(0x50);

template <typename Type>
Type* AllocateEffects(const char* location, std::size_t count,
                      bool clear = true,
                      memTag_t tag = kEffectParticleTag) {
  return static_cast<Type*>(idMem::Singleton()->AllocWithLocation(
      location, sizeof(Type) * count, tag, clear, ALIGN_16,
      HEAP_DEFAULTHEAP));
}

} // namespace

void idRenderModelEffects::EndFrame() {
  vertexBuffer[mappedBufferIndex].UnmapBuffer();
}

bool idRenderModelEffects::CommitSubclass() {
  lastLatchedParticleRange[0] = latchedParticleRange[0];
  latchedParticleRange[0] = particleRange[0];
  latchedTracerRange[0] = tracerRange[0];
  latchedDecalRange[0] = decalRange[0];
  lastLatchedParticleRange[1] = latchedParticleRange[1];
  latchedParticleRange[1] = particleRange[1];
  latchedTracerRange[1] = tracerRange[1];
  latchedDecalRange[1] = decalRange[1];
  latchedTime = gameTime;
  return false;
}

void idRenderModelEffects::Shutdown() {
  if (idLib::headless)
    return;
  if (mappedVerts != nullptr && vertexBuffer[mappedBufferIndex].IsMapped())
    vertexBuffer[mappedBufferIndex].UnmapBuffer();
  for (idVertexBuffer& buffer : vertexBuffer)
    buffer.FreeBufferObject();
  indexBuffer.FreeBufferObject();
}

void idRenderModelEffects::StartFrame() {
  if (mappedVerts != nullptr && vertexBuffer[mappedBufferIndex].IsMapped())
    vertexBuffer[mappedBufferIndex].UnmapBuffer();
  mappedBufferIndex = (mappedBufferIndex + 1) % 2;
  mappedVerts = static_cast<idDrawVert*>(
      vertexBuffer[mappedBufferIndex].MapBuffer(BM_WRITE_NOSYNC));
  vertOffset = 0;
  indexOffset = 0;
}

void idRenderModelEffects::AddDecal(
    const idMaterial* material, const idDrawVert& vertex0,
    const idDrawVert& vertex1, const idDrawVert& vertex2,
    const idDrawVert& vertex3, int start, int life, int fadeInEnd,
    int fadeOutStart) {
  if (r_skipEffectParticles.GetInteger() != 0)
    return;
  const int write = decalRange[1];
  if (write - latchedDecalRange[0] >= kDecalCapacity)
    return;
  decalRange[1] = write + 1;
  const int index = write & (kDecalCapacity - 1);
  decals[index].mat = material;
  decals[index].startTime = start;
  decals[index].lifeTime = life;
  decals[index].fadeInEndTime = fadeInEnd;
  decals[index].fadeOutStartTime = fadeOutStart;
  decalVerts[index].verts[0] = vertex0;
  decalVerts[index].verts[1] = vertex1;
  decalVerts[index].verts[2] = vertex2;
  decalVerts[index].verts[3] = vertex3;
}

void idRenderModelEffects::AddTracer(
    const idMaterial* material, const idVec3& origin,
    const idVec3& direction, const idVec3& maximumDistance, float speed,
    float length, float height, bool ensureVisual, float insuredFraction,
    float life) {
  if (r_skipTracers.GetInteger() != 0)
    return;
  const int write = tracerRange[1];
  if (write - latchedTracerRange[0] >= kTracerCapacity)
    return;
  tracerRange[1] = write + 1;
  tracerParms_t& tracer = tracers[write & (kTracerCapacity - 1)];
  tracer.startTime = gameTime;
  tracer.speed = speed;
  tracer.origin = origin;
  tracer.dir = direction;
  tracer.mat = material;
  tracer.height = height;
  tracer.length = length;

  const idVec3 distance = maximumDistance - origin;
  const float maximumLength = std::sqrt(std::max(
      distance.x * distance.x + distance.y * distance.y +
          distance.z * distance.z,
      1.17549435e-38f));
  float travelDistance = maximumLength - length * 0.5f;
  if (ensureVisual && speed * insuredFraction > travelDistance) {
    tracer.length = maximumLength * 2.0f * insuredFraction -
                    insuredFraction * 2.0f * speed;
    if (tracer.length < 0.0f) {
      tracer.length = maximumLength * 0.5f;
      tracer.speed = maximumLength / insuredFraction -
                     (maximumLength * 0.5f * 0.5f) / insuredFraction;
    }
    travelDistance = insuredFraction * tracer.speed;
  }
  if (life < 0.0f) {
    const float tracerSpeed = tracer.speed == 0.0f ? 1500.0f : tracer.speed;
    life = std::min((travelDistance / tracerSpeed) * 1000.0f, 3000.0f);
  }
  tracer.lifeTime = static_cast<int>(life);
}

bool idRenderModelEffects::AddParticles(
    const idDeclParticle* particle, int systemStartTime,
    int gameMillisecondsPerFrame, float diversity, const idVec3& origin,
    const idMat3& axis, const idVec3& velocity, const std::uint32_t& color,
    bool forceInitialTime) {
  if (r_skipEffectParticles.GetInteger() != 0 || particle == nullptr ||
      particleRange[1] - latchedParticleRange[0] >= kParticleCapacity)
    return false;
  if (gameTime < systemStartTime)
    return true;

  bool hasMoreParticles = false;
  for (int stageIndex = 0; stageIndex < particle->stages.Num(); ++stageIndex) {
    const idParticleStage* stage = particle->stages[stageIndex];
    const int duration = static_cast<int>(stage->maxParticleLife * 1000.0f);
    if (stage->cycleMsec == 0 || stage->systemProperties.material == nullptr ||
        stage->maxParticleLife <= 0.0f || stage->hidden)
      continue;

    const int totalParticles = stage->systemProperties.totalParticles;
    const int bunchDuration =
        stage->cycleMsec *
        static_cast<int>(stage->systemProperties.spawnBunching);
    const int elapsed = gameTime - systemStartTime;
    int previousParticle = 0;
    int currentParticle = 0;
    if (bunchDuration != 0) {
      currentParticle = elapsed * totalParticles / bunchDuration;
      if (currentParticle >= totalParticles)
        currentParticle = totalParticles - 1;
      const int previousElapsed = elapsed - gameMillisecondsPerFrame;
      previousParticle = previousElapsed >= 0
                             ? totalParticles * previousElapsed / bunchDuration
                             : -1;
    } else if (gameTime == systemStartTime || forceInitialTime) {
      previousParticle = -1;
      currentParticle = totalParticles - 1;
    } else {
      continue;
    }
    if (previousParticle >= totalParticles)
      continue;

    const int write = particleRange[1]++;
    if (currentParticle < totalParticles - 1)
      hasMoreParticles = true;
    effectParticleParms_t& output =
        particles[write & (kParticleCapacity - 1)];
    output.stage = stage;
    output.tables = particle->GetTables();
    output.numTables = particle->tables.Num();
    output.diversity = static_cast<int>(diversity * 65535.0f);
    output.origin = origin;
    output.axis = axis;
    output.velocity = velocity;
    output.wind = {};
    output.shadow = 1.0f;
    output.color = color;
    output.numParticles = std::max(currentParticle - previousParticle, 0);
    output.startTime = systemStartTime -
                       static_cast<int>(stage->systemProperties.timeOffset *
                                        -1000.0f);
    output.duration = duration;
    output.currTime = gameTime;
  }
  return hasMoreParticles;
}

void idRenderModelEffects::Update(int newTime,
                                  int gameMillisecondsPerFrame) {
  int trail = particleTrailRange[0];
  deltaTime = newTime - gameTime;
  gameTime = newTime;
  for (; trail < particleTrailRange[1]; ++trail) {
    particleTrailParms_t& parms =
        particleTrails[particleTrailRange[0] & (kTrailCapacity - 1)];
    int lastTime = parms.lastTime;
    if (lastTime <= gameTime) {
      const int endTime = std::min(gameTime, parms.endTime);
      int step = r_particleTrail_timeStep.GetInteger();
      if (step <= 0)
        step = 1;
      if (lastTime <= endTime) {
        const std::uint32_t color = ~0u;
        do {
          const float fraction = static_cast<float>(lastTime - parms.startTime) /
                                 static_cast<float>(parms.endTime -
                                                    parms.startTime);
          const idVec3 position = parms.startPos + parms.dir * fraction;
          AddParticles(parms.decl, lastTime, gameMillisecondsPerFrame, 0.3f,
                       position, mat3_identity, vec3_origin, color, false);
          lastTime += step;
          parms.lastTime = lastTime;
        } while (lastTime <= endTime);
      }
      if (gameTime >= parms.endTime)
        ++particleTrailRange[0];
    }
  }
  particleTrailRange[0] =
      std::max(particleTrailRange[0], particleTrailRange[1] - kTrailCapacity);

  while (particleRange[0] < particleRange[1]) {
    const effectParticleParms_t& particle =
        particles[particleRange[0] & (kParticleCapacity - 1)];
    if (gameTime - particle.startTime <= particle.duration)
      break;
    ++particleRange[0];
  }
  particleRange[0] =
      std::max(particleRange[0], particleRange[1] - kParticleCapacity);
  while (tracerRange[0] < tracerRange[1]) {
    const tracerParms_t& tracer =
        tracers[tracerRange[0] & (kTracerCapacity - 1)];
    if (gameTime - tracer.startTime <= tracer.lifeTime)
      break;
    ++tracerRange[0];
  }
  tracerRange[0] =
      std::max(tracerRange[0], tracerRange[1] - kTracerCapacity);
  while (decalRange[0] < decalRange[1]) {
    const decalParms_t& decal =
        decals[decalRange[0] & (kDecalCapacity - 1)];
    if (gameTime - decal.startTime <= decal.lifeTime)
      break;
    ++decalRange[0];
  }
  decalRange[0] =
      std::max(decalRange[0], decalRange[1] - kDecalCapacity);
  CommitThisFrame();
}

void idRenderModelEffects::SortEffectParticles() {
  int insertedStages = 0;
  for (int absoluteIndex = lastLatchedParticleRange[1];
       absoluteIndex < latchedParticleRange[1]; ++absoluteIndex) {
    const effectParticleParms_t incoming =
        particles[absoluteIndex & (kParticleCapacity - 1)];
    int stageIndex = 0;
    while (stageIndex < numSortedParticleStages &&
           sortedParticleStages[stageIndex].stage != incoming.stage)
      ++stageIndex;
    if (stageIndex < numSortedParticleStages) {
      int destination = absoluteIndex;
      for (int moveStage = numSortedParticleStages - 1;
           moveStage > stageIndex; --moveStage) {
        const int source = sortedParticleStages[moveStage].first;
        particles[destination & (kParticleCapacity - 1)] =
            particles[source & (kParticleCapacity - 1)];
        destination = source;
        ++sortedParticleStages[moveStage].first;
      }
      particles[destination & (kParticleCapacity - 1)] = incoming;
      ++sortedParticleStages[stageIndex].num;
    } else {
      if (numSortedParticleStages >= 1024)
        idLib::Error("MAX_SORTED_PARTICLE_STAGES");
      sortedParticleStages[numSortedParticleStages++] =
          {incoming.stage, absoluteIndex, 1};
    }
  }

  for (int stageIndex = 0; stageIndex < numSortedParticleStages;
       ++stageIndex) {
    sortedParticleStage_t& stage = sortedParticleStages[stageIndex];
    for (int particleIndex = 0; particleIndex < stage.num; ++particleIndex) {
      int absoluteIndex = stage.first + particleIndex;
      effectParticleParms_t& particle =
          particles[absoluteIndex & (kParticleCapacity - 1)];
      if (gameTime - particle.startTime <= particle.duration)
        continue;
      const effectParticleParms_t expired = particle;
      int destination = absoluteIndex;
      for (int moveStage = stageIndex; moveStage >= 0; --moveStage) {
        const int source = sortedParticleStages[moveStage].first;
        particles[destination & (kParticleCapacity - 1)] =
            particles[source & (kParticleCapacity - 1)];
        destination = source;
        ++sortedParticleStages[moveStage].first;
      }
      particles[destination & (kParticleCapacity - 1)] = expired;
      --stage.num;
    }
  }

  for (int read = 0; read < numSortedParticleStages; ++read) {
    if (sortedParticleStages[read].num >= 1)
      sortedParticleStages[insertedStages++] = sortedParticleStages[read];
  }
  numSortedParticleStages = insertedStages;
  lastLatchedParticleRange[0] = latchedParticleRange[0];
  lastLatchedParticleRange[1] = latchedParticleRange[1];
}

int idRenderModelEffects::EstimateQuadAllocation(
    const idParticleStage* stage, const effectParticleParms_t* particle,
    int renderTime) {
  if (stage->cycleMsec == 0 ||
      stage->systemProperties.totalParticles == 0)
    return 0;
  const int bunchDuration = static_cast<int>(
      stage->systemProperties.spawnBunching * stage->bunchTime * 1000.0f);
  const int earliestLife = renderTime -
                           static_cast<int>(stage->maxParticleLife * 1000.0f) -
                           particle->startTime;
  if (bunchDuration != 0) {
    int expired =
        (bunchDuration +
         earliestLife * stage->systemProperties.totalParticles) /
        bunchDuration;
    expired = std::max(expired, 0);
    const int visible = std::max(particle->numParticles - expired, 0);
    return visible * stage->NumVertsPerParticle() >> 2;
  }
  if (earliestLife < 0)
    return particle->numParticles * stage->NumVertsPerParticle() >> 2;
  return 0;
}

void idRenderModelEffects::Init() {
  for (idVertexBuffer& buffer : vertexBuffer) {
    buffer.AllocBufferObject(nullptr, 0x40000, 31, BU_DYNAMIC);
    void* mapped = buffer.MapBuffer(BM_WRITE);
    std::memset(mapped, 0, 0x40000);
    buffer.UnmapBuffer();
  }
  auto* indices = AllocateEffects<std::uint16_t>(
      "w:\\build\\tech5\\shared\\idlib\\containers/Array.h(105) : TAG_TEMP",
      12288, false, TAG_TEMP);
  for (int vertex = 0, index = 0; vertex < 0x2000;
       vertex += 4, index += 6) {
    indices[index + 0] = static_cast<std::uint16_t>(vertex + 0);
    indices[index + 1] = static_cast<std::uint16_t>(vertex + 2);
    indices[index + 2] = static_cast<std::uint16_t>(vertex + 3);
    indices[index + 3] = static_cast<std::uint16_t>(vertex + 0);
    indices[index + 4] = static_cast<std::uint16_t>(vertex + 3);
    indices[index + 5] = static_cast<std::uint16_t>(vertex + 1);
  }
  indexBuffer.AllocBufferObject(indices, 24576, BU_DEFAULT);
  idMem::Singleton()->Free(indices, ALIGN_16);
  mappedVerts = nullptr;
  mappedBufferIndex = 0;
  vertOffset = 0;
  indexOffset = 0;
}

idRenderModelEffects::idRenderModelEffects() {
  renderModelType = RENDERMODEL_EFFECTS;
  g.axis = mat3_identity;
  deferredAxis = mat3_identity;
  FreeSurfaces();
  g.flags0 |= 0x80u;
  g.flags1 |= 0x20u;
  g.modelDepthSort = MODEL_DEPTH_SORT_LAST;

  particles = AllocateEffects<effectParticleParms_t>(
      "w:\\build\\tech5\\engine\\models\\particles\\RenderModelEffects.cpp(174) : TAG_EFFECT_PARTICLE",
      kParticleCapacity);
  tracers = AllocateEffects<tracerParms_t>(
      "w:\\build\\tech5\\engine\\models\\particles\\RenderModelEffects.cpp(175) : TAG_EFFECT_PARTICLE",
      kTracerCapacity);
  decals = AllocateEffects<decalParms_t>(
      "w:\\build\\tech5\\engine\\models\\particles\\RenderModelEffects.cpp(176) : TAG_EFFECT_PARTICLE",
      kDecalCapacity);
  decalVerts = AllocateEffects<decalVerts_t>(
      "w:\\build\\tech5\\engine\\models\\particles\\RenderModelEffects.cpp(177) : TAG_EFFECT_PARTICLE",
      kDecalCapacity);
  particleTrails = AllocateEffects<particleTrailParms_t>(
      "w:\\build\\tech5\\engine\\models\\particles\\RenderModelEffects.cpp(178) : TAG_EFFECT_PARTICLE",
      kTrailCapacity);
  particleRenderView = AllocateEffects<particleRenderView_t>(
      "w:\\build\\tech5\\engine\\models\\particles\\RenderModelEffects.cpp(180) : TAG_EFFECT_PARTICLE",
      1, false);
  particleGenParms = AllocateEffects<deferredParticleGenParms_t>(
      "w:\\build\\tech5\\engine\\models\\particles\\RenderModelEffects.cpp(181) : TAG_EFFECT_PARTICLE",
      kParticleCapacity, false);
  for (idList<deferredStage_t, 5>& stages : deferredStages)
    stages.ClearFree();

  auto* triangleAllocation = static_cast<std::uint8_t*>(
      idMem::Singleton()->AllocWithLocation(
          "w:\\build\\tech5\\shared\\idlib\\Heap.h(113) : tag",
          sizeof(idTriangles) * 1024 + 8, kEffectParticleTag, false,
          ALIGN_16, HEAP_DEFAULTHEAP));
  *reinterpret_cast<std::uint32_t*>(triangleAllocation) = 1024;
  triangles = reinterpret_cast<idTriangles*>(triangleAllocation + 8);
  for (int index = 0; index < 1024; ++index) {
    new (&triangles[index]) idTriangles{};
    triangles[index].bounds[0] = {-99999.0f, -99999.0f, -99999.0f};
    triangles[index].bounds[1] = {99999.0f, 99999.0f, 99999.0f};
  }
}

idRenderModelEffects::~idRenderModelEffects() {
  idMem* memory = idMem::Singleton();
  memory->Free(particles, ALIGN_16);
  memory->Free(particleTrails, ALIGN_16);
  memory->Free(tracers, ALIGN_16);
  memory->Free(decals, ALIGN_16);
  memory->Free(decalVerts, ALIGN_16);
  memory->Free(particleRenderView, ALIGN_16);
  memory->Free(particleGenParms, ALIGN_16);
  for (idList<deferredStage_t, 5>& stages : deferredStages)
    stages.ClearFree();
  if (triangles != nullptr) {
    for (int index = 0; index < 1024; ++index)
      triangles[index].~idTriangles();
    memory->Free(reinterpret_cast<std::uint8_t*>(triangles) - 8, ALIGN_16);
  }
}

void idRenderModelEffects::ResetForEdit() {
  particleRange[0] = particleRange[1] = 0;
  tracerRange[0] = tracerRange[1] = 0;
  decalRange[0] = decalRange[1] = 0;
  particleTrailRange[0] = particleTrailRange[1] = 0;
  lastLatchedParticleRange[0] = lastLatchedParticleRange[1] = 0;
  latchedParticleRange[0] = latchedParticleRange[1] = 0;
  latchedTracerRange[0] = latchedTracerRange[1] = 0;
  latchedDecalRange[0] = latchedDecalRange[1] = 0;
  numSortedParticleStages = 0;
  if (surfaces.size < 0)
    surfaces.ClearFree();
  surfaces.num = surfaces.size > 0 ? 0 : surfaces.size;
}

bool idRenderModelEffects::UpdateInView(
    const idRenderView* currentView, const idRenderView* nextView,
    idRenderModelUpdateTools* updateTools) {
  if (r_skipEffectParticles.valueInteger == 1) {
    if (surfaces.size < 0)
      surfaces.ClearFree();
    surfaces.num = surfaces.size > 0 ? 0 : surfaces.size;
    CommitThisFrame();
    return false;
  }

  const int useParticleGenJobs = r_useParticleGenJobs.valueInteger;
  if (surfaces.size < 0)
    surfaces.ClearFree();
  surfaces.num = surfaces.size > 0 ? 0 : surfaces.size;

  const int tracerSurfaceStart = surfaces.num;
  if (r_skipTracers.valueInteger == 0) {
    for (int tracerIndex = latchedTracerRange[0];
         tracerIndex < latchedTracerRange[1]; ++tracerIndex) {
      const idMaterial* material =
          tracers[tracerIndex & (kTracerCapacity - 1)].mat;
      int surfaceIndex = tracerSurfaceStart;
      while (surfaceIndex < surfaces.num &&
             surfaces[surfaceIndex].material != material)
        ++surfaceIndex;
      if (surfaceIndex != surfaces.num)
        continue;

      idRenderModelSurface surface{};
      surface.material = material;
      surface.geometryIsReference = true;
      surface.referenceMask = 1;
      idTriangles* geometry = &triangles[surfaces.num];
      surface.geometry = geometry;
      geometry->vertexMask = 31;
      const int firstIndex = indexOffset;
      int numVerts = 0;
      int numIndexes = 0;

      for (int matchingIndex = tracerIndex;
           matchingIndex < latchedTracerRange[1]; ++matchingIndex) {
        const tracerParms_t& tracer =
            tracers[matchingIndex & (kTracerCapacity - 1)];
        if (tracer.mat != material)
          continue;
        if ((vertOffset >> 2) + 2 >= 2048)
          break;

        idDrawVert* verts = &mappedVerts[vertOffset];
        vertOffset += 8;
        indexOffset += 12;
        numVerts += 8;
        numIndexes += 12;

        float elapsed = static_cast<float>(latchedTime - deltaTime -
                                           tracer.startTime) *
                        tracer.speed * 0.001f;
        elapsed = std::max(elapsed, 0.0f);
        const idVec3 tail = tracer.origin + tracer.dir * elapsed;
        const idVec3 tip = tail + tracer.dir * tracer.length;
        const idVec3 toView = currentView->r.vieworg - tracer.origin;
        idVec3 side{
            tracer.dir.y * toView.z - tracer.dir.z * toView.y,
            tracer.dir.z * toView.x - tracer.dir.x * toView.z,
            tracer.dir.x * toView.y - tracer.dir.y * toView.x};
        const float sideLength = std::sqrt(std::max(
            side.x * side.x + side.y * side.y + side.z * side.z,
            1.17549435e-38f));
        side = side * (tracer.height * 0.5f / sideLength);

        verts[0].xyz = tail + side;
        verts[1].xyz = tip + side;
        verts[2].xyz = tail - side;
        verts[3].xyz = tip - side;
        verts[4].xyz = tip - side;
        verts[5].xyz = tip + side;
        verts[6].xyz = tail - side;
        verts[7].xyz = tail + side;
        verts[0].st = {0.0f, 0.0f};
        verts[1].st = {1.0f, 0.0f};
        verts[2].st = {0.0f, 1.0f};
        verts[3].st = {1.0f, 1.0f};
        verts[4].st = {1.0f, 1.0f};
        verts[5].st = {1.0f, 0.0f};
        verts[6].st = {0.0f, 1.0f};
        verts[7].st = {0.0f, 0.0f};
      }

      geometry->numVerts = numVerts;
      geometry->numIndexes = numIndexes;
      geometry->vertexBuffer.Reference(vertexBuffer[mappedBufferIndex]);
      geometry->indexBuffer.Reference(indexBuffer, 2 * firstIndex,
                                      2 * numIndexes);
      surfaces.Append(surface);
    }
  }

  const int decalSurfaceStart = surfaces.num;
  if (r_skipDecals.valueInteger == 0) {
    for (int decalIndex = latchedDecalRange[0];
         decalIndex < latchedDecalRange[1]; ++decalIndex) {
      const idMaterial* material =
          decals[decalIndex & (kDecalCapacity - 1)].mat;
      int surfaceIndex = decalSurfaceStart;
      while (surfaceIndex < surfaces.num &&
             surfaces[surfaceIndex].material != material)
        ++surfaceIndex;
      if (surfaceIndex != surfaces.num)
        continue;

      idRenderModelSurface surface{};
      surface.material = material;
      surface.geometryIsReference = true;
      surface.referenceMask = 1;
      idTriangles* geometry = &triangles[surfaces.num];
      surface.geometry = geometry;
      geometry->vertexMask = 31;
      const int firstIndex = indexOffset;
      int numVerts = 0;
      int numIndexes = 0;

      for (int matchingIndex = decalIndex;
           matchingIndex < latchedDecalRange[1]; ++matchingIndex) {
        const int slot = matchingIndex & (kDecalCapacity - 1);
        const decalParms_t& decal = decals[slot];
        if (decal.mat != material)
          continue;
        if ((vertOffset >> 2) + 1 >= 2048)
          break;

        idDrawVert localVerts[4];
        std::memcpy(localVerts, decalVerts[slot].verts, sizeof(localVerts));
        const int fadeInEnd = decal.startTime + decal.fadeInEndTime;
        const int fadeOutStart = decal.startTime + decal.fadeOutStartTime;
        float fade = 0.0f;
        if (latchedTime < fadeInEnd) {
          fade = 1.0f - static_cast<float>(latchedTime - decal.startTime) /
                            static_cast<float>(fadeInEnd - decal.startTime);
        } else if (latchedTime > fadeOutStart) {
          fade = static_cast<float>(latchedTime - fadeOutStart) /
                 static_cast<float>(decal.startTime + decal.lifeTime -
                                    fadeOutStart);
          fade = std::clamp(fade, 0.0f, 1.0f);
        }
        const std::uint8_t alpha = static_cast<std::uint8_t>(
            static_cast<int>((1.0f - fade) * 255.0f));
        const shadowMapCPU_t* shadowMap = static_cast<const shadowMapCPU_t*>(
            updateTools->globalShadowMap);
        for (idDrawVert& vertex : localVerts) {
          const float shadow = shadowMap != nullptr
                                   ? shadowMap->ShadowSample(vertex.xyz)
                                   : 1.0f;
          const int sample = std::clamp(
              static_cast<int>(shadow * 255.0f), 0, 255);
          vertex.color[0] = static_cast<std::uint8_t>(sample);
          vertex.color[1] = static_cast<std::uint8_t>(sample);
          vertex.color[2] = static_cast<std::uint8_t>(sample);
          vertex.color[3] = static_cast<std::uint8_t>(
              (static_cast<unsigned>(alpha) * vertex.color[3]) >> 8);
        }

        std::memcpy(&mappedVerts[vertOffset], localVerts, sizeof(localVerts));
        vertOffset += 4;
        indexOffset += 6;
        numVerts += 4;
        numIndexes += 6;
      }

      geometry->numVerts = numVerts;
      geometry->numIndexes = numIndexes;
      geometry->vertexBuffer.Reference(vertexBuffer[mappedBufferIndex]);
      geometry->indexBuffer.Reference(indexBuffer, 2 * firstIndex,
                                      2 * numIndexes);
      surfaces.Append(surface);
    }
  }

  const int previousUnsortedBuffer =
      (idRenderModelTransparency::mappedBufferIndex + 2) % 3;
  const idVertexBuffer& previousVerts =
      idRenderModelTransparency::vertexBufferUnsorted[previousUnsortedBuffer];
  for (int stageIndex = 0;
       stageIndex < deferredStages[previousUnsortedBuffer].num; ++stageIndex) {
    const deferredStage_t& stage =
        deferredStages[previousUnsortedBuffer][stageIndex];
    idRenderModelSurface surface{};
    surface.material = stage.mtr;
    surface.geometryIsReference = true;
    surface.referenceMask = 1;
    idTriangles* geometry = &triangles[surfaces.num];
    surface.geometry = geometry;
    geometry->vertexMask = 3167;
    geometry->numVerts = stage.vertCount;
    geometry->numIndexes = 6 * (stage.vertCount >> 2);
    geometry->vertexBuffer.Reference(previousVerts);
    geometry->indexBuffer.Reference(
        idRenderModelTransparency::indexBufferUnsorted,
        2 * stage.indexOffset, 2 * geometry->numIndexes);
    surfaces.Append(surface);
  }

  particleRenderView->viewOrg = nextView->r.vieworg;
  particleRenderView->viewFwd = nextView->r.viewaxis.mat[0];
  particleRenderView->viewLeft = nextView->r.viewaxis.mat[1];
  particleRenderView->viewUp = nextView->r.viewaxis.mat[2];
  particleRenderView->renderTime = latchedTime;
  particleRenderView->deltaTime = deltaTime;
  particleRenderView->atlasWidth = static_cast<std::uint16_t>(
      globalImages->transSortImageAtlas->ATLAS_WIDTH);
  particleRenderView->atlasHeight = static_cast<std::uint16_t>(
      globalImages->transSortImageAtlas->ATLAS_HEIGHT);

  SortEffectParticles();
  const int numParticles = latchedParticleRange[1] - latchedParticleRange[0];
  std::vector<int> estimatedQuads(static_cast<std::size_t>(numParticles));
  for (int index = 0; index < numParticles; ++index) {
    const effectParticleParms_t& particle =
        particles[(latchedParticleRange[0] + index) &
                  (kParticleCapacity - 1)];
    estimatedQuads[index] =
        EstimateQuadAllocation(particle.stage, &particle, latchedTime);
  }

  const visibleInfluenceSpheres_t* visibleInfluenceSpheres =
      updateTools->transparencyModel->visibleInfluenceSpheres;
  idList<deferredStage_t, 5>& currentDeferredStages =
      deferredStages[idRenderModelTransparency::mappedBufferIndex];
  if (currentDeferredStages.size < 0)
    currentDeferredStages.ClearFree();
  currentDeferredStages.num =
      currentDeferredStages.size > 0 ? 0 : currentDeferredStages.size;

  int generatedParmCount = 0;
  for (int particleIndex = 0; particleIndex < numParticles;
       ++particleIndex) {
    int quadCount = estimatedQuads[particleIndex];
    if (quadCount == 0)
      continue;

    effectParticleParms_t* firstParticle =
        &particles[(latchedParticleRange[0] + particleIndex) &
                   (kParticleCapacity - 1)];
    int batchCount = 1;
    while (particleIndex + 1 < numParticles) {
      effectParticleParms_t* nextParticle =
          &particles[(latchedParticleRange[0] + particleIndex + 1) &
                     (kParticleCapacity - 1)];
      if (nextParticle->stage != firstParticle->stage ||
          nextParticle == particles ||
          quadCount + estimatedQuads[particleIndex + 1] > 128 ||
          batchCount > 135)
        break;
      quadCount += estimatedQuads[++particleIndex];
      ++batchCount;
    }

    const idParticleStage* stage = firstParticle->stage;
    if (stage->colorAttributes.useGlobalShadows) {
      const shadowMapCPU_t* shadowMap = static_cast<const shadowMapCPU_t*>(
          updateTools->globalShadowMap);
      firstParticle->shadow = shadowMap != nullptr
                                  ? shadowMap->ShadowSample(firstParticle->origin)
                                  : 1.0f;
    }

    deferredParticleGenParms_t& parms =
        particleGenParms[generatedParmCount];
    parms.numJobs = 1;
    parms.mergedSurfaces = false;
    parms.numLocations = 0;
    parms.location = nullptr;
    parms.renderView = particleRenderView;
    parms.effectParticleParms = firstParticle;
    parms.modelParticleParms = nullptr;
    parms.stage = stage;
    parms.tables = firstParticle->tables;
    parms.staticVerts = stage->GetStaticVerts();
    parms.numEffectParticleParms =
        static_cast<std::uint16_t>(batchCount);
    parms.numModelParticleParms = 0;
    parms.numTables = firstParticle->numTables;
    parms.staticVertsSize = stage->GetNumStaticVerts();
    parms.deadTime = stage->maxDeadTime;
    parms.visibleInfluenceSpheres = visibleInfluenceSpheres;
    parms.maxVertsToGen = 4 * quadCount;
    parms.verts = nullptr;
    parms.quadDepth = nullptr;
    parms.quadsUsed = nullptr;
    parms.meshDistribution.ConstructEmpty();

    bool allocated = false;
    if (stage->isTransparencySorted) {
      if (r_skipEffectParticles.valueInteger != 3) {
        allocated = idRenderModelTransparency::AllocateQuadSegment(
            quadCount, parms.verts, parms.quadDepth, parms.quadsUsed,
            stage->hasEmissivePass);
      }
    } else if (r_skipEffectParticles.valueInteger != 2) {
      deferredStage_t deferredStage{};
      deferredStage.mtr = stage->systemProperties.material;
      allocated = idRenderModelTransparency::AllocateUnsortedQuadSegment(
          quadCount, parms.verts, deferredStage.indexOffset,
          deferredStage.vertCount);
      if (allocated)
        currentDeferredStages.Append(deferredStage);
    }

    if (!allocated)
      continue;
    if (useParticleGenJobs == 1) {
      updateTools->renderModelJobList->AddJob(
          reinterpret_cast<jobRun_t>(ParticleGenJob), &parms);
    } else {
      ParticleGenJob(&parms);
    }
    ++generatedParmCount;
  }

  CommitThisFrame();
  return true;
}
