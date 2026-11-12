/*
===========================================================================

Doom 3 BFG Edition GPL Source Code
Copyright (C) 1993-2012 id Software LLC, a ZeniMax Media company. 

This file is part of the Doom 3 BFG Edition GPL Source Code ("Doom 3 BFG Edition Source Code").  

Doom 3 BFG Edition Source Code is free software: you can redistribute it and/or modify
it under the terms of the GNU General Public License as published by
the Free Software Foundation, either version 3 of the License, or
(at your option) any later version.

Doom 3 BFG Edition Source Code is distributed in the hope that it will be useful,
but WITHOUT ANY WARRANTY; without even the implied warranty of
MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
GNU General Public License for more details.

You should have received a copy of the GNU General Public License
along with Doom 3 BFG Edition Source Code.  If not, see <http://www.gnu.org/licenses/>.

In addition, the Doom 3 BFG Edition Source Code is also subject to certain additional terms. You should have received a copy of these additional terms immediately following the terms and conditions of the GNU General Public License which accompanied the Doom 3 BFG Edition Source Code.  If not, please request a copy in writing from id Software at the address below.

If you have questions concerning this license or the applicable additional terms, you may contact in writing id Software LLC, c/o ZeniMax Media Inc., Suite 120, Rockville, Maryland 20850 USA.

===========================================================================
*/

#pragma hdrstop
#include "../idlib/precompiled.h"

#include "tr_local.h"

/*
=================================================================================

ENTITY DEFS

=================================================================================
*/

/*
=================
R_DeriveEntityData
=================
*/
void R_DeriveEntityData( idRenderEntityCommitted * entity ) {
	R_AxisToModelMatrix( entity->parms.axis, entity->parms.origin, entity->modelMatrix );

	idRenderMatrix::CreateFromOriginAxis( entity->parms.origin, entity->parms.axis, entity->modelRenderMatrix );

	// calculate the matrix that transforms the unit cube to exactly cover the model in world space
	idRenderMatrix::OffsetScaleForBounds( entity->modelRenderMatrix, entity->localReferenceBounds, entity->inverseBaseModelProject );

	// calculate the global model bounds by inverse projecting the unit cube with the 'inverseBaseModelProject'
	idRenderMatrix::ProjectedBounds( entity->globalReferenceBounds, entity->inverseBaseModelProject, bounds_unitCube, false );
}

/*
===================
R_FreeEntityDefDerivedData

Used by both FreeEntityDef and UpdateEntityDef
Does not actually free the entityDef.
===================
*/
void R_FreeEntityDefDerivedData( idRenderEntityCommitted *def, bool keepDecals, bool keepCachedDynamicModel ) {
	// demo playback needs to free the joints, while normal play
	// leaves them in the control of the game
	if ( common->ReadDemo() ) {
		if ( def->parms.joints ) {
			Mem_Free16( def->parms.joints );
			def->parms.joints = NULL;
		}
		if ( def->parms.callbackData ) {
			Mem_Free( def->parms.callbackData );
			def->parms.callbackData = NULL;
		}
		for ( int i = 0; i < MAX_RENDERENTITY_GUI; i++ ) {
			if ( def->parms.gui[ i ] ) {
				delete def->parms.gui[ i ];
				def->parms.gui[ i ] = NULL;
			}
		}
	}

	def->dynamicModelFrameCount = 0;

	// clear the dynamic model if present
	if ( def->dynamicModel ) {
		def->dynamicModel = NULL;
	}

	if ( !keepDecals ) {
		R_FreeEntityDefDecals( def );
		R_FreeEntityDefOverlay( def );
	}

	if ( !keepCachedDynamicModel ) {
		delete def->cachedDynamicModel;
		def->cachedDynamicModel = NULL;
	}

	// free the entityRefs from the areas
	areaReference_t * next = NULL;
	for ( areaReference_t * ref = def->entityRefs; ref != NULL; ref = next ) {
		next = ref->ownerNext;

		// unlink from the area
		ref->areaNext->areaPrev = ref->areaPrev;
		ref->areaPrev->areaNext = ref->areaNext;

		// put it back on the free list for reuse
		def->world->areaReferenceAllocator.Free( ref );
	}	
	def->entityRefs = NULL;
}

