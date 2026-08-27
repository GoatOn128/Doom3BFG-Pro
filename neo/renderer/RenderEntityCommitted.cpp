#pragma hdrstop
#include "../idlib/precompiled.h"

#include "tr_local.h"
#include "jobs/render/rendergather.h"

idRenderEntityCommitted::idRenderEntityCommitted() {
	memset( &parms, 0, sizeof( parms ) );
	memset( modelMatrix, 0, sizeof( modelMatrix ) );
	world = NULL;
	index = 0;
	lastModifiedFrameNum = 0;
	archived = false;
	dynamicModel = NULL;
	dynamicModelFrameCount = 0;
	cachedDynamicModel = NULL;
	localReferenceBounds = bounds_zero;
	globalReferenceBounds = bounds_zero;
	viewCount = 0;
	viewEntity = NULL;
	decals = NULL;
	overlays = NULL;
	entityRefs = NULL;
	needsPortalSky = false;
	postCommitMode = 0;
}

void idRenderEntityCommitted::PostCommit() {
	if ( world == NULL ) {
		return;
	}

	const byte mode = postCommitMode;
	postCommitMode = 0;
	if ( mode == 1 ) {
		R_ClearEntityDefDynamicModel( this );
		return;
	}
	if ( entityRefs != NULL || dynamicModel != NULL || cachedDynamicModel != NULL ) {
		const bool preserveMarks = mode == 2;
		R_FreeEntityDefDerivedData( this, preserveMarks, preserveMarks );
	}
	if ( common->WriteDemo() && archived ) {
		world->WriteFreeEntity( index );
		archived = false;
	}
	if ( !r_useEntityCallbacks.GetBool() && parms.callback != NULL ) {
		R_IssueEntityDefCallback( this );
	}
	if ( parms.hModel != NULL && !parms.hModel->ModelHasDrawingSurfaces() ) {
		return;
	}
	R_CreateEntityRefs( this );
}
