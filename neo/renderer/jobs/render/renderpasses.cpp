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
#include "../../virtualtexture.h"

idCVar r_drawEyeColor( "r_drawEyeColor", "0", CVAR_RENDERER | CVAR_BOOL, "Draw a colored box, red = left eye, blue = right eye, grey = non-stereo" );
idCVar r_motionBlur( "r_motionBlur", "0", CVAR_RENDERER | CVAR_INTEGER | CVAR_ARCHIVE, "1 - 5, log2 of the number of motion blur samples" );
idCVar r_skipShaderPasses( "r_skipShaderPasses", "0", CVAR_RENDERER | CVAR_BOOL, "" );
idCVar r_skipInteractionFastPath( "r_skipInteractionFastPath", "1", CVAR_RENDERER | CVAR_BOOL, "" );
idCVar r_shadowMapEnable( "r_shadowMapEnable", "1", CVAR_RENDERER | CVAR_ARCHIVE | CVAR_BOOL, "enable Tech 5 style depth-mapped shadows" );
idCVar r_shadowMapSize( "r_shadowMapSize", "1024", CVAR_RENDERER | CVAR_ARCHIVE | CVAR_INTEGER, "Tech 5 shadow atlas size", 256, 4096 );
idCVar r_shadowMapBias( "r_shadowMapBias", "0.0015", CVAR_RENDERER | CVAR_ARCHIVE | CVAR_FLOAT, "depth compare bias for shadow maps", 0.0f, 0.05f );
idCVar r_shadowMapParallelExtent( "r_shadowMapParallelExtent", "256", CVAR_RENDERER | CVAR_ARCHIVE | CVAR_FLOAT, "half-width of the view-centered parallel-light shadow map (Tech5 default coverage)", 64.0f, 8192.0f );
idCVar r_shadowMapDebug( "r_shadowMapDebug", "0", CVAR_RENDERER | CVAR_BOOL, "print depth-atlas coverage for each shadowed light" );
idCVar r_shadowMapSample( "r_shadowMapSample", "1", CVAR_RENDERER | CVAR_BOOL, "apply the shadow atlas while retaining atlas generation" );
idCVar r_shadowMapDrawCasters( "r_shadowMapDrawCasters", "1", CVAR_RENDERER | CVAR_BOOL, "draw shadow casters into the depth atlas" );
idCVar r_shadowMapMaxLights( "r_shadowMapMaxLights", "8", CVAR_RENDERER | CVAR_ARCHIVE | CVAR_INTEGER, "maximum shadow-mapped lights rendered per view", 0, 64 );
idCVar r_sb_occluderFacing( "r_sb_occluderFacing", "0", CVAR_RENDERER | CVAR_INTEGER, "shadow caster facing: 0 = front faces, 1 = back faces, 2 = back faces with doubled offset followed by front faces", 0, 2 );
idCVar r_sb_singleSide( "r_sb_singleSide", "-1", CVAR_RENDERER | CVAR_INTEGER, "only draw a single face (0-5) of point light shadow maps, -1 = all faces", -1, 5 );
idCVar r_sb_polyOfsFactor( "r_sb_polyOfsFactor", "2", CVAR_RENDERER | CVAR_FLOAT, "polygon offset factor for point and spot light shadow casters" );
idCVar r_sb_polyOfsUnits( "r_sb_polyOfsUnits", "2000", CVAR_RENDERER | CVAR_FLOAT, "polygon offset units for point and spot light shadow casters" );
idCVar r_sb_parallelPolyOfsFactor( "r_sb_parallelPolyOfsFactor", "2", CVAR_RENDERER | CVAR_FLOAT, "polygon offset factor for parallel (sun) light shadow casters" );
idCVar r_sb_parallelPolyOfsUnits( "r_sb_parallelPolyOfsUnits", "2000", CVAR_RENDERER | CVAR_FLOAT, "polygon offset units for parallel (sun) light shadow casters" );
idCVar r_sb_maxCasters( "r_sb_maxCasters", "256", CVAR_RENDERER | CVAR_INTEGER, "maximum number of shadow casters drawn per light face", 16, 1024 );
idCVar r_sb_sliceStep( "r_sb_sliceStep", "512", CVAR_RENDERER | CVAR_ARCHIVE | CVAR_FLOAT, "initial ortho half-extent of the first parallel light shadow slice", 64.0f, 4096.0f );
idCVar r_sb_sliceRampScale( "r_sb_sliceRampScale", "2", CVAR_RENDERER | CVAR_FLOAT, "larger values perform a faster lerp between parallel light shadow slices", 1.0f, 8.0f );
idCVar r_occlusionQuery( "r_occlusionQuery", "1", CVAR_RENDERER | CVAR_BOOL, "TNO style GPU occlusion culling of world models" );

// TNO style GPU occlusion culling state, shared with the front end portal walk
occlusionQueryState_t	occlusionQueryState;

extern idCVar stereoRender_swapEyes;

backEndState_t	backEnd;

/*
================
SetVertexParm
================
*/
static ID_INLINE void SetVertexParm( renderParm_t rp, const float * value ) {
	renderProgManager.SetUniformValue( rp, value );
}

/*
================
SetVertexParms
================
*/
static ID_INLINE void SetVertexParms( renderParm_t rp, const float * value, int num ) {
	for ( int i = 0; i < num; i++ ) {
		renderProgManager.SetUniformValue( (renderParm_t)( rp + i ), value + ( i * 4 ) );
	}
}

/*
================
SetFragmentParm
================
*/
static ID_INLINE void SetFragmentParm( renderParm_t rp, const float * value ) {
	renderProgManager.SetUniformValue( rp, value );
}

/*
================
RB_SetMVP
================
*/
void RB_SetMVP( const idRenderMatrix & mvp ) { 
	SetVertexParms( RENDERPARM_MVPMATRIX_X, mvp[0], 4 );
}

/*
================
RB_SetMVPWithStereoOffset
================
*/
static void RB_SetMVPWithStereoOffset( const idRenderMatrix & mvp, const float stereoOffset ) { 
	idRenderMatrix offset = mvp;
	offset[0][3] += stereoOffset;

	SetVertexParms( RENDERPARM_MVPMATRIX_X, offset[0], 4 );
}

static const float zero[4] = { 0, 0, 0, 0 };
static const float one[4] = { 1, 1, 1, 1 };
static const float negOne[4] = { -1, -1, -1, -1 };

static idImage * lightShadowAtlas = NULL;
static GLuint lightShadowFBO = 0;
static GLuint lightShadowTextureObject = 0;
static int lightShadowAtlasSize = 0;
static idRenderMatrix currentShadowMatrix;
static idRenderMatrix currentShadowProjection;
static float currentShadowType = 0.0f;

static void RB_SetVertexColorParms( stageVertexColor_t svc );
static void RB_PrepareStageTexturing( const shaderStage_t * pStage, const drawSurf_t * surf );
static void RB_FinishStageTexturing( const shaderStage_t * pStage, const drawSurf_t * surf );

void RB_ShutdownShadowAtlas() {
	if ( lightShadowFBO != 0 ) {
		GL_DeleteFramebuffers( 1, &lightShadowFBO );
		lightShadowFBO = 0;
	}
	lightShadowTextureObject = 0;
	lightShadowAtlas = NULL;
	lightShadowAtlasSize = 0;
	currentShadowType = 0.0f;
}

static void RB_GetPointLightViewMatrix( int side, idRenderMatrix & lightView ) {
	idRenderMatrix base;
	base.Zero();
	base.m[15] = 1.0f;
	switch ( side ) {
		case 0: base.m[0] = 1.0f; base.m[6] = -1.0f; base.m[9] = 1.0f; break;
		case 1: base.m[0] = -1.0f; base.m[6] = -1.0f; base.m[9] = 1.0f; break;
		case 2: base.m[1] = 1.0f; base.m[4] = -1.0f; base.m[10] = 1.0f; break;
		case 3: base.m[1] = -1.0f; base.m[4] = -1.0f; base.m[10] = 1.0f; break;
		case 4: base.m[2] = 1.0f; base.m[4] = -1.0f; base.m[9] = 1.0f; break;
		case 5: base.m[2] = -1.0f; base.m[4] = -1.0f; base.m[9] = 1.0f; break;
	}
	const idRenderMatrix flip( 0, -1, 0, 0, 0, 0, 1, 0, -1, 0, 0, 0, 0, 0, 0, 1 );
	idRenderMatrix::Multiply( flip, base, lightView );
}

static void RB_GetSpotProjection( float zNear, float tanFovX, float tanFovY, idRenderMatrix & projection ) {
	projection.Zero();
	projection.m[0] = 1.0f / tanFovX;
	projection.m[5] = 1.0f / tanFovY;
	projection.m[10] = -1.0f;
	projection.m[11] = -2.0f * zNear;
	projection.m[14] = -1.0f;
}

static void RB_GetParallelLightViewMatrix( const idVec3 & direction, idRenderMatrix & lightView ) {
	idVec3 normalized = direction;
	if ( normalized.Normalize() == 0.0f ) {
		normalized.Set( 0.0f, 0.0f, -1.0f );
	}
	float referenceY = 0.0f;
	float referenceZ = -1.0f;
	float projection = -normalized.z;
	if ( idMath::Fabs( projection ) > 0.99f ) {
		referenceY = 1.0f;
		referenceZ = 1.0f;
		projection = normalized.y + normalized.z;
	}
	idVec3 vertical( -normalized.x * projection, referenceY - normalized.y * projection, referenceZ - normalized.z * projection );
	vertical.Normalize();
	const idVec3 horizontal = normalized.Cross( vertical );
	const idRenderMatrix base( normalized.x, normalized.y, normalized.z, 0.0f,
		horizontal.x, horizontal.y, horizontal.z, 0.0f,
		vertical.x, vertical.y, vertical.z, 0.0f,
		0.0f, 0.0f, 0.0f, 1.0f );
	const idRenderMatrix flip( 0, -1, 0, 0, 0, 0, 1, 0, -1, 0, 0, 0, 0, 0, 0, 1 );
	idRenderMatrix::Multiply( flip, base, lightView );
}

static void RB_WorldToShadowMap( const idRenderMatrix & projection, float scale, float sBias, float tBias, idRenderMatrix & result ) {
	for ( int i = 0; i < 4; i++ ) {
		result.m[i] = scale * projection.m[i] + sBias * projection.m[12 + i];
		result.m[4 + i] = scale * projection.m[4 + i] + tBias * projection.m[12 + i];
		result.m[8 + i] = 0.5f * ( projection.m[8 + i] + projection.m[12 + i] );
		result.m[12 + i] = projection.m[12 + i];
	}
}

static bool RB_EnsureShadowAtlas() {
	const int size = r_shadowMapSize.GetInteger();
	const bool textureValid = lightShadowAtlas != NULL && lightShadowAtlas->IsLoaded() && lightShadowAtlas->GetAPIObject() != 0 &&
		lightShadowAtlas->GetUploadWidth() == size && lightShadowAtlas->GetUploadHeight() == size;
	if ( textureValid && lightShadowAtlasSize == size && lightShadowFBO != 0 &&
		lightShadowTextureObject == lightShadowAtlas->GetAPIObject() ) {
		lightShadowAtlas->SetReferencedOutsideLevelLoad();
		return true;
	}
	if ( lightShadowFBO != 0 ) {
		GL_DeleteFramebuffers( 1, &lightShadowFBO );
		lightShadowFBO = 0;
	}
	lightShadowTextureObject = 0;
	idImageOpts opts;
	opts.textureType = TT_2D;
	opts.format = FMT_DEPTH;
	opts.width = size;
	opts.height = size;
	opts.numLevels = 1;
	lightShadowAtlas = globalImages->ScratchImage( "_lightShadowAtlas", &opts, TF_LINEAR, TR_CLAMP, TD_DEPTH );
	if ( lightShadowAtlas == NULL || !lightShadowAtlas->IsLoaded() ) {
		common->Warning( "could not allocate the light shadow atlas" );
		lightShadowAtlasSize = 0;
		return false;
	}
	lightShadowAtlas->SetReferencedOutsideLevelLoad();
	lightShadowAtlasSize = size;
	GL_SelectTexture( 5 );
	lightShadowAtlas->Bind();
	// idImage::Bind uses glBindMultiTextureEXT and GL_SelectTexture only updates
	// the engine's DSA bookkeeping.  Legacy glTexParameteri operates on the
	// actual active unit, so select unit 5 explicitly or these depth sampler
	// parameters corrupt whichever ordinary color image is active on unit 0.
	GLint previousActiveTexture = GL_TEXTURE0;
	GL_GetIntegerv( GET_ACTIVE_TEXTURE, &previousActiveTexture );
	GL_ActiveTexture( 5 );
	GL_BindTexture( TEX_TARGET_2D, lightShadowAtlas->GetAPIObject() );
	GL_TexParameteri( TEX_TARGET_2D, TEXPARAM_COMPARE_MODE, TEX_COMPARE_REF_TO_TEXTURE );
	GL_TexParameteri( TEX_TARGET_2D, TEXPARAM_COMPARE_FUNC, TEX_COMPARE_LEQUAL );
	GL_ActiveTexture( previousActiveTexture );
	GL_GenFramebuffers( 1, &lightShadowFBO );
	GLint previousFramebuffer = 0;
	GLint previousDrawBuffer = GL_BACK;
	GLint previousReadBuffer = GL_BACK;
	GL_GetIntegerv( GET_FRAMEBUFFER_BINDING, &previousFramebuffer );
	GL_GetIntegerv( GET_DRAW_BUFFER, &previousDrawBuffer );
	GL_GetIntegerv( GET_READ_BUFFER, &previousReadBuffer );
	GL_BindFramebuffer( lightShadowFBO );
	GL_FramebufferTexture2D( ATTACHMENT_DEPTH, TEX_TARGET_2D, lightShadowAtlas->GetAPIObject(), 0 );
	GL_DrawBuffer( BUFFER_SELECT_NONE );
	GL_ReadBuffer( BUFFER_SELECT_NONE );
	const bool status = GL_CheckFramebufferStatus();
	GL_BindFramebuffer( previousFramebuffer );
	GL_DrawBuffer( previousDrawBuffer );
	GL_ReadBuffer( previousReadBuffer );
	if ( !status ) {
		common->Warning( "light shadow framebuffer incomplete" );
		GL_DeleteFramebuffers( 1, &lightShadowFBO );
		lightShadowFBO = 0;
		lightShadowTextureObject = 0;
		return false;
	}
	lightShadowTextureObject = lightShadowAtlas->GetAPIObject();
	return true;
}

// Transform a model-space point to world space with the entity render matrix.
static ID_INLINE idVec3 RB_TransformPoint( const idRenderMatrix & m, const idVec3 & p ) {
	return idVec3(
		p.x * m[0][0] + p.y * m[1][0] + p.z * m[2][0] + m[3][0],
		p.x * m[0][1] + p.y * m[1][1] + p.z * m[2][1] + m[3][1],
		p.x * m[0][2] + p.y * m[1][2] + p.z * m[2][2] + m[3][2] );
}

