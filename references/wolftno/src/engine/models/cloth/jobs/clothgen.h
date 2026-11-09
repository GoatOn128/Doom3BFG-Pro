#pragma once

#include "engine/models/cloth/declcloth.h"
#include "shared/idlib/bv/bounds.h"
#include "shared/idlib/bv/sphere.h"
#include "shared/idlib/math/plane.h"

#include <cstdint>

class idDrawVert;

struct idClothParticle {
  enum : std::uint16_t {
    STATE_CHILD_HAS_COLLISION = 1,
    STATE_ALL = 0xffff,
  };

  void Reset();
  bool UpdateLocalSpace(float timeDeltaSqr, const float* friction);
  void Collide(const idSphere* sphere, float radiusSqr);
  bool Simulate(float timeDelta, float frictionFactor);

  idVec3 origin{};
  idVec3 lastOrigin{};
  idVec3 velocity{};
  idVec3 force{};
  idMat3 axis{};
  float mass{};
  std::uint8_t neighbors[4]{};
  std::uint8_t anchored{};
  std::uint8_t recoveredPadding{};
  std::uint16_t state{};
};

struct idClothSpring {
  idClothSpring();
  void Init(float forceFactor, float friction, idClothParticle* cloth,
            int p1, int p2, bool band, bool edge);
  void Init2(int p1, int p2, float springLength, float springFriction,
             float springLengthMultiplier);
  void Constrain(idClothParticle* cloth);
  void Constrain2(idClothParticle* cloth);

  float springLength;
  float slSquared;
  float friction;
  float maxLengthMultiplier;
  float forceFactor;
  int p1;
  int p2;
  bool band;
  bool edge;
  std::uint8_t recoveredPadding[2];
};

struct clothWeaponTrace_t {
  idVec3 start{};
  idVec3 dir{};
  float strength{};
  bool causesHoles{};
  std::uint8_t recoveredPadding[3]{};
};

struct clothParms_t {
  clothType_t type{};
  std::uint32_t recoveredPadding0{};
  idClothParticle* cloth{};
  int numClothParticles{};
  std::uint32_t recoveredPadding1{};
  idClothSpring* springs{};
  int numSprings{};
  std::uint32_t recoveredPadding2{};
  idSphere* collisions{};
  int numCollisions{};
  std::uint32_t recoveredPadding3{};
  clothWeaponTrace_t* weaponTraces{};
  int numWeaponTraces{};
  std::uint32_t recoveredPadding4{};
  idDrawVert* vertices{};
  idBounds* bounds{};
  idVec3 org{};
  idMat3 axis{};
  float friction{};
  idVec3 gravity{};
  idVec3 windDirection{};
  idPlane collisionPlane{};
  bool hasCollisionPlane{};
  std::uint8_t recoveredPadding5[3]{};
  float collisionFriction{};
  int width{};
  int height{};
  float vSpacing{};
  float hSpacing{};
  float timeDelta{};
  int numIterations{};
  bool rect{};
  bool normalWind{};
  std::uint8_t recoveredTail[2]{};
};

bool SphereLineIntersection(const idVec3* origin, float radiusSqr,
                            const idVec3* start, const idVec3* end);
void MakeRotationMat(idMat3* axis, const idVec3* rotationVec, float angle);
void RopeGen(clothParms_t* clothParms);
void ClothGenJob(clothParms_t* clothParms);

static_assert(sizeof(idClothParticle) == 96, "idClothParticle ABI drift");
static_assert(sizeof(idClothSpring) == 32, "idClothSpring ABI drift");
static_assert(sizeof(clothWeaponTrace_t) == 32, "clothWeaponTrace_t ABI drift");
static_assert(sizeof(clothParms_t) == 216, "clothParms_t ABI drift");
static_assert(offsetof(clothParms_t, axis) == 100, "clothParms_t axis offset drift");
static_assert(offsetof(clothParms_t, normalWind) == 213,
              "clothParms_t normalWind offset drift");
