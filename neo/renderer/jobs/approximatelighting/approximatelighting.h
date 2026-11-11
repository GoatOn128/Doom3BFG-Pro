#ifndef __JOBS_APPROXIMATELIGHTING_APPROXIMATELIGHTING_H__
#define __JOBS_APPROXIMATELIGHTING_APPROXIMATELIGHTING_H__

/*
================================================================================================

Tech5 style approximate lighting data structures.

The full Tech5 job computes per model light contributions for the
approximate lighting shader: a prime light direction, up to three radial
light origins and colors for self shadowing, and six channel colors blended
with the ambient map. This engine consumes the prime light for dim shadows
so far. Re-implemented on idlib from the Tech5 semantics, nothing was
copied from the original code.

================================================================================================
*/

#include "ambientmapcpu.h"
#include "shadowmapcpu.h"

class idRenderLightLocal;

// lighting models the approximate lighting shader can consume
enum lightingModel_t {
	LM_PRIME_AND_CHANNEL = 0,
	LM_LOCAL_LIGHTS = 1
};

struct approximateLighting_t {
	void				Clear();

	idVec4				lightingModel;
	idVec4				primeLightDir;
	idVec4				lightOrigin[3];
	idVec4				lightColor[3];
	idVec4				channelLighting[6];
};

// one light contribution gathered for the approximate lighting job
struct lightContribution_t {
	const idRenderLightLocal *	light;
	idVec3				color;
};

#endif // __JOBS_APPROXIMATELIGHTING_APPROXIMATELIGHTING_H__
