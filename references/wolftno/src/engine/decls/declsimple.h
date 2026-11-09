#pragma once

#include "decl.h"
#include "declaf.h"
#include "declbreakable.h"
#include "declentitydef.h"
#include "declflare.h"
#include "declfx.h"
#include "declholder.h"
#include "declloadingscreen.h"
#include "declmapinfo.h"
#include "declmaterialdef.h"
#include "declreachik.h"
#include "declrollbones.h"
#include "declskins.h"
#include "declswfresource.h"
#include "decltypeinfograph.h"
#include "declwalkik.h"
#include "declvmtrpreload.h"
#include "shared/idlib/math/vector.h"

class idMaterial;
class idDeclTable;
class idDeclParticle;
class idSoundShader;
class idDeclFX;

template <typename T> struct idRange {
  T minRange{};
  T maxRange{};
};

class idDeclElectricBolt final : public idDeclTypeInfo {
public:
  struct branchOverride_t {
    const idMaterial* material;
    int frames;
    int width;
  };

  idDeclElectricBolt();
  idDeclInfo* GetDeclInfo() const override;

  const idMaterial* material;
  bool applyGradient;
  int revealTime;
  const idDeclTable* brightnessTable;
  float startWidth;
  float endWidth;
  float maxDeviation;
  idVec4 color;
  float brightness;
  int subdivisions;
  float branchProbability;
  int maxBranchLevels;
  float branchStartWidth;
  float branchEndWidth;
  idVec2 branchLength;
  float maxBranchAngle;
  int branchSubdivisions;
  float maxBranchDeviation;
  const idDeclTable* jitterTable;
  const idDeclTable* jitterFalloffTable;
  float jitterDecay;
  float jitterSpeed;
  idVec2 jitterLeftMag;
  idVec2 jitterUpMag;
  bool useBranchOverride;
  branchOverride_t branchOverride;

  static idDeclInfoTemplate<idDeclElectricBolt> resourceList;
};

class idDeclImpactParticle final : public idDeclTypeInfo {
public:
  idDeclImpactParticle();
  const idDeclParticle* GetImpactParticleForMaterial(int materialID) const;
  idDeclInfo* GetDeclInfo() const override;

  const idDeclParticle* defaultPrt;
  const idDeclParticle* metalPrt;
  const idDeclParticle* stonePrt;
  const idDeclParticle* fleshPrt;
  const idDeclParticle* woodPrt;
  const idDeclParticle* cardboardPrt;
  const idDeclParticle* liquidPrt;
  const idDeclParticle* glassPrt;
  const idDeclParticle* plasticPrt;
  const idDeclParticle* asphaltPrt;
  const idDeclParticle* dirtPrt;
  const idDeclParticle* concretePrt;
  const idDeclParticle* foliagePrt;
  const idDeclParticle* linoleumPrt;
  const idDeclParticle* fabricPrt;
  const idDeclParticle* rubberPrt;
  const idDeclParticle* rockPrt;
  const idDeclParticle* steamPipePrt;
  const idDeclParticle* waterPipePrt;
  const idDeclParticle* armorPrt;
  const idDeclParticle* sludgePrt;
  const idDeclParticle* shieldPrt;
  const idDeclParticle* bloodPoolPrt;
  const idDeclParticle* ashPrt;
  const idDeclParticle* mutantFleshPrt;
  const idDeclParticle* thickPaddingPrt;
  const idDeclParticle* porcelainPrt;
  const idDeclParticle* plasterPrt;
  const idDeclParticle* electronicsPrt;
  const idDeclParticle* oilPipePrt;
  const idDeclParticle* panzerMetalPrt;
  const idDeclParticle* breakableArmorPrt;
  const idDeclParticle* electricShieldPrt;

  static idDeclInfoTemplate<idDeclImpactParticle> resourceList;
};

class idDeclImpactSound final : public idDeclTypeInfo {
public:
  idDeclImpactSound();
  const idSoundShader* GetImpactSoundForMaterial(int materialID) const;
  idDeclInfo* GetDeclInfo() const override;

