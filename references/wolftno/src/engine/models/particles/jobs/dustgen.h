#pragma once

#include "engine/models/static/jobs/statictransparencygen.h"
#include "engine/models/transparency/jobs/influencespherecull.h"
#include "shared/idlib/bv/bounds.h"
#include "shared/idlib/lookuptable.h"
#include "shared/idlib/math/random.h"

#include <cstddef>
#include <cstdint>

class idDeclParticle;
class idMaterial;
class idStaticParticleModelData;
struct prtLocation_t;

enum particleCalcType_t : std::int32_t {
  PARTICLE_CALC_NONE = 0,
  PARTICLE_CALC_CURVE_GENERIC = 1,
  PARTICLE_CALC_GENERIC = 2,
  PARTICLE_CALC_CURVE_MOD_CURVE = 3,
  PARTICLE_CALC_CURVE_ADD_CURVE = 4,
  PARTICLE_CALC_PARAMETRIC_EVAL = 5,
  PARTICLE_CALC_PARAMETRIC_INTEGRATE = 6,
  PARTICLE_CALC_PARAMETRIC_INTEGRATE_MINMAX = 7,
  NUM_PARTICLE_CALC_TYPES = 8,
};

enum particleEditCalcType_t : std::int32_t {
  PARTICLE_EDIT_CALC_NONE = 0,
  PARTICLE_EDIT_CALC_CONSTANT = 1,
  PARTICLE_EDIT_CALC_MINMAX = 2,
  PARTICLE_EDIT_CALC_CURVE = 3,
  PARTICLE_EDIT_CALC_CURVE_SCALE_BIAS = 4,
  PARTICLE_EDIT_CALC_CURVE_VARIANCE_MOD_CONSTANT = 5,
  PARTICLE_EDIT_CALC_CURVE_MOD_CURVE = 6,
  PARTICLE_EDIT_CALC_CURVE_ADD_CURVE = 7,
  PARTICLE_EDIT_CALC_PARAMETRIC_EVAL = 8,
  PARTICLE_EDIT_CALC_PARAMETRIC_INTEGRATE = 9,
  PARTICLE_EDIT_CALC_PARAMETRIC_INTEGRATE_MINMAX = 10,
  NUM_PARTICLE_EDIT_CALC_TYPES = 11,
};

class idParticleParm {
public:
  float val0;
  float val1;
  float variance;
  std::int16_t tableIdx;
  std::int16_t table2Idx;
  particleCalcType_t calcType;

  float Compute(const idLookupTable* tables, float frac,
                idRandom2& random) const;
  float GetMaxParmVal(const idLookupTable* tables) const;
  void SetCalcTypeFromEditType(particleEditCalcType_t editType);
  particleEditCalcType_t GetEditCalcType() const;

private:
  float EvalGeneric(float min, float max, idRandom2& rnd) const;
  float IntegrateMinMax(float min, float max, idRandom2& rnd,
                        float frac) const;
  float EvalTableWithVariance(const idLookupTable* tables, int idx,
                              float frac, idRandom2& rnd) const;
};

enum prtOrientationType_t : std::int32_t {
  POR_VIEW = 0,
  POR_TRAIL = 1,
  POR_AIMED = 2,
  POR_X = 3,
  POR_Y = 4,
  POR_Z = 5,
  POR_XYZ = 6,
};

enum prtTextureFlipMode_t : std::int32_t {
  PTEXTURE_FLIP_NONE = 0,
  PTEXTURE_FLIP_RANDOM = 1,
  PTEXTURE_FLIP_ALWAYS = 2,
};

enum prtDistributionType_t : std::int32_t {
  PDIST_RECT = 0,
  PDIST_CYLINDER = 1,
  PDIST_SPHERE = 2,
  PDIST_RECT_SURFACE = 3,
  PDIST_CYLINDER_SURFACE = 4,
  PDIST_SPHERE_SURFACE = 5,
  PDIST_MESH_SURFACE = 6,
};

enum prtDirectionType_t : std::int32_t {
  PDIR_CONE = 0,
  PDIR_OUTWARD = 1,
  PDIR_SPEED = 2,
};

enum prtAnimationType_t : std::int32_t {
  PANIM_TYPE_CYLE_RATE = 0,
  PANIM_TYPE_SINGLE_CYCLE_RATE = 1,
  PANIM_TYPE_SINGLE_CYCLE = 2,
};

enum prtSortType_t : std::int32_t {
  PSORT_TYPE_NONE = 0,
  PSORT_TYPE_NEWEST_TO_OLDEST = 1,
  PSORT_TYPE_OLDEST_TO_NEWEST = 2,
};

enum prtLightingType_t : std::int32_t {
  PLIGHTING_TYPE_OFF = 0,
  PLIGHTING_TYPE_AVERAGE = 1,
  PLIGHTING_TYPE_FULL = 2,
};

enum prtCustomPthType_t : std::int32_t {
  PPATH_STANDARD = 0,
  PPATH_HELIX = 1,
  PPATH_FLIES = 2,
  PPATH_ORBIT = 3,
  PPATH_DRIP = 4,
};

