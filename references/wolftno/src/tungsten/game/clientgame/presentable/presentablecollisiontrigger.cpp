#include "tungsten/game/clientgame/presentable/presentablecollisiontrigger.h"

// DIA: idPresentableCollisionTrigger::GetType
int idPresentableCollisionTrigger::GetType()const{return 29;}
// DIA: idPresentableCollisionTrigger::`scalar deleting destructor'
// DIA: idPresentableCollisionTrigger::idPresentableCollisionTrigger
idPresentableCollisionTrigger::idPresentableCollisionTrigger()=default;
// DIA: idPresentableCollisionTrigger::Serialize
std::vector<std::uint8_t> idPresentableCollisionTrigger::Serialize()const{auto data=idPresentable::Serialize();data.push_back(static_cast<std::uint8_t>(triggered));return data;}
// DIA: idPresentableCollisionTrigger::PredictTouch
void idPresentableCollisionTrigger::PredictTouch(idPresentable* player){if(!triggered&&player!=nullptr&&targetPresentable!=nullptr){targetPresentable->ClientOverrideClipContents(1);triggered=true;}}