/*
===================
R_FreeEntityDefDecals
===================
*/
void R_FreeEntityDefDecals( idRenderEntityCommitted *def ) {
	def->decals = NULL;
}

/*
===================
R_FreeEntityDefFadedDecals
===================
*/
void R_FreeEntityDefFadedDecals( idRenderEntityCommitted *def, int time ) {
	if ( def->decals != NULL ) {
		def->decals->RemoveFadedDecals( time );
	}
}

/*
===================
R_FreeEntityDefOverlay
===================
*/
void R_FreeEntityDefOverlay( idRenderEntityCommitted *def ) {
	def->overlays = NULL;
}

/*
===============
R_CreateEntityRefs

Creates all needed model references in portal areas,
chaining them to both the area and the entityDef.

Bumps tr.viewCount, which means viewCount can change many times each frame.
===============
*/
void R_CreateEntityRefs( idRenderEntityCommitted *entity ) {
	if ( entity->parms.hModel == NULL ) {
		entity->parms.hModel = renderModelManager->DefaultModel();
	}

	// if the entity hasn't been fully specified due to expensive animation calcs
	// for md5 and particles, use the provided conservative bounds.
	if ( entity->parms.callback != NULL ) {
		entity->localReferenceBounds = entity->parms.bounds;
	} else {
		entity->localReferenceBounds = entity->parms.hModel->Bounds( &entity->parms );
	}

	// some models, like empty particles, may not need to be added at all
	if ( entity->localReferenceBounds.IsCleared() ) {
		return;
	}

	if ( r_showUpdates.GetBool() && 
			( entity->localReferenceBounds[1][0] - entity->localReferenceBounds[0][0] > 1024.0f ||
				entity->localReferenceBounds[1][1] - entity->localReferenceBounds[0][1] > 1024.0f ) ) {
		common->Printf( "big entityRef: %f,%f\n", entity->localReferenceBounds[1][0] - entity->localReferenceBounds[0][0],
						entity->localReferenceBounds[1][1] - entity->localReferenceBounds[0][1] );
	}

	// derive entity data
	R_DeriveEntityData( entity );

	// bump the view count so we can tell if an
	// area already has a reference
	tr.viewCount++;

	// push the model frustum down the BSP tree into areas
	entity->world->PushFrustumIntoTree( entity, NULL, entity->inverseBaseModelProject, bounds_unitCube );
}

/*
=================================================================================

LIGHT DEFS

=================================================================================
*/

/*
========================
R_ComputePointLightProjectionMatrix

Computes the light projection matrix for a point light.
========================
*/
static float R_ComputePointLightProjectionMatrix( idRenderLightCommitted * light, idRenderMatrix & localProject ) {
	assert( light->parms.pointLight );

	// A point light uses a box projection.
	// This projects into the 0.0 - 1.0 texture range instead of -1.0 to 1.0 clip space range.
	localProject.Zero();
	localProject[0][0] = 0.5f / light->parms.lightRadius[0];
	localProject[1][1] = 0.5f / light->parms.lightRadius[1];
	localProject[2][2] = 0.5f / light->parms.lightRadius[2];
	localProject[0][3] = 0.5f;
	localProject[1][3] = 0.5f;
	localProject[2][3] = 0.5f;
	localProject[3][3] = 1.0f;	// identity perspective

	return 1.0f;
}

static const float SPOT_LIGHT_MIN_Z_NEAR	= 8.0f;
static const float SPOT_LIGHT_MIN_Z_FAR		= 16.0f;

