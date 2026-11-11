#ifndef __JOBS_APPROXIMATELIGHTING_SHADOWMAPCPU_H__
#define __JOBS_APPROXIMATELIGHTING_SHADOWMAPCPU_H__

/*
================================================================================================

Tech5 style CPU side global shadow map.

A 1024 x 1024 16 bit depth map with the light matrix it was rendered with,
produced by an offline compiler. The approximate lighting code samples it to
attenuate parallel light contributions, matching the Tech5 renderer.

================================================================================================
*/

#include "../../../idlib/geometry/RenderMatrix.h"
#include "../../../idlib/math/Vector.h"
#include "../../../idlib/sys/sys_types.h"

struct shadowMapCPU_t {
	float				ShadowSample( const idVec3 & worldPoint ) const;

	int					width;
	uint16 *			map;
	idRenderMatrix		mvp;
	float				coordinateBias;
	float				depthBias;
	bool				useShadowMap;
};

#endif // __JOBS_APPROXIMATELIGHTING_SHADOWMAPCPU_H__
