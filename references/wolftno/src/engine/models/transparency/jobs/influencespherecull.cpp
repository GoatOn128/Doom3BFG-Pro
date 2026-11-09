#include "engine/models/transparency/jobs/influencespherecull.h"

#include "shared/idlib/parallelism/paralleljoblist.h"

#include <cstdint>

void CullInfluenceSpheresForView(const influenceSphereCullParms_t* parms) {
  int sortIndex[1024];
  std::uint8_t keep[1024];
  const int numInfluenceSpheres = parms->numInfluenceSpheres;
  const float farDist = parms->farDist;
  const float nearDist = parms->nearDist;
  const float verticalFov = parms->fov.y;
  int keptCount = 0;
  int sortedCount = 0;

  for (int index = 0; index < numInfluenceSpheres; ++index) {
    const influenceSphere_t& sphere = parms->influenceSpheres[index];
    keep[index] = 0;
    const float deltaX = sphere.center.x - parms->viewOrg.x;
    const float deltaY = sphere.center.y - parms->viewOrg.y;
    const float deltaZ = sphere.center.z - parms->viewOrg.z;
    const float diameter = sphere.outerRadius * 2.0f;
    const float depth =
        (parms->viewFwd.y * deltaY + parms->viewFwd.x * deltaX) +
        parms->viewFwd.z * deltaZ;
    if (nearDist <= depth + diameter && depth - diameter <= farDist) {
      const float perpendicularY = deltaY - parms->viewFwd.y * depth;
      const float perpendicularZ = deltaZ - parms->viewFwd.z * depth;
      const float perpendicularX = deltaX - parms->viewFwd.x * depth;
      const float horizontal =
          (perpendicularY * parms->viewLeft.y +
           perpendicularX * parms->viewLeft.x) +
          perpendicularZ * parms->viewLeft.z;
      const float horizontalExtent = depth * parms->fov.x;
      if (-horizontalExtent <= horizontal + diameter &&
          horizontal - diameter <= horizontalExtent) {
        const float vertical =
            (perpendicularY * parms->viewUp.y +
             perpendicularX * parms->viewUp.x) +
            perpendicularZ * parms->viewUp.z;
        const float verticalExtent = depth * verticalFov;
        if (-verticalExtent <= vertical + diameter &&
            vertical - diameter <= verticalExtent) {
          sortIndex[sortedCount] = index;
          int insertion = 0;
          while (insertion < sortedCount &&
                 sphere.angle <=
                     parms->influenceSpheres[sortIndex[insertion]].angle) {
            ++insertion;
          }
          for (int move = sortedCount; move > insertion; --move)
            sortIndex[move] = sortIndex[move - 1];
          sortIndex[insertion] = index;
          keep[index] = 1;
          if (keptCount >= 256) {
            keep[sortIndex[sortedCount]] = 0;
          } else {
            ++keptCount;
            ++sortedCount;
          }
        }
      }
    }
  }

  int visibleCount = 0;
  for (int index = 0; index < parms->numInfluenceSpheres; ++index) {
    if (keep[index] != 0)
      parms->visibleInfluenceSpheres->spheres[visibleCount++] =
          parms->influenceSpheres[index];
  }
  parms->visibleInfluenceSpheres->numSpheres = visibleCount;
}

idParallelJobRegistration register_CullInfluenceSpheresForView(
    reinterpret_cast<jobRun_t>(CullInfluenceSpheresForView),
    "CullInfluenceSpheresForView");
