#pragma once
#include "tungsten/game/clientgame/presentable/presentable.h"

class idPresentableCollisionTrigger:public idPresentable{
public:idPresentableCollisionTrigger();~idPresentableCollisionTrigger()override=default;int GetType()const;std::vector<std::uint8_t> Serialize()const;void PredictTouch(idPresentable* player);void SetTarget(idPresentable* value){targetPresentable=value;}void SetTriggered(bool value){triggered=value;}bool IsTriggered()const{return triggered;}
private:idPresentable* targetPresentable{nullptr};bool triggered{false};
};
