#pragma hdrstop
#include "../../idlib/precompiled.h"

#include "globalshadows.h"

idCVar r_globalShadowsCoordinateBias( "r_globalShadowsCoordinateBias", "0.5", CVAR_FLOAT, "bias to the global shadow coordinate calculation" );
idCVar r_globalShadowsDepthBias( "r_globalShadowsDepthBias", "0", CVAR_FLOAT, "bias to the global shadow depth comparison" );
idCVar r_useGlobalShadows( "r_useGlobalShadows", "1", CVAR_BOOL, "apply the offline global shadow map to parallel light contributions" );

idGlobalShadows & R_GetGlobalShadows() {
	static idGlobalShadows globalShadows;
	return globalShadows;
}

idGlobalShadows::idGlobalShadows() {
	shadowMap = ( shadowMapCPU_t * )Mem_Alloc( sizeof( shadowMapCPU_t ), TAG_RENDER );
	shadowMap->width = 1024;
	shadowMap->map = NULL;
	shadowMap->mvp.Identity();
	shadowMap->coordinateBias = -0.5f;
	shadowMap->depthBias = -40.0f;
	shadowMap->useShadowMap = true;
}

idGlobalShadows::~idGlobalShadows() {
	if ( shadowMap->map != NULL ) {
		Mem_Free( shadowMap->map );
		shadowMap->map = NULL;
	}
	Mem_Free( shadowMap );
}

void idGlobalShadows::LoadGlobalShadows( const char * mapName ) {
	if ( shadowMap->map != NULL ) {
		Mem_Free( shadowMap->map );
		shadowMap->map = NULL;
	}
	shadowMap->mvp.Identity();

	// Tech5 stores the map beside the map folder as <map>/globalShadows2.bin.
	// This engine keeps one .map file per level, so the shadow map is the map
	// file name with a .globalShadows2.bin extension instead.
	idStr shadowFilename = mapName;
	shadowFilename.StripFileExtension();
	shadowFilename += ".globalShadows2.bin";

	idFileLocal file( fileSystem->OpenFileRead( shadowFilename ) );
	if ( file == NULL ) {
		return;
	}
	// the file is a 64 byte matrix followed by a 1024x1024 16 bit depth map
	if ( file->Length() < ( int )( sizeof( shadowMap->mvp ) + 0x200000 ) ) {
		common->Warning( "globalShadows2.bin for %s is too small, ignoring", mapName );
		return;
	}

	idLib::Printf( "Loading globalShadows.\n" );
	file->Read( &shadowMap->mvp, sizeof( shadowMap->mvp ) );
	shadowMap->width = 1024;
	shadowMap->map = ( uint16 * )Mem_Alloc( 0x200000, TAG_RENDER );
	file->Read( shadowMap->map, 0x200000 );

	shadowMap->coordinateBias = r_globalShadowsCoordinateBias.GetFloat();
	shadowMap->depthBias = r_globalShadowsDepthBias.GetFloat();
	shadowMap->useShadowMap = r_useGlobalShadows.GetBool();
}

const shadowMapCPU_t * idGlobalShadows::GetShadowMap() {
	shadowMap->coordinateBias = r_globalShadowsCoordinateBias.GetFloat();
	shadowMap->depthBias = r_globalShadowsDepthBias.GetFloat();
	shadowMap->useShadowMap = r_useGlobalShadows.GetBool();
	return shadowMap;
}
