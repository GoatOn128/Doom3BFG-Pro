#include "declmaterialdef.h"

idDeclInfoTemplate<idDeclMaterialDef> idDeclMaterialDef::resourceList(
    "materialDef", "idDeclMaterialDef", "def", false);

idDeclMaterialDef::idDeclMaterialDef()
    : interactionProg(nullptr), stageProg(nullptr), addProg(nullptr), blendProg(nullptr),
      useDiffuseMap(false), useBumpMap(false), useSpecularMap(false), useCoverMap(false),
      usePowerMap(false), useTransMap(false), useTransMap1(false), useTransMap2(false),
      useTransAtlasMap(false), useSpareDiffuseMap(false), useSpareDiffuseMap2(false),
      useSpareBumpMap(false), useSpareBumpMap1(false), useSpareBumpMap2(false),
      useSpareCube1Map(false), useSpareSpecularMap(false), useSpareSpecularMap2(false),
      useFadeMap(false), useSpare1Map(false), useSpare2Map(false), useSpare3Map(false),
      useTextureMap(false), useLightProjectMap(false), useLightFalloffMap(false) {
  resourceListPtr = &resourceList;
}

idDeclInfo* idDeclMaterialDef::GetDeclInfo() const {
  return &resourceList;
}
