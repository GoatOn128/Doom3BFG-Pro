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


#include "aas2_tree.h"

#include <algorithm>
#include <cstdint>
#include <limits>
#include <vector>

namespace darklight {
namespace compilers {
namespace aas2 {
namespace {

float CenterAxis(const Area& area, int axis) {
    return axis == 0 ? area.center.x : axis == 1 ? area.center.y : area.center.z;
}

Bounds AreaListBounds(const File& file, const std::vector<std::uint32_t>& areas) {
    Bounds bounds;
    bounds.mins = {std::numeric_limits<float>::max(),
                   std::numeric_limits<float>::max(),
                   std::numeric_limits<float>::max()};
    bounds.maxs = {-std::numeric_limits<float>::max(),
                   -std::numeric_limits<float>::max(),
                   -std::numeric_limits<float>::max()};
    for (std::uint32_t areaIndex : areas) {
        const Bounds& area = file.areas[areaIndex].bounds;
        bounds.mins.x = std::min(bounds.mins.x, area.mins.x);
        bounds.mins.y = std::min(bounds.mins.y, area.mins.y);
        bounds.mins.z = std::min(bounds.mins.z, area.mins.z);
        bounds.maxs.x = std::max(bounds.maxs.x, area.maxs.x);
        bounds.maxs.y = std::max(bounds.maxs.y, area.maxs.y);
        bounds.maxs.z = std::max(bounds.maxs.z, area.maxs.z);
    }
    return bounds;
}

std::int32_t BuildNode(File& file, std::vector<std::uint32_t> areas) {
    if (areas.size() == 1) {
        return -static_cast<std::int32_t>(areas.front()) - 1;
    }
    const Bounds bounds = AreaListBounds(file, areas);
    const float extents[3] = {
        bounds.maxs.x - bounds.mins.x,
        bounds.maxs.y - bounds.mins.y,
        bounds.maxs.z - bounds.mins.z
    };
    int axis = extents[1] > extents[0] ? 1 : 0;
    if (extents[2] > extents[axis]) {
        axis = 2;
    }
    std::stable_sort(areas.begin(), areas.end(),
        [&](std::uint32_t left, std::uint32_t right) {
            const float leftCenter = CenterAxis(file.areas[left], axis);
            const float rightCenter = CenterAxis(file.areas[right], axis);
            return leftCenter == rightCenter ? left < right : leftCenter < rightCenter;
        });
    const std::size_t middle = areas.size() / 2;
    std::vector<std::uint32_t> back(areas.begin(), areas.begin() + middle);
    std::vector<std::uint32_t> front(areas.begin() + middle, areas.end());

    Node node;
    node.bounds = bounds;
    if (axis == 0) node.normal.x = 1.0f;
    if (axis == 1) node.normal.y = 1.0f;
    if (axis == 2) node.normal.z = 1.0f;
    node.distance = (CenterAxis(file.areas[back.back()], axis) +
                     CenterAxis(file.areas[front.front()], axis)) * 0.5f;
    const std::int32_t nodeIndex = static_cast<std::int32_t>(file.nodes.size());
    file.nodes.push_back(node);
    file.nodes[nodeIndex].children[0] = BuildNode(file, std::move(front));
    file.nodes[nodeIndex].children[1] = BuildNode(file, std::move(back));
    return nodeIndex;
}

} // namespace

void BuildAreaTree(File& file) {
    file.nodes.clear();
    file.trees.clear();
    Tree tree;
    tree.areas.reserve(file.areas.size());
    for (std::size_t area = 0; area < file.areas.size(); ++area) {
        tree.areas.push_back(static_cast<std::uint32_t>(area));
        file.areas[area].tree = 0;
    }
    if (!tree.areas.empty()) {
        tree.rootNode = BuildNode(file, tree.areas);
    }
    file.trees.push_back(std::move(tree));
}

} // namespace aas2
} // namespace compilers
} // namespace darklight



