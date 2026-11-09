#include "tungsten/game/clientgame/presentable/presentablearmorpiece.h"

// DIA: idArmorPiecePhysicsCallback::GetName
// DIA: idPresentableArmorPiece::GetType
// DIA: idPresentableArmorPiece::InitArmorDefaults
// DIA: idArmorPiecePhysicsCallback::ApplyImpulse
// DIA: idArmorPiecePhysicsCallback::ApplyForce
// DIA: idArmorPiecePhysicsCallback::ActivatePhysics
// DIA: idArmorPiecePhysicsCallback::DeactivatePhysics
// DIA: idPresentableArmorPiece::idPresentableArmorPiece
// DIA: `idPresentableArmorPiece::idPresentableArmorPiece'::`1'::dtor$0
// DIA: `idPresentableArmorPiece::idPresentableArmorPiece'::`1'::dtor$1
// DIA: `idPresentableArmorPiece::idPresentableArmorPiece'::`1'::dtor$2
// DIA: idPresentableArmorPiece::`scalar deleting destructor'
// DIA: idPresentableArmorPiece::ClientThink
// DIA: `dynamic initializer for 'armor_test''
// DIA: `dynamic atexit destructor for 'armor_test''

const char* idArmorPiecePhysicsCallback::GetName()const{return "Client Armor piece physics";}void idArmorPiecePhysicsCallback::ApplyImpulse(int,int,const idVec3&,const idVec3& impulse){if(self)self->ApplyImpulse(impulse);}void idArmorPiecePhysicsCallback::ApplyForce(int,int,const idVec3&,const idVec3& force){if(self)self->ApplyForce(force);}void idArmorPiecePhysicsCallback::ActivatePhysics(int){if(self)self->ActivatePhysics();}void idArmorPiecePhysicsCallback::DeactivatePhysics(int){if(self)self->DeactivatePhysics();}
int idPresentableArmorPiece::GetType()const{return 21;}void idPresentableArmorPiece::InitArmorDefaults(){velocity={};fadeTime=0;restFrames=0;physicsActive=false;removeWhenAtRest=false;queuedForDelete=false;}idPresentableArmorPiece::idPresentableArmorPiece(const idVec3& origin,const idMat3& axis,int entityNumber):physicsCallback(this){SetOrigin(origin);SetAxis(axis);SetEntityNumber(entityNumber);InitArmorDefaults();}void idPresentableArmorPiece::ApplyImpulse(const idVec3& value){velocity+=value;physicsActive=true;}void idPresentableArmorPiece::ApplyForce(const idVec3& value){velocity+=value*0.016F;physicsActive=true;}void idPresentableArmorPiece::ActivatePhysics(){physicsActive=true;}void idPresentableArmorPiece::DeactivatePhysics(){physicsActive=false;}void idPresentableArmorPiece::ClientThink(int currentTime,float fraction,bool predict){(void)currentTime;(void)predict;if(physicsActive){velocity*=0.98F;if(velocity.Length()<0.01F){velocity={};physicsActive=false;++restFrames;}}if(removeWhenAtRest&&!physicsActive){if(fadeTime<=0||fraction>=1)queuedForDelete=true;}Present();}
idCVar armor_test("armor_test","0",1,"armor-piece debug display");
