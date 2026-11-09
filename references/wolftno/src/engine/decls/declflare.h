#pragma once

#include "decl.h"
#include "shared/idlib/math/vector.h"

class idMaterial;

class idDeclFlare final : public idDeclTypeInfo {
public:
  struct subFlare_t {
    float width;
    float height;
    float minScaleX;
    float minScaleY;
    float viewScaleX;
    float viewScaleY;
    float coneAngle;
    float maxDistScaleX;
    float maxDistScaleY;
    float scaleDistance;
    float position;
    idVec4 color;
    const idMaterial* mtr;
    bool reflectX;
    bool reflectY;
  };

  idDeclFlare();
  void FreeData() override;
  idDeclInfo* GetDeclInfo() const override;

  idList<subFlare_t, 79> subflares;
  std::uint8_t useAltViewVecCalc : 1;
  float viewAngleFade;
  float invViewAngleFade;
  float brightness;
  float cullDistance;

  static idDeclInfoTemplate<idDeclFlare> resourceList;
};

static_assert(sizeof(idDeclFlare::subFlare_t) == 80, "idDeclFlare::subFlare_t ABI drift");
static_assert(sizeof(idDeclFlare) == 144, "idDeclFlare ABI drift");
