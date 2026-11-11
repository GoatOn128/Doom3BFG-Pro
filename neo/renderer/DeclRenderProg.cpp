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
#include "../idlib/precompiled.h"

#include "DeclRenderProg.h"
#include "DeclRenderProgGenerator.h"
#include "ParmBlockParser.h"
#include "../idlib/Parser.h"
#include "tr_local.h"

/*
Adapted from the Wolfenstein: The New Order idDeclRenderProg, decl render
prog generator, and GL upload path reconstructed under
references/wolfnto-reversedengineered (declrenderprog.cpp,
declrenderproggenerator.cpp, declrenderprog_ogl_pc.cpp). That material is
decompiled proprietary code and is not GPL-compatible; this is a
re-implementation on this engine's idlib.

The decl parses the inherit, parms, state, flags, interaction, and
hlsl_vp/hlsl_fp blocks. The parms block is parsed by idParmBlockParser and
the shader bodies are translated to GLSL by DeclRenderProgGenerator. The
TNO platform resource manager and generated-file cache are replaced by a
checksum over the decl text and the translated code.

The TNO generator reads the shared include through the parser preprocessor;
this engine's idParser has no include stack, so renderprogs/global.inc is
read raw and embedded verbatim, which keeps its #define macros intact for
the GLSL compiler to expand.
*/

/*
====================
LoadGlobalInclude
====================
*/
static void LoadGlobalInclude( idStr & out ) {
	out.Clear();
	if ( fileSystem == nullptr ) {
		return;
	}
	void * buffer = nullptr;
	const int length = fileSystem->ReadFile( "renderprogs/global.inc", &buffer );
	if ( length <= 0 || buffer == nullptr ) {
		if ( buffer != nullptr ) {
			fileSystem->FreeFile( buffer );
		}
		return;
	}
	out.Append( static_cast< const char * >( buffer ), length );
	fileSystem->FreeFile( buffer );
}

/*
====================
renderProgData_t::Clear
====================
*/
void renderProgData_t::Clear() {
	inheritsFrom.Clear();
	compoundCheckSum = 0;
	parentCheckSum = 0;
	dependencies.Clear();
	glState = 0;
	parmBlock.Clear();
	vertexParms.Clear();
	fragmentParms.Clear();
	textureParms.Clear();
	samplerParms.Clear();
	flags = 0;
	stageSort = 0.0f;
	hasInteractions = false;
	hasAlphaToCoverage = false;
	has16BitScaleBias = false;
	hasHardwareSkinning = false;
	hasVertexTexture = false;
	hasClip = false;
	hasDepthOutput = false;
	hasBranchStripping = false;
	vertexMask = 0;
	fragmentOutputs = 0;
	vertexCode.Clear();
	fragmentCode.Clear();
}

/*
====================
renderProgData_t::operator=
====================
*/
renderProgData_t & renderProgData_t::operator=( const renderProgData_t & other ) {
	if ( this == &other ) {
		return *this;
	}
	inheritsFrom = other.inheritsFrom;
	compoundCheckSum = other.compoundCheckSum;
	parentCheckSum = other.parentCheckSum;
	dependencies = other.dependencies;
	glState = other.glState;
	parmBlock.CopyFrom( other.parmBlock );
	vertexParms = other.vertexParms;
	fragmentParms = other.fragmentParms;
	textureParms = other.textureParms;
	samplerParms = other.samplerParms;
	flags = other.flags;
	stageSort = other.stageSort;
	hasInteractions = other.hasInteractions;
	hasAlphaToCoverage = other.hasAlphaToCoverage;
	has16BitScaleBias = other.has16BitScaleBias;
	hasHardwareSkinning = other.hasHardwareSkinning;
	hasVertexTexture = other.hasVertexTexture;
	hasClip = other.hasClip;
	hasDepthOutput = other.hasDepthOutput;
	hasBranchStripping = other.hasBranchStripping;
	vertexMask = other.vertexMask;
	fragmentOutputs = other.fragmentOutputs;
	vertexCode = other.vertexCode;
	fragmentCode = other.fragmentCode;
	return *this;
}

