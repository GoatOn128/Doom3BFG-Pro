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

If you have questions concerning the license or the applicable additional terms, you may contact in writing id Software LLC, c/o ZeniMax Media Inc., Suite 120, Rockville, Maryland 20850 USA.

===========================================================================
*/

#pragma hdrstop
#include "../idlib/precompiled.h"

#include "tr_local.h"

#include "DeclRenderProg.h"
#include "RenderProgParm.h"

/*
The decl backed manager is adapted from the Wolfenstein: The New Order
render prog manager reconstructed under references/wolfnto-reversedengineered
(renderprogmanager.h/.cpp). That material is decompiled proprietary code and
is not GPL-compatible; this is a re-implementation on this engine's idlib.
The integer based API of the legacy .vertex/.pixel manager is kept so the
backend code does not change; every shader resolves to an idDeclRenderProg
decl that owns the generated GLSL and the linked GL program.
*/

idRenderProgManager renderProgManager;

/*
================================================================================================
idRenderProgManager::idRenderProgManager()
================================================================================================
*/
idRenderProgManager::idRenderProgManager() {
	currentVertexShader = -1;
	currentFragmentShader = -1;
	currentRenderProgram = -1;
	currentDecl = nullptr;
	currentUsesJoints = false;
	currentOptionalSkinning = false;
}

/*
================================================================================================
idRenderProgManager::~idRenderProgManager()
================================================================================================
*/
idRenderProgManager::~idRenderProgManager() {
}

/*
================================================================================================
FindRenderProgDecl

Resolves a legacy shader file name (with or without extension) to the
renderprog decl of the same name.
================================================================================================
*/
static idDeclRenderProg * FindRenderProgDecl( const char * name ) {
	idStr declName( name );
	declName.StripFileExtension();
	const idDeclRenderProg * decl = static_cast< const idDeclRenderProg * >(
		declManager->FindType( DECL_RENDERPROG, declName.c_str(), false ) );
	return const_cast< idDeclRenderProg * >( decl );
}

/*
================================================================================================
R_ReloadShaders

Drops the compiled GL programs; decl text is reloaded through the decl
manager (reloadDecls) and the programs compile again on the next bind.
================================================================================================
*/
static void R_ReloadShaders( const idCmdArgs &args ) {
	renderProgManager.KillAllShaders();
	renderProgManager.LoadAllShaders();
}

