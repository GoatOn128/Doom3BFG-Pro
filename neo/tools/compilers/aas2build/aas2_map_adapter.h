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

#include <cstddef>
#include <vector>

class idMapFile;

struct DarklightAAS2MapStatistics {
    std::size_t entities = 0;
    std::size_t brushPrimitives = 0;
    std::size_t patchPrimitives = 0;
    std::size_t acceptedBrushes = 0;
    std::size_t ignoredBrushes = 0;
    std::size_t ignoredPatches = 0;
    std::size_t acceptedPatchBrushes = 0;
    std::size_t areaVolumes = 0;
    std::size_t taggedAreas = 0;
};

struct DarklightAAS2PrimitivePolicy {
    bool includeBrushes = true;
    bool includePatches = false;
};

struct DarklightAAS2AreaVolume {
    idBounds bounds;
    unsigned short areaFlags = 0;
};

// Converts worldspawn and func_aas_obstacle brushes from Darklight's parsed map
// representation into the portable compiler model. Material contents determine
// whether a brush participates in AAS construction.
bool CollectDarklightAAS2Brushes(
    const idMapFile& mapFile,
    const DarklightAAS2PrimitivePolicy& primitivePolicy,
    std::vector<darklight::compilers::aas2::ConvexBrush>& output,
    std::vector<DarklightAAS2AreaVolume>& areaVolumes,
    DarklightAAS2MapStatistics& statistics);

void ApplyDarklightAAS2AreaVolumes(
    darklight::compilers::aas2::File& file,
    const std::vector<DarklightAAS2AreaVolume>& areaVolumes,
    const darklight::compilers::aas2::AgentBounds& agent,
    DarklightAAS2MapStatistics& statistics);

