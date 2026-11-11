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
#include "renderpasses.h"
#include "renderselfshadows.h"

// defined in renderpasses.cpp
void RB_SetMVP( const idRenderMatrix & mvp );

/*
================================================================================================

TNO style per model self shadows.

Re-implements the semantics of the Wolfenstein: The New Order
renderselfshadows job on this engine idlib. The original MachineGames code
is decompiled proprietary material and not GPL compatible, nothing was
copied from it.

For every model whose approximate lighting uses the local lights model:
1. Build up to four shadow projections in model space, one per radial
   light slot plus one ambient occlusion projection from the view origin.
2. Render the model's own surfaces depth only into its four tiles of a
   shared depth atlas.
3. Multiply the model's surfaces on the default framebuffer by the
   filtered shadow result, darkening them where they occlude themselves.

Tech5 walks committed render models and their slave attachments here, this
engine has no committed model architecture, so every view entity is its
own self shadow model. See TODO.md for the remaining gaps.

================================================================================================
*/

idCVar r_selfShadows( "r_selfShadows", "1", CVAR_RENDERER | CVAR_BOOL, "TNO style per model self shadows from the approximate lighting light slots" );
idCVar r_selfShadowResolution( "r_selfShadowResolution", "1024", CVAR_RENDERER | CVAR_INTEGER, "resolution of the shared self shadow atlas", 64, 2048 );
idCVar r_selfShadowLixelScale( "r_selfShadowLixelScale", "1.0", CVAR_RENDERER | CVAR_FLOAT, "clip space to lixel scale factor, 0 disables the per model resolution", 0.0f, 16.0f );
idCVar r_selfShadowMaxLixelsPerUnit( "r_selfShadowMaxLixelsPerUnit", "8.0", CVAR_RENDERER | CVAR_FLOAT, "cap shadow map size to a factor of bounding box radius", 0.0f, 64.0f );
idCVar r_selfShadowResolutionCap( "r_selfShadowResolutionCap", "2048", CVAR_RENDERER | CVAR_INTEGER, "cap self shadow tile size", 64, 2048 );
idCVar r_selfShadowDensity( "r_selfShadowDensity", "0.6", CVAR_RENDERER | CVAR_FLOAT, "maximum darkness of the self shadows", 0.0f, 1.0f );
idCVar r_selfShadowBias( "r_selfShadowBias", "0.002", CVAR_RENDERER | CVAR_FLOAT, "normalized depth bias that keeps lit surfaces unshadowed", 0.0f, 0.1f );
idCVar r_selfShadowMaxRange( "r_selfShadowMaxRange", "1500", CVAR_RENDERER | CVAR_FLOAT, "distance at which self shadows are fully faded out", 0.0f, 10000.0f );
idCVar r_selfShadowFadeStart( "r_selfShadowFadeStart", "1000", CVAR_RENDERER | CVAR_FLOAT, "distance at which self shadows start to fade", 0.0f, 10000.0f );
idCVar r_selfShadowMaxModels( "r_selfShadowMaxModels", "4", CVAR_RENDERER | CVAR_INTEGER, "maximum number of self shadow models per view", 0, 16 );
idCVar r_selfShadowMaxModelSize( "r_selfShadowMaxModelSize", "400", CVAR_RENDERER | CVAR_FLOAT, "skip models whose bounds diagonal is larger than this", 0.0f, 100000.0f );
idCVar r_selfShadowAmbientOcclusion( "r_selfShadowAmbientOcclusion", "1", CVAR_RENDERER | CVAR_BOOL, "add a fourth self shadow projection from the view origin as ambient occlusion" );

static const int MAX_SELF_SHADOW_MODELS = 16;
static const int SELF_SHADOW_TILES = 4;
// Tech5 spot shadow near plane minimum
static const float SELF_SHADOW_MIN_NEAR = 8.0f;
// light colors weaker than this get no shadow projection
static const float SELF_SHADOW_MIN_COLOR = 1.0e-8f;

static idImage * selfShadowImage = NULL;
static GLuint selfShadowFBO = 0;
static GLuint selfShadowTextureObject = 0;
static int selfShadowTargetSize = 0;

