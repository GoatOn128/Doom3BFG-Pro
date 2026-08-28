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


#include "aas2_floor_merge.h"

#include <algorithm>
#include <cmath>
#include <cstdint>
#include <limits>
#include <unordered_map>
#include <utility>
#include <vector>

namespace darklight {
namespace compilers {
namespace aas2 {
namespace {

float Dot(const Vec3& left, const Vec3& right) {
    return left.x * right.x + left.y * right.y + left.z * right.z;
}

Vec3 Add(const Vec3& left, const Vec3& right) {
    return {left.x + right.x, left.y + right.y, left.z + right.z};
}

Vec3 Scale(const Vec3& value, float scale) {
    return {value.x * scale, value.y * scale, value.z * scale};
}

Vec3 Normalize(const Vec3& value) {
    const float length = std::sqrt(Dot(value, value));
    return length > 1.0e-12f ? Scale(value, 1.0f / length) : Vec3{};
}

struct DisjointSet {
    explicit DisjointSet(std::size_t count) : parents(count), groups(count) {
        for (std::size_t i = 0; i < count; ++i) {
            parents[i] = static_cast<std::uint32_t>(i);
            groups[i].push_back(static_cast<std::uint32_t>(i));
        }
    }

    std::uint32_t Find(std::uint32_t value) {
        if (parents[value] != value) {
            parents[value] = Find(parents[value]);
        }
        return parents[value];
    }

    void Join(std::uint32_t left, std::uint32_t right) {
        left = Find(left);
        right = Find(right);
        if (left != right) {
            if (groups[left].size() < groups[right].size()) {
                std::swap(left, right);
            }
            parents[right] = left;
            groups[left].insert(groups[left].end(),
                                groups[right].begin(), groups[right].end());
            groups[right].clear();
        }
    }

    std::vector<std::uint32_t> Combined(std::uint32_t left,
                                        std::uint32_t right) {
        left = Find(left);
        right = Find(right);
        std::vector<std::uint32_t> members;
        members.reserve(groups[left].size() + groups[right].size());
        members.insert(members.end(), groups[left].begin(), groups[left].end());
        members.insert(members.end(), groups[right].begin(), groups[right].end());
        return members;
    }

