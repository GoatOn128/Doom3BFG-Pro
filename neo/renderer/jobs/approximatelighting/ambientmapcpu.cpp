#pragma hdrstop
#include "../../../idlib/precompiled.h"

#include "ambientmapcpu.h"

/*
================
ClampByte
================
*/
static uint8 ClampByte( int value ) {
	if ( value < 0 ) {
		return 0;
	}
	if ( value > 255 ) {
		return 255;
	}
	return ( uint8 )value;
}

/*
================
DistanceSquared
================
*/
static int64 DistanceSquared( const ambientSample_t & sample, const int64 corner[3] ) {
	const int64 dx = ( int64 )sample.origin[0] - corner[0];
	const int64 dy = ( int64 )sample.origin[1] - corner[1];
	const int64 dz = ( int64 )sample.origin[2] - corner[2];
	return dx * dx + dy * dy + dz * dz;
}

/*
================
ambientMapCPU_t::SamplesForPoint

Walks the octree down to the leaf holding the world point and fills
cubeSamples with the eight samples nearest the leaf cube corners. cube gets
the leaf bounds as x, y, z, size. Re-implements the Tech5 semantics on idlib.
================
*/
void ambientMapCPU_t::SamplesForPoint( const idVec3 & worldPoint, int * cube, ambientSample_t * cubeSamples ) const {
	if ( nodes == NULL ) {
		memset( cubeSamples, 0, sizeof( ambientSample_t ) * 8 );
		cube[0] = -32768;
		cube[1] = -32768;
		cube[2] = -32768;
		cube[3] = 65536;
		return;
	}

	const int point[3] = { idMath::Ftoi( worldPoint.x ), idMath::Ftoi( worldPoint.y ), idMath::Ftoi( worldPoint.z ) };
	int bounds[2][3] = { { -32768, -32768, -32768 }, { 32768, 32768, 32768 } };
	int center[3] = { 0, 0, 0 };
	bool side[3] = { point[0] > 0, point[1] > 0, point[2] > 0 };

	// walk down the octree, high bit set means the child is a sample index
	const ambientNode_t * node = nodes;
	uint16 child = node->children[( side[0] ? 1 : 0 ) | ( side[1] ? 2 : 0 ) | ( side[2] ? 4 : 0 )];
	while ( ( child & 0x8000u ) == 0 ) {
		for ( int axis = 0; axis < 3; axis++ ) {
			bounds[side[axis] ? 0 : 1][axis] = center[axis];
			center[axis] = ( bounds[0][axis] + bounds[1][axis] ) / 2;
			side[axis] = point[axis] > center[axis];
		}
		node = &nodes[child];
		child = node->children[( side[0] ? 1 : 0 ) | ( side[1] ? 2 : 0 ) | ( side[2] ? 4 : 0 )];
	}

	// best case the leaf holds exactly the eight cube corner samples
	int sampleIndices[8];
	bool allChildrenAreSamples = true;
	for ( int i = 0; i < 8; i++ ) {
		if ( ( node->children[i] & 0x8000u ) == 0 ) {
			allChildrenAreSamples = false;
			break;
		}
	}
	if ( allChildrenAreSamples ) {
		for ( int index = 0; index < 8; index++ ) {
			sampleIndices[index] = node->children[index] & 0x7FFFu;
		}
	} else {
		// otherwise nearest neighbor filtering inside the leaf cube
		const int sampleIndex = child & 0x7FFFu;
		for ( int index = 0; index < 8; index++ ) {
			sampleIndices[index] = sampleIndex;
		}
		for ( int axis = 0; axis < 3; axis++ ) {
			bounds[side[axis] ? 0 : 1][axis] = center[axis];
		}
	}

	// identical consecutive samples only need to be evaluated once
	int sampleCount = 1;
	while ( sampleCount < 8 && sampleIndices[sampleCount] != sampleIndices[sampleCount - 1] ) {
		sampleCount++;
	}

	const ambientSample_t * localSamples[8];
	for ( int index = 0; index < sampleCount; index++ ) {
		const int sampleIndex = sampleIndices[index];
		const int absoluteIndex = sampleIndex < 0 ? -sampleIndex : sampleIndex;
		localSamples[index] = &samples[absoluteIndex];
	}

	// per cube corner, the closest of the leaf samples
	int64 corners[8][3];
	int64 closestDistanceSquared[8];
	int closestSamples[8] = { 0 };
	for ( int cornerIndex = 0; cornerIndex < 8; cornerIndex++ ) {
		corners[cornerIndex][0] = bounds[( cornerIndex >> 0 ) & 1][0];
		corners[cornerIndex][1] = bounds[( cornerIndex >> 1 ) & 1][1];
		corners[cornerIndex][2] = bounds[( cornerIndex >> 2 ) & 1][2];
		closestDistanceSquared[cornerIndex] = DistanceSquared( *localSamples[0], corners[cornerIndex] );
	}

	for ( int sampleIndex = 1; sampleIndex < sampleCount; sampleIndex++ ) {
		for ( int cornerIndex = 0; cornerIndex < 8; cornerIndex++ ) {
			const int64 distance = DistanceSquared( *localSamples[sampleIndex], corners[cornerIndex] );
			if ( distance < closestDistanceSquared[cornerIndex] ) {
				closestDistanceSquared[cornerIndex] = distance;
				closestSamples[cornerIndex] = sampleIndex;
			}
		}
	}

	for ( int cornerIndex = 0; cornerIndex < 8; cornerIndex++ ) {
		cubeSamples[cornerIndex] = *localSamples[closestSamples[cornerIndex]];
	}
	cube[0] = bounds[0][0];
	cube[1] = bounds[0][1];
	cube[2] = bounds[0][2];
	cube[3] = bounds[1][0] - bounds[0][0];
}

