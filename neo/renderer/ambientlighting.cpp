#pragma hdrstop
#include "../../idlib/precompiled.h"

#include "ambientlighting.h"

#include "RenderWorld.h"

// the .ambient file magic written by the offline compiler
static const int AMBIENT_FILE_MAGIC = 0x21155701;

idAmbientLighting & R_GetAmbientLighting() {
	static idAmbientLighting ambientLighting;
	return ambientLighting;
}

/*
================
SwapBytes16 / SwapBytes32

The offline compiler writes big endian data.
================
*/
static uint16 SwapBytes16( uint16 value ) {
	return ( uint16 )( ( value << 8 ) | ( value >> 8 ) );
}

static uint32 SwapBytes32( uint32 value ) {
	return ( ( value & 0x000000FFu ) << 24 ) | ( ( value & 0x0000FF00u ) << 8 ) |
		( ( value & 0x00FF0000u ) >> 8 ) | ( ( value & 0xFF000000u ) >> 24 );
}

idAmbientLighting::idAmbientLighting() {
	ambientMap = ( ambientMapCPU_t * )Mem_Alloc( sizeof( ambientMapCPU_t ), TAG_RENDER );
	memset( ambientMap, 0, sizeof( ambientMapCPU_t ) );
}

idAmbientLighting::~idAmbientLighting() {
	FreeData();
	Mem_Free( ambientMap );
}

void idAmbientLighting::FreeData() {
	if ( ambientMap->data != NULL ) {
		Mem_Free( ambientMap->data );
		ambientMap->data = NULL;
		ambientMap->samples = NULL;
		ambientMap->nodes = NULL;
	}
}

void idAmbientLighting::LoadAmbientLighting( const char * mapName ) {
	FreeData();

	// Tech5 stores the file as <map>.ambient beside the map; this engine
	// keeps one .map file per level, so the ambient file is the map file
	// name with a .ambient extension instead.
	idStr filename = mapName;
	filename.StripFileExtension();
	filename += ".ambient";

	idFileLocal file( fileSystem->OpenFileRead( filename ) );
	if ( file == NULL ) {
		return;
	}

	const int dataLen = file->Length();
	if ( dataLen <= ( int )sizeof( ambientHeader_t ) ) {
		common->Warning( "idAmbientLighting::LoadAmbientLighting: file too small: %s", filename.c_str() );
		return;
	}

	idLib::Printf( "Loading ambientLighting.\n" );
	ambientMap->data = ( ambientHeader_t * )Mem_Alloc( dataLen, TAG_RENDER );
	ambientMap->dataLen = dataLen;
	file->Read( ambientMap->data, dataLen );

	ambientMap->data->magic = ( int )SwapBytes32( ( uint32 )ambientMap->data->magic );
	ambientMap->data->numNodes = ( int )SwapBytes32( ( uint32 )ambientMap->data->numNodes );
	ambientMap->data->numSamples = ( int )SwapBytes32( ( uint32 )ambientMap->data->numSamples );

	if ( ambientMap->data->magic != AMBIENT_FILE_MAGIC ) {
		common->Warning( "idAmbientLighting::LoadAmbientLighting: bad file magic on %s", filename.c_str() );
		FreeData();
		return;
	}

	ambientMap->nodes = ( ambientNode_t * )( ambientMap->data + 1 );
	ambientMap->samples = ( ambientSample_t * )( ambientMap->nodes + ambientMap->data->numNodes );

	// swap the big endian nodes and samples in place
	for ( int i = 0; i < ambientMap->data->numNodes; i++ ) {
		for ( int j = 0; j < 8; j++ ) {
			ambientMap->nodes[i].children[j] = SwapBytes16( ambientMap->nodes[i].children[j] );
		}
	}
	for ( int i = 0; i < ambientMap->data->numSamples; i++ ) {
		for ( int j = 0; j < 3; j++ ) {
			ambientMap->samples[i].origin[j] = ( int16 )SwapBytes16( ( uint16 )ambientMap->samples[i].origin[j] );
		}
	}

	// an octree without nodes has nothing to sample
	if ( ambientMap->data->numNodes == 0 ) {
		FreeData();
		return;
	}
	ambientMap->useForcedColors = false;
}

/*
================
idAmbientLighting::DebugDrawAmbientLightingSamples

Draws the leaf cube holding the position and lines from its corners to the
samples the ambient sample blends from.
================
*/
void idAmbientLighting::DebugDrawAmbientLightingSamples( idRenderWorld * renderWorld, const idVec3 & position ) const {
	if ( renderWorld == NULL || ambientMap->data == NULL ) {
		return;
	}

	int cube[4];
	ambientSample_t cubeSamples[8];
	ambientMap->SamplesForPoint( position, cube, cubeSamples );

	const idBounds bounds( idVec3( ( float )cube[0], ( float )cube[1], ( float )cube[2] ),
		idVec3( ( float )( cube[0] + cube[3] ), ( float )( cube[1] + cube[3] ), ( float )( cube[2] + cube[3] ) ) );
	renderWorld->DebugBounds( idVec4( 0.0f, 1.0f, 1.0f, 1.0f ), bounds, vec3_origin, 1 );

	const idVec4 lineColor( 1.0f, 0.0f, 0.0f, 1.0f );
	for ( int index = 0; index < 8; index++ ) {
		const idVec3 corner( bounds[( index >> 0 ) & 1].x, bounds[( index >> 1 ) & 1].y, bounds[( index >> 2 ) & 1].z );
		const idVec3 sampleOrigin( ( float )cubeSamples[index].origin[0], ( float )cubeSamples[index].origin[1], ( float )cubeSamples[index].origin[2] );
		renderWorld->DebugLine( lineColor, corner, sampleOrigin, 1, true );
	}
}
