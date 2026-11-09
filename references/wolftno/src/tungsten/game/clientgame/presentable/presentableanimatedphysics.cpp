#include "tungsten/game/clientgame/presentable/presentableanimatedphysics.h"

// DIA: idPresentableDoorAnimated::LinkCollision
// DIA: idPresentableDoorAnimated::Present
// DIA: idPresentableAnimatedPhysics::GetType
// DIA: idDummyPhysicsCallback::GetName
// DIA: idPresentableAnimatedPhysics::`scalar deleting destructor'
// DIA: idSerializer::SerializeString
// DIA: idPresentableAnimatedPhysics::idPresentableAnimatedPhysics
// DIA: idPresentableAnimatedPhysics::Serialize
// DIA: `dynamic initializer for 'dummyCB''
// DIA: `dynamic atexit destructor for 'dummyCB''

void idPresentableDoorAnimated::LinkCollision(){idPresentableAnimatedPhysics::LinkCollision();}
void idPresentableDoorAnimated::Present(){idPresentableAnimatedPhysics::Present();}
int idPresentableAnimatedPhysics::GetType()const{return 31;}
idDummyPhysicsCallback::idDummyPhysicsCallback():name("dummyCB"){}
idDummyPhysicsCallback::~idDummyPhysicsCallback()=default;
const char* idDummyPhysicsCallback::GetName()const{return name.c_str();}
void idSerializer::SerializeString(idAtomicString* value){if(value==nullptr)return;if(writing)strings.push_back(value->value);else if(readIndex<strings.size())value->value=strings[readIndex++];}
idPresentableAnimatedPhysics::idPresentableAnimatedPhysics(int owner,const std::string& clipName,bool isObstacle):clip(clipName),entityOwnerIndex(owner),obstacle(isObstacle){}
void idPresentableAnimatedPhysics::Serialize(idSerializer* serializer){if(serializer==nullptr)return;idAtomicString value{clip};serializer->SerializeString(&value);if(!serializer->writing)clip=value.value;}
void idPresentableAnimatedPhysics::LinkCollision(){linked=true;}void idPresentableAnimatedPhysics::UnlinkCollision(){linked=false;}void idPresentableAnimatedPhysics::Present(){idPresentable::Present();}
idDummyPhysicsCallback dummyCB;
