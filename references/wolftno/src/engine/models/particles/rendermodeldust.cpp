#include "engine/models/particles/rendermodeldust.h"

#include "engine/models/renderworld.h"
#include "engine/models/transparency/rendermodeltransparency.h"
#include "engine/renderer/imagemanager.h"
#include "shared/idlib/parallelism/paralleljoblist.h"
#include "shared/idlib/sys/sys_alloc.h"

#include <algorithm>
#include <cmath>
#include <cstdint>
#include <cstring>
#include <limits>
#include <xmmintrin.h>

idCVar r_useDustGenJobs("r_useDustGenJobs", "1", CVAR_INTEGER,
                        "0 = off, 1 = threaded", 0.0f, 1.0f);
idCVar r_skipDust("r_skipDust", "0", CVAR_INTEGER, "", 0.0f, 1.0f);

namespace {

constexpr float kRandomScale = 0.000030518509f;

std::uint32_t NextDustRandom(std::uint32_t& seed) {
  seed = 1103515245u * seed + 12345u;
  return seed;
}

float UnitDustRandom(std::uint32_t value) {
  return static_cast<float>((value >> 16) & 0x7fffu) * kRandomScale;
}

float SignedDustRandom(std::uint32_t value) {
  return UnitDustRandom(value) * 2.0f - 1.0f;
}

float ClampUnit(float value) {
  if (value < 0.0f)
    return 0.0f;
  if (value > 1.0f)
    return 1.0f;
  return value;
}

const idVec3& ModelOrigin(const idRenderModelDust& model) {
  if (model.useDeferredPosition != 0)
    return model.deferredOrigin;
  return *reinterpret_cast<const idVec3*>(model.recoveredParms);
}

} // namespace

float idBounds::GetMaxDistFromOrigin() const {
  float maximum = b[0].x * b[0].x + b[0].y * b[0].y + b[0].z * b[0].z;
  const float other =
      b[1].x * b[1].x + b[1].y * b[1].y + b[1].z * b[1].z;
  if (other > maximum)
    maximum = other;
  const __m128 clamped =
      _mm_max_ss(_mm_set_ss(maximum),
                 _mm_set_ss(std::numeric_limits<float>::min()));
  return _mm_cvtss_f32(_mm_sqrt_ss(clamped));
}

idRenderModelDust::idRenderModelDust()
    : numParticles(0), particles(nullptr), lastRenderTime(-1),
      particleDecl(nullptr) {
  renderModelType = RENDERMODEL_DUST;
  constexpr int modelDepthSortAfterWorld = 1;
  std::memcpy(recoveredParms + 172, &modelDepthSortAfterWorld,
              sizeof(modelDepthSortAfterWorld));
}

idRenderModelDust::~idRenderModelDust() {
  idMem::Singleton()->Free(particles, ALIGN_16);
}