static void RB_DrawShadowCasters( const drawSurf_t * surfs, const idRenderMatrix & shadowViewProjection, const idVec3 & sortOrigin, const bool parallelLight ) {
	// Match Tech 5's bounded shadow-caster work list.  Besides bounding worst-
	// case cost, this prevents a malformed light chain from wedging the backend.
	static const int MAX_SHADOW_CASTERS = 256;
	const int maxCasters = Min( r_sb_maxCasters.GetInteger(), MAX_SHADOW_CASTERS );

	// Tech5's RB_RenderShadowBuffer sorts the caster chain so the nearest
	// (most important) occluders land in the map when the work list
	// overflows.  The chains are short, so a bounded insertion sort over a
	// scratch list is cheaper than maintaining a second chain.
	drawSurf_t * sorted[MAX_SHADOW_CASTERS];
	float dists[MAX_SHADOW_CASTERS];
	int numSorted = 0;
	for ( drawSurf_t * surf = const_cast<drawSurf_t *>( surfs ); surf != NULL && numSorted < MAX_SHADOW_CASTERS; surf = surf->nextOnLight ) {
		if ( surf->numIndexes == 0 || !vertexCache.CacheIsCurrent( surf->ambientCache ) || !vertexCache.CacheIsCurrent( surf->indexCache ) ) {
			continue;
		}
		if ( surf->frontEndGeo != NULL && surf->space != NULL ) {
			const idVec3 center = RB_TransformPoint( surf->space->modelRenderMatrix, ( surf->frontEndGeo->bounds[0] + surf->frontEndGeo->bounds[1] ) * 0.5f );
			dists[numSorted] = ( center - sortOrigin ).Length();
		} else {
			dists[numSorted] = 0.0f;
		}
		sorted[numSorted++] = surf;
	}
	// descending: nearest casters first
	for ( int i = 1; i < numSorted; i++ ) {
		drawSurf_t * sortSurf = sorted[i];
		const float sortDist = dists[i];
		int j = i - 1;
		for ( ; j >= 0 && dists[j] < sortDist; j-- ) {
			sorted[j + 1] = sorted[j];
			dists[j + 1] = dists[j];
		}
		sorted[j + 1] = sortSurf;
		dists[j + 1] = sortDist;
	}

	// Tech5 keeps separate polygon offsets for the sun/parallel shadow map
	// and the point and spot shadow maps.
	const float polyOfsFactor = parallelLight ? r_sb_parallelPolyOfsFactor.GetFloat() : r_sb_polyOfsFactor.GetFloat();
	const float polyOfsUnits = parallelLight ? r_sb_parallelPolyOfsUnits.GetFloat() : r_sb_polyOfsUnits.GetFloat();
	const int occluderFacing = r_sb_occluderFacing.GetInteger();

	int numDrawn = 0;
	for ( int i = 0; i < numSorted && numDrawn < maxCasters; i++ ) {
		const drawSurf_t * surf = sorted[i];
		const idMaterial * shader = surf->material;
		idRenderMatrix mvp;
		idRenderMatrix::Multiply( shadowViewProjection, surf->space->modelRenderMatrix, mvp );
		// A surface linked to a point light usually belongs to only one cube
		// face.  Cull it against this side before touching GPU state, matching
		// the Tech 5 shadow job rather than redrawing every caster six times.
		if ( surf->frontEndGeo != NULL && idRenderMatrix::CullBoundsToMVP( mvp, surf->frontEndGeo->bounds, false ) ) {
			continue;
		}
		RB_SetMVP( mvp );

		if ( shader->Coverage() == MC_PERFORATED ) {
			const float * regs = surf->shaderRegisters;
			bool drewAlphaStage = false;
			for ( int stage = 0; stage < shader->GetNumStages(); stage++ ) {
				const shaderStage_t * pStage = shader->GetStage( stage );
				if ( !pStage->hasAlphaTest || regs[pStage->conditionRegister] == 0.0f || regs[pStage->color.registers[3]] <= 0.0f ) {
					continue;
				}
				drewAlphaStage = true;
				GL_State( GLS_COLORMASK | GLS_POLYGON_OFFSET | GLS_DEPTHFUNC_LESS );
				GL_PolygonOffset( polyOfsFactor, polyOfsUnits );
				idVec4 alphaTestValue( regs[pStage->alphaTestRegister] );
				SetFragmentParm( RENDERPARM_ALPHA_TEST, alphaTestValue.ToFloatPtr() );
				if ( surf->jointCache ) {
					renderProgManager.BindShader_TextureVertexColorSkinned();
				} else {
					renderProgManager.BindShader_TextureVertexColor();
				}
				RB_SetVertexColorParms( SVC_IGNORE );
				GL_SelectTexture( 0 );
				pStage->texture.image->Bind();
				RB_PrepareStageTexturing( pStage, surf );
				RB_DrawElementsWithCounters( surf );
				backEnd.pc.c_shadowElements++;
				backEnd.pc.c_shadowIndexes += surf->numIndexes;
				RB_FinishStageTexturing( pStage, surf );
			}
			SetFragmentParm( RENDERPARM_ALPHA_TEST, vec4_zero.ToFloatPtr() );
			if ( drewAlphaStage ) {
				numDrawn++;
				continue;
			}
		}

		// Tech5's occluder facing options: front faces by default, back faces
		// for cheaper silhouettes, or back faces pushed farther out followed
		// by front faces so the two depth layers average out without leaking
		// at silhouette edges.
		if ( occluderFacing == 1 || occluderFacing == 2 ) {
			if ( surf->jointCache ) {
				renderProgManager.BindShader_DepthSkinned();
			} else {
				renderProgManager.BindShader_Depth();
			}
			GL_Cull( CT_BACK_SIDED );
			GL_PolygonOffset( polyOfsFactor, polyOfsUnits * 2.0f );
			RB_DrawElementsWithCounters( surf );
			backEnd.pc.c_shadowElements++;
			backEnd.pc.c_shadowIndexes += surf->numIndexes;
			numDrawn++;
			if ( occluderFacing == 1 ) {
				continue;
			}
		}
		if ( surf->jointCache ) {
			renderProgManager.BindShader_DepthSkinned();
		} else {
			renderProgManager.BindShader_Depth();
		}
		GL_Cull( CT_FRONT_SIDED );
		GL_PolygonOffset( polyOfsFactor, polyOfsUnits );
		RB_DrawElementsWithCounters( surf );
		backEnd.pc.c_shadowElements++;
		backEnd.pc.c_shadowIndexes += surf->numIndexes;
		numDrawn++;
	}
}

static bool RB_RenderShadowMap( const viewLight_t * vLight ) {
	if ( !r_shadowMapEnable.GetBool() || vLight->lightShader->IsAmbientLight() || ( vLight->globalShadows == NULL && vLight->localShadows == NULL ) || !RB_EnsureShadowAtlas() ) {
		currentShadowType = 0.0f;
		return false;
	}
	idRenderMatrix lightTransform;
	idRenderMatrix inverseLightTransform;
	idRenderMatrix::CreateFromOriginAxis( vLight->shadowLightOrigin, vLight->shadowLightAxis, lightTransform );
	idRenderMatrix::Inverse( lightTransform, inverseLightTransform );

	idRenderMatrix shadowMVP[6];
	int numSides = 1;
	int sideStart = 0;
	int tileSize = lightShadowAtlasSize;
	if ( vLight->shadowParallel ) {
		idRenderMatrix viewTransform;
		idRenderMatrix viewInverse;
		idRenderMatrix::CreateFromOriginAxis( vLight->shadowViewOrigin, vLight->shadowLightAxis, viewTransform );
		idRenderMatrix::Inverse( viewTransform, viewInverse );
		idRenderMatrix parallelView;
		RB_GetParallelLightViewMatrix( -vLight->shadowLightCenter, parallelView );
		idRenderMatrix baseProjection;
		idRenderMatrix::Multiply( parallelView, viewInverse, baseProjection );
		// Tech5 slice cascade: slice 0 covers the base extent, each wider
		// slice doubles it.  The matrix handed to the shader covers the
		// largest extent and maps it to [-2, 2] texture coordinates; the
		// shader folds each slice into its own atlas quadrant.
		const float slice0Extent = Max( r_sb_sliceStep.GetFloat() * 0.35355338f, r_shadowMapParallelExtent.GetFloat() );
		const float snapStep = 64.0f * slice0Extent / static_cast<float>( lightShadowAtlasSize );
		baseProjection.m[3] = idMath::Floor( baseProjection.m[3] / snapStep ) * snapStep;
		baseProjection.m[7] = idMath::Floor( baseProjection.m[7] / snapStep ) * snapStep;
		baseProjection.m[11] = idMath::Floor( baseProjection.m[11] / snapStep ) * snapStep;
		float depthMin;
		float depthMax;
		idRenderMatrix::DepthBoundsForBounds( depthMin, depthMax, baseProjection, vLight->shadowLightBounds, false );
		if ( depthMax - depthMin < 1.0f ) {
			depthMax = depthMin + 1.0f;
		}
		const float depthScale = -2.0f / ( depthMax - depthMin );
		const float depthBias = ( depthMin + depthMax ) / ( depthMax - depthMin );
		for ( int slice = 0; slice < 4; slice++ ) {
			const float sliceScale = 1.0f / ( slice0Extent * static_cast<float>( 1 << slice ) );
			const idRenderMatrix projection( sliceScale, 0, 0, 0,
				0, -sliceScale, 0, 0,
				0, 0, depthScale, depthBias,
				0, 0, 0, 1 );
			idRenderMatrix::Multiply( projection, baseProjection, shadowMVP[slice] );
		}
		// the shader unprojects against the largest slice and folds each
		// slice into its quadrant, so hand it the largest extent matrix
		RB_WorldToShadowMap( shadowMVP[3], 2.0f, 0.0f, 0.0f, currentShadowMatrix );
		currentShadowProjection.Identity();
		// raw light-view depth row for the shader's slice LOD; the vertex
		// shader negates it because view space depth is negative forward
		for ( int i = 0; i < 4; i++ ) {
			currentShadowProjection.m[8 + i] = baseProjection.m[8 + i];
		}
		// rpParallelLightSliceScale = ( 1 / slice0 extent, log2 scale, 1 / log2 scale, ramp )
		const float logarithmicScale = 0.69314718f * 1.442695f;
		const idVec4 sliceParms( 1.0f / slice0Extent, logarithmicScale, 1.0f / logarithmicScale, r_sb_sliceRampScale.GetFloat() );
		SetFragmentParm( RENDERPARM_PARALLEL_LIGHT_SLICE_SCALE, sliceParms.ToFloatPtr() );
		currentShadowType = 1.5f;
	} else if ( vLight->shadowPointLight ) {
		idRenderMatrix centerTranslation;
		centerTranslation.Identity();
		centerTranslation.m[3] = -vLight->shadowLightCenter.x;
		centerTranslation.m[7] = -vLight->shadowLightCenter.y;
		centerTranslation.m[11] = -vLight->shadowLightCenter.z;
		idRenderMatrix::Multiply( centerTranslation, inverseLightTransform, currentShadowMatrix );
		idRenderMatrix projection;
		RB_GetSpotProjection( 8.0f, idMath::Tan( idMath::M_DEG2RAD * 46.5f ), idMath::Tan( idMath::M_DEG2RAD * 46.5f ), projection );
		for ( int side = 0; side < 6; side++ ) {
			idRenderMatrix sideView;
			idRenderMatrix sideTransform;
			RB_GetPointLightViewMatrix( side, sideView );
			idRenderMatrix::Multiply( sideView, currentShadowMatrix, sideTransform );
			idRenderMatrix::Multiply( projection, sideTransform, shadowMVP[side] );
		}
		RB_WorldToShadowMap( projection, 0.5f, 0.0f, 0.0f, currentShadowProjection );
		currentShadowType = 2.0f;
		// Tech5's r_sb_singleSide limits point light shadow rendering to a
		// single cube face for debugging and tuning.
		if ( r_sb_singleSide.GetInteger() >= 0 && r_sb_singleSide.GetInteger() < 6 ) {
			sideStart = r_sb_singleSide.GetInteger();
			numSides = 1;
		} else {
			numSides = 6;
		}
		tileSize = lightShadowAtlasSize / 3;
	} else {
		const float targetLength = idMath::Sqrt( Max( vLight->shadowLightTarget.LengthSqr(), idMath::FLT_SMALLEST_NON_DENORMAL ) );
		const float rightLength = idMath::Sqrt( Max( vLight->shadowLightRight.LengthSqr(), idMath::FLT_SMALLEST_NON_DENORMAL ) );
		const float upLength = idMath::Sqrt( Max( vLight->shadowLightUp.LengthSqr(), idMath::FLT_SMALLEST_NON_DENORMAL ) );
		const idVec3 target = vLight->shadowLightTarget / targetLength;
		const float zNear = Max( target * vLight->shadowLightStart, 8.0f );
		idRenderMatrix spotBase( target.x, target.y, target.z, 0, -vLight->shadowLightRight.x / rightLength, -vLight->shadowLightRight.y / rightLength, -vLight->shadowLightRight.z / rightLength, 0, vLight->shadowLightUp.x / upLength, vLight->shadowLightUp.y / upLength, vLight->shadowLightUp.z / upLength, 0, 0, 0, 0, 1 );
		const idRenderMatrix flip( 0, -1, 0, 0, 0, 0, 1, 0, -1, 0, 0, 0, 0, 0, 0, 1 );
		idRenderMatrix spotView;
		idRenderMatrix lightView;
		idRenderMatrix::Multiply( flip, spotBase, spotView );
		idRenderMatrix::Multiply( spotView, inverseLightTransform, lightView );
		idRenderMatrix projection;
		RB_GetSpotProjection( zNear, rightLength * 1.05f / targetLength, upLength * 1.05f / targetLength, projection );
		idRenderMatrix::Multiply( projection, lightView, shadowMVP[0] );
		RB_WorldToShadowMap( shadowMVP[0], 0.5f, 0.5f, 0.5f, currentShadowMatrix );
		currentShadowProjection.Identity();
		currentShadowType = 1.0f;
	}

	GLint previousFramebuffer = 0;
	GLint previousViewport[4];
	GLint previousScissor[4];
	GLint previousDrawBuffer = GL_BACK;
	GLint previousReadBuffer = GL_BACK;
	const int previousCullType = backEnd.glState.faceCulling;
	const uint64 previousState = GL_GetCurrentState();
	GL_GetIntegerv( GET_FRAMEBUFFER_BINDING, &previousFramebuffer );
	GL_GetIntegerv( GET_VIEWPORT, previousViewport );
	GL_GetIntegerv( GET_SCISSOR_BOX, previousScissor );
	GL_GetIntegerv( GET_DRAW_BUFFER, &previousDrawBuffer );
	GL_GetIntegerv( GET_READ_BUFFER, &previousReadBuffer );
	GL_BindFramebuffer( lightShadowFBO );
	// glClear obeys the scissor test.  The previous light's screen-space
	// scissor and depth write mask.  Interaction rendering leaves depth writes
	// disabled, so establish the shadow depth state before clearing or the
	// shared atlas retains depths from the previous light.
	GL_ViewportAndScissor( 0, 0, lightShadowAtlasSize, lightShadowAtlasSize );
	if ( r_shadowMapDrawCasters.GetBool() ) {
		GL_State( GLS_COLORMASK | GLS_ALPHAMASK | GLS_POLYGON_OFFSET | GLS_DEPTHFUNC_LESS );
		GL_Cull( CT_TWO_SIDED );
		GL_PolygonOffset( 1.0f, 2.0f );
		GL_ClearDepth( 1.0 );
		GL_Clear( false, true, false, 0, 0, 0, 0, 0 );
	}
	static const int slotX[6] = { 0, 1, 0, 1, 0, 1 };
	static const int slotY[6] = { 0, 0, 1, 1, 2, 2 };
	if ( r_shadowMapDrawCasters.GetBool() ) {
		const idVec3 & sortOrigin = vLight->shadowParallel ? vLight->shadowViewOrigin : vLight->shadowLightOrigin;
		for ( int side = 0; side < numSides; side++ ) {
			int x = 0;
			int y = 0;
			int matrixIndex = 0;
			if ( vLight->shadowPointLight ) {
				const int face = sideStart + side;
				x = slotX[face] * tileSize;
				y = slotY[face] * tileSize;
				matrixIndex = face;
			} else if ( vLight->shadowParallel ) {
				// slice 0 top-left, wider slices outward, matching the
				// shader's quadrant bias math
				x = ( side & 1 ) * tileSize;
				y = ( side >> 1 ) * tileSize;
				matrixIndex = side;
			}
			GL_ViewportAndScissor( x, y, tileSize, tileSize );
			RB_DrawShadowCasters( vLight->globalShadows, shadowMVP[matrixIndex], sortOrigin, vLight->shadowParallel );
			RB_DrawShadowCasters( vLight->localShadows, shadowMVP[matrixIndex], sortOrigin, vLight->shadowParallel );
		}
	}
	static bool reportedShadowAtlas = false;
	if ( r_shadowMapDebug.GetBool() && !reportedShadowAtlas ) {
		idTempArray<float> depth( lightShadowAtlasSize * lightShadowAtlasSize );
		GL_ReadPixels( 0, 0, lightShadowAtlasSize, lightShadowAtlasSize, PIXELFORMAT_DEPTH_FLOAT, depth.Ptr() );
		float minimum = 1.0f;
		float maximum = 0.0f;
		int written = 0;
		for ( unsigned int i = 0; i < depth.Num(); i++ ) {
			minimum = Min( minimum, depth[i] );
			maximum = Max( maximum, depth[i] );
			written += depth[i] < 0.999999f;
		}
		common->Printf( "shadow atlas: type %d, depth %.6f..%.6f, written %d/%d\n", static_cast<int>( currentShadowType ), minimum, maximum, written, depth.Num() );
		reportedShadowAtlas = true;
		static bool dumpedShadowAtlas = false;
		if ( !dumpedShadowAtlas ) {
			idTempArray<byte> rgba( depth.Num() * 4 );
			for ( unsigned int i = 0; i < depth.Num(); i++ ) {
				const byte value = static_cast<byte>( idMath::Ftob( depth[i] * 255.0f ) );
				rgba[i * 4 + 0] = value;
				rgba[i * 4 + 1] = value;
				rgba[i * 4 + 2] = value;
				rgba[i * 4 + 3] = 255;
			}
			R_WriteTGA( "shadowAtlas.tga", rgba.Ptr(), lightShadowAtlasSize, lightShadowAtlasSize, true );
			dumpedShadowAtlas = true;
		}
	}
	renderProgManager.Unbind();
	GL_Cull( previousCullType );
	GL_BindFramebuffer( previousFramebuffer );
	GL_DrawBuffer( previousDrawBuffer );
	GL_ReadBuffer( previousReadBuffer );
	GL_State( previousState );
	GL_Viewport( previousViewport[0], previousViewport[1], previousViewport[2], previousViewport[3] );
	GL_Scissor( previousScissor[0], previousScissor[1], previousScissor[2], previousScissor[3] );
	// The GL scissor has been restored exactly, but its engine-side identity is
	// intentionally invalidated so the next draw cannot skip a required update.
	backEnd.currentScissor.Clear();
	backEnd.currentSpace = NULL;
	GL_SelectTexture( 5 );
	lightShadowAtlas->Bind();
	return true;
}

/*
================
RB_SetVertexColorParms
================
*/
static void RB_SetVertexColorParms( stageVertexColor_t svc ) {
	switch ( svc ) {
		case SVC_IGNORE:
			SetVertexParm( RENDERPARM_VERTEXCOLOR_MODULATE, zero );
			SetVertexParm( RENDERPARM_VERTEXCOLOR_ADD, one );
			break;
		case SVC_MODULATE:
			SetVertexParm( RENDERPARM_VERTEXCOLOR_MODULATE, one );
			SetVertexParm( RENDERPARM_VERTEXCOLOR_ADD, zero );
			break;
		case SVC_INVERSE_MODULATE:
			SetVertexParm( RENDERPARM_VERTEXCOLOR_MODULATE, negOne );
			SetVertexParm( RENDERPARM_VERTEXCOLOR_ADD, one );
			break;
	}
}

