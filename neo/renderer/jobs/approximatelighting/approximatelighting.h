#ifndef __JOBS_APPROXIMATELIGHTING_APPROXIMATELIGHTING_H__
#define __JOBS_APPROXIMATELIGHTING_APPROXIMATELIGHTING_H__

/*
================================================================================================

Tech5 style approximate lighting data structures.

The full Tech5 job computes per model light contributions for the
approximate lighting shader: a prime light direction, up to three radial
light origins and colors for self shadowing, and six channel colors blended
with the offline ambient map. Re-implemented on idlib from the Tech5
semantics, nothing was copied from the original code.

================================================================================================
*/

#include "ambientmapcpu.h"
#include "shadowmapcpu.h"

class idRenderLightLocal;

// lighting models the approximate lighting data can describe
enum lightingModel_t {
	LM_PRIME_AND_CHANNEL = 0,
	LM_LOCAL_LIGHTS = 1
};

// maximum number of radial lights gathered for the self shadow slots
const int MAX_CONTRIBUTING_LIGHTS = 3;

struct approximateLighting_t {
	void				Clear();

	idVec4				lightingModel;
	idVec4				primeLightDir;
	idVec4				lightOrigin[3];
	idVec4				lightColor[3];
	idVec4				channelLighting[6];
};

// one radial light gathered for the approximate lighting job
struct contributingLight_t {
	// world space light position, w == 1.0f marks a light that never casts shadows
	idVec4				lightOrigin;
	idVec3				lightColor;
};

// per model inputs for the approximate lighting job
struct approximateLightParms_t {
	idVec3				modelOrigin;		// world space center of the model
	idVec3				viewOrigin;			// world space view position
	int					numContributingLights;
	contributingLight_t	contributingLights[MAX_CONTRIBUTING_LIGHTS];
	ambientMapCPU_t *	ambientMap;			// NULL when the map has no ambient data
};

void AmbientOnlyLighting( const approximateLightParms_t & parms, approximateLighting_t & lighting );
void LocalLighting( const approximateLightParms_t & parms, approximateLighting_t & lighting );

// one light contribution gathered for the approximate lighting job
struct lightContribution_t {
	const idRenderLightLocal *	light;
	idVec3				color;
};

#endif // __JOBS_APPROXIMATELIGHTING_APPROXIMATELIGHTING_H__