/*
========================
R_ComputeSpotLightProjectionMatrix

Computes the light projection matrix for a spot light.
========================
*/
static float R_ComputeSpotLightProjectionMatrix( idRenderLightCommitted * light, idRenderMatrix & localProject ) {
	const float targetDistSqr = light->parms.target.LengthSqr();
	const float invTargetDist = idMath::InvSqrt( targetDistSqr );
	const float targetDist = invTargetDist * targetDistSqr;

	const idVec3 normalizedTarget = light->parms.target * invTargetDist;
	const idVec3 normalizedRight = light->parms.right * ( 0.5f * targetDist / light->parms.right.LengthSqr() );
	const idVec3 normalizedUp = light->parms.up * ( -0.5f * targetDist / light->parms.up.LengthSqr() );

	localProject[0][0] = normalizedRight[0];
	localProject[0][1] = normalizedRight[1];
	localProject[0][2] = normalizedRight[2];
	localProject[0][3] = 0.0f;

	localProject[1][0] = normalizedUp[0];
	localProject[1][1] = normalizedUp[1];
	localProject[1][2] = normalizedUp[2];
	localProject[1][3] = 0.0f;

	localProject[3][0] = normalizedTarget[0];
	localProject[3][1] = normalizedTarget[1];
	localProject[3][2] = normalizedTarget[2];
	localProject[3][3] = 0.0f;

	// Set the falloff vector.
	// This is similar to the Z calculation for depth buffering, which means that the
	// mapped texture is going to be perspective distorted heavily towards the zero end.
	const float zNear = Max( light->parms.start * normalizedTarget, SPOT_LIGHT_MIN_Z_NEAR );
	const float zFar = Max( light->parms.end * normalizedTarget, SPOT_LIGHT_MIN_Z_FAR );
	const float zScale = ( zNear + zFar ) / zFar;

	localProject[2][0] = normalizedTarget[0] * zScale;
	localProject[2][1] = normalizedTarget[1] * zScale;
	localProject[2][2] = normalizedTarget[2] * zScale;
	localProject[2][3] = - zNear * zScale;

	// now offset to the 0.0 - 1.0 texture range instead of -1.0 to 1.0 clip space range
	idVec4 projectedTarget;
	localProject.TransformPoint( light->parms.target, projectedTarget );

	const float ofs0 = 0.5f - projectedTarget[0] / projectedTarget[3];
	localProject[0][0] += ofs0 * localProject[3][0];
	localProject[0][1] += ofs0 * localProject[3][1];
	localProject[0][2] += ofs0 * localProject[3][2];
	localProject[0][3] += ofs0 * localProject[3][3];

	const float ofs1 = 0.5f - projectedTarget[1] / projectedTarget[3];
	localProject[1][0] += ofs1 * localProject[3][0];
	localProject[1][1] += ofs1 * localProject[3][1];
	localProject[1][2] += ofs1 * localProject[3][2];
	localProject[1][3] += ofs1 * localProject[3][3];

	return 1.0f / ( zNear + zFar );
}

/*
========================
R_ComputeParallelLightProjectionMatrix

Computes the light projection matrix for a parallel light.
========================
*/
static float R_ComputeParallelLightProjectionMatrix( idRenderLightCommitted * light, idRenderMatrix & localProject ) {
	assert( light->parms.parallel );

	// A parallel light uses a box projection.
	// This projects into the 0.0 - 1.0 texture range instead of -1.0 to 1.0 clip space range.
	localProject.Zero();
	localProject[0][0] = 0.5f / light->parms.lightRadius[0];
	localProject[1][1] = 0.5f / light->parms.lightRadius[1];
	localProject[2][2] = 0.5f / light->parms.lightRadius[2];
	localProject[0][3] = 0.5f;
	localProject[1][3] = 0.5f;
	localProject[2][3] = 0.5f;
	localProject[3][3] = 1.0f;	// identity perspective

	return 1.0f;
}

/*
=================
R_DeriveLightData

Fills everything in based on light->parms
=================
*/
void R_ResolveLightResources( const renderLight_t & parms, const idMaterial *& lightShader, idImage *& falloffImage ) {
	lightShader = parms.shader;
	if ( lightShader == NULL ) {
		lightShader = parms.pointLight ? tr.defaultPointLight : tr.defaultProjectedLight;
	}

	// Explicitly parse/touch the selected material here so every material query
	// performed after the commit boundary is read-only.
	declManager->Touch( static_cast< const idDecl *>( lightShader ) );
	falloffImage = lightShader->LightFalloffImage();
	if ( falloffImage == NULL ) {
		// Touch and resolve fallback resources while declaration loading is legal.
		const idMaterial * defaultShader = parms.pointLight ? tr.defaultPointLight : tr.defaultProjectedLight;
		declManager->Touch( static_cast< const idDecl *>( defaultShader ) );
		falloffImage = defaultShader->LightFalloffImage();
	}
}