  const idSoundShader* defaultSound;
  const idSoundShader* metalSound;
  const idSoundShader* stoneSound;
  const idSoundShader* fleshSound;
  const idSoundShader* woodSound;
  const idSoundShader* cardboardSound;
  const idSoundShader* liquidSound;
  const idSoundShader* glassSound;
  const idSoundShader* plasticSound;
  const idSoundShader* asphaltSound;
  const idSoundShader* dirtSound;
  const idSoundShader* concreteSound;
  const idSoundShader* foliageSound;
  const idSoundShader* linoleumSound;
  const idSoundShader* fabricSound;
  const idSoundShader* rubberSound;
  const idSoundShader* rockSound;
  const idSoundShader* steamPipeSound;
  const idSoundShader* waterPipeSound;
  const idSoundShader* armorSound;
  const idSoundShader* sludgeSound;
  const idSoundShader* impNestSound;
  const idSoundShader* hollowMetalSound;
  const idSoundShader* sheetMetalSound;
  const idSoundShader* ventMetalSound;
  const idSoundShader* fleshPlayerSound;
  const idSoundShader* bloodPoolSound;
  const idSoundShader* ashSound;
  const idSoundShader* mutantFleshSound;
  const idSoundShader* thickPaddingSound;
  const idSoundShader* playerAPCMetalSound;
  const idSoundShader* ricketyMetalSound;
  const idSoundShader* ricketyWoodSound;
  const idSoundShader* porcelainSound;
  const idSoundShader* plasterSound;
  const idSoundShader* electronicsSound;
  const idSoundShader* oilPipeSound;
  const idSoundShader* panzerMetalSound;
  const idSoundShader* breakableArmorSound;
  const idSoundShader* electricShieldSound;
  int soundInterval;

  static idDeclInfoTemplate<idDeclImpactSound> resourceList;
};

class idDeclStaticImage final : public idDeclTypeInfo {
public:
  idDeclStaticImage();
  idDeclInfo* GetDeclInfo() const override;
  idAtomicString fileName;
  static idDeclInfoTemplate<idDeclStaticImage> resourceList;
};

class idDeclGlass final : public idDeclTypeInfo {
public:
  idDeclGlass();
  idDeclInfo* GetDeclInfo() const override;
  const idDeclFX* fx;
  int impact;
  int destroy;
  const idDeclParticle* impactParticle;
  const idDeclParticle* destroyParticle;
  const idDeclParticle* impactFillParticle;
  const idDeclParticle* destroyFillParticle;
  const idSoundShader* impactSound;
  const idSoundShader* destroySound;
  const idMaterial* impactMaterial;
  const idMaterial* decalMaterial;
  idRange<float> decalSize;
  idRange<int> decalClipAlpha;
  bool playerClip;
  bool monsterClip;
  bool floor;
  bool unbreakable;
  static idDeclInfoTemplate<idDeclGlass> resourceList;
};

class idDeclRibbon final : public idDeclTypeInfo {
public:
  struct helix_t {
    float rotationScale;
    float radius;
    float velocity;
    idVec2 distortion0;
    idVec2 distortion1;
  };
  struct turbulence_t {
    float frequency;
    float magnitude;
    float velocity;
    idVec4 magic;
  };
  idDeclRibbon();
  idDeclInfo* GetDeclInfo() const override;
  const idMaterial* material;
  int duration;
  float maxLength;
  float fadeInFraction;
  float fadeOutFraction;
  idVec3 gravity;
  idVec3 velocity;
  float startWidth;
  float endWidth;
  idVec4 color;
  bool viewOriented;
  bool reorientNodes;
  bool textureRepeat;
  bool textureRepeatStretch;
  float textureTMin;
  float textureTMax;
  float nodeSubdivision;
  float minNodeDistance;
  helix_t helix;
  turbulence_t turbulence;
  static idDeclInfoTemplate<idDeclRibbon> resourceList;
};

class idDeclProductionFilter final : public idDeclTypeInfo {
public:
  idDeclProductionFilter();
  ~idDeclProductionFilter() override;
  void Parse(idParser& source) override;
  idDeclInfo* GetDeclInfo() const override;
  idList<idStr> entityFilter;
  idList<idStr> entityRenderModelFilter;
  idList<idStr> layerListFilter;
  static idDeclInfoTemplate<idDeclProductionFilter> resourceList;
};

struct modelLODSubModel_t {
  idStr model;
  bool megatexturedModel = false;
};

struct modelLODLevel_t {
  idList<modelLODSubModel_t> models;
  float distance = 0;
  int dimShadows = 0;
};

class idDeclModelLOD final : public idDeclTypeInfo {
public:
  idDeclModelLOD();
  void FreeData() override;
  idDeclInfo* GetDeclInfo() const override;
  float fade;
  idList<modelLODLevel_t> LODs;
  static idDeclInfoTemplate<idDeclModelLOD> resourceList;
};
