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


#include "aas2_cspace.h"

#include <algorithm>
#include <cmath>
#include <utility>

namespace darklight {
namespace compilers {
namespace aas2 {
namespace {

bool Finite(float value) {
    return std::isfinite(value) != 0;
}

bool Finite(const Vec3& value) {
    return Finite(value.x) && Finite(value.y) && Finite(value.z);
}

float Length(const Vec3& value) {
    return std::sqrt(value.x * value.x +
                     value.y * value.y +
                     value.z * value.z);
}

float ReflectedBoundsSupport(const AgentBounds& bounds, const Vec3& normal) {
    const float x = normal.x >= 0.0f ? -bounds.mins.x : -bounds.maxs.x;
    const float y = normal.y >= 0.0f ? -bounds.mins.y : -bounds.maxs.y;
    const float z = normal.z >= 0.0f ? -bounds.mins.z : -bounds.maxs.z;
    return normal.x * x + normal.y * y + normal.z * z;
}

} // namespace

AgentBounds MakeAgentBounds(float radius, float height) {
    const float safeRadius = std::max(0.0f, radius);
    const float safeHeight = std::max(0.0f, height);
    AgentBounds bounds;
    bounds.mins = {-safeRadius, -safeRadius, 0.0f};
    bounds.maxs = {safeRadius, safeRadius, safeHeight};
    return bounds;
}

CSpaceResult ExpandConfigurationSpace(const std::vector<ConvexBrush>& input,
                                      const AgentBounds& agent,
                                      std::vector<ConvexBrush>& output) {
    output.clear();
    CSpaceResult result;
    if (!Finite(agent.mins) || !Finite(agent.maxs) ||
        agent.mins.x > agent.maxs.x ||
        agent.mins.y > agent.maxs.y ||
        agent.mins.z > agent.maxs.z) {
        result.error = CSpaceError::invalidAgentBounds;
        return result;
    }

    output.reserve(input.size());
    for (std::size_t brushIndex = 0; brushIndex < input.size(); ++brushIndex) {
        const ConvexBrush& source = input[brushIndex];
        if (source.planes.empty()) {
            output.clear();
            result.error = CSpaceError::emptyBrush;
            result.sourceBrush = brushIndex;
            return result;
        }
        ConvexBrush expanded;
        expanded.contents = source.contents;
        expanded.sourceEntity = source.sourceEntity;
        expanded.sourcePrimitive = source.sourcePrimitive;
        expanded.planes.reserve(source.planes.size());
        for (std::size_t planeIndex = 0;
             planeIndex < source.planes.size(); ++planeIndex) {
            const BrushPlane& sourcePlane = source.planes[planeIndex];
            const float normalLength = Length(sourcePlane.normal);
            if (!Finite(sourcePlane.normal) || !Finite(sourcePlane.distance) ||
                !Finite(normalLength) || normalLength <= 1.0e-8f) {
                output.clear();
                result.error = CSpaceError::invalidPlane;
                result.sourceBrush = brushIndex;
                result.sourcePlane = planeIndex;
                return result;
            }

            BrushPlane plane;
            plane.normal = {sourcePlane.normal.x / normalLength,
                            sourcePlane.normal.y / normalLength,
                            sourcePlane.normal.z / normalLength};
            plane.distance = sourcePlane.distance / normalLength;
            plane.distance += ReflectedBoundsSupport(agent, plane.normal);
            expanded.planes.push_back(plane);
            ++result.planes;
        }
        output.push_back(std::move(expanded));
    }
    result.brushes = output.size();
    return result;
}

const char* CSpaceErrorName(CSpaceError error) {
    switch (error) {
        case CSpaceError::none: return "none";
        case CSpaceError::invalidAgentBounds: return "invalid agent bounds";
        case CSpaceError::emptyBrush: return "empty brush";
        case CSpaceError::invalidPlane: return "invalid brush plane";
    }
    return "unknown";
}

} // namespace aas2
} // namespace compilers
} // namespace darklight



