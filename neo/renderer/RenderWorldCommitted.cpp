#pragma hdrstop
#include "../idlib/precompiled.h"

#include "tr_local.h"

idRenderWorldCommitted::idRenderWorldCommitted( idRenderWorldLocal * owner ) : world( owner ) {
}

static void R_GrowCommittedEntitySlots( idList< idRenderEntityCommitted * > & objects, idList< byte > & flags, int index ) {
	while ( objects.Num() <= index ) {
		objects.Append( NULL );
		flags.Append( 0 );
	}
}

static void R_GrowCommittedLightSlots( idList< idRenderLightCommitted * > & objects, idList< byte > & flags, int index ) {
	while ( objects.Num() <= index ) {
		objects.Append( NULL );
		flags.Append( 0 );
	}
}

void idRenderWorldCommitted::PublishEntity( int index, idRenderEntityCommitted * entity, bool postCommit ) {
	R_GrowCommittedEntitySlots( committedRenderEntities, postCommitEntities, index );
	committedRenderEntities[index] = entity;
	postCommitEntities[index] = ( postCommitEntities[index] != 0 || postCommit ) ? 1 : 0;
}

void idRenderWorldCommitted::PublishLight( int index, idRenderLightCommitted * light, bool postCommit ) {
	R_GrowCommittedLightSlots( committedRenderLights, postCommitLights, index );
	committedRenderLights[index] = light;
	postCommitLights[index] = ( postCommitLights[index] != 0 || postCommit ) ? 1 : 0;
}

void idRenderWorldCommitted::ForgetEntity( int index ) {
	if ( index >= 0 && index < committedRenderEntities.Num() ) {
		committedRenderEntities[index] = NULL;
		postCommitEntities[index] = 0;
	}
}

void idRenderWorldCommitted::ForgetLight( int index ) {
	if ( index >= 0 && index < committedRenderLights.Num() ) {
		committedRenderLights[index] = NULL;
		postCommitLights[index] = 0;
	}
}

void idRenderWorldCommitted::PostCommitData() {
	for ( int i = 0; i < committedRenderEntities.Num(); ++i ) {
		if ( postCommitEntities[i] != 0 && committedRenderEntities[i] != NULL ) {
			postCommitEntities[i] = 0;
			committedRenderEntities[i]->PostCommit();
		}
	}
	for ( int i = 0; i < committedRenderLights.Num(); ++i ) {
		if ( postCommitLights[i] != 0 && committedRenderLights[i] != NULL ) {
			postCommitLights[i] = 0;
			committedRenderLights[i]->PostCommit();
		}
	}
}
