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

The prime light is approximated with the dominant parallel shadow light of
the view. TNO derives a per model prime direction from its approximate
lighting pass, which this engine does not have yet.

================================================================================================
*/

idCVar r_dimShadows( "r_dimShadows", "1", CVAR_RENDERER | CVAR_BOOL, "TNO style per model dim shadows from the prime light" );
idCVar r_dimShadowResolution( "r_dimShadowResolution", "1024", CVAR_RENDERER | CVAR_INTEGER, "resolution of the shared dim shadow depth target", 64, 2048 );
idCVar r_dimShadowDensity( "r_dimShadowDensity", "0.75", CVAR_RENDERER | CVAR_FLOAT, "maximum darkness of the dim shadows", 0.0f, 1.0f );
idCVar r_dimShadowBias( "r_dimShadowBias", "0.002", CVAR_RENDERER | CVAR_FLOAT, "normalized depth bias that keeps the caster itself lit", 0.0f, 0.1f );
idCVar r_dimShadowMaxRange( "r_dimShadowMaxRange", "2200", CVAR_RENDERER | CVAR_FLOAT, "distance at which dim shadows are fully faded out", 0.0f, 10000.0f );
idCVar r_dimShadowFadeStart( "r_dimShadowFadeStart", "1800", CVAR_RENDERER | CVAR_FLOAT, "distance at which dim shadows start to fade", 0.0f, 10000.0f );
idCVar r_dimShadowExtrude( "r_dimShadowExtrude", "1000", CVAR_RENDERER | CVAR_FLOAT, "how far caster bounds are extruded along the light direction", 16.0f, 8192.0f );
idCVar r_dimShadowMaxModels( "r_dimShadowMaxModels", "8", CVAR_RENDERER | CVAR_INTEGER, "maximum number of dim shadow casters per view", 0, 32 );
idCVar r_dimShadowMaxModelSize( "r_dimShadowMaxModelSize", "600", CVAR_RENDERER | CVAR_FLOAT, "skip casters whose bounds diagonal is larger than this", 0.0f, 100000.0f );

static const int MAX_DIM_SHADOW_CASTERS = 32;

static idImage * dimShadowDepthImage = NULL;
static idImage * dimShadowViewDepthImage = NULL;
static GLuint dimShadowFBO = 0;
static GLuint dimShadowTextureObject = 0;
static int dimShadowTargetSize = 0;

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
	rect.x1 = Max( rect.x1, 0 );
	rect.y1 = Max( rect.y1, 0 );
	rect.x2 = Min( rect.x2, (int)viewWidth );
	rect.y2 = Min( rect.y2, (int)viewHeight );
	return !rect.IsEmpty();
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

	// the prime light is the dominant parallel shadow light of the view
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
	if ( primeLight == NULL ) {
		return;
	}

	// direction the prime light shines, same convention as the shadow map
	// view which looks along -shadowLightCenter
	idVec3 primeDir = -primeLight->shadowLightCenter;
	if ( primeDir.Normalize() == 0.0f ) {
		return;
	}

	// collect the casters first so the view depth copy only happens when a
	// caster is actually present
	const viewEntity_t * casters[MAX_DIM_SHADOW_CASTERS];
	idBounds casterBounds[MAX_DIM_SHADOW_CASTERS];
	int numCasters = 0;
	for ( const viewEntity_t * vEntity = backEnd.viewDef->viewEntitys; vEntity != NULL && numCasters < MAX_DIM_SHADOW_CASTERS; vEntity = vEntity->next ) {
		if ( vEntity->isGuiSurface || vEntity->weaponDepthHack ) {
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
		if ( numSurfs == 0 || worldBounds.IsEmpty() ) {
			continue;
		}
		// the world model and other very large geometry are not dim shadow
		// casters, TNO gates them out with entity flags this engine lacks
		if ( ( worldBounds[1] - worldBounds[0] ).Length() > r_dimShadowMaxModelSize.GetFloat() ) {
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
	const idVec3 extrude = primeDir * r_dimShadowExtrude.GetFloat();

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
		if ( fade <= 0.0f ) {
			continue;
		}

		// extrude the caster bounds along the light travel direction
		idBounds extrudedBounds;
		for ( int a = 0; a < 3; a++ ) {
			extrudedBounds[0][a] = Min( worldBounds[0][a], worldBounds[0][a] + extrude[a] ) - 1.0f;
			extrudedBounds[1][a] = Max( worldBounds[1][a], worldBounds[1][a] + extrude[a] ) + 1.0f;
		}

		idScreenRect screenRect;
		if ( !RB_DimShadowScreenRect( extrudedBounds, screenRect ) ) {
			continue;
		}

		// light space view and orthographic projection over the extruded
		// bounds
		idRenderMatrix lightView;
		RB_DimShadowViewMatrix( primeDir, boundsCenter, lightView );
		idBounds lightSpaceBounds;
		lightSpaceBounds.Clear();
		for ( int c = 0; c < 8; c++ ) {
			const idVec3 corner( ( c & 1 ) ? extrudedBounds[1].x : extrudedBounds[0].x,
				( c & 2 ) ? extrudedBounds[1].y : extrudedBounds[0].y,
				( c & 4 ) ? extrudedBounds[1].z : extrudedBounds[0].z );
			lightSpaceBounds.AddPoint( RB_DimShadowTransformPoint( lightView, corner ) );
		}
		if ( lightSpaceBounds.IsEmpty() ) {
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
		GL_ViewportAndScissor( 0, 0, dimShadowTargetSize, dimShadowTargetSize );
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
			if ( surf->jointCache ) {
				renderProgManager.BindShader_DepthSkinned();
			} else {
				renderProgManager.BindShader_Depth();
			}
			RB_DrawElementsWithCounters( surf );
		}

		// blend the shadow factor over the caster screen region, the depth
		// compare in the fragment program does the occlusion work
		GL_BindFramebuffer( previousFramebuffer );
		GL_ViewportAndScissor( viewport.x1 + screenRect.x1, viewport.y1 + screenRect.y1,
			screenRect.x2 - screenRect.x1 + 1, screenRect.y2 - screenRect.y1 + 1 );
		GL_State( GLS_SRCBLEND_SRC_ALPHA | GLS_DSTBLEND_ONE_MINUS_SRC_ALPHA | GLS_DEPTHFUNC_ALWAYS );
		GL_Cull( CT_TWO_SIDED );
		renderProgManager.BindShader_DimShadowDraw();
		GL_SelectTexture( 0 );
		dimShadowViewDepthImage->Bind();
		GL_SelectTexture( 1 );
		dimShadowDepthImage->Bind();

		float windowCoord[4];
		windowCoord[0] = 1.0f / viewWidth;
		windowCoord[1] = 1.0f / viewHeight;
		windowCoord[2] = (float)viewport.x1;
		windowCoord[3] = (float)viewport.y1;
		SetFragmentParm( RENDERPARM_WINDOWCOORD, windowCoord );
		// the sample transform rows are stored in the spare shadow parms,
		// they are only used by the interactions while rpShadowParams gates
		// them on and this pass runs outside the interaction passes
		SetVertexParms( RENDERPARM_SHADOWMATRIX_X, inverseViewProjection.m, 4 );
		SetVertexParms( RENDERPARM_SHADOWPROJECTION_X, shadowMatrix.m, 4 );
		const float shadowParms[4] = { fade, 0.0f, bias, 0.0f };
		SetFragmentParm( RENDERPARM_SHADOWPARAMS, shadowParms );

		RB_SetMVP( identity );
		RB_DrawElementsWithCounters( &backEnd.unitSquareSurface );
		numRendered++;
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
