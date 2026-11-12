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
#include "../../../idlib/precompiled.h"

#include "../../tr_local.h"
#include "renderlights.h"

extern idCVar r_useAreasConnectedForShadowCulling;

idCVar r_useAreasConnectedForShadowCulling( "r_useAreasConnectedForShadowCulling", "2", CVAR_RENDERER | CVAR_INTEGER, "cull entities cut off by doors" );
idCVar r_useParallelAddLights( "r_useParallelAddLights", "1", CVAR_RENDERER | CVAR_BOOL, "aadd all lights in parallel with jobs" );

/*
============================
R_ShadowBounds

Even though the extruded shadows are drawn projected to infinity, their effects are limited
to a fraction of the light's volume.  An extruded box would require 12 faces to specify and
be a lot of trouble, but an axial bounding box is quick and easy to determine.

If the light is completely contained in the view, there is no value in trying to cull the
shadows, as they will all pass.

Pure function.
============================
*/
void R_ShadowBounds( const idBounds & modelBounds, const idBounds & lightBounds, const idVec3 & lightOrigin, idBounds & shadowBounds ) {
	for ( int i = 0; i < 3; i++ ) {
		shadowBounds[0][i] = __fsels( modelBounds[0][i] - lightOrigin[i], modelBounds[0][i], lightBounds[0][i] );
		shadowBounds[1][i] = __fsels( lightOrigin[i] - modelBounds[1][i], modelBounds[1][i], lightBounds[1][i] );
	}
}

/*
============================
idRenderEntityCommitted::IsDirectlyVisible()
============================
*/
bool idRenderEntityCommitted::IsDirectlyVisible() const {
	if ( viewCount != tr.viewCount ) {
		return false;
	}
	if ( viewEntity->scissorRect.IsEmpty() ) {
		// a viewEntity was created for shadow generation, but the
		// model global reference bounds isn't directly visible
		return false;
	}
	return true;
}

