#pragma once

#include "shared/idlib/containers/list.h"
#include "shared/idlib/math/vector.h"

float RectPackingFraction(const idList<idVec2i>& inputSizes, idVec2i totalSize);
bool RectAllocator(const idList<idVec2i>& inputSizes, idList<idVec2i>& outputPositions,
                   idVec2i& totalSize);
