#include "engine/models/cloth/jobs/clothgen.h"

#include "shared/idlib/geometry/drawvert.h"
#include "shared/idlib/parallelism/paralleljoblist.h"

#include <algorithm>
#include <cfloat>
#include <cmath>
#include <cstdlib>
#include <cstring>

namespace {

float InvLength(const idVec3& value) {
  return 1.0f / std::sqrt(std::max(value.LengthSqr(), FLT_MIN));
}

idVec3 Normalized(const idVec3& value) {
  return value * InvLength(value);
}

std::uint8_t PackDirection(float value) {
  const long packed = std::lrintf((value + 1.0f) * 127.5f);
  return static_cast<std::uint8_t>(std::clamp(packed, 0L, 255L));
}

void PackDirection3(std::uint8_t packed[4], const idVec3& value) {
  packed[0] = PackDirection(value.x);
  packed[1] = PackDirection(value.y);
  packed[2] = PackDirection(value.z);
}

void ClearBounds(idBounds& bounds) {
  bounds.b[0] = {1.0e30f, 1.0e30f, 1.0e30f};
  bounds.b[1] = {-1.0e30f, -1.0e30f, -1.0e30f};
}

void AddPoint(idBounds& bounds, const idVec3& point) {
  bounds.b[0].x = std::fmin(bounds.b[0].x, point.x);
  bounds.b[0].y = std::fmin(bounds.b[0].y, point.y);
  bounds.b[0].z = std::fmin(bounds.b[0].z, point.z);
  bounds.b[1].x = std::fmax(bounds.b[1].x, point.x);
  bounds.b[1].y = std::fmax(bounds.b[1].y, point.y);
  bounds.b[1].z = std::fmax(bounds.b[1].z, point.z);
}

idVec3 WorldToLocalVector(const idMat3& axis, const idVec3& value) {
  return {value * axis.mat[0], value * axis.mat[1], value * axis.mat[2]};
}

}  // namespace

bool SphereLineIntersection(const idVec3* origin, float radiusSqr,
                            const idVec3* start, const idVec3* end) {
  const idVec3 localStart = *start - *origin;
  const idVec3 localEnd = *end - *origin;
  const idVec3 segment = localEnd - localStart;
  const float projection = (-localStart) * segment;

  float distanceSqr;
  if (projection <= 0.0f) {
    distanceSqr = localStart.LengthSqr();
  } else {
    const float segmentLengthSqr = segment.LengthSqr();
    if (projection < segmentLengthSqr) {
      distanceSqr = (localStart + segment * (projection / segmentLengthSqr)).LengthSqr();
    } else {
      distanceSqr = localEnd.LengthSqr();
    }
  }
  return distanceSqr < radiusSqr;
}

void MakeRotationMat(idMat3* axis, const idVec3* rotationVec, float angle) {
  const float halfAngle = angle * 0.5f;
  const float sine = std::sin(halfAngle);
  const float cosine = std::cos(halfAngle);
  const float qx = sine * rotationVec->x;
  const float qy = sine * rotationVec->y;
  const float qz = sine * rotationVec->z;

  const float qx2 = qx + qx;
  const float qy2 = qy + qy;
  const float qz2 = qz + qz;
  axis->mat[0].x = 1.0f - (qz2 * qz + qy2 * qy);
  axis->mat[0].y = qy2 * qx - qz2 * cosine;
  axis->mat[0].z = qy2 * cosine + qz2 * qx;
  axis->mat[1].x = qz2 * cosine + qy2 * qx;
  axis->mat[1].y = 1.0f - (qz2 * qz + qx2 * qx);
  axis->mat[1].z = qz2 * qy - qx2 * cosine;
  axis->mat[2].x = qz2 * qx - qy2 * cosine;
  axis->mat[2].y = qx2 * cosine + qz2 * qy;
  axis->mat[2].z = 1.0f - (qy2 * qy + qx2 * qx);
}

