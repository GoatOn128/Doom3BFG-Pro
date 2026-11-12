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

// defined in renderpasses.cpp
void RB_SetMVP( const idRenderMatrix & mvp );

/*
================================================================================================

TNO style dim shadows.

Re-implements the semantics of the Wolfenstein: The New Order renderdimshadows
job on this engine idlib. The original MachineGames code is decompiled
proprietary material and not GPL compatible, nothing was copied from it.

Per dim shadow caster:
1. Render the caster geometry depth only into a shared depth target from the
   prime light direction with an orthographic projection over the extruded
   caster bounds.
2. Blend a black shadow factor over the caster screen region of the default
   framebuffer. The fragment program reconstructs the world position of each
   pixel from a copy of the view depth, projects it into the dim shadow target
   and compares depths, so only surfaces behind the caster relative to the
   light are darkened.

The prime light comes from a front end approximate lighting pass that
accumulates the non shadowed lights of the view per model, TNO style.
Models without a usable prime direction fall back to the dominant parallel
shadow light of the view.

Alpha tested casters render their active alpha test stages with the
textured vertex color program so the mask holes show up in the depth
target, matching the alpha bucket of the TNO dim shadow job.

================================================================================================
*/

idCVar r_dimShadows( "r_dimShadows", "1", CVAR_RENDERER | CVAR_BOOL, "TNO style per model dim shadows from the prime light" );
idCVar r_dimShadowResolution( "r_dimShadowResolution", "1024", CVAR_RENDERER | CVAR_INTEGER, "resolution of the shared dim shadow depth target", 64, 2048 );
idCVar r_dimShadowLixelScale( "r_dimShadowLixelScale", "1.0", CVAR_RENDERER | CVAR_FLOAT, "clip space to lixel scale factor, 0 disables the per caster resolution", 0.0f, 16.0f );
idCVar r_dimShadowMaxLixelsPerUnit( "r_dimShadowMaxLixelsPerUnit", "8.0", CVAR_RENDERER | CVAR_FLOAT, "cap shadow map size to a factor of bounding box radius", 0.0f, 64.0f );
idCVar r_dimShadowResolutionCap( "r_dimShadowResolutionCap", "2048", CVAR_RENDERER | CVAR_INTEGER, "cap dim shadow size", 64, 2048 );
idCVar r_dimShadowHighQuality( "r_dimShadowHighQuality", "2", CVAR_RENDERER | CVAR_INTEGER, "TNO r_dimShadowForceHighQuality minus the per model flags this engine lacks: 2 = dithered 4x4 PCF, 1 = dithered 2x2 PCF, 0 = standard single tap, -1 = never use the higher quality draws", -1, 2 );
idCVar r_dimShadowDensity( "r_dimShadowDensity", "0.75", CVAR_RENDERER | CVAR_FLOAT, "maximum darkness of the dim shadows", 0.0f, 1.0f );
idCVar r_dimShadowBias( "r_dimShadowBias", "0.002", CVAR_RENDERER | CVAR_FLOAT, "normalized depth bias that keeps the caster itself lit", 0.0f, 0.1f );
idCVar r_dimShadowMaxRange( "r_dimShadowMaxRange", "2200", CVAR_RENDERER | CVAR_FLOAT, "distance at which dim shadows are fully faded out", 0.0f, 10000.0f );
idCVar r_dimShadowFadeStart( "r_dimShadowFadeStart", "1800", CVAR_RENDERER | CVAR_FLOAT, "distance at which dim shadows start to fade", 0.0f, 10000.0f );
idCVar r_dimShadowExtrude( "r_dimShadowExtrude", "1000", CVAR_RENDERER | CVAR_FLOAT, "how far caster bounds are extruded along the light direction", 16.0f, 8192.0f );
idCVar r_dimShadowMaxModels( "r_dimShadowMaxModels", "8", CVAR_RENDERER | CVAR_INTEGER, "maximum number of dim shadow casters per view", 0, 32 );
idCVar r_dimShadowMaxModelSize( "r_dimShadowMaxModelSize", "600", CVAR_RENDERER | CVAR_FLOAT, "skip casters whose bounds diagonal is larger than this", 0.0f, 100000.0f );
idCVar r_dimShadowDepthBounds( "r_dimShadowDepthBounds", "1", CVAR_RENDERER | CVAR_BOOL, "reject dim shadow draw fragments outside the extruded bounds depth range with GL_EXT_depth_bounds_test" );
idCVar r_dimShadowSingleModel( "r_dimShadowSingleModel", "-1", CVAR_RENDERER | CVAR_INTEGER, "debug filter: only this entity index casts a dim shadow, -1 = all", -1, 4096 );