/*
================
RB_DrawElementsWithCounters
================
*/
void RB_DrawElementsWithCounters( const drawSurf_t *surf ) {
	// get vertex buffer
	const vertCacheHandle_t vbHandle = surf->ambientCache;
	idVertexBuffer * vertexBuffer;
	if ( vertexCache.CacheIsStatic( vbHandle ) ) {
		vertexBuffer = &vertexCache.staticData.vertexBuffer;
	} else {
		const uint64 frameNum = (int)( vbHandle >> VERTCACHE_FRAME_SHIFT ) & VERTCACHE_FRAME_MASK;
		if ( frameNum != ( ( vertexCache.currentFrame - 1 ) & VERTCACHE_FRAME_MASK ) ) {
			idLib::Warning( "RB_DrawElementsWithCounters, vertexBuffer == NULL" );
			return;
		}
		vertexBuffer = &vertexCache.frameData[vertexCache.drawListNum].vertexBuffer;
	}
	const int vertOffset = (int)( vbHandle >> VERTCACHE_OFFSET_SHIFT ) & VERTCACHE_OFFSET_MASK;

	// get index buffer
	const vertCacheHandle_t ibHandle = surf->indexCache;
	idIndexBuffer * indexBuffer;
	if ( vertexCache.CacheIsStatic( ibHandle ) ) {
		indexBuffer = &vertexCache.staticData.indexBuffer;
	} else {
		const uint64 frameNum = (int)( ibHandle >> VERTCACHE_FRAME_SHIFT ) & VERTCACHE_FRAME_MASK;
		if ( frameNum != ( ( vertexCache.currentFrame - 1 ) & VERTCACHE_FRAME_MASK ) ) {
			idLib::Warning( "RB_DrawElementsWithCounters, indexBuffer == NULL" );
			return;
		}
		indexBuffer = &vertexCache.frameData[vertexCache.drawListNum].indexBuffer;
	}
	const int indexOffset = (int)( ibHandle >> VERTCACHE_OFFSET_SHIFT ) & VERTCACHE_OFFSET_MASK;

	RENDERLOG_PRINTF( "Binding Buffers: %p:%i %p:%i\n", vertexBuffer, vertOffset, indexBuffer, indexOffset );

	if ( surf->jointCache ) {
		if ( !verify( renderProgManager.ShaderUsesJoints() ) ) {
			return;
		}
	} else {
		if ( !verify( !renderProgManager.ShaderUsesJoints() || renderProgManager.ShaderHasOptionalSkinning() ) ) {
			return;
		}
	}


	if ( surf->jointCache ) {
		idJointBuffer jointBuffer;
		if ( !vertexCache.GetJointBuffer( surf->jointCache, &jointBuffer ) ) {
			idLib::Warning( "RB_DrawElementsWithCounters, jointBuffer == NULL" );
			return;
		}
		assert( ( jointBuffer.GetOffset() & ( glConfig.uniformBufferOffsetAlignment - 1 ) ) == 0 );

		const GLuint ubo = reinterpret_cast< GLuint >( jointBuffer.GetAPIObject() );
		GL_BindBufferRange( BUFFER_TARGET_UNIFORM, 0, ubo, jointBuffer.GetOffset(), jointBuffer.GetNumJoints() * sizeof( idJointMat ) );
	}

	renderProgManager.CommitUniforms();

	if ( backEnd.glState.currentIndexBuffer != (GLuint)indexBuffer->GetAPIObject() || !r_useStateCaching.GetBool() ) {
		GL_BindBuffer( BUFFER_TARGET_ELEMENT, (GLuint)indexBuffer->GetAPIObject() );
		backEnd.glState.currentIndexBuffer = (GLuint)indexBuffer->GetAPIObject();
	}

	if ( ( backEnd.glState.vertexLayout != LAYOUT_DRAW_VERT ) || ( backEnd.glState.currentVertexBuffer != (GLuint)vertexBuffer->GetAPIObject() ) || !r_useStateCaching.GetBool() ) {
		GL_BindBuffer( BUFFER_TARGET_ARRAY, (GLuint)vertexBuffer->GetAPIObject() );
		backEnd.glState.currentVertexBuffer = (GLuint)vertexBuffer->GetAPIObject();

		GL_EnableVertexAttribArray( PC_ATTRIB_INDEX_VERTEX );
		GL_EnableVertexAttribArray( PC_ATTRIB_INDEX_NORMAL );
		GL_EnableVertexAttribArray( PC_ATTRIB_INDEX_COLOR );
		GL_EnableVertexAttribArray( PC_ATTRIB_INDEX_COLOR2 );
		GL_EnableVertexAttribArray( PC_ATTRIB_INDEX_ST );
		GL_EnableVertexAttribArray( PC_ATTRIB_INDEX_TANGENT );

		GL_VertexAttribPointer( PC_ATTRIB_INDEX_VERTEX, 3, VERTEX_ATTRIB_FLOAT, false, sizeof( idDrawVert ), DRAWVERT_XYZ_OFFSET );
		GL_VertexAttribPointer( PC_ATTRIB_INDEX_NORMAL, 4, VERTEX_ATTRIB_UBYTE, true, sizeof( idDrawVert ), DRAWVERT_NORMAL_OFFSET );
		GL_VertexAttribPointer( PC_ATTRIB_INDEX_COLOR, 4, VERTEX_ATTRIB_UBYTE, true, sizeof( idDrawVert ), DRAWVERT_COLOR_OFFSET );
		GL_VertexAttribPointer( PC_ATTRIB_INDEX_COLOR2, 4, VERTEX_ATTRIB_UBYTE, true, sizeof( idDrawVert ), DRAWVERT_COLOR2_OFFSET );
		GL_VertexAttribPointer( PC_ATTRIB_INDEX_ST, 2, VERTEX_ATTRIB_HALF_FLOAT, true, sizeof( idDrawVert ), DRAWVERT_ST_OFFSET );
		GL_VertexAttribPointer( PC_ATTRIB_INDEX_TANGENT, 4, VERTEX_ATTRIB_UBYTE, true, sizeof( idDrawVert ), DRAWVERT_TANGENT_OFFSET );

		backEnd.glState.vertexLayout = LAYOUT_DRAW_VERT;
	}
	
	GL_DrawElementsBaseVertex( 
							  r_singleTriangle.GetBool() ? 3 : surf->numIndexes,
							  
							  (triIndex_t *)indexOffset,
							  vertOffset / sizeof ( idDrawVert ) );
							  

}

/*
======================
RB_GetShaderTextureMatrix
======================
*/
static void RB_GetShaderTextureMatrix( const float *shaderRegisters, const textureStage_t *texture, float matrix[16] ) {
	matrix[0*4+0] = shaderRegisters[ texture->matrix[0][0] ];
	matrix[1*4+0] = shaderRegisters[ texture->matrix[0][1] ];
	matrix[2*4+0] = 0.0f;
	matrix[3*4+0] = shaderRegisters[ texture->matrix[0][2] ];

	matrix[0*4+1] = shaderRegisters[ texture->matrix[1][0] ];
	matrix[1*4+1] = shaderRegisters[ texture->matrix[1][1] ];
	matrix[2*4+1] = 0.0f;
	matrix[3*4+1] = shaderRegisters[ texture->matrix[1][2] ];

	// we attempt to keep scrolls from generating incredibly large texture values, but
	// center rotations and center scales can still generate offsets that need to be > 1
	if ( matrix[3*4+0] < -40.0f || matrix[12] > 40.0f ) {
		matrix[3*4+0] -= (int)matrix[3*4+0];
	}
	if ( matrix[13] < -40.0f || matrix[13] > 40.0f ) {
		matrix[13] -= (int)matrix[13];
	}

	matrix[0*4+2] = 0.0f;
	matrix[1*4+2] = 0.0f;
	matrix[2*4+2] = 1.0f;
	matrix[3*4+2] = 0.0f;

	matrix[0*4+3] = 0.0f;
	matrix[1*4+3] = 0.0f;
	matrix[2*4+3] = 0.0f;
	matrix[3*4+3] = 1.0f;
}

/*
======================
RB_LoadShaderTextureMatrix
======================
*/
static void RB_LoadShaderTextureMatrix( const float *shaderRegisters, const textureStage_t *texture ) {	
	float texS[4] = { 1.0f, 0.0f, 0.0f, 0.0f };
	float texT[4] = { 0.0f, 1.0f, 0.0f, 0.0f };

	if ( texture->hasMatrix ) {
		float matrix[16];
		RB_GetShaderTextureMatrix( shaderRegisters, texture, matrix );
		texS[0] = matrix[0*4+0];
		texS[1] = matrix[1*4+0];
		texS[2] = matrix[2*4+0];
		texS[3] = matrix[3*4+0];
	
		texT[0] = matrix[0*4+1];
		texT[1] = matrix[1*4+1];
		texT[2] = matrix[2*4+1];
		texT[3] = matrix[3*4+1];

		RENDERLOG_PRINTF( "Setting Texture Matrix\n");
		renderLog.Indent();
		RENDERLOG_PRINTF( "Texture Matrix S : %4.3f, %4.3f, %4.3f, %4.3f\n", texS[0], texS[1], texS[2], texS[3] );
		RENDERLOG_PRINTF( "Texture Matrix T : %4.3f, %4.3f, %4.3f, %4.3f\n", texT[0], texT[1], texT[2], texT[3] );
		renderLog.Outdent();
	} 

	SetVertexParm( RENDERPARM_TEXTUREMATRIX_S, texS );
	SetVertexParm( RENDERPARM_TEXTUREMATRIX_T, texT );
}

/*
=====================
RB_BakeTextureMatrixIntoTexgen
=====================
*/
static void RB_BakeTextureMatrixIntoTexgen( idPlane lightProject[3], const float *textureMatrix ) {
	float genMatrix[16];
	float final[16];

	genMatrix[0*4+0] = lightProject[0][0];
	genMatrix[1*4+0] = lightProject[0][1];
	genMatrix[2*4+0] = lightProject[0][2];
	genMatrix[3*4+0] = lightProject[0][3];

	genMatrix[0*4+1] = lightProject[1][0];
	genMatrix[1*4+1] = lightProject[1][1];
	genMatrix[2*4+1] = lightProject[1][2];
	genMatrix[3*4+1] = lightProject[1][3];

	genMatrix[0*4+2] = 0.0f;
	genMatrix[1*4+2] = 0.0f;
	genMatrix[2*4+2] = 0.0f;
	genMatrix[3*4+2] = 0.0f;

	genMatrix[0*4+3] = lightProject[2][0];
	genMatrix[1*4+3] = lightProject[2][1];
	genMatrix[2*4+3] = lightProject[2][2];
	genMatrix[3*4+3] = lightProject[2][3];

	R_MatrixMultiply( genMatrix, textureMatrix, final );

	lightProject[0][0] = final[0*4+0];
	lightProject[0][1] = final[1*4+0];
	lightProject[0][2] = final[2*4+0];
	lightProject[0][3] = final[3*4+0];

	lightProject[1][0] = final[0*4+1];
	lightProject[1][1] = final[1*4+1];
	lightProject[1][2] = final[2*4+1];
	lightProject[1][3] = final[3*4+1];
}

/*
======================
RB_BindVariableStageImage

Handles generating a cinematic frame if needed
======================
*/
static void RB_BindVariableStageImage( const textureStage_t *texture, const float *shaderRegisters ) {
	if ( texture->cinematic ) {
		cinData_t cin;

		if ( r_skipDynamicTextures.GetBool() ) {
			globalImages->defaultImage->Bind();
			return;
		}

		// offset time by shaderParm[7] (FIXME: make the time offset a parameter of the shader?)
		// We make no attempt to optimize for multiple identical cinematics being in view, or
		// for cinematics going at a lower framerate than the renderer.
		cin = texture->cinematic->ImageForTime( backEnd.viewDef->renderView.time[0] + idMath::Ftoi( 1000.0f * backEnd.viewDef->renderView.shaderParms[11] ) );
		if ( cin.imageY != NULL ) {
			GL_SelectTexture( 0 );
			cin.imageY->Bind();
			GL_SelectTexture( 1 );
			cin.imageCr->Bind();
			GL_SelectTexture( 2 );
			cin.imageCb->Bind();
		} else {
			globalImages->blackImage->Bind();
			// because the shaders may have already been set - we need to make sure we are not using a bink shader which would 
			// display incorrectly.  We may want to get rid of RB_BindVariableStageImage and inline the code so that the
			// SWF GUI case is handled better, too
			renderProgManager.BindShader_TextureVertexColor();
		}
	} else {
		// FIXME: see why image is invalid
		if ( texture->image != NULL ) {
			texture->image->Bind();
		}
	}
}

/*
================
RB_PrepareStageTexturing
================
*/
static void RB_PrepareStageTexturing( const shaderStage_t * pStage,  const drawSurf_t * surf ) {
	float useTexGenParm[4] = { 0.0f, 0.0f, 0.0f, 0.0f };

	// set the texture matrix if needed
	RB_LoadShaderTextureMatrix( surf->shaderRegisters, &pStage->texture );

	// texgens
	if ( pStage->texture.texgen == TG_REFLECT_CUBE ) {

		// see if there is also a bump map specified
		const shaderStage_t *bumpStage = surf->material->GetBumpStage();
		if ( bumpStage != NULL ) {
			// per-pixel reflection mapping with bump mapping
			GL_SelectTexture( 1 );
			bumpStage->texture.image->Bind();
			GL_SelectTexture( 0 );

			RENDERLOG_PRINTF( "TexGen: TG_REFLECT_CUBE: Bumpy Environment\n" );
			if ( surf->jointCache ) {
				renderProgManager.BindShader_BumpyEnvironmentSkinned();
			} else {
				renderProgManager.BindShader_BumpyEnvironment();
			}
		} else {
			RENDERLOG_PRINTF( "TexGen: TG_REFLECT_CUBE: Environment\n" );
			if ( surf->jointCache ) {
				renderProgManager.BindShader_EnvironmentSkinned();
			} else {
				renderProgManager.BindShader_Environment();
			}
		}

	} else if ( pStage->texture.texgen == TG_SKYBOX_CUBE ) {

		renderProgManager.BindShader_SkyBox();

	} else if ( pStage->texture.texgen == TG_WOBBLESKY_CUBE ) {

		const int * parms = surf->material->GetTexGenRegisters();

		float wobbleDegrees = surf->shaderRegisters[ parms[0] ] * ( idMath::PI / 180.0f );
		float wobbleSpeed = surf->shaderRegisters[ parms[1] ] * ( 2.0f * idMath::PI / 60.0f );
		float rotateSpeed = surf->shaderRegisters[ parms[2] ] * ( 2.0f * idMath::PI / 60.0f );

		idVec3 axis[3];
		{
			// very ad-hoc "wobble" transform
			float s, c;
			idMath::SinCos( wobbleSpeed * backEnd.viewDef->renderView.time[0] * 0.001f, s, c );

			float ws, wc;
			idMath::SinCos( wobbleDegrees, ws, wc );

			axis[2][0] = ws * c;
			axis[2][1] = ws * s;
			axis[2][2] = wc;

			axis[1][0] = -s * s * ws;
			axis[1][2] = -s * ws * ws;
			axis[1][1] = idMath::Sqrt( idMath::Fabs( 1.0f - ( axis[1][0] * axis[1][0] + axis[1][2] * axis[1][2] ) ) );

			// make the second vector exactly perpendicular to the first
			axis[1] -= ( axis[2] * axis[1] ) * axis[2];
			axis[1].Normalize();

			// construct the third with a cross
			axis[0].Cross( axis[1], axis[2] );
		}

		// add the rotate
		float rs, rc;
		idMath::SinCos( rotateSpeed * backEnd.viewDef->renderView.time[0] * 0.001f, rs, rc );

		float transform[12];
		transform[0*4+0] = axis[0][0] * rc + axis[1][0] * rs;
		transform[0*4+1] = axis[0][1] * rc + axis[1][1] * rs;
		transform[0*4+2] = axis[0][2] * rc + axis[1][2] * rs;
		transform[0*4+3] = 0.0f;

		transform[1*4+0] = axis[1][0] * rc - axis[0][0] * rs;
		transform[1*4+1] = axis[1][1] * rc - axis[0][1] * rs;
		transform[1*4+2] = axis[1][2] * rc - axis[0][2] * rs;
		transform[1*4+3] = 0.0f;

		transform[2*4+0] = axis[2][0];
		transform[2*4+1] = axis[2][1];
		transform[2*4+2] = axis[2][2];
		transform[2*4+3] = 0.0f;

		SetVertexParms( RENDERPARM_WOBBLESKY_X, transform, 3 );
		renderProgManager.BindShader_WobbleSky();

	} else if ( ( pStage->texture.texgen == TG_SCREEN ) || ( pStage->texture.texgen == TG_SCREEN2 ) ) {

		useTexGenParm[0] = 1.0f;
		useTexGenParm[1] = 1.0f;
		useTexGenParm[2] = 1.0f;
		useTexGenParm[3] = 1.0f;

		float mat[16];
		R_MatrixMultiply( surf->space->modelViewMatrix, backEnd.viewDef->projectionMatrix, mat );

		RENDERLOG_PRINTF( "TexGen : %s\n", ( pStage->texture.texgen == TG_SCREEN ) ? "TG_SCREEN" : "TG_SCREEN2" );
		renderLog.Indent();

		float plane[4];
		plane[0] = mat[0*4+0];
		plane[1] = mat[1*4+0];
		plane[2] = mat[2*4+0];
		plane[3] = mat[3*4+0];
		SetVertexParm( RENDERPARM_TEXGEN_0_S, plane );
		RENDERLOG_PRINTF( "TEXGEN_S = %4.3f, %4.3f, %4.3f, %4.3f\n",  plane[0], plane[1], plane[2], plane[3] );

		plane[0] = mat[0*4+1];
		plane[1] = mat[1*4+1];
		plane[2] = mat[2*4+1];
		plane[3] = mat[3*4+1];
		SetVertexParm( RENDERPARM_TEXGEN_0_T, plane );
		RENDERLOG_PRINTF( "TEXGEN_T = %4.3f, %4.3f, %4.3f, %4.3f\n",  plane[0], plane[1], plane[2], plane[3] );

		plane[0] = mat[0*4+3];
		plane[1] = mat[1*4+3];
		plane[2] = mat[2*4+3];
		plane[3] = mat[3*4+3];
		SetVertexParm( RENDERPARM_TEXGEN_0_Q, plane );	
		RENDERLOG_PRINTF( "TEXGEN_Q = %4.3f, %4.3f, %4.3f, %4.3f\n",  plane[0], plane[1], plane[2], plane[3] );

		renderLog.Outdent();

	} else if ( pStage->texture.texgen == TG_DIFFUSE_CUBE ) {

		// As far as I can tell, this is never used
		idLib::Warning( "Using Diffuse Cube! Please contact Brian!" );

	} else if ( pStage->texture.texgen == TG_GLASSWARP ) {

		// As far as I can tell, this is never used
		idLib::Warning( "Using GlassWarp! Please contact Brian!" );
	}

	SetVertexParm( RENDERPARM_TEXGEN_0_ENABLED, useTexGenParm );
}

