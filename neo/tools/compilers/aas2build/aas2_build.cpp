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


#include "aas2_build.h"

namespace darklight {
namespace compilers {
namespace aas2 {

BuildResult CompileBrushes(const std::vector<ConvexBrush>& source,
                           const BuildSettings& settings,
                           File& output) {
    output = File{};
    BuildResult result;
    if (settings.progress != nullptr) {
        settings.progress("configuration space", 0, source.size(),
                          settings.progressUserData);
    }
    std::vector<ConvexBrush> expanded;
    result.configurationSpace =
        ExpandConfigurationSpace(source, settings.agent, expanded);
    if (!result.configurationSpace) {
        result.error = BuildError::configurationSpace;
        return result;
    }
    if (settings.progress != nullptr) {
        settings.progress("configuration space", source.size(), source.size(),
                          settings.progressUserData);
    }

    SourceGeometry floors;
    result.brushSurface = BuildBrushFloorGeometry(
        expanded, settings.navigation.maxFloorSlopeDegrees,
        settings.geometryEpsilon, floors, settings.progress,
        settings.progressUserData);
    if (!result.brushSurface) {
        result.error = BuildError::brushSurface;
        return result;
    }

    result.surface = CompileSurface(floors, settings.navigation, output,
                                    settings.progress,
                                    settings.progressUserData);
    if (!result.surface) {
        result.error = BuildError::surfaceCompiler;
    }
    return result;
}

const char* BuildErrorName(BuildError error) {
    switch (error) {
        case BuildError::none: return "none";
        case BuildError::configurationSpace: return "configuration space";
        case BuildError::brushSurface: return "brush surface";
        case BuildError::surfaceCompiler: return "surface compiler";
    }
    return "unknown";
}

} // namespace aas2
} // namespace compilers
} // namespace darklight