static void R_DeriveLightData( idRenderLightCommitted * light ) {
	// Shader and image resources were resolved by UpdateLightDef. From this
	// point onward derivation is renderer-only and must not load declarations.
	assert( light->lightShader != NULL );

	// ------------------------------------
	// compute the light projection matrix
	// ------------------------------------

	idRenderMatrix localProject;
	float zScale = 1.0f;
	if ( light->parms.parallel ) {
		zScale = R_ComputeParallelLightProjectionMatrix( light, localProject );
	} else if ( light->parms.pointLight ) {
		zScale = R_ComputePointLightProjectionMatrix( light, localProject );
	} else {
		zScale = R_ComputeSpotLightProjectionMatrix( light, localProject );
	}

	// set the old style light projection where Z and W are flipped and
	// for projected lights lightProject[3] is divided by ( zNear + zFar )
	light->lightProject[0][0] = localProject[0][0];
	light->lightProject[0][1] = localProject[0][1];
	light->lightProject[0][2] = localProject[0][2];
	light->lightProject[0][3] = localProject[0][3];

	light->lightProject[1][0] = localProject[1][0];
	light->lightProject[1][1] = localProject[1][1];
	light->lightProject[1][2] = localProject[1][2];
	light->lightProject[1][3] = localProject[1][3];

	light->lightProject[2][0] = localProject[3][0];
	light->lightProject[2][1] = localProject[3][1];
	light->lightProject[2][2] = localProject[3][2];
	light->lightProject[2][3] = localProject[3][3];

	light->lightProject[3][0] = localProject[2][0] * zScale;
	light->lightProject[3][1] = localProject[2][1] * zScale;
	light->lightProject[3][2] = localProject[2][2] * zScale;
	light->lightProject[3][3] = localProject[2][3] * zScale;

	// transform the lightProject
	float lightTransform[16];
	R_AxisToModelMatrix( light->parms.axis, light->parms.origin, lightTransform );
	for ( int i = 0; i < 4; i++ ) {
		idPlane temp = light->lightProject[i];
		R_LocalPlaneToGlobal( lightTransform, temp, light->lightProject[i] );
	}

	// adjust global light origin for off center projections and parallel projections
	// we are just faking parallel by making it a very far off center for now
	if ( light->parms.parallel ) {
		idVec3 dir = light->parms.lightCenter;
		if ( dir.Normalize() == 0.0f ) {
			// make point straight up if not specified
			dir[2] = 1.0f;
		}
		light->globalLightOrigin = light->parms.origin + dir * 100000.0f;
	} else {
		light->globalLightOrigin = light->parms.origin + light->parms.axis * light->parms.lightCenter;
	}

	// Rotate and translate the light projection by the light matrix.
	// 99% of lights remain axis aligned in world space.
	idRenderMatrix lightMatrix;
	idRenderMatrix::CreateFromOriginAxis( light->parms.origin, light->parms.axis, lightMatrix );

	idRenderMatrix inverseLightMatrix;
	if ( !idRenderMatrix::Inverse( lightMatrix, inverseLightMatrix ) ) {
		idLib::Warning( "lightMatrix invert failed" );
	}

	// 'baseLightProject' goes from global space -> light local space -> light projective space
	idRenderMatrix::Multiply( localProject, inverseLightMatrix, light->baseLightProject );

	// Invert the light projection so we can deform zero-to-one cubes into
	// the light model and calculate global bounds.
	if ( !idRenderMatrix::Inverse( light->baseLightProject, light->inverseBaseLightProject ) ) {
		idLib::Warning( "baseLightProject invert failed" );
	}

	// calculate the global light bounds by inverse projecting the zero to one cube with the 'inverseBaseLightProject'
	idRenderMatrix::ProjectedBounds( light->globalLightBounds, light->inverseBaseLightProject, bounds_zeroOneCube, false );
}