// this idlib's idBounds has no IsEmpty, compare the corners directly
static bool SelfShadowBoundsEmpty( const idBounds & b ) {
	return b[0][0] > b[1][0] || b[0][1] > b[1][1] || b[0][2] > b[1][2];
}

/*
==================
RB_ShutdownSelfShadows
==================
*/
void RB_ShutdownSelfShadows() {
	if ( selfShadowFBO != 0 ) {
		GL_DeleteFramebuffers( 1, &selfShadowFBO );
		selfShadowFBO = 0;
	}
	selfShadowTextureObject = 0;
	selfShadowImage = NULL;
	selfShadowTargetSize = 0;
}

/*
==================
RB_EnsureSelfShadowTarget

Depth atlas for the creation pass. Follows the RB_EnsureShadowAtlas
allocation pattern: depth scratch image, explicit compare mode on the real
active unit, own framebuffer with color attachments disabled.
==================
*/
static bool RB_EnsureSelfShadowTarget() {
	const int size = r_selfShadowResolution.GetInteger();

	const bool textureValid = selfShadowImage != NULL && selfShadowImage->IsLoaded() && selfShadowImage->GetAPIObject() != 0 &&
		selfShadowImage->GetUploadWidth() == size && selfShadowImage->GetUploadHeight() == size;
	if ( !textureValid || selfShadowFBO == 0 || selfShadowTextureObject != selfShadowImage->GetAPIObject() ) {
		if ( selfShadowFBO != 0 ) {
			GL_DeleteFramebuffers( 1, &selfShadowFBO );
			selfShadowFBO = 0;
		}
		selfShadowTextureObject = 0;
		idImageOpts opts;
		opts.textureType = TT_2D;
		opts.format = FMT_DEPTH;
		opts.width = size;
		opts.height = size;
		opts.numLevels = 1;
		selfShadowImage = globalImages->ScratchImage( "_selfShadowDepth", &opts, TF_LINEAR, TR_CLAMP, TD_DEPTH );
		if ( selfShadowImage == NULL || !selfShadowImage->IsLoaded() ) {
			common->Warning( "could not allocate the self shadow atlas" );
			selfShadowTargetSize = 0;
			return false;
		}
		selfShadowImage->SetReferencedOutsideLevelLoad();
		selfShadowTargetSize = size;
		// the sample pass reads this as sampler2DShadow, so the comparison
		// mode has to live on the texture object. Legacy glTexParameteri
		// operates on the real active unit, so select it explicitly.
		GLint previousActiveTexture = GL_TEXTURE0;
		GL_GetIntegerv( GET_ACTIVE_TEXTURE, &previousActiveTexture );
		GL_ActiveTexture( 1 );
		GL_BindTexture( TEX_TARGET_2D, selfShadowImage->GetAPIObject() );
		GL_TexParameteri( TEX_TARGET_2D, TEXPARAM_COMPARE_MODE, TEX_COMPARE_REF_TO_TEXTURE );
		GL_TexParameteri( TEX_TARGET_2D, TEXPARAM_COMPARE_FUNC, TEX_COMPARE_LEQUAL );
		GL_ActiveTexture( previousActiveTexture );
		GL_GenFramebuffers( 1, &selfShadowFBO );
		GLint previousFramebuffer = 0;
		GLint previousDrawBuffer = GL_BACK;
		GLint previousReadBuffer = GL_BACK;
		GL_GetIntegerv( GET_FRAMEBUFFER_BINDING, &previousFramebuffer );
		GL_GetIntegerv( GET_DRAW_BUFFER, &previousDrawBuffer );
		GL_GetIntegerv( GET_READ_BUFFER, &previousReadBuffer );
		GL_BindFramebuffer( selfShadowFBO );
		GL_FramebufferTexture2D( ATTACHMENT_DEPTH, TEX_TARGET_2D, selfShadowImage->GetAPIObject(), 0 );
		GL_DrawBuffer( BUFFER_SELECT_NONE );
		GL_ReadBuffer( BUFFER_SELECT_NONE );
		const bool status = GL_CheckFramebufferStatus();
		GL_BindFramebuffer( previousFramebuffer );
		GL_DrawBuffer( previousDrawBuffer );
		GL_ReadBuffer( previousReadBuffer );
		if ( !status ) {
			common->Warning( "self shadow framebuffer incomplete" );
			GL_DeleteFramebuffers( 1, &selfShadowFBO );
			selfShadowFBO = 0;
			selfShadowTextureObject = 0;
			return false;
		}
		selfShadowTextureObject = selfShadowImage->GetAPIObject();
	} else {
		selfShadowImage->SetReferencedOutsideLevelLoad();
	}
	return true;
}

