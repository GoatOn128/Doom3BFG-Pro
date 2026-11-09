#pragma once

#include "tungsten/game/clientgame/presentable/presentableanimatedphysics.h"

#include <string>

struct netBoolEvent_t {
  void Serialize(idSerializer* serializer);
  int count{0};
  int lastCount{0};
};

struct idCodeResourceParticleParm {
  explicit idCodeResourceParticleParm(const char* value):name(value!=nullptr?value:""){}
  std::string name;
};

class idPresentableParticleEmitter : public idPresentable {
public:
  explicit idPresentableParticleEmitter(const std::string& particleName={},int entityNumber=-1,const std::string& renderModel={});
  ~idPresentableParticleEmitter()override;
  int GetType()const;
  void InitParticleDefaults();
  void ClientThink(int currentTime,float fraction,bool predict);
  void TriggerShow();
  void TriggerHide();
  void SetParticle(const std::string& particleName);
  void PostSerializeRead(bool firstClientFrame);
  void Serialize(idSerializer* serializer);

  void SetUseGlobalShadows(bool value){useGlobalShadows=value;}
  void SetUseSmokeSystem(bool value){useSmokeSystem=value;}
  void SetFadeTimes(float inSeconds,float outSeconds){fadeIn=inSeconds;fadeOut=outSeconds;}
  void SetAlphaScale(float value){alphaScale=value;}
  void SetDistributionScale(const idVec3& value){distributionScale=value;}
  void SetVelocity(const idVec3& value){velocity=value;}
  void SetColor(const idVec4& value){colorv4=value;}
  void SetSmokeSystemRate(int value){smokeSystemRate=value;}
  const std::string& GetParticle()const{return particleSystem;}
  int GetActivatedCount()const{return activated.count;}
  int GetDeactivatedCount()const{return deactivated.count;}
  int GetEmissionCount()const{return emissionCount;}
  const idVec3& GetVelocity()const{return velocity;}

private:
  netBoolEvent_t activated{};
  netBoolEvent_t deactivated{};
  std::string particleSystem;
  bool useGlobalShadows{false};
  bool useSmokeSystem{false};
  float fadeIn{0.5F};
  float fadeOut{0.5F};
  float alphaScale{1.0F};
  idVec3 distributionScale{1,1,1};
  idVec3 velocity{};
  idVec4 colorv4{1,1,1,1};
  int smokeSystemRate{1};
  int fadeEndTime{0};
  bool hideWhenFadeEnds{false};
  int emissionCount{0};
};

extern idCodeResourceParticleParm rpParticleAlphaScale;
extern idCodeResourceParticleParm rpParticleScale;
extern idCodeResourceParticleParm rpParticleVel;
