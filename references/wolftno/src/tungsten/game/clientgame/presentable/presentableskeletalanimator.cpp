#include "tungsten/game/clientgame/presentable/presentableskeletalanimator.h"
#include <functional>
// DIA: idPresentableAnimatedEntity::Interpolate
// DIA: idPresentableAnimatedEntity::Hide
// DIA: idPresentableAnimatedEntity::GetTreeAnimator
// DIA: idPresentableAnimatedEntity::idPresentableAnimatedEntity
// DIA: `idPresentableAnimatedEntity::idPresentableAnimatedEntity'::`1'::dtor$0
// DIA: `idPresentableAnimatedEntity::idPresentableAnimatedEntity'::`1'::dtor$1
// DIA: idPresentableAnimatedEntity::Present
// DIA: idPresentableAnimatedEntity::UpdateClientCollision
// DIA: idPresentableAnimatedEntity::Show
// DIA: idPresentableAnimatedEntity::ClientJobSync
// DIA: idPresentableAnimatedEntity::SetRenderModel
// DIA: idPresentableAnimatedEntity::GetPresentableTagTransform
// DIA: idPresentableAnimatedEntity::GetPresentableTagTransform
// DIA: idPresentableAnimatedEntity::GetPresentableTagPosition
// DIA: idPresentableAnimatedEntity::GetPresentableTagPosition
// DIA: idPresentableAnimatedEntity::GetPresentableTagAxis
// DIA: idPresentableAnimatedEntity::GetInventory
// DIA: idPresentableAnimatedEntity::GetInventory
// DIA: idPresentableAnimatedEntity::InventoryAdded
// DIA: idPresentableAnimatedEntity::GetEyePos
// DIA: idPresentableAnimatedEntity::GetTotalCurHealth
// DIA: idPresentableAnimatedEntity::GetTotalMaxHealth
// DIA: idPresentableAnimatedEntity::GetCanAimAssist
// DIA: idPresentableAnimatedEntity::IsTargetLockable
// DIA: idPresentableAnimatedEntity::~idPresentableAnimatedEntity
// DIA: `idPresentableAnimatedEntity::~idPresentableAnimatedEntity'::`1'::dtor$0
// DIA: idPresentableAnimatedEntity::Serialize
// DIA: idPresentableAnimatedEntity::UpdateAnimation
// DIA: idPresentableAnimatedEntity::`scalar deleting destructor'
// DIA: idPresentableAnimatedEntity::ClientThink
// DIA: idPresentableAnimatedEntity::PostSerializeRead
// DIA: idPresentableAnimatedEntity::UpdateAimAssist
idPresentableAnimatedEntity::idPresentableAnimatedEntity(int number,const std::string& model){SetEntityNumber(number);idPresentable::SetRenderModel(model);}idPresentableAnimatedEntity::~idPresentableAnimatedEntity()=default;void idPresentableAnimatedEntity::Interpolate(int time,float fraction){(void)time;idPresentable::Interpolate(fraction);}void idPresentableAnimatedEntity::Hide(bool stopFx){if(stopFx)StopAllFX();idPresentable::Hide();}idTreeAnimator* idPresentableAnimatedEntity::GetTreeAnimator(){return&animator;}void idPresentableAnimatedEntity::Present(){UpdateAnimation(animator.GetLastUpdate());idPresentable::Present();}void idPresentableAnimatedEntity::UpdateClientCollision(){idPresentable::UpdateClientCollision();}void idPresentableAnimatedEntity::Show(){idPresentable::Show();}void idPresentableAnimatedEntity::ClientJobSync(){++animationUpdates;}void idPresentableAnimatedEntity::SetRenderModel(const std::string& model){idPresentable::SetRenderModel(model);}
bool idPresentableAnimatedEntity::GetPresentableTagTransform(int tag,idVec3&o,idMat3&a)const{idTagTransform t;if(!animator.GetTag(tag,t))return false;o=t.origin;a=t.axis;return true;}bool idPresentableAnimatedEntity::GetPresentableTagTransform(const std::string& tag,idVec3&o,idMat3&a)const{return GetPresentableTagTransform(static_cast<int>(std::hash<std::string>{}(tag)),o,a);}bool idPresentableAnimatedEntity::GetPresentableTagPosition(int tag,idVec3&p)const{idMat3 a{};return GetPresentableTagTransform(tag,p,a);}bool idPresentableAnimatedEntity::GetPresentableTagPosition(const std::string& tag,idVec3&p)const{idMat3 a{};return GetPresentableTagTransform(tag,p,a);}bool idPresentableAnimatedEntity::GetPresentableTagAxis(int tag,idMat3&a)const{idVec3 p{};return GetPresentableTagTransform(tag,p,a);}const idInventoryCollection* idPresentableAnimatedEntity::GetInventory()const{return&inventory;}idInventoryCollection* idPresentableAnimatedEntity::GetInventory(){return&inventory;}void idPresentableAnimatedEntity::InventoryAdded(idInventoryItem* item){if(item)inventory.items.push_back(item);}void idPresentableAnimatedEntity::GetEyePos(idVec3&p)const{idVec3 o{};idMat3 a{};idPresentable::GetWorldTransform(o,a);p=o+eyeOffset;}float idPresentableAnimatedEntity::GetTotalCurHealth()const{return curHealth;}float idPresentableAnimatedEntity::GetTotalMaxHealth()const{return maxHealth;}bool idPresentableAnimatedEntity::GetCanAimAssist()const{return canAimAssist&&curHealth>0;}bool idPresentableAnimatedEntity::IsTargetLockable()const{return GetCanAimAssist()&&!IsHidden();}
void idPresentableAnimatedEntity::Serialize(idSerializer*s){if(!s)return;s->SerializeFloat(curHealth);s->SerializeFloat(maxHealth);s->SerializeBool(canAimAssist);s->SerializeVec3(eyeOffset);}void idPresentableAnimatedEntity::UpdateAnimation(int time){animator.Update(time);++animationUpdates;}void idPresentableAnimatedEntity::ClientThink(int time,float fraction,bool predict){(void)predict;Interpolate(time,fraction);UpdateAnimation(time);idPresentable::ClientThink();UpdateAimAssist();}void idPresentableAnimatedEntity::PostSerializeRead(bool first){idPresentable::PostSerializeRead();if(first)UpdateAnimation(0);UpdateAimAssist();}void idPresentableAnimatedEntity::UpdateAimAssist(){aimAssistActive=GetCanAimAssist()&&!IsHidden();}
