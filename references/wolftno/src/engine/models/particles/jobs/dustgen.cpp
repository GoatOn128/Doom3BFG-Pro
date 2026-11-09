#include "engine/models/particles/jobs/dustgen.h"

#include "engine/models/particles/jobs/particlegen.h"
#include "shared/idlib/parallelism/paralleljoblist.h"

#include <algorithm>
#include <cmath>
#include <cstring>
#include <limits>

namespace {

constexpr float kRandomScale = 0.000030518509f;

std::uint32_t NextRandom(idRandom2& random) {
  random.seed = 1664525u * random.seed + 1013904223u;
  return random.seed;
}

float UnitRandom(idRandom2& random) {
  return static_cast<float>((NextRandom(random) >> 10) & 0x7fffu) *
         kRandomScale;
}

float SignedRandom(idRandom2& random) {
  return UnitRandom(random) * 2.0f - 1.0f;
}

float SafeLength(const idVec3& value) {
  return std::sqrt(std::max(value.LengthSqr(),
                            std::numeric_limits<float>::min()));
}

std::uint8_t ToByte(float value) {
  return static_cast<std::uint8_t>(
      std::clamp(static_cast<int>(value * 255.0f), 0, 255));
}

void SinCos(float angle, float& sine, float& cosine) {
  float value = angle;
  if (value < 0.0f || value >= 6.2831855f)
    value -= std::floor(value * 0.15915494f) * 6.2831855f;
  float cosineSign = 1.0f;
  if (value < 3.1415927f) {
    if (value > 1.5707964f) {
      value = 3.1415927f - value;
      cosineSign = -1.0f;
    }
  } else if (value > 4.712389f) {
    value -= 6.2831855f;
  } else {
    value = 3.1415927f - value;
    cosineSign = -1.0f;
  }
  const float square = value * value;
  sine = (((((0.0000027526f - square * 0.0000000239f) * square -
              0.000198409f) *
                 square +
             0.0083333319f) *
                square -
            0.16666667f) *
               square +
           1.0f) *
          value;
  cosine = cosineSign *
           (((((0.0000247609f - square * 0.0000002605f) * square -
                0.0013888397f) *
                   square +
               0.041666642f) *
                  square -
              0.5f) *
                 square +
             1.0f);
}

} // namespace

float idParticleParm::EvalGeneric(float min, float max,
                                  idRandom2& rnd) const {
  const float value = UnitRandom(rnd) * (max - min) + min;
  return SignedRandom(rnd) * (value * variance) + value;
}

float idParticleParm::IntegrateMinMax(float min, float max, idRandom2& rnd,
                                      float frac) const {
  const float sampled = UnitRandom(rnd) * (max - min) + min;
  const float value = (0.5f * (sampled - sampled) * frac + sampled) * frac;
  return SignedRandom(rnd) * (value * variance) + value;
}

float idParticleParm::EvalTableWithVariance(const idLookupTable* tables,
                                            int idx, float frac,
                                            idRandom2& rnd) const {
  if (idx < 0 || tables == nullptr)
    return 0.0f;
  const float value = const_cast<idLookupTable&>(tables[idx]).TableLookup(
      frac, false);
  return SignedRandom(rnd) * (value * variance) + value;
}

float idParticleParm::Compute(const idLookupTable* tables, float frac,
                              idRandom2& random) const {
  switch (calcType) {
    case PARTICLE_CALC_CURVE_GENERIC:
      return EvalTableWithVariance(tables, tableIdx, frac, random) * val0 +
             val1;
    case PARTICLE_CALC_GENERIC:
      return EvalGeneric(val0, val1, random);
    case PARTICLE_CALC_CURVE_MOD_CURVE: {
      const float first = tableIdx >= 0 && tables != nullptr
          ? const_cast<idLookupTable&>(tables[tableIdx]).TableLookup(frac,
                                                                    false)
          : 0.0f;
      const float second = table2Idx >= 0 && tables != nullptr
          ? const_cast<idLookupTable&>(tables[table2Idx]).TableLookup(frac,
                                                                     false)
          : 0.0f;
      return first * second;
    }
    case PARTICLE_CALC_CURVE_ADD_CURVE: {
      const float first = tableIdx >= 0 && tables != nullptr
          ? const_cast<idLookupTable&>(tables[tableIdx]).TableLookup(frac,
                                                                    false)
          : 0.0f;
      const float second = table2Idx >= 0 && tables != nullptr
          ? const_cast<idLookupTable&>(tables[table2Idx]).TableLookup(frac,
                                                                     false)
          : 0.0f;
      return first + second;
    }
    case PARTICLE_CALC_PARAMETRIC_EVAL: {
      const float value = (val1 - val0) * frac + val0;
      return SignedRandom(random) * (value * variance) + value;
    }
    case PARTICLE_CALC_PARAMETRIC_INTEGRATE: {
      const float value = ((val1 - val0) * frac * 0.5f + val0) * frac;
      return SignedRandom(random) * (value * variance) + value;
    }
    case PARTICLE_CALC_PARAMETRIC_INTEGRATE_MINMAX:
      return IntegrateMinMax(val0, val1, random, frac);
    default:
      return 0.0f;
  }
}

