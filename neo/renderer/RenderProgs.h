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
#ifndef __RENDERPROGS_H__
#define __RENDERPROGS_H__

#include "RenderProgParm.h"


static const int PC_ATTRIB_INDEX_VERTEX		= 0;
static const int PC_ATTRIB_INDEX_NORMAL		= 2;
static const int PC_ATTRIB_INDEX_COLOR		= 3;
static const int PC_ATTRIB_INDEX_COLOR2		= 4;
static const int PC_ATTRIB_INDEX_ST			= 8;
static const int PC_ATTRIB_INDEX_TANGENT	= 9;

// This enum list corresponds to the global constant register indecies as defined in global.inc for all
// shaders.  We used a shared pool to keeps things simple.  If something changes here then it also
// needs to change in global.inc and vice versa
enum renderParm_t {
	// For backwards compatibility, do not change the order of the first 17 items
	RENDERPARM_SCREENCORRECTIONFACTOR = 0,
	RENDERPARM_WINDOWCOORD,
	RENDERPARM_DIFFUSEMODIFIER,
	RENDERPARM_SPECULARMODIFIER,

	RENDERPARM_LOCALLIGHTORIGIN,
	RENDERPARM_LOCALVIEWORIGIN,

	RENDERPARM_LIGHTPROJECTION_S,
	RENDERPARM_LIGHTPROJECTION_T,
	RENDERPARM_LIGHTPROJECTION_Q,
	RENDERPARM_LIGHTFALLOFF_S,

	RENDERPARM_BUMPMATRIX_S,
	RENDERPARM_BUMPMATRIX_T,

	RENDERPARM_DIFFUSEMATRIX_S,
	RENDERPARM_DIFFUSEMATRIX_T,

	RENDERPARM_SPECULARMATRIX_S,
	RENDERPARM_SPECULARMATRIX_T,

	RENDERPARM_VERTEXCOLOR_MODULATE,
	RENDERPARM_VERTEXCOLOR_ADD,

	// The following are new and can be in any order
	
	RENDERPARM_COLOR,
	RENDERPARM_VIEWORIGIN,
	RENDERPARM_GLOBALEYEPOS,

	RENDERPARM_MVPMATRIX_X,
	RENDERPARM_MVPMATRIX_Y,
	RENDERPARM_MVPMATRIX_Z,
	RENDERPARM_MVPMATRIX_W,

	RENDERPARM_MODELMATRIX_X,
	RENDERPARM_MODELMATRIX_Y,
	RENDERPARM_MODELMATRIX_Z,
	RENDERPARM_MODELMATRIX_W,

	RENDERPARM_PROJMATRIX_X,
	RENDERPARM_PROJMATRIX_Y,
	RENDERPARM_PROJMATRIX_Z,
	RENDERPARM_PROJMATRIX_W,

	RENDERPARM_MODELVIEWMATRIX_X,
	RENDERPARM_MODELVIEWMATRIX_Y,
	RENDERPARM_MODELVIEWMATRIX_Z,
	RENDERPARM_MODELVIEWMATRIX_W,

	RENDERPARM_TEXTUREMATRIX_S,
	RENDERPARM_TEXTUREMATRIX_T,

	RENDERPARM_TEXGEN_0_S,
	RENDERPARM_TEXGEN_0_T,
	RENDERPARM_TEXGEN_0_Q,
	RENDERPARM_TEXGEN_0_ENABLED,

	RENDERPARM_TEXGEN_1_S,
	RENDERPARM_TEXGEN_1_T,
	RENDERPARM_TEXGEN_1_Q,
	RENDERPARM_TEXGEN_1_ENABLED,

	RENDERPARM_WOBBLESKY_X,
	RENDERPARM_WOBBLESKY_Y,
	RENDERPARM_WOBBLESKY_Z,

	RENDERPARM_OVERBRIGHT,
	RENDERPARM_ENABLE_SKINNING,
	RENDERPARM_ALPHA_TEST,

	RENDERPARM_SHADOWMATRIX_X,
	RENDERPARM_SHADOWMATRIX_Y,
	RENDERPARM_SHADOWMATRIX_Z,
	RENDERPARM_SHADOWMATRIX_W,
	RENDERPARM_SHADOWPROJECTION_X,
	RENDERPARM_SHADOWPROJECTION_Y,
	RENDERPARM_SHADOWPROJECTION_Z,
	RENDERPARM_SHADOWPROJECTION_W,
	RENDERPARM_SHADOWPARAMS,
	RENDERPARM_PARALLEL_LIGHT_SLICE_SCALE,

	RENDERPARM_TOTAL,
	RENDERPARM_USER = 128,
};


