#pragma once

#include "engine/renderer/rendermodelparms.h"
#include "shared/idlib/geometry/rendermatrix.h"

class idRenderView;

void SetupModelMVPMatrix(idRenderMatrix* mvpMatrix,
                         const idRenderModelParms* parameters,
                         const idRenderMatrix* modelMatrix,
                         const idRenderView* renderView, float zNear,
                         float zFar, float guiOriginOffset);
