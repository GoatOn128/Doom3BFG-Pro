#pragma once

#include "engine/renderer/render.h"

#include <cstddef>

class idDeclRenderParm;
class idDeclRenderProg;
class idImage;
class idRenderDestination;
class idTriangles;

struct renderDynamicEnvParms_t {
  const renderSettings_t* settings;
  const idRenderView* renderView;
  const idDeclRenderParm* rpEnvBlendMip;
  const idDeclRenderParm* rpAmbientEnvMap;
  const idDeclRenderParm* rpUserEnvMap;
  const idDeclRenderParm* rpColor;
  const idDeclRenderParm* rpEnvSMatrix;
  const idDeclRenderParm* rpEnvTMatrix;
  const idDeclRenderParm* rpEnvRMatrix;
  const idRenderDestination* renderDestDynamicEnv;
  const idImage* imgDynamicEnv;
  const idImage* imgAmbientEnv;
  const idImage* imgUserEnv;
  const idTriangles* unitCubeTris;
  const idDeclRenderProg* progBlend;
  idMat3 envMapRotation;
  envBlend_t* envBlend;
};

void RenderDynamicEnv(const renderDynamicEnvParms_t* parameters);

static_assert(offsetof(renderDynamicEnvParms_t, envMapRotation) == 0x78,
              "dynamic-environment rotation offset drift");
static_assert(offsetof(renderDynamicEnvParms_t, envBlend) == 0xA0,
              "dynamic-environment blend-state offset drift");
static_assert(sizeof(renderDynamicEnvParms_t) == 0xA8,
              "dynamic-environment parameters ABI drift");
