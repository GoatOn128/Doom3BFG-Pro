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
#include "rendershadowbuffer.h"

// uniform helper shared with renderpasses.cpp, kept as a local inline here
static ID_INLINE void SetFragmentParm( renderParm_t rp, const float * value ) {
	renderProgManager.SetUniformValue( rp, value );
}

// stage helpers defined in renderpasses.cpp and draw_common.cpp
void RB_SetMVP( const idRenderMatrix & mvp );
void RB_SetVertexColorParms( stageVertexColor_t svc );
void RB_PrepareStageTexturing( const shaderStage_t * pStage, const drawSurf_t * surf );
void RB_FinishStageTexturing( const shaderStage_t * pStage, const drawSurf_t * surf );

extern idCVar r_shadowMapEnable;

/*
================================================================================================

Tech 5 style light shadow buffer.

Moved here from renderpasses.cpp so the shadow atlas, the per light type shadow
matrices and the caster draw live in their own file matching the Tech5 job
layout, with no behavior change.

================================================================================================
*/

idCVar r_shadowMapSize( "r_shadowMapSize", "1024", CVAR_RENDERER | CVAR_ARCHIVE | CVAR_INTEGER, "Tech 5 shadow atlas size", 256, 4096 );
idCVar r_shadowMapParallelExtent( "r_shadowMapParallelExtent", "256", CVAR_RENDERER | CVAR_ARCHIVE | CVAR_FLOAT, "half-width of the view-centered parallel-light shadow map (Tech5 default coverage)", 64.0f, 8192.0f );
idCVar r_shadowMapDebug( "r_shadowMapDebug", "0", CVAR_RENDERER | CVAR_BOOL, "print depth-atlas coverage for each shadowed light" );
idCVar r_shadowMapDrawCasters( "r_shadowMapDrawCasters", "1", CVAR_RENDERER | CVAR_BOOL, "draw shadow casters into the depth atlas" );
idCVar r_sb_occluderFacing( "r_sb_occluderFacing", "0", CVAR_RENDERER | CVAR_INTEGER, "shadow caster facing: 0 = front faces, 1 = back faces, 2 = back faces with doubled offset followed by front faces", 0, 2 );
idCVar r_sb_singleSide( "r_sb_singleSide", "-1", CVAR_RENDERER | CVAR_INTEGER, "only draw a single face (0-5) of point light shadow maps, -1 = all faces", -1, 5 );
idCVar r_sb_polyOfsFactor( "r_sb_polyOfsFactor", "2", CVAR_RENDERER | CVAR_FLOAT, "polygon offset factor for point and spot light shadow casters" );
idCVar r_sb_polyOfsUnits( "r_sb_polyOfsUnits", "2000", CVAR_RENDERER | CVAR_FLOAT, "polygon offset units for point and spot light shadow casters" );
idCVar r_sb_parallelPolyOfsFactor( "r_sb_parallelPolyOfsFactor", "2", CVAR_RENDERER | CVAR_FLOAT, "polygon offset factor for parallel (sun) light shadow casters" );
idCVar r_sb_parallelPolyOfsUnits( "r_sb_parallelPolyOfsUnits", "2000", CVAR_RENDERER | CVAR_FLOAT, "polygon offset units for parallel (sun) light shadow casters" );
idCVar r_sb_maxCasters( "r_sb_maxCasters", "256", CVAR_RENDERER | CVAR_INTEGER, "maximum number of shadow casters drawn per light face", 16, 1024 );
idCVar r_sb_sliceStep( "r_sb_sliceStep", "512", CVAR_RENDERER | CVAR_ARCHIVE | CVAR_FLOAT, "initial ortho half-extent of the first parallel light shadow slice", 64.0f, 4096.0f );
idCVar r_sb_sliceRampScale( "r_sb_sliceRampScale", "2", CVAR_RENDERER | CVAR_FLOAT, "larger values perform a faster lerp between parallel light shadow slices", 1.0f, 8.0f );

static idImage * lightShadowAtlas = NULL;
static GLuint lightShadowFBO = 0;
static GLuint lightShadowTextureObject = 0;
int lightShadowAtlasSize = 0;
idRenderMatrix currentShadowMatrix;
idRenderMatrix currentShadowProjection;
float currentShadowType = 0.0f;

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

bool RB_EnsureShadowAtlas() {
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

bool RB_RenderShadowMap( const viewLight_t * vLight ) {
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
