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

/*
==================
idRenderEntityCommitted::GetLightingPoint

Tech5 lighting sample point: the game can override it per model with
parms.localLightPositionOverride in local space, otherwise it comes from
the global reference bounds like the default case.
==================
*/
idVec3 idRenderEntityCommitted::GetLightingPoint() const {
	const idVec3 & overridePoint = parms.localLightPositionOverride;
	if ( overridePoint.x != 0.0f || overridePoint.y != 0.0f || overridePoint.z != 0.0f ) {
		return idVec3(
			overridePoint.x * modelRenderMatrix[0][0] + overridePoint.y * modelRenderMatrix[1][0] + overridePoint.z * modelRenderMatrix[2][0] + modelRenderMatrix[3][0],
			overridePoint.x * modelRenderMatrix[0][1] + overridePoint.y * modelRenderMatrix[1][1] + overridePoint.z * modelRenderMatrix[2][1] + modelRenderMatrix[3][1],
			overridePoint.x * modelRenderMatrix[0][2] + overridePoint.y * modelRenderMatrix[1][2] + overridePoint.z * modelRenderMatrix[2][2] + modelRenderMatrix[3][2] );
	}
	return globalReferenceBounds.GetCenter();
}

/*
==================
idRenderEntityCommitted::GetLightingRadius
==================
*/
float idRenderEntityCommitted::GetLightingRadius() const {
	return globalReferenceBounds.GetRadius( GetLightingPoint() );
}