void idRenderModelDust::SetParams(const idDeclParticle* const decl,
                                  const idBounds& fullVolume,
                                  const idBounds& coreVolume, float spacing,
                                  float edgeFadeCurve,
                                  const dustParameters_t& dustParms) {
  std::memmove(&parms.parms, &dustParms, sizeof(parms.parms));

  const float volumeX = fullVolume.b[1].x - fullVolume.b[0].x;
  const float volumeY = fullVolume.b[1].y - fullVolume.b[0].y;
  const float volumeZ = fullVolume.b[1].z - fullVolume.b[0].z;
  const float clampedSpacing = std::fmax(spacing, 1.0f);
  const int countX = static_cast<int>(std::fmin(
      static_cast<float>(static_cast<int>(volumeX / clampedSpacing)),
      10000.0f));
  const int countY = static_cast<int>(std::fmin(
      static_cast<float>(static_cast<int>(volumeY / clampedSpacing)),
      10000.0f));
  numParticles = countX * countY;
  particles = static_cast<dustParticle_t*>(
      idMem::Singleton()->AllocWithLocation(
          "w:\\build\\tech5\\engine\\models\\particles\\RenderModelDust.cpp(48) : TAG_PARTICLE",
          sizeof(dustParticle_t) * static_cast<std::size_t>(numParticles),
          static_cast<memTag_t>(79), false, ALIGN_16, HEAP_DEFAULTHEAP));
  particleDecl = decl;

  const float coreMaxY =
      std::fmin(coreVolume.b[1].y, fullVolume.b[1].y - 0.0001f);
  const float coreMaxX =
      std::fmin(coreVolume.b[1].x, fullVolume.b[1].x - 0.0001f);
  const float coreMinY =
      std::fmax(coreVolume.b[0].y, fullVolume.b[0].y + 0.0001f);
  const float coreMinX =
      std::fmax(coreVolume.b[0].x, fullVolume.b[0].x + 0.0001f);
  const idVec3& modelOrigin = ModelOrigin(*this);
  const float halfZ = volumeZ * 0.5f;
  const float cellX = volumeX / static_cast<float>(countX);
  const float cellY = volumeY / static_cast<float>(countY);
  const float fullMinX = modelOrigin.x + fullVolume.b[0].x;
  const float fullMinY = modelOrigin.y + fullVolume.b[0].y;
  const float centerZ =
      halfZ + (modelOrigin.z + fullVolume.b[0].z);
  const float worldCoreMinX = modelOrigin.x + coreMinX;
  const float worldCoreMinY = modelOrigin.y + coreMinY;
  const float worldCoreMaxX = modelOrigin.x + coreMaxX;
  const float worldCoreMaxY = modelOrigin.y + coreMaxY;
  const float firstY = cellY * 0.5f + fullMinY;
  const float leftFadeSpan = fullMinX - worldCoreMinX;
  const float lowerFadeSpan = fullMinY - worldCoreMinY;
  const float rightFadeSpan =
      modelOrigin.x + fullVolume.b[1].x - worldCoreMaxX;
  const float upperFadeSpan =
      modelOrigin.y + fullVolume.b[1].y - worldCoreMaxY;

  const float particleRadius = decl->bounds.GetMaxDistFromOrigin();
  referenceBounds.b[0].x = -(volumeX * 0.5f) - particleRadius;
  referenceBounds.b[0].y = -(volumeY * 0.5f) - particleRadius;
  referenceBounds.b[0].z = -halfZ - particleRadius;
  referenceBounds.b[1].x = volumeX * 0.5f + particleRadius;
  referenceBounds.b[1].y = volumeY * 0.5f + particleRadius;
  referenceBounds.b[1].z = halfZ + particleRadius;
  recoveredParms[118] |= 0x20u;

  std::uint32_t random = 0;
  for (int xIndex = 0; xIndex < countX; ++xIndex) {
    for (int yIndex = 0; yIndex < countY; ++yIndex) {
      const std::uint32_t randomZ = NextDustRandom(random);
      const std::uint32_t randomY = NextDustRandom(random);
      const std::uint32_t randomX = NextDustRandom(random);
      const float y =
          static_cast<float>(yIndex) / static_cast<float>(countY) * volumeY +
          firstY + SignedDustRandom(randomY) * cellY * 0.5f;
      const float z = centerZ + SignedDustRandom(randomZ) * volumeZ * 0.5f;
      const float x =
          static_cast<float>(xIndex) / static_cast<float>(countX) * volumeX +
          (cellX * 0.5f + fullMinX) +
          SignedDustRandom(randomX) * cellX * 0.5f;

      const float left = ClampUnit((x - worldCoreMinX) / leftFadeSpan);
      const float lower = ClampUnit((y - worldCoreMinY) / lowerFadeSpan);
      const float right = ClampUnit((x - worldCoreMaxX) / rightFadeSpan);
      const float upper = ClampUnit((y - worldCoreMaxY) / upperFadeSpan);

      dustParticle_t& particle =
          particles[xIndex * static_cast<std::int64_t>(countY) + yIndex];
      particle.initial = {x, y, z};
      const float leftFade = std::fmin(1.0f, 1.0f - left);
      const float lowerFade = std::fmin(leftFade, 1.0f - lower);
      const float rightFade = std::fmin(lowerFade, 1.0f - right);
      particle.baseAlpha =
          std::pow(std::fmin(rightFade, 1.0f - upper), edgeFadeCurve);

      const std::uint32_t influenceRandom = NextDustRandom(random);
      particle.influenceForce =
          UnitDustRandom(influenceRandom) *
              (parms.parms.influenceForce.max -
               parms.parms.influenceForce.min) +
          parms.parms.influenceForce.min;
      const std::uint32_t velocityRandom = NextDustRandom(random);
      particle.maxVelocity =
          UnitDustRandom(velocityRandom) *
              (parms.parms.maxVelocity.max - parms.parms.maxVelocity.min) +
          parms.parms.maxVelocity.min;
      const std::uint32_t energyRandom = NextDustRandom(random);
      particle.energySizeIncrase =
          UnitDustRandom(energyRandom) *
              (parms.parms.energySizeIncrase.max -
               parms.parms.energySizeIncrase.min) +
          parms.parms.energySizeIncrase.min;
      const std::uint32_t rotationRandom = NextDustRandom(random);
      particle.rotationSpeed =
          UnitDustRandom(rotationRandom) *
              (parms.parms.rotationSpeed.max -
               parms.parms.rotationSpeed.min) +
          parms.parms.rotationSpeed.min;
      const std::uint32_t directionRandom = NextDustRandom(random);
      particle.rotationDirection =
          ((directionRandom >> 16) & 1u) != 0u ? 1.0f : -1.0f;
      const std::uint32_t lifetimeRandom = NextDustRandom(random);
      particle.origin = particle.initial;
      particle.velocity = {0.0f, 0.0f, 0.0f};
      particle.state = 0;
      particle.fade = 1.0f;
      particle.energy = 0.0f;
      particle.rotation = 0.0f;
      particle.lifeTime = UnitDustRandom(lifetimeRandom) * 2.0f - 1.0f;
    }
  }
}