class idParticleStage {
public:
  idParticleStage();
  void operator=(const idParticleStage& src);
  void SetMaterial(const idMaterial* material);
  void SetStaticMesh(const idStaticParticleModelData* staticMesh);
  const idDrawVert* GetStaticVerts() const;
  int GetNumStaticVerts() const;
  int NumVertsPerParticle() const;
  void Default(const idLookupTable* tables);

  struct prtDistribution_t {
    prtDistributionType_t type;
    idParticleParm size[3];
    bool random;
    std::uint8_t pad65[3];
  };
  struct prtDirection_t {
    prtDirectionType_t type;
    float parms[4];
    float angleOffsetStart[2];
    float angleOffsetRange[2];
    idMat3 coneAxis;
  };
  struct prtSpeed_t { idParticleParm speed[3]; };
  struct prtAcceleration_t {
    idParticleParm acceleration[3];
    bool world;
    std::uint8_t pad61[3];
  };
  struct prtGravity_t {
    idParticleParm gravity;
    bool world;
    std::uint8_t pad21[3];
  };
  struct prtFriction_t { idParticleParm friction[3]; };
  struct prtOffset_t { idParticleParm offset[3]; };
  struct prtSpawnLocation_t { idParticleParm spawnLocation[3]; };
  struct prtCustomPath_t {
    prtCustomPthType_t type;
    bool useRandomSpawn;
    std::uint8_t pad5[3];
    idParticleParm parms[5];
  };
  struct prtPivot_t { idVec2 pivotOffset; };
  struct prtLODParms_t {
    float sizeScale;
    float lerpAmount;
    std::int16_t radius;
    std::int16_t totalParticles;
  };
  struct prtStageProperties_t {
    const idMaterial* material;
    std::int16_t totalParticles;
    std::int16_t cycles;
    std::int32_t diversity;
    idParticleParm particleLife;
    float timeOffset;
    idParticleParm deadTime;
    float useSysTime;
    float spawnBunching;
    float emissionTime;
    prtTextureFlipMode_t textureFlipS;
    prtTextureFlipMode_t textureFlipT;
    idParticleParm windBias;
    prtSortType_t sortType;
    float boundsExpansion;
    bool randomOnCycle;
    std::uint8_t pad109[3];
    prtLightingType_t lightingType;
    std::uint8_t pad116[4];
  };
  struct prtOrientation_t {
    prtOrientationType_t type;
    std::int16_t numTrails;
    std::uint16_t pad6;
    float segmentLength;
    float viewFade;
    bool orientToVelOnly;
    bool world;
    bool aimedSafeQuad;
    std::uint8_t pad19;
    float aimedSafeQuadAlign;
    idParticleParm depthOffset;
  };
  struct prtColorAttributes_t {
    idParticleParm baseColor[4];
    idVec4 fadeColor;
    float fadeInFraction;
    float fadeOutFraction;
    float fadeIndexFraction;
    float softParticleAlphaScale;
    idParticleParm brightness;
    bool useGlobalShadows;
    std::uint8_t pad133[3];
    float minShadowVal;
    float entityColorBlendVal;
  };
  struct prtRotation_t {
    idParticleParm rotation[3];
    bool allowRotDirOverride;
    std::uint8_t pad61[3];
  };
  struct prtInitialRotation_t { idParticleParm initialAngle[3]; };
  struct prtSize_t {
    idParticleParm size[3];
    idParticleParm aspectRatio;
  };
  struct prtTexAnimation_t {
    prtAnimationType_t type;
    std::uint16_t numColumns;
    std::uint16_t numRows;
    idParticleParm rate;
    std::int16_t startFrame;
    bool useRandomRow;
    bool useFrameBlending;
  };
  struct prtGenericParms_t { idParticleParm genericParm[4]; };

  const idStaticParticleModelData* staticData;
  std::int32_t cycleMsec;
  float maxParticleLife;
  float maxDeadTime;
  float bunchTime;
  idBounds bounds;
  idVec4 atlasScaleBias;
  union {
    std::uint8_t stageFlags;
    struct {
      bool alphaBlended : 1;
      bool isTransparencySorted : 1;
      bool usesTransSortAtlas : 1;
      bool hasEmissivePass : 1;
      bool hidden : 1;
    };
  };
  std::uint8_t pad65[7];
  prtStageProperties_t systemProperties;
  prtDistribution_t distribution;
  prtOrientation_t orientation;
  prtDirection_t direction;
  prtSpeed_t speed;
  prtAcceleration_t acceleration;
  prtGravity_t gravity;
  prtFriction_t friction;
  prtOffset_t offset;
  prtSpawnLocation_t spawnLocation;
  prtColorAttributes_t colorAttributes;
  prtRotation_t rotation;
  prtInitialRotation_t initialRotation;
  prtPivot_t pivot;
  prtSize_t size;
  prtTexAnimation_t texAnimation;
  prtCustomPath_t customPath;
  prtGenericParms_t genericParm;
  prtLODParms_t lodParms;
  std::uint8_t tailPad[4];

private:
  friend class idDeclParticle;
  friend void ValidateParticleStageBounds(idParticleStage& stage,
                                          const idLookupTable* tables,
                                          const prtLocation_t* locations,
                                          int numLocations);
  void CalculateBounds(const idLookupTable* tables,
                       const prtLocation_t* locations, int numLocations);
};