/*
=================
idDeclRenderProg::Clear
=================
*/
void idDeclRenderProg::Clear() {
	// the GL program and its location lists belong to the parsed data and
	// are released together; a re-parsed decl rebuilds its program on the
	// next bind
	FreeProgram();
	data.Clear();
	inheritsFromDecl = nullptr;
	vertexProgramChecksum = 0;
	fragmentProgramChecksum = 0;
	hasDerived = false;
	vertexConstructInfo.Clear();
	fragmentConstructInfo.Clear();
	vertexUniformLocations.Clear();
	fragmentUniformLocations.Clear();
	samplerLocations.Clear();
	versions.Clear();
	versions.Append( this );
	versions.Append( this );
	versions.Append( this );
}

/*
=================
idDeclRenderProg::FreeData
=================
*/
void idDeclRenderProg::FreeData() {
	if ( program != 0 ) {
		GL_DeleteProgram( program );
		program = 0;
	}
	Clear();
}

/*
=================
idDeclRenderProg::DefaultDefinition
=================
*/
const char * idDeclRenderProg::DefaultDefinition() const {
	return "{\tstate { depthFunc GL_LEQUAL }\n"
		"\thlsl_vp {\n"
		"\t\tresult.position.x = dot4( vertex.position, $rpMVPmatrixX );\n"
		"\t\tresult.position.y = dot4( vertex.position, $rpMVPmatrixY );\n"
		"\t\tresult.position.z = dot4( vertex.position, $rpMVPmatrixZ );\n"
		"\t\tresult.position.w = dot4( vertex.position, $rpMVPmatrixW );\n"
		"\t}\n"
		"\thlsl_fp {\n"
		"\t\tresult.color = float4( 0.2, 1.0, 0.4, 1.0 );\n"
		"\t}\n"
		"}\n";
}

