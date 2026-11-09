#pragma once

#include "decl.h"
#include "shared/idlib/math/vector.h"

class idDeclFX;
class idDeclImpactSound;
class idDeclParticle;
class idMaterial;

struct idBreakableSettings {
  float linearFriction, angularFriction, contactFriction;
  float linearFrictionWater, angularFrictionWater, bouncyness;
  idVec3 gravityVector;
  bool worldCollisionOnly, simplePointCollision;
  float crazyBounceChance, maxSimulationTime;
  const idDeclParticle *impactParticle, *deathParticle, *harmlessParticle;
  const idDeclImpactSound* impactSoundTable;
  float stopSpeed, maxLinearVelocity, maxAngularVelocity;
  bool noShadows;
  int clipMask;
  float dampeningDecay;
};

struct idBreakableExplosion {
  const idDeclFX* fx;
  const idDeclParticle* particle;
  idVec3 position;
  float radius, impulse, falloff, angular;
  int delay, duration, activateAllDelay;
  float distTimeScaleDelay;
  bool useLinear;
  idVec3 endLinear;
  idVec6 randomLinear, randomAngular;
  idList<int, 59> pieces;
};

struct idBreakableTrail {
  const idDeclParticle *particleTrail, *particleBounce, *particleDie;
  const idMaterial* glowQuadMaterial;
  float glowQuadSize, minTrailVelocity, minBounceVelocity, trailSpacing, trailVelocityScale;
  int delay, duration, fadeInDuration, fadeOutDuration;
  idList<int, 59> pieces;
};

struct idBreakableDecal {
  idList<const idMaterial*, 59> materials;
  idVec2 minSize, maxSize;
  int lifetime, fadeOutStartTime, fadeInEndTime;
  float minBounceVelocity, decalSpacing, depth;
  idList<int, 59> pieces;
  bool splatterOnBounce, splatterOnRest;
};

struct idBreakablePieceDecal {
  const idMaterial* material;
  idVec2 size;
  float depth, angle;
  idVec3 position, direction;
  idList<int> affectedPieces;
};

struct idBreakableSpark {
  idBreakableSpark();
  idList<const idMaterial*, 59> materials;
  float sizeMin, sizeMax, velocityScaleMin, velocityScaleMax;
};

class idDeclBreakable final : public idDecl {
public:
  idDeclBreakable();
  ~idDeclBreakable() override;

  const char* DefaultDefinition() const override;
  idDeclInfo* GetDeclInfo() const override;
  void Parse(idParser& source) override;
  void Clear();

  static int ContentsFromString(const char* text);
  void ParseArmoredPieces(idParser& source);
  void ParseHealthPieces(idParser& source);
  void ParseDestroyPieces(idParser& source);
  void ParseContents(idParser& source, int& contents);
  void ParseSpark(idParser& source);
  void ParseExplosion(idParser& source);
  void ParseTrail(idParser& source);
  void ParseDecals(idParser& source);
  void ParsePieceDecal(idParser& source);

  idAtomicString modelName;
  int numPieces;
  idBreakableSettings settings;
  idList<idBreakableExplosion, 59> explosions;
  idList<idBreakableTrail, 59> trails;
  idList<int, 59> armoredPieces, healthPieces, destroyPieces;
  idList<idBreakableDecal, 59> decals;
  idList<idBreakablePieceDecal> pieceDecals;
  idBreakableSpark sparks;
  static idDeclInfoTemplate<idDeclBreakable> resourceList;
};

static_assert(sizeof(idDeclBreakable) == 408, "idDeclBreakable ABI drift");
