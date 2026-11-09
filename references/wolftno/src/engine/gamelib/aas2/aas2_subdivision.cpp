#include "engine/gamelib/aas2/aas2.h"

char idAAS2Local::GetAreaCells(int areaNum, int subSize, int* subx,
                               int* suby) {
  if (file == nullptr || areaNum <= 0) {
    return 0;
  }

  const idBounds bounds = file->AreaBounds(areaNum);
  *subx = static_cast<int>(bounds[1].x - bounds[0].x) / subSize + 1;
  *suby = static_cast<int>(bounds[1].y - bounds[0].y) / subSize + 1;
  return 1;
}

char idAAS2Local::WorldPosToCellPos(int areaNum, int subSize,
                                    const idVec3* origin, int* subx,
                                    int* suby) {
  if (file == nullptr || areaNum <= 0) {
    return 0;
  }

  const idBounds bounds = file->AreaBounds(areaNum);
  *subx = static_cast<int>((origin->x - bounds[0].x) /
                           static_cast<float>(subSize));
  *suby = static_cast<int>((origin->y - bounds[0].y) /
                           static_cast<float>(subSize));
  return 1;
}

char idAAS2Local::CellPosToWorldPos(int areaNum, int subSize, int subx,
                                    int suby, idVec3* worldPos) {
  if (file == nullptr || areaNum <= 0) {
    return 0;
  }

  const idBounds bounds = file->AreaBounds(areaNum);
  worldPos->x = static_cast<float>(subx * subSize) + bounds[0].x;
  worldPos->y = static_cast<float>(suby * subSize) + bounds[0].y;
  worldPos->z = bounds[1].z;
  return 1;
}