/*
====================
R_FreeLightDefDerivedData

Frees all references and lit surfaces from the light
====================
*/
void R_FreeLightDefDerivedData( idRenderLightCommitted *ldef ) {
	// remove any portal fog references
	for ( doublePortal_t *dp = ldef->foggedPortals; dp != NULL; dp = dp->nextFoggedPortal ) {
		dp->fogLight = NULL;
	}

	// free all the references to the light
	areaReference_t * nextRef = NULL;
	for ( areaReference_t * lref = ldef->references; lref != NULL; lref = nextRef ) {
		nextRef = lref->ownerNext;

		// unlink from the area
		lref->areaNext->areaPrev = lref->areaPrev;
		lref->areaPrev->areaNext = lref->areaNext;

		// put it back on the free list for reuse
		ldef->world->areaReferenceAllocator.Free( lref );
	}
	ldef->references = NULL;

	// free all the approximate lighting references to the light
	for ( areaReference_t * lref = ldef->approxReferences; lref != NULL; lref = nextRef ) {
		nextRef = lref->ownerNext;

		// unlink from the area
		lref->areaNext->areaPrev = lref->areaPrev;
		lref->areaPrev->areaNext = lref->areaNext;

		// put it back on the free list for reuse
		ldef->world->areaReferenceAllocator.Free( lref );
	}
	ldef->approxReferences = NULL;
}

/*
===============
WindingCompletelyInsideLight
===============
*/
static bool WindingCompletelyInsideLight( const idWinding *w, const idRenderLightCommitted *ldef ) {
	for ( int i = 0; i < w->GetNumPoints(); i++ ) {
		if ( idRenderMatrix::CullPointToMVP( ldef->baseLightProject, (*w)[i].ToVec3(), true ) ) {
			return false;
		}
	}
	return true;
}

/*
======================
R_CreateLightDefFogPortals

When a fog light is created or moved, see if it completely
encloses any portals, which may allow them to be fogged closed.
======================
*/
static void R_CreateLightDefFogPortals( idRenderLightCommitted *ldef ) {
	ldef->foggedPortals = NULL;

	if ( !ldef->lightShader->IsFogLight() ) {
		return;
	}

	// some fog lights will explicitly disallow portal fogging
	if ( ldef->lightShader->TestMaterialFlag( MF_NOPORTALFOG ) ) {
		return;
	}

	for ( areaReference_t * lref = ldef->references; lref != NULL; lref = lref->ownerNext ) {
		// check all the models in this area
		portalArea_t * area = lref->area;

		for ( portal_t * prt = area->portals; prt != NULL; prt = prt->next ) {
			doublePortal_t * dp = prt->doublePortal;

			// we only handle a single fog volume covering a portal
			// this will never cause incorrect drawing, but it may
			// fail to cull a portal 
			if ( dp->fogLight ) {
				continue;
			}

			if ( WindingCompletelyInsideLight( prt->w, ldef ) ) {
				dp->fogLight = ldef;
				dp->nextFoggedPortal = ldef->foggedPortals;
				ldef->foggedPortals = dp;
			}
		}
	}
}

/*
=================
R_CreateLightRefs
=================
*/
void R_CreateLightRefs( idRenderLightCommitted * light ) {
	// derive light data
	R_DeriveLightData( light );

	// determine the areaNum for the light origin, which may let us
	// cull the light if it is behind a closed door
	// it is debatable if we want to use the entity origin or the center offset origin,
	// but we definitely don't want to use a parallel offset origin
	light->areaNum = light->world->PointInArea( light->globalLightOrigin );
	if ( light->areaNum == -1 ) {
		light->areaNum = light->world->PointInArea( light->parms.origin );
	}

	// bump the view count so we can tell if an
	// area already has a reference
	tr.viewCount++;

	// Push the complete light frustum into the BSP. Shadow maps need references
	// for every possible caster, including geometry outside portal visibility.
	light->world->PushFrustumIntoTree( NULL, light, light->inverseBaseLightProject, bounds_zeroOneCube );

	R_CreateLightDefFogPortals( light );
}

/*
=================================================================================

WORLD MODEL & LIGHT DEFS

=================================================================================
*/

