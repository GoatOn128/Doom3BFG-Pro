#ifndef __JOBS_APPROXIMATELIGHTING_AMBIENTMAPCPU_H__
#define __JOBS_APPROXIMATELIGHTING_AMBIENTMAPCPU_H__

/*
================================================================================================

Tech5 style CPU side ambient map.

An octree of ambient color samples covering the map, produced by an offline
compiler and stored in the .ambient file next to the map. Each sample holds
six channel colors, one per cube face direction, so the approximate lighting
code can light a point from every direction. Re-implemented on idlib from
the Tech5 semantics, nothing was copied from the original code.

================================================================================================
*/

#include "../../../idlib/math/Vector.h"
#include "../../../idlib/sys/sys_types.h"

// the .ambient file layout: header, then numNodes nodes, then numSamples samples
struct ambientHeader_t {
	int					magic;
	int					numNodes;
	int					numSamples;
};

struct ambientNode_t {
	uint16				children[8];
};

struct ambientColors_t {
	uint8				channels[6][3];
};

struct ambientSample_t {
	ambientColors_t		colors;
	int16				origin[3];
};

struct ambientMapCPU_t {
	void				SamplesForPoint( const idVec3 & worldPoint, int * cube, ambientSample_t * cubeSamples ) const;
	void				AmbientSample( const idVec3 & worldPoint, ambientColors_t * lighting ) const;

	ambientHeader_t *	data;
	int					dataLen;
	ambientColors_t		forcedColors;
	bool				useForcedColors;
	ambientNode_t *		nodes;
	ambientSample_t *	samples;
};

// file format layout checks, the compiler tool must match these exactly
static_assert( sizeof( ambientHeader_t ) == 12, "ambient header layout drift" );
static_assert( sizeof( ambientNode_t ) == 16, "ambient node layout drift" );
static_assert( sizeof( ambientColors_t ) == 18, "ambient colors layout drift" );
static_assert( sizeof( ambientSample_t ) == 24, "ambient sample layout drift" );

#endif // __JOBS_APPROXIMATELIGHTING_AMBIENTMAPCPU_H__
