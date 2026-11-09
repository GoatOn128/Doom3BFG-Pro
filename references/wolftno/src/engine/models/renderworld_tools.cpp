#include "engine/models/renderworld.h"

#include <cstring>

void idRenderWorldLocal::ApproximateLightingForBounds(
    const idBounds* bounds, approximateLighting_t* lighting) {
  if (lighting != nullptr && committedWorld != nullptr)
    committedWorld->ApproximateLightingForBounds(bounds, lighting);
}

void idRenderWorldLocal::ApproximateAmbientForBounds(
    const idBounds* bounds, approximateLighting_t* lighting) {
  if (lighting != nullptr && committedWorld != nullptr)
    committedWorld->ApproximateAmbientForBounds(bounds, lighting);
}

void idRenderWorldLocal::BoundsInAreas(const idBounds* bounds, int* areas,
                                       int* numAreas, int maxAreas) {
  committedWorld->BoundsInAreas(bounds, areas, numAreas, maxAreas);
}

bool idRenderWorldLocal::Trace(modelTrace_t* trace, const idVec3*, const idVec3*,
                               const idTraceOpts*) {
  std::memset(trace, 0, sizeof(*trace));
  return false;
}

const idParmBlock* idRenderWorldLocal::GetCurrentBlendedParms() {
  static idParmBlock block;
  return &block;
}

void idRenderWorldLocal::GetStaticWorldBounds(idBounds* bounds) {
  bounds->b[0] = {1.0e30f, 1.0e30f, 1.0e30f};
  bounds->b[1] = {-1.0e30f, -1.0e30f, -1.0e30f};
}