void idClothParticle::Reset() {
  origin = {};
  lastOrigin = {};
  axis.mat[0] = {1.0f, 0.0f, 0.0f};
  axis.mat[1] = {0.0f, 1.0f, 0.0f};
  axis.mat[2] = {0.0f, 0.0f, 1.0f};
  velocity = {};
  force = {};
  anchored = 0;
  state = 0;
}

bool idClothParticle::UpdateLocalSpace(float timeDeltaSqr,
                                       const float* friction) {
  bool result = false;
  if (anchored == 0) {
    const idVec3 oldOrigin = origin;
    velocity = force * (1.0f / mass);
    const float currentFactor = 2.0f - *friction;
    const float previousFactor = 1.0f - *friction;
    origin = origin * currentFactor - lastOrigin * previousFactor +
             velocity * timeDeltaSqr;
    lastOrigin = oldOrigin;
    result = true;
  }
  force = {};
  return result;
}

void idClothParticle::Collide(const idSphere* sphere, float radiusSqr) {
  const idVec3 delta = origin - sphere->origin;
  const float distanceSqr = delta.LengthSqr();
  if (sphere->radius >= 0.0f) {
    if (radiusSqr <= distanceSqr)
      return;
  } else if (distanceSqr <= radiusSqr) {
    return;
  }

  if (anchored == 0) {
    origin = sphere->origin +
             delta * (InvLength(delta) * std::fabs(sphere->radius));
  }
}

bool idClothParticle::Simulate(float timeDelta, float frictionFactor) {
  if (anchored == 0) {
    const idVec3 movement = origin - lastOrigin;
    lastOrigin = origin;
    origin += movement * (1.0f - frictionFactor) +
              force * (timeDelta * timeDelta);
  }
  return true;
}

idClothSpring::idClothSpring() {
  forceFactor = 1.0f;
  p1 = 0;
  friction = 0.0f;
  p2 = 0;
  band = false;
  edge = false;
}

void idClothSpring::Init(float newForceFactor, float newFriction,
                         idClothParticle* cloth, int newP1, int newP2,
                         bool newBand, bool newEdge) {
  friction = newFriction;
  forceFactor = newForceFactor;
  p2 = newP2;
  p1 = newP1;
  band = newBand;
  edge = newEdge;
  const idVec3 delta = cloth[p1].origin - cloth[p2].origin;
  springLength = std::sqrt(std::max(delta.LengthSqr(), FLT_MIN));
  if (band)
    springLength *= 1.15f;
  slSquared = springLength * springLength;
}

void idClothSpring::Init2(int newP1, int newP2, float newSpringLength,
                          float springFriction,
                          float springLengthMultiplier) {
  springLength = newSpringLength;
  p1 = newP1;
  p2 = newP2;
  band = false;
  edge = false;
  friction = springFriction;
  maxLengthMultiplier = springLengthMultiplier;
}

void idClothSpring::Constrain(idClothParticle* cloth) {
  idClothParticle& particle1 = cloth[p1];
  idClothParticle& particle2 = cloth[p2];
  const idVec3 delta = particle1.origin - particle2.origin;
  const float scale =
      (slSquared / (delta.LengthSqr() + slSquared) - 0.5f) *
      (forceFactor * 0.5f);
  const idVec3 correction = delta * scale;
  if (particle2.anchored == 0)
    particle2.origin -= correction;
  if (particle1.anchored == 0)
    particle1.origin += correction;
}

void idClothSpring::Constrain2(idClothParticle* cloth) {
  idClothParticle& particle1 = cloth[p1];
  idClothParticle& particle2 = cloth[p2];
  const idVec3 delta = particle2.origin - particle1.origin;
  const float distanceSqr = delta.LengthSqr();
  const float inverseDistance = 1.0f / std::sqrt(std::max(distanceSqr, FLT_MIN));
  const float stretch = inverseDistance * distanceSqr - springLength;
  const idVec3 correction =
      delta * inverseDistance * (stretch * 0.5f * forceFactor);
  if (particle1.anchored == 0)
    particle1.origin += correction;
  if (particle2.anchored == 0)
    particle2.origin -= correction;
}