/*
===================
R_AddSingleLight

May be run in parallel.

Sets vLight->removeFromList to true if the light should be removed from the list.
Builds a chain of entities that need to be added for shadows only off vLight->shadowOnlyViewEntities.
Allocates and fills in vLight->entityInteractionState.

Calc the light shader values, removing any light from the viewLight list
if it is determined to not have any visible effect due to being flashed off or turned off.

Prepare the light and its shadow-map state.
===================
*/
static void R_AddSingleLight( viewLight_t * vLight ) {
	// until proven otherwise
	vLight->removeFromList = true;
	vLight->shadowOnlyViewEntities = NULL;

	// globals we really should pass in...
	const viewDef_t * viewDef = tr.viewDef;

	const idRenderLightCommitted *light = vLight->lightDef;
	const idMaterial * lightShader = light->lightShader;
	if ( lightShader == NULL ) {
		common->Error( "R_AddSingleLight: NULL lightShader" );
		return;
	}

	SCOPED_PROFILE_EVENT( lightShader->GetName() );

	// see if we are suppressing the light in this view
	if ( !r_skipSuppress.GetBool() ) {
		if ( light->parms.suppressLightInViewID && light->parms.suppressLightInViewID == viewDef->renderView.viewID ) {
			return;
		}
		if ( light->parms.allowLightInViewID && light->parms.allowLightInViewID != viewDef->renderView.viewID ) {
			return;
		}
	}

	// TNO prelight only lights never draw as view lights, they only feed
	// the approximate lighting through the area references
	if ( light->parms.lightClass == LIGHT_PRELIGHT_ONLY ) {
		return;
	}

	// evaluate the light shader registers
	float * lightRegs = (float *)R_FrameAlloc( lightShader->GetNumRegisters() * sizeof( float ), FRAME_ALLOC_SHADER_REGISTER );
	lightShader->EvaluateRegisters( lightRegs, light->parms.shaderParms, viewDef->renderView.shaderParms, 
		tr.viewDef->renderView.time[0] * 0.001f, light->parms.referenceSound );
		
	// if this is a purely additive light and no stage in the light shader evaluates
	// to a positive light value, we can completely skip the light
	if ( !lightShader->IsFogLight() && !lightShader->IsBlendLight() ) {
		int lightStageNum;
		for ( lightStageNum = 0; lightStageNum < lightShader->GetNumStages(); lightStageNum++ ) {
			const shaderStage_t	*lightStage = lightShader->GetStage( lightStageNum );

			// ignore stages that fail the condition
			if ( !lightRegs[ lightStage->conditionRegister ] ) {
				continue;
			}

			const int * registers = lightStage->color.registers;

			// snap tiny values to zero
			if ( lightRegs[ registers[0] ] < 0.001f ) {
				lightRegs[ registers[0] ] = 0.0f;
			}
			if ( lightRegs[ registers[1] ] < 0.001f ) {
				lightRegs[ registers[1] ] = 0.0f;
			}
			if ( lightRegs[ registers[2] ] < 0.001f ) {
				lightRegs[ registers[2] ] = 0.0f;
			}

			if ( lightRegs[ registers[0] ] > 0.0f ||
					lightRegs[ registers[1] ] > 0.0f ||
						lightRegs[ registers[2] ] > 0.0f ) {
				break;
			}
		}
		if ( lightStageNum == lightShader->GetNumStages() ) {
			// we went through all the stages and didn't find one that adds anything
			// remove the light from the viewLights list, and change its frame marker
			// so interaction generation doesn't think the light is visible and
			// create a shadow for it
			return;
		}
	}

	//--------------------------------------------
	// copy data used by backend
	//--------------------------------------------
	vLight->globalLightOrigin = light->globalLightOrigin;
	vLight->shadowLightAxis = light->parms.axis;
	vLight->shadowLightOrigin = light->parms.origin;
	vLight->shadowLightCenter = light->parms.lightCenter;
	vLight->shadowLightTarget = light->parms.target;
	vLight->shadowLightRight = light->parms.right;
	vLight->shadowLightUp = light->parms.up;
	vLight->shadowLightStart = light->parms.start;
	vLight->shadowLightBounds = light->globalLightBounds;
	vLight->shadowViewOrigin = viewDef->renderView.vieworg;
	vLight->shadowPointLight = light->parms.pointLight;
	vLight->shadowParallel = light->parms.parallel;
	vLight->lightProject[0] = light->lightProject[0];
	vLight->lightProject[1] = light->lightProject[1];
	vLight->lightProject[2] = light->lightProject[2];
	vLight->lightProject[3] = light->lightProject[3];

	// the fog plane is the light far clip plane
	idPlane fogPlane(	light->baseLightProject[2][0] - light->baseLightProject[3][0],
						light->baseLightProject[2][1] - light->baseLightProject[3][1],
						light->baseLightProject[2][2] - light->baseLightProject[3][2],
						light->baseLightProject[2][3] - light->baseLightProject[3][3] );
	const float planeScale = idMath::InvSqrt( fogPlane.Normal().LengthSqr() );
	vLight->fogPlane[0] = fogPlane[0] * planeScale;
	vLight->fogPlane[1] = fogPlane[1] * planeScale;
	vLight->fogPlane[2] = fogPlane[2] * planeScale;
	vLight->fogPlane[3] = fogPlane[3] * planeScale;

	// copy the matrix for deforming the 'zeroOneCubeModel' to exactly cover the light volume in world space
	vLight->inverseBaseLightProject = light->inverseBaseLightProject;

	vLight->falloffImage = light->falloffImage;
	vLight->lightShader = light->lightShader;
	vLight->shaderRegisters = lightRegs;

	if ( r_useLightScissors.GetInteger() != 0 ) {
		// Calculate the matrix that projects the zero-to-one cube to exactly cover the
		// light frustum in clip space.
		idRenderMatrix invProjectMVPMatrix;
		idRenderMatrix::Multiply( viewDef->worldSpace.mvp, light->inverseBaseLightProject, invProjectMVPMatrix );

		// Calculate the projected bounds, either not clipped at all, near clipped, or fully clipped.
		idBounds projected;
		if ( r_useLightScissors.GetInteger() == 1 ) {
			idRenderMatrix::ProjectedBounds( projected, invProjectMVPMatrix, bounds_zeroOneCube );
		} else if ( r_useLightScissors.GetInteger() == 2 ) {
			idRenderMatrix::ProjectedNearClippedBounds( projected, invProjectMVPMatrix, bounds_zeroOneCube );
		} else {
			idRenderMatrix::ProjectedFullyClippedBounds( projected, invProjectMVPMatrix, bounds_zeroOneCube );
		}

		if ( projected[0][2] >= projected[1][2] ) {
			// the light was culled to the view frustum
			return;
		}

		float screenWidth = (float)viewDef->viewport.x2 - (float)viewDef->viewport.x1;
		float screenHeight = (float)viewDef->viewport.y2 - (float)viewDef->viewport.y1;

		idScreenRect lightScissorRect;
		lightScissorRect.x1 = idMath::Ftoi( projected[0][0] * screenWidth );
		lightScissorRect.x2 = idMath::Ftoi( projected[1][0] * screenWidth );
		lightScissorRect.y1 = idMath::Ftoi( projected[0][1] * screenHeight );
		lightScissorRect.y2 = idMath::Ftoi( projected[1][1] * screenHeight );
		lightScissorRect.Expand();

		vLight->scissorRect.Intersect( lightScissorRect );
		vLight->scissorRect.zmin = projected[0][2];
		vLight->scissorRect.zmax = projected[1][2];
	}

	// this one stays on the list
	vLight->removeFromList = false;

	//--------------------------------------------
	// create interactions with all entities the light may touch, and add viewEntities
	// that may cast shadows, even if they aren't directly visible.  Any real work
	// will be deferred until we walk through the viewEntities
	//--------------------------------------------
	const int renderViewID = viewDef->renderView.viewID;

	// this bool array will be set true whenever the entity will visibly interact with the light
	vLight->entityInteractionState = (byte *)R_ClearedFrameAlloc( light->world->entityDefs.Num() * sizeof( vLight->entityInteractionState[0] ), FRAME_ALLOC_INTERACTION_STATE );

	const bool lightCastsShadows = light->LightCastsShadows();

	for ( areaReference_t * lref = light->references; lref != NULL; lref = lref->ownerNext ) {
		portalArea_t *area = lref->area;

		// some lights have their center of projection outside the world, but otherwise
		// we want to ignore areas that are not connected to the light center due to a closed door
		if ( light->areaNum != -1 && r_useAreasConnectedForShadowCulling.GetInteger() == 2 ) {
			if ( !light->world->AreasAreConnected( light->areaNum, area->areaNum, PS_BLOCK_VIEW ) ) {
				// can't possibly be seen or shadowed
				continue;
			}
		}

		// check all the models in this area
		for ( areaReference_t * eref = area->entityRefs.areaNext; eref != &area->entityRefs; eref = eref->areaNext ) {
			idRenderEntityCommitted * edef = eref->entity;

			if ( vLight->entityInteractionState[ edef->index ] != viewLight_t::INTERACTION_UNCHECKED ) {
				continue;
			}
			// until proven otherwise
			vLight->entityInteractionState[ edef->index ] = viewLight_t::INTERACTION_NO;

			const renderEntity_t & eParms = edef->parms;
			const idRenderModel * eModel = eParms.hModel;

			// We don't want the lights on weapons to illuminate anything else.
			// There are two assumptions here -- that allowLightInViewID is only
			// used for weapon lights, and that all weapons will have weaponDepthHack.
			// A more general solution would be to have an allowLightOnEntityID field.
			// HACK: the armor-mounted flashlight is a private spot light, which is probably
			// wrong -- you would expect to see them in multiplayer.
			if ( light->parms.allowLightInViewID && light->parms.pointLight && !eParms.weaponDepthHack ) {
				continue;
			}

			// non-shadow casting entities don't need to be added if they aren't
			// directly visible
			if ( ( eParms.noShadow || ( eModel && !eModel->ModelHasShadowCastingSurfaces() ) ) && !edef->IsDirectlyVisible() ) {
				continue;
			}

			// if the model doesn't accept lighting or cast shadows, it doesn't need to be added
			if ( eModel && !eModel->ModelHasInteractingSurfaces() && !eModel->ModelHasShadowCastingSurfaces() ) {
				continue;
			}

			// Evaluate overlap from committed spatial state for this view. The old
			// noDynamicInteractions depended on the removed prebuilt pair cache, so
			// it cannot suppress this path without dropping static-world lighting.
			if ( R_CullModelBoundsToLight( light, edef->localReferenceBounds, edef->modelRenderMatrix ) ) {
				continue;
			}

			// we now know that the entity and light do overlap

			if ( edef->IsDirectlyVisible() ) {
				// entity is directly visible, so the interaction is definitely needed
				vLight->entityInteractionState[ edef->index ] = viewLight_t::INTERACTION_YES;
				continue;
			}

			// the entity is not directly visible, but if we can tell that it may cast
			// shadows onto visible surfaces, we must make a viewEntity for it
			if ( !lightCastsShadows ) {
				// surfaces are never shadowed in this light
				continue;
			}
			// if we are suppressing its shadow in this view (player shadows, etc), skip
			if ( !r_skipSuppress.GetBool() ) {
				if ( eParms.suppressShadowInViewID && eParms.suppressShadowInViewID == renderViewID ) {
					continue;
				}
				if ( eParms.suppressShadowInLightID && eParms.suppressShadowInLightID == light->parms.lightId ) {
					continue;
				}
			}

			// should we use the shadow bounds from pre-calculated interactions?
			idBounds shadowBounds;
			R_ShadowBounds( edef->globalReferenceBounds, light->globalLightBounds, light->globalLightOrigin, shadowBounds );

			// this test is pointless if we knew the light was completely contained
			// in the view frustum, but the entity would also be directly visible in most
			// of those cases.

			// this doesn't say that the shadow can't effect anything, only that it can't
			// effect anything in the view, so we shouldn't set up a view entity
			if ( idRenderMatrix::CullBoundsToMVP( viewDef->worldSpace.mvp, shadowBounds ) ) {
				continue;
			}

			// debug tool to allow viewing of only one entity at a time
			if ( r_singleEntity.GetInteger() >= 0 && r_singleEntity.GetInteger() != edef->index ) {
				continue;
			}

			// we do need it for shadows
			vLight->entityInteractionState[ edef->index ] = viewLight_t::INTERACTION_YES;

			// we will need to create a viewEntity_t for it in the serial code section
			shadowOnlyEntity_t * shadEnt = (shadowOnlyEntity_t *)R_FrameAlloc( sizeof( shadowOnlyEntity_t ), FRAME_ALLOC_SHADOW_ONLY_ENTITY );
			shadEnt->next = vLight->shadowOnlyViewEntities;
			shadEnt->edef = edef;
			vLight->shadowOnlyViewEntities = shadEnt;
		}
	}

	// Static-world casters arrive through the ordinary model-surface path.
}