bool idRenderModelDust::UpdateInView(
    const idRenderView* currentView, const idRenderView* nextView,
    idRenderModelUpdateTools* updateTools) {
  if (r_skipDust.valueInteger != 0)
    return false;
  if (particleDecl == nullptr || particleDecl->stages.num <= 0)
    return false;

  parms.visibleInfluenceSpheres =
      updateTools->transparencyModel->visibleInfluenceSpheres;
  parms.stage = particleDecl->stages.list[0];
  parms.tables = particleDecl->GetTables();
  parms.viewOrg = nextView->r.vieworg;
  parms.viewFwd = nextView->r.viewaxis.mat[0];
  parms.viewLeft = nextView->r.viewaxis.mat[1];
  parms.viewUp = nextView->r.viewaxis.mat[2];
  parms.origin = ModelOrigin(*this);
  parms.transSortWidth = static_cast<float>(
      globalImages->transSortImageAtlas->ATLAS_WIDTH);
  parms.transSortHeight = static_cast<float>(
      globalImages->transSortImageAtlas->ATLAS_HEIGHT);
  parms.numParticles = numParticles;
  parms.particles = particles;

  const int renderTime = currentView->r.renderTime & 0x00ffffff;
  parms.timeDelta = lastRenderTime == -1
                        ? 0.016000001f
                        : std::fmin(0.016000001f,
                                    static_cast<float>(renderTime -
                                                       lastRenderTime) *
                                        0.001f);
  lastRenderTime = renderTime;

  int* used = nullptr;
  if (!idRenderModelTransparency::AllocateQuadSegment(
          numParticles, parms.verts, parms.depths, used, false))
    return false;
  if (r_useDustGenJobs.valueInteger == 1) {
    updateTools->renderModelJobList->AddJob(
        reinterpret_cast<jobRun_t>(DustGenJob), &parms);
  } else {
    DustGenJob(&parms);
  }
  *used = numParticles;
  return true;
}
