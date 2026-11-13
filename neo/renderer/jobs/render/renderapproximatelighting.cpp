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
#include "../../ambientlighting.h"
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

// Tech5 approximate lighting debug visualization
idCVar r_showDynamicLighting( "r_showDynamicLighting", "0", CVAR_RENDERER | CVAR_INTEGER, "draw approximate lighting debug for each model, 2 = also draw non contributing lights" );
idCVar r_showDynamicLightingSingleModel( "r_showDynamicLightingSingleModel", "-1", CVAR_RENDERER | CVAR_INTEGER, "restrict the approximate lighting debug to one entity handle" );
idCVar r_showAmbientLighting( "r_showAmbientLighting", "0", CVAR_RENDERER | CVAR_BOOL, "draw the ambient map samples around each model lighting point" );


/*
==================
R_ShowApproximateLighting

Tech5 style debug visualization of the approximate lighting data:
model bounds, lighting point, contributing light lines and the ambient
map samples. Driven by r_showDynamicLighting and r_showAmbientLighting.
==================
*/
static void R_ShowApproximateLighting( const viewEntity_t * vEnt, const approximateLightParms_t & parms ) {
	idRenderWorldLocal * world = tr.primaryWorld;
	if ( world == NULL || vEnt->entityDef == NULL ) {
		return;
	}
	const int selectedModel = r_showDynamicLightingSingleModel.GetInteger();
	if ( selectedModel >= 0 && selectedModel != vEnt->entityDef->index ) {
		return;
	}
	if ( r_showAmbientLighting.GetBool() ) {
		R_GetAmbientLighting().DebugDrawAmbientLightingSamples( world, parms.modelOrigin );
	}
	const int mode = r_showDynamicLighting.GetInteger();
	if ( mode <= 0 ) {
		return;
	}

	// oriented bounds of the model and the lighting point
	const idBox box( vEnt->entityDef->localReferenceBounds, vEnt->entityDef->parms.origin, vEnt->entityDef->parms.axis );
	world->DebugBox( colorWhite, box, 1 );
	idBounds sampleBounds( parms.modelOrigin );
	sampleBounds.ExpandSelf( 2.0f );
	world->DebugBounds( colorWhite, sampleBounds, vec3_origin, 1 );

	// one line per contributing light, colored like the light
	for ( int i = 0; i < parms.numContributingLights; i++ ) {
		const contributingLight_t & cl = parms.contributingLights[i];
		const idVec4 lineColor( cl.lightColor.x, cl.lightColor.y, cl.lightColor.z, 1.0f );
		world->DebugLine( lineColor, cl.lightOrigin.ToVec3(), parms.modelOrigin, 3 );
	}

	// mode 2 also shows the view lights that did not contribute
	if ( mode >= 2 ) {
		const idVec4 unusedColor( 0.4f, 0.4f, 0.4f, 1.0f );
		for ( const viewLight_t * vLight = tr.viewDef->viewLights; vLight != NULL; vLight = vLight->next ) {
			const idMaterial * lightShader = vLight->lightShader;
			if ( lightShader == NULL || lightShader->IsFogLight() || lightShader->IsBlendLight() || vLight->shadowParallel ) {
				continue;
			}
			bool contributed = false;
			for ( int i = 0; i < parms.numContributingLights; i++ ) {
				if ( parms.contributingLights[i].lightOrigin.ToVec3() == vLight->globalLightOrigin ) {
					contributed = true;
					break;
				}
			}
			if ( contributed ) {
				continue;
			}
			world->DebugLine( unusedColor, vLight->globalLightOrigin, parms.modelOrigin, 3 );
		}
	}

	// labels above the lighting point
	char label[256];
	idStr::snPrintf( label, sizeof( label ), "%i: %s", vEnt->entityDef->index, vEnt->entityDef->parms.hModel != NULL ? vEnt->entityDef->parms.hModel->Name() : "<none>" );
	idVec3 labelPoint = parms.modelOrigin;
	labelPoint.z += 8.0f;
	world->DrawText( label, labelPoint, 0.2f, colorWhite, mat3_identity, 1, 3 );
	idStr::snPrintf( label, sizeof( label ), "%i light slots used", parms.numContributingLights );
	world->DrawText( label, parms.modelOrigin, 0.2f, colorWhite, mat3_identity, 1, 3 );
}
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
		const idVec3 center = vEnt->entityDef->GetLightingPoint();

		idVec3 accum( 0.0f, 0.0f, 0.0f );
		for ( const viewLight_t * vLight = tr.viewDef->viewLights; vLight != NULL; vLight = vLight->next ) {
			// lights with real shadow maps already shadow themselves, dim
			// shadows are for the lights that do not
			if ( vLight->globalShadows != NULL || vLight->localShadows != NULL ) {
				continue;
			}
			// TNO blended only lights never feed the prime light
			if ( vLight->lightDef != NULL && vLight->lightDef->parms.lightClass == LIGHT_BLENDED_ONLY ) {
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

		// TNO private prime lights only feed the prime light accumulation,
		// they are not drawn as view lights
		if ( tr.primaryWorld != NULL && tr.primaryWorld->portalAreas != NULL ) {
			idBounds walkBounds;
			int primeAreas[128];
			// oversized model bounds (world model, sky boxes) cannot walk
			// the area tree, they just get no private prime light
			int numPrimeAreas = 0;
			if ( R_BoundsForAreaWalk( globalBounds, walkBounds ) ) {
				numPrimeAreas = tr.primaryWorld->BoundsInAreas( walkBounds, primeAreas, 128 );
			}
			const idRenderLightCommitted * primeLights[64];
			int numPrimeLights = 0;
			for ( int i = 0; i < numPrimeAreas && numPrimeLights < 64; i++ ) {
				const portalArea_t * area = &tr.primaryWorld->portalAreas[primeAreas[i]];
				for ( const areaReference_t * ref = area->approxLightRefs.areaNext; ref != &area->approxLightRefs; ref = ref->areaNext ) {
					if ( ref->light == NULL || ref->light->parms.lightClass != LIGHT_PRIVATE_PRIME || numPrimeLights >= 64 ) {
						continue;
					}
					bool duplicate = false;
					for ( int j = 0; j < numPrimeLights; j++ ) {
						if ( primeLights[j] == ref->light ) {
							duplicate = true;
							break;
						}
					}
					if ( !duplicate ) {
						primeLights[numPrimeLights++] = ref->light;
					}
				}
			}
			for ( int i = 0; i < numPrimeLights; i++ ) {
				const idRenderLightCommitted * light = primeLights[i];
				const idMaterial * lightShader = light->lightShader;
				if ( lightShader == NULL || lightShader->IsFogLight() || lightShader->IsBlendLight() ) {
					continue;
				}
				float * lightRegs = (float *)_alloca16( lightShader->GetNumRegisters() * sizeof( float ) );
				const float globalShaderParms[MAX_GLOBAL_SHADER_PARMS] = {};
				lightShader->EvaluateRegisters( lightRegs, light->parms.shaderParms, globalShaderParms, tr.viewDef->renderView.time[0] * 0.001f, light->parms.referenceSound );
				idVec3 color( 0.0f, 0.0f, 0.0f );
				for ( int s = 0; s < lightShader->GetNumStages(); s++ ) {
					const shaderStage_t * stage = lightShader->GetStage( s );
					if ( lightRegs[stage->conditionRegister] == 0.0f ) {
						continue;
					}
					color.x = lightRegs[stage->color.registers[0]];
					color.y = lightRegs[stage->color.registers[1]];
					color.z = lightRegs[stage->color.registers[2]];
					break;
				}
				float weight = color.Length();
				if ( weight <= 0.0f ) {
					continue;
				}
				idVec3 dir = center - light->globalLightOrigin;
				const float dist = dir.Normalize();
				if ( dist <= 1.0f ) {
					continue;
				}
				// smooth falloff over the light radius
				const float radius = Max( ( light->globalLightBounds[1] - light->globalLightBounds[0] ).Length() * 0.5f, 1.0f );
				weight *= radius * radius / ( radius * radius + dist * dist );
				accum += dir * weight;
			}
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
			const float noShadows = ( vLight->lightDef != NULL && vLight->lightDef->parms.noShadows ) ? 1.0f : 0.0f;
			cl.lightOrigin = idVec4( vLight->globalLightOrigin.x, vLight->globalLightOrigin.y, vLight->globalLightOrigin.z, noShadows );
			cl.lightColor = color;
		}
		LocalLighting( parms, vEnt->approximateLighting );

		// Tech5 debug visualization of the approximate lighting data
		if ( r_showDynamicLighting.GetInteger() > 0 || r_showAmbientLighting.GetBool() ) {
			R_ShowApproximateLighting( vEnt, parms );
		}
	}
}

