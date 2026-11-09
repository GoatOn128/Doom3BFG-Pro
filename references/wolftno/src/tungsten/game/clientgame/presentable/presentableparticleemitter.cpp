#include "tungsten/game/clientgame/presentable/presentableparticleemitter.h"

#include <algorithm>

// DIA: idPresentableParticleEmitter::GetType
// DIA: idPresentableParticleEmitter::`scalar deleting destructor'
// DIA: idPresentableParticleEmitter::InitParticleDefaults
// DIA: idPresentableParticleEmitter::ClientThink
// DIA: idPresentableParticleEmitter::TriggerShow
// DIA: idPresentableParticleEmitter::TriggerHide
// DIA: idPresentableParticleEmitter::SetParticle
// DIA: idBitMsg::ReadQuantizedVector<idVec3,32768,16>
// DIA: netBoolEvent_t::Serialize
// DIA: idPresentableParticleEmitter::idPresentableParticleEmitter
// DIA: idPresentableParticleEmitter::PostSerializeRead
// DIA: idBitMsg::WriteQuantizedVector<idVec3,32768,16>
// DIA: idPresentableParticleEmitter::Serialize
// DIA: `dynamic initializer for 'rpParticleAlphaScale''
// DIA: `dynamic initializer for 'rpParticleScale''
// DIA: `dynamic initializer for 'rpParticleVel''

int idPresentableParticleEmitter::GetType()const{return 27;}
idPresentableParticleEmitter::~idPresentableParticleEmitter()=default;

void idPresentableParticleEmitter::InitParticleDefaults(){
  particleSystem.clear();
  useGlobalShadows=false;
  useSmokeSystem=false;
  fadeIn=0.5F;
  fadeOut=0.5F;
  alphaScale=1.0F;
  smokeSystemRate=1;
  distributionScale={1,1,1};
  velocity={};
  colorv4={1,1,1,1};
  fadeEndTime=0;
  hideWhenFadeEnds=false;
}

void idPresentableParticleEmitter::ClientThink(int currentTime,float fraction,bool predict){
  (void)predict;
  idPresentable::Interpolate(fraction);
  idPresentable::ClientThink();
  if(fadeEndTime>0&&currentTime>=fadeEndTime){
    if(hideWhenFadeEnds)Hide();else Show();
    fadeEndTime=0;
  }
  if(useSmokeSystem&&(smokeSystemRate==0||currentTime%std::max(1,smokeSystemRate)==0)&&!particleSystem.empty())++emissionCount;
}

void idPresentableParticleEmitter::TriggerShow(){
  activated.count=(activated.count+1)%7;
  if(particleSystem.empty())return;
  if(!useSmokeSystem){
    Show();
    hideWhenFadeEnds=false;
    fadeEndTime=fadeIn>0.0F?static_cast<int>(fadeIn*1000.0F):0;
    SetParm("particleScale",distributionScale);
    SetParm("particleVelocity",idVec3{});
    SetParm("particleAlphaScale",alphaScale!=0.0F?1.0F/alphaScale:0.0F);
    Present();
  }
}

void idPresentableParticleEmitter::TriggerHide(){
  deactivated.count=(deactivated.count+1)%7;
  if(useSmokeSystem)return;
  if(fadeOut==0.0F)Hide();
  else{hideWhenFadeEnds=true;fadeEndTime=static_cast<int>(fadeOut*1000.0F);}
}

void idPresentableParticleEmitter::SetParticle(const std::string& particleName){
  if(particleName.empty()||GetRenderModel_Particle().empty())return;
  particleSystem=particleName;
  SetParm("particleColor",idVec3{colorv4.x,colorv4.y,colorv4.z});
  SetParm("particleScale",distributionScale);
  SetParm("particleVelocity",idVec3{});
  SetParm("particleAlphaScale",alphaScale!=0.0F?1.0F/alphaScale:0.0F);
  SetParm("noShadows",useGlobalShadows?0.0F:1.0F);
  Present();
}

template<> idVec3 idBitMsg::ReadQuantizedVector<idVec3,32768,16>(){
  return ReadQuantizedVector<idVec3,32768,17>();
}

void netBoolEvent_t::Serialize(idSerializer* serializer){
  if(serializer==nullptr)return;
  serializer->SerializeInt(count);
  count=((count%7)+7)%7;
}

idPresentableParticleEmitter::idPresentableParticleEmitter(const std::string& particleName,int number,const std::string& model){
  InitParticleDefaults();
  SetEntityNumber(number);
  SetRenderModel(model);
  particleSystem=particleName;
}

void idPresentableParticleEmitter::PostSerializeRead(bool firstClientFrame){
  (void)firstClientFrame;
  idPresentable::PostSerializeRead();
  if(activated.count!=activated.lastCount){activated.lastCount=activated.count;TriggerShow();}
  if(deactivated.count!=deactivated.lastCount){deactivated.lastCount=deactivated.count;TriggerHide();}
}

template<> void idBitMsg::WriteQuantizedVector<idVec3,32768,16>(const idVec3& value){
  WriteQuantizedVector<idVec3,32768,17>(value);
}

void idPresentableParticleEmitter::Serialize(idSerializer* serializer){
  if(serializer==nullptr)return;
  activated.Serialize(serializer);
  deactivated.Serialize(serializer);
  serializer->SerializeBool(useGlobalShadows);
  serializer->SerializeBool(useSmokeSystem);
  serializer->SerializeFloat(fadeIn);
  serializer->SerializeFloat(fadeOut);
  serializer->SerializeFloat(alphaScale);
  serializer->SerializeVec3(distributionScale);
  serializer->SerializeVec3(velocity);
  serializer->SerializeVec4(colorv4);
  serializer->SerializeInt(smokeSystemRate);
  idAtomicString particle{particleSystem};
  serializer->SerializeString(&particle);
  if(!serializer->writing&&particleSystem!=particle.value){
    if(GetRenderModel_Particle().empty())particleSystem=particle.value;
    else SetParticle(particle.value);
  }
}

idCodeResourceParticleParm rpParticleAlphaScale("rpParticleAlphaScale");
idCodeResourceParticleParm rpParticleScale("rpParticleScale");
idCodeResourceParticleParm rpParticleVel("rpParticleVel");
