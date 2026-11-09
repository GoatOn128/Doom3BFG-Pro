#pragma once

#include "tungsten/game/clientgame/presentable/entityinfluencetrail.h"
#include "tungsten/game/clientgame/presentable/presentable.h"

#include <string>
#include <vector>

class idAI2;
class idAnimStack;
class idAnimator_Pain;
class idDeclFX;
class idInventoryCollection;
class idRenderModel;

class idPresentableActor {
public:
  void* GetActorModifierManager();
  void SetActorModifierManager(void* value) { modifierManager=value; }
private: void* modifierManager{nullptr};
};

class idPresentableAI : public idPresentable {
public:
  idPresentableAI();
  idPresentableAI(idAI2* entity, idRenderModel* renderModel,
                  idAnimStack* animStack, idAnimator_Pain* painAnimator,
                  idEntityInfluenceTrail* influence, int entityNumber,
                  const idDeclFX* fxDeclaration, bool useSphereModel);
  ~idPresentableAI() override;
  void InternalSplatterBlood(const idVec3& position,float amount);
  void InternalPlayAdditivePain(const std::string& animation,float weight);
  bool InternalLootBody(int playerIndex);
  void Interpolate(float fraction);
  void UpdateInfluenceTrail(const idVec3& position);
  bool ShouldSerializeRenderModelParms()const;
  void SetDefaults();
  std::vector<std::uint8_t> Serialize()const;
  void SplatterBlood(const idVec3& position,float amount);
  void PlayAdditivePain(const std::string& animation,float weight);
  bool LootBody(int playerIndex);
  void ClientThink();
  bool PredictHitScanHit(const idVec3& start,const idVec3& direction,float range)const;

  int GetBloodSplatterCount()const{return bloodSplatterCount;} const std::string& GetPainAnimation()const{return painAnimation;} bool IsLooted()const{return looted;} int InfluenceTrailSize()const{return static_cast<int>(influenceTrail.size());}
  idAI2* GetAIEntityForTest() const { return aiEntity; }
  idRenderModel* GetSourceRenderModelForTest() const { return sourceRenderModel; }
  idAnimStack* GetAnimStackForTest() const { return sourceAnimStack; }
  idAnimator_Pain* GetPainAnimatorForTest() const { return sourcePainAnimator; }
  const idEntityInfluenceTrail& GetInfluenceForTest() const { return influence; }
  const idInventoryCollection* GetInventoryForTest() const { return inventory; }
  const idDeclFX* GetFXDeclarationForTest() const { return fxDeclaration; }
  bool UsesSphereModelForTest() const { return useSphereModel; }
private:
  std::vector<idVec3> influenceTrail; std::string painAnimation; float painWeight{0}; int bloodSplatterCount{0}; int lootedBy{-1}; bool looted{false};
  idAI2* aiEntity{nullptr};
  idRenderModel* sourceRenderModel{nullptr};
  idAnimStack* sourceAnimStack{nullptr};
  idAnimator_Pain* sourcePainAnimator{nullptr};
  idEntityInfluenceTrail influence{};
  const idInventoryCollection* inventory{nullptr};
  const idDeclFX* fxDeclaration{nullptr};
  bool useSphereModel{false};
};

extern idCVar presentable_ai_serialize_render_parms;
extern idCVar ai_influenceForce;
