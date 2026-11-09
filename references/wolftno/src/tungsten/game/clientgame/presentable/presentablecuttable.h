#pragma once
#include "tungsten/game/clientgame/presentable/presentable.h"
class idPresentableCuttable:public idPresentable{public:explicit idPresentableCuttable(const std::string& renderModel);~idPresentableCuttable()override=default;};
