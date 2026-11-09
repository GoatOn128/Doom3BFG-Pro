#include "tungsten/game/clientgame/presentable/presentableprop.h"
#include <algorithm>
// DIA: idPresentableProp::~idPresentableProp
// DIA: idPresentableProp::InternalPickupProp
// DIA: idPresentableProp::GetType
// DIA: idPresentableProp::`scalar deleting destructor'
// DIA: idPresentableProp::idPresentableProp
// DIA: idPresentableProp::ClientPredict
// DIA: idPresentableProp::GetOnlineUsableText
// DIA: idPresentableProp::Serialize
// DIA: idPresentableProp::GetPlayerIndex
// DIA: idPresentableProp::PickupProp
idPresentableProp::~idPresentableProp()=default;void idPresentableProp::InternalPickupProp(idPresentable* activator){(void)activator;LocalStopAllFX();if(!pickupSound.empty()&&shouldPlayPickupSound)StartSoundShader(pickupSound,2);Hide();SetLocalClipModelContents(0);LocalStartFX("prop/pickup");}int idPresentableProp::GetType()const{return 16;}idPresentableProp::idPresentableProp(const std::string& inventoryName,bool usable):inventoryDecl(inventoryName),isUsable(usable){}
void idPresentableProp::ClientPredict(int time,float fraction){Interpolate(fraction);if(eventTime!=lastHandledEventTime){if(!predictedPickup)InternalPickupProp(nullptr);predictedPickup=false;lastHandledEventTime=eventTime;}if(predictedPickup){Hide();SetLocalClipModelContents(0);}eventTime=std::max(eventTime,time);}std::string idPresentableProp::GetOnlineUsableText()const{return isUsable?inventoryDecl:std::string{};}
void idPresentableProp::Serialize(idSerializer* s){if(!s)return;idAtomicString inventory{inventoryDecl};idAtomicString sound{pickupSound};s->SerializeString(&inventory);s->SerializeString(&sound);s->SerializeInt(eventTime);s->SerializeInt(playerIndex);s->SerializeInt(presentableIndex);s->SerializeBool(isUsable);s->SerializeBool(shouldPlayPickupSound);s->SerializeBool(pickupOnTouch);s->SerializeBool(pickupOnUse);inventoryDecl=inventory.value;pickupSound=sound.value;}
int idPresentableProp::GetPlayerIndex(idPresentable* presentable)const{return presentable?1:0;}void idPresentableProp::PickupProp(idPresentable* activator){predictedPickup=true;presentableIndex=activator?1:-1;playerIndex=GetPlayerIndex(activator);++eventTime;InternalPickupProp(activator);}
