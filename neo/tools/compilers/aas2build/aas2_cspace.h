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
#include <cstdint>
#include <vector>

namespace darklight {
namespace compilers {
namespace aas2 {

// Plane convention: points inside a convex brush satisfy
// Dot(plane.normal, point) <= plane.distance.
struct BrushPlane {
    Vec3 normal{};
    float distance = 0.0f;
};

struct ConvexBrush {
    std::vector<BrushPlane> planes;
    std::uint32_t contents = 0;
    std::uint32_t sourceEntity = 0;
    std::uint32_t sourcePrimitive = 0;
};

struct AgentBounds {
    Vec3 mins{};
    Vec3 maxs{};
};

enum class CSpaceError {
    none,
    invalidAgentBounds,
    emptyBrush,
    invalidPlane
};

struct CSpaceResult {
    CSpaceError error = CSpaceError::none;
    std::size_t sourceBrush = 0;
    std::size_t sourcePlane = 0;
    std::size_t brushes = 0;
    std::size_t planes = 0;

    explicit operator bool() const { return error == CSpaceError::none; }
};

AgentBounds MakeAgentBounds(float radius, float height);

// Builds obstacle configuration space for an agent whose origin is translated
// through the world. This is the convex-brush Minkowski expansion that precedes
// BSP construction; brush contents and source provenance are preserved.
CSpaceResult ExpandConfigurationSpace(const std::vector<ConvexBrush>& input,
                                      const AgentBounds& agent,
                                      std::vector<ConvexBrush>& output);

const char* CSpaceErrorName(CSpaceError error);

} // namespace aas2
} // namespace compilers
} // namespace darklight



