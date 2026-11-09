#pragma once

#include "engine/renderer/parmblock.h"

#include <cstddef>
#include <cstdint>

class idDeclRenderProg;

struct idParmBlockOptimizer {
  int usedParms[200];
  int numUsedParms;

  static int c_retainedOps;
  static int c_removedOps;

  void AddRenderProg(const idDeclRenderProg* prog);
  void OptimizeParmBlockForUsedParms(idParmBlock& parmBlock);
};

static_assert(offsetof(idParmBlock, constants) == 0x18,
              "render parameter constant-list offset drift");
static_assert(offsetof(idParmBlockOptimizer, numUsedParms) == 0x320,
              "parameter optimizer used-count offset drift");
static_assert(sizeof(idParmBlockOptimizer) == 0x324,
              "idParmBlockOptimizer ABI drift");