static const int MAX_DIM_SHADOW_CASTERS = 32;

static idImage * dimShadowDepthImage = NULL;
static idImage * dimShadowViewDepthImage = NULL;
static GLuint dimShadowFBO = 0;
static GLuint dimShadowTextureObject = 0;
static int dimShadowTargetSize = 0;

// this idlib's idBounds has no IsEmpty, compare the corners directly
static bool DimShadowBoundsEmpty( const idBounds & b ) {
	return b[0][0] > b[1][0] || b[0][1] > b[1][1] || b[0][2] > b[1][2];
}

/*
==================
RB_ShutdownDimShadows
==================
*/
void RB_ShutdownDimShadows() {
	if ( dimShadowFBO != 0 ) {
		GL_DeleteFramebuffers( 1, &dimShadowFBO );
		dimShadowFBO = 0;
	}
	dimShadowTextureObject = 0;
	dimShadowDepthImage = NULL;
	dimShadowViewDepthImage = NULL;
	dimShadowTargetSize = 0;
}

/*
==================
RB_EnsureDimShadowTarget

Depth target for the caster pass plus a view depth copy target for the draw
pass. Follows the RB_EnsureShadowAtlas allocation pattern: depth scratch
image, explicit compare mode on the real active unit, own framebuffer with
color attachments disabled.
==================
*/
static bool RB_EnsureDimShadowTarget() {
	const int size = r_dimShadowResolution.GetInteger();

	const bool textureValid = dimShadowDepthImage != NULL && dimShadowDepthImage->IsLoaded() && dimShadowDepthImage->GetAPIObject() != 0 &&
		dimShadowDepthImage->GetUploadWidth() == size && dimShadowDepthImage->GetUploadHeight() == size;
	if ( !textureValid || dimShadowFBO == 0 || dimShadowTextureObject != dimShadowDepthImage->GetAPIObject() ) {
		if ( dimShadowFBO != 0 ) {
			GL_DeleteFramebuffers( 1, &dimShadowFBO );
			dimShadowFBO = 0;
		}
		dimShadowTextureObject = 0;
		idImageOpts opts;
		opts.textureType = TT_2D;
		opts.format = FMT_DEPTH;
		opts.width = size;
		opts.height = size;
		opts.numLevels = 1;
		dimShadowDepthImage = globalImages->ScratchImage( "_dimShadowDepth", &opts, TF_LINEAR, TR_CLAMP, TD_DEPTH );
		if ( dimShadowDepthImage == NULL || !dimShadowDepthImage->IsLoaded() ) {
			common->Warning( "could not allocate the dim shadow depth target" );
			dimShadowTargetSize = 0;
			return false;
		}
		dimShadowDepthImage->SetReferencedOutsideLevelLoad();
		dimShadowTargetSize = size;
		// the draw pass samples this as sampler2DShadow, so the comparison
		// mode has to live on the texture object. Legacy glTexParameteri
		// operates on the real active unit, so select it explicitly.
		GLint previousActiveTexture = GL_TEXTURE0;
		GL_GetIntegerv( GET_ACTIVE_TEXTURE, &previousActiveTexture );
		GL_ActiveTexture( 1 );
		GL_BindTexture( TEX_TARGET_2D, dimShadowDepthImage->GetAPIObject() );
		GL_TexParameteri( TEX_TARGET_2D, TEXPARAM_COMPARE_MODE, TEX_COMPARE_REF_TO_TEXTURE );
		GL_TexParameteri( TEX_TARGET_2D, TEXPARAM_COMPARE_FUNC, TEX_COMPARE_LEQUAL );
		GL_ActiveTexture( previousActiveTexture );
		GL_GenFramebuffers( 1, &dimShadowFBO );
		GLint previousFramebuffer = 0;
		GLint previousDrawBuffer = GL_BACK;
		GLint previousReadBuffer = GL_BACK;
		GL_GetIntegerv( GET_FRAMEBUFFER_BINDING, &previousFramebuffer );
		GL_GetIntegerv( GET_DRAW_BUFFER, &previousDrawBuffer );
		GL_GetIntegerv( GET_READ_BUFFER, &previousReadBuffer );
		GL_BindFramebuffer( dimShadowFBO );
		GL_FramebufferTexture2D( ATTACHMENT_DEPTH, TEX_TARGET_2D, dimShadowDepthImage->GetAPIObject(), 0 );
		GL_DrawBuffer( BUFFER_SELECT_NONE );
		GL_ReadBuffer( BUFFER_SELECT_NONE );
		const bool status = GL_CheckFramebufferStatus();
		GL_BindFramebuffer( previousFramebuffer );
		GL_DrawBuffer( previousDrawBuffer );
		GL_ReadBuffer( previousReadBuffer );
		if ( !status ) {
			common->Warning( "dim shadow framebuffer incomplete" );
			GL_DeleteFramebuffers( 1, &dimShadowFBO );
			dimShadowFBO = 0;
			dimShadowTextureObject = 0;
			return false;
		}
		dimShadowTextureObject = dimShadowDepthImage->GetAPIObject();
	} else {
		dimShadowDepthImage->SetReferencedOutsideLevelLoad();
	}

	if ( dimShadowViewDepthImage == NULL || !dimShadowViewDepthImage->IsLoaded() ) {
		idImageOpts opts;
		opts.textureType = TT_2D;
		opts.format = FMT_DEPTH;
		opts.width = 64;
		opts.height = 64;
		opts.numLevels = 1;
		dimShadowViewDepthImage = globalImages->ScratchImage( "_dimShadowViewDepth", &opts, TF_NEAREST, TR_CLAMP, TD_DEPTH );
		if ( dimShadowViewDepthImage == NULL || !dimShadowViewDepthImage->IsLoaded() ) {
			common->Warning( "could not allocate the dim shadow view depth image" );
			return false;
		}
		dimShadowViewDepthImage->SetReferencedOutsideLevelLoad();
	}
	return true;
}

