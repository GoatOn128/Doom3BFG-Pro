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

struct FloorMergeResult {
    std::size_t mergedPairs = 0;
};

// Greedily joins adjacent coplanar floor areas when their combined boundary is
// a single convex polygon. Interior triangulation edges are removed and the
// remaining edge array is compacted.
FloorMergeResult MergeCoplanarFloors(File& file,
                                     float normalDegrees,
                                     float planeDistanceEpsilon);

} // namespace aas2
} // namespace compilers
} // namespace darklight



