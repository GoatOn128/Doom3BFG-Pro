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

#include "aas2_types.h"

#include <cstddef>

namespace darklight {
namespace compilers {
namespace aas2 {

struct Settings {
    float maxFloorSlopeDegrees = 45.0f;
    float maxStepHeight = 18.0f;
    float maxFallHeight = 128.0f;
    float agentHeight = 72.0f;
    float agentRadius = 16.0f;
    float weldEpsilon = 0.01f;
    bool mergeCoplanarFloors = true;
    float floorMergeNormalDegrees = 1.0f;
    float floorMergeDistanceEpsilon = 0.05f;
};

struct Statistics {
    std::size_t inputTriangles = 0;
    std::size_t walkableTriangles = 0;
    std::size_t rejectedSteepTriangles = 0;
    std::size_t rejectedDegenerateTriangles = 0;
    std::size_t rejectedDuplicateTriangles = 0;
    std::size_t nonManifoldEdges = 0;
    std::size_t mergedFloorPairs = 0;
    std::size_t vertices = 0;
    std::size_t edges = 0;
    std::size_t areas = 0;
    std::size_t reachabilities = 0;
    std::size_t stepReachabilities = 0;
    std::size_t fallReachabilities = 0;
    std::size_t ledgeGrabReachabilities = 0;
};

enum class Error {
    none,
    emptyGeometry,
    invalidVertexIndex,
    degenerateTriangle,
    nonManifoldEdge,
    invalidOutput
};

struct Result {
    Error error = Error::none;
    std::size_t sourceTriangle = 0;
    Statistics statistics{};

    explicit operator bool() const { return error == Error::none; }
};

// Reconstructed surface/floor slice: converts indexed walkable triangle
// geometry into the portable AAS2 model, merges convex coplanar floors, and
// builds shared-boundary reachability. Configuration-space expansion, BSP,
// obstacle PVS, and spatial analysis remain subsequent pipeline stages.
Result CompileSurface(const SourceGeometry& source,
                      const Settings& settings,
                      File& output,
                      ProgressCallback progress = nullptr,
                      void* progressUserData = nullptr);

// Adds endpoint/T-junction transitions for areas tagged by dynamic portal or
// obstacle volumes after those tags have been applied by the map adapter.
std::size_t AddDynamicVolumeReachabilities(File& file,
                                           const Settings& settings);

bool ValidateFile(const File& file);
const char* ErrorName(Error error);

} // namespace aas2
} // namespace compilers
} // namespace darklight
