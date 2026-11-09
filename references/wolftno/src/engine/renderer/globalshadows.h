#pragma once

#include "engine/renderer/jobs/approximatelighting/shadowmapcpu.h"

#include <cstddef>
#include <cstdint>

class idCVar;

class idGlobalShadows {
public:
  idGlobalShadows();
  ~idGlobalShadows();

  void LoadGlobalShadows(const char* mapName);
  const shadowMapCPU_t* GetShadowMap();

  shadowMapCPU_t* shadowMap;
};

extern idCVar r_globalShadowsCoordinateBias;
extern idCVar r_globalShadowsDepthBias;
extern idCVar r_useGlobalShadows;

static_assert(sizeof(idGlobalShadows) == sizeof(void*),
              "global-shadows ABI drift");