// Transform a model-space point to world space with the entity render matrix.
static ID_INLINE idVec3 RB_SelfShadowTransformPoint( const idRenderMatrix & m, const idVec3 & p ) {
	return idVec3(
		p.x * m[0][0] + p.y * m[1][0] + p.z * m[2][0] + m[3][0],
		p.x * m[0][1] + p.y * m[1][1] + p.z * m[2][1] + m[3][1],
		p.x * m[0][2] + p.y * m[1][2] + p.z * m[2][2] + m[3][2] );
}

/*
==================
RB_SelfShadowSpotProjection

Iterative tilted spot projection over a model space bounds, the Tech5
SetupShadowProjection. Refits a light basis to the bounds corners four
times, then builds a perspective projection over the accumulated light
space extents. Returns false when no usable projection exists.
==================
*/
static bool RB_SelfShadowSpotProjection( const idVec3 & localLightOrigin, const idBounds & localBounds, idRenderMatrix & shadowProjection, idRenderMatrix & localShadowProjection, float & projectionDepth ) {
	idVec3 direction = localLightOrigin - ( localBounds[0] + localBounds[1] ) * 0.5f;

	float minimumX = 1.0e20f;
	float minimumY = 1.0e20f;
	float minimumDepth = 1.0e20f;
	float maximumX = -1.0e20f;
	float maximumY = -1.0e20f;
	float maximumDepth = -1.0e20f;
	idRenderMatrix lightMatrix;

	for ( int iteration = 0; iteration < 4; iteration++ ) {
		if ( direction.Normalize() == 0.0f ) {
			return false;
		}
		idVec3 vertical;
		if ( idMath::Fabs( direction.z ) >= 0.99f ) {
			const float yz = direction.z + direction.y;
			vertical.Set( -direction.x * yz, 1.0f - direction.y * yz, 1.0f - direction.z * yz );
		} else {
			vertical.Set( direction.z * direction.x, direction.z * direction.y, direction.z * direction.z - 1.0f );
		}
		if ( vertical.Normalize() == 0.0f ) {
			return false;
		}
		const idVec3 horizontal = vertical.Cross( direction );

		lightMatrix = idRenderMatrix(
			horizontal.x, horizontal.y, horizontal.z, -( horizontal * localLightOrigin ),
			vertical.x, vertical.y, vertical.z, -( vertical * localLightOrigin ),
			direction.x, direction.y, direction.z, -( direction * localLightOrigin ),
			0.0f, 0.0f, 0.0f, 1.0f );

		minimumX = 1.0e20f;
		minimumY = 1.0e20f;
		minimumDepth = 1.0e20f;
		maximumX = -1.0e20f;
		maximumY = -1.0e20f;
		maximumDepth = -1.0e20f;
		for ( int corner = 0; corner < 8; corner++ ) {
			const idVec3 point( localBounds[corner & 1].x, localBounds[( corner >> 1 ) & 1].y, localBounds[( corner >> 2 ) & 1].z );
			const idVec3 projected = RB_SelfShadowTransformPoint( lightMatrix, point );
			const float viewDepth = -projected.z;
			if ( viewDepth >= SELF_SHADOW_MIN_NEAR ) {
				const float inverseDepth = 1.0f / viewDepth;
				const float x = projected.x * inverseDepth;
				const float y = projected.y * inverseDepth;
				minimumX = Min( minimumX, x );
				minimumY = Min( minimumY, y );
				minimumDepth = Min( minimumDepth, viewDepth );
				maximumX = Max( maximumX, x );
				maximumY = Max( maximumY, y );
				maximumDepth = Max( maximumDepth, viewDepth );
			} else {
				minimumDepth = SELF_SHADOW_MIN_NEAR;
			}
		}

		// Tech5 correction: tilt the view direction toward the bounds
		// center until the projection is roughly centered
		const float xAsymmetry = idMath::Fabs( maximumX + minimumX );
		const float yAsymmetry = idMath::Fabs( maximumY + minimumY );
		if ( Max( xAsymmetry, yAsymmetry ) > 0.99f ) {
			break;
		}
		direction -= horizontal * ( 0.353553385f * ( maximumX + minimumX ) );
		direction -= vertical * ( 0.353553385f * ( maximumY + minimumY ) );
	}

	if ( maximumDepth <= minimumDepth ) {
		return false;	// every corner was too close to the light
	}
	const float nearPlane = Max( minimumDepth, SELF_SHADOW_MIN_NEAR );
	const float farPlane = maximumDepth;
	const float maxX = Max( Max( -minimumX, maximumX ), 0.001f );
	const float maxY = Max( Max( -minimumY, maximumY ), 0.001f );

	// perspective projection over the light space slopes, far plane at the
	// most negative eye z like the standard GL convention
	shadowProjection = idRenderMatrix(
		1.0f / maxX, 0.0f, 0.0f, 0.0f,
		0.0f, 1.0f / maxY, 0.0f, 0.0f,
		0.0f, 0.0f, ( farPlane + nearPlane ) / ( farPlane - nearPlane ), -2.0f * farPlane * nearPlane / ( farPlane - nearPlane ),
		0.0f, 0.0f, -1.0f, 0.0f );
	idRenderMatrix::Multiply( shadowProjection, lightMatrix, localShadowProjection );
	projectionDepth = farPlane - nearPlane;
	return true;
}