/*
================================================================================================
idRenderProgManager::Init()

Every builtin shader resolves to a renderprog decl; the GL programs are
compiled lazily on first bind, so a decl that fails to compile reports at
the first draw that uses it instead of at startup.
================================================================================================
*/
void idRenderProgManager::Init() {
	common->Printf( "----- Initializing Render Shaders -----\n" );

	// the decl manager registers the renderprogs folder during its own
	// Init(), before the renderer initializes, so the builtin lookups below
	// find every decl without an extra RegisterDeclFolder here.

	for ( int i = 0; i < MAX_BUILTINS; i++ ) {
		builtinShaders[i] = -1;
	}
	struct builtinShaders_t {
		int index;
		const char * name;
	} builtins[] = {
		{ BUILTIN_GUI, "gui.vfp" },
		{ BUILTIN_COLOR, "color.vfp" },
		{ BUILTIN_SIMPLESHADE, "simpleshade.vfp" },
		{ BUILTIN_TEXTURED, "texture.vfp" },
		{ BUILTIN_TEXTURE_VERTEXCOLOR, "texture_color.vfp" },
		{ BUILTIN_TEXTURE_VERTEXCOLOR_SKINNED, "texture_color_skinned.vfp" },
		{ BUILTIN_TEXTURE_TEXGEN_VERTEXCOLOR, "texture_color_texgen.vfp" },
		{ BUILTIN_INTERACTION, "interaction.vfp" },
		{ BUILTIN_INTERACTION_SKINNED, "interaction_skinned.vfp" },
		{ BUILTIN_INTERACTION_AMBIENT, "interactionAmbient.vfp" },
		{ BUILTIN_INTERACTION_AMBIENT_SKINNED, "interactionAmbient_skinned.vfp" },
		{ BUILTIN_VT_FEEDBACK, "vtFeedback.vfp" },
		{ BUILTIN_VT_FEEDBACK_SKINNED, "vtFeedback_skinned.vfp" },
		{ BUILTIN_ENVIRONMENT, "environment.vfp" },
		{ BUILTIN_ENVIRONMENT_SKINNED, "environment_skinned.vfp" },
		{ BUILTIN_BUMPY_ENVIRONMENT, "bumpyEnvironment.vfp" },
		{ BUILTIN_BUMPY_ENVIRONMENT_SKINNED, "bumpyEnvironment_skinned.vfp" },

		{ BUILTIN_DEPTH, "depth.vfp" },
		{ BUILTIN_DEPTH_SKINNED, "depth_skinned.vfp" },
		{ BUILTIN_BLENDLIGHT, "blendlight.vfp" },
		{ BUILTIN_FOG, "fog.vfp" },
		{ BUILTIN_FOG_SKINNED, "fog_skinned.vfp" },
		{ BUILTIN_SKYBOX, "skybox.vfp" },
		{ BUILTIN_WOBBLESKY, "wobblesky.vfp" },
		{ BUILTIN_POSTPROCESS, "postprocess.vfp" },
		{ BUILTIN_STEREO_DEGHOST, "stereoDeGhost.vfp" },
		{ BUILTIN_STEREO_WARP, "stereoWarp.vfp" },
		{ BUILTIN_ZCULL_RECONSTRUCT, "zcullReconstruct.vfp" },
		{ BUILTIN_BINK, "bink.vfp" },
		{ BUILTIN_BINK_GUI, "bink_gui.vfp" },
		{ BUILTIN_STEREO_INTERLACE, "stereoInterlace.vfp" },
		{ BUILTIN_MOTION_BLUR, "motionBlur.vfp" },
	};
	int numBuiltins = sizeof( builtins ) / sizeof( builtins[0] );

	for ( int i = 0; i < numBuiltins; i++ ) {
		idDeclRenderProg * decl = FindRenderProgDecl( builtins[i].name );
		if ( decl == nullptr ) {
			common->Warning( "no renderprog decl for builtin shader %s", builtins[i].name );
		}
		shaderEntry_t shader;
		shader.name = builtins[i].name;
		shader.decl = decl;
		const int vertexShaderIndex = vertexShaders.Append( shader );
		const int fragmentShaderIndex = fragmentShaders.Append( shader );
		programEntry_t program;
		program.name = builtins[i].name;
		program.decl = decl;
		program.vertexShaderIndex = vertexShaderIndex;
		program.fragmentShaderIndex = fragmentShaderIndex;
		builtinShaders[builtins[i].index] = glslPrograms.Append( program );
	}
	glslUniforms.SetNum( RENDERPARM_USER + MAX_GLSL_USER_PARMS, vec4_zero );

	cmdSystem->AddCommand( "reloadShaders", R_ReloadShaders, CMD_FL_RENDERER, "reloads shaders" );
}

/*
================================================================================================
idRenderProgManager::LoadAllShaders()

The GL programs are compiled lazily on the next bind, so there is nothing
to load eagerly.
================================================================================================
*/
void idRenderProgManager::LoadAllShaders() {
}

/*
================================================================================================
idRenderProgManager::KillAllShaders()
================================================================================================
*/
void idRenderProgManager::KillAllShaders() {
	Unbind();
	for ( int i = 0; i < glslPrograms.Num(); i++ ) {
		if ( glslPrograms[i].decl != nullptr ) {
			glslPrograms[i].decl->FreeProgram();
		}
	}
	// the decl may also be referenced through a shader entry that has no
	// program entry yet
	for ( int i = 0; i < vertexShaders.Num(); i++ ) {
		if ( vertexShaders[i].decl != nullptr ) {
			vertexShaders[i].decl->FreeProgram();
		}
	}
	for ( int i = 0; i < fragmentShaders.Num(); i++ ) {
		if ( fragmentShaders[i].decl != nullptr ) {
			fragmentShaders[i].decl->FreeProgram();
		}
	}
}

/*
================================================================================================
idRenderProgManager::Shutdown()
================================================================================================
*/
void idRenderProgManager::Shutdown() {
	KillAllShaders();
}

/*
================================================================================================
idRenderProgManager::FindVertexShader

The legacy heat haze names carried the usesJoints / optionalSkinning hints
of the old manager; the decls now derive both from the shader body (the
matrices_ubo block and the rpEnableSkinning parm), so no name special case
is needed here.
================================================================================================
*/
int idRenderProgManager::FindVertexShader( const char * name ) {
	for ( int i = 0; i < vertexShaders.Num(); i++ ) {
		if ( vertexShaders[i].name.Icmp( name ) == 0 ) {
			return i;
		}
	}
	shaderEntry_t shader;
	shader.name = name;
	shader.decl = FindRenderProgDecl( name );
	if ( shader.decl == nullptr ) {
		common->Warning( "no renderprog decl for vertex shader %s", name );
	}
	int index = vertexShaders.Append( shader );
	currentVertexShader = index;
	return index;
}

