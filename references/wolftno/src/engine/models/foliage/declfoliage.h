#pragma once

#include "engine/decls/decl.h"
#include "shared/idlib/csystems/cvarsystemlocal.h"
#include "shared/idlib/math/vector.h"

#include <cstddef>

class idMaterial;

enum foliageShape_t : std::int32_t {
  FOLIAGE_SHAPE_AUTOSPRITES = 0,
  FOLIAGE_SHAPE_DOUBLES = 1,
  FOLIAGE_SHAPE_TRIPLES = 2,
  FOLIAGE_SHAPE_FIXEDSPRITES = 3,
  FOLIAGE_SHAPE_NUM = 4,
};

class idDeclFoliage : public idDecl {
public:
  idDeclFoliage();
  ~idDeclFoliage() override;

  const char* DefaultDefinition() const override;
  void Print() override { static_cast<const idDeclFoliage*>(this)->Print(); }
  void Print() const;
  void List() const override;
  idDeclInfo* GetDeclInfo() const override;
  void Parse(idParser& source) override;

  std::uint16_t quadWidth;
  std::uint16_t quadHeight;
  float widthVariance;
  float heightVariance;
  idVec3 colorVariance;
  bool rndFlipHoriz;
  float swayMagnitude;
  const idMaterial* material;
  foliageShape_t shape;
  idList<const idMaterial*, 5> stampMaterials;
  float stampScale;
  float stampParms[4];
  float stampCovers[4];
  idAtomicString stampBlendMode;
  bool isOpaque;
  bool isReferenceType;
  bool isSinglePlacement;

  static idDeclInfoTemplate<idDeclFoliage> resourceList;

private:
  void Clear();
};

extern idCVar r_skipFoliage;

static_assert(sizeof(idDeclFoliage) == 208);
static_assert(offsetof(idDeclFoliage, quadWidth) == 80);
static_assert(offsetof(idDeclFoliage, quadHeight) == 82);
static_assert(offsetof(idDeclFoliage, widthVariance) == 84);
static_assert(offsetof(idDeclFoliage, heightVariance) == 88);
static_assert(offsetof(idDeclFoliage, colorVariance) == 92);
static_assert(offsetof(idDeclFoliage, rndFlipHoriz) == 104);
static_assert(offsetof(idDeclFoliage, swayMagnitude) == 108);
static_assert(offsetof(idDeclFoliage, material) == 112);
static_assert(offsetof(idDeclFoliage, shape) == 120);
static_assert(offsetof(idDeclFoliage, stampMaterials) == 128);
static_assert(offsetof(idDeclFoliage, stampScale) == 152);
static_assert(offsetof(idDeclFoliage, stampParms) == 156);
static_assert(offsetof(idDeclFoliage, stampCovers) == 172);
static_assert(offsetof(idDeclFoliage, stampBlendMode) == 192);
static_assert(offsetof(idDeclFoliage, isOpaque) == 200);
static_assert(offsetof(idDeclFoliage, isReferenceType) == 201);
static_assert(offsetof(idDeclFoliage, isSinglePlacement) == 202);