/*
=================
idDeclRenderProg::Parse
=================
*/
bool idDeclRenderProg::Parse( const char * text, const int textLength, bool allowBinaryVersion ) {
	idParser src( DECL_LEXER_FLAGS );
	idToken token;

	Clear();

	src.LoadMemory( text, textLength, GetFileName() );
	src.AddDefine( "PC 1" );
	src.AddDefine( "GLSL 1" );
	src.SkipUntilString( "{" );

	idStr globalInclude;
	LoadGlobalInclude( globalInclude );

	bool parsedShader = false;
	while ( src.ReadToken( &token ) ) {
		if ( token.Icmp( "}" ) == 0 ) {
			break;
		} else if ( token.Icmp( "inherit" ) == 0 ) {
			if ( data.glState != 0 || data.flags != 0 || data.parmBlock.NumOps() != 0 || parsedShader ) {
				src.Warning( "'inherit' MUST come before all other commands" );
				MakeDefault();
				return false;
			}
			if ( !src.ReadToken( &token ) ) {
				src.Warning( "Unexpected EOF after 'inherit'" );
				MakeDefault();
				return false;
			}
			const idDeclRenderProg * parent = static_cast< const idDeclRenderProg * >( declManager->FindType( DECL_RENDERPROG, token.c_str(), false ) );
			if ( parent == nullptr ) {
				src.Warning( "Unknown RenderProg %s", token.c_str() );
				MakeDefault();
				return false;
			}
			data = parent->data;
			vertexConstructInfo = parent->vertexConstructInfo;
			fragmentConstructInfo = parent->fragmentConstructInfo;
			vertexProgramChecksum = parent->vertexProgramChecksum;
			fragmentProgramChecksum = parent->fragmentProgramChecksum;
			data.inheritsFrom = parent->GetName();
			data.parentCheckSum = data.compoundCheckSum;
			inheritsFromDecl = parent;
			const_cast< idDeclRenderProg * >( parent )->hasDerived = true;
		} else if ( token.Icmp( "parms" ) == 0 ) {
			if ( !src.ExpectTokenString( "{" ) ) {
				MakeDefault();
				return false;
			}
			idParmBlockParser parser( src, data.parmBlock );
			// the TNO engine computes data.stageSort from a stageSort parm
			// here; this engine's registry has no such parm
		} else if ( token.Icmp( "state" ) == 0 ) {
			data.glState = 0;
			data.hasAlphaToCoverage = false;
			if ( !ParseState( &src, &data ) ) {
				MakeDefault();
				return false;
			}
		} else if ( token.Icmp( "flags" ) == 0 ) {
			data.flags = 0;
			if ( !ParseFlags( &src, &data ) ) {
				MakeDefault();
				return false;
			}
		} else if ( token.Icmp( "interaction" ) == 0 ) {
			data.hasInteractions = true;
		} else if ( token.Icmp( "hlsl_vp" ) == 0 || token.Icmp( "hlsl_fp" ) == 0 ) {
			const int oldFlags = src.GetFlags();
			src.SetFlags( oldFlags & ~LEXFL_ALLOWPATHNAMES );
			const programType_t programType = ( token.Icmp( "hlsl_vp" ) == 0 ) ? PROGRAM_TYPE_VERTEX : PROGRAM_TYPE_FRAGMENT;
			shaderConstructInfo_t * shaderInfo = ( programType == PROGRAM_TYPE_VERTEX ) ? &vertexConstructInfo : &fragmentConstructInfo;
			shaderInfo->Clear();
			const bool parsed = ParseShader( programType, &src, &globalInclude, &data, shaderInfo );
			src.SetFlags( oldFlags );
			if ( !parsed ) {
				MakeDefault();
				return false;
			}
			parsedShader = true;
		} else {
			src.Warning( "Unknown program type: %s", token.c_str() );
			MakeDefault();
			return false;
		}
	}

	if ( data.hasInteractions ) {
		idStr variant( GetName() );
		variant.Append( "_point" );
		versions[0] = static_cast< const idDeclRenderProg * >( declManager->FindType( DECL_RENDERPROG, variant, false ) );
		variant = GetName();
		variant.Append( "_spot" );
		versions[1] = static_cast< const idDeclRenderProg * >( declManager->FindType( DECL_RENDERPROG, variant, false ) );
		variant = GetName();
		variant.Append( "_parallel" );
		versions[2] = static_cast< const idDeclRenderProg * >( declManager->FindType( DECL_RENDERPROG, variant, false ) );
	}

	GenerateCode( &data, &vertexConstructInfo, &fragmentConstructInfo );
	if ( vertexConstructInfo.parsed ) {
		data.vertexCode = ConvertCG2GLSL( data.vertexCode, GetName(), true );
		vertexProgramChecksum = MD5_BlockChecksum( data.vertexCode.c_str(), data.vertexCode.Length() );
	}
	if ( fragmentConstructInfo.parsed ) {
		data.fragmentCode = ConvertCG2GLSL( data.fragmentCode, GetName(), false );
		fragmentProgramChecksum = MD5_BlockChecksum( data.fragmentCode.c_str(), data.fragmentCode.Length() );
	}
	bool foundGlobalInclude = false;
	for ( int index = 0; index < data.dependencies.Num(); index++ ) {
		if ( data.dependencies[index].Icmp( "renderprogs/global.inc" ) == 0 ) {
			foundGlobalInclude = true;
			break;
		}
	}
	if ( !foundGlobalInclude ) {
		data.dependencies.Append( idStr( "renderprogs/global.inc" ) );
	}
	data.compoundCheckSum = data.parentCheckSum ^ MD5_BlockChecksum( text, textLength );

	return true;
}