/*
================================================================================================
idRenderProgManager::FindFragmentShader
================================================================================================
*/
int idRenderProgManager::FindFragmentShader( const char * name ) {
	for ( int i = 0; i < fragmentShaders.Num(); i++ ) {
		if ( fragmentShaders[i].name.Icmp( name ) == 0 ) {
			return i;
		}
	}
	shaderEntry_t shader;
	shader.name = name;
	shader.decl = FindRenderProgDecl( name );
	if ( shader.decl == nullptr ) {
		common->Warning( "no renderprog decl for fragment shader %s", name );
	}
	int index = fragmentShaders.Append( shader );
	currentFragmentShader = index;
	return index;
}

/*
================================================================================================
idRenderProgManager::BindDecl
================================================================================================
*/
void idRenderProgManager::BindDecl( idDeclRenderProg * decl ) {
	currentDecl = decl;
	currentUsesJoints = false;
	currentOptionalSkinning = false;
	if ( decl == nullptr ) {
		qglUseProgram( 0 );
		return;
	}
	decl->EnsureUploaded();
	if ( !decl->IsUploaded() ) {
		// the decl has no generated code to upload; keep no program bound so
		// the draw is skipped instead of drawing with the wrong program
		common->Warning( "renderprog %s has no GL program", decl->GetName() );
		qglUseProgram( 0 );
		currentDecl = nullptr;
		return;
	}
	RENDERLOG_PRINTF( "Binding GLSL Program %s\n", decl->GetName() );
	qglUseProgram( decl->GetProgram() );
	currentUsesJoints = decl->UsesJoints();
	// optional skinning means the vertex program branches on the
	// rpEnableSkinning parm; only the heat haze shaders do
	for ( int i = 0; i < decl->data.vertexParms.Num(); i++ ) {
		if ( decl->data.vertexParms[i] == RENDERPARM_ENABLE_SKINNING ) {
			currentOptionalSkinning = true;
			break;
		}
	}
}

/*
================================================================================================
idRenderProgManager::BindShader
================================================================================================
*/
void idRenderProgManager::BindShader( int vIndex, int fIndex ) {
	if ( currentVertexShader == vIndex && currentFragmentShader == fIndex ) {
		return;
	}
	currentVertexShader = vIndex;
	currentFragmentShader = fIndex;

	idDeclRenderProg * decl = nullptr;
	if ( vIndex >= 0 && vIndex < vertexShaders.Num() ) {
		decl = vertexShaders[vIndex].decl;
	}
	if ( decl != nullptr &&
			fIndex >= 0 && fIndex < fragmentShaders.Num() &&
			fragmentShaders[fIndex].decl != nullptr &&
			fragmentShaders[fIndex].decl != decl ) {
		// the legacy files always pair a vertex and a fragment shader of the
		// same name, so the two indices resolve to the same decl; mixing
		// stages from different decls is not supported
		common->Warning( "vertex shader %s and fragment shader %s resolve to different renderprog decls",
						 vertexShaders[vIndex].name.c_str(), fragmentShaders[fIndex].name.c_str() );
	}

	// find or create the program entry for this pair
	currentRenderProgram = -1;
	for ( int i = 0; i < glslPrograms.Num(); ++i ) {
		if ( glslPrograms[i].vertexShaderIndex == vIndex && glslPrograms[i].fragmentShaderIndex == fIndex ) {
			currentRenderProgram = i;
			break;
		}
	}
	if ( currentRenderProgram == -1 ) {
		programEntry_t program;
		program.name = ( vIndex >= 0 && vIndex < vertexShaders.Num() ) ? vertexShaders[vIndex].name : "";
		program.decl = decl;
		program.vertexShaderIndex = vIndex;
		program.fragmentShaderIndex = fIndex;
		currentRenderProgram = glslPrograms.Append( program );
	}

	BindDecl( decl );
}

/*
================================================================================================
idRenderProgManager::BindShader
================================================================================================
*/
void idRenderProgManager::BindShader( int programIndex ) {
	if ( programIndex < 0 || programIndex >= glslPrograms.Num() ) {
		return;
	}
	const programEntry_t & program = glslPrograms[programIndex];
	if ( currentVertexShader == program.vertexShaderIndex && currentFragmentShader == program.fragmentShaderIndex ) {
		return;
	}
	currentVertexShader = program.vertexShaderIndex;
	currentFragmentShader = program.fragmentShaderIndex;
	currentRenderProgram = programIndex;
	BindDecl( program.decl );
}