REGISTER_PARALLEL_JOB( R_AddSingleLight, "R_AddSingleLight" );

/*
=================
R_AddLights
=================
*/
void R_AddLights() {
	SCOPED_PROFILE_EVENT( "R_AddLights" );

	//-------------------------------------------------
	// check each light individually, possibly in parallel
	//-------------------------------------------------

	if ( r_useParallelAddLights.GetBool() ) {
		for ( viewLight_t * vLight = tr.viewDef->viewLights; vLight != NULL; vLight = vLight->next ) {
			tr.frontEndJobList->AddJob( (jobRun_t)R_AddSingleLight, vLight );
		}
		tr.frontEndJobList->Submit();
		tr.frontEndJobList->Wait();
	} else {
		for ( viewLight_t * vLight = tr.viewDef->viewLights; vLight != NULL; vLight = vLight->next ) {
			R_AddSingleLight( vLight );
		}
	}

	//-------------------------------------------------
	// cull lights from the list if they turned out to not be needed
	//-------------------------------------------------

	tr.pc.c_viewLights = 0;
	viewLight_t **ptr = &tr.viewDef->viewLights;
	while ( *ptr != NULL ) {
		viewLight_t *vLight = *ptr;

		if ( vLight->removeFromList ) {
			vLight->lightDef->viewCount = -1;	// this probably doesn't matter with current code
			*ptr = vLight->next;
			continue;
		}

		ptr = &vLight->next;

		// serial work
		tr.pc.c_viewLights++;

		for ( shadowOnlyEntity_t * shadEnt = vLight->shadowOnlyViewEntities; shadEnt != NULL; shadEnt = shadEnt->next ) {
			// this will add it to the viewEntities list, but with an empty scissor rect
			R_SetEntityDefViewEntity( shadEnt->edef );
		}

		if ( r_showLightScissors.GetBool() ) {
			R_ShowColoredScreenRect( vLight->scissorRect, vLight->lightDef->index );
		}
	}

}

