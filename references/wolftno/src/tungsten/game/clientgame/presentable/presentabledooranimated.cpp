#include "tungsten/game/clientgame/presentable/presentableanimatedphysics.h"

// Complete recovered function inventory. The constructor and out-of-line
// destructor provide the retail cleanup and scalar-delete machinery.
// DIA: idFakeCBStopWhenCollide::GetName
// DIA: idPresentableDoorAnimated::UnlinkCollision
// DIA: idPresentableDoorAnimated::GetType
// DIA: idPresentableDoorAnimated::`scalar deleting destructor'
// DIA: idPresentableDoorAnimated::InitPhysics
// DIA: idPresentableDoorAnimated::idPresentableDoorAnimated
// DIA: `idPresentableDoorAnimated::idPresentableDoorAnimated'::`1'::dtor$0
// DIA: `idPresentableDoorAnimated::idPresentableDoorAnimated'::`1'::dtor$1
// DIA: idPresentableDoorAnimated::Serialize
// DIA: `dynamic initializer for 'fakeCB''
// DIA: `dynamic atexit destructor for 'fakeCB''

const char* idFakeCBStopWhenCollide::GetName()const{return name.c_str();}

void idPresentableDoorAnimated::UnlinkCollision(){
  idPresentableAnimatedPhysics::UnlinkCollision();
}

int idPresentableDoorAnimated::GetType()const{return 18;}

idPresentableDoorAnimated::~idPresentableDoorAnimated()=default;

void idPresentableDoorAnimated::InitPhysics(const std::string& clipName,bool isObstacle){
  clip=clipName;
  obstacle=isObstacle;
  physicsInit=!clip.empty();
}

idPresentableDoorAnimated::idPresentableDoorAnimated(int owner,const std::string& clipName,bool isObstacle)
  :idPresentableAnimatedPhysics(owner,clipName,isObstacle){
  InitPhysics(clipName,isObstacle);
}

void idPresentableDoorAnimated::Serialize(idSerializer* serializer){
  if(serializer==nullptr)return;
  const bool wasLinked=linked;
  idPresentableAnimatedPhysics::Serialize(serializer);
  serializer->SerializeInt(entityOwnerIndex);
  serializer->SerializeBool(linked);
  if(!serializer->writing&&!physicsInit)InitPhysics(clip,false);
  if(!serializer->writing&&linked!=wasLinked){
    if(linked)idPresentableAnimatedPhysics::LinkCollision();
    else idPresentableAnimatedPhysics::UnlinkCollision();
  }
}

idFakeCBStopWhenCollide fakeCB;