/*
=================
idDeclRenderProg::List
=================
*/
void idDeclRenderProg::List() const {
	idLib::Printf( "%2i %2i %2i %s\n", data.vertexParms.Num(), data.fragmentParms.Num(), data.textureParms.Num(), GetName() );
}

/*
=================
idDeclRenderProg::Print
=================
*/
void idDeclRenderProg::Print() const {
	const idStaticList<short, 42> * uniformParms[2] = { &data.vertexParms, &data.fragmentParms };
	const char * uniformHeadings[2] = { "--- vertex uniforms ---\n", "--- fragment uniforms ---\n" };
	for ( int group = 0; group < 2; group++ ) {
		idLib::Printf( "%s", uniformHeadings[group] );
		for ( int index = 0; index < uniformParms[group]->Num(); index++ ) {
			const char * name = renderProgParms.GetParmNameForIndex( ( *uniformParms[group] )[index] );
			idLib::Printf( "%s\n", name != nullptr ? name : "<badParmIndex>" );
		}
	}
	const idStaticList<short, 16> * textureParms[2] = { &data.textureParms, &data.samplerParms };
	const char * textureHeadings[2] = { "--- textures ---\n", "--- samplers ---\n" };
	for ( int group = 0; group < 2; group++ ) {
		idLib::Printf( "%s", textureHeadings[group] );
		for ( int index = 0; index < textureParms[group]->Num(); index++ ) {
			const char * name = renderProgParms.GetParmNameForIndex( ( *textureParms[group] )[index] );
			idLib::Printf( "%s\n", name != nullptr ? name : "<badParmIndex>" );
		}
	}
}

/*
=================
idDeclRenderProg::ProgUsesParm
=================
*/
bool idDeclRenderProg::ProgUsesParm( const idRenderProgParm * parm ) const {
	if ( parm == nullptr ) {
		return false;
	}
	const short parmIndex = ( short )parm->GetIndex();
	const idStaticList<short, 42> * uniformParms[2] = { &data.vertexParms, &data.fragmentParms };
	for ( int group = 0; group < 2; group++ ) {
		for ( int index = 0; index < uniformParms[group]->Num(); index++ ) {
			if ( ( *uniformParms[group] )[index] == parmIndex ) {
				return true;
			}
		}
	}
	const idStaticList<short, 16> * textureParms[2] = { &data.textureParms, &data.samplerParms };
	for ( int group = 0; group < 2; group++ ) {
		for ( int index = 0; index < textureParms[group]->Num(); index++ ) {
			if ( ( *textureParms[group] )[index] == parmIndex ) {
				return true;
			}
		}
	}
	return false;
}

/*
=================
idDeclRenderProg::RecoverFromDefaulted

A decl can end up defaulted when its source text was not available or a
parse failed; left alone it would keep uploading the green default program.
Try the real source text once per session.
=================
*/
void idDeclRenderProg::RecoverFromDefaulted() {
	if ( defaultedReparseAttempted ) {
		return;
	}
	defaultedReparseAttempted = true;
	common->Warning( "renderprog %s is defaulted, trying to reparse", GetName() );
	base->ReparseIfDefaulted();
	if ( GetState() == DS_DEFAULTED ) {
		common->Warning( "renderprog %s still defaulted, its program will not be drawn", GetName() );
	}
}

/*
=================
idDeclRenderProg::EnsureUploaded
=================
*/
void idDeclRenderProg::EnsureUploaded() {
	// the program is only valid together with the location lists resolved
	// for the current parm lists; if the decl data was re-populated after
	// the last upload the sizes no longer match, and the program is
	// rebuilt before any draw commits uniforms through stale locations
	if ( program != 0 &&
		 vertexUniformLocations.Num() == data.vertexParms.Num() &&
		 fragmentUniformLocations.Num() == data.fragmentParms.Num() &&
		 samplerLocations.Num() == data.textureParms.Num() ) {
		return;
	}
	if ( program != 0 ) {
		FreeProgram();
	}
	UploadTargetCode();
}