/*
================
RB_FinishStageTexturing
================
*/
static void RB_FinishStageTexturing( const shaderStage_t *pStage, const drawSurf_t *surf ) {

	if ( pStage->texture.cinematic ) {
		// unbind the extra bink textures
		GL_SelectTexture( 1 );
		globalImages->BindNull();
		GL_SelectTexture( 2 );
		globalImages->BindNull();
		GL_SelectTexture( 0 );
	}

	if ( pStage->texture.texgen == TG_REFLECT_CUBE ) {
		// see if there is also a bump map specified
		const shaderStage_t *bumpStage = surf->material->GetBumpStage();
		if ( bumpStage != NULL ) {
			// per-pixel reflection mapping with bump mapping
			GL_SelectTexture( 1 );
			globalImages->BindNull();
			GL_SelectTexture( 0 );
		} else {
			// per-pixel reflection mapping without bump mapping
		}
		renderProgManager.Unbind();
	}
}

/*
=========================================================================================

DEPTH BUFFER RENDERING

=========================================================================================
*/

/*
==================
RB_FillDepthBufferGeneric
==================
*/
static void RB_FillDepthBufferGeneric( const drawSurf_t * const * drawSurfs, int numDrawSurfs ) {
	for ( int i = 0; i < numDrawSurfs; i++ ) {
		const drawSurf_t * drawSurf = drawSurfs[i];
		const idMaterial * shader = drawSurf->material;

		// translucent surfaces don't put anything in the depth buffer and don't
		// test against it, which makes them fail the mirror clip plane operation
		if ( shader->Coverage() == MC_TRANSLUCENT ) {
			continue;
		}

		// get the expressions for conditionals / color / texcoords
		const float * regs = drawSurf->shaderRegisters;

		// if all stages of a material have been conditioned off, don't do anything
		int stage = 0;
		for ( ; stage < shader->GetNumStages(); stage++ ) {		
			const shaderStage_t * pStage = shader->GetStage( stage );
			// check the stage enable condition
			if ( regs[ pStage->conditionRegister ] != 0 ) {
				break;
			}
		}
		if ( stage == shader->GetNumStages() ) {
			continue;
		}

		// change the matrix if needed
		if ( drawSurf->space != backEnd.currentSpace ) {
			RB_SetMVP( drawSurf->space->mvp );

			backEnd.currentSpace = drawSurf->space;
		}

		uint64 surfGLState = 0;

		// set polygon offset if necessary
		if ( shader->TestMaterialFlag( MF_POLYGONOFFSET ) ) {
			surfGLState |= GLS_POLYGON_OFFSET;
			GL_PolygonOffset( r_offsetFactor.GetFloat(), r_offsetUnits.GetFloat() * shader->GetPolygonOffset() );
		}

		// subviews will just down-modulate the color buffer
		float color[4];
		if ( shader->GetSort() == SS_SUBVIEW ) {
			surfGLState |= GLS_SRCBLEND_DST_COLOR | GLS_DSTBLEND_ZERO | GLS_DEPTHFUNC_LESS;
			color[0] = 1.0f;
			color[1] = 1.0f;
			color[2] = 1.0f;
			color[3] = 1.0f;
		} else {
			// others just draw black
			color[0] = 0.0f;
			color[1] = 0.0f;
			color[2] = 0.0f;
			color[3] = 1.0f;
		}

		renderLog.OpenBlock( shader->GetName() );

		bool drawSolid = false;
		if ( shader->Coverage() == MC_OPAQUE ) {
			drawSolid = true;
		} else if ( shader->Coverage() == MC_PERFORATED ) {
			// we may have multiple alpha tested stages
			// if the only alpha tested stages are condition register omitted,
			// draw a normal opaque surface
			bool didDraw = false;

			// perforated surfaces may have multiple alpha tested stages
			for ( stage = 0; stage < shader->GetNumStages(); stage++ ) {		
				const shaderStage_t *pStage = shader->GetStage(stage);

				if ( !pStage->hasAlphaTest ) {
					continue;
				}

				// check the stage enable condition
				if ( regs[ pStage->conditionRegister ] == 0 ) {
					continue;
				}

				// if we at least tried to draw an alpha tested stage,
				// we won't draw the opaque surface
				didDraw = true;

				// set the alpha modulate
				color[3] = regs[ pStage->color.registers[3] ];

				// skip the entire stage if alpha would be black
				if ( color[3] <= 0.0f ) {
					continue;
				}

				uint64 stageGLState = surfGLState;

				// set privatePolygonOffset if necessary
				if ( pStage->privatePolygonOffset ) {
					GL_PolygonOffset( r_offsetFactor.GetFloat(), r_offsetUnits.GetFloat() * pStage->privatePolygonOffset );
					stageGLState |= GLS_POLYGON_OFFSET;
				}

				GL_Color( color );

				GL_State( stageGLState );
				idVec4 alphaTestValue( regs[ pStage->alphaTestRegister ] );
				SetFragmentParm( RENDERPARM_ALPHA_TEST, alphaTestValue.ToFloatPtr() );

				if ( drawSurf->jointCache ) {
					renderProgManager.BindShader_TextureVertexColorSkinned();
				} else {
					renderProgManager.BindShader_TextureVertexColor();
				}

				RB_SetVertexColorParms( SVC_IGNORE );

				// bind the texture
				GL_SelectTexture( 0 );
				pStage->texture.image->Bind();

				// set texture matrix and texGens
				RB_PrepareStageTexturing( pStage, drawSurf );

				// must render with less-equal for Z-Cull to work properly
				assert( ( GL_GetCurrentState() & GLS_DEPTHFUNC_BITS ) == GLS_DEPTHFUNC_LESS );

				// draw it
				RB_DrawElementsWithCounters( drawSurf );

				// clean up
				RB_FinishStageTexturing( pStage, drawSurf );

				// unset privatePolygonOffset if necessary
				if ( pStage->privatePolygonOffset ) {
					GL_PolygonOffset( r_offsetFactor.GetFloat(), r_offsetUnits.GetFloat() * shader->GetPolygonOffset() );
				}
			}

			if ( !didDraw ) {
				drawSolid = true;
			}
		}

		// draw the entire surface solid
		if ( drawSolid ) {
			if ( shader->GetSort() == SS_SUBVIEW ) {
				renderProgManager.BindShader_Color();
				GL_Color( color );
				GL_State( surfGLState );
			} else {
				if ( drawSurf->jointCache ) {
					renderProgManager.BindShader_DepthSkinned();
				} else {
					renderProgManager.BindShader_Depth();
				}
				GL_State( surfGLState | GLS_ALPHAMASK );
			}

			// must render with less-equal for Z-Cull to work properly
			assert( ( GL_GetCurrentState() & GLS_DEPTHFUNC_BITS ) == GLS_DEPTHFUNC_LESS );

			// draw it
			RB_DrawElementsWithCounters( drawSurf );
		}

		renderLog.CloseBlock();
	}

	SetFragmentParm( RENDERPARM_ALPHA_TEST, vec4_zero.ToFloatPtr() );
}

/*
=====================
RB_FillDepthBufferFast

Optimized fast path code.

If there are subview surfaces, they must be guarded in the depth buffer to allow
the mirror / subview to show through underneath the current view rendering.

Surfaces with perforated shaders need the full shader setup done, but should be
drawn after the opaque surfaces.

The bulk of the surfaces should be simple opaque geometry that can be drawn very rapidly.

If there are no subview surfaces, we could clear to black and use fast-Z rendering
on the 360.
=====================
*/
static void RB_FillDepthBufferFast( drawSurf_t **drawSurfs, int numDrawSurfs ) {
	if ( numDrawSurfs == 0 ) {
		return;
	}

	// if we are just doing 2D rendering, no need to fill the depth buffer
	if ( backEnd.viewDef->viewEntitys == NULL ) {
		return;
	}

	renderLog.OpenMainBlock( MRB_FILL_DEPTH_BUFFER );
	renderLog.OpenBlock( "RB_FillDepthBufferFast" );

	GL_StartDepthPass( backEnd.viewDef->scissor );

	// force MVP change on first surface
	backEnd.currentSpace = NULL;

	// draw all the subview surfaces, which will already be at the start of the sorted list,
	// with the general purpose path
	GL_State( GLS_DEFAULT );

	int	surfNum;
	for ( surfNum = 0; surfNum < numDrawSurfs; surfNum++ ) {
		if ( drawSurfs[surfNum]->material->GetSort() != SS_SUBVIEW ) {
			break;
		}
		RB_FillDepthBufferGeneric( &drawSurfs[surfNum], 1 );
	}

	const drawSurf_t ** perforatedSurfaces = (const drawSurf_t ** )_alloca( numDrawSurfs * sizeof( drawSurf_t * ) );
	int numPerforatedSurfaces = 0;

	// draw all the opaque surfaces and build up a list of perforated surfaces that
	// we will defer drawing until all opaque surfaces are done
	GL_State( GLS_DEFAULT );

	// continue checking past the subview surfaces
	for ( ; surfNum < numDrawSurfs; surfNum++ ) {
		const drawSurf_t * surf = drawSurfs[ surfNum ];
		const idMaterial * shader = surf->material;

		// translucent surfaces don't put anything in the depth buffer
		if ( shader->Coverage() == MC_TRANSLUCENT ) {
			continue;
		}
		if ( shader->Coverage() == MC_PERFORATED ) {
			// save for later drawing
			perforatedSurfaces[ numPerforatedSurfaces ] = surf;
			numPerforatedSurfaces++;
			continue;
		}

		// set polygon offset?

		// set mvp matrix
		if ( surf->space != backEnd.currentSpace ) {
			RB_SetMVP( surf->space->mvp );
			backEnd.currentSpace = surf->space;
		}

		renderLog.OpenBlock( shader->GetName() );

		if ( surf->jointCache ) {
			renderProgManager.BindShader_DepthSkinned();
		} else {
			renderProgManager.BindShader_Depth();
		}

		// must render with less-equal for Z-Cull to work properly
		assert( ( GL_GetCurrentState() & GLS_DEPTHFUNC_BITS ) == GLS_DEPTHFUNC_LESS );

		// draw it solid
		RB_DrawElementsWithCounters( surf );

		renderLog.CloseBlock();
	}

	// draw all perforated surfaces with the general code path
	if ( numPerforatedSurfaces > 0 ) {
		RB_FillDepthBufferGeneric( perforatedSurfaces, numPerforatedSurfaces );
	}

	// Allow platform specific data to be collected after the depth pass.
	GL_FinishDepthPass();

	renderLog.CloseBlock();
	renderLog.CloseMainBlock();
}

/*
=========================================================================================

GENERAL INTERACTION RENDERING

=========================================================================================
*/

const int INTERACTION_TEXUNIT_BUMP			= 0;
const int INTERACTION_TEXUNIT_FALLOFF		= 1;
const int INTERACTION_TEXUNIT_PROJECTION	= 2;
const int INTERACTION_TEXUNIT_DIFFUSE		= 3;
const int INTERACTION_TEXUNIT_SPECULAR		= 4;

/*
==================
RB_SetupInteractionStage
==================
*/
static void RB_SetupInteractionStage( const shaderStage_t *surfaceStage, const float *surfaceRegs, const float lightColor[4],
									idVec4 matrix[2], float color[4] ) {

	if ( surfaceStage->texture.hasMatrix ) {
		matrix[0][0] = surfaceRegs[surfaceStage->texture.matrix[0][0]];
		matrix[0][1] = surfaceRegs[surfaceStage->texture.matrix[0][1]];
		matrix[0][2] = 0.0f;
		matrix[0][3] = surfaceRegs[surfaceStage->texture.matrix[0][2]];

		matrix[1][0] = surfaceRegs[surfaceStage->texture.matrix[1][0]];
		matrix[1][1] = surfaceRegs[surfaceStage->texture.matrix[1][1]];
		matrix[1][2] = 0.0f;
		matrix[1][3] = surfaceRegs[surfaceStage->texture.matrix[1][2]];

		// we attempt to keep scrolls from generating incredibly large texture values, but
		// center rotations and center scales can still generate offsets that need to be > 1
		if ( matrix[0][3] < -40.0f || matrix[0][3] > 40.0f ) {
			matrix[0][3] -= idMath::Ftoi( matrix[0][3] );
		}
		if ( matrix[1][3] < -40.0f || matrix[1][3] > 40.0f ) {
			matrix[1][3] -= idMath::Ftoi( matrix[1][3] );
		}
	} else {
		matrix[0][0] = 1.0f;
		matrix[0][1] = 0.0f;
		matrix[0][2] = 0.0f;
		matrix[0][3] = 0.0f;

		matrix[1][0] = 0.0f;
		matrix[1][1] = 1.0f;
		matrix[1][2] = 0.0f;
		matrix[1][3] = 0.0f;
	}

	if ( color != NULL ) {
		for ( int i = 0; i < 4; i++ ) {
			// clamp here, so cards with a greater range don't look different.
			// we could perform overbrighting like we do for lights, but
			// it doesn't currently look worth it.
			color[i] = idMath::ClampFloat( 0.0f, 1.0f, surfaceRegs[surfaceStage->color.registers[i]] ) * lightColor[i];
		}
	}
}

/*
=================
RB_DrawSingleInteraction
=================
*/
static void RB_DrawSingleInteraction( drawInteraction_t * din ) {
	if ( din->bumpImage == NULL ) {
		// stage wasn't actually an interaction
		return;
	}

	if ( din->diffuseImage == NULL || r_skipDiffuse.GetBool() ) {
		// this isn't a YCoCg black, but it doesn't matter, because
		// the diffuseColor will also be 0
		din->diffuseImage = globalImages->blackImage;
	}
	if ( din->specularImage == NULL || r_skipSpecular.GetBool() || din->ambientLight ) {
		din->specularImage = globalImages->blackImage;
	}
	if ( r_skipBump.GetBool() ) {
		din->bumpImage = globalImages->flatNormalMap;
	}

	// if we wouldn't draw anything, don't call the Draw function
	const bool diffuseIsBlack = ( din->diffuseImage == globalImages->blackImage )
									|| ( ( din->diffuseColor[0] <= 0 ) && ( din->diffuseColor[1] <= 0 ) && ( din->diffuseColor[2] <= 0 ) );
	const bool specularIsBlack = ( din->specularImage == globalImages->blackImage )
									|| ( ( din->specularColor[0] <= 0 ) && ( din->specularColor[1] <= 0 ) && ( din->specularColor[2] <= 0 ) );
	if ( diffuseIsBlack && specularIsBlack ) {
		return;
	}

	// bump matrix
	SetVertexParm( RENDERPARM_BUMPMATRIX_S, din->bumpMatrix[0].ToFloatPtr() );
	SetVertexParm( RENDERPARM_BUMPMATRIX_T, din->bumpMatrix[1].ToFloatPtr() );

	// diffuse matrix
	SetVertexParm( RENDERPARM_DIFFUSEMATRIX_S, din->diffuseMatrix[0].ToFloatPtr() );
	SetVertexParm( RENDERPARM_DIFFUSEMATRIX_T, din->diffuseMatrix[1].ToFloatPtr() );

	// specular matrix
	SetVertexParm( RENDERPARM_SPECULARMATRIX_S, din->specularMatrix[0].ToFloatPtr() );
	SetVertexParm( RENDERPARM_SPECULARMATRIX_T, din->specularMatrix[1].ToFloatPtr() );

	RB_SetVertexColorParms( din->vertexColor );

	SetFragmentParm( RENDERPARM_DIFFUSEMODIFIER, din->diffuseColor.ToFloatPtr() );
	SetFragmentParm( RENDERPARM_SPECULARMODIFIER, din->specularColor.ToFloatPtr() );

	// texture 0 will be the per-surface bump map
 	GL_SelectTexture( INTERACTION_TEXUNIT_BUMP );
	din->bumpImage->Bind();

	// texture 3 is the per-surface diffuse map
	GL_SelectTexture( INTERACTION_TEXUNIT_DIFFUSE );
	din->diffuseImage->Bind();

	// texture 4 is the per-surface specular map
	GL_SelectTexture( INTERACTION_TEXUNIT_SPECULAR );
	din->specularImage->Bind();

	RB_DrawElementsWithCounters( din->surf );
}