/*
================================================================================================
idRenderProgManager

Adapted from the Wolfenstein: The New Order render prog manager
(references/wolfnto-reversedengineered). That material is decompiled
proprietary code and is not GPL-compatible; this is a re-implementation on
this engine's idlib.

The manager keeps the integer based API of the legacy .vertex/.pixel
manager so the backend code does not change, but every shader resolves to
an idDeclRenderProg decl that owns the generated GLSL and the linked GL
program. The shader tables below only remember which decl each legacy
shader name maps to.
================================================================================================
*/
class idDeclRenderProg;

class idRenderProgManager {
public:
	idRenderProgManager();
	virtual ~idRenderProgManager();

	void	Init();
	void	Shutdown();

	void	SetRenderParm( renderParm_t rp, const float * value );
	void	SetRenderParms( renderParm_t rp, const float * values, int numValues );

	int		FindVertexShader( const char * name );
	int		FindFragmentShader( const char * name );

	void	BindShader( int vIndex, int fIndex );
	void	BindShader( int programIndex );

	void	BindShader_GUI( ) { BindShader_Builtin( BUILTIN_GUI ); }
	void	BindShader_Color( ) { BindShader_Builtin( BUILTIN_COLOR ); }
	void	BindShader_Texture( ) { BindShader_Builtin( BUILTIN_TEXTURED ); }
	void	BindShader_TextureVertexColor() { BindShader_Builtin( BUILTIN_TEXTURE_VERTEXCOLOR ); };
	void	BindShader_TextureVertexColorSkinned() { BindShader_Builtin( BUILTIN_TEXTURE_VERTEXCOLOR_SKINNED ); };
	void	BindShader_TextureTexGenVertexColor() { BindShader_Builtin( BUILTIN_TEXTURE_TEXGEN_VERTEXCOLOR ); };
	void	BindShader_Interaction()  { BindShader_Builtin( BUILTIN_INTERACTION ); }
	void	BindShader_InteractionSkinned()  { BindShader_Builtin( BUILTIN_INTERACTION_SKINNED ); }
	void	BindShader_InteractionAmbient()  { BindShader_Builtin( BUILTIN_INTERACTION_AMBIENT ); }
	void	BindShader_InteractionAmbientSkinned()  { BindShader_Builtin( BUILTIN_INTERACTION_AMBIENT_SKINNED ); }
	void	BindShader_VirtualTextureFeedback() { BindShader_Builtin( BUILTIN_VT_FEEDBACK ); }
	void	BindShader_VirtualTextureFeedbackSkinned() { BindShader_Builtin( BUILTIN_VT_FEEDBACK_SKINNED ); }
	void	BindShader_SimpleShade() { BindShader_Builtin( BUILTIN_SIMPLESHADE ); }
	void	BindShader_Environment() { BindShader_Builtin( BUILTIN_ENVIRONMENT ); }
	void	BindShader_EnvironmentSkinned() { BindShader_Builtin( BUILTIN_ENVIRONMENT_SKINNED ); }
	void	BindShader_BumpyEnvironment() { BindShader_Builtin( BUILTIN_BUMPY_ENVIRONMENT ); }
	void	BindShader_BumpyEnvironmentSkinned() { BindShader_Builtin( BUILTIN_BUMPY_ENVIRONMENT_SKINNED ); }

	void	BindShader_Depth() { BindShader_Builtin( BUILTIN_DEPTH ); }
	void	BindShader_DepthSkinned() { BindShader_Builtin( BUILTIN_DEPTH_SKINNED ); }

	void	BindShader_BlendLight() { BindShader_Builtin( BUILTIN_BLENDLIGHT ); }
	void	BindShader_Fog() { BindShader_Builtin( BUILTIN_FOG ); }
	void	BindShader_FogSkinned() { BindShader_Builtin( BUILTIN_FOG_SKINNED ); }
	void	BindShader_SkyBox() { BindShader_Builtin( BUILTIN_SKYBOX ); }
	void	BindShader_WobbleSky() { BindShader_Builtin( BUILTIN_WOBBLESKY ); }
	void	BindShader_StereoDeGhost() { BindShader_Builtin( BUILTIN_STEREO_DEGHOST ); }
	void	BindShader_StereoWarp() { BindShader_Builtin( BUILTIN_STEREO_WARP ); }
	void	BindShader_StereoInterlace() { BindShader_Builtin( BUILTIN_STEREO_INTERLACE ); }
	void	BindShader_PostProcess() { BindShader_Builtin( BUILTIN_POSTPROCESS ); }
	void	BindShader_ZCullReconstruct() { BindShader_Builtin( BUILTIN_ZCULL_RECONSTRUCT ); }
	void	BindShader_Bink() { BindShader_Builtin( BUILTIN_BINK ); }
	void	BindShader_BinkGUI() { BindShader_Builtin( BUILTIN_BINK_GUI ); }
	void	BindShader_MotionBlur() { BindShader_Builtin( BUILTIN_MOTION_BLUR); }
	void	BindShader_DimShadowDraw() { BindShader_Builtin( BUILTIN_DIM_SHADOW_DRAW ); }
	void	BindShader_DimShadowDrawHighQuality() { BindShader_Builtin( BUILTIN_DIM_SHADOW_DRAW_HQ ); }

