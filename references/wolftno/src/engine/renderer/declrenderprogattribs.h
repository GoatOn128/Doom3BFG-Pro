#pragma once

#include "shared/idlib/containers/staticlist.h"
#include "shared/idlib/text/str.h"

enum renderProgAttribType_t {
  AT_VS_IN = 1 << 1,
  AT_VS_OUT = 1 << 2,
  AT_PS_IN = 1 << 3,
  AT_PS_OUT = 1 << 4,
  AT_PS_INOUT = 1 << 5,
};

struct attribInfo_t {
  const char* type;
  const char* name;
  const char* semantic;
  const char* glsl;
  int bind;
  int flags;
  int vertexMask;
};

extern attribInfo_t attribsPC[];
extern attribInfo_t attribsPCD3D[];
extern attribInfo_t attribs360[];
extern attribInfo_t attribsPS3[];
extern attribInfo_t attribsOrbis[];

struct shaderConstructInfo_t {
  shaderConstructInfo_t();
  ~shaderConstructInfo_t();
  void Clear();

  bool parsed;
  unsigned char alignmentPadding[7];
  idStr headerString;
  idStr sourceString;
  idStr outputAttribInitString;
  idStaticList<attribInfo_t, 16> inAttribs;
  idStaticList<attribInfo_t, 16> outAttribs;
};

static_assert(sizeof(attribInfo_t) == 0x30, "render-program attribute ABI drift");
static_assert(sizeof(shaderConstructInfo_t) == 0x6B0,
              "shader construction-info ABI drift");
