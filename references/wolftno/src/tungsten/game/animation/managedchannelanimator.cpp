#include "tungsten/game/animation/managedchannelanimator.h"

// DIA: `dynamic initializer for 'managedChannelAnimator_animBlendInMS''
// DIA: `dynamic atexit destructor for 'managedChannelAnimator_animBlendInMS''
idCVar managedChannelAnimator_animBlendInMS(
    "managedChannelAnimator_animBlendInMS", "150", 2,
    "default blend-in duration");

// DIA: `dynamic initializer for 'managedChannelAnimator_animIsDoneFrames''
// DIA: `dynamic atexit destructor for 'managedChannelAnimator_animIsDoneFrames''
idCVar managedChannelAnimator_animIsDoneFrames(
    "managedChannelAnimator_animIsDoneFrames", "2", 2,
    "frames to look ahead when testing animation completion");

// DIA: `dynamic initializer for 'managedChannelANimator_animBlendOutMS''
// DIA: `dynamic atexit destructor for 'managedChannelANimator_animBlendOutMS''
idCVar managedChannelANimator_animBlendOutMS(
    "managedChannelAnimator_animBlendOutMS", "250", 2,
    "default blend-out duration");
