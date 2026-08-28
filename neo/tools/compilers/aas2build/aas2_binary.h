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

#include <cstdint>
#include <string>
#include <vector>

namespace darklight {
namespace compilers {
namespace aas2 {

struct BinaryMetadata {
    std::uint32_t mapCRC = 0;
    std::uint32_t sourceTimestamp = 0;
    std::string name;
    std::string sourcePath;
    AgentBounds agent{{-16.0f, -16.0f, 0.0f},
                      {16.0f, 16.0f, 72.0f}};
    float maxStepHeight = 18.0f;
    float maxBarrierHeight = 32.0f;
    std::int32_t maxFallHeight = 128;
    bool usePatches = true;
    std::string fileExtensionAAS = "aas48";
    std::string groupName = "aas";
    std::string explicitGroupName = "aas48";
    std::int32_t type = 1;
    std::int32_t primitiveModeBrush = 0;
    std::int32_t primitiveModePatch = 1;
    std::int32_t primitiveModeModel = 1;
    Vec3 gravityDir{0.0f, 0.0f, -1.0f};
    float gravityValue = 1066.0f;
    float maxWaterJumpHeight = 20.0f;
    float minFloorCos = 0.7f;
    float minHighCeiling = 80.0f;
    float groundSpeed = 250.0f;
    float waterSpeed = 150.0f;
    float ladderSpeed = 50.0f;
    float wallCornerEdgeRadius = 16.0f;
    float ledgeCornerEdgeRadius = 16.0f;
    float obstaclePVSRadius = 1024.0f;
    float minCrouchingCoverHeight = 32.0f;
    float minStandingCoverHeight = 64.0f;
    float crouchingFireHeight = 48.0f;
    float standingFireHeight = 72.0f;
    float minWallWidth = 8.0f;
    float maxWallWidth = 32.0f;
    float minDoorWidth = 32.0f;
    float maxDoorWidth = 80.0f;
    float coverCornerDistance = 8.0f;
    float coverWallDistance = 8.0f;
    float chokePointWidth = 96.0f;
    std::int32_t ttBarrierJump = 100;
    std::int32_t ttWaterJump = 100;
    std::int32_t ttStartWalkOffLedge = 100;
    std::int32_t ttStartLadderClimb = 100;
};

enum class BinaryError {
    none,
    invalidFile,
    tooManyAreas,
    tooManyReachabilities,
    unorientedArea,
    tooLarge,
    ioError
};

BinaryError SerializeWolfTNOAAS2(const File& file,
                                 const BinaryMetadata& metadata,
                                 std::vector<std::uint8_t>& output);
BinaryError DeserializeWolfTNOAAS2(const std::vector<std::uint8_t>& input,
                                   File& file,
                                   BinaryMetadata& metadata);
BinaryError WriteWolfTNOAAS2(const char* path,
                             const File& file,
                             const BinaryMetadata& metadata);

const char* BinaryErrorName(BinaryError error);

} // namespace aas2
} // namespace compilers
} // namespace darklight
