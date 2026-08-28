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


#include "../../../idlib/precompiled.h"
#pragma hdrstop

#include "compiler_commands.h"
#include "aas2_map_adapter.h"
#include "aas2_build.h"
#include "aas2_binary.h"
#include "aas2_brush_surface.h"
#include "aas2_cspace.h"
#include "aas2_compiler.h"

namespace {

struct AAS2ProgressState {
    idStr stage;
    int lastPercent = -1;
    int startTime = 0;
};

void AAS2BuildProgress(const char* stage, std::size_t current,
                       std::size_t total, void* userData) {
    AAS2ProgressState* state = static_cast<AAS2ProgressState*>(userData);
    const int percent = total != 0 ?
        static_cast<int>((current * 100) / total) : 100;
    if (state->stage.Icmp(stage) != 0) {
        state->stage = stage;
        state->lastPercent = -1;
    }
    if (percent != 100 && state->lastPercent >= 0 &&
        percent < state->lastPercent + 5) {
        return;
    }
    state->lastPercent = percent;
    common->Printf("AAS2 %-28s %3d%%  (%u/%u, %.1fs)\n",
                   stage, percent,
                   static_cast<unsigned int>(current),
                   static_cast<unsigned int>(total),
                   (Sys_Milliseconds() - state->startTime) * 0.001f);
}

bool ReadAAS2Profile(const char* requestedName,
                     darklight::compilers::aas2::BuildSettings& build,
                     darklight::compilers::aas2::BinaryMetadata& binary,
                     idStr& profileName) {
    profileName = requestedName && *requestedName ? requestedName : "aas48";
    if (profileName.Icmpn("aas", 3) != 0) {
        profileName.Insert("aas", 0);
    }
    const idDeclEntityDef* decl = static_cast<const idDeclEntityDef*>(
        declManager->FindType(DECL_ENTITYDEF, profileName, false));
    if (decl == NULL) {
        common->Warning("AAS2 settings decl '%s' was not found", profileName.c_str());
        return false;
    }
    const idDict& dict = decl->dict;
    idVec3 mins, maxs, gravity;
    if (!dict.GetVector("mins", NULL, mins) || !dict.GetVector("maxs", NULL, maxs)) {
        common->Warning("AAS2 settings decl '%s' has no mins/maxs", profileName.c_str());
        return false;
    }
    build.agent.mins = {mins.x, mins.y, mins.z};
    build.agent.maxs = {maxs.x, maxs.y, maxs.z};
    build.navigation.agentHeight = maxs.z - mins.z;
    build.navigation.agentRadius = Max(Max(idMath::Fabs(mins.x), idMath::Fabs(maxs.x)),
                                       Max(idMath::Fabs(mins.y), idMath::Fabs(maxs.y)));
    dict.GetFloat("maxStepHeight", "18", build.navigation.maxStepHeight);
    dict.GetFloat("maxFallHeight", "64", build.navigation.maxFallHeight);
    float minFloorCos = 0.7f;
    dict.GetFloat("minFloorCos", "0.7", minFloorCos);
    minFloorCos = idMath::ClampFloat(-1.0f, 1.0f, minFloorCos);
    build.navigation.maxFloorSlopeDegrees = idMath::ACos(minFloorCos) * idMath::M_RAD2DEG;

    binary.agent = build.agent;
    binary.fileExtensionAAS = dict.GetString("fileExtension", profileName.c_str());
    binary.groupName = dict.GetString("groupName", "aas");
    binary.explicitGroupName = dict.GetString("explicitGroupName", profileName.c_str());
    const char* type = dict.GetString("type", "monster");
    binary.type = idStr::Icmp(type, "player") == 0 ? 0 :
                  idStr::Icmp(type, "vehicle") == 0 ? 2 :
                  idStr::Icmp(type, "compass") == 0 ? 3 :
                  idStr::Icmp(type, "bot") == 0 ? 4 : 1;
    dict.GetInt("primitiveModeBrush", "0", binary.primitiveModeBrush);
    dict.GetInt("primitiveModePatch", dict.GetBool("usePatches", "0") ? "0" : "1",
                binary.primitiveModePatch);
    dict.GetInt("primitiveModeModel", "1", binary.primitiveModeModel);
    dict.GetVector("gravity", "0 0 -1066", gravity);
    binary.gravityValue = gravity.Length();
    if (binary.gravityValue > 0.0f) gravity /= binary.gravityValue;
    binary.gravityDir = {gravity.x, gravity.y, gravity.z};
    binary.maxStepHeight = build.navigation.maxStepHeight;
    dict.GetFloat("maxBarrierHeight", "32", binary.maxBarrierHeight);
    dict.GetFloat("maxWaterJumpHeight", "20", binary.maxWaterJumpHeight);
    binary.maxFallHeight = static_cast<int>(build.navigation.maxFallHeight);
    binary.minFloorCos = minFloorCos;
#define AAS2_FLOAT_SETTING(key, member, fallback) dict.GetFloat(key, fallback, binary.member)
    AAS2_FLOAT_SETTING("minHighCeiling", minHighCeiling, "80");
    AAS2_FLOAT_SETTING("groundSpeed", groundSpeed, "250");
    AAS2_FLOAT_SETTING("waterSpeed", waterSpeed, "150");
    AAS2_FLOAT_SETTING("ladderSpeed", ladderSpeed, "50");
    AAS2_FLOAT_SETTING("wallCornerEdgeRadius", wallCornerEdgeRadius, "16");
    AAS2_FLOAT_SETTING("ledgeCornerEdgeRadius", ledgeCornerEdgeRadius, "16");
    AAS2_FLOAT_SETTING("obstaclePVSRadius", obstaclePVSRadius, "1024");
    AAS2_FLOAT_SETTING("minCrouchingCoverHeight", minCrouchingCoverHeight, "32");
    AAS2_FLOAT_SETTING("minStandingCoverHeight", minStandingCoverHeight, "64");
    AAS2_FLOAT_SETTING("crouchingFireHeight", crouchingFireHeight, "48");
    AAS2_FLOAT_SETTING("standingFireHeight", standingFireHeight, "72");
    AAS2_FLOAT_SETTING("minWallWidth", minWallWidth, "8");
    AAS2_FLOAT_SETTING("maxWallWidth", maxWallWidth, "32");
    AAS2_FLOAT_SETTING("minDoorWidth", minDoorWidth, "32");
    AAS2_FLOAT_SETTING("maxDoorWidth", maxDoorWidth, "80");
    AAS2_FLOAT_SETTING("coverCornerDistance", coverCornerDistance, "8");
    AAS2_FLOAT_SETTING("coverWallDistance", coverWallDistance, "8");
    AAS2_FLOAT_SETTING("chokePointWidth", chokePointWidth, "96");
#undef AAS2_FLOAT_SETTING
    dict.GetInt("tt_barrierJump", "100", binary.ttBarrierJump);
    dict.GetInt("tt_waterJump", "100", binary.ttWaterJump);
    dict.GetInt("tt_startWalkOffLedge", "100", binary.ttStartWalkOffLedge);
    dict.GetInt("tt_startLadderClimb", "100", binary.ttStartLadderClimb);
    binary.usePatches = dict.GetBool("usePatches", "0");
    return true;
}

idStr AAS2OutputName(const char* mapName, const char* profile) {
    idStr output = mapName;
    output.StripFileExtension();
    idStr suffix = profile;
    if (suffix.Icmpn("aas_", 4) == 0) suffix = suffix.Mid(4, suffix.Length() - 4);
    else if (suffix.Icmpn("aas", 3) == 0) suffix = suffix.Mid(3, suffix.Length() - 3);
    output += "_";
    output += suffix;
    output += ".aas2";
    return output;
}

void PrintAAS2BuildFailure(const darklight::compilers::aas2::BuildResult& result) {
    namespace aas2 = darklight::compilers::aas2;
    switch (result.error) {
        case aas2::BuildError::configurationSpace:
            common->Warning("AAS2 build failed in configuration space: %s "
                            "at brush %u plane %u",
                            aas2::CSpaceErrorName(result.configurationSpace.error),
                            static_cast<unsigned int>(result.configurationSpace.sourceBrush),
                            static_cast<unsigned int>(result.configurationSpace.sourcePlane));
            return;
        case aas2::BuildError::brushSurface:
            if (result.brushSurface.error ==
                aas2::BrushSurfaceError::fragmentLimit) {
                common->Warning("AAS2 floor CSG safety limit at face brush %u "
                                "while clipping brush %u (peak %u fragments, "
                                "%u points)",
                                static_cast<unsigned int>(
                                    result.brushSurface.sourceBrush),
                                static_cast<unsigned int>(
                                    result.brushSurface.sourcePlane),
                                static_cast<unsigned int>(
                                    result.brushSurface.peakFragments),
                                static_cast<unsigned int>(
                                    result.brushSurface.peakFragmentPoints));
                return;
            }
            common->Warning("AAS2 build failed in brush surfaces: %s "
                            "at brush %u plane %u",
                            aas2::BrushSurfaceErrorName(result.brushSurface.error),
                            static_cast<unsigned int>(result.brushSurface.sourceBrush),
                            static_cast<unsigned int>(result.brushSurface.sourcePlane));
            return;
        case aas2::BuildError::surfaceCompiler:
            common->Warning("AAS2 build failed in surface compilation: %s "
                            "at triangle %u",
                            aas2::ErrorName(result.surface.error),
                            static_cast<unsigned int>(result.surface.sourceTriangle));
            return;
        case aas2::BuildError::none:
            return;
    }
}

void AAS2CompilerSelfTest_f(const idCmdArgs& args) {
    (void)args;
    namespace aas2 = darklight::compilers::aas2;

    aas2::ConvexBrush brush;
    brush.planes = {
        {{1.0f, 0.0f, 0.0f}, 32.0f},
        {{-1.0f, 0.0f, 0.0f}, 0.0f},
        {{0.0f, 1.0f, 0.0f}, 32.0f},
        {{0.0f, -1.0f, 0.0f}, 0.0f},
        {{0.0f, 0.0f, 1.0f}, 64.0f},
        {{0.0f, 0.0f, -1.0f}, 0.0f}
    };
    std::vector<aas2::ConvexBrush> expandedBrushes;
    const aas2::CSpaceResult cspace = aas2::ExpandConfigurationSpace(
        {brush}, aas2::MakeAgentBounds(16.0f, 72.0f), expandedBrushes);
    if (!cspace) {
        common->Warning("AAS2 configuration-space self-test failed: %s "
                        "at brush %u plane %u",
                        aas2::CSpaceErrorName(cspace.error),
                        static_cast<unsigned int>(cspace.sourceBrush),
                        static_cast<unsigned int>(cspace.sourcePlane));
        return;
    }

    aas2::SourceGeometry source;
    const aas2::BrushSurfaceResult surfaces =
        aas2::BuildBrushFloorGeometry(expandedBrushes, 45.0f, 0.01f, source);
    if (!surfaces) {
        common->Warning("AAS2 brush-surface self-test failed: %s at brush %u plane %u",
                        aas2::BrushSurfaceErrorName(surfaces.error),
                        static_cast<unsigned int>(surfaces.sourceBrush),
                        static_cast<unsigned int>(surfaces.sourcePlane));
        return;
    }

    aas2::File output;
    const aas2::Result result =
        aas2::CompileSurface(source, aas2::Settings{}, output);
    if (!result) {
        common->Warning("AAS2 compiler self-test failed: %s at triangle %u",
                        aas2::ErrorName(result.error),
                        static_cast<unsigned int>(result.sourceTriangle));
        return;
    }

    common->Printf("AAS2 compiler self-test passed: %u C-space brushes, "
                   "%u brush faces, %u floor triangles, %u vertices, %u edges, "
                   "%u areas, %u floor merges, %u reachabilities "
                   "(%u step, %u fall, %u ledge-grab)\n",
                   static_cast<unsigned int>(cspace.brushes),
                   static_cast<unsigned int>(surfaces.boundaryFaces),
                   static_cast<unsigned int>(surfaces.floorTriangles),
                   static_cast<unsigned int>(result.statistics.vertices),
                   static_cast<unsigned int>(result.statistics.edges),
                   static_cast<unsigned int>(result.statistics.areas),
                   static_cast<unsigned int>(result.statistics.mergedFloorPairs),
                   static_cast<unsigned int>(result.statistics.reachabilities),
                   static_cast<unsigned int>(result.statistics.stepReachabilities),
                   static_cast<unsigned int>(result.statistics.fallReachabilities),
                   static_cast<unsigned int>(result.statistics.ledgeGrabReachabilities));
}

void AAS2Build_f(const idCmdArgs& args) {
    namespace aas2 = darklight::compilers::aas2;
    if (args.Argc() < 2 || args.Argc() > 3) {
        common->Printf("usage: aas2build <mapName> [aasType]\n"
                       "example: aas2build maps/game/alphalabs2 aas48\n");
        return;
    }

    aas2::BuildSettings buildSettings;
    aas2::BinaryMetadata metadata;
    idStr profileName;
    if (!ReadAAS2Profile(args.Argc() == 3 ? args.Argv(2) : "aas48",
                         buildSettings, metadata, profileName)) {
        return;
    }

    idMapFile mapFile;
    common->Printf("AAS2 parsing map '%s'...\n", args.Argv(1));
    if (!mapFile.Parse(args.Argv(1))) {
        common->Warning("AAS2 could not parse map '%s'", args.Argv(1));
        return;
    }
    std::vector<aas2::ConvexBrush> brushes;
    std::vector<DarklightAAS2AreaVolume> areaVolumes;
    DarklightAAS2MapStatistics mapStatistics;
    DarklightAAS2PrimitivePolicy primitivePolicy;
    primitivePolicy.includeBrushes = metadata.primitiveModeBrush == 0;
    primitivePolicy.includePatches = metadata.primitiveModePatch == 0;
    if (!CollectDarklightAAS2Brushes(mapFile, primitivePolicy,
                                     brushes, areaVolumes,
                                     mapStatistics)) {
        common->Warning("AAS2 found no participating solid brushes in '%s'",
                        args.Argv(1));
        return;
    }
    common->Printf("AAS2 collected %u solid brushes and %u dynamic volumes\n",
                   static_cast<unsigned int>(brushes.size()),
                   static_cast<unsigned int>(areaVolumes.size()));
    common->Printf("AAS2 primitive policy: brushes %s (%u map brushes, %u "
                   "ignored), patches %s (%u patches, %u generated triangles, "
                   "%u ignored)\n",
                   primitivePolicy.includeBrushes ? "enabled" : "disabled",
                   static_cast<unsigned int>(mapStatistics.brushPrimitives),
                   static_cast<unsigned int>(mapStatistics.ignoredBrushes),
                   primitivePolicy.includePatches ? "enabled" : "disabled",
                   static_cast<unsigned int>(mapStatistics.patchPrimitives),
                   static_cast<unsigned int>(mapStatistics.acceptedPatchBrushes),
                   static_cast<unsigned int>(mapStatistics.ignoredPatches));

    aas2::File output;
    AAS2ProgressState progress;
    progress.startTime = Sys_Milliseconds();
    buildSettings.progress = AAS2BuildProgress;
    buildSettings.progressUserData = &progress;
    common->SetRefreshOnPrint(true);
    const aas2::BuildResult result =
        aas2::CompileBrushes(brushes, buildSettings, output);
    common->SetRefreshOnPrint(false);
    if (!result) {
        PrintAAS2BuildFailure(result);
        return;
    }
    ApplyDarklightAAS2AreaVolumes(output, areaVolumes, buildSettings.agent,
                                  mapStatistics);
    const std::size_t dynamicReachabilities =
        aas2::AddDynamicVolumeReachabilities(output,
                                             buildSettings.navigation);
    common->Printf("AAS2 compiled model: %u vertices, %u edges, %u areas, "
                   "%u reachabilities (%u dynamic-volume), %u nodes\n",
                   static_cast<unsigned int>(output.vertices.size()),
                   static_cast<unsigned int>(output.edges.size()),
                   static_cast<unsigned int>(output.areas.size()),
                   static_cast<unsigned int>(output.reachabilities.size()),
                   static_cast<unsigned int>(dynamicReachabilities),
                   static_cast<unsigned int>(output.nodes.size()));
    metadata.mapCRC = mapFile.GetGeometryCRC();
    metadata.sourceTimestamp = static_cast<unsigned int>(mapFile.GetFileTime());
    metadata.name = mapFile.GetName();
    metadata.sourcePath = args.Argv(1);
    std::vector<std::uint8_t> bytes;
    const aas2::BinaryError binary = aas2::SerializeWolfTNOAAS2(output, metadata, bytes);
    const idStr outputName = AAS2OutputName(args.Argv(1), metadata.fileExtensionAAS.c_str());
    if (binary != aas2::BinaryError::none) {
        common->Warning("AAS2 failed to serialize '%s': %s "
                        "(%u areas, %u reachabilities)", outputName.c_str(),
                        aas2::BinaryErrorName(binary),
                        static_cast<unsigned int>(output.areas.size()),
                        static_cast<unsigned int>(output.reachabilities.size()));
        return;
    }
    idFile* destination = fileSystem->OpenFileWrite(outputName, "fs_basepath");
    if (destination == NULL || destination->Write(bytes.data(), bytes.size()) != bytes.size()) {
        if (destination != NULL) fileSystem->CloseFile(destination);
        common->Warning("AAS2 failed to write '%s'", outputName.c_str());
        return;
    }
    fileSystem->CloseFile(destination);
    common->Printf("AAS2 wrote '%s' using profile '%s'\n",
                   outputName.c_str(), profileName.c_str());
    common->Printf("AAS2 floor CSG: %u broad-phase pairs, %u rejected before "
                   "clipping, peak %u fragments / %u points per face\n",
                   static_cast<unsigned int>(result.brushSurface.candidatePairs),
                   static_cast<unsigned int>(
                       result.brushSurface.rejectedCandidatePairs),
                   static_cast<unsigned int>(result.brushSurface.peakFragments),
                   static_cast<unsigned int>(
                       result.brushSurface.peakFragmentPoints));
    common->Printf("AAS2 discarded %u collapsed CSG triangles, %u triangles "
                   "collapsed by surface welding, and %u duplicate triangles; "
                   "%u multi-owned edges remain isolated\n",
                   static_cast<unsigned int>(
                       result.brushSurface.discardedDegenerateTriangles),
                   static_cast<unsigned int>(
                       result.surface.statistics.rejectedDegenerateTriangles),
                   static_cast<unsigned int>(
                       result.surface.statistics.rejectedDuplicateTriangles),
                   static_cast<unsigned int>(
                       result.surface.statistics.nonManifoldEdges));
    common->Printf("AAS2 in-memory map build passed: %u/%u brushes, "
                   "%u ignored patches, %u floor faces, %u areas, "
                   "%u dynamic volumes tagged %u areas, %u reachabilities "
                   "(%u step, %u fall, %u ledge-grab)\n",
                   static_cast<unsigned int>(mapStatistics.acceptedBrushes),
                   static_cast<unsigned int>(mapStatistics.brushPrimitives),
                   static_cast<unsigned int>(mapStatistics.ignoredPatches),
                   static_cast<unsigned int>(result.brushSurface.floorFaces),
                   static_cast<unsigned int>(result.surface.statistics.areas),
                   static_cast<unsigned int>(mapStatistics.areaVolumes),
                   static_cast<unsigned int>(mapStatistics.taggedAreas),
                   static_cast<unsigned int>(result.surface.statistics.reachabilities),
                   static_cast<unsigned int>(result.surface.statistics.stepReachabilities),
                   static_cast<unsigned int>(result.surface.statistics.fallReachabilities),
                   static_cast<unsigned int>(result.surface.statistics.ledgeGrabReachabilities));
}

} // namespace

static idCommandLink aas2BuildCommandLink(
    "aas2build", AAS2Build_f,
    "compiles a map and AAS declaration to maps/..._<profile>.aas2");
static idCommandLink aas2SelfTestCommandLink(
    "aas2CompilerSelfTest", AAS2CompilerSelfTest_f,
    "runs reconstructed AAS2 compiler structural tests");