void DustGenJob(const deferredDustGenParms_t* parms) {
  float sine, cosine;
  SinCos(-parms->viewUp.z * parms->parms.particleViewTilt, sine, cosine);
  const idVec3 adjustedViewUp = parms->viewUp * cosine -
                                parms->viewFwd * sine;
  particleGen_t gen{};
  for (int index = 0; index < parms->numParticles; ++index) {
    dustParticle_t& particle = parms->particles[index];
    if (particle.state != 0 || particle.fade > 0.5f) {
      idVec3 force{0.0f, 0.0f, -parms->parms.gravity};
      const int sphereCount = parms->visibleInfluenceSpheres->numSpheres;
      for (int sphereIndex = 0; sphereIndex < sphereCount; ++sphereIndex) {
        const influenceSphere_t& sphere =
            parms->visibleInfluenceSpheres->spheres[sphereIndex];
        const idVec3 delta = sphere.center - particle.origin;
        const float distance = SafeLength(delta);
        if (sphere.outerRadius > distance) {
          if (SafeLength(sphere.directionalForce) > 0.0f) {
            force += sphere.directionalForce * particle.influenceForce;
          } else {
            force += delta * ((1.0f / distance) *
                              ((1.0f - sphere.outerRadius) *
                               particle.influenceForce));
          }
        }
      }
      particle.velocity += force * parms->timeDelta;
      particle.velocity *= 1.0f - parms->parms.dampning;
    }

    const float speed = SafeLength(particle.velocity);
    if (speed > particle.maxVelocity)
      particle.velocity *= particle.maxVelocity / speed;

    const idVec3 oldOrigin = particle.origin;
    particle.origin += particle.velocity * parms->timeDelta;
    if (particle.initial.z > particle.origin.z) {
      particle.origin.z = particle.initial.z;
      particle.velocity.z = 0.0f;
    }

    const float movement = SafeLength(particle.origin - oldOrigin);
    const float viewDistance = SafeLength(parms->viewOrg - particle.origin);
    const float fadeStep = parms->timeDelta / parms->parms.fadeouttime;
    const float effectiveMovement = std::max(movement - 0.001f, 0.0f);
    particle.energy += effectiveMovement / particle.maxVelocity * fadeStep;
    particle.rotation += effectiveMovement *
                             parms->parms.energyRotationSpeed *
                             particle.rotationDirection +
                         parms->timeDelta * particle.rotationSpeed;
    if (particle.state == 1) {
      particle.fade -= fadeStep;
      if (particle.fade < 0.0f) {
        particle.state = 0;
        particle.fade = 0.0f;
        particle.energy = 0.0f;
        particle.rotation = 0.0f;
        particle.origin = particle.initial;
        particle.velocity = {0.0f, 0.0f, 0.0f};
      }
    } else {
      const idVec2 planarDelta{particle.origin.x - particle.initial.x,
                               particle.origin.y - particle.initial.y};
      if (particle.fade > 0.5f &&
          planarDelta.LengthSqr() >
              parms->parms.distanceBeforeFadeout *
                  parms->parms.distanceBeforeFadeout)
        particle.state = 1;
      particle.fade += parms->timeDelta / parms->parms.fadeintime;
      particle.fade = std::min(particle.fade, 1.0f);
    }

    particle.lifeTime = std::fmod(
        std::min(parms->parms.movementAnimation * movement +
                     parms->timeDelta * 0.05f,
                 parms->timeDelta * parms->parms.maxAnimationSpeed) +
            particle.lifeTime,
        1.0f);
    const float parmFraction = particle.state != 0
                                   ? (1.0f - particle.fade) + particle.lifeTime
                                   : particle.lifeTime;
    gen.index = index;
    gen.cycleAge = parmFraction;
    gen.totalAge = parmFraction;
    gen.particleLife = 1.0f;
    gen.frac = parmFraction;
    gen.parmVal = parmFraction;

    const float alpha =
        parms->stage->colorAttributes.baseColor[3].Compute(
            parms->tables, parmFraction, gen.random) *
        particle.fade * particle.baseAlpha;
    const float blue = parms->stage->colorAttributes.baseColor[2].Compute(
        parms->tables, parmFraction, gen.random);
    const float green = parms->stage->colorAttributes.baseColor[1].Compute(
        parms->tables, parmFraction, gen.random);
    const float red = parms->stage->colorAttributes.baseColor[0].Compute(
        parms->tables, parmFraction, gen.random);
    gen.vertColor = {red, green, blue, alpha};

    idVec3 left, up;
    ParticleLeftAndUp(parms->stage, parms->tables, &gen, adjustedViewUp,
                      parms->viewLeft, particle.rotation, left, up);
    const float heightScale =
        (std::min(std::max(std::fabs(parms->viewOrg.z - particle.origin.z) -
                               parms->parms.heightScaleOffset,
                           0.0f),
                  parms->parms.heightScaleRange) /
             parms->parms.heightScaleRange *
             parms->parms.heightScaleAmount) +
        1.0f;
    const float size = parms->stage->size.size[0].Compute(
                           parms->tables, parmFraction, gen.random) *
                       (particle.energySizeIncrase * particle.energy + 1.0f) *
                       heightScale;
    const float aspect = parms->stage->size.aspectRatio.Compute(
                             parms->tables, parmFraction, gen.random) * size;
    const idVec3 scaledUp = up * size;
    const idVec3 scaledLeft = left * aspect;
    alignas(16) idTransparencyVert verts[4];
    verts[0].xyz = particle.origin + scaledUp - scaledLeft;
    verts[1].xyz = particle.origin + scaledUp + scaledLeft;
    verts[2].xyz = particle.origin - scaledUp - scaledLeft;
    verts[3].xyz = particle.origin - scaledUp + scaledLeft;

    const float generic3 =
        parms->stage->genericParm.genericParm[3].Compute(
            parms->tables, parmFraction, gen.random);
    const float generic2 =
        parms->stage->genericParm.genericParm[2].Compute(
            parms->tables, parmFraction, gen.random);
    const float generic1 =
        parms->stage->genericParm.genericParm[1].Compute(
            parms->tables, parmFraction, gen.random);
    const float generic0 =
        parms->stage->genericParm.genericParm[0].Compute(
            parms->tables, parmFraction, gen.random);
    const std::uint8_t generic[4] = {
        ToByte(generic0), ToByte(generic1), ToByte(generic2),
        ToByte(generic3)};
    const std::uint8_t color[4] = {ToByte(gen.vertColor.x),
                                   ToByte(gen.vertColor.y),
                                   ToByte(gen.vertColor.z),
                                   ToByte(gen.vertColor.w)};
    const std::uint8_t softAlpha = ToByte(
        parms->stage->colorAttributes.softParticleAlphaScale);
    for (int corner = 0; corner < 4; ++corner) {
      std::memcpy(verts[corner].normal, generic, sizeof(generic));
      verts[corner].tangent[3] = softAlpha;
      std::memcpy(verts[corner].color, color, sizeof(color));
    }

    parms->depths[index] = viewDistance;
    ParticleTexCoords(parms->stage, parms->tables, &gen, {0.0f, 1.0f},
                      {0.0f, 1.0f},
                      static_cast<int>(parms->transSortWidth),
                      static_cast<int>(parms->transSortHeight), verts);
    std::memcpy(parms->verts + index * 4, verts, sizeof(verts));
  }
}

idParallelJobRegistration register_DustGenJob(
    reinterpret_cast<jobRun_t>(DustGenJob), "DustGenJob");