void idDrawVert::SetBiTangent(const idVec3& tangentDirection) {
  idVec3 decodedTangent{
      tangent[0] * 0.0078431377f - 1.0f,
      tangent[1] * 0.0078431377f - 1.0f,
      tangent[2] * 0.0078431377f - 1.0f};
  idVec3 decodedNormal{
      normal[0] * 0.0078431377f - 1.0f,
      normal[1] * 0.0078431377f - 1.0f,
      normal[2] * 0.0078431377f - 1.0f};
  decodedTangent *= InvLength(decodedTangent);
  decodedNormal *= InvLength(decodedNormal);

  const float handedness = decodedNormal.Cross(decodedTangent) * tangentDirection;
  tangent[3] = static_cast<std::uint8_t>((tangent[3] & ~0x80u) |
                                         (handedness >= 0.0f ? 0x80u : 0u));
}

void RopeGen(clothParms_t* clothParms) {
  if (clothParms->timeDelta > 0.0f) {
    for (int traceIndex = 0; traceIndex < clothParms->numWeaponTraces;
         ++traceIndex) {
      for (int particleIndex = 0;
           particleIndex < clothParms->numClothParticles - 1;
           particleIndex += 2) {
        if (clothParms->cloth[particleIndex + 1].anchored != 0)
          continue;
        const idVec3& pointA = clothParms->cloth[particleIndex].origin;
        const idVec3& pointB = clothParms->cloth[particleIndex + 2].origin;
        const idVec3 center = (pointA + pointB) * 0.5f;
        const float radiusSqr = (pointA - center).LengthSqr();
        if (SphereLineIntersection(&center, radiusSqr, &pointA, &pointB)) {
          clothParms->cloth[0].state |=
              idClothParticle::STATE_CHILD_HAS_COLLISION;
          break;
        }
      }
    }

    for (int particleIndex = 0;
         particleIndex < clothParms->numClothParticles; ++particleIndex) {
      idClothParticle& particle = clothParms->cloth[particleIndex];
      particle.force += clothParms->windDirection;
      particle.force += clothParms->gravity;
      particle.force += particle.velocity * -clothParms->friction;

      if (clothParms->hasCollisionPlane) {
        const idVec3 planeNormal{clothParms->collisionPlane.a,
                                 clothParms->collisionPlane.b,
                                 clothParms->collisionPlane.c};
        const float planeDistance =
            planeNormal * particle.origin + clothParms->collisionPlane.d;
        if (planeDistance < 0.00001f) {
          const idVec3 tangentialVelocity =
              particle.velocity - planeNormal * (planeNormal * particle.velocity);
          particle.force +=
              tangentialVelocity * -clothParms->collisionFriction;
        }
      }

      particle.Simulate(clothParms->timeDelta, clothParms->friction);

      if (clothParms->hasCollisionPlane) {
        const idVec3 planeNormal{clothParms->collisionPlane.a,
                                 clothParms->collisionPlane.b,
                                 clothParms->collisionPlane.c};
        const float planeDistance =
            planeNormal * particle.origin + clothParms->collisionPlane.d;
        if (planeDistance < 0.0f && particle.anchored == 0)
          particle.origin += planeNormal * -planeDistance;
      }

      for (int collisionIndex = 0;
           collisionIndex < clothParms->numCollisions; ++collisionIndex) {
        const idSphere& sphere = clothParms->collisions[collisionIndex];
        const idVec3 delta = particle.origin - sphere.origin;
        if (delta.LengthSqr() < sphere.radius * sphere.radius &&
            particle.anchored == 0) {
          particle.origin =
              sphere.origin + delta * (InvLength(delta) * sphere.radius);
        }
      }
      particle.force = {};
    }

    for (int springIndex = 0; springIndex < clothParms->numSprings;
         ++springIndex) {
      clothParms->springs[springIndex].Constrain2(clothParms->cloth);
    }

    for (int springIndex = 0; springIndex < clothParms->numSprings;
         ++springIndex) {
      const idClothSpring& spring = clothParms->springs[springIndex];
      idClothParticle* reference = &clothParms->cloth[spring.p1];
      idClothParticle* movable = &clothParms->cloth[spring.p2];
      if (movable->anchored != 0)
        std::swap(reference, movable);

      const idVec3 delta = reference->origin - movable->origin;
      const float distanceSqr = delta.LengthSqr();
      const float inverseDistance =
          1.0f / std::sqrt(std::max(distanceSqr, FLT_MIN));
      const float distance = inverseDistance * distanceSqr;
      const idVec3 direction = delta * inverseDistance;
      const float maximumLength =
          spring.maxLengthMultiplier * spring.springLength;
      if (distance > maximumLength && movable->anchored == 0)
        movable->origin = reference->origin - direction * maximumLength;
      if (spring.springLength > distance && movable->anchored == 0)
        movable->origin = reference->origin - direction * spring.springLength;
    }
  }

  if (clothParms->numClothParticles >= 2) {
    idVec3 previousRight = clothParms->cloth[0].axis.mat[0];
    idVec3 previousForward = clothParms->cloth[0].axis.mat[1];
    idVec3 previousUp = clothParms->cloth[0].axis.mat[2];

    for (int particleIndex = 0;
         particleIndex < clothParms->numClothParticles - 1; ++particleIndex) {
      idVec3 currentForward =
          clothParms->cloth[particleIndex + 1].origin -
          clothParms->cloth[particleIndex].origin;
      if (currentForward.x != 0.0f || currentForward.y != 0.0f ||
          currentForward.z != 0.0f) {
        currentForward *= InvLength(currentForward);
      }

      const float alignment = previousForward * currentForward;
      idVec3 currentRight = previousRight;
      idVec3 currentUp = previousUp;
      if (alignment < -0.99999f) {
        currentUp = -currentUp;
      } else if (alignment <= 0.99999f) {
        idVec3 rotationAxis = previousForward.Cross(currentForward);
        rotationAxis *= InvLength(rotationAxis);
        const float angle = alignment > -1.0f
                                ? (alignment < 1.0f ? std::acos(alignment) : 0.0f)
                                : 3.1415927f;
        idMat3 rotationMatrix;
        MakeRotationMat(&rotationMatrix, &rotationAxis, -angle);
        currentRight = rotationMatrix * previousRight;
        currentRight *= InvLength(currentRight);
        currentUp = currentRight.Cross(currentForward);
        currentUp *= InvLength(currentUp);
      }

      clothParms->cloth[particleIndex].axis.mat[0] = currentRight;
      clothParms->cloth[particleIndex].axis.mat[1] = currentForward;
      clothParms->cloth[particleIndex].axis.mat[2] = currentUp;
      previousRight = currentRight;
      previousForward = currentForward;
      previousUp = currentUp;
    }
    clothParms->cloth[clothParms->numClothParticles - 1].axis =
        clothParms->cloth[clothParms->numClothParticles - 2].axis;
  }

  ClearBounds(*clothParms->bounds);
  for (int particleIndex = 0;
       particleIndex < clothParms->numClothParticles; ++particleIndex) {
    AddPoint(*clothParms->bounds, clothParms->cloth[particleIndex].origin);
  }
}

