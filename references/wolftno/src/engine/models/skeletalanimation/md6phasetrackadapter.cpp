#include "engine/models/skeletalanimation/md6phasetrack.h"

idMD6PhaseTrackSkelLookup md6PhaseTrackSkeletonLookup = nullptr;

void SetMD6PhaseTrackSkelLookup(idMD6PhaseTrackSkelLookup lookup) {
  md6PhaseTrackSkeletonLookup = lookup;
}