/*
=====================
R_OptimizeViewLightsList
=====================
*/
void R_OptimizeViewLightsList() {
	// go through each visible light
	int numViewLights = 0;
	for ( viewLight_t * vLight = tr.viewDef->viewLights; vLight != NULL; vLight = vLight->next ) {
		numViewLights++;
		// If the light didn't have any lit surfaces visible, there is no need to
		// draw any of the shadows.  We still keep the vLight for debugging draws.
		if ( !vLight->localInteractions && !vLight->globalInteractions && !vLight->translucentInteractions ) {
			vLight->localShadows = NULL;
			vLight->globalShadows = NULL;
		}
	}

	if ( r_useShadowSurfaceScissor.GetBool() ) {
		// shrink the light scissor rect to only intersect the surfaces that will actually be drawn.
		// This doesn't seem to actually help, perhaps because the surface scissor
		// rects aren't actually the surface, but only the portal clippings.
		for ( viewLight_t * vLight = tr.viewDef->viewLights; vLight; vLight = vLight->next ) {
			drawSurf_t * surf;
			idScreenRect surfRect;

			if ( !vLight->lightShader->LightCastsShadows() ) {
				continue;
			}

			surfRect.Clear();

			for ( surf = vLight->globalInteractions; surf != NULL; surf = surf->nextOnLight ) {
				surfRect.Union( surf->scissorRect );
			}
			for ( surf = vLight->localShadows; surf != NULL; surf = surf->nextOnLight ) {
				surf->scissorRect.Intersect( surfRect );
			}

			for ( surf = vLight->localInteractions; surf != NULL; surf = surf->nextOnLight ) {
				surfRect.Union( surf->scissorRect );
			}
			for ( surf = vLight->globalShadows; surf != NULL; surf = surf->nextOnLight ) {
				surf->scissorRect.Intersect( surfRect );
			}

			for ( surf = vLight->translucentInteractions; surf != NULL; surf = surf->nextOnLight ) {
				surfRect.Union( surf->scissorRect );
			}

			vLight->scissorRect.Intersect( surfRect );
		}
	}

	// sort the viewLights list so the largest lights come first, which will reduce
	// the chance of GPU pipeline bubbles
	struct sortLight_t {
		viewLight_t *	vLight;
		int				screenArea;
		static int sort( const void * a, const void * b ) {
			return ((sortLight_t *)a)->screenArea - ((sortLight_t *)b)->screenArea;
		}
	};
	sortLight_t * sortLights = (sortLight_t *)_alloca( sizeof( sortLight_t ) * numViewLights );
	int	numSortLightsFilled = 0;
	for ( viewLight_t * vLight = tr.viewDef->viewLights; vLight != NULL; vLight = vLight->next ) {
		sortLights[ numSortLightsFilled ].vLight = vLight;
		sortLights[ numSortLightsFilled ].screenArea = vLight->scissorRect.GetArea();
		numSortLightsFilled++;
	}

	qsort( sortLights, numSortLightsFilled, sizeof( sortLights[0] ), sortLight_t::sort );

	// rebuild the linked list in order
	tr.viewDef->viewLights = NULL;
	for ( int i = 0; i < numSortLightsFilled; i++ ) {
		sortLights[i].vLight->next = tr.viewDef->viewLights;
		tr.viewDef->viewLights = sortLights[i].vLight;
	}
}
