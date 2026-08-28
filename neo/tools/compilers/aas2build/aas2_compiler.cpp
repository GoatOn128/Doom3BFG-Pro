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


#include "aas2_compiler.h"
#include "aas2_floor_merge.h"
#include "aas2_tree.h"

#include <algorithm>
#include <cmath>
#include <cstdint>
#include <limits>
#include <unordered_map>
#include <unordered_set>
#include <utility>
#include <vector>

namespace darklight {
namespace compilers {
namespace aas2 {
namespace {

void ReportProgress(ProgressCallback callback, void* userData,
                    const char* stage, std::size_t current,
                    std::size_t total) {
    if (callback == nullptr) {
        return;
    }
    const std::size_t interval = std::max<std::size_t>(1, total / 100);
    if (current == 0 || current == total || current % interval == 0) {
        callback(stage, current, total, userData);
    }
}

Vec3 Subtract(const Vec3& left, const Vec3& right) {
    return {left.x - right.x, left.y - right.y, left.z - right.z};
}

Vec3 Cross(const Vec3& left, const Vec3& right) {
    return {
        left.y * right.z - left.z * right.y,
        left.z * right.x - left.x * right.z,
        left.x * right.y - left.y * right.x
    };
}

float Length(const Vec3& value) {
    return std::sqrt(value.x * value.x + value.y * value.y + value.z * value.z);
}

Vec3 Scale(const Vec3& value, float scale) {
    return {value.x * scale, value.y * scale, value.z * scale};
}

Vec3 Normalize(const Vec3& value) {
    const float length = Length(value);
    return length > 1.0e-12f ? Scale(value, 1.0f / length) : Vec3{};
}

Vec3 TriangleCenter(const Vec3& a, const Vec3& b, const Vec3& c) {
    return {(a.x + b.x + c.x) / 3.0f,
            (a.y + b.y + c.y) / 3.0f,
            (a.z + b.z + c.z) / 3.0f};
}

Bounds TriangleBounds(const Vec3& a, const Vec3& b, const Vec3& c) {
    Bounds bounds;
    bounds.mins = {std::min(a.x, std::min(b.x, c.x)),
                   std::min(a.y, std::min(b.y, c.y)),
                   std::min(a.z, std::min(b.z, c.z))};
    bounds.maxs = {std::max(a.x, std::max(b.x, c.x)),
                   std::max(a.y, std::max(b.y, c.y)),
                   std::max(a.z, std::max(b.z, c.z))};
    return bounds;
}

bool Near(const Vec3& left, const Vec3& right, float epsilon) {
    return std::fabs(left.x - right.x) <= epsilon &&
           std::fabs(left.y - right.y) <= epsilon &&
           std::fabs(left.z - right.z) <= epsilon;
}

struct VertexCell {
    long long x;
    long long y;
    long long z;
    bool operator==(const VertexCell& other) const {
        return x == other.x && y == other.y && z == other.z;
    }
};

struct VertexCellHash {
    std::size_t operator()(const VertexCell& cell) const {
        const std::size_t first = std::hash<long long>()(cell.x);
        const std::size_t second = std::hash<long long>()(cell.y);
        const std::size_t third = std::hash<long long>()(cell.z);
        return first ^ (second + 0x9e3779b9u + (first << 6) + (first >> 2)) ^
               (third + 0x9e3779b9u + (second << 6) + (second >> 2));
    }
};

class VertexWelder {
public:
    VertexWelder(std::vector<Vec3>& output, float tolerance,
                 std::size_t expectedVertices)
        : vertices(output), epsilon(std::max(0.0f, tolerance)),
          cellSize(std::max(1.0e-6f, tolerance)) {
        buckets.reserve(expectedVertices);
    }

    std::uint32_t Weld(const Vec3& vertex) {
        const VertexCell center = Cell(vertex);
        for (long long x = center.x - 1; x <= center.x + 1; ++x) {
            for (long long y = center.y - 1; y <= center.y + 1; ++y) {
                for (long long z = center.z - 1; z <= center.z + 1; ++z) {
                    const auto range = buckets.equal_range(VertexCell{x, y, z});
                    for (auto found = range.first; found != range.second; ++found) {
                        if (Near(vertex, vertices[found->second], epsilon)) {
                            return found->second;
                        }
                    }
                }
            }
        }
        const std::uint32_t index = static_cast<std::uint32_t>(vertices.size());
        vertices.push_back(vertex);
        buckets.emplace(center, index);
        return index;
    }

private:
    VertexCell Cell(const Vec3& vertex) const {
        return {static_cast<long long>(std::floor(vertex.x / cellSize)),
                static_cast<long long>(std::floor(vertex.y / cellSize)),
                static_cast<long long>(std::floor(vertex.z / cellSize))};
    }