/*
================================================================================================
idRenderProgManager::Unbind
================================================================================================
*/
void idRenderProgManager::Unbind() {
	currentVertexShader = -1;
	currentFragmentShader = -1;
	currentRenderProgram = -1;
	currentDecl = nullptr;
	currentUsesJoints = false;
	currentOptionalSkinning = false;

	qglUseProgram( 0 );
}

/*
================================================================================================
idRenderProgManager::SetRenderParms
================================================================================================
*/
void idRenderProgManager::SetRenderParms( renderParm_t rp, const float * value, int num ) {
	for ( int i = 0; i < num; i++ ) {
		SetRenderParm( (renderParm_t)(rp + i), value + ( i * 4 ) );
	}
}

/*
================================================================================================
idRenderProgManager::SetRenderParm
================================================================================================
*/
void idRenderProgManager::SetRenderParm( renderParm_t rp, const float * value ) {
	SetUniformValue( rp, value );
}

/*
================================================================================================
idRenderProgManager::FindGLSLProgram
================================================================================================
*/
int idRenderProgManager::FindGLSLProgram( const char * name, int vIndex, int fIndex ) {
	for ( int i = 0; i < glslPrograms.Num(); ++i ) {
		if ( glslPrograms[i].vertexShaderIndex == vIndex && glslPrograms[i].fragmentShaderIndex == fIndex ) {
			return i;
		}
	}
	programEntry_t program;
	program.name = name;
	program.decl = nullptr;
	if ( vIndex >= 0 && vIndex < vertexShaders.Num() ) {
		program.decl = vertexShaders[vIndex].decl;
	}
	if ( program.decl == nullptr && fIndex >= 0 && fIndex < fragmentShaders.Num() ) {
		program.decl = fragmentShaders[fIndex].decl;
	}
	program.vertexShaderIndex = vIndex;
	program.fragmentShaderIndex = fIndex;
	return glslPrograms.Append( program );
}

/*
================================================================================================
idRenderProgManager::GetGLSLParmName
================================================================================================
*/
const char* idRenderProgManager::GetGLSLParmName( int rp ) const {
	if ( rp >= RENDERPARM_USER ) {
		int userParmIndex = rp - RENDERPARM_USER;
		return va("rpUser%d", userParmIndex );
	}
	assert( rp < RENDERPARM_TOTAL );
	const char * parmName = renderProgParms.GetParmNameForIndex( rp );
	return ( parmName != nullptr ) ? parmName : "<badParmIndex>";
}

/*
================================================================================================
idRenderProgManager::SetUniformValue
================================================================================================
*/
void idRenderProgManager::SetUniformValue( const renderParm_t rp, const float * value ) {
	for ( int i = 0; i < 4; i++ ) {
		glslUniforms[rp][i] = value[i];
	}
}

/*
================================================================================================
idRenderProgManager::CommitUniforms

The decl stores the GL uniform location + 1 for every parm of its vertex
and fragment programs; a location of 0 means the compiled program does not
use that parm. The render parm indices of the registry match the
renderParm_t values, so the uniform values are looked up directly.
================================================================================================
*/
void idRenderProgManager::CommitUniforms() {
	const idDeclRenderProg * decl = currentDecl;
	if ( decl == nullptr ) {
		return;
	}
	const renderProgData_t & data = decl->data;
	for ( int index = 0; index < data.vertexParms.Num(); index++ ) {
		const int location = decl->GetVertexUniformLocation( index );
		if ( location != 0 ) {
			qglUniform4fv( location - 1, 1, glslUniforms[data.vertexParms[index]].ToFloatPtr() );
		}
	}
	for ( int index = 0; index < data.fragmentParms.Num(); index++ ) {
		const int location = decl->GetFragmentUniformLocation( index );
		if ( location != 0 ) {
			qglUniform4fv( location - 1, 1, glslUniforms[data.fragmentParms[index]].ToFloatPtr() );
		}
	}
}

/*
================================================================================================
idRenderProgManager::ZeroUniforms
================================================================================================
*/
void idRenderProgManager::ZeroUniforms() {
	memset( glslUniforms.Ptr(), 0, glslUniforms.Allocated() );
}

