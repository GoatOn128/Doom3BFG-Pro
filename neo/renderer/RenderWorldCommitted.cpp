#pragma hdrstop
#include "../idlib/precompiled.h"

#include "tr_local.h"

#include "ambientlighting.h"

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

/*
===================
R_LightColorForQuery

Color of the first active light shader stage, evaluated with the light
own shader parms. Used outside a view where no viewLight registers exist.
===================
*/
static idVec3 R_LightColorForQuery( const idRenderLightCommitted * light ) {
	const idMaterial * lightShader = light->lightShader;
	if ( lightShader == NULL || lightShader->IsFogLight() || lightShader->IsBlendLight() ) {
		return idVec3( 0.0f, 0.0f, 0.0f );
	}
	float * registers = (float *)_alloca16( lightShader->GetNumRegisters() * sizeof( float ) );
	const float globalShaderParms[MAX_GLOBAL_SHADER_PARMS] = {};
	lightShader->EvaluateRegisters( registers, light->parms.shaderParms, globalShaderParms, 0.0f, NULL );
	for ( int s = 0; s < lightShader->GetNumStages(); s++ ) {
		const shaderStage_t * stage = lightShader->GetStage( s );
		if ( registers[stage->conditionRegister] == 0.0f ) {
			continue;
		}
		return idVec3( registers[stage->color.registers[0]], registers[stage->color.registers[1]], registers[stage->color.registers[2]] );
	}
	return idVec3( 0.0f, 0.0f, 0.0f );
}

/*
===================
idRenderWorldCommitted::ApproximateLightingForBounds

Tech5 game facing query: approximate lighting for an arbitrary world
bounds from the committed lights that touch it, without needing a view.
===================
*/
void idRenderWorldCommitted::ApproximateLightingForBounds( const idBounds & bounds, approximateLighting_t & lighting ) const {
	lighting.Clear();

	approximateLightParms_t parms;
	memset( &parms, 0, sizeof( parms ) );
	parms.modelOrigin = bounds.GetCenter();
	parms.viewOrigin = parms.modelOrigin;
	parms.ambientMap = R_GetAmbientLighting().ambientMap;

	// gather the unique committed lights in the areas the bounds touch
	const int maxLights = 128;
	const idRenderLightCommitted * lights[128];
	int numLights = 0;
	int areas[128];
	const int numAreas = world->BoundsInAreas( bounds, areas, 128 );
	for ( int i = 0; i < numAreas && numLights < maxLights; i++ ) {
		const portalArea_t * area = &world->portalAreas[areas[i]];
		for ( const areaReference_t * ref = area->lightRefs.areaNext; ref != &area->lightRefs; ref = ref->areaNext ) {
			if ( ref->light == NULL ) {
				continue;
			}
			bool duplicate = false;
			for ( int j = 0; j < numLights; j++ ) {
				if ( lights[j] == ref->light ) {
					duplicate = true;
					break;
				}
			}
			if ( !duplicate ) {
				lights[numLights++] = ref->light;
				if ( numLights == maxLights ) {
					break;
				}
			}
		}
	}

	for ( int i = 0; i < numLights && parms.numContributingLights < MAX_CONTRIBUTING_LIGHTS; i++ ) {
		// parallel lights have no radial origin for the self shadow slots
		if ( lights[i]->parms.parallel ) {
			continue;
		}
		const idVec3 color = R_LightColorForQuery( lights[i] );
		if ( color.LengthSqr() <= 0.0f ) {
			continue;
		}
		contributingLight_t & cl = parms.contributingLights[parms.numContributingLights++];
		// w == 1.0f marks a light that never casts shadows
		const float noShadows = lights[i]->parms.noShadows ? 1.0f : 0.0f;
		cl.lightOrigin = idVec4( lights[i]->globalLightOrigin.x, lights[i]->globalLightOrigin.y, lights[i]->globalLightOrigin.z, noShadows );
		cl.lightColor = color;
	}

	LocalLighting( parms, lighting );
}

/*
===================
idRenderWorldCommitted::ApproximateAmbientForBounds

Ambient only variant: blends the offline ambient map over the bounds
without gathering radial lights.
===================
*/
void idRenderWorldCommitted::ApproximateAmbientForBounds( const idBounds & bounds, approximateLighting_t & lighting ) const {
	lighting.Clear();

	approximateLightParms_t parms;
	memset( &parms, 0, sizeof( parms ) );
	parms.modelOrigin = bounds.GetCenter();
	parms.viewOrigin = parms.modelOrigin;
	parms.ambientMap = R_GetAmbientLighting().ambientMap;

	AmbientOnlyLighting( parms, lighting );
}