    std::vector<std::uint32_t> parents;
    std::vector<std::vector<std::uint32_t>> groups;
};

bool Coplanar(const File& file,
              const std::vector<std::uint32_t>& members,
              float minimumNormalDot,
              float distanceEpsilon) {
    const Area& reference = file.areas[members.front()];
    const Vec3 normal = Normalize(reference.floorNormal);
    const float distance = Dot(normal, reference.center);
    for (std::uint32_t member : members) {
        const Area& area = file.areas[member];
        if (Dot(normal, Normalize(area.floorNormal)) < minimumNormalDot) {
            return false;
        }
        for (std::uint32_t edgeIndex : area.edges) {
            const Edge& edge = file.edges[edgeIndex];
            for (int endpoint = 0; endpoint < 2; ++endpoint) {
                if (std::fabs(Dot(normal, file.vertices[edge.vertices[endpoint]]) -
                              distance) > distanceEpsilon) {
                    return false;
                }
            }
        }
    }
    return true;
}

struct Boundary {
    std::vector<std::uint32_t> edges;
    std::vector<std::uint32_t> vertices;
};

bool BuildBoundary(const File& file,
                   const std::vector<std::uint32_t>& members,
                   Boundary& boundary) {
    std::unordered_map<std::uint32_t, std::size_t> edgeCounts;
    for (std::uint32_t member : members) {
        for (std::uint32_t edge : file.areas[member].edges) {
            ++edgeCounts[edge];
        }
    }

    std::unordered_map<std::uint32_t, std::vector<std::uint32_t>> adjacency;
    for (const auto& entry : edgeCounts) {
        if (entry.second > 2) {
            return false;
        }
        if (entry.second == 1) {
            const Edge& edge = file.edges[entry.first];
            boundary.edges.push_back(entry.first);
            adjacency[edge.vertices[0]].push_back(entry.first);
            adjacency[edge.vertices[1]].push_back(entry.first);
        }
    }
    if (boundary.edges.size() < 3) {
        return false;
    }
    for (const auto& entry : adjacency) {
        if (entry.second.size() != 2) {
            return false;
        }
    }

    const std::uint32_t firstEdge = boundary.edges.front();
    std::uint32_t currentVertex = file.edges[firstEdge].vertices[0];
    std::uint32_t currentEdge = firstEdge;
    const std::uint32_t startVertex = currentVertex;
    boundary.vertices.clear();
    boundary.edges.clear();
    do {
        boundary.vertices.push_back(currentVertex);
        boundary.edges.push_back(currentEdge);
        const Edge& edge = file.edges[currentEdge];
        currentVertex = edge.vertices[0] == currentVertex ?
            edge.vertices[1] : edge.vertices[0];
        const std::vector<std::uint32_t>& incident = adjacency[currentVertex];
        currentEdge = incident[0] == currentEdge ? incident[1] : incident[0];
        if (boundary.edges.size() > adjacency.size()) {
            return false;
        }
    } while (currentVertex != startVertex);
    return boundary.edges.size() == adjacency.size() && currentEdge == firstEdge;
}

void Project(const Vec3& point, int droppedAxis, float& first, float& second) {
    if (droppedAxis == 0) {
        first = point.y;
        second = point.z;
    } else if (droppedAxis == 1) {
        first = point.x;
        second = point.z;
    } else {
        first = point.x;
        second = point.y;
    }
}

bool ConvexBoundary(const File& file,
                    const Boundary& boundary,
                    const Vec3& normal) {
    const float components[3] = {
        std::fabs(normal.x), std::fabs(normal.y), std::fabs(normal.z)
    };
    const int droppedAxis = components[0] > components[1] ?
        (components[0] > components[2] ? 0 : 2) :
        (components[1] > components[2] ? 1 : 2);
    float sign = 0.0f;
    for (std::size_t i = 0; i < boundary.vertices.size(); ++i) {
        const Vec3& a = file.vertices[boundary.vertices[i]];
        const Vec3& b = file.vertices[boundary.vertices[(i + 1) % boundary.vertices.size()]];
        const Vec3& c = file.vertices[boundary.vertices[(i + 2) % boundary.vertices.size()]];
        float ax, ay, bx, by, cx, cy;
        Project(a, droppedAxis, ax, ay);
        Project(b, droppedAxis, bx, by);
        Project(c, droppedAxis, cx, cy);
        const float cross = (bx - ax) * (cy - by) - (by - ay) * (cx - bx);
        if (std::fabs(cross) <= 1.0e-5f) {
            continue;
        }
        if (sign == 0.0f) {
            sign = cross;
        } else if ((sign > 0.0f) != (cross > 0.0f)) {
            return false;
        }
    }
    return sign != 0.0f;
}

void RebuildAreas(File& file, DisjointSet& sets) {
    std::unordered_map<std::uint32_t, std::vector<std::uint32_t>> groups;
    for (std::size_t area = 0; area < file.areas.size(); ++area) {
        groups[sets.Find(static_cast<std::uint32_t>(area))].push_back(
            static_cast<std::uint32_t>(area));
    }

    std::vector<Area> rebuilt;
    rebuilt.reserve(groups.size());
    for (const auto& entry : groups) {
        Boundary boundary;
        if (!BuildBoundary(file, entry.second, boundary)) {
            continue;
        }
        Area area = file.areas[entry.second.front()];
        area.edges = boundary.edges;
        area.reachabilities.clear();
        area.center = Vec3{};
        area.floorNormal = Vec3{};
        area.bounds.mins = {std::numeric_limits<float>::max(),
                            std::numeric_limits<float>::max(),
                            std::numeric_limits<float>::max()};
        area.bounds.maxs = {-std::numeric_limits<float>::max(),
                            -std::numeric_limits<float>::max(),
                            -std::numeric_limits<float>::max()};
        for (std::uint32_t vertexIndex : boundary.vertices) {
            const Vec3& vertex = file.vertices[vertexIndex];
            area.center = Add(area.center, vertex);
            area.bounds.mins.x = std::min(area.bounds.mins.x, vertex.x);
            area.bounds.mins.y = std::min(area.bounds.mins.y, vertex.y);
            area.bounds.mins.z = std::min(area.bounds.mins.z, vertex.z);
            area.bounds.maxs.x = std::max(area.bounds.maxs.x, vertex.x);
            area.bounds.maxs.y = std::max(area.bounds.maxs.y, vertex.y);
            area.bounds.maxs.z = std::max(area.bounds.maxs.z, vertex.z);
        }
        area.center = Scale(area.center, 1.0f / boundary.vertices.size());
        for (std::uint32_t member : entry.second) {
            area.floorNormal = Add(area.floorNormal, file.areas[member].floorNormal);
        }
        area.floorNormal = Normalize(area.floorNormal);
        rebuilt.push_back(std::move(area));
    }
    file.areas = std::move(rebuilt);

    std::vector<std::uint32_t> remap(file.edges.size(),
                                     std::numeric_limits<std::uint32_t>::max());
    std::vector<Edge> compactEdges;
    for (Area& area : file.areas) {
        for (std::uint32_t& edge : area.edges) {
            if (remap[edge] == std::numeric_limits<std::uint32_t>::max()) {
                remap[edge] = static_cast<std::uint32_t>(compactEdges.size());
                compactEdges.push_back(file.edges[edge]);
            }
            edge = remap[edge];
        }
    }
    file.edges = std::move(compactEdges);
}

} // namespace

FloorMergeResult MergeCoplanarFloors(File& file,
                                     float normalDegrees,
                                     float planeDistanceEpsilon) {
    FloorMergeResult result;
    if (file.areas.size() < 2) {
        return result;
    }

    std::vector<std::vector<std::uint32_t>> owners(file.edges.size());
    for (std::size_t area = 0; area < file.areas.size(); ++area) {
        for (std::uint32_t edge : file.areas[area].edges) {
            owners[edge].push_back(static_cast<std::uint32_t>(area));
        }
    }
    std::vector<std::pair<std::uint32_t, std::uint32_t>> candidates;
    for (const auto& edgeOwners : owners) {
        if (edgeOwners.size() == 2) {
            candidates.emplace_back(edgeOwners[0], edgeOwners[1]);
        }
    }

    const float clampedDegrees = std::max(0.0f, std::min(90.0f, normalDegrees));
    const float minimumNormalDot =
        std::cos(clampedDegrees * 3.14159265358979323846f / 180.0f);
    const float distanceEpsilon = std::max(0.0f, planeDistanceEpsilon);
    DisjointSet sets(file.areas.size());
    bool changed = true;
    while (changed) {
        changed = false;
        for (const auto& candidate : candidates) {
            const std::uint32_t firstRoot = sets.Find(candidate.first);
            const std::uint32_t secondRoot = sets.Find(candidate.second);
            if (firstRoot == secondRoot) {
                continue;
            }
            const std::vector<std::uint32_t> members =
                sets.Combined(firstRoot, secondRoot);
            Boundary boundary;
            if (!Coplanar(file, members, minimumNormalDot, distanceEpsilon) ||
                !BuildBoundary(file, members, boundary) ||
                !ConvexBoundary(file, boundary,
                                file.areas[members.front()].floorNormal)) {
                continue;
            }
            sets.Join(firstRoot, secondRoot);
            ++result.mergedPairs;
            changed = true;
        }
    }

    if (result.mergedPairs != 0) {
        RebuildAreas(file, sets);
    }
    return result;
}

} // namespace aas2
} // namespace compilers
} // namespace darklight