/*
===================
R_FreeDerivedData

ReloadModels and RegenerateWorld call this
===================
*/
void R_FreeDerivedData() {
	for ( int j = 0; j < tr.worlds.Num(); j++ ) {
		idRenderWorldLocal * rw = tr.worlds[j];

		for ( int i = 0; i < rw->entityDefs.Num(); i++ ) {
			idRenderEntityCommitted * def = rw->entityDefs[i];
			if ( def == NULL ) {
				continue;
			}
			R_FreeEntityDefDerivedData( def, false, false );
		}

		for ( int i = 0; i < rw->lightDefs.Num(); i++ ) {
			idRenderLightCommitted * light = rw->lightDefs[i];
			if ( light == NULL ) {
				continue;
			}
			R_FreeLightDefDerivedData( light );
		}
	}
}

/*
===================
R_CheckForEntityDefsUsingModel
===================
*/
void R_CheckForEntityDefsUsingModel( idRenderModel *model ) {
	for ( int j = 0; j < tr.worlds.Num(); j++ ) {
		idRenderWorldLocal * rw = tr.worlds[j];

		for ( int i = 0; i < rw->entityDefs.Num(); i++ ) {
			idRenderEntityCommitted	* def = rw->entityDefs[i];
			if ( !def ) {
				continue;
			}
			if ( def->parms.hModel == model ) {
				//assert( 0 );
				// this should never happen but Radiant messes it up all the time so just free the derived data
				R_FreeEntityDefDerivedData( def, false, false );
			}
		}
	}
}

/*
===================
R_ReCreateWorldReferences

ReloadModels and RegenerateWorld call this
===================
*/
void R_ReCreateWorldReferences() {
	// let the interaction generation code know this
	// shouldn't be optimized for a particular view
	tr.viewDef = NULL;

	for ( int j = 0; j < tr.worlds.Num(); j++ ) {
		idRenderWorldLocal * rw = tr.worlds[j];

		for ( int i = 0; i < rw->entityDefs.Num(); i++ ) {
			idRenderEntityCommitted * def = rw->entityDefs[i];
			if ( def == NULL ) {
				continue;
			}
			// the world model entities are put specifically in a single
			// area, instead of just pushing their bounds into the tree
			if ( i < rw->numPortalAreas ) {
				rw->AddEntityRefToArea( def, &rw->portalAreas[i] );
			} else {
				R_CreateEntityRefs( def );
			}
		}

		for ( int i = 0; i < rw->lightDefs.Num(); i++ ) {
			idRenderLightCommitted * light = rw->lightDefs[i];
			if ( light == NULL ) {
				continue;
			}

			// R_FreeDerivedData() already removed the old references. Rebuild the
			// derived committed state in place so an object-backed light keeps its
			// producer/committed identity across reloadModels and RegenerateWorld.
			R_CreateLightRefs( light );
			light->needsReferences = false;
			tr.pc.c_lightReferenceCommits++;
		}
	}
}

/*
====================
R_ModulateLights_f

Modifies the shaderParms on all the lights so the level
designers can easily test different color schemes
====================
*/
void R_ModulateLights_f( const idCmdArgs &args ) {
	if ( !tr.primaryWorld ) {
		return;
	}
	if ( args.Argc() != 4 ) {
		common->Printf( "usage: modulateLights <redFloat> <greenFloat> <blueFloat>\n" );
		return;
	}

	float modulate[3];
	for ( int i = 0; i < 3; i++ ) {
		modulate[i] = atof( args.Argv( i+1 ) );
	}

	int count = 0;
	for ( int i = 0; i < tr.primaryWorld->lightDefs.Num(); i++ ) {
		idRenderLightCommitted * light = tr.primaryWorld->lightDefs[i];
		if ( light != NULL ) {
			count++;
			idRenderLight * renderLight = i < tr.primaryWorld->lightProducers.Num()
				? tr.primaryWorld->lightProducers[i]
				: NULL;
			renderLight_t * producer = renderLight != NULL
				? static_cast< renderLight_t * >( renderLight )
				: &light->parms;
			for ( int j = 0; j < 3; j++ ) {
				producer->shaderParms[j] *= modulate[j];
			}
			if ( renderLight != NULL ) {
				renderLight->CommitThisFrame();
			} else {
				tr.primaryWorld->UpdateLightDef( i, producer );
			}
		}
	}
	common->Printf( "modulated %i lights\n", count );
}
