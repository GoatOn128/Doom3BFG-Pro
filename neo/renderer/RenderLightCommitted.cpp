#pragma hdrstop
#include "../idlib/precompiled.h"

#include "tr_local.h"

idRenderLightCommitted::idRenderLightCommitted() {
	memset( &parms, 0, sizeof( parms ) );
	memset( lightProject, 0, sizeof( lightProject ) );
	world = NULL;
	index = 0;
	areaNum = 0;
	lastModifiedFrameNum = 0;
	archived = false;
	lightShader = NULL;
	falloffImage = NULL;
	globalLightOrigin = vec3_zero;
	viewCount = 0;
	viewLight = NULL;
	references = NULL;
	needsReferences = false;
	foggedPortals = NULL;
	needsPostCommit = false;
	baseLightProject.Zero();
	inverseBaseLightProject.Zero();
}

void idRenderLightCommitted::PostCommit() {
	if ( world == NULL || !needsPostCommit ) {
		return;
	}

	if ( needsReferences ) {
		R_FreeLightDefDerivedData( this );
		R_CreateLightRefs( this );
		needsReferences = false;
		tr.pc.c_lightReferenceCommits++;
	}

	needsPostCommit = false;
	if ( common->WriteDemo() && archived ) {
		world->WriteFreeLight( index );
		archived = false;
	}
	tr.pc.c_lightCommits++;
}