/*
================
ambientMapCPU_t::AmbientSample

Trilinear blend of the eight cube corner samples at the world point.
Re-implements the Tech5 semantics on idlib.
================
*/
void ambientMapCPU_t::AmbientSample( const idVec3 & worldPoint, ambientColors_t * lighting ) const {
	if ( useForcedColors ) {
		*lighting = forcedColors;
		return;
	}
	if ( data == NULL ) {
		memset( lighting, 0, sizeof( *lighting ) );
		return;
	}

	int cube[4];
	ambientSample_t cubeSamples[8];
	for ( int i = 0; i < 8; i++ ) {
		memset( &cubeSamples[i].colors, 0, sizeof( cubeSamples[i].colors ) );
	}
	SamplesForPoint( worldPoint, cube, cubeSamples );

	const float inverseSize = 1.0f / ( float )cube[3];
	const float x = ( worldPoint.x - ( float )cube[0] ) * inverseSize;
	const float y = ( worldPoint.y - ( float )cube[1] ) * inverseSize;
	const float z = ( worldPoint.z - ( float )cube[2] ) * inverseSize;
	const float oneMinusX = 1.0f - x;
	const float oneMinusY = 1.0f - y;
	const float oneMinusZ = 1.0f - z;
	const float x0y0 = oneMinusX * oneMinusY;
	const float x1y0 = oneMinusY * x;
	const float x0y1 = oneMinusX * y;
	const float x1y1 = y * x;
	const float weights[8] = { oneMinusZ * x0y0, x1y0 * oneMinusZ, x0y1 * oneMinusZ, x1y1 * oneMinusZ,
		x0y0 * z, x1y0 * z, x0y1 * z, x1y1 * z };

	for ( int direction = 0; direction < 6; direction++ ) {
		for ( int channel = 0; channel < 3; channel++ ) {
			float value = ( float )cubeSamples[0].colors.channels[direction][channel] * weights[0];
			for ( int corner = 1; corner < 8; corner++ ) {
				value += ( float )cubeSamples[corner].colors.channels[direction][channel] * weights[corner];
			}
			lighting->channels[direction][channel] = ClampByte( idMath::Ftoi( value ) );
		}
	}
}