// Transform a model-space point to world space with the entity render matrix.
static ID_INLINE idVec3 RB_DimShadowTransformPoint( const idRenderMatrix & m, const idVec3 & p ) {
	return idVec3(
		p.x * m[0][0] + p.y * m[1][0] + p.z * m[2][0] + m[3][0],
		p.x * m[0][1] + p.y * m[1][1] + p.z * m[2][1] + m[3][1],
		p.x * m[0][2] + p.y * m[1][2] + p.z * m[2][2] + m[3][2] );
}

/*
==================
RB_DimShadowViewMatrix

Right handed view matrix looking along the light travel direction, row major
with the translation in the fourth column like every idRenderMatrix.
==================
*/
static void RB_DimShadowViewMatrix( const idVec3 & dir, const idVec3 & origin, idRenderMatrix & view ) {
	const idVec3 zAxis = -dir;
	idVec3 refUp( 0.0f, 0.0f, 1.0f );
	if ( idMath::Fabs( dir.z ) > 0.9f ) {
		refUp.Set( 0.0f, 1.0f, 0.0f );
	}
	idVec3 xAxis = refUp.Cross( zAxis );
	if ( xAxis.Normalize() == 0.0f ) {
		xAxis.Set( 1.0f, 0.0f, 0.0f );
	}
	idVec3 yAxis = zAxis.Cross( xAxis );
	yAxis.Normalize();

	view = idRenderMatrix(
		xAxis.x, xAxis.y, xAxis.z, -( xAxis * origin ),
		yAxis.x, yAxis.y, yAxis.z, -( yAxis * origin ),
		zAxis.x, zAxis.y, zAxis.z, -( zAxis * origin ),
		0.0f, 0.0f, 0.0f, 1.0f );
}

/*
==================
RB_DimShadowProjection

Orthographic projection of a light space box to clip space, far plane at the
most negative eye z like the standard GL convention.
==================
*/
static void RB_DimShadowProjection( const idBounds & lightSpaceBounds, idRenderMatrix & projection ) {
	const idVec3 boxSize = lightSpaceBounds[1] - lightSpaceBounds[0];
	const float dx = Max( boxSize.x, 1.0f );
	const float dy = Max( boxSize.y, 1.0f );
	const float dz = Max( boxSize.z, 1.0f );
	projection = idRenderMatrix(
		2.0f / dx, 0.0f, 0.0f, -( lightSpaceBounds[1].x + lightSpaceBounds[0].x ) / dx,
		0.0f, 2.0f / dy, 0.0f, -( lightSpaceBounds[1].y + lightSpaceBounds[0].y ) / dy,
		0.0f, 0.0f, -2.0f / dz, ( lightSpaceBounds[0].z + lightSpaceBounds[1].z ) / dz,
		0.0f, 0.0f, 0.0f, 1.0f );
}