void ClothGenJob(clothParms_t* clothParms) {
  if (clothParms->type != CLOTH_FLAG) {
    RopeGen(clothParms);
    return;
  }

  const float timeDeltaSqr = clothParms->timeDelta * clothParms->timeDelta;
  const float friction = clothParms->friction;
  if (timeDeltaSqr > 0.0f) {
    for (int traceIndex = 0; traceIndex < clothParms->numWeaponTraces;
         ++traceIndex) {
      const clothWeaponTrace_t& trace = clothParms->weaponTraces[traceIndex];
      const idVec3 localStart =
          WorldToLocalVector(clothParms->axis, trace.start - clothParms->org);
      const idVec3 localDirection =
          WorldToLocalVector(clothParms->axis, trace.dir);
      int closestParticle = -1;
      float closestRatio = 999999.0f;
      float closestHoleRatio = 999999.0f;
      int pushIndices[9]{};

      for (int particleIndex = 0;
           particleIndex < clothParms->numClothParticles; ++particleIndex) {
        const idVec3 delta =
            clothParms->cloth[particleIndex].origin - localStart;
        const float projection = delta * localDirection;
        if (projection <= 0.0f)
          continue;
        const float ratio = delta.LengthSqr() / (projection * projection);
        if (ratio < closestRatio && ratio < 25.0f) {
          closestRatio = ratio;
          closestParticle = particleIndex;
          const idClothParticle& particle = clothParms->cloth[particleIndex];
          pushIndices[0] = particleIndex;
          pushIndices[1] = particle.neighbors[0];
          pushIndices[2] = particle.neighbors[1];
          pushIndices[3] = particle.neighbors[2];
          pushIndices[4] = particle.neighbors[3];
          pushIndices[5] = clothParms->cloth[particle.neighbors[1]].neighbors[0];
          pushIndices[6] = clothParms->cloth[particle.neighbors[1]].neighbors[2];
          pushIndices[7] = clothParms->cloth[particle.neighbors[3]].neighbors[2];
          pushIndices[8] = clothParms->cloth[particle.neighbors[3]].neighbors[0];
        }
        if (closestParticle != -1 && trace.causesHoles &&
            ratio < closestHoleRatio) {
          closestHoleRatio = ratio;
        }
      }

      if (closestParticle != -1) {
        for (int pushIndex = 0; pushIndex < 9; ++pushIndex) {
          float strengthScale;
          if (pushIndex == 0)
            strengthScale = 1.0f;
          else if (pushIndex <= 4)
            strengthScale = 0.75f;
          else if (pushIndex >= 6)
            strengthScale = 0.25f;
          else
            continue;
          clothParms->cloth[pushIndices[pushIndex]].force +=
              localDirection * (trace.strength * strengthScale);
        }
      }
    }

    const idVec3 localWind =
        WorldToLocalVector(clothParms->axis, clothParms->windDirection);
    const idVec3 localGravity =
        WorldToLocalVector(clothParms->axis, clothParms->gravity);
    bool updated = false;
    for (int iteration = 0; iteration < clothParms->numIterations;
         ++iteration) {
      for (int particleIndex = 0;
           particleIndex < clothParms->numClothParticles; ++particleIndex) {
        idVec3 appliedWind;
        const int randomValue = std::rand();
        const int windScale =
            randomValue % clothParms->numClothParticles /
            (clothParms->numClothParticles / 2);
        if (clothParms->normalWind) {
          const idClothParticle& particle = clothParms->cloth[particleIndex];
          const idVec3 edgeA = Normalized(
              clothParms->cloth[particle.neighbors[1]].origin -
              clothParms->cloth[particle.neighbors[3]].origin);
          const idVec3 edgeB = Normalized(
              clothParms->cloth[particle.neighbors[2]].origin -
              clothParms->cloth[particle.neighbors[0]].origin);
          const idVec3 normal = edgeB.Cross(edgeA);
          const float normalWind =
              (localWind * normal) * (static_cast<float>(windScale) * 0.7f);
          appliedWind = localWind * 0.3f + normal * normalWind;
        } else {
          appliedWind = localWind * static_cast<float>(windScale);
        }

        idClothParticle& particle = clothParms->cloth[particleIndex];
        particle.force += localGravity * particle.mass + appliedWind;
        updated |= particle.UpdateLocalSpace(timeDeltaSqr, &friction);
      }

      if (updated) {
        for (int springIndex = 0; springIndex < clothParms->numSprings;
             ++springIndex) {
          clothParms->springs[springIndex].Constrain(clothParms->cloth);
        }
      }
      for (int collisionIndex = 0;
           collisionIndex < clothParms->numCollisions; ++collisionIndex) {
        const idSphere& sphere = clothParms->collisions[collisionIndex];
        const float radiusSqr = sphere.radius * sphere.radius;
        for (int particleIndex = 0;
             particleIndex < clothParms->numClothParticles; ++particleIndex) {
          clothParms->cloth[particleIndex].Collide(&sphere, radiusSqr);
        }
      }
    }

    if (clothParms->numIterations > 1) {
      for (int particleIndex = 0;
           particleIndex < clothParms->numClothParticles; ++particleIndex) {
        idClothParticle& particle = clothParms->cloth[particleIndex];
        if (particle.anchored == 2)
          particle.anchored = 0;
        particle.lastOrigin = particle.origin;
      }
    }
  }

  idDrawVert localVerts[128]{};
  ClearBounds(*clothParms->bounds);
  for (int particleIndex = 0;
       particleIndex < clothParms->numClothParticles; ++particleIndex) {
    localVerts[particleIndex].xyz = clothParms->cloth[particleIndex].origin;
    AddPoint(*clothParms->bounds, localVerts[particleIndex].xyz);
  }

  if (clothParms->vertices == nullptr)
    return;

  int vertexIndex = 0;
  if (clothParms->rect) {
    float textureY = 0.01f;
    const float textureYStep =
        0.98f / (static_cast<float>(clothParms->height) - 1.0f);
    const float textureXStep =
        0.98f / (static_cast<float>(clothParms->width) - 1.0f);
    for (int row = 0; row < clothParms->height; ++row) {
      float textureX = 0.01f;
      for (int column = 0; column < clothParms->width;
           ++column, ++vertexIndex) {
        idDrawVert& vertex = localVerts[vertexIndex];
        vertex.st = {textureY, textureX};
        const idClothParticle& particle = clothParms->cloth[vertexIndex];
        const idVec3 right = Normalized(
            localVerts[particle.neighbors[1]].xyz -
            localVerts[particle.neighbors[3]].xyz);
        const idVec3 up = Normalized(
            localVerts[particle.neighbors[2]].xyz -
            localVerts[particle.neighbors[0]].xyz);
        PackDirection3(vertex.normal, up.Cross(right));
        PackDirection3(vertex.tangent, up);
        vertex.SetBiTangent(right);
        textureX += textureXStep;
      }
      textureY += textureYStep;
    }
  } else {
    const float fullWidth =
        static_cast<float>(clothParms->width - 1) * clothParms->hSpacing;
    float rowStart = fullWidth * 0.5f;
    int rowWidth = 1;
    for (int row = 0; row < clothParms->height; ++row) {
      float horizontal = rowStart;
      const float textureY =
          (static_cast<float>(row) * clothParms->vSpacing) /
          (static_cast<float>(clothParms->height - 1) * clothParms->vSpacing);
      for (int column = 0; column < rowWidth;
           ++column, ++vertexIndex) {
        idDrawVert& vertex = localVerts[vertexIndex];
        vertex.st = {textureY, horizontal / fullWidth};
        const idClothParticle& particle = clothParms->cloth[vertexIndex];
        const idVec3 bitangent = Normalized(
            clothParms->cloth[particle.neighbors[1]].origin -
            clothParms->cloth[particle.neighbors[3]].origin);
        const idVec3 tangent = Normalized(
            clothParms->cloth[particle.neighbors[2]].origin -
            clothParms->cloth[particle.neighbors[0]].origin);
        PackDirection3(vertex.normal, tangent.Cross(bitangent));
        PackDirection3(vertex.tangent, tangent);
        vertex.SetBiTangent(bitangent);
        horizontal += clothParms->hSpacing;
      }
      rowWidth += 2;
      rowStart -= clothParms->hSpacing;
    }
  }

  std::memcpy(clothParms->vertices, localVerts,
              static_cast<std::size_t>(clothParms->numClothParticles) *
                  sizeof(idDrawVert));
}

idParallelJobRegistration register_ClothGenJob(
    reinterpret_cast<jobRun_t>(ClothGenJob), "ClothGenJob");
