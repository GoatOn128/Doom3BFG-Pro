#pragma hdrstop
#include "../idlib/precompiled.h"

#include "tr_local.h"

idRenderLight::idRenderLight() {
	memset( static_cast< renderLight_t * >( this ), 0, sizeof( renderLight_t ) );
	world = NULL;
	index = -1;
	committed = NULL;
	stagedLightShader = NULL;
	stagedFalloffImage = NULL;
	needsCommit = false;
	worldOwned = false;
}

idRenderLight::~idRenderLight() {
	FreeRenderLight();
}

void idRenderLight::CommitThisFrame( bool forceUpdate ) {
	(void)forceUpdate;
	if ( world != NULL && index >= 0 ) {
		// Resolve declaration-backed resources while the game-facing producer is
		// being staged. PostCommit is renderer-only and must never discover or
		// load a material declaration.
		R_ResolveLightResources( *this, stagedLightShader, stagedFalloffImage );
		if ( shader != NULL && shader->Spectrum() ) {
			noShadows = true;
		}
		tr.pc.c_lightCommitRequests++;
		needsCommit = true;
	}
}

void idRenderLight::Commit() {
	if ( world == NULL || index < 0 || !needsCommit ) {
		return;
	}
	// AddRenderLight queues through CommitThisFrame, but retain this check for
	// restored or externally staged producers that predate registration.
	if ( stagedLightShader == NULL ) {
		R_ResolveLightResources( *this, stagedLightShader, stagedFalloffImage );
	}
	idRenderWorldLocal * renderWorld = static_cast< idRenderWorldLocal * >( world );
	idRenderLightCommitted * light = renderWorld->lightDefs[index];
	if ( light == NULL ) {
		light = new (TAG_RENDER_LIGHT) idRenderLightCommitted;
		light->world = renderWorld;
		light->index = index;
		renderWorld->lightDefs[index] = light;
	}
	const renderLight_t & staged = *this;
	const bool hasCommittedState = light->lightShader != NULL;
	bool shapeUnchanged = false;
	if ( hasCommittedState ) {
		shapeUnchanged = staged.axis == light->parms.axis && staged.end == light->parms.end &&
			staged.lightCenter == light->parms.lightCenter && staged.lightRadius == light->parms.lightRadius &&
			staged.noShadows == light->parms.noShadows && staged.origin == light->parms.origin &&
			staged.parallel == light->parms.parallel && staged.pointLight == light->parms.pointLight &&
			staged.right == light->parms.right && staged.start == light->parms.start &&
			staged.target == light->parms.target && staged.up == light->parms.up &&
			stagedLightShader == light->lightShader;
	}
	light->parms = staged;
	light->lightShader = stagedLightShader;
	light->falloffImage = stagedFalloffImage;
	light->needsReferences = !shapeUnchanged;
	light->needsPostCommit = true;
	light->lastModifiedFrameNum = tr.frameCount;
	committed = light;
	needsCommit = false;
	renderWorld->committedWorld->PublishLight( index, light, true );
}

void idRenderLight::FreeRenderLight() {
	if ( world != NULL && index >= 0 ) {
		world->FreeLightDef( index );
	}
}

void idRenderLight::ForceUpdate() {
	CommitThisFrame( true );
}