struct dustRandomParm_t { float min; float max; };
struct dustParameters_t {
  float gravity;
  float dampning;
  float particleViewTilt;
  float fadeintime;
  float fadeouttime;
  float heightScaleOffset;
  float heightScaleRange;
  float heightScaleAmount;
  float distanceBeforeFadeout;
  float movementAnimation;
  float maxAnimationSpeed;
  dustRandomParm_t rotationSpeed;
  float energyRotationSpeed;
  dustRandomParm_t maxVelocity;
  dustRandomParm_t influenceForce;
  dustRandomParm_t energySizeIncrase;
};

struct dustParticle_t {
  idVec3 initial;
  float baseAlpha;
  float influenceForce;
  float maxVelocity;
  float energySizeIncrase;
  float rotationSpeed;
  float rotationDirection;
  idVec3 origin;
  idVec3 velocity;
  float rotation;
  std::int32_t state;
  float fade;
  float energy;
  float lifeTime;
};

struct particleGen_t {
  std::int32_t index;
  float cycleAge;
  float totalAge;
  float particleLife;
  float frac;
  float parmVal;
  idRandom2 random;
  idRandom2 originalRandom;
  idVec4 vertColor;
};

struct deferredDustGenParms_t {
  const visibleInfluenceSpheres_t* visibleInfluenceSpheres;
  const idParticleStage* stage;
  const idLookupTable* tables;
  idVec3 origin;
  idVec3 viewOrg;
  idVec3 viewFwd;
  idVec3 viewLeft;
  idVec3 viewUp;
  dustParameters_t parms;
  std::int32_t numParticles;
  float transSortWidth;
  float transSortHeight;
  float timeDelta;
  std::uint32_t pad180;
  dustParticle_t* particles;
  idTransparencyVert* verts;
  float* depths;
};

void DustGenJob(const deferredDustGenParms_t* parms);

static_assert(sizeof(idRandom2) == 4);
static_assert(sizeof(idParticleParm) == 20);
static_assert(offsetof(idParticleParm, calcType) == 16);
static_assert(sizeof(idParticleStage::prtStageProperties_t) == 120);
static_assert(sizeof(idParticleStage::prtDistribution_t) == 68);
static_assert(sizeof(idParticleStage::prtOrientation_t) == 44);
static_assert(sizeof(idParticleStage::prtDirection_t) == 72);
static_assert(sizeof(idParticleStage::prtCustomPath_t) == 108);
static_assert(sizeof(idParticleStage::prtPivot_t) == 8);
static_assert(sizeof(idParticleStage::prtLODParms_t) == 12);
static_assert(sizeof(idParticleStage::prtColorAttributes_t) == 144);
static_assert(sizeof(idParticleStage) == 1296);
static_assert(offsetof(idParticleStage, staticData) == 0);
static_assert(offsetof(idParticleStage, cycleMsec) == 8);
static_assert(offsetof(idParticleStage, bounds) == 24);
static_assert(offsetof(idParticleStage, atlasScaleBias) == 48);
static_assert(offsetof(idParticleStage, stageFlags) == 64);
static_assert(offsetof(idParticleStage, systemProperties) == 72);
static_assert(offsetof(idParticleStage, distribution) == 192);
static_assert(offsetof(idParticleStage, orientation) == 260);
static_assert(offsetof(idParticleStage, direction) == 304);
static_assert(offsetof(idParticleStage, speed) == 376);
static_assert(offsetof(idParticleStage, acceleration) == 436);
static_assert(offsetof(idParticleStage, gravity) == 500);
static_assert(offsetof(idParticleStage, friction) == 524);
static_assert(offsetof(idParticleStage, offset) == 584);
static_assert(offsetof(idParticleStage, spawnLocation) == 644);
static_assert(offsetof(idParticleStage, colorAttributes) == 704);
static_assert(offsetof(idParticleStage, rotation) == 848);
static_assert(offsetof(idParticleStage, initialRotation) == 912);
static_assert(offsetof(idParticleStage, pivot) == 972);
static_assert(offsetof(idParticleStage, size) == 980);
static_assert(offsetof(idParticleStage, texAnimation) == 1060);
static_assert(offsetof(idParticleStage, customPath) == 1092);
static_assert(offsetof(idParticleStage, genericParm) == 1200);
static_assert(offsetof(idParticleStage, lodParms) == 1280);
static_assert(sizeof(dustParameters_t) == 80);
static_assert(sizeof(dustParticle_t) == 80);
static_assert(offsetof(dustParticle_t, origin) == 36);
static_assert(offsetof(dustParticle_t, velocity) == 48);
static_assert(offsetof(dustParticle_t, state) == 64);
static_assert(sizeof(particleGen_t) == 48);
static_assert(sizeof(deferredDustGenParms_t) == 208);
static_assert(offsetof(deferredDustGenParms_t, parms) == 84);
static_assert(offsetof(deferredDustGenParms_t, particles) == 184);
