#include "tungsten/game/clientgame/presentable/presentablepusher.h"

// DIA: idPresentablePusher::DisableClip
// DIA: idPresentablePusher::GetType
// DIA: idPresentablePusher::`scalar deleting destructor'
// DIA: idPresentablePusher::idPresentablePusher
// DIA: `idPresentablePusher::idPresentablePusher'::`1'::dtor$0
// DIA: idPresentablePusher::InitPhysics
// DIA: idPresentablePusher::Serialize
// DIA: idPresentablePusher::UpdateClientCollision
// DIA: `dynamic initializer for 'fakeCB''
// DIA: `dynamic initializer for 'pp_disablePush''
// DIA: `dynamic atexit destructor for 'pp_disablePush''
// DIA: `dynamic atexit destructor for 'fakeCB''

namespace { struct PusherFakeCB{PusherFakeCB():name("fakeCB"){}std::string name;};static PusherFakeCB fakeCB; }
void idPresentablePusher::DisableClip(bool disable){if(physicsInit)clipEnabled=!disable;}
int idPresentablePusher::GetType()const{return 22;}
idPresentablePusher::~idPresentablePusher()=default;
idPresentablePusher::idPresentablePusher(int owner,const std::string& model){SetEntityNumber(owner);SetRenderModel(model);}
void idPresentablePusher::InitPhysics(bool isObstacle){obstacle=isObstacle;physicsInit=true;clipEnabled=true;}
void idPresentablePusher::Serialize(idSerializer* serializer){if(!serializer)return;serializer->SerializeBool(pushEnabled);serializer->SerializeBool(obstacle);if(!serializer->writing&&!physicsInit){InitPhysics(obstacle);Present();}}
void idPresentablePusher::UpdateClientCollision(const idVec3& oldOrigin,const idMat3& oldAxis,const idVec3& newOrigin,const idMat3& newAxis){
  (void)oldAxis;
  const bool moved=!(oldOrigin==newOrigin);
  if(pp_disablePush.GetInteger()==0&&pushEnabled&&physicsInit&&clipEnabled&&moved)++pushCount;
  physicsOrigin=newOrigin;physicsAxis=newAxis;SetOrigin(newOrigin);SetAxis(newAxis);idPresentable::UpdateClientCollision();
}
idCVar pp_disablePush("pp_disablePush","0",1,"disable presentable pushers on clients");
