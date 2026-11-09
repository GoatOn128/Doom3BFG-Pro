#include "tungsten/game/clientgame/presentable/presentablebreakable.h"

#include <algorithm>
#include <cstring>

// Complete recovered function inventory. Compiler-generated destruction
// funclets are supplied by the out-of-line virtual destructor below.
// DIA: idPresentableDamageable::SetInitProperties
// DIA: idPresentableDamageable::StartExplosions
// DIA: idPresentableDamageable::ActivateAllNow
// DIA: idPresentableDamageable::GetType
// DIA: idPresentableDamageable::idPresentableDamageable
// DIA: idPresentableDamageable::Interpolate
// DIA: idPresentableDamageable::GetWorldTransform
// DIA: idPresentableDamageable::AddRayImpact
// DIA: idPresentableDamageable::`scalar deleting destructor'
// DIA: `idPresentableDamageable::`scalar deleting destructor''::`1'::dtor$0
// DIA: `idPresentableDamageable::`scalar deleting destructor''::`1'::dtor$1
// DIA: idPresentableDamageable::Serialize

namespace {
template <typename T>
void AppendBytes(std::vector<std::uint8_t>& destination,const T& value){
  const auto* first=reinterpret_cast<const std::uint8_t*>(&value);
  destination.insert(destination.end(),first,first+sizeof(value));
}

idMat3 IdentityAxis(){
  idMat3 value{};
  value[0]={1.0F,0.0F,0.0F};
  value[1]={0.0F,1.0F,0.0F};
  value[2]={0.0F,0.0F,1.0F};
  return value;
}
}

void idPresentableDamageable::SetInitProperties(const std::string& impactTableName,float minBounceVelocity){
  pieceMinBounceVelocity=std::max(0.0F,minBounceVelocity);
  impactTable=impactTableName;
}

void idPresentableDamageable::StartExplosions(){
  explosionFrame=(explosionFrame+1)%16;
  currentExplosionFrame=explosionFrame;
}

void idPresentableDamageable::ActivateAllNow(){
  activateAllNowFrame=(activateAllNowFrame+1)%16;
  currentActivateAllNowFrame=activateAllNowFrame;
}

int idPresentableDamageable::GetType()const{return 6;}

idPresentableDamageable::idPresentableDamageable()=default;
idPresentableDamageable::~idPresentableDamageable()=default;

void idPresentableDamageable::Interpolate(int currentTime,float fraction){
  (void)currentTime;
  idPresentable::Interpolate(fraction);
  if(GetRenderModel_DiscreteAnimation().empty())return;
  if(currentExplosionFrame!=explosionFrame){
    SetLocalClipModelContents(0);
    currentExplosionFrame=explosionFrame;
  }
  if(currentActivateAllNowFrame!=activateAllNowFrame){
    SetLocalClipModelContents(0);
    currentActivateAllNowFrame=activateAllNowFrame;
  }
  if(!IsHidden())++simulationCount;
}

void idPresentableDamageable::GetWorldTransform(idVec3& outOrigin,idMat3& outAxis)const{
  if(!GetRenderModel_DiscreteAnimation().empty()){
    outOrigin={};
    outAxis=IdentityAxis();
    return;
  }
  idPresentable::GetWorldTransform(outOrigin,outAxis);
}

void idPresentableDamageable::AddRayImpact(const idVec3& start,const idVec3& end){
  rayImpactBuffer[static_cast<std::size_t>(rayImpactBufferPos)]={start,end};
  rayImpactBufferPos=(rayImpactBufferPos+1)%static_cast<int>(rayImpactBuffer.size());
}

std::vector<std::uint8_t> idPresentableDamageable::Serialize()const{
  auto data=idPresentable::Serialize();
  AppendBytes(data,rayImpactBufferPos);
  AppendBytes(data,explosionFrame);
  AppendBytes(data,activateAllNowFrame);
  for(const auto& impact:rayImpactBuffer){
    AppendBytes(data,impact.end);
    AppendBytes(data,impact.start);
  }
  return data;
}