/*
=================
RB_SetupForFastPathInteractions

These are common for all fast path surfaces
=================
*/
static void RB_SetupForFastPathInteractions( const idVec4 & diffuseColor, const idVec4 & specularColor ) {
	const idVec4 sMatrix( 1, 0, 0, 0 );
	const idVec4 tMatrix( 0, 1, 0, 0 );

	// bump matrix
	SetVertexParm( RENDERPARM_BUMPMATRIX_S, sMatrix.ToFloatPtr() );
	SetVertexParm( RENDERPARM_BUMPMATRIX_T, tMatrix.ToFloatPtr() );

	// diffuse matrix
	SetVertexParm( RENDERPARM_DIFFUSEMATRIX_S, sMatrix.ToFloatPtr() );
	SetVertexParm( RENDERPARM_DIFFUSEMATRIX_T, tMatrix.ToFloatPtr() );

	// specular matrix
	SetVertexParm( RENDERPARM_SPECULARMATRIX_S, sMatrix.ToFloatPtr() );
	SetVertexParm( RENDERPARM_SPECULARMATRIX_T, tMatrix.ToFloatPtr() );

	RB_SetVertexColorParms( SVC_IGNORE );

	SetFragmentParm( RENDERPARM_DIFFUSEMODIFIER, diffuseColor.ToFloatPtr() );
	SetFragmentParm( RENDERPARM_SPECULARMODIFIER, specularColor.ToFloatPtr() );
}

/*
=============
RB_RenderInteractions

With added sorting and trivial path work.
=============
*/
static void RB_RenderInteractions( const drawSurf_t *surfList, const viewLight_t * vLight, int depthFunc, bool useShadowMap, bool useLightDepthBounds ) {
	if ( surfList == NULL ) {
		return;
	}

	// change the scissor if needed, it will be constant across all the surfaces lit by the light
	if ( !backEnd.currentScissor.Equals( vLight->scissorRect ) && r_useScissor.GetBool() ) {
		GL_Scissor( backEnd.viewDef->viewport.x1 + vLight->scissorRect.x1, 
					backEnd.viewDef->viewport.y1 + vLight->scissorRect.y1,
					vLight->scissorRect.x2 + 1 - vLight->scissorRect.x1,
					vLight->scissorRect.y2 + 1 - vLight->scissorRect.y1 );
		backEnd.currentScissor = vLight->scissorRect;
	}

	// perform setup here that will be constant for all interactions
	GL_State( GLS_SRCBLEND_ONE | GLS_DSTBLEND_ONE | GLS_DEPTHMASK | depthFunc );

	// some rare lights have multiple animating stages, loop over them outside the surface list
	const idMaterial * lightShader = vLight->lightShader;
	const float * lightRegs = vLight->shaderRegisters;

	drawInteraction_t inter = {};
	inter.ambientLight = lightShader->IsAmbientLight();

	//---------------------------------
	// Split out the complex surfaces from the fast-path surfaces
	// so we can do the fast path ones all in a row.
	// The surfaces should already be sorted by space because they
	// are added single-threaded, and there is only a negligable amount
	// of benefit to trying to sort by materials.
	//---------------------------------
	static const int MAX_INTERACTIONS_PER_LIGHT = 1024;
	static const int MAX_COMPLEX_INTERACTIONS_PER_LIGHT = 128;
	idStaticList< const drawSurf_t *, MAX_INTERACTIONS_PER_LIGHT > allSurfaces;
	idStaticList< const drawSurf_t *, MAX_COMPLEX_INTERACTIONS_PER_LIGHT > complexSurfaces;
	for ( const drawSurf_t * walk = surfList; walk != NULL; walk = walk->nextOnLight ) {

		const idMaterial * surfaceShader = walk->material;
		if ( surfaceShader->GetFastPathBumpImage() ) {
			allSurfaces.Append( walk );
		} else {
			complexSurfaces.Append( walk );
		}
	}
	for ( int i = 0; i < complexSurfaces.Num(); i++ ) {
		allSurfaces.Append( complexSurfaces[i] );
	}

	bool lightDepthBoundsDisabled = false;

	for ( int lightStageNum = 0; lightStageNum < lightShader->GetNumStages(); lightStageNum++ ) {
		const shaderStage_t	*lightStage = lightShader->GetStage( lightStageNum );

		// ignore stages that fail the condition
		if ( !lightRegs[ lightStage->conditionRegister ] ) {
			continue;
		}

		const float lightScale = r_lightScale.GetFloat();
		const idVec4 lightColor(
			lightScale * lightRegs[ lightStage->color.registers[0] ],
			lightScale * lightRegs[ lightStage->color.registers[1] ],
			lightScale * lightRegs[ lightStage->color.registers[2] ],
			lightRegs[ lightStage->color.registers[3] ] );
		// apply the world-global overbright and the 2x factor for specular
		const idVec4 diffuseColor = lightColor;
		const idVec4 specularColor = lightColor * 2.0f;

		float lightTextureMatrix[16];
		if ( lightStage->texture.hasMatrix ) {
			RB_GetShaderTextureMatrix( lightRegs, &lightStage->texture, lightTextureMatrix );
		}

		// texture 1 will be the light falloff texture
		GL_SelectTexture( INTERACTION_TEXUNIT_FALLOFF );
		vLight->falloffImage->Bind();

		// texture 2 will be the light projection texture
		GL_SelectTexture( INTERACTION_TEXUNIT_PROJECTION );
		lightStage->texture.image->Bind();

		// force the light textures to not use anisotropic filtering, which is wasted on them
		// all of the texture sampler parms should be constant for all interactions, only
		// the actual texture image bindings will change

		//----------------------------------
		// For all surfaces on this light list, generate an interaction for this light stage
		//----------------------------------

		// setup renderparms assuming we will be drawing trivial surfaces first
		RB_SetupForFastPathInteractions( diffuseColor, specularColor );

		// even if the space does not change between light stages, each light stage may need a different lightTextureMatrix baked in
		backEnd.currentSpace = NULL;

		for ( int sortedSurfNum = 0; sortedSurfNum < allSurfaces.Num(); sortedSurfNum++ ) {
			const drawSurf_t * const surf = allSurfaces[ sortedSurfNum ];
			const idMaterial * surfaceShader = surf->material;

			// select the render prog
			if ( lightShader->IsAmbientLight() ) {
				if ( surf->jointCache ) {
					renderProgManager.BindShader_InteractionAmbientSkinned();
				} else {
					renderProgManager.BindShader_InteractionAmbient();
				}
			} else {
				if ( surf->jointCache ) {
					renderProgManager.BindShader_InteractionSkinned();
				} else {
					renderProgManager.BindShader_Interaction();
				}
			}

			const float * surfaceRegs = surf->shaderRegisters;
			virtualTextureSystem.BindVirtualMaterial( surfaceShader );

			inter.surf = surf;

			// change the MVP matrix, view/light origin and light projection vectors if needed
			if ( surf->space != backEnd.currentSpace ) {
				backEnd.currentSpace = surf->space;

				// turn off the light depth bounds test if this model is rendered with a depth hack
				if ( useLightDepthBounds ) {
					if ( !surf->space->weaponDepthHack && surf->space->modelDepthHack == 0.0f ) {
						if ( lightDepthBoundsDisabled ) {
							GL_DepthBoundsTest( vLight->scissorRect.zmin, vLight->scissorRect.zmax );
							lightDepthBoundsDisabled = false;
						}
					} else {
						if ( !lightDepthBoundsDisabled ) {
							GL_DepthBoundsTest( 0.0f, 0.0f );
							lightDepthBoundsDisabled = true;
						}
					}
				}

				// model-view-projection
				RB_SetMVP( surf->space->mvp );

				// tranform the light/view origin into model local space
				idVec4 localLightOrigin( 0.0f );
				idVec4 localViewOrigin( 1.0f );
				R_GlobalPointToLocal( surf->space->modelMatrix, vLight->globalLightOrigin, localLightOrigin.ToVec3() );
				R_GlobalPointToLocal( surf->space->modelMatrix, backEnd.viewDef->renderView.vieworg, localViewOrigin.ToVec3() );

				// set the local light/view origin
				SetVertexParm( RENDERPARM_LOCALLIGHTORIGIN, localLightOrigin.ToFloatPtr() );
				SetVertexParm( RENDERPARM_LOCALVIEWORIGIN, localViewOrigin.ToFloatPtr() );

				// transform the light project into model local space
				idPlane lightProjection[4];
				for ( int i = 0; i < 4; i++ ) {
					R_GlobalPlaneToLocal( surf->space->modelMatrix, vLight->lightProject[i], lightProjection[i] );
				}

				// optionally multiply the local light projection by the light texture matrix
				if ( lightStage->texture.hasMatrix ) {
					RB_BakeTextureMatrixIntoTexgen( lightProjection, lightTextureMatrix );
				}

				// set the light projection
				SetVertexParm( RENDERPARM_LIGHTPROJECTION_S, lightProjection[0].ToFloatPtr() );
				SetVertexParm( RENDERPARM_LIGHTPROJECTION_T, lightProjection[1].ToFloatPtr() );
				SetVertexParm( RENDERPARM_LIGHTPROJECTION_Q, lightProjection[2].ToFloatPtr() );
				SetVertexParm( RENDERPARM_LIGHTFALLOFF_S, lightProjection[3].ToFloatPtr() );

				idRenderMatrix localShadowMatrix;
				idRenderMatrix::Multiply( currentShadowMatrix, surf->space->modelRenderMatrix, localShadowMatrix );
				SetVertexParms( RENDERPARM_SHADOWMATRIX_X, localShadowMatrix.m, 4 );
				if ( currentShadowType == 1.5f ) {
					// cascade path: the vertex shader dots the MODEL-space
					// position against rpShadowProjectionZ for the slice LOD,
					// so fold the model transform into the projection rows
					idRenderMatrix localShadowProjection;
					idRenderMatrix::Multiply( currentShadowProjection, surf->space->modelRenderMatrix, localShadowProjection );
					SetVertexParms( RENDERPARM_SHADOWPROJECTION_X, localShadowProjection.m, 4 );
				} else {
					SetVertexParms( RENDERPARM_SHADOWPROJECTION_X, currentShadowProjection.m, 4 );
				}
				const idVec4 shadowParms( useShadowMap && r_shadowMapSample.GetBool() ? currentShadowType : 0.0f, lightShadowAtlasSize > 0 ? 1.0f / lightShadowAtlasSize : 0.0f, r_shadowMapBias.GetFloat(), 0.0f );
				SetFragmentParm( RENDERPARM_SHADOWPARAMS, shadowParms.ToFloatPtr() );
			}

			// check for the fast path
			if ( surfaceShader->GetFastPathBumpImage() && !r_skipInteractionFastPath.GetBool() ) {
				renderLog.OpenBlock( surf->material->GetName() );

				// texture 0 will be the per-surface bump map
				GL_SelectTexture( INTERACTION_TEXUNIT_BUMP );
				surfaceShader->GetFastPathBumpImage()->Bind();

				// texture 3 is the per-surface diffuse map
				GL_SelectTexture( INTERACTION_TEXUNIT_DIFFUSE );
				surfaceShader->GetFastPathDiffuseImage()->Bind();

				// texture 4 is the per-surface specular map
				GL_SelectTexture( INTERACTION_TEXUNIT_SPECULAR );
				surfaceShader->GetFastPathSpecularImage()->Bind();

				RB_DrawElementsWithCounters( surf );

				renderLog.CloseBlock();
				continue;
			}
			
			renderLog.OpenBlock( surf->material->GetName() );

			inter.bumpImage = NULL;
			inter.specularImage = NULL;
			inter.diffuseImage = NULL;
			inter.diffuseColor[0] = inter.diffuseColor[1] = inter.diffuseColor[2] = inter.diffuseColor[3] = 0;
			inter.specularColor[0] = inter.specularColor[1] = inter.specularColor[2] = inter.specularColor[3] = 0;

			// go through the individual surface stages
			//
			// This is somewhat arcane because of the old support for video cards that had to render
			// interactions in multiple passes.
			//
			// We also have the very rare case of some materials that have conditional interactions
			// for the "hell writing" that can be shined on them.
			for ( int surfaceStageNum = 0; surfaceStageNum < surfaceShader->GetNumStages(); surfaceStageNum++ ) {
				const shaderStage_t	*surfaceStage = surfaceShader->GetStage( surfaceStageNum );

				switch( surfaceStage->lighting ) {
					case SL_COVERAGE: {
						// ignore any coverage stages since they should only be used for the depth fill pass
						// for diffuse stages that use alpha test.
						break;
					}
					case SL_AMBIENT: {
						// ignore ambient stages while drawing interactions
						break;
					}
					case SL_BUMP: {
						// ignore stage that fails the condition
						if ( !surfaceRegs[ surfaceStage->conditionRegister ] ) {
							break;
						}
						// draw any previous interaction
						if ( inter.bumpImage != NULL ) {
							RB_DrawSingleInteraction( &inter );
						}
						inter.bumpImage = surfaceStage->texture.image;
						inter.diffuseImage = NULL;
						inter.specularImage = NULL;
						RB_SetupInteractionStage( surfaceStage, surfaceRegs, NULL,
												inter.bumpMatrix, NULL );
						break;
					}
					case SL_DIFFUSE: {
						// ignore stage that fails the condition
						if ( !surfaceRegs[ surfaceStage->conditionRegister ] ) {
							break;
						}
						// draw any previous interaction
						if ( inter.diffuseImage != NULL ) {
							RB_DrawSingleInteraction( &inter );
						}
						inter.diffuseImage = surfaceStage->texture.image;
						inter.vertexColor = surfaceStage->vertexColor;
						RB_SetupInteractionStage( surfaceStage, surfaceRegs, diffuseColor.ToFloatPtr(),
												inter.diffuseMatrix, inter.diffuseColor.ToFloatPtr() );
						break;
					}
					case SL_SPECULAR: {
						// ignore stage that fails the condition
						if ( !surfaceRegs[ surfaceStage->conditionRegister ] ) {
							break;
						}
						// draw any previous interaction
						if ( inter.specularImage != NULL ) {
							RB_DrawSingleInteraction( &inter );
						}
						inter.specularImage = surfaceStage->texture.image;
						inter.vertexColor = surfaceStage->vertexColor;
						RB_SetupInteractionStage( surfaceStage, surfaceRegs, specularColor.ToFloatPtr(),
												inter.specularMatrix, inter.specularColor.ToFloatPtr() );
						break;
					}
				}
			}

			// draw the final interaction
			RB_DrawSingleInteraction( &inter );

			renderLog.CloseBlock();
		}
	}

	if ( useLightDepthBounds && lightDepthBoundsDisabled ) {
		GL_DepthBoundsTest( vLight->scissorRect.zmin, vLight->scissorRect.zmax );
	}

	renderProgManager.Unbind();
}

// Stencil-volume rendering was removed; opaque interactions use the depth atlas above.

/*
==============================================================================================

DRAW INTERACTIONS

==============================================================================================
*/
/*
==================
RB_DrawInteractions
==================
*/
static void RB_DrawInteractions() {
	if ( r_skipInteractions.GetBool() ) {
		return;
	}

	renderLog.OpenMainBlock( MRB_DRAW_INTERACTIONS );
	renderLog.OpenBlock( "RB_DrawInteractions" );

	GL_SelectTexture( 0 );


	const bool useLightDepthBounds = r_useLightDepthBounds.GetBool();
	int shadowMapsRendered = 0;

	//
	// for each light, perform shadowing and adding
	//
	for ( const viewLight_t * vLight = backEnd.viewDef->viewLights; vLight != NULL; vLight = vLight->next ) {
		// do fogging later
		if ( vLight->lightShader->IsFogLight() ) {
			continue;
		}
		if ( vLight->lightShader->IsBlendLight() ) {
			continue;
		}

		if ( vLight->localInteractions == NULL && vLight->globalInteractions == NULL && vLight->translucentInteractions == NULL ) {
			continue;
		}

		const idMaterial * lightShader = vLight->lightShader;
		renderLog.OpenBlock( lightShader->GetName() );

		// Tech 5 reuses one shadow destination sequentially: render this light's
		// depth atlas, consume it immediately, then overwrite it for the next
		// light.
		bool useShadowMap = false;
		if ( shadowMapsRendered < r_shadowMapMaxLights.GetInteger() ) {
			GL_DepthBoundsTest( 0.0f, 0.0f );
			useShadowMap = RB_RenderShadowMap( vLight );
			if ( useShadowMap ) {
				shadowMapsRendered++;
			}
		}

		if ( useLightDepthBounds ) {
			GL_DepthBoundsTest( vLight->scissorRect.zmin, vLight->scissorRect.zmax );
		}

		if ( vLight->localInteractions != NULL ) {
			renderLog.OpenBlock( "Local Light Interactions" );
			RB_RenderInteractions( vLight->localInteractions, vLight, GLS_DEPTHFUNC_EQUAL, useShadowMap, useLightDepthBounds );
			renderLog.CloseBlock();
		}

		if ( vLight->globalInteractions != NULL ) {
			renderLog.OpenBlock( "Global Light Interactions" );
			RB_RenderInteractions( vLight->globalInteractions, vLight, GLS_DEPTHFUNC_EQUAL, useShadowMap, useLightDepthBounds );
			renderLog.CloseBlock();
		}


		if ( vLight->translucentInteractions != NULL && !r_skipTranslucent.GetBool() ) {
			renderLog.OpenBlock( "Translucent Interactions" );

			// Disable the depth bounds test because translucent surfaces don't work with
			// the depth bounds tests since they did not write depth during the depth pass.
			if ( useLightDepthBounds ) {
				GL_DepthBoundsTest( 0.0f, 0.0f );
			}

			// The depth buffer wasn't filled in for translucent surfaces, so they
			// can never be constrained to perforated surfaces with the depthfunc equal.

			// Translucent surfaces do not contribute to the view depth buffer and
			// are shaded separately from the opaque shadow-receiving path.

			RB_RenderInteractions( vLight->translucentInteractions, vLight, GLS_DEPTHFUNC_LESS, false, false );

			renderLog.CloseBlock();
		}

		renderLog.CloseBlock();
	}

	// restore the default state after additive shadow-mapped interactions
	GL_State( GLS_DEFAULT );

	// unbind texture units
	for ( int i = 0; i < 6; i++ ) {
		GL_SelectTexture( i );
		globalImages->BindNull();
	}
	GL_SelectTexture( 0 );

	// reset depth bounds
	if ( useLightDepthBounds ) {
		GL_DepthBoundsTest( 0.0f, 0.0f );
	}

	renderLog.CloseBlock();
	renderLog.CloseMainBlock();
}

