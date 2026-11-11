#pragma hdrstop
#include "../../../idlib/precompiled.h"

#include "shadowmapcpu.h"

/*
================
shadowMapCPU_t::ShadowSample

Bilinear sample of the global shadow depth map. Returns the lit fraction at
the world point: 1.0 means fully lit, 0.0 means fully shadowed. Re-implements
the Tech5 semantics on idlib, nothing was copied from the original code.
================
*/
float shadowMapCPU_t::ShadowSample( const idVec3 & worldPoint ) const {
	if ( !useShadowMap || map == NULL ) {
		return 1.0f;
	}

	// row vector times matrix, the idRenderMatrix convention
	const float projectedX = worldPoint.x * mvp.m[0] + worldPoint.y * mvp.m[1] + worldPoint.z * mvp.m[2] + mvp.m[3];
	const float projectedY = worldPoint.x * mvp.m[4] + worldPoint.y * mvp.m[5] + worldPoint.z * mvp.m[6] + mvp.m[7];
	const float projectedZ = worldPoint.x * mvp.m[8] + worldPoint.y * mvp.m[9] + worldPoint.z * mvp.m[10] + mvp.m[11];

	int coords[2];
	float fractions[2][2];
	const float projected2D[2] = { projectedX, projectedY };
	for ( int axis = 0; axis < 2; axis++ ) {
		const float coordinate = width * projected2D[axis] + coordinateBias;
		const float integral = idMath::Floor( coordinate );
		coords[axis] = idMath::Ftoi( integral );
		fractions[axis][1] = coordinate - integral;
		if ( coords[axis] < 0 ) {
			coords[axis] = 0;
			fractions[axis][1] = 0.0f;
		} else if ( coords[axis] >= width - 1 ) {
			coords[axis] = width - 2;
			fractions[axis][1] = 1.0f;
		}
		fractions[axis][0] = 1.0f - fractions[axis][1];
	}

	const int sampleOffsets[4] = {
		coords[0] + coords[1] * width,
		coords[0] + 1 + coords[1] * width,
		coords[0] + ( coords[1] + 1 ) * width,
		coords[0] + 1 + ( coords[1] + 1 ) * width };
	float clampedDepth = projectedZ;
	clampedDepth = Max( Min( clampedDepth, 1.0f ), 0.0f );
	const float depth = clampedDepth * 65535.0f + depthBias;
	const unsigned int sampleDepth = ( unsigned int )idMath::Ftoi( depth );
	const float equalDepthWeight = 1.0f - ( depth - idMath::Floor( depth ) );

	float shadow = 0.0f;
	for ( int x = 0; x < 2; x++ ) {
		for ( int y = 0; y < 2; y++ ) {
			int offset = sampleOffsets[y * 2 + x];
			if ( offset < 0 ) {
				offset = -offset;
			}
			const unsigned int mapDepth = map[offset];
			const float weight = fractions[1][y] * fractions[0][x];
			if ( mapDepth > sampleDepth ) {
				shadow += weight;
			} else if ( mapDepth == sampleDepth ) {
				shadow += equalDepthWeight * weight;
			}
		}
	}
	return shadow;
}
