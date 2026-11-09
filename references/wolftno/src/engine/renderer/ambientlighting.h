#pragma once

#include "engine/renderer/jobs/approximatelighting/ambientmapcpu.h"

#include <cstddef>
#include <cstdint>

class idRenderDebug;

class idAmbientLighting {
public:
  idAmbientLighting();
  ~idAmbientLighting();

  void LoadAmbientLighting(const char* mapName);
  void DebugDrawAmbientLightingSamples(idRenderDebug* renderDebug,
                                       const idVec3& position) const;
  static void SwapNodes(ambientNode_t* nodes, int numNodes);
  static void SwapSamples(ambientSample_t* samples, int numSamples);

private:
  friend class idRender;
  friend class idRenderWorldCommitted;
  void FreeData();

  ambientMapCPU_t* ambientMap;
};

static_assert(sizeof(idAmbientLighting) == sizeof(void*),
              "ambient-lighting ABI drift");
