#pragma once

#include "engine/decls/decl.h"
#include "engine/models/particles/jobs/dustgen.h"
#include "engine/models/particles/jobs/particlestageimposter.h"
#include "shared/idlib/containers/list.h"
#include "shared/idlib/text/atomicstring.h"

#include <cstddef>

class idDeclTable;
class idFile;

struct idPartImposterData {
  int sumLocations;
  prtLocation_t* locData;
};

struct particleParmDesc_t {
  const char* name;
  int count;
  const char* desc;
};

extern const particleParmDesc_t particleDistributionDesc[7];
extern const particleParmDesc_t particleDirectionDesc[3];
extern const particleParmDesc_t particleOrientationDesc[7];
extern const particleParmDesc_t particleCustomDesc[5];
extern const particleParmDesc_t particleTexFlipTypeDesc[3];
extern const particleParmDesc_t particleAnimationTypeDesc[3];
extern const particleParmDesc_t particleSortTypeDesc[3];
extern const particleParmDesc_t particleLightingDesc[3];

int ParticleDistributionNameToType(const idStr& text);
int ParticleDirectionNameToType(const idStr& text);
int ParticleOrientationNameToType(const idStr& text);
int ParticleCustomNameToType(const idStr& text);
int ParticleTextureFlipNameToType(const idStr& text);
int ParticleAnimationNameToType(const idStr& text);
int ParticleSortNameToType(const idStr& text);
int ParticleLightingToType(const idStr& text);
void WriteBounds(idFile* file, const idStr& variableName,
                 const idBounds& bounds);
void ParseBounds(idParser& source, idBounds& bounds);

class idDeclParticle final : public idDecl {
public:
  idDeclParticle();
  ~idDeclParticle() override;

  idParticleStageImposter* CreateNewDuplicateSpawn();
  idParticleStage* CreateNewStage();
  const char* DefaultDefinition() const override;
  idParticleStage* FindStageByName(const char* stageName);
  int CalcLodForDistance(float distanceSquared) const;
  const idLookupTable* GetTables() const;
  idDeclInfo* GetDeclInfo() const override;
  bool RebuildTextSource() override;
  void LogMissingDecl() override;
  virtual void LogMissingDecl() const;
  void Parse(idParser& source) override;
  void FreeData() override;

  idList<idParticleStage*, 79> stages;
  idList<idAtomicString, 79> stageNames;
  idList<idAtomicString, 79> stageInheritNames;
  idList<const idDeclTable*, 79> tableDecls;
  idList<idLookupTable, 79> tables;
  idList<idParticleStageImposter*, 79> imposters;
  idList<idPartImposterData*, 79> imposterData;
  idBounds bounds{};
  int maxSystemDuration{};
  int lodRadius{};
  bool usesWind{};
  bool mergeSurfaces{};

  static idDeclInfoTemplate<idDeclParticle> resourceList;

private:
  friend bool ValidateDeclParticleBinaryRoundTrip();
  void CalcBatchingGroups();
  void CreateDuplicateData();
  bool LoadGeneratedFile(const idStr& generatedName, int sourceTimestamp);
  idParticleStageImposter* ParseDuplicateImposter(idParser& source,
                                                  const idStr& duplicateName);
  void ParseParms(idParser& source, float* parameters, int maxParameters);
  idParticleStage* ParseParticleStage(idParser& source,
                                      const idStr& stageName,
                                      idStr& inheritedStageName);
  void SaveGeneratedFile(const idStr& generatedName, int sourceTimestamp);
  void UpdateBounds();
  void UpdateMaxSystemDuration();
  void UpdateWindUsage();
  void WriteStage(idFile* file, const idParticleStage* stage,
                  const char* stageName,
                  const idParticleStage* inheritedStage,
                  const char* inheritedStageName);
  void WriteStageImposter(idFile* file,
                          const idParticleStageImposter* imposter);
};

static_assert(sizeof(idPartImposterData) == 16,
              "idPartImposterData ABI drift");
static_assert(sizeof(idDeclParticle) == 288,
              "idDeclParticle ABI drift");
static_assert(offsetof(idDeclParticle, stages) == 80,
              "idDeclParticle stages offset drift");
static_assert(offsetof(idDeclParticle, tables) == 176,
              "idDeclParticle tables offset drift");
static_assert(offsetof(idDeclParticle, bounds) == 248,
              "idDeclParticle bounds offset drift");
static_assert(offsetof(idDeclParticle, maxSystemDuration) == 272,
              "idDeclParticle duration offset drift");
static_assert(offsetof(idDeclParticle, lodRadius) == 276,
              "idDeclParticle lod radius offset drift");
