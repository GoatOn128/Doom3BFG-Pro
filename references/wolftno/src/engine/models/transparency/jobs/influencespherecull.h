#pragma once

#include "shared/idlib/math/vector.h"

#include <cstddef>

struct influenceSphere_t {
  idVec3 center;
  idVec3 directionalForce;
  float innerRadius;
  float outerRadius;
  float angle;
};

struct visibleInfluenceSpheres_t {
  int numSpheres;
  int pad[3];
  influenceSphere_t spheres[256];
};

struct influenceSphereCullParms_t {
  float nearDist;
  float farDist;
  idVec2 fov;
  idVec3 viewOrg;
  idVec3 viewFwd;
  idVec3 viewLeft;
  idVec3 viewUp;
  const influenceSphere_t* influenceSpheres;
  int numInfluenceSpheres;
  visibleInfluenceSpheres_t* visibleInfluenceSpheres;
};

void CullInfluenceSpheresForView(const influenceSphereCullParms_t* parms);

static_assert(sizeof(influenceSphere_t) == 36);
static_assert(offsetof(influenceSphere_t, directionalForce) == 12);
static_assert(offsetof(influenceSphere_t, innerRadius) == 24);
static_assert(offsetof(influenceSphere_t, angle) == 32);
static_assert(sizeof(visibleInfluenceSpheres_t) == 9232);
static_assert(offsetof(visibleInfluenceSpheres_t, spheres) == 16);
static_assert(sizeof(influenceSphereCullParms_t) == 88);
static_assert(offsetof(influenceSphereCullParms_t, viewOrg) == 16);
static_assert(offsetof(influenceSphereCullParms_t, influenceSpheres) == 64);
static_assert(offsetof(influenceSphereCullParms_t, numInfluenceSpheres) == 72);
static_assert(offsetof(influenceSphereCullParms_t, visibleInfluenceSpheres) == 80);
