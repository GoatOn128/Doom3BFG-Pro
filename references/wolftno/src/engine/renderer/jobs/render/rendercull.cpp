#include "engine/renderer/jobs/render/rendercull.h"

namespace {
bool ContainsBounds(const idBounds& outer, const idBounds& inner) {
  return outer[0].x <= inner[0].x && outer[0].y <= inner[0].y &&
         outer[0].z <= inner[0].z && inner[1].x <= outer[1].x &&
         inner[1].y <= outer[1].y && inner[1].z <= outer[1].z;
}
} // namespace

void RenderCull(const renderCullParms_t* parameters) {
  const idRenderMatrix& worldMvp = parameters->renderView->worldSpaceMVPMatrix;

  for (int areaNode = 0; areaNode < parameters->numAreaNodes; ++areaNode) {
    unsigned char clipBits = 0;
    parameters->areaNodeBoundsCulled[areaNode] =
        idRenderMatrix::CullBoundsToMVPbits(
            worldMvp, parameters->areaNodeBounds[areaNode], clipBits, false);
  }

  for (int worldArea = 0; worldArea < parameters->numWorldAreas; ++worldArea) {
    const idBounds& geometryBounds =
        parameters->worldAreaGeometryBounds[worldArea];
    const bool subspaceCulled = idRenderMatrix::CullBoundsToMVPbits(
        worldMvp, parameters->worldAreaSubspaceBounds[worldArea],
        parameters->worldAreaSubspaceBoundsCulled[worldArea], false);

    unsigned char& classification =
        parameters->worldAreaGeometryBoundsCulled[worldArea];
    classification = 3;
    if (subspaceCulled)
      continue;

    unsigned char clipBits = 0;
    if (idRenderMatrix::CullBoundsToMVPbits(worldMvp, geometryBounds,
                                            clipBits, false))
      continue;

    if (!ContainsBounds(parameters->settings->detailBounds, geometryBounds)) {
      classification = 2;
    } else if (!ContainsBounds(parameters->settings->viewBounds,
                               geometryBounds)) {
      classification = 1;
    } else {
      classification = 0;
    }
  }
}

