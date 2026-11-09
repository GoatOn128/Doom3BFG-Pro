#pragma once

#include "engine/decls/decl.h"

class idImage;
class idTextureSampler;
class idDeclRenderProg;

enum parmType_t : int {
  PT_VECTOR = 0,
  PT_TEXTURE_2D = 1,
  PT_TEXTURE_3D = 2,
  PT_TEXTURE_CUBE = 3,
  PT_TEXTURE_SHADOW_2D = 4,
  PT_TEXTURE_SHADOW_3D = 5,
  PT_TEXTURE_SHADOW_CUBE = 6,
  PT_TEXTURE_DIM_SHADOW_2D = 7,
  PT_TEXTURE_MULTISAMPLE_2D = 8,
  PT_IMAGE_2D_READ = 9,
  PT_IMAGE_2D_WRITE = 10,
  PT_SAMPLER = 11,
  PT_PROGRAM = 12,
  PT_STRING = 13,
  PT_MAX = 14
};

static_assert(PT_VECTOR == 0 && PT_TEXTURE_MULTISAMPLE_2D == 8 &&
                  PT_IMAGE_2D_READ == 9 && PT_IMAGE_2D_WRITE == 10 &&
                  PT_SAMPLER == 11 && PT_PROGRAM == 12 && PT_STRING == 13 &&
                  PT_MAX == 14,
              "render-parm type ordering drift");

enum parmCreator_t : int {
  PC_PARSED = 0,
  PC_CONSTANT = 1,
  PC_TEMPORARY = 2
};

enum parmEdit_t : int {
  PE_NO_EDIT = 0,
  PE_BOOL = 1,
  PE_RANGE = 2,
  PE_COLOR = 3,
  PE_NO_DISPLAY = 4
};

union parmValue_t {
  float value[4];
  const idImage* image;
  const idTextureSampler* sampler;
  const idDeclRenderProg* program;
  const char* string;
  int swizzle[4];
};

class idDeclRenderParm final : public idDecl {
public:
  idDeclRenderParm();
  ~idDeclRenderParm() override;

  const char* DefaultDefinition() const override;
  idDeclInfo* GetDeclInfo() const override;
  void List() const override;
  void Parse(idParser& source) override;

  idImage* ParseImageLine(idParser& source) const;
  static bool ParseVectorConstant(idParser& source, float* vector);
  void ParseValue(idParser& source, parmValue_t& value);
  void ParseStringToValue(const char* text, parmValue_t& value);

  int parmIndex;
  parmType_t parmType;
  parmCreator_t creator;
  bool cubeFilterTexture;
  parmValue_t declaredValue;
  parmEdit_t edit;
  float editRange[2];

  static idDeclInfoTemplate<idDeclRenderParm> resourceList;
};

static_assert(sizeof(parmValue_t) == 16, "parmValue_t ABI drift");
static_assert(offsetof(idDeclRenderParm, parmIndex) == 0x50,
              "render-parm index offset drift");
static_assert(offsetof(idDeclRenderParm, declaredValue) == 0x60,
              "render-parm value offset drift");
static_assert(offsetof(idDeclRenderParm, edit) == 0x70,
              "render-parm edit offset drift");
static_assert(sizeof(idDeclRenderParm) == 128, "idDeclRenderParm ABI drift");