    std::vector<Vec3>& vertices;
    float epsilon;
    float cellSize;
    std::unordered_multimap<VertexCell, std::uint32_t, VertexCellHash> buckets;
};

std::uint64_t EdgeKey(std::uint32_t first, std::uint32_t second) {
    const std::uint32_t low = std::min(first, second);
    const std::uint32_t high = std::max(first, second);
    return (static_cast<std::uint64_t>(low) << 32) | high;
}

int GridCell(float coordinate, float cellSize) {
    return static_cast<int>(std::floor(coordinate / cellSize));
}

std::uint64_t GridKey(int x, int y) {
    return (static_cast<std::uint64_t>(static_cast<std::uint32_t>(x)) << 32) |
           static_cast<std::uint32_t>(y);
}

struct EdgeUse {
    std::uint32_t edge = 0;
    std::vector<std::uint32_t> areas;
};

struct TriangleKey {
    std::uint32_t vertices[3]{};

    bool operator==(const TriangleKey& other) const {
        return vertices[0] == other.vertices[0] &&
               vertices[1] == other.vertices[1] &&
               vertices[2] == other.vertices[2];
    }
};

struct TriangleKeyHash {
    std::size_t operator()(const TriangleKey& key) const {
        std::size_t value = std::hash<std::uint32_t>()(key.vertices[0]);
        for (int i = 1; i < 3; ++i) {
            const std::size_t next =
                std::hash<std::uint32_t>()(key.vertices[i]);
            value ^= next + 0x9e3779b9u + (value << 6) + (value >> 2);
        }
        return value;
    }
};

bool VerticalTransitionPoints(const Edge& firstEdge,
                              const Edge& secondEdge,
                              const std::vector<Vec3>& vertices,
                              float epsilon,
                              float maxHorizontalGap,
                              Vec3& firstPoint,
                              Vec3& secondPoint) {
    const Vec3& a = vertices[firstEdge.vertices[0]];
    const Vec3& b = vertices[firstEdge.vertices[1]];
    const Vec3& c = vertices[secondEdge.vertices[0]];
    const Vec3& d = vertices[secondEdge.vertices[1]];
    const float abX = b.x - a.x;
    const float abY = b.y - a.y;
    const float cdX = d.x - c.x;
    const float cdY = d.y - c.y;
    const float abLengthSquared = abX * abX + abY * abY;
    const float cdLengthSquared = cdX * cdX + cdY * cdY;
    if (abLengthSquared <= epsilon * epsilon ||
        cdLengthSquared <= epsilon * epsilon) {
        return false;
    }
    const float directionCross = abX * cdY - abY * cdX;
    const float offsetCross = (c.x - a.x) * abY - (c.y - a.y) * abX;
    if (std::fabs(directionCross) > epsilon *
            std::sqrt(abLengthSquared * cdLengthSquared) ||
        std::fabs(offsetCross) > maxHorizontalGap *
            std::sqrt(abLengthSquared)) {
        return false;
    }

    const float cParameter = ((c.x - a.x) * abX + (c.y - a.y) * abY) /
                             abLengthSquared;
    const float dParameter = ((d.x - a.x) * abX + (d.y - a.y) * abY) /
                             abLengthSquared;
    const float overlapStart = std::max(0.0f, std::min(cParameter, dParameter));
    const float overlapEnd = std::min(1.0f, std::max(cParameter, dParameter));
    if ((overlapEnd - overlapStart) * std::sqrt(abLengthSquared) <= epsilon) {
        return false;
    }
    const float firstParameter = (overlapStart + overlapEnd) * 0.5f;
    firstPoint = {a.x + (b.x - a.x) * firstParameter,
                  a.y + (b.y - a.y) * firstParameter,
                  a.z + (b.z - a.z) * firstParameter};
    const float secondParameter =
        ((firstPoint.x - c.x) * cdX + (firstPoint.y - c.y) * cdY) /
        cdLengthSquared;
    secondPoint = {c.x + (d.x - c.x) * secondParameter,
                   c.y + (d.y - c.y) * secondParameter,
                   c.z + (d.z - c.z) * secondParameter};
    // The spans already overlap. Identical transition points are valid for a
    // coplanar T-junction whose two sides use different edge subdivisions.
    return true;
}

// Return the closest points of two finite floor-boundary segments in XY.
bool ClosestTransitionPoints(const Edge& firstEdge,
                             const Edge& secondEdge,
                             const std::vector<Vec3>& vertices,
                             float maxHorizontalGap,
                             Vec3& firstPoint,
                             Vec3& secondPoint) {
    const Vec3& a = vertices[firstEdge.vertices[0]];
    const Vec3& b = vertices[firstEdge.vertices[1]];
    const Vec3& c = vertices[secondEdge.vertices[0]];
    const Vec3& d = vertices[secondEdge.vertices[1]];
    const float ux = b.x - a.x;
    const float uy = b.y - a.y;
    const float vx = d.x - c.x;
    const float vy = d.y - c.y;
    const float wx = a.x - c.x;
    const float wy = a.y - c.y;
    const float uu = ux * ux + uy * uy;
    const float vv = vx * vx + vy * vy;
    const float uv = ux * vx + uy * vy;
    const float uw = ux * wx + uy * wy;
    const float vw = vx * wx + vy * wy;
    if (uu <= 1.0e-8f || vv <= 1.0e-8f) {
        return false;
    }

    float firstParameter = 0.0f;
    float secondParameter = 0.0f;
    const float denominator = uu * vv - uv * uv;
    if (std::fabs(denominator) > 1.0e-8f * uu * vv) {
        firstParameter = (uv * vw - vv * uw) / denominator;
    }
    firstParameter = std::max(0.0f, std::min(1.0f, firstParameter));
    secondParameter = (uv * firstParameter + vw) / vv;
    if (secondParameter < 0.0f) {
        secondParameter = 0.0f;
        firstParameter = std::max(0.0f, std::min(1.0f, -uw / uu));
    } else if (secondParameter > 1.0f) {
        secondParameter = 1.0f;
        firstParameter = std::max(0.0f, std::min(1.0f, (uv - uw) / uu));
    }

    firstPoint = {a.x + ux * firstParameter,
                  a.y + uy * firstParameter,
                  a.z + (b.z - a.z) * firstParameter};
    secondPoint = {c.x + vx * secondParameter,
                   c.y + vy * secondParameter,
                   c.z + (d.z - c.z) * secondParameter};
    const float dx = firstPoint.x - secondPoint.x;
    const float dy = firstPoint.y - secondPoint.y;
    return dx * dx + dy * dy <= maxHorizontalGap * maxHorizontalGap;
}

void AddReachability(File& output,
                     std::uint32_t fromArea,
                     std::uint32_t toArea,
                     const Vec3& start,
                     const Vec3& end,
                     std::uint32_t travelFlags) {
    Reachability reachability;
    reachability.fromArea = fromArea;
    reachability.toArea = toArea;
    reachability.travelFlags = travelFlags;
    reachability.start = start;
    reachability.end = end;
    const std::uint32_t index =
        static_cast<std::uint32_t>(output.reachabilities.size());
    output.reachabilities.push_back(std::move(reachability));
    output.areas[fromArea].reachabilities.push_back(index);
}

void AddReachabilityPair(File& output,
                         std::uint32_t firstArea,
                         std::uint32_t secondArea,
                         const Vec3& firstPoint,
                         const Vec3& secondPoint) {
    const std::uint32_t areas[2] = {firstArea, secondArea};
    const Vec3 points[2] = {firstPoint, secondPoint};
    for (int direction = 0; direction < 2; ++direction) {
        AddReachability(output, areas[direction], areas[1 - direction],
                        points[direction], points[1 - direction],
                        AAS2_TRAVEL_WALK);
    }
}

} // namespace

Result CompileSurface(const SourceGeometry& source,
                      const Settings& settings,
                      File& output,
                      ProgressCallback progress,
                      void* progressUserData) {
    output = File{};
    Result result;
    result.statistics.inputTriangles = source.triangles.size();

    if (source.vertices.empty() || source.triangles.empty()) {
        result.error = Error::emptyGeometry;
        return result;
    }

    const float clampedSlope =
        std::max(0.0f, std::min(89.9f, settings.maxFloorSlopeDegrees));
    const float minFloorZ =
        std::cos(clampedSlope * 3.14159265358979323846f / 180.0f);
    const float weldEpsilon = std::max(0.0f, settings.weldEpsilon);

    std::unordered_map<std::uint64_t, EdgeUse> edgeUses;
    std::unordered_set<TriangleKey, TriangleKeyHash> triangleUses;
    triangleUses.reserve(source.triangles.size());
    VertexWelder vertexWelder(output.vertices, weldEpsilon,
                              source.vertices.size());
    ReportProgress(progress, progressUserData, "area graph",
                   0, source.triangles.size());
    for (std::size_t triangleIndex = 0;
         triangleIndex < source.triangles.size(); ++triangleIndex) {
        ReportProgress(progress, progressUserData, "area graph",
                       triangleIndex, source.triangles.size());
        const SourceTriangle& triangle = source.triangles[triangleIndex];
        for (int corner = 0; corner < 3; ++corner) {
            if (triangle.vertices[corner] >= source.vertices.size()) {
                result.error = Error::invalidVertexIndex;
                result.sourceTriangle = triangleIndex;
                return result;
            }
        }

        const Vec3& sourceA = source.vertices[triangle.vertices[0]];
        const Vec3& sourceB = source.vertices[triangle.vertices[1]];
        const Vec3& sourceC = source.vertices[triangle.vertices[2]];
        const Vec3 normal = Normalize(Cross(Subtract(sourceB, sourceA),
                                            Subtract(sourceC, sourceA)));
        if (Length(normal) <= 1.0e-8f) {
            ++result.statistics.rejectedDegenerateTriangles;
            continue;
        }
        if (normal.z < minFloorZ) {
            ++result.statistics.rejectedSteepTriangles;
            continue;
        }

        const std::uint32_t vertices[3] = {
            vertexWelder.Weld(sourceA),
            vertexWelder.Weld(sourceB),
            vertexWelder.Weld(sourceC)
        };
        if (vertices[0] == vertices[1] || vertices[0] == vertices[2] ||
            vertices[1] == vertices[2]) {
            ++result.statistics.rejectedDegenerateTriangles;
            continue;
        }

        TriangleKey triangleKey;
        triangleKey.vertices[0] = vertices[0];
        triangleKey.vertices[1] = vertices[1];
        triangleKey.vertices[2] = vertices[2];
        std::sort(triangleKey.vertices, triangleKey.vertices + 3);
        if (!triangleUses.insert(triangleKey).second) {
            ++result.statistics.rejectedDuplicateTriangles;
            continue;
        }

        Area area;
        area.center = TriangleCenter(output.vertices[vertices[0]],
                                     output.vertices[vertices[1]],
                                     output.vertices[vertices[2]]);
        area.bounds = TriangleBounds(output.vertices[vertices[0]],
                                     output.vertices[vertices[1]],
                                     output.vertices[vertices[2]]);
        area.floorNormal = normal;
        const std::uint32_t areaIndex = static_cast<std::uint32_t>(output.areas.size());

        for (int edgeCorner = 0; edgeCorner < 3; ++edgeCorner) {
            const std::uint32_t first = vertices[edgeCorner];
            const std::uint32_t second = vertices[(edgeCorner + 1) % 3];
            const std::uint64_t key = EdgeKey(first, second);
            auto found = edgeUses.find(key);
            if (found == edgeUses.end()) {
                Edge edge;
                edge.vertices[0] = std::min(first, second);
                edge.vertices[1] = std::max(first, second);
                const std::uint32_t edgeIndex =
                    static_cast<std::uint32_t>(output.edges.size());
                output.edges.push_back(edge);
                EdgeUse use;
                use.edge = edgeIndex;
                found = edgeUses.emplace(key, std::move(use)).first;
            }
            found->second.areas.push_back(areaIndex);
            area.edges.push_back(found->second.edge);
        }

        output.areas.push_back(std::move(area));
        ++result.statistics.walkableTriangles;
    }
    ReportProgress(progress, progressUserData, "area graph",
                   source.triangles.size(), source.triangles.size());

    if (output.areas.empty()) {
        result.error = Error::emptyGeometry;
        return result;
    }

    for (const auto& entry : edgeUses) {
        const EdgeUse& use = entry.second;
        if (use.areas.size() > 2) {
            ++result.statistics.nonManifoldEdges;
        }
    }

    if (settings.mergeCoplanarFloors) {
        ReportProgress(progress, progressUserData, "coplanar floor merge", 0, 1);
        result.statistics.mergedFloorPairs =
            MergeCoplanarFloors(output,
                                settings.floorMergeNormalDegrees,
                                settings.floorMergeDistanceEpsilon).mergedPairs;
        ReportProgress(progress, progressUserData, "coplanar floor merge", 1, 1);
    }

    std::vector<std::vector<std::uint32_t>> finalEdgeOwners(output.edges.size());
    for (std::size_t area = 0; area < output.areas.size(); ++area) {
        for (std::uint32_t edge : output.areas[area].edges) {
            finalEdgeOwners[edge].push_back(static_cast<std::uint32_t>(area));
        }
    }
    std::unordered_set<std::uint64_t> linkedAreaPairs;
    ReportProgress(progress, progressUserData, "shared-edge reachability",
                   0, finalEdgeOwners.size());
    for (std::size_t edgeIndex = 0;
         edgeIndex < finalEdgeOwners.size(); ++edgeIndex) {
        ReportProgress(progress, progressUserData, "shared-edge reachability",
                       edgeIndex, finalEdgeOwners.size());
        const std::vector<std::uint32_t>& owners = finalEdgeOwners[edgeIndex];
        if (owners.size() != 2) {
            continue;
        }
		const std::uint64_t pairKey = EdgeKey(owners[0], owners[1]);
		if (linkedAreaPairs.count(pairKey) != 0) {
			continue;
		}

        const Edge& edge = output.edges[edgeIndex];
        const Vec3& first = output.vertices[edge.vertices[0]];
        const Vec3& second = output.vertices[edge.vertices[1]];
        const Vec3 midpoint{(first.x + second.x) * 0.5f,
                            (first.y + second.y) * 0.5f,
                            (first.z + second.z) * 0.5f};

        AddReachabilityPair(output, owners[0], owners[1], midpoint, midpoint);
        linkedAreaPairs.insert(pairKey);
    }
    ReportProgress(progress, progressUserData, "shared-edge reachability",
                   finalEdgeOwners.size(), finalEdgeOwners.size());

    const float stepEpsilon = std::max(0.01f, weldEpsilon);
    // CSG clipping can leave neighboring coplanar floor boundaries a small
    // distance apart even though an agent can cross the seam. The old test
    // accepted only a vertical Z difference and therefore discarded every
    // coplanar seam that did not weld into one shared edge.
    const float seamTolerance = std::max(0.5f, weldEpsilon * 4.0f);
    const float maxStepHeight = std::max(0.0f, settings.maxStepHeight);
    // Configuration-space expansion bevels stair corners by the agent radius.
    // Adjacent treads can therefore have no shared/collinear edge even though
    // their vertical rise is a legal step. Search one agent radius sideways
    // for non-coplanar transitions; coplanar gaps remain restricted below to
    // seamTolerance so this cannot bridge through ordinary walls.
    const float stepHorizontalTolerance = std::max(
        seamTolerance,
        std::min(std::max(0.0f, settings.agentRadius), maxStepHeight));
    const float maxFallHeight = std::max(maxStepHeight, settings.maxFallHeight);
    const float maxLedgeGrabHeight = std::max(maxStepHeight, settings.agentHeight);
    const float reachCellSize = std::max(64.0f, settings.agentRadius * 4.0f);
    const int maximumEdgeCells = 4096;
    std::unordered_map<std::uint64_t, std::vector<std::uint32_t>> reachCells;
    std::vector<std::uint32_t> globalBoundaryEdges;
    std::vector<std::uint32_t> boundaryEdges;
    boundaryEdges.reserve(finalEdgeOwners.size());
    reachCells.reserve(finalEdgeOwners.size());
    for (std::size_t edgeIndex = 0; edgeIndex < finalEdgeOwners.size(); ++edgeIndex) {
        if (finalEdgeOwners[edgeIndex].size() != 1) {
            continue;
        }
        boundaryEdges.push_back(static_cast<std::uint32_t>(edgeIndex));
        const Edge& edge = output.edges[edgeIndex];
        const Vec3& first = output.vertices[edge.vertices[0]];
        const Vec3& second = output.vertices[edge.vertices[1]];
        const int minX = GridCell(std::min(first.x, second.x) - stepHorizontalTolerance,
                                  reachCellSize);
        const int maxX = GridCell(std::max(first.x, second.x) + stepHorizontalTolerance,
                                  reachCellSize);
        const int minY = GridCell(std::min(first.y, second.y) - stepHorizontalTolerance,
                                  reachCellSize);
        const int maxY = GridCell(std::max(first.y, second.y) + stepHorizontalTolerance,
                                  reachCellSize);
        const long long width = static_cast<long long>(maxX) - minX + 1;
        const long long cellHeight = static_cast<long long>(maxY) - minY + 1;
        if (width <= 0 || cellHeight <= 0 ||
            width * cellHeight > maximumEdgeCells) {
            globalBoundaryEdges.push_back(static_cast<std::uint32_t>(edgeIndex));
            continue;
        }
        for (int x = minX; x <= maxX; ++x) {
            for (int y = minY; y <= maxY; ++y) {
                reachCells[GridKey(x, y)].push_back(
                    static_cast<std::uint32_t>(edgeIndex));
            }
        }
    }
    std::vector<std::uint32_t> edgeStamps(finalEdgeOwners.size(), 0);
    std::uint32_t edgeStamp = 0;
    std::vector<std::uint32_t> secondEdgeCandidates;
    ReportProgress(progress, progressUserData, "vertical reachability",
                   0, boundaryEdges.size());
    for (std::size_t boundaryIndex = 0;
         boundaryIndex < boundaryEdges.size(); ++boundaryIndex) {
        ReportProgress(progress, progressUserData, "vertical reachability",
                       boundaryIndex, boundaryEdges.size());
        const std::uint32_t firstEdge = boundaryEdges[boundaryIndex];
        secondEdgeCandidates.clear();
        if (++edgeStamp == 0) {
            std::fill(edgeStamps.begin(), edgeStamps.end(), 0);
            edgeStamp = 1;
        }
        for (std::uint32_t edge : globalBoundaryEdges) {
            edgeStamps[edge] = edgeStamp;
            secondEdgeCandidates.push_back(edge);
        }
        const Edge& firstBoundary = output.edges[firstEdge];
        const Vec3& firstStart = output.vertices[firstBoundary.vertices[0]];
        const Vec3& firstEnd = output.vertices[firstBoundary.vertices[1]];
        const int minX = GridCell(std::min(firstStart.x, firstEnd.x) - stepHorizontalTolerance,
                                  reachCellSize);
        const int maxX = GridCell(std::max(firstStart.x, firstEnd.x) + stepHorizontalTolerance,
                                  reachCellSize);
        const int minY = GridCell(std::min(firstStart.y, firstEnd.y) - stepHorizontalTolerance,
                                  reachCellSize);
        const int maxY = GridCell(std::max(firstStart.y, firstEnd.y) + stepHorizontalTolerance,
                                  reachCellSize);
        const long long width = static_cast<long long>(maxX) - minX + 1;
        const long long cellHeight = static_cast<long long>(maxY) - minY + 1;
        if (width * cellHeight > maximumEdgeCells) {
            secondEdgeCandidates = boundaryEdges;
        } else {
            for (int x = minX; x <= maxX; ++x) {
                for (int y = minY; y <= maxY; ++y) {
                    const auto found = reachCells.find(GridKey(x, y));
                    if (found == reachCells.end()) {
                        continue;
                    }
                    for (std::uint32_t edge : found->second) {
                        if (edgeStamps[edge] != edgeStamp) {
                            edgeStamps[edge] = edgeStamp;
                            secondEdgeCandidates.push_back(edge);
                        }
                    }
                }
            }
        }
        std::sort(secondEdgeCandidates.begin(), secondEdgeCandidates.end());
        for (std::uint32_t secondEdge : secondEdgeCandidates) {
            if (secondEdge <= firstEdge) {
                continue;
            }
            const std::uint32_t firstArea = finalEdgeOwners[firstEdge][0];
            const std::uint32_t secondArea = finalEdgeOwners[secondEdge][0];
            const std::uint64_t pairKey = EdgeKey(firstArea, secondArea);
            if (firstArea == secondArea || linkedAreaPairs.count(pairKey) != 0) {
                continue;
            }
            Vec3 firstPoint;
            Vec3 secondPoint;
            bool hasTransition = VerticalTransitionPoints(
                output.edges[firstEdge], output.edges[secondEdge],
                output.vertices, stepEpsilon, stepHorizontalTolerance,
                firstPoint, secondPoint);
            if (!hasTransition) {
                // Expanded solid corners can leave two valid floor polygons
                // meeting at a point, or two adjacent stair treads separated
                // horizontally by the expansion bevel. Collinear-span testing
                // misses both forms of transition.
                hasTransition = ClosestTransitionPoints(
                    output.edges[firstEdge], output.edges[secondEdge],
                    output.vertices, stepHorizontalTolerance,
                    firstPoint, secondPoint);
            }
            if (!hasTransition) {
                continue;
            }
            const float height = std::fabs(firstPoint.z - secondPoint.z);
            const float transitionDx = firstPoint.x - secondPoint.x;
            const float transitionDy = firstPoint.y - secondPoint.y;
            const float horizontalDistanceSquared =
                transitionDx * transitionDx + transitionDy * transitionDy;
            if (height <= stepEpsilon &&
                horizontalDistanceSquared > seamTolerance * seamTolerance) {
                continue;
            }
            if (height <= maxStepHeight + stepEpsilon) {
                AddReachabilityPair(output, firstArea, secondArea,
                                    firstPoint, secondPoint);
                result.statistics.stepReachabilities += 2;
            } else {
                const bool firstIsHigh = firstPoint.z > secondPoint.z;
                const std::uint32_t highArea = firstIsHigh ? firstArea : secondArea;
                const std::uint32_t lowArea = firstIsHigh ? secondArea : firstArea;
                const Vec3& highPoint = firstIsHigh ? firstPoint : secondPoint;
                const Vec3& lowPoint = firstIsHigh ? secondPoint : firstPoint;
                if (height <= maxFallHeight + stepEpsilon) {
                    AddReachability(output, highArea, lowArea, highPoint, lowPoint,
                                    AAS2_TRAVEL_WALK_OFF_LEDGE);
                    ++result.statistics.fallReachabilities;
                }
                if (height <= maxLedgeGrabHeight + stepEpsilon) {
                    AddReachability(output, lowArea, highArea, lowPoint, highPoint,
                                    AAS2_TRAVEL_LEDGE_GRAB);
                    ++result.statistics.ledgeGrabReachabilities;
                }
                if (height > maxFallHeight + stepEpsilon &&
                    height > maxLedgeGrabHeight + stepEpsilon) {
                    continue;
                }
            }
            linkedAreaPairs.insert(pairKey);
        }
    }
    ReportProgress(progress, progressUserData, "vertical reachability",
                   boundaryEdges.size(), boundaryEdges.size());

    ReportProgress(progress, progressUserData, "area tree", 0, 1);
    BuildAreaTree(output);
    ReportProgress(progress, progressUserData, "area tree", 1, 1);

    result.statistics.vertices = output.vertices.size();
    result.statistics.edges = output.edges.size();
    result.statistics.areas = output.areas.size();
    result.statistics.reachabilities = output.reachabilities.size();
    if (!ValidateFile(output)) {
        output = File{};
        result.error = Error::invalidOutput;
    }
    return result;
}

std::size_t AddDynamicVolumeReachabilities(File& file,
                                           const Settings& settings) {
    if (file.areas.empty() || file.edges.empty()) {
        return 0;
    }
    const std::uint16_t dynamicFlags =
        AAS2_AREA_CLUSTER_PORTAL | AAS2_AREA_OBSTACLE;
    // A dynamic doorway is authored as an intentional connection.  C-space
    // expansion can separate its floor fragment from the adjoining room by
    // as much as one actor radius, while ordinary seams must retain the much
    // tighter weld tolerance.
    const float tolerance = std::max(
        std::max(0.5f, settings.weldEpsilon * 4.0f),
        std::min(16.0f, std::max(0.0f, settings.agentRadius)));
    const float cellSize = std::max(64.0f, settings.agentRadius * 4.0f);
    const float maxStepHeight = std::max(0.0f, settings.maxStepHeight) +
                                std::max(0.01f, settings.weldEpsilon);

    std::vector<std::vector<std::uint32_t>> owners(file.edges.size());
    for (std::size_t area = 0; area < file.areas.size(); ++area) {
        for (std::uint32_t edge : file.areas[area].edges) {
            if (edge < owners.size()) owners[edge].push_back(
                static_cast<std::uint32_t>(area));
        }
    }
    std::unordered_set<std::uint64_t> linkedPairs;
    for (const Reachability& reach : file.reachabilities) {
        linkedPairs.insert(EdgeKey(reach.fromArea, reach.toArea));
    }
    std::unordered_map<std::uint64_t, std::vector<std::uint32_t>> cells;
    std::vector<std::uint32_t> boundaries;
    for (std::size_t edgeIndex = 0; edgeIndex < owners.size(); ++edgeIndex) {
        if (owners[edgeIndex].size() != 1) continue;
        boundaries.push_back(static_cast<std::uint32_t>(edgeIndex));
        const Edge& edge = file.edges[edgeIndex];
        const Vec3& a = file.vertices[edge.vertices[0]];
        const Vec3& b = file.vertices[edge.vertices[1]];
        const int minX = GridCell(std::min(a.x, b.x) - tolerance, cellSize);
        const int maxX = GridCell(std::max(a.x, b.x) + tolerance, cellSize);
        const int minY = GridCell(std::min(a.y, b.y) - tolerance, cellSize);
        const int maxY = GridCell(std::max(a.y, b.y) + tolerance, cellSize);
        for (int x = minX; x <= maxX; ++x) {
            for (int y = minY; y <= maxY; ++y) {
                cells[GridKey(x, y)].push_back(
                    static_cast<std::uint32_t>(edgeIndex));
            }
        }
    }

    std::vector<std::uint32_t> stamps(file.edges.size(), 0);
    std::uint32_t stamp = 0;
    std::size_t added = 0;
    for (std::uint32_t firstEdge : boundaries) {
        const std::uint32_t firstArea = owners[firstEdge][0];
        if ((file.areas[firstArea].flags & dynamicFlags) == 0) continue;
        if (++stamp == 0) { std::fill(stamps.begin(), stamps.end(), 0); stamp = 1; }
        const Edge& edge = file.edges[firstEdge];
        const Vec3& a = file.vertices[edge.vertices[0]];
        const Vec3& b = file.vertices[edge.vertices[1]];
        const int minX = GridCell(std::min(a.x, b.x) - tolerance, cellSize);
        const int maxX = GridCell(std::max(a.x, b.x) + tolerance, cellSize);
        const int minY = GridCell(std::min(a.y, b.y) - tolerance, cellSize);
        const int maxY = GridCell(std::max(a.y, b.y) + tolerance, cellSize);
        for (int x = minX; x <= maxX; ++x) for (int y = minY; y <= maxY; ++y) {
            const auto found = cells.find(GridKey(x, y));
            if (found == cells.end()) continue;
            for (std::uint32_t secondEdge : found->second) {
                if (secondEdge == firstEdge || stamps[secondEdge] == stamp) continue;
                stamps[secondEdge] = stamp;
                const std::uint32_t secondArea = owners[secondEdge][0];
                const std::uint64_t key = EdgeKey(firstArea, secondArea);
                if (firstArea == secondArea || linkedPairs.count(key) != 0) continue;
                Vec3 firstPoint, secondPoint;
                if (!ClosestTransitionPoints(file.edges[firstEdge],
                                             file.edges[secondEdge],
                                             file.vertices, tolerance,
                                             firstPoint, secondPoint) ||
                    std::fabs(firstPoint.z - secondPoint.z) > maxStepHeight) {
                    continue;
                }
                AddReachabilityPair(file, firstArea, secondArea,
                                    firstPoint, secondPoint);
                linkedPairs.insert(key);
                added += 2;
            }
        }
    }
    return added;
}

bool ValidateFile(const File& file) {
    if (file.areas.empty() || file.trees.empty()) {
        return false;
    }
    for (const Edge& edge : file.edges) {
        if (edge.vertices[0] >= file.vertices.size() ||
            edge.vertices[1] >= file.vertices.size() ||
            edge.vertices[0] == edge.vertices[1]) {
            return false;
        }
    }
    for (std::size_t areaIndex = 0; areaIndex < file.areas.size(); ++areaIndex) {
        const Area& area = file.areas[areaIndex];
        if (area.tree >= file.trees.size() || area.edges.size() < 3) {
            return false;
        }
        for (std::uint32_t edge : area.edges) {
            if (edge >= file.edges.size()) {
                return false;
            }
        }
        for (std::uint32_t reachability : area.reachabilities) {
            if (reachability >= file.reachabilities.size() ||
                file.reachabilities[reachability].fromArea != areaIndex) {
                return false;
            }
        }
    }
    for (const Reachability& reachability : file.reachabilities) {
        if (reachability.fromArea >= file.areas.size() ||
            reachability.toArea >= file.areas.size() ||
            reachability.fromArea == reachability.toArea ||
            reachability.travelFlags == 0) {
            return false;
        }
    }
    for (const Tree& tree : file.trees) {
        if (tree.rootNode >= 0 &&
            static_cast<std::size_t>(tree.rootNode) >= file.nodes.size()) {
            return false;
        }
        if (tree.rootNode < 0 &&
            static_cast<std::size_t>(-tree.rootNode - 1) >= file.areas.size()) {
            return false;
        }
        for (std::uint32_t area : tree.areas) {
            if (area >= file.areas.size()) {
                return false;
            }
        }
    }
    for (const Node& node : file.nodes) {
        for (std::int32_t child : node.children) {
            if ((child >= 0 && static_cast<std::size_t>(child) >= file.nodes.size()) ||
                (child < 0 && static_cast<std::size_t>(-child - 1) >= file.areas.size())) {
                return false;
            }
        }
    }
    return true;
}

const char* ErrorName(Error error) {
    switch (error) {
        case Error::none: return "none";
        case Error::emptyGeometry: return "empty geometry";
        case Error::invalidVertexIndex: return "invalid vertex index";
        case Error::degenerateTriangle: return "degenerate triangle";
        case Error::nonManifoldEdge: return "non-manifold edge";
        case Error::invalidOutput: return "invalid output";
    }
    return "unknown";
}

} // namespace aas2
} // namespace compilers
} // namespace darklight
