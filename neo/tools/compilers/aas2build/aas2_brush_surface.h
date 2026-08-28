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

#include "aas2_cspace.h"
#include "aas2_types.h"

#include <cstddef>
#include <vector>

namespace darklight {
namespace compilers {
namespace aas2 {

enum class BrushSurfaceError {
    none,
    invalidPlane,
    unboundedOrDegenerateBrush,
    fragmentLimit
};

struct BrushSurfaceResult {
    BrushSurfaceError error = BrushSurfaceError::none;
    std::size_t sourceBrush = 0;
    std::size_t sourcePlane = 0;
    std::size_t boundaryFaces = 0;
    std::size_t floorFaces = 0;
    std::size_t floorTriangles = 0;
    std::size_t discardedDegenerateTriangles = 0;
    std::size_t candidatePairs = 0;
    std::size_t rejectedCandidatePairs = 0;
    std::size_t peakFragments = 0;
    std::size_t peakFragmentPoints = 0;

    explicit operator bool() const { return error == BrushSurfaceError::none; }
};

// Intersects normalized convex-brush half-spaces, reconstructs each finite
// boundary polygon, and emits walkable outward-facing polygons as triangles.
// Floor polygons are CSG-subtracted against the other convex brushes before
// triangulation. Full solid BSP node construction and clearance pruning remain
// subsequent stages.
BrushSurfaceResult BuildBrushFloorGeometry(
    const std::vector<ConvexBrush>& brushes,
    float maxFloorSlopeDegrees,
    float epsilon,
    SourceGeometry& output,
    ProgressCallback progress = nullptr,
    void* progressUserData = nullptr);

const char* BrushSurfaceErrorName(BrushSurfaceError error);

} // namespace aas2
} // namespace compilers
} // namespace darklight