	// the joints buffer should only be bound for vertex programs that use joints
	bool	ShaderUsesJoints() const { return currentUsesJoints; }
	// the rpEnableSkinning render parm should only be set for vertex programs that use it
	bool	ShaderHasOptionalSkinning() const { return currentOptionalSkinning; }

	// unbind the currently bound render program
	void	Unbind();

	// this should only be called via the reload shader console command
	void	LoadAllShaders();
	void	KillAllShaders();

	static const int	MAX_GLSL_USER_PARMS = 8;
	const char*	GetGLSLParmName( int rp ) const;
	int			GetGLSLCurrentProgram() const { return currentRenderProgram; }
	void		SetUniformValue( const renderParm_t rp, const float * value );
	void		CommitUniforms();
	int			FindGLSLProgram( const char* name, int vIndex, int fIndex );
	void		ZeroUniforms();

protected:
	enum {
		BUILTIN_GUI,
		BUILTIN_COLOR,
		BUILTIN_SIMPLESHADE,
		BUILTIN_TEXTURED,
		BUILTIN_TEXTURE_VERTEXCOLOR,
		BUILTIN_TEXTURE_VERTEXCOLOR_SKINNED,
		BUILTIN_TEXTURE_TEXGEN_VERTEXCOLOR,
		BUILTIN_INTERACTION,
		BUILTIN_INTERACTION_SKINNED,
		BUILTIN_INTERACTION_AMBIENT,
		BUILTIN_INTERACTION_AMBIENT_SKINNED,
		BUILTIN_VT_FEEDBACK,
		BUILTIN_VT_FEEDBACK_SKINNED,
		BUILTIN_ENVIRONMENT,
		BUILTIN_ENVIRONMENT_SKINNED,
		BUILTIN_BUMPY_ENVIRONMENT,
		BUILTIN_BUMPY_ENVIRONMENT_SKINNED,

		BUILTIN_DEPTH,
		BUILTIN_DEPTH_SKINNED,

		BUILTIN_BLENDLIGHT,
		BUILTIN_FOG,
		BUILTIN_FOG_SKINNED,
		BUILTIN_SKYBOX,
		BUILTIN_WOBBLESKY,
		BUILTIN_POSTPROCESS,
		BUILTIN_STEREO_DEGHOST,
		BUILTIN_STEREO_WARP,
		BUILTIN_ZCULL_RECONSTRUCT,
		BUILTIN_BINK,
		BUILTIN_BINK_GUI,
		BUILTIN_STEREO_INTERLACE,
		BUILTIN_MOTION_BLUR,

		BUILTIN_DIM_SHADOW_DRAW,
	BUILTIN_DIM_SHADOW_DRAW_HQ,

		MAX_BUILTINS
	};
	int builtinShaders[MAX_BUILTINS];
	void BindShader_Builtin( int i ) { BindShader( builtinShaders[i] ); }

	// a legacy shader name mapped to the renderprog decl that implements it
	struct shaderEntry_t {
		idStr	name;
		idDeclRenderProg * decl;
	};
	// a linked pair of a vertex and a fragment shader; the decl of the pair
	// owns the GL program
	struct programEntry_t {
		idStr	name;
		idDeclRenderProg * decl;
		int		vertexShaderIndex;
		int		fragmentShaderIndex;
	};

	// uploads the decl if needed, binds its GL program, and refreshes the
	// cached joint and skinning state of the current program
	void	BindDecl( idDeclRenderProg * decl );

	int	currentRenderProgram;
	idList<programEntry_t, TAG_RENDER> glslPrograms;
	idStaticList<idVec4, RENDERPARM_USER + MAX_GLSL_USER_PARMS> glslUniforms;

	int		currentVertexShader;
	int		currentFragmentShader;
	idList<shaderEntry_t, TAG_RENDER> vertexShaders;
	idList<shaderEntry_t, TAG_RENDER> fragmentShaders;

	// cached state of the currently bound decl; the per-shader tables of the
	// legacy manager are gone so the joint and skinning queries read these
	// copies instead
	idDeclRenderProg *	currentDecl;
	bool	currentUsesJoints;
	bool	currentOptionalSkinning;
};

extern idRenderProgManager renderProgManager;

#endif
