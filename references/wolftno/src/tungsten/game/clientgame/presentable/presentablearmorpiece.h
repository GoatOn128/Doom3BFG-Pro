#pragma once
#include "tungsten/game/clientgame/presentable/presentable.h"

class idPresentableArmorPiece;
class idArmorPiecePhysicsCallback {
public: explicit idArmorPiecePhysicsCallback(idPresentableArmorPiece* owner=nullptr):self(owner){} const char* GetName()const; void ApplyImpulse(int physId,int bodyId,const idVec3& point,const idVec3& impulse); void ApplyForce(int physId,int bodyId,const idVec3& point,const idVec3& force); void ActivatePhysics(int physId); void DeactivatePhysics(int physId);
private:idPresentableArmorPiece* self{nullptr};
};
class idPresentableArmorPiece:public idPresentable{
public:idPresentableArmorPiece(const idVec3& origin,const idMat3& axis,int entityNumber);~idPresentableArmorPiece()override=default;int GetType()const;void InitArmorDefaults();void ClientThink(int currentTime,float fraction,bool predict);void ApplyImpulse(const idVec3& value);void ApplyForce(const idVec3& value);void ActivatePhysics();void DeactivatePhysics();void SetRemoveWhenAtRest(bool value,float fadeSeconds){removeWhenAtRest=value;fadeTime=fadeSeconds;}const idVec3& GetVelocity()const{return velocity;}bool IsPhysicsActive()const{return physicsActive;}bool IsQueuedForDelete()const{return queuedForDelete;}idArmorPiecePhysicsCallback& GetCallback(){return physicsCallback;}
private:idArmorPiecePhysicsCallback physicsCallback;idVec3 velocity{};float fadeTime{0};int restFrames{0};bool physicsActive{false};bool removeWhenAtRest{false};bool queuedForDelete{false};
};
extern idCVar armor_test;
