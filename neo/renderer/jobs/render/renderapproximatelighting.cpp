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
#include "../../globalshadows.h"
#include "../approximatelighting/approximatelighting.h"

/*
================================================================================================

TNO style approximate lighting.

Re-implements the semantics of the Wolfenstein: The New Order approximate
lighting pass on this engine idlib. The original MachineGames code is
decompiled proprietary material and not GPL compatible, nothing was copied
from it.

A minimal subset is enough for the dim shadows: every model in the view gets
a prime light direction accumulated from the lights that illuminate it
without casting shadow maps, weighted by light color and distance falloff.
The dim shadow pass uses the direction to build the per model shadow
projection. The full job also blends the offline ambient map into six
channel colors and gathers the strongest radial lights, which the self
shadow pass consumes.

================================================================================================
*/

// prime directions weaker than this are treated as no prime light at all
static const float DIM_SHADOW_MIN_PRIME_WEIGHT = 0.01f;

/*
==================
R_ApproximateLighting

Runs in the front end after the view lights are finalized, so the shadow
chains tell which lights already cast real shadow maps.
==================
*/
void R_ApproximateLighting() {
	if ( tr.viewDef == NULL || tr.viewDef->isSubview || tr.viewDef->is2Dgui ) {
		return;
	}
	SCOPED_PROFILE_EVENT( "R_ApproximateLighting" );

	const shadowMapCPU_t * globalShadowMap = R_GetGlobalShadows().GetShadowMap();

	for ( viewEntity_t * vEnt = tr.viewDef->viewEntitys; vEnt != NULL; vEnt = vEnt->next ) {
		vEnt->primeLightDir.Zero();
		vEnt->primeLightWeight = 0.0f;
		if ( vEnt->isGuiSurface || vEnt->weaponDepthHack || vEnt->entityDef == NULL ) {
			continue;
		}
		const idBounds & globalBounds = vEnt->entityDef->globalReferenceBounds;
		if ( globalBounds.IsCleared() ) {
			continue;
		}
		const idVec3 center = ( globalBounds[0] + globalBounds[1] ) * 0.5f;

		idVec3 accum( 0.0f, 0.0f, 0.0f );
		for ( const viewLight_t * vLight = tr.viewDef->viewLights; vLight != NULL; vLight = vLight->next ) {
			// lights with real shadow maps already shadow themselves, dim
			// shadows are for the lights that do not
			if ( vLight->globalShadows != NULL || vLight->localShadows != NULL ) {
				continue;
			}
			const idMaterial * lightShader = vLight->lightShader;
			if ( lightShader == NULL || lightShader->IsFogLight() || lightShader->IsBlendLight() ) {
				continue;
			}
			// color of the first active light shader stage
			idVec3 color( 0.0f, 0.0f, 0.0f );
			if ( vLight->shaderRegisters != NULL ) {
				for ( int s = 0; s < lightShader->GetNumStages(); s++ ) {
					const shaderStage_t * stage = lightShader->GetStage( s );
					if ( vLight->shaderRegisters[stage->conditionRegister] == 0.0f ) {
						continue;
					}
					color.x = vLight->shaderRegisters[stage->color.registers[0]];
					color.y = vLight->shaderRegisters[stage->color.registers[1]];
					color.z = vLight->shaderRegisters[stage->color.registers[2]];
					break;
				}
			}
			float weight = color.Length();
			if ( weight <= 0.0f ) {
				continue;
			}

			idVec3 dir;
			if ( vLight->shadowParallel ) {
				// parallel lights carry their travel direction in lightCenter
				dir = -vLight->shadowLightCenter;
				if ( dir.Normalize() == 0.0f ) {
					continue;
				}
			} else {
				dir = center - vLight->globalLightOrigin;
				const float dist = dir.Normalize();
				if ( dist <= 1.0f ) {
					continue;
				}
				// smooth falloff over the light radius
				const float radius = Max( ( vLight->shadowLightBounds[1] - vLight->shadowLightBounds[0] ).Length() * 0.5f, 1.0f );
				weight *= radius * radius / ( radius * radius + dist * dist );
			}
			// Tech5 attenuates parallel light contributions by the offline
			// global shadow map sampled at the model center
			if ( vLight->shadowParallel && globalShadowMap != NULL ) {
				weight *= globalShadowMap->ShadowSample( center );
				if ( weight <= 0.0f ) {
					continue;
				}
			}
			accum += dir * weight;
		}

		const float total = accum.Normalize();
		if ( total > DIM_SHADOW_MIN_PRIME_WEIGHT ) {
			vEnt->primeLightDir = accum;
			vEnt->primeLightWeight = total;
		}

		// full Tech5 approximate lighting: blend the offline ambient map
		// and gather the strongest radial lights for the self shadow pass
		approximateLightParms_t parms;
		memset( &parms, 0, sizeof( parms ) );
		parms.modelOrigin = center;
		parms.viewOrigin = tr.viewDef->renderView.vieworg;
		parms.ambientMap = R_GetAmbientLighting().ambientMap;
		for ( const viewLight_t * vLight = tr.viewDef->viewLights; vLight != NULL && parms.numContributingLights < MAX_CONTRIBUTING_LIGHTS; vLight = vLight->next ) {
			const idMaterial * lightShader = vLight->lightShader;
			if ( lightShader == NULL || lightShader->IsFogLight() || lightShader->IsBlendLight() ) {
				continue;
			}
			// parallel lights have no radial origin for the self shadow slots
			if ( vLight->shadowParallel ) {
				continue;
			}
			idVec3 color( 0.0f, 0.0f, 0.0f );
			if ( vLight->shaderRegisters != NULL ) {
				for ( int s = 0; s < lightShader->GetNumStages(); s++ ) {
					const shaderStage_t * stage = lightShader->GetStage( s );
					if ( vLight->shaderRegisters[stage->conditionRegister] == 0.0f ) {
						continue;
					}
					color.x = vLight->shaderRegisters[stage->color.registers[0]];
					color.y = vLight->shaderRegisters[stage->color.registers[1]];
					color.z = vLight->shaderRegisters[stage->color.registers[2]];
					break;
				}
			}
			if ( color.LengthSqr() <= 0.0f ) {
				continue;
			}
			contributingLight_t & cl = parms.contributingLights[parms.numContributingLights++];
			// w == 1.0f marks a light that never casts shadows
			cl.lightOrigin = idVec4( vLight->globalLightOrigin, ( vLight->lightDef != NULL && vLight->lightDef->parms.noShadows ) ? 1.0f : 0.0f );
			cl.lightColor = color;
		}
		LocalLighting( parms, vEnt->approximateLighting );
	}
}

