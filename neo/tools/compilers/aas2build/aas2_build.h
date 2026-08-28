/*
===========================================================================

Darklight Engine GPL Source Code
Copyright (C) 2026 Justin Marshall(justinmarshall20@gmail.com)

This file is part of the idTech 5 Reconstruction Project and the Darklight Engine Project

Darklight Engine GPL is free software: you can redistribute it and/or modify
it under the terms of the GNU General Public License as published by
the Free Software Foundation, either version 3 of the License, or
(at your option) any later version.

Darklight Engine GPL is distributed in the hope that it will be useful,
but WITHOUT ANY WARRANTY; without even the implied warranty of
MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
GNU General Public License for more details.

You should have received a copy of the GNU General Public License
along with Darklight Engine GPL.  If not, see <http://www.gnu.org/licenses/>.

===========================================================================
*/


#pragma once

#include "aas2_brush_surface.h"
#include "aas2_compiler.h"
#include "aas2_cspace.h"

#include <vector>

namespace darklight {
namespace compilers {
namespace aas2 {

struct BuildSettings {
    Settings navigation{};
    AgentBounds agent{{-16.0f, -16.0f, 0.0f},
                      {16.0f, 16.0f, 72.0f}};
    float geometryEpsilon = 0.01f;
    ProgressCallback progress = nullptr;
    void* progressUserData = nullptr;
};

enum class BuildError {
    none,
    configurationSpace,
    brushSurface,
    surfaceCompiler
};

struct BuildResult {
    BuildError error = BuildError::none;
    CSpaceResult configurationSpace{};
    BrushSurfaceResult brushSurface{};
    Result surface{};

    explicit operator bool() const { return error == BuildError::none; }
};

BuildResult CompileBrushes(const std::vector<ConvexBrush>& source,
                           const BuildSettings& settings,
                           File& output);

const char* BuildErrorName(BuildError error);

} // namespace aas2
} // namespace compilers
} // namespace darklight


