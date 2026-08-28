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

#include <cstddef>
#include <cstdint>
#include <string>
#include <vector>

namespace darklight {
namespace compilers {
namespace aas2 {

using ProgressCallback = void (*)(const char* stage,
                                  std::size_t current,
                                  std::size_t total,
                                  void* userData);

struct Vec3 {
    float x = 0.0f;
    float y = 0.0f;
    float z = 0.0f;
};

struct Bounds {
    Vec3 mins{};
    Vec3 maxs{};
};

struct SourceTriangle {
    std::uint32_t vertices[3]{};
    std::uint32_t flags = 0;
};

struct SourceGeometry {
    std::vector<Vec3> vertices;
    std::vector<SourceTriangle> triangles;
};

enum AreaFlags : std::uint16_t {
    AAS2_AREA_LEDGE = 0x0001u,
    AAS2_AREA_OUTSIDE = 0x0004u,
    AAS2_AREA_HIGH_CEILING = 0x0008u,
    AAS2_AREA_NO_PUSH = 0x0010u,
    AAS2_AREA_CLUSTER_PORTAL = 0x0400u,
    AAS2_AREA_OBSTACLE = 0x0800u
};

enum TravelFlags : std::uint32_t {
    AAS2_TRAVEL_WALK = 0x00000020u,
    AAS2_TRAVEL_WALK_OFF_LEDGE = 0x00000040u,
    AAS2_TRAVEL_LEDGE_GRAB = 0x00000080u,
    AAS2_TRAVEL_WATER_JUMP = 0x00000100u,
    AAS2_TRAVEL_LADDER = 0x00000400u,
    // Darklight retains flying navigation.  This bit is outside the recovered
    // Wolf walk/step/ledge range and is translated at the idAAS boundary.
    AAS2_TRAVEL_FLY = 0x00000800u,
    AAS2_TRAVEL_SPECIAL = 0x02000000u
};

struct Edge {
    std::uint32_t vertices[2]{};
    std::uint32_t flags = 0;
};

struct Reachability {
    std::uint32_t fromArea = 0;
    std::uint32_t toArea = 0;
    std::uint32_t travelFlags = 0;
    Vec3 start{};
    Vec3 end{};
    std::string name;
};

struct Area {
    Bounds bounds{};
    Vec3 center{};
    Vec3 floorNormal{0.0f, 0.0f, 1.0f};
    std::uint16_t flags = 0;
    std::uint32_t travelFlags = 0;
    std::uint32_t tree = 0;
    std::vector<std::uint32_t> edges;
    std::vector<std::uint32_t> reachabilities;
};

struct Node {
    Vec3 normal{};
    float distance = 0.0f;
    std::int32_t children[2]{};
    Bounds bounds{};
};

struct Tree {
    std::int32_t rootNode = -1;
    Vec3 floorNormal{0.0f, 0.0f, 1.0f};
    std::vector<std::uint32_t> areas;
};

struct File {
    std::vector<Vec3> vertices;
    std::vector<Edge> edges;
    std::vector<Area> areas;
    std::vector<Node> nodes;
    std::vector<Reachability> reachabilities;
    std::vector<Tree> trees;
};

} // namespace aas2
} // namespace compilers
} // namespace darklight

