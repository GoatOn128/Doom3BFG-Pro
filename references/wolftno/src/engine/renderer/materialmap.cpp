#include "engine/renderer/materialmap.h"

#include "shared/idlib/csystems/cvarsystemlocal.h"

idCVar r_writePackedMaterials(
    "r_writePackedMaterials", "0", CVAR_BOOL,
    "debug option to write all the packed materials out to viewable tga files as they are retrieved");

bool idMaterialMap::GetImageDimensions(int& imageWidth, int& imageHeight) const {
  imageWidth = width;
  imageHeight = height;
  return true;
}
