#pragma once

#include <cstddef>

class idDeclRenderParm;
class idImage;

struct setupColorGradingParms_t {
  const idDeclRenderParm* rpCGShadow;
  const idDeclRenderParm* rpCGMidtone;
  const idDeclRenderParm* rpCGHighlight;
  const idDeclRenderParm* rpCGColorDodgeBlend;
  const idDeclRenderParm* rpCGColorBurnBlend;
  const idDeclRenderParm* rpCGColorMultiplyBlend;
  const idDeclRenderParm* rpCGColorScreenBlend;
  const idDeclRenderParm* rpCGBlackLevel;
  const idDeclRenderParm* rpColorGradingLUT;
  idImage* imageLUT;
};

void SetupColorGrading(const setupColorGradingParms_t* parameters);

static_assert(offsetof(setupColorGradingParms_t, rpColorGradingLUT) == 0x40,
              "color-grading LUT parameter offset drift");
static_assert(offsetof(setupColorGradingParms_t, imageLUT) == 0x48,
              "color-grading image offset drift");
static_assert(sizeof(setupColorGradingParms_t) == 0x50,
              "color-grading parameters ABI drift");