// Disabled tile: point every sample at the cleared far corner of the atlas
// so the compare always returns lit.
static void RB_SelfShadowDisabledMatrix( int destinationResolution, idRenderMatrix & shadowMap ) {
	const float corner = (float)( destinationResolution - 1 ) / (float)destinationResolution;
	shadowMap = idRenderMatrix(
		0.0f, 0.0f, 0.0f, corner,
		0.0f, 0.0f, 0.0f, corner,
		0.0f, 0.0f, 0.0f, 0.0f,
		0.0f, 0.0f, 0.0f, 1.0f );
}

/*
==================
RB_RenderSelfShadows

Called from RB_DrawViewInternal after RB_RenderDimShadows and before the
light interactions.
==================
*/
void RB_RenderSelfShadows() {
	if ( !r_selfShadows.GetBool() || r_selfShadowMaxModels.GetInteger() <= 0 ) {
		return;
	}
	if ( backEnd.viewDef->isSubview || backEnd.viewDef->is2Dgui || backEnd.viewDef->viewEntitys == NULL ) {
		return;
	}

	// collect the models first so the atlas allocation only happens when a
	// model actually wants self shadows
	const viewEntity_t * models[MAX_SELF_SHADOW_MODELS];
	idBounds modelWorldBounds[MAX_SELF_SHADOW_MODELS];
	idBounds modelLocalBounds[MAX_SELF_SHADOW_MODELS];
	int numModels = 0;
	for ( const viewEntity_t * vEntity = backEnd.viewDef->viewEntitys; vEntity != NULL && numModels < MAX_SELF_SHADOW_MODELS; vEntity = vEntity->next ) {
		if ( vEntity->isGuiSurface || vEntity->weaponDepthHack ) {
			continue;
		}
		// Tech5 gate: only models whose approximate lighting uses the local
		// lights model get self shadows
		if ( vEntity->approximateLighting.lightingModel.x != 1.0f || vEntity->approximateLighting.lightingModel.y != 1.0f ) {
			continue;
		}
		idBounds worldBounds, localBounds;
		worldBounds.Clear();
		localBounds.Clear();
		int numSurfs = 0;
		for ( const drawSurf_t * surf = vEntity->drawSurfs; surf != NULL; surf = surf->nextOnLight ) {
			if ( surf->numIndexes == 0 || surf->material == NULL || surf->frontEndGeo == NULL ) {
				continue;
			}
			if ( surf->material->Coverage() == MC_TRANSLUCENT ) {
				continue;
			}
			if ( !vertexCache.CacheIsCurrent( surf->ambientCache ) || !vertexCache.CacheIsCurrent( surf->indexCache ) ) {
				continue;
			}
			worldBounds.AddPoint( RB_SelfShadowTransformPoint( vEntity->modelRenderMatrix, surf->frontEndGeo->bounds[0] ) );
			worldBounds.AddPoint( RB_SelfShadowTransformPoint( vEntity->modelRenderMatrix, surf->frontEndGeo->bounds[1] ) );
			localBounds.AddPoint( surf->frontEndGeo->bounds[0] );
			localBounds.AddPoint( surf->frontEndGeo->bounds[1] );
			numSurfs++;
		}
		if ( numSurfs == 0 || SelfShadowBoundsEmpty( worldBounds ) || SelfShadowBoundsEmpty( localBounds ) ) {
			continue;
		}
		if ( ( worldBounds[1] - worldBounds[0] ).Length() > r_selfShadowMaxModelSize.GetFloat() ) {
			continue;
		}
		models[numModels] = vEntity;
		modelWorldBounds[numModels] = worldBounds;
		modelLocalBounds[numModels] = localBounds;
		numModels++;
	}
	if ( numModels == 0 ) {
		return;
	}

	if ( !RB_EnsureSelfShadowTarget() ) {
		return;
	}

	renderLog.OpenBlock( "RB_RenderSelfShadows" );

	const idScreenRect & viewport = backEnd.viewDef->viewport;
	const int viewWidth = viewport.x2 - viewport.x1 + 1;
	const int viewHeight = viewport.y2 - viewport.y1 + 1;

	GLint previousFramebuffer = 0;
	GL_GetIntegerv( GET_FRAMEBUFFER_BINDING, &previousFramebuffer );
	const int previousCullType = backEnd.glState.faceCulling;
	const uint64 previousState = GL_GetCurrentState();

	const float maxRange = r_selfShadowMaxRange.GetFloat();
	const float fadeStart = Min( r_selfShadowFadeStart.GetFloat(), maxRange );
	const float density = r_selfShadowDensity.GetFloat();
	const float bias = r_selfShadowBias.GetFloat();
	const int destinationResolution = selfShadowTargetSize;
	const float inverseDestinationResolution = 1.0f / (float)destinationResolution;

	int numRendered = 0;
	for ( int i = 0; i < numModels; i++ ) {
		const viewEntity_t * vEntity = models[i];

		const idVec3 boundsCenter = ( modelWorldBounds[i][0] + modelWorldBounds[i][1] ) * 0.5f;
		const float dist = ( boundsCenter - backEnd.viewDef->renderView.vieworg ).Length();
		if ( dist >= maxRange ) {
			continue;
		}
		float fade = density;
		if ( dist > fadeStart ) {
			fade *= 1.0f - ( dist - fadeStart ) / ( maxRange - fadeStart );
		}
		if ( fade <= 0.0f ) {
			continue;
		}

		// per model tile resolution, the TNO lixel math
		int tileResolution = destinationResolution / 2;
		if ( r_selfShadowLixelScale.GetFloat() > 0.0f ) {
			const idVec3 boundsRadiusVector = modelWorldBounds[i][1] - boundsCenter;
			const float boundsRadius = idMath::Sqrt( Max( boundsRadiusVector.LengthSqr(), idMath::FLT_SMALLEST_NON_DENORMAL ) );
			const idRenderMatrix & projection = backEnd.viewDef->projectionRenderMatrix;
			const float projectedSize = ( boundsRadius * projection[0][0] - dist * projection[0][2] + projection[0][3] ) /
				( boundsRadius * projection[3][0] - dist * projection[3][2] + projection[3][3] );
			if ( projectedSize < 0.02f ) {
				continue;
			}
			const int densityResolution = (int)( boundsRadius * r_selfShadowMaxLixelsPerUnit.GetFloat() );
			const int screenResolution = (int)( (float)viewWidth * r_selfShadowLixelScale.GetFloat() * 0.5f * projectedSize );
			tileResolution = Min( densityResolution, screenResolution ) & ~1;
			tileResolution = Min( tileResolution, Min( r_selfShadowResolutionCap.GetInteger(), destinationResolution / 2 ) );
			tileResolution = Max( tileResolution, 2 );
		}

		numRendered++;

		// inverse model matrix: world to model space
		idRenderMatrix inverseModelMatrix;
		idRenderMatrix::Inverse( vEntity->modelRenderMatrix, inverseModelMatrix );

		const approximateLighting_t & lighting = vEntity->approximateLighting;
		idVec4 lightOrigins[SELF_SHADOW_TILES];
		idVec4 lightColors[SELF_SHADOW_TILES];
		for ( int t = 0; t < 3; t++ ) {
			lightOrigins[t] = lighting.lightOrigin[t];
			lightColors[t] = lighting.lightColor[t];
		}
		// fourth tile: ambient occlusion projection from the view origin,
		// gated by the first ambient channel like the Tech5 job
		lightOrigins[3] = idVec4( backEnd.viewDef->renderView.vieworg, 0.0f );
		lightColors[3] = r_selfShadowAmbientOcclusion.GetBool() ? lighting.channelLighting[0] : idVec4( 0.0f, 0.0f, 0.0f, 0.0f );

		// depth only render of the model into its atlas tiles
		GL_BindFramebuffer( selfShadowFBO );
		GL_ViewportAndScissor( 0, 0, destinationResolution, destinationResolution );
		GL_State( GLS_COLORMASK | GLS_DEPTHMASK | GLS_DEPTHFUNC_LESS );
		GL_Cull( CT_TWO_SIDED );
		GL_ClearDepth( 1.0 );
		GL_Clear( false, true, false, 0, 0, 0, 0, 0 );

		idRenderMatrix shadowProjection[SELF_SHADOW_TILES];
		idRenderMatrix worldToShadowMap[SELF_SHADOW_TILES];
		float projectionDepth[SELF_SHADOW_TILES];
		int numActiveProjections = 0;

		for ( int tile = 0; tile < SELF_SHADOW_TILES; tile++ ) {
			const idVec4 & lightOrigin = lightOrigins[tile];
			const idVec4 & lightColor = lightColors[tile];
			if ( lightColor.x + lightColor.y + lightColor.z <= SELF_SHADOW_MIN_COLOR || lightOrigin.w == 1.0f ) {
				RB_SelfShadowDisabledMatrix( destinationResolution, worldToShadowMap[tile] );
				shadowProjection[tile].Identity();
				projectionDepth[tile] = 1.0f;
				continue;
			}

			idVec3 localLightOrigin = RB_SelfShadowTransformPoint( inverseModelMatrix, lightOrigin.ToVec3() );
			// expand the bounds and push a light inside them out along the
			// center ray so a projection always exists
			idBounds expandedBounds = modelLocalBounds[i];
			expandedBounds.ExpandSelf( 4.0f );
			if ( localLightOrigin.x >= expandedBounds[0].x && localLightOrigin.x <= expandedBounds[1].x &&
				localLightOrigin.y >= expandedBounds[0].y && localLightOrigin.y <= expandedBounds[1].y &&
				localLightOrigin.z >= expandedBounds[0].z && localLightOrigin.z <= expandedBounds[1].z ) {
				const idVec3 expandedCenter = ( expandedBounds[0] + expandedBounds[1] ) * 0.5f;
				idVec3 rayDirection = expandedCenter - localLightOrigin;
				if ( rayDirection.Normalize() != 0.0f ) {
					float scale1 = 0.0f;
					float scale2 = 0.0f;
					if ( expandedBounds.RayIntersection( localLightOrigin, rayDirection, scale1, scale2 ) ) {
						localLightOrigin += rayDirection * scale1;
					}
				}
			}

			idRenderMatrix localShadowProjection;
			if ( !RB_SelfShadowSpotProjection( localLightOrigin, modelLocalBounds[i], shadowProjection[tile], localShadowProjection, projectionDepth[tile] ) ) {
				RB_SelfShadowDisabledMatrix( destinationResolution, worldToShadowMap[tile] );
				shadowProjection[tile].Identity();
				projectionDepth[tile] = 1.0f;
				continue;
			}
			numActiveProjections++;

			// world to light clip, then clip to the atlas tile window
			idRenderMatrix worldShadowProjection;
			idRenderMatrix::Multiply( localShadowProjection, inverseModelMatrix, worldShadowProjection );
			const float tileScale = (float)tileResolution * inverseDestinationResolution;
			const float tileOffsetX = (float)( tile & 1 ) * tileScale;
			const float tileOffsetY = (float)( ( tile >> 1 ) & 1 ) * tileScale;
			const idRenderMatrix tileBias(
				0.5f * tileScale, 0.0f, 0.0f, tileOffsetX + 0.5f * tileScale,
				0.0f, 0.5f * tileScale, 0.0f, tileOffsetY + 0.5f * tileScale,
				0.0f, 0.0f, 0.5f, 0.5f,
				0.0f, 0.0f, 0.0f, 1.0f );
			idRenderMatrix::Multiply( tileBias, worldShadowProjection, worldToShadowMap[tile] );

			const int tileX = ( tile & 1 ) * tileResolution;
			const int tileY = ( ( tile >> 1 ) & 1 ) * tileResolution;
			GL_ViewportAndScissor( tileX, tileY, tileResolution, tileResolution );
			for ( const drawSurf_t * surf = vEntity->drawSurfs; surf != NULL; surf = surf->nextOnLight ) {
				if ( surf->numIndexes == 0 || surf->material == NULL || surf->frontEndGeo == NULL || surf->material->Coverage() == MC_TRANSLUCENT ) {
					continue;
				}
				if ( !vertexCache.CacheIsCurrent( surf->ambientCache ) || !vertexCache.CacheIsCurrent( surf->indexCache ) ) {
					continue;
				}
				idRenderMatrix mvp;
				idRenderMatrix::Multiply( worldShadowProjection, surf->space->modelRenderMatrix, mvp );
				if ( idRenderMatrix::CullBoundsToMVP( mvp, surf->frontEndGeo->bounds, false ) ) {
					continue;
				}
				RB_SetMVP( mvp );

				// alpha tested surfaces punch holes into the depth atlas
				// with their alpha test stages, the same pattern the dim
				// shadow caster pass uses
				bool drewAlphaStage = false;
				if ( surf->material->Coverage() == MC_PERFORATED ) {
					const float * regs = surf->shaderRegisters;
					for ( int stage = 0; stage < surf->material->GetNumStages(); stage++ ) {
						const shaderStage_t * pStage = surf->material->GetStage( stage );
						if ( !pStage->hasAlphaTest || regs[pStage->conditionRegister] == 0.0f || regs[pStage->color.registers[3]] <= 0.0f ) {
							continue;
						}
						drewAlphaStage = true;
						const idVec4 alphaTestValue( regs[pStage->alphaTestRegister] );
						renderProgManager.SetUniformValue( RENDERPARM_ALPHA_TEST, alphaTestValue.ToFloatPtr() );
						if ( surf->jointCache ) {
							renderProgManager.BindShader_TextureVertexColorSkinned();
						} else {
							renderProgManager.BindShader_TextureVertexColor();
						}
						// keep the vertex color out of the alpha mask
						const float zero[4] = { 0.0f, 0.0f, 0.0f, 0.0f };
						const float one[4] = { 1.0f, 1.0f, 1.0f, 1.0f };
						renderProgManager.SetUniformValue( RENDERPARM_VERTEXCOLOR_MODULATE, zero );
						renderProgManager.SetUniformValue( RENDERPARM_VERTEXCOLOR_ADD, one );
						GL_SelectTexture( 0 );
						pStage->texture.image->Bind();
						RB_DrawElementsWithCounters( surf );
					}
					renderProgManager.SetUniformValue( RENDERPARM_ALPHA_TEST, vec4_zero.ToFloatPtr() );
				}
				if ( drewAlphaStage ) {
					continue;
				}
				if ( surf->jointCache ) {
					renderProgManager.BindShader_DepthSkinned();
				} else {
					renderProgManager.BindShader_Depth();
				}
				RB_DrawElementsWithCounters( surf );
			}
		}

		if ( numActiveProjections <= 0 ) {
			continue;
		}

		// multiply the shadow factor over the model's surfaces on the
		// default framebuffer, the depth equal test matches the pixels the
		// main passes drew for this model
		GL_BindFramebuffer( previousFramebuffer );
		GL_ViewportAndScissor( viewport.x1, viewport.y1, viewWidth, viewHeight );
		GL_State( GLS_SRCBLEND_DST_COLOR | GLS_DSTBLEND_ZERO | GLS_DEPTHFUNC_EQUAL );
		GL_Cull( CT_TWO_SIDED );
		GL_SelectTexture( 0 );
		selfShadowImage->Bind();

		const float shadowParms[4] = { fade, bias, 0.0f, 0.0f };
		renderProgManager.SetUniformValue( RENDERPARM_SHADOWPARAMS, shadowParms );

		for ( const drawSurf_t * surf = vEntity->drawSurfs; surf != NULL; surf = surf->nextOnLight ) {
			if ( surf->numIndexes == 0 || surf->material == NULL || surf->frontEndGeo == NULL || surf->material->Coverage() == MC_TRANSLUCENT ) {
				continue;
			}
			if ( !vertexCache.CacheIsCurrent( surf->ambientCache ) || !vertexCache.CacheIsCurrent( surf->indexCache ) ) {
				continue;
			}
			idRenderMatrix mvp;
			idRenderMatrix::Multiply( backEnd.viewDef->worldSpace.mvp, surf->space->modelRenderMatrix, mvp );
			RB_SetMVP( mvp );
			// model space to atlas tile uv and compare depth, tile 0 and 1
			// ride in the shadow matrix parms, tile 2 and 3 in the user parms
			for ( int tile = 0; tile < SELF_SHADOW_TILES; tile++ ) {
				idRenderMatrix modelToShadow;
				idRenderMatrix::Multiply( worldToShadowMap[tile], surf->space->modelRenderMatrix, modelToShadow );
				const int parmBase = ( tile < 2 ) ? ( RENDERPARM_SHADOWMATRIX_X + tile * 4 ) : ( RENDERPARM_USER + ( tile - 2 ) * 4 );
				for ( int row = 0; row < 4; row++ ) {
					renderProgManager.SetUniformValue( (renderParm_t)( parmBase + row ), modelToShadow[row] );
				}
			}
			if ( surf->jointCache ) {
				renderProgManager.BindShader_SelfShadowSampleSkinned();
			} else {
				renderProgManager.BindShader_SelfShadowSample();
			}
			RB_DrawElementsWithCounters( surf );
		}
	}

	renderProgManager.Unbind();
	GL_Cull( previousCullType );
	GL_BindFramebuffer( previousFramebuffer );
	GL_State( previousState );
	GL_Viewport( viewport.x1, viewport.y1, viewport.x2 + 1 - viewport.x1, viewport.y2 + 1 - viewport.y1 );
	GL_Scissor( backEnd.viewDef->viewport.x1 + backEnd.viewDef->scissor.x1,
		backEnd.viewDef->viewport.y1 + backEnd.viewDef->scissor.y1,
		backEnd.viewDef->scissor.x2 + 1 - backEnd.viewDef->scissor.x1,
		backEnd.viewDef->scissor.y2 + 1 - backEnd.viewDef->scissor.y1 );
	backEnd.currentScissor = backEnd.viewDef->scissor;
	backEnd.currentSpace = NULL;
	GL_SelectTexture( 0 );

	renderLog.CloseBlock();
}