/*
=============================================================================================

NON-INTERACTION SHADER PASSES

=============================================================================================
*/

/*
=====================
RB_DrawShaderPasses

Draw non-light dependent passes

If we are rendering Guis, the drawSurf_t::sort value is a depth offset that can
be multiplied by guiEye for polarity and screenSeparation for scale.
=====================
*/
static int RB_DrawShaderPasses( const drawSurf_t * const * const drawSurfs, const int numDrawSurfs, 
									const float guiStereoScreenOffset, const int stereoEye ) {
	// only obey skipAmbient if we are rendering a view
	if ( backEnd.viewDef->viewEntitys && r_skipAmbient.GetBool() ) {
		return numDrawSurfs;
	}

	renderLog.OpenBlock( "RB_DrawShaderPasses" );

	GL_SelectTexture( 1 );
	globalImages->BindNull();

	GL_SelectTexture( 0 );

	backEnd.currentSpace = (const viewEntity_t *)1;	// using NULL makes /analyze think surf->space needs to be checked...
	float currentGuiStereoOffset = 0.0f;

	int i = 0;
	for ( ; i < numDrawSurfs; i++ ) {
		const drawSurf_t * surf = drawSurfs[i];
		const idMaterial * shader = surf->material;

		if ( !shader->HasAmbient() ) {
			continue;
		}

		if ( shader->IsPortalSky() ) {
			continue;
		}

		// some deforms may disable themselves by setting numIndexes = 0
		if ( surf->numIndexes == 0 ) {
			continue;
		}

		if ( shader->SuppressInSubview() ) {
			continue;
		}

		if ( backEnd.viewDef->isXraySubview && surf->space->entityDef ) {
			if ( surf->space->entityDef->parms.xrayIndex != 2 ) {
				continue;
			}
		}

		// we need to draw the post process shaders after we have drawn the fog lights
		if ( shader->GetSort() >= SS_POST_PROCESS && !backEnd.currentRenderCopied ) {
			break;
		}

		// if we are rendering a 3D view and the surface's eye index doesn't match 
		// the current view's eye index then we skip the surface
		// if the stereoEye value of a surface is 0 then we need to draw it for both eyes.
		const int shaderStereoEye = shader->GetStereoEye();
		const bool isEyeValid = stereoRender_swapEyes.GetBool() ? ( shaderStereoEye == stereoEye ) : ( shaderStereoEye != stereoEye );
		if ( ( stereoEye != 0 ) && ( shaderStereoEye != 0 ) && ( isEyeValid ) ) {
			continue;
		}

		renderLog.OpenBlock( shader->GetName() );

		// determine the stereoDepth offset 
		// guiStereoScreenOffset will always be zero for 3D views, so the !=
		// check will never force an update due to the current sort value.
		const float thisGuiStereoOffset = guiStereoScreenOffset * surf->sort;

		// change the matrix and other space related vars if needed
		if ( surf->space != backEnd.currentSpace || thisGuiStereoOffset != currentGuiStereoOffset ) {
			backEnd.currentSpace = surf->space;
			currentGuiStereoOffset = thisGuiStereoOffset;

			const viewEntity_t *space = backEnd.currentSpace;

			if ( guiStereoScreenOffset != 0.0f ) {
				RB_SetMVPWithStereoOffset( space->mvp, currentGuiStereoOffset );
			} else {
				RB_SetMVP( space->mvp );
			}

			// set eye position in local space
			idVec4 localViewOrigin( 1.0f );
			R_GlobalPointToLocal( space->modelMatrix, backEnd.viewDef->renderView.vieworg, localViewOrigin.ToVec3() );
			SetVertexParm( RENDERPARM_LOCALVIEWORIGIN, localViewOrigin.ToFloatPtr() );

			// set model Matrix
			float modelMatrixTranspose[16];
			R_MatrixTranspose( space->modelMatrix, modelMatrixTranspose );
			SetVertexParms( RENDERPARM_MODELMATRIX_X, modelMatrixTranspose, 4 );

			// Set ModelView Matrix
			float modelViewMatrixTranspose[16];
			R_MatrixTranspose( space->modelViewMatrix, modelViewMatrixTranspose );
			SetVertexParms( RENDERPARM_MODELVIEWMATRIX_X, modelViewMatrixTranspose, 4 );
		}

		// change the scissor if needed
		if ( !backEnd.currentScissor.Equals( surf->scissorRect ) && r_useScissor.GetBool() ) {
			GL_Scissor( backEnd.viewDef->viewport.x1 + surf->scissorRect.x1, 
						backEnd.viewDef->viewport.y1 + surf->scissorRect.y1,
						surf->scissorRect.x2 + 1 - surf->scissorRect.x1,
						surf->scissorRect.y2 + 1 - surf->scissorRect.y1 );
			backEnd.currentScissor = surf->scissorRect;
		}

		// get the expressions for conditionals / color / texcoords
		const float	*regs = surf->shaderRegisters;

		// set face culling appropriately
		if ( surf->space->isGuiSurface ) {
			GL_Cull( CT_TWO_SIDED );
		} else {
			GL_Cull( shader->GetCullType() );
		}

		uint64 surfGLState = surf->extraGLState;

		// set polygon offset if necessary
		if ( shader->TestMaterialFlag(MF_POLYGONOFFSET) ) {
			GL_PolygonOffset( r_offsetFactor.GetFloat(), r_offsetUnits.GetFloat() * shader->GetPolygonOffset() );
			surfGLState = GLS_POLYGON_OFFSET;
		}

		for ( int stage = 0; stage < shader->GetNumStages(); stage++ ) {		
			const shaderStage_t *pStage = shader->GetStage(stage);

			// check the enable condition
			if ( regs[ pStage->conditionRegister ] == 0 ) {
				continue;
			}

			// skip the stages involved in lighting
			if ( pStage->lighting != SL_AMBIENT ) {
				continue;
			}

			uint64 stageGLState = surfGLState;
			if ( ( surfGLState & GLS_OVERRIDE ) == 0 ) {
				stageGLState |= pStage->drawStateBits;
			}

			// skip if the stage is ( GL_ZERO, GL_ONE ), which is used for some alpha masks
			if ( ( stageGLState & ( GLS_SRCBLEND_BITS | GLS_DSTBLEND_BITS ) ) == ( GLS_SRCBLEND_ZERO | GLS_DSTBLEND_ONE ) ) {
				continue;
			}

			// see if we are a new-style stage
			newShaderStage_t *newStage = pStage->newStage;
			if ( newStage != NULL ) {
				//--------------------------
				//
				// new style stages
				//
				//--------------------------
				if ( r_skipNewAmbient.GetBool() ) {
					continue;
				}
				renderLog.OpenBlock( "New Shader Stage" );

				GL_State( stageGLState );
			
				renderProgManager.BindShader( newStage->glslProgram );

				for ( int j = 0; j < newStage->numVertexParms; j++ ) {
					float parm[4];
					parm[0] = regs[ newStage->vertexParms[j][0] ];
					parm[1] = regs[ newStage->vertexParms[j][1] ];
					parm[2] = regs[ newStage->vertexParms[j][2] ];
					parm[3] = regs[ newStage->vertexParms[j][3] ];
					SetVertexParm( (renderParm_t)( RENDERPARM_USER + j ), parm );
				}

				// set rpEnableSkinning if the shader has optional support for skinning
				if ( surf->jointCache && renderProgManager.ShaderHasOptionalSkinning() ) {
					const idVec4 skinningParm( 1.0f );
					SetVertexParm( RENDERPARM_ENABLE_SKINNING, skinningParm.ToFloatPtr() );
				}

				// new stages that sample _currentRender need the current framebuffer
				// resolved first, the regular resolve only happens later for
				// post-process sorts so the texture would still hold stale data here
				bool needsCurrentRender = false;
				for ( int j = 0; j < newStage->numFragmentProgramImages; j++ ) {
					if ( newStage->fragmentProgramImages[j] == globalImages->currentRenderImage ) {
						needsCurrentRender = true;
						break;
					}
				}
				if ( needsCurrentRender && !backEnd.currentRenderCopied ) {
					const idScreenRect & viewport = backEnd.viewDef->viewport;
					const int w = viewport.x2 - viewport.x1 + 1;
					const int h = viewport.y2 - viewport.y1 + 1;

					GL_SelectTexture( 0 );
					globalImages->currentRenderImage->CopyFramebuffer( viewport.x1, viewport.y1, w, h );
					backEnd.currentRenderCopied = true;

					// screen power of two correction factor (no longer relevant now)
					float screenCorrectionParm[4];
					screenCorrectionParm[0] = 1.0f;
					screenCorrectionParm[1] = 1.0f;
					screenCorrectionParm[2] = 0.0f;
					screenCorrectionParm[3] = 1.0f;
					SetFragmentParm( RENDERPARM_SCREENCORRECTIONFACTOR, screenCorrectionParm ); // rpScreenCorrectionFactor

					// window coord to 0.0 to 1.0 conversion
					float windowCoordParm[4];
					windowCoordParm[0] = 1.0f / w;
					windowCoordParm[1] = 1.0f / h;
					windowCoordParm[2] = 0.0f;
					windowCoordParm[3] = 1.0f;
					SetFragmentParm( RENDERPARM_WINDOWCOORD, windowCoordParm ); // rpWindowCoord
				}

				// bind texture units
				for ( int j = 0; j < newStage->numFragmentProgramImages; j++ ) {
					idImage * image = newStage->fragmentProgramImages[j];
					if ( image != NULL ) {
						GL_SelectTexture( j );
						image->Bind();
						if ( image == globalImages->currentRenderImage ) {
							// _currentRender feeds refraction surfaces; a stale
							// per-unit binding cache would silently sample whatever
							// last touched this unit, so always issue the real bind
							GL_BindMultiTexture( j, TEX_TARGET_2D, image->GetAPIObject() );
						}
					}
				}

				// draw it
				RB_DrawElementsWithCounters( surf );

				// unbind texture units
				for ( int j = 0; j < newStage->numFragmentProgramImages; j++ ) {
					idImage * image = newStage->fragmentProgramImages[j];
					if ( image != NULL ) {
						GL_SelectTexture( j );
						globalImages->BindNull();
					}
				}

				// clear rpEnableSkinning if it was set
				if ( surf->jointCache && renderProgManager.ShaderHasOptionalSkinning() ) {
					const idVec4 skinningParm( 0.0f );
					SetVertexParm( RENDERPARM_ENABLE_SKINNING, skinningParm.ToFloatPtr() );
				}

				GL_SelectTexture( 0 );
				renderProgManager.Unbind();

				renderLog.CloseBlock();
				continue;
			}

			//--------------------------
			//
			// old style stages
			//
			//--------------------------

			// set the color
			float color[4];
			color[0] = regs[ pStage->color.registers[0] ];
			color[1] = regs[ pStage->color.registers[1] ];
			color[2] = regs[ pStage->color.registers[2] ];
			color[3] = regs[ pStage->color.registers[3] ];

			// skip the entire stage if an add would be black
			if ( ( stageGLState & ( GLS_SRCBLEND_BITS | GLS_DSTBLEND_BITS ) ) == ( GLS_SRCBLEND_ONE | GLS_DSTBLEND_ONE ) 
				&& color[0] <= 0 && color[1] <= 0 && color[2] <= 0 ) {
				continue;
			}

			// skip the entire stage if a blend would be completely transparent
			if ( ( stageGLState & ( GLS_SRCBLEND_BITS | GLS_DSTBLEND_BITS ) ) == ( GLS_SRCBLEND_SRC_ALPHA | GLS_DSTBLEND_ONE_MINUS_SRC_ALPHA )
				&& color[3] <= 0 ) {
				continue;
			}

			stageVertexColor_t svc = pStage->vertexColor;

			renderLog.OpenBlock( "Old Shader Stage" );
			GL_Color( color );

			if ( surf->space->isGuiSurface ) {
				// Force gui surfaces to always be SVC_MODULATE
				svc = SVC_MODULATE;

				// use special shaders for bink cinematics
				if ( pStage->texture.cinematic ) {
					if ( ( stageGLState & GLS_OVERRIDE ) != 0 ) {
						// This is a hack... Only SWF Guis set GLS_OVERRIDE
						// Old style guis do not, and we don't want them to use the new GUI renederProg
						renderProgManager.BindShader_BinkGUI();
					} else {
						renderProgManager.BindShader_Bink();
					}
				} else {
					if ( ( stageGLState & GLS_OVERRIDE ) != 0 ) {
						// This is a hack... Only SWF Guis set GLS_OVERRIDE
						// Old style guis do not, and we don't want them to use the new GUI renderProg
						renderProgManager.BindShader_GUI();
					} else {
						if ( surf->jointCache ) {
							renderProgManager.BindShader_TextureVertexColorSkinned();
						} else {
							renderProgManager.BindShader_TextureVertexColor();
						}
					}
				}
			} else if ( ( pStage->texture.texgen == TG_SCREEN ) || ( pStage->texture.texgen == TG_SCREEN2 ) ) {
				renderProgManager.BindShader_TextureTexGenVertexColor();
			} else if ( pStage->texture.cinematic ) {
				renderProgManager.BindShader_Bink();
			} else {
				if ( surf->jointCache ) {
					renderProgManager.BindShader_TextureVertexColorSkinned();
				} else {
					renderProgManager.BindShader_TextureVertexColor();
				}
			}
		
			RB_SetVertexColorParms( svc );

			// bind the texture
			RB_BindVariableStageImage( &pStage->texture, regs );

			// set privatePolygonOffset if necessary
			if ( pStage->privatePolygonOffset ) {
				GL_PolygonOffset( r_offsetFactor.GetFloat(), r_offsetUnits.GetFloat() * pStage->privatePolygonOffset );
				stageGLState |= GLS_POLYGON_OFFSET;
			}

			// set the state
			GL_State( stageGLState );
		
			RB_PrepareStageTexturing( pStage, surf );

			// draw it
			RB_DrawElementsWithCounters( surf );

			RB_FinishStageTexturing( pStage, surf );

			// unset privatePolygonOffset if necessary
			if ( pStage->privatePolygonOffset ) {
				GL_PolygonOffset( r_offsetFactor.GetFloat(), r_offsetUnits.GetFloat() * shader->GetPolygonOffset() );
			}
			renderLog.CloseBlock();
		}

		renderLog.CloseBlock();
	}

	GL_Cull( CT_FRONT_SIDED );
	GL_Color( 1.0f, 1.0f, 1.0f );

	renderLog.CloseBlock();
	return i;
}

/*
=============================================================================================

BLEND LIGHT PROJECTION

=============================================================================================
*/

/*
=====================
RB_T_BlendLight
=====================
*/
static void RB_T_BlendLight( const drawSurf_t *drawSurfs, const viewLight_t * vLight ) {
	backEnd.currentSpace = NULL;

	for ( const drawSurf_t * drawSurf = drawSurfs; drawSurf != NULL; drawSurf = drawSurf->nextOnLight ) {
		if ( drawSurf->scissorRect.IsEmpty() ) {
			continue;	// !@# FIXME: find out why this is sometimes being hit!
						// temporarily jump over the scissor and draw so the gl error callback doesn't get hit
		}

		if ( !backEnd.currentScissor.Equals( drawSurf->scissorRect ) && r_useScissor.GetBool() ) {
			// change the scissor
			GL_Scissor( backEnd.viewDef->viewport.x1 + drawSurf->scissorRect.x1,
						backEnd.viewDef->viewport.y1 + drawSurf->scissorRect.y1,
						drawSurf->scissorRect.x2 + 1 - drawSurf->scissorRect.x1,
						drawSurf->scissorRect.y2 + 1 - drawSurf->scissorRect.y1 );
			backEnd.currentScissor = drawSurf->scissorRect;
		}

		if ( drawSurf->space != backEnd.currentSpace ) {
			// change the matrix
			RB_SetMVP( drawSurf->space->mvp );

			// change the light projection matrix
			idPlane	lightProjectInCurrentSpace[4];
			for ( int i = 0; i < 4; i++ ) {
				R_GlobalPlaneToLocal( drawSurf->space->modelMatrix, vLight->lightProject[i], lightProjectInCurrentSpace[i] );
			}

			SetVertexParm( RENDERPARM_TEXGEN_0_S, lightProjectInCurrentSpace[0].ToFloatPtr() );
			SetVertexParm( RENDERPARM_TEXGEN_0_T, lightProjectInCurrentSpace[1].ToFloatPtr() );
			SetVertexParm( RENDERPARM_TEXGEN_0_Q, lightProjectInCurrentSpace[2].ToFloatPtr() );
			SetVertexParm( RENDERPARM_TEXGEN_1_S, lightProjectInCurrentSpace[3].ToFloatPtr() );	// falloff

			backEnd.currentSpace = drawSurf->space;
		}

		RB_DrawElementsWithCounters( drawSurf );
	}
}

