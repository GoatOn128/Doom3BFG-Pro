#pragma once

struct trackingParms_t {
  float springK{1.0F};
  float springMass{1.0F};
  float springDamping{2.0F};
  float springMaxSpeed{0.0F};
  float springMaxLength{0.0F};
  float deltaSeconds{0.016F};
};

enum aimPoint_t : int {
  AIMPOINT_ORIGIN = 0,
  AIMPOINT_HEAD = 1,
  AIMPOINT_TORSO = 2,
  AIMPOINT_CENTER = 3,
  AIMPOINT_LEGS = 4,
  AIMPOINT_FEET = 5,
  AIMPOINT_BEST = 6,
  AIMPOINT_EYELEVEL = 7,
  AIMPOINT_LOOKAHEAD = 8,
  AIMPOINT_MAX = 9
};

enum aiPlayer_t {
  AIPLAYER_CLOSEST,
  AIPLAYER_FARTHEST,
  AIPLAYER_LOWEST_HEALTH,
  AIPLAYER_HIGHEST_HEALTH
};