/*
==================
RB_DimShadowScreenRect

Viewport relative screen rect of a world space bounds, computed like
ScreenRectFromWinding in the front end.
==================
*/
static bool RB_DimShadowScreenRect( const idBounds & bounds, idScreenRect & rect ) {
	const idRenderMatrix & mvp = backEnd.viewDef->worldSpace.mvp;
	const float viewWidth = (float)backEnd.viewDef->viewport.x2 - (float)backEnd.viewDef->viewport.x1;
	const float viewHeight = (float)backEnd.viewDef->viewport.y2 - (float)backEnd.viewDef->viewport.y1;
	rect.Clear();
	for ( int i = 0; i < 8; i++ ) {
		const idVec3 corner( ( i & 1 ) ? bounds[1].x : bounds[0].x,
			( i & 2 ) ? bounds[1].y : bounds[0].y,
			( i & 4 ) ? bounds[1].z : bounds[0].z );
		// clamp the divide instead of rejecting so casters touching the near
		// plane still produce a usable rect
		const float cw = mvp[3][0] * corner.x + mvp[3][1] * corner.y + mvp[3][2] * corner.z + mvp[3][3];
		const float w = ( cw >= 0.01f ) ? cw : 0.01f;
		const float ndcX = ( mvp[0][0] * corner.x + mvp[0][1] * corner.y + mvp[0][2] * corner.z + mvp[0][3] ) / w;
		const float ndcY = ( mvp[1][0] * corner.x + mvp[1][1] * corner.y + mvp[1][2] * corner.z + mvp[1][3] ) / w;
		rect.AddPoint( ( ndcX * 0.5f + 0.5f ) * viewWidth, ( ndcY * 0.5f + 0.5f ) * viewHeight );
	}
	rect.Expand();
	rect.x1 = (short)Max( (int)rect.x1, 0 );
	rect.y1 = (short)Max( (int)rect.y1, 0 );
	rect.x2 = (short)Min( (int)rect.x2, (int)viewWidth );
	rect.y2 = (short)Min( (int)rect.y2, (int)viewHeight );
	return !rect.IsEmpty();
}

/*
==================
RB_DimShadowDistanceToBox

Distance from a point to a bounds, zero inside.
==================
*/
static float RB_DimShadowDistanceToBox( const idBounds & bounds, const idVec3 & point ) {
	float distanceSquared = 0.0f;
	for ( int a = 0; a < 3; a++ ) {
		if ( point[a] < bounds[0][a] ) {
			const float edge = bounds[0][a] - point[a];
			distanceSquared += edge * edge;
		} else if ( point[a] > bounds[1][a] ) {
			const float edge = point[a] - bounds[1][a];
			distanceSquared += edge * edge;
		}
	}
	return idMath::Sqrt( distanceSquared );
}

/*
==================
RB_DimShadowLODDistance

Distance from the camera used to size a caster shadow map: the distance to
the caster bounds pushed out along the light direction, so casters whose
shadow volume stretches away from the viewer get a coarser resolution.
==================
*/
static float RB_DimShadowLODDistance( const idBounds & bounds, const idVec3 & camera, const idVec3 & lightDir ) {
	const idVec3 center = ( bounds[0] + bounds[1] ) * 0.5f;
	const idVec3 delta = center - camera;
	const float radius = ( bounds[1] - bounds[0] ).Length() * 0.5f;
	const float crossDistance = delta.Cross( lightDir ).Length();
	const float alongLight = delta * lightDir;
	const float shadowDistance = Max( Max( 0.0f, crossDistance - radius * 0.5f ),
		Max( -alongLight, Max( 0.0f, alongLight - radius * 3.0f ) ) );
	return Max( Min( RB_DimShadowDistanceToBox( bounds, camera ), shadowDistance ), 8.0f );
}

