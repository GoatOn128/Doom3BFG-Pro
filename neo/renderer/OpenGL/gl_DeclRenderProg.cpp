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
#include "../../idlib/precompiled.h"

#include "../DeclRenderProg.h"
#include "../DeclRenderProgAttribs.h"
#include "../RenderProgParm.h"
#include "../tr_local.h"

idCVar r_dumpGeneratedGLSL( "r_dumpGeneratedGLSL", "0", CVAR_BOOL, "dump the GLSL generated for renderprog decls into generated/glsl" );

/*
=================
CompileGLSL
=================
*/
static unsigned int CompileGLSL( const idStr & programGLSL, bool isVertexProgram, const char * name, const char * fileName ) {
	const GLuint shader = qglCreateShader( isVertexProgram ? GL_VERTEX_SHADER : GL_FRAGMENT_SHADER );
	if ( shader == 0 ) {
		return 0;
	}

	const char * source = programGLSL.c_str();
	qglShaderSource( shader, 1, &source, NULL );
	qglCompileShader( shader );

	int infoLogLength = 0;
	qglGetShaderiv( shader, GL_INFO_LOG_LENGTH, &infoLogLength );
	if ( infoLogLength > 1 ) {
		idTempArray<char> infoLog( infoLogLength );
		int charsWritten = 0;
		qglGetShaderInfoLog( shader, infoLogLength, &charsWritten, infoLog.Ptr() );

		// catch the strings the ATI and Intel drivers output on success
		if ( strstr( infoLog.Ptr(), "successfully compiled to run on hardware" ) == NULL &&
			 strstr( infoLog.Ptr(), "No errors." ) == NULL ) {
			idLib::Printf( "While compiling %s program %s from %s\n",
						   isVertexProgram ? "vertex" : "fragment", name, fileName );
			idLib::Printf( "-----------------\n" );
			int lineNumber = 1;
			const char * lineStart = source;
			while ( *lineStart != '\0' ) {
				const char * lineEnd = strchr( lineStart, '\n' );
				if ( lineEnd == NULL ) {
					idLib::Printf( "%3d: %s\n", lineNumber, lineStart );
					break;
				}
				idLib::Printf( "%3d: %.*s\n", lineNumber,
							   static_cast< int >( lineEnd - lineStart ), lineStart );
				lineStart = lineEnd + 1;
				++lineNumber;
			}
			idLib::Printf( "-----------------\n" );
			idLib::Printf( "%s\n", infoLog.Ptr() );
		}
	}

	int compiled = GL_FALSE;
	qglGetShaderiv( shader, GL_COMPILE_STATUS, &compiled );
	if ( compiled == GL_FALSE ) {
		qglDeleteShader( shader );
		return 0;
	}
	return shader;
}