/*
=====================
RB_BlendLight

Dual texture together the falloff and projection texture with a blend
mode to the framebuffer, instead of interacting with the surface texture
=====================
*/
static void RB_BlendLight( const drawSurf_t *drawSurfs, const drawSurf_t *drawSurfs2, const viewLight_t * vLight ) {
	if ( drawSurfs == NULL ) {
		return;
	}
	if ( r_skipBlendLights.GetBool() ) {
		return;
	}
	renderLog.OpenBlock( vLight->lightShader->GetName() );

	const idMaterial * lightShader = vLight->lightShader;
	const float	* regs = vLight->shaderRegisters;

	// texture 1 will get the falloff texture
	GL_SelectTexture( 1 );
	vLight->falloffImage->Bind();

	// texture 0 will get the projected texture
	GL_SelectTexture( 0 );

	renderProgManager.BindShader_BlendLight();

	for ( int i = 0; i < lightShader->GetNumStages(); i++ ) {
		const shaderStage_t	*stage = lightShader->GetStage(i);

		if ( !regs[ stage->conditionRegister ] ) {
			continue;
		}

		GL_State( GLS_DEPTHMASK | stage->drawStateBits | GLS_DEPTHFUNC_EQUAL );

		GL_SelectTexture( 0 );
		stage->texture.image->Bind();

		if ( stage->texture.hasMatrix ) {
			RB_LoadShaderTextureMatrix( regs, &stage->texture );
		}

		// get the modulate values from the light, including alpha, unlike normal lights
		float lightColor[4];
		lightColor[0] = regs[ stage->color.registers[0] ];
		lightColor[1] = regs[ stage->color.registers[1] ];
		lightColor[2] = regs[ stage->color.registers[2] ];
		lightColor[3] = regs[ stage->color.registers[3] ];
		GL_Color( lightColor );

		RB_T_BlendLight( drawSurfs, vLight );
		RB_T_BlendLight( drawSurfs2, vLight );
	}

	GL_SelectTexture( 1 );
	globalImages->BindNull();

	GL_SelectTexture( 0 );

	renderProgManager.Unbind();
	renderLog.CloseBlock();
}

/*
========================
RB_DrawVirtualTextureFeedback

Renders compact page IDs after the color pass.  Readback is double buffered,
so the CPU analyzes the previous feedback image while the GPU fills the next.
========================
*/
static void RB_DrawVirtualTextureFeedback( drawSurf_t ** drawSurfs, int numDrawSurfs ) {
	static bool reportedDrawCount = false;
	static int feedbackIntervalCounter = 0;
	const int feedbackInterval = Max( 1, r_virtualTextureFeedbackInterval.GetInteger() );
	if ( ( feedbackIntervalCounter++ % feedbackInterval ) != 0 ) {
		return;
	}
	if ( !virtualTextureSystem.BeginFeedbackPass() ) {
		return;
	}
	const uint64 previousState = GL_GetCurrentState();
	const int previousCullType = backEnd.glState.faceCulling;
	// Build visibility directly in the low-resolution feedback target. Depth
	// writes are enabled so only the closest opaque VMTR surface owns each pixel.
	GL_State( GLS_DEPTHFUNC_LESS, true );
	GL_Cull( CT_TWO_SIDED );
	backEnd.currentSpace = NULL;
	int feedbackDraws = 0;
	for ( int i = 0; i < numDrawSurfs; ++i ) {
		const drawSurf_t * surf = drawSurfs[i];
		if ( surf == NULL || surf->material == NULL ||
				surf->material->Coverage() == MC_TRANSLUCENT ) {
			continue;
		}
		idVirtualMaterial * vmtr = virtualTextureSystem.VirtualMaterialForMaterial( surf->material );
		if ( vmtr == NULL ) {
			continue;
		}
		if ( surf->jointCache ) {
			renderProgManager.BindShader_VirtualTextureFeedbackSkinned();
		} else {
			renderProgManager.BindShader_VirtualTextureFeedback();
		}
		if ( surf->space != backEnd.currentSpace ) {
			backEnd.currentSpace = surf->space;
			RB_SetMVP( surf->space->mvp );
		}
		const idVec4 feedbackParms = virtualTextureSystem.FeedbackParms( vmtr );
		renderProgManager.SetUniformValue( static_cast< renderParm_t >( RENDERPARM_USER + 0 ),
			vmtr->scaleAndBias.ToFloatPtr() );
		renderProgManager.SetUniformValue( static_cast< renderParm_t >( RENDERPARM_USER + 1 ),
			feedbackParms.ToFloatPtr() );
		RB_DrawElementsWithCounters( surf );
		++feedbackDraws;
	}
	if ( !reportedDrawCount ) {
		idLib::Printf( "VT runtime: feedback pass submitted %i VMTR surfaces\n", feedbackDraws );
		reportedDrawCount = true;
	}
	renderProgManager.Unbind();
	virtualTextureSystem.EndFeedbackPass();
	// Feedback is an auxiliary pass and must be invisible to whatever follows
	// it. In particular, its no-depth-write/two-sided state must not become the
	// starting state for a translucent or post-process material.
	GL_State( previousState, true );
	GL_Cull( previousCullType );
}

/*
=========================================================================================================

FOG LIGHTS

=========================================================================================================
*/

/*
=====================
RB_T_BasicFog
=====================
*/
static void RB_T_BasicFog( const drawSurf_t *drawSurfs, const idPlane fogPlanes[4], const idRenderMatrix * inverseBaseLightProject ) {
	backEnd.currentSpace = NULL;

	for ( const drawSurf_t * drawSurf = drawSurfs; drawSurf != NULL; drawSurf = drawSurf->nextOnLight ) {
		if ( drawSurf->scissorRect.IsEmpty() ) {
			continue;	// !@# FIXME: find out why this is sometimes being hit!
						// temporarily jump over the scissor and draw so the gl error callback doesn't get hit
		}

		if ( !backEnd.currentScissor.Equals( drawSurf->scissorRect ) && r_useScissor.GetBool() ) {
			// change the scissor
			GL_Scissor( backEnd.viewDef->viewport.x1 + drawSurf->scissorRect.x1,
						backEnd.viewDef->viewport.y1 + drawSurf->scissorRect.y1,
						drawSurf->scissorRect.x2 + 1 - drawSurf->scissorRect.x1,
						drawSurf->scissorRect.y2 + 1 - drawSurf->scissorRect.y1 );
			backEnd.currentScissor = drawSurf->scissorRect;
		}

		if ( drawSurf->space != backEnd.currentSpace ) {
			idPlane localFogPlanes[4];
			if ( inverseBaseLightProject == NULL ) {
				RB_SetMVP( drawSurf->space->mvp );
				for ( int i = 0; i < 4; i++ ) {
					R_GlobalPlaneToLocal( drawSurf->space->modelMatrix, fogPlanes[i], localFogPlanes[i] );
				}
			} else {
				idRenderMatrix invProjectMVPMatrix;
				idRenderMatrix::Multiply( backEnd.viewDef->worldSpace.mvp, *inverseBaseLightProject, invProjectMVPMatrix );
				RB_SetMVP( invProjectMVPMatrix );
				for ( int i = 0; i < 4; i++ ) {
					inverseBaseLightProject->InverseTransformPlane( fogPlanes[i], localFogPlanes[i], false );
				}
			}

			SetVertexParm( RENDERPARM_TEXGEN_0_S, localFogPlanes[0].ToFloatPtr() );
			SetVertexParm( RENDERPARM_TEXGEN_0_T, localFogPlanes[1].ToFloatPtr() );
			SetVertexParm( RENDERPARM_TEXGEN_1_T, localFogPlanes[2].ToFloatPtr() );
			SetVertexParm( RENDERPARM_TEXGEN_1_S, localFogPlanes[3].ToFloatPtr() );

			backEnd.currentSpace = ( inverseBaseLightProject == NULL ) ? drawSurf->space : NULL;
		}

		if ( drawSurf->jointCache ) {
			renderProgManager.BindShader_FogSkinned();
		} else {
			renderProgManager.BindShader_Fog();
		}

		RB_DrawElementsWithCounters( drawSurf );
	}
}

/*
==================
RB_FogPass
==================
*/
static void RB_FogPass( const drawSurf_t * drawSurfs,  const drawSurf_t * drawSurfs2, const viewLight_t * vLight ) {
	renderLog.OpenBlock( vLight->lightShader->GetName() );

	// find the current color and density of the fog
	const idMaterial * lightShader = vLight->lightShader;
	const float * regs = vLight->shaderRegisters;
	// assume fog shaders have only a single stage
	const shaderStage_t * stage = lightShader->GetStage( 0 );

	float lightColor[4];
	lightColor[0] = regs[ stage->color.registers[0] ];
	lightColor[1] = regs[ stage->color.registers[1] ];
	lightColor[2] = regs[ stage->color.registers[2] ];
	lightColor[3] = regs[ stage->color.registers[3] ];

	GL_Color( lightColor );

	// calculate the falloff planes
	float a;

	// if they left the default value on, set a fog distance of 500
	if ( lightColor[3] <= 1.0f ) {
		a = -0.5f / DEFAULT_FOG_DISTANCE;
	} else {
		// otherwise, distance = alpha color
		a = -0.5f / lightColor[3];
	}

	// texture 0 is the falloff image
	GL_SelectTexture( 0 );
	globalImages->fogImage->Bind();

	// texture 1 is the entering plane fade correction
	GL_SelectTexture( 1 );
	globalImages->fogEnterImage->Bind();

	// S is based on the view origin
	const float s = vLight->fogPlane.Distance( backEnd.viewDef->renderView.vieworg );

	const float FOG_SCALE = 0.001f;

	idPlane fogPlanes[4];

	// S-0
	fogPlanes[0][0] = a * backEnd.viewDef->worldSpace.modelViewMatrix[0*4+2];
	fogPlanes[0][1] = a * backEnd.viewDef->worldSpace.modelViewMatrix[1*4+2];
	fogPlanes[0][2] = a * backEnd.viewDef->worldSpace.modelViewMatrix[2*4+2];
	fogPlanes[0][3] = a * backEnd.viewDef->worldSpace.modelViewMatrix[3*4+2] + 0.5f;

	// T-0
	fogPlanes[1][0] = 0.0f;//a * backEnd.viewDef->worldSpace.modelViewMatrix[0*4+0];
	fogPlanes[1][1] = 0.0f;//a * backEnd.viewDef->worldSpace.modelViewMatrix[1*4+0];
	fogPlanes[1][2] = 0.0f;//a * backEnd.viewDef->worldSpace.modelViewMatrix[2*4+0];
	fogPlanes[1][3] = 0.5f;//a * backEnd.viewDef->worldSpace.modelViewMatrix[3*4+0] + 0.5f;

	// T-1 will get a texgen for the fade plane, which is always the "top" plane on unrotated lights
	fogPlanes[2][0] = FOG_SCALE * vLight->fogPlane[0];
	fogPlanes[2][1] = FOG_SCALE * vLight->fogPlane[1];
	fogPlanes[2][2] = FOG_SCALE * vLight->fogPlane[2];
	fogPlanes[2][3] = FOG_SCALE * vLight->fogPlane[3] + FOG_ENTER;

	// S-1
	fogPlanes[3][0] = 0.0f;
	fogPlanes[3][1] = 0.0f;
	fogPlanes[3][2] = 0.0f;
	fogPlanes[3][3] = FOG_SCALE * s + FOG_ENTER;

	// draw it
	GL_State( GLS_DEPTHMASK | GLS_SRCBLEND_SRC_ALPHA | GLS_DSTBLEND_ONE_MINUS_SRC_ALPHA | GLS_DEPTHFUNC_EQUAL );
	RB_T_BasicFog( drawSurfs, fogPlanes, NULL );
	RB_T_BasicFog( drawSurfs2, fogPlanes, NULL );

	// the light frustum bounding planes aren't in the depth buffer, so use depthfunc_less instead
	// of depthfunc_equal
	GL_State( GLS_DEPTHMASK | GLS_SRCBLEND_SRC_ALPHA | GLS_DSTBLEND_ONE_MINUS_SRC_ALPHA | GLS_DEPTHFUNC_LESS );
	GL_Cull( CT_BACK_SIDED );

	backEnd.zeroOneCubeSurface.space = &backEnd.viewDef->worldSpace;
	backEnd.zeroOneCubeSurface.scissorRect = backEnd.viewDef->scissor;
	RB_T_BasicFog( &backEnd.zeroOneCubeSurface, fogPlanes, &vLight->inverseBaseLightProject );

	GL_Cull( CT_FRONT_SIDED );

	GL_SelectTexture( 1 );
	globalImages->BindNull();

	GL_SelectTexture( 0 );

	renderProgManager.Unbind();

	renderLog.CloseBlock();
}

/*
==================
RB_FogAllLights
==================
*/
static void RB_FogAllLights() {
	if ( r_skipFogLights.GetBool() || r_showOverDraw.GetInteger() != 0 
		 || backEnd.viewDef->isXraySubview /* don't fog in xray mode*/ ) {
		return;
	}
	renderLog.OpenMainBlock( MRB_FOG_ALL_LIGHTS );
	renderLog.OpenBlock( "RB_FogAllLights" );

	// force fog plane to recalculate
	backEnd.currentSpace = NULL;

	for ( viewLight_t * vLight = backEnd.viewDef->viewLights; vLight != NULL; vLight = vLight->next ) {
		if ( vLight->lightShader->IsFogLight() ) {
			RB_FogPass( vLight->globalInteractions, vLight->localInteractions, vLight );
		} else if ( vLight->lightShader->IsBlendLight() ) {
			RB_BlendLight( vLight->globalInteractions, vLight->localInteractions, vLight );
		}
	}

	renderLog.CloseBlock();
	renderLog.CloseMainBlock();
}

/*
=========================================================================================================

BACKEND COMMANDS

=========================================================================================================
*/

/*
========================
RB_RunOcclusionQueries

TNO style occlusion culling: draw the bounds of every entity in the view
with a samples-passed query around it, testing against the depth buffer
that was just filled. The front end portal walk consumes the results on a
later frame and skips entities whose box failed the depth test.
========================
*/
static void RB_RunOcclusionQueries() {
	if ( !r_occlusionQuery.GetBool() || !glConfig.occlusionQueryAvailable ) {
		return;
	}
	// subviews share the query objects with the main view, only run there
	if ( backEnd.viewDef->isSubview || backEnd.viewDef->viewEntitys == NULL ) {
		return;
	}

	renderLog.OpenBlock( "RB_RunOcclusionQueries" );

	// collect results that became available, never stall the GPU:
	// a query that is not ready yet keeps its previous result
	for ( int i = 0; i < MAX_OCCLUSION_QUERIES; i++ ) {
		if ( occlusionQueryState.pending[i] && GL_QueryResultAvailable( occlusionQueryState.queryIds[i] ) ) {
			GL_GetQueryObjectuiv( occlusionQueryState.queryIds[i], &occlusionQueryState.results[i] );
			occlusionQueryState.pending[i] = false;
		}
	}

	// draw the bounds boxes depth only, no color or depth writes
	GL_State( GLS_DEPTHFUNC_LESS | GLS_DEPTHMASK | GLS_COLORMASK | GLS_ALPHAMASK );
	GL_Cull( CT_TWO_SIDED );
	renderProgManager.BindShader_Depth();

	for ( const viewEntity_t * vEntity = backEnd.viewDef->viewEntitys; vEntity != NULL; vEntity = vEntity->next ) {
		const int index = vEntity->occlusionIndex;
		if ( index < 0 || index >= MAX_OCCLUSION_QUERIES ) {
			continue;
		}
		unsigned int & queryId = occlusionQueryState.queryIds[index];
		if ( queryId == 0 ) {
			GL_GenQueries( 1, &queryId );
		}
		if ( occlusionQueryState.pending[index] ) {
			// previous query still in flight, keep the old result this frame
			continue;
		}
		RB_SetMVP( vEntity->occlusionMVP );
		GL_BeginQuery( QUERY_TARGET_SAMPLES_PASSED, queryId );
		RB_DrawElementsWithCounters( &backEnd.zeroOneCubeSurface );
		GL_EndQuery( QUERY_TARGET_SAMPLES_PASSED );
		occlusionQueryState.pending[index] = true;
		occlusionQueryState.queryFrames[index] = tr.frameCount;
	}

	// restore the state the following passes expect
	GL_Cull( CT_FRONT_SIDED );
	GL_State( GLS_DEFAULT );

	renderLog.CloseBlock();
}

