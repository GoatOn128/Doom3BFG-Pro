#pragma once

#include "decl.h"

class idDeclRenderProg;

class idDeclMaterialDef final : public idDeclTypeInfo {
public:
  idDeclMaterialDef();
  idDeclInfo* GetDeclInfo() const override;

  idStr desc;
  const idDeclRenderProg* interactionProg;
  const idDeclRenderProg* stageProg;
  const idDeclRenderProg* addProg;
  const idDeclRenderProg* blendProg;
  bool useDiffuseMap;
  bool useBumpMap;
  bool useSpecularMap;
  bool useCoverMap;
  bool usePowerMap;
  bool useTransMap;
  bool useTransMap1;
  bool useTransMap2;
  bool useTransAtlasMap;
  bool useSpareDiffuseMap;
  bool useSpareDiffuseMap2;
  bool useSpareBumpMap;
  bool useSpareBumpMap1;
  bool useSpareBumpMap2;
  bool useSpareCube1Map;
  bool useSpareSpecularMap;
  bool useSpareSpecularMap2;
  bool useFadeMap;
  bool useSpare1Map;
  bool useSpare2Map;
  bool useSpare3Map;
  bool useTextureMap;
  bool useLightProjectMap;
  bool useLightFalloffMap;

  static idDeclInfoTemplate<idDeclMaterialDef> resourceList;
};

static_assert(sizeof(idDeclMaterialDef) == 192, "idDeclMaterialDef ABI drift");