/*
=================
idDeclRenderProg::UploadTargetCode
=================
*/
void idDeclRenderProg::UploadTargetCode() {
	if ( data.vertexCode.IsEmpty() || data.fragmentCode.IsEmpty() ) {
		idLib::Warning( "renderprog %s has no generated shader code to upload", GetName() );
		return;
	}

	if ( r_dumpGeneratedGLSL.GetBool() && fileSystem != NULL ) {
		idStr vertexName( "generated/glsl/" );
		vertexName += GetName();
		vertexName += "_vertex.glsl";
		fileSystem->WriteFile( vertexName.c_str(), data.vertexCode.c_str(), data.vertexCode.Length() );
		idStr fragmentName( "generated/glsl/" );
		fragmentName += GetName();
		fragmentName += "_fragment.glsl";
		fileSystem->WriteFile( fragmentName.c_str(), data.fragmentCode.c_str(), data.fragmentCode.Length() );
		idLib::Printf( "dumped GLSL for renderprog %s into generated/glsl\n", GetName() );
	}

	const unsigned int vertexShader = CompileGLSL( data.vertexCode, true, GetName(), GetFileName() );
	const unsigned int fragmentShader = CompileGLSL( data.fragmentCode, false, GetName(), GetFileName() );
	if ( vertexShader == 0 || fragmentShader == 0 ) {
		if ( vertexShader != 0 ) {
			qglDeleteShader( vertexShader );
		}
		if ( fragmentShader != 0 ) {
			qglDeleteShader( fragmentShader );
		}
		idLib::Error( "Failed to compile render prog %s from %s", GetName(), GetFileName() );
		return;
	}

	const GLuint glProgram = qglCreateProgram();
	qglAttachShader( glProgram, vertexShader );
	qglAttachShader( glProgram, fragmentShader );

	// bind the vertex attribute locations to the PC attribute indices
	for ( int index = 0; attribsPC[index].glsl != NULL; ++index ) {
		if ( ( attribsPC[index].flags & AT_VS_IN ) != 0 ) {
			qglBindAttribLocation( glProgram, attribsPC[index].bind, attribsPC[index].glsl );
		}
	}

	// bind the fragment outputs to color numbers; gl_* built-ins like
	// gl_FragDepth are assigned by the driver and must not be bound
	for ( int index = 0; attribsPC[index].glsl != NULL; ++index ) {
		if ( ( attribsPC[index].flags & AT_PS_OUT ) != 0 &&
			 idStr::Cmpn( attribsPC[index].glsl, "gl_", 3 ) != 0 ) {
			qglBindFragDataLocation( glProgram, attribsPC[index].bind, attribsPC[index].glsl );
		}
	}

	qglLinkProgram( glProgram );

	// the shaders are part of the program now; the names can be released
	qglDeleteShader( vertexShader );
	qglDeleteShader( fragmentShader );

	int infoLogLength = 0;
	qglGetProgramiv( glProgram, GL_INFO_LOG_LENGTH, &infoLogLength );
	if ( infoLogLength > 1 ) {
		idTempArray<char> infoLog( infoLogLength );
		int charsWritten = 0;
		qglGetProgramInfoLog( glProgram, infoLogLength, &charsWritten, infoLog.Ptr() );

		// catch the strings the ATI and Intel drivers output on success
		if ( strstr( infoLog.Ptr(), "Vertex shader(s) linked, fragment shader(s) linked." ) == NULL &&
			 strstr( infoLog.Ptr(), "No errors." ) == NULL ) {
			idLib::Printf( "While linking render prog %s from %s\n%s\n", GetName(), GetFileName(), infoLog.Ptr() );
		}
	}

	int linked = GL_FALSE;
	qglGetProgramiv( glProgram, GL_LINK_STATUS, &linked );
	if ( linked == GL_FALSE ) {
		qglDeleteProgram( glProgram );
		idLib::Error( "Failed to link render prog %s from %s", GetName(), GetFileName() );
		return;
	}

	program = glProgram;

	// resolve the uniform locations; the GLSL compiler strips uniforms the
	// program does not use, so a location of -1 is normal and stored as 0
	vertexUniformLocations.SetNum( data.vertexParms.Num() );
	for ( int index = 0; index < data.vertexParms.Num(); ++index ) {
		const char * parmName = renderProgParms.GetParmNameForIndex( data.vertexParms[index] );
		const int location = ( parmName != NULL ) ? qglGetUniformLocation( glProgram, parmName ) : -1;
		vertexUniformLocations[index] = location + 1;
	}
	fragmentUniformLocations.SetNum( data.fragmentParms.Num() );
	for ( int index = 0; index < data.fragmentParms.Num(); ++index ) {
		const char * parmName = renderProgParms.GetParmNameForIndex( data.fragmentParms[index] );
		const int location = ( parmName != NULL ) ? qglGetUniformLocation( glProgram, parmName ) : -1;
		fragmentUniformLocations[index] = location + 1;
	}

	// the joints uniform block of skinned programs binds to binding point 0
	GLint blockIndex = qglGetUniformBlockIndex( glProgram, "matrices_ubo" );
	if ( blockIndex != -1 ) {
		qglUniformBlockBinding( glProgram, blockIndex, 0 );
	}

	// assign the texture units once; a sampler the driver stripped keeps
	// location 0 and is skipped when committing textures
	samplerLocations.SetNum( data.textureParms.Num() );
	qglUseProgram( glProgram );
	for ( int index = 0; index < data.textureParms.Num(); ++index ) {
		idStr samplerName( "samp_" );
		const char * parmName = renderProgParms.GetParmNameForIndex( data.textureParms[index] );
		if ( parmName != NULL ) {
			samplerName += parmName;
			const int location = qglGetUniformLocation( glProgram, samplerName.c_str() );
			samplerLocations[index] = location + 1;
			if ( location != -1 ) {
				qglUniform1i( location, index );
			}
		} else {
			samplerLocations[index] = 0;
		}
	}
	// shaders converted from the legacy .vertex/.pixel files declare their
	// samplers as samp0..samp15 instead of the named samp_ uniforms; those
	// shaders select their textures through the fixed joint order of the old
	// render prog manager, so the units are assigned positionally here
	const int maxLegacySamplers = 16;
	for ( int unit = 0; unit < maxLegacySamplers; ++unit ) {
		const char * samplerName = va( "samp%i", unit );
		const int location = qglGetUniformLocation( glProgram, samplerName );
		if ( location != -1 ) {
			qglUniform1i( location, unit );
		}
	}
}

/*
=================
idDeclRenderProg::FreeProgram
=================
*/
void idDeclRenderProg::FreeProgram() {
	if ( program != 0 ) {
		qglDeleteProgram( program );
		program = 0;
	}
	// the locations belong to the deleted program; the next upload
	// resolves them again
	vertexUniformLocations.Clear();
	fragmentUniformLocations.Clear();
	samplerLocations.Clear();
}

