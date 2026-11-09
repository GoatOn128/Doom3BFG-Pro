#pragma hdrstop
#include "../idlib/precompiled.h"
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
r_dumpGeneratedGLSL
====================
*/
idCVar r_dumpGeneratedGLSL( "r_dumpGeneratedGLSL", "0", CVAR_BOOL,
	"dump the GLSL generated for renderprog decls into generated/glsl" );

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
		qglDeleteProgram( program );
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
CompileGLSL

Adapted from the Wolfenstein: The New Order GLSL compile path
(declrenderprog_ogl_pc.cpp). Compiles one stage of the generated code and
prints the numbered source lines when the driver reports a problem.
=================
*/
static unsigned int CompileGLSL( const idStr & programGLSL, bool isVertexProgram,
								const char * name, const char * fileName ) {
	const GLuint shader = qglCreateShader( isVertexProgram ? GL_VERTEX_SHADER : GL_FRAGMENT_SHADER );
	if ( shader == 0 ) {
		return 0;
	}

	const char * source = programGLSL.c_str();
	qglShaderSource( shader, 1, &source, nullptr );
	qglCompileShader( shader );

	int infoLogLength = 0;
	qglGetShaderiv( shader, GL_INFO_LOG_LENGTH, &infoLogLength );
	if ( infoLogLength > 1 ) {
		idTempArray<char> infoLog( infoLogLength );
		int charsWritten = 0;
		qglGetShaderInfoLog( shader, infoLogLength, &charsWritten, infoLog.Ptr() );

		// catch the strings the ATI and Intel drivers output on success
		if ( strstr( infoLog.Ptr(), "successfully compiled to run on hardware" ) == nullptr &&
			 strstr( infoLog.Ptr(), "No errors." ) == nullptr ) {
			idLib::Printf( "While compiling %s program %s from %s\n",
						   isVertexProgram ? "vertex" : "fragment", name, fileName );
			idLib::Printf( "-----------------\n" );
			int lineNumber = 1;
			const char * lineStart = source;
			while ( *lineStart != '\0' ) {
				const char * lineEnd = strchr( lineStart, '\n' );
				if ( lineEnd == nullptr ) {
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

Adapted from the Wolfenstein: The New Order UploadTargetCode
(declrenderprog_ogl_pc.cpp): compiles both stages, links the program, binds
the attribute and fragment data locations from the attrib table, and
resolves the uniform and sampler locations. The TNO precompiled program
binary cache is dropped.
=================
*/
void idDeclRenderProg::UploadTargetCode() {
	if ( data.vertexCode.IsEmpty() || data.fragmentCode.IsEmpty() ) {
		idLib::Warning( "renderprog %s has no generated shader code to upload", GetName() );
		return;
	}

	if ( r_dumpGeneratedGLSL.GetBool() && fileSystem != nullptr ) {
		idStr vertexName( "generated/glsl/" );
		vertexName += GetName();
		vertexName += "_vertex.glsl";
		fileSystem->WriteFile( vertexName.c_str(), data.vertexCode.c_str(), data.vertexCode.Length() );
		idStr fragmentName( "generated/glsl/" );
		fragmentName += GetName();
		fragmentName += "_fragment.glsl";
		fileSystem->WriteFile( fragmentName.c_str(), data.fragmentCode.c_str(), data.fragmentCode.Length() );
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
	for ( int index = 0; attribsPC[index].glsl != nullptr; ++index ) {
		if ( ( attribsPC[index].flags & AT_VS_IN ) != 0 ) {
			qglBindAttribLocation( glProgram, attribsPC[index].bind, attribsPC[index].glsl );
		}
	}

	// bind the fragment outputs to color numbers; gl_* built-ins like
	// gl_FragDepth are assigned by the driver and must not be bound
	for ( int index = 0; attribsPC[index].glsl != nullptr; ++index ) {
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
		if ( strstr( infoLog.Ptr(), "Vertex shader(s) linked, fragment shader(s) linked." ) == nullptr &&
			 strstr( infoLog.Ptr(), "No errors." ) == nullptr ) {
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
		const int location = ( parmName != nullptr ) ? qglGetUniformLocation( glProgram, parmName ) : -1;
		vertexUniformLocations[index] = location + 1;
	}
	fragmentUniformLocations.SetNum( data.fragmentParms.Num() );
	for ( int index = 0; index < data.fragmentParms.Num(); ++index ) {
		const char * parmName = renderProgParms.GetParmNameForIndex( data.fragmentParms[index] );
		const int location = ( parmName != nullptr ) ? qglGetUniformLocation( glProgram, parmName ) : -1;
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
		if ( parmName != nullptr ) {
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