/*
==================
RB_RenderDimShadows

Called from RB_DrawViewInternal after the depth buffer is filled and before
the light interactions.
==================
*/
void RB_RenderDimShadows() {
	if ( !r_dimShadows.GetBool() || r_dimShadowMaxModels.GetInteger() <= 0 ) {
		return;
	}
	if ( backEnd.viewDef->isSubview || backEnd.viewDef->is2Dgui || backEnd.viewDef->viewEntitys == NULL ) {
		return;
	}

	// view level fallback prime light: the dominant parallel shadow light of
	// the view, used for models without their own approximate lighting
	const viewLight_t * primeLight = NULL;
	float bestScore = 0.0f;
	for ( const viewLight_t * vLight = backEnd.viewDef->viewLights; vLight != NULL; vLight = vLight->next ) {
		if ( !vLight->shadowParallel || ( vLight->globalShadows == NULL && vLight->localShadows == NULL ) ) {
			continue;
		}
		const float score = ( vLight->scissorRect.x2 - vLight->scissorRect.x1 + 1 ) * ( vLight->scissorRect.y2 - vLight->scissorRect.y1 + 1 );
		if ( score > bestScore ) {
			bestScore = score;
			primeLight = vLight;
		}
	}
	// direction the prime light shines, same convention as the shadow map
	// view which looks along -shadowLightCenter
	idVec3 fallbackDir( 0.0f, 0.0f, 0.0f );
	bool haveFallback = false;
	if ( primeLight != NULL ) {
		fallbackDir = -primeLight->shadowLightCenter;
		if ( fallbackDir.Normalize() != 0.0f ) {
			haveFallback = true;
		}
	}

	// collect the casters first so the view depth copy only happens when a
	// caster is actually present
	const viewEntity_t * casters[MAX_DIM_SHADOW_CASTERS];
	idBounds casterBounds[MAX_DIM_SHADOW_CASTERS];
	idVec3 casterDirs[MAX_DIM_SHADOW_CASTERS];
	int numCasters = 0;
	for ( const viewEntity_t * vEntity = backEnd.viewDef->viewEntitys; vEntity != NULL && numCasters < MAX_DIM_SHADOW_CASTERS; vEntity = vEntity->next ) {
		if ( vEntity->isGuiSurface || vEntity->weaponDepthHack ) {
			continue;
		}
		// per model dim shadow selection, the game can turn a caster off
		if ( vEntity->castDimShadows == DIMSHADOW_OFF ) {
			continue;
		}
		// TNO singleDimShadow debug filter: limit the casters to exactly
		// one model index
		if ( r_dimShadowSingleModel.GetInteger() >= 0 && vEntity->entityDef->index != r_dimShadowSingleModel.GetInteger() ) {
			continue;
		}
		idBounds worldBounds;
		worldBounds.Clear();
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
			worldBounds.AddPoint( RB_DimShadowTransformPoint( vEntity->modelRenderMatrix, surf->frontEndGeo->bounds[0] ) );
			worldBounds.AddPoint( RB_DimShadowTransformPoint( vEntity->modelRenderMatrix, surf->frontEndGeo->bounds[1] ) );
			numSurfs++;
		}
		if ( numSurfs == 0 || DimShadowBoundsEmpty( worldBounds ) ) {
			continue;
		}
		// the world model and other very large geometry are not dim shadow
		// casters, TNO gates them out with entity flags this engine lacks
		if ( ( worldBounds[1] - worldBounds[0] ).Length() > r_dimShadowMaxModelSize.GetFloat() ) {
			continue;
		}
		// per model prime light from the front end approximate lighting pass,
		// falling back to the dominant parallel shadow light of the view
		if ( vEntity->primeLightWeight > 0.0f ) {
			// TNO: models whose accumulated approximate lighting is brighter
			// than 0.95 cast no dim shadow at all
			if ( vEntity->primeLightWeight > 0.95f ) {
				continue;
			}
			casterDirs[numCasters] = vEntity->primeLightDir;
		} else if ( haveFallback ) {
			casterDirs[numCasters] = fallbackDir;
		} else {
			continue;
		}
		casters[numCasters] = vEntity;
		casterBounds[numCasters] = worldBounds;
		numCasters++;
	}
	if ( numCasters == 0 ) {
		return;
	}

	if ( !RB_EnsureDimShadowTarget() ) {
		return;
	}

	renderLog.OpenBlock( "RB_RenderDimShadows" );

	// copy the view depth once, the draw pass reconstructs world positions
	// from it
	const idScreenRect & viewport = backEnd.viewDef->viewport;
	const int viewWidth = viewport.x2 - viewport.x1 + 1;
	const int viewHeight = viewport.y2 - viewport.y1 + 1;
	GL_SelectTexture( 0 );
	dimShadowViewDepthImage->CopyDepthbuffer( viewport.x1, viewport.y1, viewWidth, viewHeight );

	GLint previousFramebuffer = 0;
	GL_GetIntegerv( GET_FRAMEBUFFER_BINDING, &previousFramebuffer );
	const int previousCullType = backEnd.glState.faceCulling;
	const uint64 previousState = GL_GetCurrentState();

	const float maxRange = r_dimShadowMaxRange.GetFloat();
	const float fadeStart = Min( r_dimShadowFadeStart.GetFloat(), maxRange );
	const float density = r_dimShadowDensity.GetFloat();
	const float bias = r_dimShadowBias.GetFloat();
	// clip space [-1,1] to depth texture [0,1]
	static const idRenderMatrix windowBias( 0.5f, 0.0f, 0.0f, 0.5f,
		0.0f, 0.5f, 0.0f, 0.5f,
		0.0f, 0.0f, 0.5f, 0.5f,
		0.0f, 0.0f, 0.0f, 1.0f );
	idRenderMatrix identity;
	identity.Identity();

	int numRendered = 0;
	for ( int i = 0; i < numCasters; i++ ) {
		const viewEntity_t * vEntity = casters[i];
		const idBounds & worldBounds = casterBounds[i];

		const idVec3 boundsCenter = ( worldBounds[0] + worldBounds[1] ) * 0.5f;
		const float dist = ( boundsCenter - backEnd.viewDef->renderView.vieworg ).Length();
		if ( dist >= maxRange ) {
			continue;
		}
		float fade = density;
		if ( dist > fadeStart ) {
			fade *= 1.0f - ( dist - fadeStart ) / ( maxRange - fadeStart );
		}
		// TNO light fade: the brighter the model's accumulated approximate
		// lighting, the fainter its dim shadow
		if ( vEntity->primeLightWeight > 0.0f ) {
			fade *= Max( 0.0f, 1.0f - vEntity->primeLightWeight );
		}
		if ( fade <= 0.0f ) {
			continue;
		}

		// per caster resolution: the smaller of a density cap based on the
		// caster radius and the screen space size of the caster, the TNO
		// lixel math. A scale of zero keeps the full target.
		int shadowResolution = dimShadowTargetSize;
		if ( r_dimShadowLixelScale.GetFloat() > 0.0f ) {
			const idVec3 boundsRadiusVector = worldBounds[1] - boundsCenter;
			const float boundsRadius = idMath::Sqrt( Max( boundsRadiusVector.LengthSqr(), idMath::FLT_SMALLEST_NON_DENORMAL ) );
			const float lodDistance = RB_DimShadowLODDistance( worldBounds, backEnd.viewDef->renderView.vieworg, casterDirs[i] );
			const idRenderMatrix & projection = backEnd.viewDef->projectionRenderMatrix;
			const float projectedSize = ( boundsRadius * projection[0][0] - lodDistance * projection[0][2] + projection[0][3] ) /
				( boundsRadius * projection[3][0] - lodDistance * projection[3][2] + projection[3][3] );
			if ( projectedSize < 0.02f ) {
				continue;
			}
			const int densityResolution = (int)( boundsRadius * r_dimShadowMaxLixelsPerUnit.GetFloat() );
			const int screenResolution = (int)( (float)viewWidth * r_dimShadowLixelScale.GetFloat() * 0.5f * projectedSize );
			shadowResolution = Min( densityResolution, screenResolution ) & ~1;
			shadowResolution = Min( shadowResolution, Min( r_dimShadowResolutionCap.GetInteger(), dimShadowTargetSize ) );
			shadowResolution = Max( shadowResolution, 2 );
		}
		const float sampleScale = (float)shadowResolution / (float)dimShadowTargetSize;
		const float sampleOffset = 0.5f * ( 1.0f - sampleScale );

		// extrude the caster bounds along the light travel direction
		const idVec3 extrude = casterDirs[i] * r_dimShadowExtrude.GetFloat();
		idBounds extrudedBounds;
		for ( int a = 0; a < 3; a++ ) {
			extrudedBounds[0][a] = Min( worldBounds[0][a], worldBounds[0][a] + extrude[a] ) - 1.0f;
			extrudedBounds[1][a] = Max( worldBounds[1][a], worldBounds[1][a] + extrude[a] ) + 1.0f;
		}
		// TNO clip plane: the shadow volume stops one model height past the
		// model on the far side from the light, so a caster never darkens
		// geometry farther below or above it than its own height. TNO clips
		// the depth bounds with a plane, clamping the projected box has the
		// same effect through the inside gate of the draw programs.
		const float modelHeight = worldBounds[1][2] - worldBounds[0][2];
		if ( casterDirs[i].z < 0.0f ) {
			float clipZ = worldBounds[0][2] - modelHeight;
			if ( clipZ + 1.0f > worldBounds[0][2] ) {
				clipZ = worldBounds[0][2] - 1.0f;
			}
			extrudedBounds[0][2] = Max( extrudedBounds[0][2], clipZ );
		} else {
			float clipZ = worldBounds[1][2] + modelHeight;
			if ( clipZ - 1.0f < worldBounds[1][2] ) {
				clipZ = worldBounds[1][2] + 1.0f;
			}
			extrudedBounds[1][2] = Min( extrudedBounds[1][2], clipZ );
		}

		idScreenRect screenRect;
		if ( !RB_DimShadowScreenRect( extrudedBounds, screenRect ) ) {
			continue;
		}

		// light space view and orthographic projection over the extruded
		// bounds
		idRenderMatrix lightView;
		RB_DimShadowViewMatrix( casterDirs[i], boundsCenter, lightView );
		idBounds lightSpaceBounds;
		lightSpaceBounds.Clear();
		for ( int c = 0; c < 8; c++ ) {
			const idVec3 corner( ( c & 1 ) ? extrudedBounds[1].x : extrudedBounds[0].x,
				( c & 2 ) ? extrudedBounds[1].y : extrudedBounds[0].y,
				( c & 4 ) ? extrudedBounds[1].z : extrudedBounds[0].z );
			lightSpaceBounds.AddPoint( RB_DimShadowTransformPoint( lightView, corner ) );
		}
		if ( DimShadowBoundsEmpty( lightSpaceBounds ) ) {
			continue;
		}
		idRenderMatrix lightProjection;
		RB_DimShadowProjection( lightSpaceBounds, lightProjection );
		idRenderMatrix viewProjection;
		idRenderMatrix::Multiply( lightProjection, lightView, viewProjection );
		idRenderMatrix inverseViewProjection;
		idRenderMatrix::Inverse( viewProjection, inverseViewProjection );
		idRenderMatrix shadowMatrix;
		idRenderMatrix::Multiply( windowBias, viewProjection, shadowMatrix );

		// depth only render of the caster into the shared target
		GL_BindFramebuffer( dimShadowFBO );
		// render into a centered sub-viewport of the shared target sized to
		// the caster resolution
		const int viewportOffset = ( dimShadowTargetSize - shadowResolution ) >> 1;
		GL_ViewportAndScissor( viewportOffset, viewportOffset, shadowResolution, shadowResolution );
		GL_State( GLS_COLORMASK | GLS_DEPTHMASK | GLS_DEPTHFUNC_LESS );
		GL_Cull( CT_TWO_SIDED );
		GL_ClearDepth( 1.0 );
		GL_Clear( false, true, false, 0, 0, 0, 0, 0 );
		for ( const drawSurf_t * surf = vEntity->drawSurfs; surf != NULL; surf = surf->nextOnLight ) {
			if ( surf->numIndexes == 0 || surf->material == NULL || surf->frontEndGeo == NULL || surf->material->Coverage() == MC_TRANSLUCENT ) {
				continue;
			}
			if ( !vertexCache.CacheIsCurrent( surf->ambientCache ) || !vertexCache.CacheIsCurrent( surf->indexCache ) ) {
				continue;
			}
			idRenderMatrix mvp;
			idRenderMatrix::Multiply( viewProjection, surf->space->modelRenderMatrix, mvp );
			if ( idRenderMatrix::CullBoundsToMVP( mvp, surf->frontEndGeo->bounds, false ) ) {
				continue;
			}
			RB_SetMVP( mvp );

			// alpha tested casters punch holes into the depth target with
			// their alpha test stages, one draw per active stage, the same
			// pattern the Tech 5 shadow map caster pass uses. Without an
			// active alpha stage the surface casts a solid depth shadow.
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

		// fill rate culling around the screen space draw: reject fragments
		// outside the extruded bounds depth range before the fragment shader
		// runs, the same job the light depth bounds test does for the
		// interactions. Projected depth range of the extruded bounds, the
		// same projection RB_DimShadowScreenRect uses.
		bool depthBoundsEnabled = false;
		if ( r_dimShadowDepthBounds.GetBool() && glConfig.depthBoundsTestAvailable ) {
			const idRenderMatrix & viewMvp = backEnd.viewDef->worldSpace.mvp;
			float zmin = 1.0f;
			float zmax = 0.0f;
			for ( int c = 0; c < 8; c++ ) {
				const idVec3 corner( ( c & 1 ) ? extrudedBounds[1].x : extrudedBounds[0].x,
					( c & 2 ) ? extrudedBounds[1].y : extrudedBounds[0].y,
					( c & 4 ) ? extrudedBounds[1].z : extrudedBounds[0].z );
				const float cw = viewMvp[3][0] * corner.x + viewMvp[3][1] * corner.y + viewMvp[3][2] * corner.z + viewMvp[3][3];
				const float w = ( cw >= 0.01f ) ? cw : 0.01f;
				const float ndcZ = ( viewMvp[2][0] * corner.x + viewMvp[2][1] * corner.y + viewMvp[2][2] * corner.z + viewMvp[2][3] ) / w;
				const float depth = ndcZ * 0.5f + 0.5f;
				zmin = Min( zmin, depth );
				zmax = Max( zmax, depth );
			}
			zmin = Max( zmin, 0.0f );
			zmax = Min( zmax, 1.0f );
			if ( zmin < zmax ) {
				GL_DepthBoundsTest( zmin, zmax );
				depthBoundsEnabled = true;
			}
		}

		// blend the shadow factor over the caster screen region, the depth
		// compare in the fragment program does the occlusion work
		GL_BindFramebuffer( previousFramebuffer );
		GL_ViewportAndScissor( viewport.x1 + screenRect.x1, viewport.y1 + screenRect.y1,
			screenRect.x2 - screenRect.x1 + 1, screenRect.y2 - screenRect.y1 + 1 );
		GL_State( GLS_SRCBLEND_SRC_ALPHA | GLS_DSTBLEND_ONE_MINUS_SRC_ALPHA | GLS_DEPTHFUNC_ALWAYS );
		GL_Cull( CT_TWO_SIDED );
		// TNO quality ladder: 2 = high, 1 = mid, 0 = standard, -1 never
		// picks a higher quality program
		// per model high quality selection, a -1 cvar still means never
		int shadowQuality = r_dimShadowHighQuality.GetInteger();
		if ( vEntity->castDimShadows == DIMSHADOW_HIGH_QUALITY && shadowQuality >= 0 ) {
			shadowQuality = 2;
		}
		if ( shadowQuality >= 2 ) {
			renderProgManager.BindShader_DimShadowDrawHighQuality();
		} else if ( shadowQuality == 1 ) {
			renderProgManager.BindShader_DimShadowDrawMidQuality();
		} else {
			renderProgManager.BindShader_DimShadowDraw();
		}
		GL_SelectTexture( 0 );
		dimShadowViewDepthImage->Bind();
		GL_SelectTexture( 1 );
		dimShadowDepthImage->Bind();

		float windowCoord[4];
		windowCoord[0] = 1.0f / viewWidth;
		windowCoord[1] = 1.0f / viewHeight;
		windowCoord[2] = (float)viewport.x1;
		windowCoord[3] = (float)viewport.y1;
		renderProgManager.SetUniformValue( RENDERPARM_WINDOWCOORD, windowCoord );
		// the sample transform rows are stored in the spare shadow parms,
		// they are only used by the interactions while rpShadowParams gates
		// them on and this pass runs outside the interaction passes
		for ( int i = 0; i < 4; i++ ) {
			renderProgManager.SetUniformValue( (renderParm_t)( RENDERPARM_SHADOWMATRIX_X + i ), inverseViewProjection[i] );
			renderProgManager.SetUniformValue( (renderParm_t)( RENDERPARM_SHADOWPROJECTION_X + i ), shadowMatrix[i] );
		}
		// the sample transform rides in the spare shadow parm components,
		// the draw programs remap their target lookup into the caster
		// sub-viewport with them, w holds the target texel size for the
		// PCF taps of the high quality draw
		const float shadowParms[4] = { fade, sampleScale, bias, 1.0f / (float)dimShadowTargetSize };
		renderProgManager.SetUniformValue( RENDERPARM_SHADOWPARAMS, shadowParms );

		RB_SetMVP( identity );
		RB_DrawElementsWithCounters( &backEnd.unitSquareSurface );
		numRendered++;

		if ( depthBoundsEnabled ) {
			GL_DepthBoundsTest( 0.0f, 0.0f );
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