/*
==================
RB_DrawViewInternal
==================
*/
void RB_DrawViewInternal( const viewDef_t * viewDef, const int stereoEye ) {
	renderLog.OpenBlock( "RB_DrawViewInternal" );

	//-------------------------------------------------
	// guis can wind up referencing purged images that need to be loaded.
	// this used to be in the gui emit code, but now that it can be running
	// in a separate thread, it must not try to load images, so do it here.
	//-------------------------------------------------
	drawSurf_t **drawSurfs = (drawSurf_t **)&viewDef->drawSurfs[0];
	const int numDrawSurfs = viewDef->numDrawSurfs;

	for ( int i = 0; i < numDrawSurfs; i++ ) {
		const drawSurf_t * ds = viewDef->drawSurfs[ i ];
		if ( ds->material != NULL ) {
			const_cast<idMaterial *>( ds->material )->EnsureNotPurged();
		}
	}

	//-------------------------------------------------
	// RB_BeginDrawingView
	//
	// Any mirrored or portaled views have already been drawn, so prepare
	// to actually render the visible surfaces for this view
	//
	// clear the z buffer, set the projection matrix, etc
	//-------------------------------------------------

	// set the window clipping
	GL_Viewport( viewDef->viewport.x1,
		viewDef->viewport.y1,
		viewDef->viewport.x2 + 1 - viewDef->viewport.x1,
		viewDef->viewport.y2 + 1 - viewDef->viewport.y1 );

	// the scissor may be smaller than the viewport for subviews
	GL_Scissor( backEnd.viewDef->viewport.x1 + viewDef->scissor.x1,
				backEnd.viewDef->viewport.y1 + viewDef->scissor.y1,
				viewDef->scissor.x2 + 1 - viewDef->scissor.x1,
				viewDef->scissor.y2 + 1 - viewDef->scissor.y1 );
	backEnd.currentScissor = viewDef->scissor;

	backEnd.glState.faceCulling = -1;		// force face culling to set next time

	// ensures that depth writes are enabled for the depth clear
	GL_State( GLS_DEFAULT );


	// SWF masks use 128 as their neutral stencil value.
	GL_Clear( false, true, true, STENCIL_BASE_VALUE, 0.0f, 0.0f, 0.0f, 0.0f );

	// normal face culling
	GL_Cull( CT_FRONT_SIDED );

	// bind one global Vertex Array Object (VAO)
	GL_BindVertexArray( glConfig.global_vao );

	// Allocate persistent shadow resources before any view geometry is drawn.
	// idImage allocation invalidates the renderer's texture binding caches, so
	// creating the atlas lazily from the interaction pass would disrupt a view
	// whose depth and ambient setup was already in progress.
	if ( r_shadowMapEnable.GetBool() ) {
		RB_EnsureShadowAtlas();
		GL_SelectTexture( 0 );
	}
	// Allocate VT render targets before depth/ambient rendering. Scratch-image
	// creation changes GL texture bindings, so it must not happen mid-pass.
	virtualTextureSystem.EnsureRuntimeResources();
	GL_SelectTexture( 0 );

	//------------------------------------
	// sets variables that can be used by all programs
	//------------------------------------
	{
		//
		// set eye position in global space
		//
		float parm[4];
		parm[0] = backEnd.viewDef->renderView.vieworg[0];
		parm[1] = backEnd.viewDef->renderView.vieworg[1];
		parm[2] = backEnd.viewDef->renderView.vieworg[2];
		parm[3] = 1.0f;

		SetVertexParm( RENDERPARM_GLOBALEYEPOS, parm ); // rpGlobalEyePos

		// sets overbright to make world brighter
		// This value is baked into the specularScale and diffuseScale values so
		// the interaction programs don't need to perform the extra multiply,
		// but any other renderprogs that want to obey the brightness value
		// can reference this.
		float overbright = r_lightScale.GetFloat() * 0.5f;
		parm[0] = overbright;
		parm[1] = overbright;
		parm[2] = overbright;
		parm[3] = overbright;
		SetFragmentParm( RENDERPARM_OVERBRIGHT, parm );

		// Set Projection Matrix
		float projMatrixTranspose[16];
		R_MatrixTranspose( backEnd.viewDef->projectionMatrix, projMatrixTranspose );
		SetVertexParms( RENDERPARM_PROJMATRIX_X, projMatrixTranspose, 4 );
	}

	//-------------------------------------------------
	// fill the depth buffer and clear color buffer to black except on subviews
	//-------------------------------------------------
	RB_FillDepthBufferFast( drawSurfs, numDrawSurfs );

	//-------------------------------------------------
	// TNO style occlusion queries against the filled depth buffer,
	// main view only so the query objects are never reused in a frame
	//-------------------------------------------------
	if ( stereoEye == 0 ) {
		RB_RunOcclusionQueries();
	}

	//-------------------------------------------------
	// main light renderer
	//-------------------------------------------------
	//-------------------------------------------------
	// TNO style dim shadows: soft per model shadows from the prime
	// light, blended against the freshly filled depth
	//-------------------------------------------------
	RB_RenderDimShadows();

	RB_DrawInteractions();

	//-------------------------------------------------
	// now draw any non-light dependent shading passes
	//-------------------------------------------------
	int processed = 0;
	if ( !r_skipShaderPasses.GetBool() ) {
		renderLog.OpenMainBlock( MRB_DRAW_SHADER_PASSES );
		float guiScreenOffset;
		if ( viewDef->viewEntitys != NULL ) {
			// guiScreenOffset will be 0 in non-gui views
			guiScreenOffset = 0.0f;
		} else {
			guiScreenOffset = stereoEye * viewDef->renderView.stereoScreenSeparation;
		}
		processed = RB_DrawShaderPasses( drawSurfs, numDrawSurfs, guiScreenOffset, stereoEye );
		renderLog.CloseMainBlock();
	}

	//-------------------------------------------------
	// fog and blend lights, drawn after emissive surfaces
	// so they are properly dimmed down
	//-------------------------------------------------
	RB_FogAllLights();

	//-------------------------------------------------
	// capture the depth for the motion blur before rendering any post process surfaces that may contribute to the depth
	//-------------------------------------------------
	if ( r_motionBlur.GetInteger() > 0 ) {
		const idScreenRect & viewport = backEnd.viewDef->viewport;
		globalImages->currentDepthImage->CopyDepthbuffer( viewport.x1, viewport.y1, viewport.GetWidth(), viewport.GetHeight() );
	}

	//-------------------------------------------------
	// now draw any screen warping post-process effects using _currentRender
	//-------------------------------------------------
	if ( processed < numDrawSurfs && !r_skipPostProcess.GetBool() ) {
		int x = backEnd.viewDef->viewport.x1;
		int y = backEnd.viewDef->viewport.y1;
		int	w = backEnd.viewDef->viewport.x2 - backEnd.viewDef->viewport.x1 + 1;
		int	h = backEnd.viewDef->viewport.y2 - backEnd.viewDef->viewport.y1 + 1;

		RENDERLOG_PRINTF( "Resolve to %i x %i buffer\n", w, h );

		GL_SelectTexture( 0 );

		// resolve the screen
		globalImages->currentRenderImage->CopyFramebuffer( x, y, w, h );
		backEnd.currentRenderCopied = true;

		// RENDERPARM_SCREENCORRECTIONFACTOR amd RENDERPARM_WINDOWCOORD overlap
		// diffuseScale and specularScale

		// screen power of two correction factor (no longer relevant now)
		float screenCorrectionParm[4];
		screenCorrectionParm[0] = 1.0f;
		screenCorrectionParm[1] = 1.0f;
		screenCorrectionParm[2] = 0.0f;
		screenCorrectionParm[3] = 1.0f;
		SetFragmentParm( RENDERPARM_SCREENCORRECTIONFACTOR, screenCorrectionParm ); // rpScreenCorrectionFactor

		// window coord to 0.0 to 1.0 conversion
		float windowCoordParm[4];
		windowCoordParm[0] = 1.0f / w;
		windowCoordParm[1] = 1.0f / h;
		windowCoordParm[2] = 0.0f;
		windowCoordParm[3] = 1.0f;
		SetFragmentParm( RENDERPARM_WINDOWCOORD, windowCoordParm ); // rpWindowCoord

		// render the remaining surfaces
		renderLog.OpenMainBlock( MRB_DRAW_SHADER_PASSES_POST );
		RB_DrawShaderPasses( drawSurfs + processed, numDrawSurfs - processed, 0.0f /* definitely not a gui */, stereoEye );
		renderLog.CloseMainBlock();
	}

	//-------------------------------------------------
	// Virtual-texture visibility feedback is independent of the displayed
	// color. Run it only after all _currentRender consumers have finished so
	// feedback-frame FBO/state transitions can never perturb glass/refraction.
	//-------------------------------------------------
	if ( viewDef->viewEntitys != NULL ) {
		RB_DrawVirtualTextureFeedback( drawSurfs, numDrawSurfs );
	}

	// Consume completed page jobs only after every visible color consumer has
	// finished.  Runtime uploads use legacy texture update entry points and are
	// intentionally kept outside the scene/post-process portion of the view.
	// Newly uploaded mappings become visible on the following view, which is the
	// same one-frame latency already imposed by asynchronous feedback readback.
	virtualTextureSystem.UpdateRuntime();
	GL_SelectTexture( 0 );

	//-------------------------------------------------
	// render debug tools
	//-------------------------------------------------
	RB_RenderDebugTools( drawSurfs, numDrawSurfs );

	renderLog.CloseBlock();
}

/*
==================
RB_MotionBlur

Experimental feature
==================
*/
void RB_MotionBlur() {
	if ( !backEnd.viewDef->viewEntitys ) {
		// 3D views only
		return;
	}
	if ( r_motionBlur.GetInteger() <= 0 ) {
		return;
	}
	if ( backEnd.viewDef->isSubview ) {
		return;
	}

	GL_CheckErrors();

	// clear the alpha buffer and draw only the hands + weapon into it so
	// we can avoid blurring them
	GL_ClearColor( 0, 0, 0, 1 );
	GL_State( GLS_COLORMASK | GLS_DEPTHMASK );
	GL_Clear( true, false, false, 0, 0, 0, 0, 1 );
	GL_Color( 0, 0, 0, 0 );
	GL_SelectTexture( 0 );
	globalImages->blackImage->Bind();
	backEnd.currentSpace = NULL;

	drawSurf_t **drawSurfs = (drawSurf_t **)&backEnd.viewDef->drawSurfs[0];
	for ( int surfNum = 0; surfNum < backEnd.viewDef->numDrawSurfs; surfNum++ ) {
		const drawSurf_t * surf = drawSurfs[ surfNum ];

		if ( !surf->space->weaponDepthHack && !surf->space->skipMotionBlur && !surf->material->HasSubview() ) {
			// Apply motion blur to this object
			continue;
		}

		const idMaterial * shader = surf->material;
		if ( shader->Coverage() == MC_TRANSLUCENT ) {
			// muzzle flash, etc
			continue;
		}

		// set mvp matrix
		if ( surf->space != backEnd.currentSpace ) {
			RB_SetMVP( surf->space->mvp );
			backEnd.currentSpace = surf->space;
		}

		// this could just be a color, but we don't have a skinned color-only prog
		if ( surf->jointCache ) {
			renderProgManager.BindShader_TextureVertexColorSkinned();
		} else {
			renderProgManager.BindShader_TextureVertexColor();
		}

		// draw it solid
		RB_DrawElementsWithCounters( surf );
	}
	GL_State( GLS_DEPTHFUNC_ALWAYS );

	// copy off the color buffer and the depth buffer for the motion blur prog
	// we use the viewport dimensions for copying the buffers in case resolution scaling is enabled.
	const idScreenRect & viewport = backEnd.viewDef->viewport;
	globalImages->currentRenderImage->CopyFramebuffer( viewport.x1, viewport.y1, viewport.GetWidth(), viewport.GetHeight() );

	// in stereo rendering, each eye needs to get a separate previous frame mvp
	int mvpIndex = ( backEnd.viewDef->renderView.viewEyeBuffer == 1 ) ? 1 : 0;

	// derive the matrix to go from current pixels to previous frame pixels
	idRenderMatrix	inverseMVP;
	idRenderMatrix::Inverse( backEnd.viewDef->worldSpace.mvp, inverseMVP );

	idRenderMatrix	motionMatrix;
	idRenderMatrix::Multiply( backEnd.prevMVP[mvpIndex], inverseMVP, motionMatrix );

	backEnd.prevMVP[mvpIndex] = backEnd.viewDef->worldSpace.mvp;

	RB_SetMVP( motionMatrix );

	GL_State( GLS_DEPTHFUNC_ALWAYS );
	GL_Cull( CT_TWO_SIDED );

	renderProgManager.BindShader_MotionBlur();

	// let the fragment program know how many samples we are going to use
	idVec4 samples( (float)( 1 << r_motionBlur.GetInteger() ) );
	SetFragmentParm( RENDERPARM_OVERBRIGHT, samples.ToFloatPtr() );

	GL_SelectTexture( 0 );
	globalImages->currentRenderImage->Bind();
	GL_SelectTexture( 1 );
	globalImages->currentDepthImage->Bind();

	RB_DrawElementsWithCounters( &backEnd.unitSquareSurface );
	GL_CheckErrors();
}

/*
==================
RB_DrawView

StereoEye will always be 0 in mono modes, or -1 / 1 in stereo modes.
If the view is a GUI view that is repeated for both eyes, the viewDef.stereoEye value
is 0, so the stereoEye parameter is not always the same as that.
==================
*/
void RB_DrawView( const void *data, const int stereoEye ) {
	const drawSurfsCommand_t * cmd = (const drawSurfsCommand_t *)data;

	backEnd.viewDef = cmd->viewDef;

	// we will need to do a new copyTexSubImage of the screen
	// when a SS_POST_PROCESS material is used
	backEnd.currentRenderCopied = false;

	// if there aren't any drawsurfs, do nothing
	if ( !backEnd.viewDef->numDrawSurfs ) {
		return;
	}

	// skip render bypasses everything that has models, assuming
	// them to be 3D views, but leaves 2D rendering visible
	if ( r_skipRender.GetBool() && backEnd.viewDef->viewEntitys ) {
		return;
	}

	// skip render context sets the wgl context to NULL,
	// which should factor out the API cost, under the assumption
	// that all gl calls just return if the context isn't valid
	if ( r_skipRenderContext.GetBool() && backEnd.viewDef->viewEntitys ) {
		GLimp_DeactivateContext();
	}

	backEnd.pc.c_surfaces += backEnd.viewDef->numDrawSurfs;

	RB_ShowOverdraw();

	// render the scene
	RB_DrawViewInternal( cmd->viewDef, stereoEye );

	RB_MotionBlur();

	// restore the context for 2D drawing if we were stubbing it out
	if ( r_skipRenderContext.GetBool() && backEnd.viewDef->viewEntitys ) {
		GLimp_ActivateContext();
		GL_SetDefaultState();
	}

	// optionally draw a box colored based on the eye number
	if ( r_drawEyeColor.GetBool() ) {
		const idScreenRect & r = backEnd.viewDef->viewport;
		GL_Scissor( ( r.x1 + r.x2 ) / 2, ( r.y1 + r.y2 ) / 2, 32, 32 );
		switch ( stereoEye ) {
			case -1:
				GL_Clear( true, false, false, 0, 1.0f, 0.0f, 0.0f, 1.0f );
				break;
			case 1:
				GL_Clear( true, false, false, 0, 0.0f, 1.0f, 0.0f, 1.0f );
				break;
			default:
				GL_Clear( true, false, false, 0, 0.5f, 0.5f, 0.5f, 1.0f );
				break;
		}
	}
}

/*
==================
RB_CopyRender

Copy part of the current framebuffer to an image
==================
*/
void RB_CopyRender( const void *data ) {
	const copyRenderCommand_t * cmd = (const copyRenderCommand_t *)data;

	if ( r_skipCopyTexture.GetBool() ) {
		return;
	}

	RENDERLOG_PRINTF( "***************** RB_CopyRender *****************\n" );

	if ( cmd->image ) {
		cmd->image->CopyFramebuffer( cmd->x, cmd->y, cmd->imageWidth, cmd->imageHeight );
	}

	if ( cmd->clearColorAfterCopy ) {
		GL_Clear( true, false, false, STENCIL_BASE_VALUE, 0, 0, 0, 0 );
	}
}

/*
==================
RB_PostProcess

==================
*/
extern idCVar rs_enable;
void RB_PostProcess( const void * data ) {

	// only do the post process step if resolution scaling is enabled. Prevents the unnecessary copying of the framebuffer and
	// corresponding full screen quad pass.
	if ( rs_enable.GetInteger() == 0 ) { 
		return;
	}

	// resolve the scaled rendering to a temporary texture
	postProcessCommand_t * cmd = (postProcessCommand_t *)data;
	const idScreenRect & viewport = cmd->viewDef->viewport;
	globalImages->currentRenderImage->CopyFramebuffer( viewport.x1, viewport.y1, viewport.GetWidth(), viewport.GetHeight() );

	GL_State( GLS_SRCBLEND_ONE | GLS_DSTBLEND_ZERO | GLS_DEPTHMASK | GLS_DEPTHFUNC_ALWAYS );
	GL_Cull( CT_TWO_SIDED );

	int screenWidth = renderSystem->GetWidth();
	int screenHeight = renderSystem->GetHeight();

	// set the window clipping
	GL_Viewport( 0, 0, screenWidth, screenHeight );
	GL_Scissor( 0, 0, screenWidth, screenHeight );

	GL_SelectTexture( 0 );
	globalImages->currentRenderImage->Bind();
	renderProgManager.BindShader_PostProcess();

	// Draw
	RB_DrawElementsWithCounters( &backEnd.unitSquareSurface );

	renderLog.CloseBlock();
}
