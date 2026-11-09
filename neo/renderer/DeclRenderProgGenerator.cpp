#pragma hdrstop
#include "../idlib/precompiled.h"
#pragma hdrstop
#include "../idlib/precompiled.h"

#include <cstdarg>
#include <cstring>

#include "DeclRenderProgGenerator.h"
#include "../idlib/Parser.h"

/*
Adapted from the Wolfenstein: The New Order idDeclRenderProgGenerator
reconstructed under references/wolfnto-reversedengineered
(declrenderproggenerator.cpp). That material is decompiled proprietary code
and is not GPL-compatible; this is a re-implementation on this engine's
idlib.

The console targets of the original are dropped and only the PC GLSL path
remains. The dead code stripper is also dropped: this engine's global.inc
contains preprocessor defines that the block splitter cannot model, and the
GLSL compiler strips unreferenced declarations anyway.
*/

namespace {

struct typeConversion_t {
	const char * typeCG;
	const char * typeGLSL;
};

struct builtinConversion_t {
	const char * nameCG;
	const char * nameGLSL;
};

struct inOutVariable_t {
	idStr type;
	idStr nameCg;
	idStr nameGLSL;
	bool declareInOut = false;
};

const typeConversion_t typeConversion[] = {
	{ "void", "void" },
	{ "fixed", "float" },
	{ "float", "float" },
	{ "float2", "vec2" },
	{ "float3", "vec3" },
	{ "float4", "vec4" },
	{ "half", "float" },
	{ "half2", "vec2" },
	{ "half3", "vec3" },
	{ "half4", "vec4" },
	{ "int", "int" },
	{ "int2", "ivec2" },
	{ "int3", "ivec3" },
	{ "int4", "ivec4" },
	{ "bool", "bool" },
	{ "bool2", "bvec2" },
	{ "bool3", "bvec3" },
	{ "bool4", "bvec4" },
	{ "float2x2", "mat2x2" },
	{ "float2x3", "mat2x3" },
	{ "float2x4", "mat2x4" },
	{ "float3x2", "mat3x2" },
	{ "float3x3", "mat3x3" },
	{ "float3x4", "mat3x4" },
	{ "float4x2", "mat4x2" },
	{ "float4x3", "mat4x3" },
	{ "float4x4", "mat4x4" },
	{ "sampler1D", "sampler1D" },
	{ "sampler2D", "sampler2D" },
	{ "sampler3D", "sampler3D" },
	{ "samplerCUBE", "samplerCube" },
	{ "sampler1DShadow", "sampler1DShadow" },
	{ "sampler2DShadow", "sampler2DShadow" },
	{ "sampler3DShadow", "sampler3DShadow" },
	{ "samplerCubeShadow", "samplerCubeShadow" },
	{ "sampler2DMS", "sampler2DMS" },
	{ nullptr, nullptr },
};

const builtinConversion_t builtinConversion[] = {
	{ "frac", "fract" },
	{ "lerp", "mix" },
	{ "rsqrt", "inversesqrt" },
	{ "ddx", "dFdx" },
	{ "ddy", "dFdy" },
	{ nullptr, nullptr },
};

const char * const vertexInsert =
	"#version 150\n\n"
	"float saturate( float v ) { return clamp( v, 0.0, 1.0 ); }\n"
	"vec2 saturate( vec2 v ) { return clamp( v, vec2( 0.0 ), vec2( 1.0 ) ); }\n"
	"vec3 saturate( vec3 v ) { return clamp( v, vec3( 0.0 ), vec3( 1.0 ) ); }\n"
	"vec4 saturate( vec4 v ) { return clamp( v, vec4( 0.0 ), vec4( 1.0 ) ); }\n"
	"vec4 tex2Dlod( sampler2D sampler, vec4 texcoord ) { return textureLod( sampler, texcoord.xy, texcoord.w ); }\n"
	"vec4 tex1Dproj( sampler1D sampler, vec2 texcoord ) { return textureProj( sampler, texcoord ); }\n"
	"vec4 tex2Dproj( sampler2D sampler, vec3 texcoord ) { return textureProj( sampler, texcoord ); }\n"
	"vec4 tex3Dproj( sampler3D sampler, vec4 texcoord ) { return textureProj( sampler, texcoord ); }\n\n";

const char * const fragmentInsert =
	"#version 150\n\n"
	"void clip( float v ) { if ( v < 0.0 ) { discard; } }\n"
	"void clip( vec2 v ) { if ( any( lessThan( v, vec2( 0.0 ) ) ) ) { discard; } }\n"
	"void clip( vec3 v ) { if ( any( lessThan( v, vec3( 0.0 ) ) ) ) { discard; } }\n"
	"void clip( vec4 v ) { if ( any( lessThan( v, vec4( 0.0 ) ) ) ) { discard; } }\n\n"
	"float saturate( float v ) { return clamp( v, 0.0, 1.0 ); }\n"
	"vec2 saturate( vec2 v ) { return clamp( v, vec2( 0.0 ), vec2( 1.0 ) ); }\n"
	"vec3 saturate( vec3 v ) { return clamp( v, vec3( 0.0 ), vec3( 1.0 ) ); }\n"
	"vec4 saturate( vec4 v ) { return clamp( v, vec4( 0.0 ), vec4( 1.0 ) ); }\n\n"
	"vec4 tex2D( sampler2D sampler, vec2 texcoord ) { return texture( sampler, texcoord.xy ); }\n"
	"vec4 tex2D( sampler2DShadow sampler, vec3 texcoord ) { return vec4( texture( sampler, texcoord.xyz ) ); }\n\n"
	"vec4 tex2D( sampler2D sampler, vec2 texcoord, vec2 dx, vec2 dy ) { return textureGrad( sampler, texcoord.xy, dx, dy ); }\n"
	"vec4 tex2D( sampler2DShadow sampler, vec3 texcoord, vec2 dx, vec2 dy ) { return vec4( textureGrad( sampler, texcoord.xyz, dx, dy ) ); }\n\n"
	"vec4 texCUBE( samplerCube sampler, vec3 texcoord ) { return texture( sampler, texcoord.xyz ); }\n"
	"vec4 texCUBE( samplerCubeShadow sampler, vec4 texcoord ) { return vec4( texture( sampler, texcoord.xyzw ) ); }\n\n"
	"vec4 tex1Dproj( sampler1D sampler, vec2 texcoord ) { return textureProj( sampler, texcoord ); }\n"
	"vec4 tex2Dproj( sampler2D sampler, vec3 texcoord ) { return textureProj( sampler, texcoord ); }\n"
	"vec4 tex3Dproj( sampler3D sampler, vec4 texcoord ) { return textureProj( sampler, texcoord ); }\n\n"
	"vec4 tex1Dbias( sampler1D sampler, vec4 texcoord ) { return texture( sampler, texcoord.x, texcoord.w ); }\n"
	"vec4 tex2Dbias( sampler2D sampler, vec4 texcoord ) { return texture( sampler, texcoord.xy, texcoord.w ); }\n"
	"vec4 tex3Dbias( sampler3D sampler, vec4 texcoord ) { return texture( sampler, texcoord.xyz, texcoord.w ); }\n"
	"vec4 texCUBEbias( samplerCube sampler, vec4 texcoord ) { return texture( sampler, texcoord.xyz, texcoord.w ); }\n\n"
	"vec4 tex1Dlod( sampler1D sampler, vec4 texcoord ) { return textureLod( sampler, texcoord.x, texcoord.w ); }\n"
	"vec4 tex2Dlod( sampler2D sampler, vec4 texcoord ) { return textureLod( sampler, texcoord.xy, texcoord.w ); }\n"
	"vec4 tex3Dlod( sampler3D sampler, vec4 texcoord ) { return textureLod( sampler, texcoord.xyz, texcoord.w ); }\n"
	"vec4 texCUBElod( samplerCube sampler, vec4 texcoord ) { return textureLod( sampler, texcoord.xyz, texcoord.w ); }\n\n";

void AppendFormat( idStr & destination, const char * format, ... ) {
	char buffer[16384];
	va_list arguments;
	va_start( arguments, format );
	idStr::vsnPrintf( buffer, sizeof( buffer ), format, arguments );
	va_end( arguments );
	destination.Append( buffer );
}

bool HasWhiteSpace( const idToken & token ) {
	return token.WhiteSpaceBeforeToken() > 0;
}

void AppendLeadingWhiteSpace( idStr & result, const idToken & token ) {
	if ( token.linesCrossed > 0 ) {
		result += '\n';
	} else if ( HasWhiteSpace( token ) ) {
		result += ' ';
	}
}

const char * ParmName( int parmIndex ) {
	const char * name = renderProgParms.GetParmNameForIndex( parmIndex );
	return name != nullptr ? name : "<badParmIndex>";
}

void CheckForAttribSemanticCollision( const idStaticList<attribInfo_t, 16> & attributes, idParser * source ) {
	if ( attributes.Num() < 2 ) {
		return;
	}
	const attribInfo_t & added = attributes[attributes.Num() - 1];
	for ( int index = 0; index < attributes.Num() - 1; ++index ) {
		if ( idStr::Icmp( attributes[index].semantic, added.semantic ) == 0 ) {
			source->Error( "Semantic collision \"%s\" and \"%s\" both resolve to semantic \"%s\"",
						  attributes[index].name, added.name, added.semantic );
			return;
		}
	}
}

int FindIndex( const idList<int> & values, int value ) {
	for ( int index = 0; index < values.Num(); ++index ) {
		if ( values[index] == value ) {
			return index;
		}
	}
	return -1;
}

template< typename T >
int FindParmIndex( const T & values, int parmIndex ) {
	for ( int index = 0; index < values.Num(); ++index ) {
		if ( values[index] == parmIndex ) {
			return index;
		}
	}
	return -1;
}

void AppendTextureDeclarations( const renderProgData_t * data, idStr & header ) {
	for ( int slot = 0; slot < data->textureParms.Num(); ++slot ) {
		const int parmIndex = data->textureParms[slot];
		AppendFormat( header, "uniform %s samp_%s : register(s%i);\n",
					  TextureTypeForTextureParm( parmIndex ), ParmName( parmIndex ), slot );
	}
}

void AppendStructMember( idStr & code, const attribInfo_t & attribute,
							 const idList<declaredMember_t> & declaredMembers ) {
	// an old style body's own in/out structs declare the exact member
	// types; the table defaults are wider (float4 texcoords) and would
	// break the unswizzled function calls in the bodies
	const char * type = attribute.type;
	for ( int index = 0; index < declaredMembers.Num(); ++index ) {
		if ( declaredMembers[index].name.Cmp( attribute.name ) == 0 ) {
			type = declaredMembers[index].type.c_str();
			break;
		}
	}
	AppendFormat( code, "\t%s %s : %s;\n", type, attribute.name, attribute.semantic );
}

void NormalizeStateToken( idToken & token ) {
	token.ToUpper();
	token.StripLeading( "GL_" );
}

bool ReadStateToken( idParser * source, idToken & token ) {
	if ( source->ReadToken( &token ) ) {
		return true;
	}
	source->Warning( "EOF in program." );
	return false;
}

// the decl parser that reads a shader body and the lexer that re-lexes
// the generated code are different types with no shared base class,
// but both expose the same token interface, so this works for either
template< typename parser_t >
void ParseInOutStruct( parser_t * source, int attribType, idList<inOutVariable_t> * variables ) {
	source->ExpectTokenString( "{" );
	while ( !source->CheckTokenString( "}" ) ) {
		inOutVariable_t variable;
		idToken token;
		if ( !source->ReadToken( &token ) ) {
			break;
		}
		variable.type = token;
		if ( !source->ReadToken( &token ) ) {
			break;
		}
		variable.nameCg = token;
		if ( !source->CheckTokenString( ":" ) ) {
			source->SkipUntilString( ";" );
			continue;
		}
		source->ReadToken( &token );
		variable.nameGLSL = token;
		source->ExpectTokenString( ";" );

		for ( int index = 0; typeConversion[index].typeCG != nullptr; ++index ) {
			if ( variable.type.Cmp( typeConversion[index].typeCG ) == 0 ) {
				variable.type = typeConversion[index].typeGLSL;
				break;
			}
		}

		bool isBuiltIn = false;
		for ( int index = 0; attribsPC[index].semantic != nullptr; ++index ) {
			if ( ( attribsPC[index].flags & attribType ) != 0 &&
				 variable.nameGLSL.Cmp( attribsPC[index].semantic ) == 0 ) {
				variable.nameGLSL = attribsPC[index].glsl;
				// a built-in like gl_FragCoord is provided by the driver and
				// must not be declared as a user in or out variable
				isBuiltIn = ( attribsPC[index].flags & AT_PS_INOUT ) != 0 ||
					idStr::Cmpn( attribsPC[index].glsl, "gl_", 3 ) == 0;
				break;
			}
		}

		variable.declareInOut = !isBuiltIn;
		for ( int index = 0; index < variables->Num(); ++index ) {
			if ( variable.nameGLSL == ( *variables )[index].nameGLSL ) {
				variable.declareInOut = false;
				break;
			}
		}
		variables->Append( variable );
	}
	source->ExpectTokenString( ";" );
}

bool AppendShaderAttribute( shaderConstructInfo_t * shaderInfo, int tableCount, bool * used,
							int requiredFlag, const idToken & attributeName, bool output,
							idParser * source, renderProgData_t * data ) {
	for ( int index = 0; index < tableCount; ++index ) {
		if ( ( attribsPC[index].flags & requiredFlag ) == 0 || used[index] ||
			 attributeName.Cmp( attribsPC[index].name ) != 0 ) {
			continue;
		}
		idStaticList<attribInfo_t, 16> & destination = output ? shaderInfo->outAttribs : shaderInfo->inAttribs;
		destination.Append( attribsPC[index] );
		used[index] = true;
		CheckForAttribSemanticCollision( destination, source );
		if ( requiredFlag == AT_VS_IN ) {
			data->vertexMask |= attribsPC[index].vertexMask;
		}
		if ( requiredFlag == AT_PS_OUT ) {
			data->fragmentOutputs = ( unsigned short )( data->fragmentOutputs + 1 );
		}
		if ( requiredFlag == AT_VS_OUT ) {
			AppendFormat( shaderInfo->outputAttribInitString, "\tresult.%s = 0;\n", attribsPC[index].name );
		}
		return true;
	}
	return false;
}

} // namespace

/*
====================
attribsPC

GLSL attribute table; each row maps an HLSL-style shader variable with a
semantic to the GLSL built-in or in/out declaration it becomes. The flags
select which program sides the row applies to.
====================
*/
attribInfo_t attribsPC[] = {
	// vertex attributes; the bind index is the PC attribute index the vertex
	// streams are uploaded to (PC_ATTRIB_INDEX_* in RenderProgs.h) and the
	// vertex mask is BIT( bind ), matching the VERTEX_MASK_* flags the draw
	// code uses to pick the vertex declaration
	{ "float4", "position", "POSITION", "in_Position", 0, AT_VS_IN, 1 },
	{ "float2", "texcoord", "TEXCOORD0", "in_TexCoord", 8, AT_VS_IN, 256 },
	{ "float2", "texcoord0", "TEXCOORD0", "in_TexCoord", 8, AT_VS_IN, 256 },
	{ "float4", "normal", "ATTR2", "in_Normal", 2, AT_VS_IN, 4 },
	{ "float4", "tangent", "ATTR9", "in_Tangent", 9, AT_VS_IN, 512 },
	{ "float4", "color", "COLOR", "in_Color", 3, AT_VS_IN, 8 },
	{ "float4", "color2", "COLOR1", "in_Color2", 4, AT_VS_IN, 16 },
	{ "float4", "position", "POSITION", "gl_Position", 0, AT_VS_OUT | AT_PS_INOUT, 0 },
	{ "float", "clip0", "CLP0", "gl_ClipDistance[0]", 0, AT_VS_OUT, 0 },
	{ "float", "clip1", "CLP1", "gl_ClipDistance[1]", 0, AT_VS_OUT, 0 },
	{ "float", "clip2", "CLP2", "gl_ClipDistance[2]", 0, AT_VS_OUT, 0 },
	{ "float", "clip3", "CLP3", "gl_ClipDistance[3]", 0, AT_VS_OUT, 0 },
	{ "float", "clip4", "CLP4", "gl_ClipDistance[4]", 0, AT_VS_OUT, 0 },
	{ "float", "clip5", "CLP5", "gl_ClipDistance[5]", 0, AT_VS_OUT, 0 },
	{ "float4", "position", "WPOS", "gl_FragCoord", 0, AT_PS_IN, 0 },
	{ "half4", "hposition", "WPOS", "gl_FragCoord", 0, AT_PS_IN, 0 },
	{ "bool", "facing", "FACE", "gl_FrontFacing", 0, AT_PS_IN | AT_PS_INOUT, 0 },
	{ "float4", "color", "COLOR", "out_FragColor0", 0, AT_PS_OUT, 0 },
	{ "half4", "hcolor", "COLOR", "out_FragColor0", 0, AT_PS_OUT, 0 },
	{ "float4", "color0", "COLOR0", "out_FragColor0", 0, AT_PS_OUT, 0 },
	{ "float4", "color1", "COLOR1", "out_FragColor1", 1, AT_PS_OUT, 0 },
	{ "float4", "color2", "COLOR2", "out_FragColor2", 2, AT_PS_OUT, 0 },
	{ "float4", "color3", "COLOR3", "out_FragColor3", 3, AT_PS_OUT, 0 },
	{ "float", "depth", "DEPTH", "gl_FragDepth", 4, AT_PS_OUT | AT_PS_INOUT, 0 },
	{ "float4", "color", "COLOR", "vofi_Color", 0, AT_VS_OUT | AT_PS_IN, 0 },
	{ "float4", "color0", "COLOR0", "vofi_Color", 0, AT_VS_OUT | AT_PS_IN, 0 },
	{ "float4", "color1", "COLOR1", "vofi_Color", 0, AT_VS_OUT | AT_PS_IN, 0 },
	{ "half4", "hcolor", "COLOR", "vofi_Color", 0, AT_PS_IN, 0 },
	{ "half4", "hcolor0", "COLOR0", "vofi_Color", 0, AT_PS_IN, 0 },
	{ "half4", "hcolor1", "COLOR1", "vofi_Color", 0, AT_PS_IN, 0 },
	{ "float2", "texcoord", "TEXCOORD0", "vofi_TexCoord0", 0, AT_VS_OUT | AT_PS_IN, 0 },
	{ "float4", "texcoord0", "TEXCOORD0", "vofi_TexCoord0", 0, AT_VS_OUT | AT_PS_IN, 0 },
	{ "float4", "texcoord1", "TEXCOORD1", "vofi_TexCoord1", 0, AT_VS_OUT | AT_PS_IN, 0 },
	{ "float4", "texcoord2", "TEXCOORD2", "vofi_TexCoord2", 0, AT_VS_OUT | AT_PS_IN, 0 },
	{ "float4", "texcoord3", "TEXCOORD3", "vofi_TexCoord3", 0, AT_VS_OUT | AT_PS_IN, 0 },
	{ "float4", "texcoord4", "TEXCOORD4", "vofi_TexCoord4", 0, AT_VS_OUT | AT_PS_IN, 0 },
	{ "float4", "texcoord5", "TEXCOORD5", "vofi_TexCoord5", 0, AT_VS_OUT | AT_PS_IN, 0 },
	{ "float4", "texcoord6", "TEXCOORD6", "vofi_TexCoord6", 0, AT_VS_OUT | AT_PS_IN, 0 },
	{ "float4", "texcoord7", "TEXCOORD7", "vofi_TexCoord7", 0, AT_VS_OUT | AT_PS_IN, 0 },
	{ "float4", "texcoord8", "TEXCOORD8", "vofi_TexCoord8", 0, AT_VS_OUT | AT_PS_IN, 0 },
	{ "float4", "texcoord9", "TEXCOORD9", "vofi_TexCoord9", 0, AT_VS_OUT | AT_PS_IN, 0 },
	{ "half4", "htexcoord0", "TEXCOORD0", "vofi_TexCoord0", 0, AT_PS_IN, 0 },
	{ "half4", "htexcoord1", "TEXCOORD1", "vofi_TexCoord1", 0, AT_PS_IN, 0 },
	{ "half4", "htexcoord2", "TEXCOORD2", "vofi_TexCoord2", 0, AT_PS_IN, 0 },
	{ "half4", "htexcoord3", "TEXCOORD3", "vofi_TexCoord3", 0, AT_PS_IN, 0 },
	{ "half4", "htexcoord4", "TEXCOORD4", "vofi_TexCoord4", 0, AT_PS_IN, 0 },
	{ "half4", "htexcoord5", "TEXCOORD5", "vofi_TexCoord5", 0, AT_PS_IN, 0 },
	{ "half4", "htexcoord6", "TEXCOORD6", "vofi_TexCoord6", 0, AT_PS_IN, 0 },
	{ "half4", "htexcoord7", "TEXCOORD7", "vofi_TexCoord7", 0, AT_PS_IN, 0 },
	{ "half4", "htexcoord8", "TEXCOORD8", "vofi_TexCoord8", 0, AT_PS_IN, 0 },
	{ "half4", "htexcoord9", "TEXCOORD9", "vofi_TexCoord9", 0, AT_PS_IN, 0 },
	{ "float", "fog", "FOG", "vofi_Fog", 0, AT_VS_OUT, 0 },
	{ "float4", "fog", "FOG", "vofi_Fog", 0, AT_PS_IN, 0 },
	{ nullptr, nullptr, nullptr, nullptr, 0, 0, 0 },
};

/*
====================
TextureTypeForTextureParm
====================
*/
const char * TextureTypeForTextureParm( int parmIndex ) {
	const idRenderProgParm * parm = renderProgParms.GetRenderParmForIndex( parmIndex );
	if ( parm == nullptr ) {
		return "<badParmIndex>";
	}
	switch ( parm->GetType() ) {
		case PT_TEXTURE_2D:		return "sampler2D";
		case PT_TEXTURE_3D:		return "sampler3D";
		case PT_TEXTURE_CUBE:	return "samplerCUBE";
		default:				return "<badParmType>";
	}
}

/*
====================
GenerateHeader

The raw global include is the header; it already declares every uniform
register, so no per-parm uniform declarations are emitted here and the GLSL
compiler strips the unused ones. Fragment programs and vertex programs that
sample textures get the sampler declarations appended.
====================
*/
void GenerateHeader( programType_t programType, const idStr * globalInclude,
					 const renderProgData_t * data, shaderConstructInfo_t * shaderInfo ) {
	idStr & header = shaderInfo->headerString;
	header.Clear();
	header += *globalInclude;

	if ( programType == PROGRAM_TYPE_VERTEX ) {
		if ( data->hasVertexTexture ) {
			AppendTextureDeclarations( data, header );
		}
		return;
	}
	AppendTextureDeclarations( data, header );
}

/*
====================
GenerateCode
====================
*/
void GenerateCode( renderProgData_t * data, shaderConstructInfo_t * vertexInfo, shaderConstructInfo_t * fragmentInfo ) {
	if ( vertexInfo->parsed ) {
		idStr & code = data->vertexCode;
		code = vertexInfo->headerString;
		code.Append( "struct VS_IN {\n" );
		for ( int index = 0; index < vertexInfo->inAttribs.Num(); ++index ) {
			AppendStructMember( code, vertexInfo->inAttribs[index], vertexInfo->declaredInMembers );
		}
		code.Append( "};\nstruct VS_OUT {\n" );

		// order the vertex outputs to match the fragment inputs first
		idList<int> used;
		for ( int fragmentIndex = 0; fragmentIndex < fragmentInfo->inAttribs.Num(); ++fragmentIndex ) {
			for ( int vertexIndex = 0; vertexIndex < vertexInfo->outAttribs.Num(); ++vertexIndex ) {
				if ( idStr::Icmp( fragmentInfo->inAttribs[fragmentIndex].semantic,
								 vertexInfo->outAttribs[vertexIndex].semantic ) == 0 ) {
					AppendStructMember( code, vertexInfo->outAttribs[vertexIndex], vertexInfo->declaredOutMembers );
					used.Append( vertexIndex );
					break;
				}
			}
		}
		for ( int vertexIndex = 0; vertexIndex < vertexInfo->outAttribs.Num(); ++vertexIndex ) {
			if ( FindIndex( used, vertexIndex ) == -1 ) {
				AppendStructMember( code, vertexInfo->outAttribs[vertexIndex], vertexInfo->declaredOutMembers );
			}
		}
		code.Append( "};\nvoid main( VS_IN vertex, out VS_OUT result ) {\n" );
		code.Append( vertexInfo->sourceString );
		code.Append( "\n}\n" );
	}

	if ( fragmentInfo->parsed ) {
		idStr & code = data->fragmentCode;
		code = fragmentInfo->headerString;
		code.Append( "struct PS_IN {\n" );
		for ( int index = 0; index < fragmentInfo->inAttribs.Num(); ++index ) {
			AppendStructMember( code, fragmentInfo->inAttribs[index], fragmentInfo->declaredInMembers );
		}
		code.Append( "};\nstruct PS_OUT {\n" );
		for ( int index = 0; index < fragmentInfo->outAttribs.Num(); ++index ) {
			AppendStructMember( code, fragmentInfo->outAttribs[index], fragmentInfo->declaredOutMembers );
		}
		code.Append( "};\n" );
		code.Append( "void main( PS_IN fragment, out PS_OUT result ) {\n" );
		code.Append( fragmentInfo->sourceString );
		code.Append( "\n}\n" );
	}
}

/*
====================
shaderConstructInfo_t::Clear
====================
*/
void shaderConstructInfo_t::Clear() {
	parsed = false;
	headerString.Clear();
	sourceString.Clear();
	outputAttribInitString.Clear();
	inAttribs.Clear();
	outAttribs.Clear();
	declaredInMembers.Clear();
	declaredOutMembers.Clear();
}

/*
====================
ParseState
====================
*/
bool ParseState( idParser * source, renderProgData_t * data ) {
	idToken token;
	if ( !source->ExpectTokenString( "{" ) ) {
		return false;
	}
	while ( source->ReadToken( &token ) ) {
		if ( token.Icmp( "}" ) == 0 ) {
			return true;
		}

		if ( token.Icmp( "blend" ) == 0 ) {
			if ( !ReadStateToken( source, token ) ) {
				return false;
			}
			NormalizeStateToken( token );
			if ( token.Icmp( "ONE" ) == 0 ) {
			} else if ( token.Icmp( "ZERO" ) == 0 ) { data->glState |= 0x1;
			} else if ( token.Icmp( "DST_COLOR" ) == 0 ) { data->glState |= 0x2;
			} else if ( token.Icmp( "ONE_MINUS_DST_COLOR" ) == 0 ) { data->glState |= 0x3;
			} else if ( token.Icmp( "SRC_ALPHA" ) == 0 ) { data->glState |= 0x4;
			} else if ( token.Icmp( "ONE_MINUS_SRC_ALPHA" ) == 0 ) { data->glState |= 0x5;
			} else if ( token.Icmp( "DST_ALPHA" ) == 0 ) { data->glState |= 0x6;
			} else if ( token.Icmp( "ONE_MINUS_DST_ALPHA" ) == 0 ) { data->glState |= 0x7;
			} else {
				source->Warning( "bad blend mode %s", token.c_str() );
				return false;
			}
			if ( !ReadStateToken( source, token ) ) {
				return false;
			}
			NormalizeStateToken( token );
			if ( token.Icmp( "ZERO" ) == 0 ) {
			} else if ( token.Icmp( "ONE" ) == 0 ) { data->glState |= 0x8;
			} else if ( token.Icmp( "SRC_COLOR" ) == 0 ) { data->glState |= 0x10;
			} else if ( token.Icmp( "ONE_MINUS_SRC_COLOR" ) == 0 ) { data->glState |= 0x18;
			} else if ( token.Icmp( "SRC_ALPHA" ) == 0 ) { data->glState |= 0x20;
			} else if ( token.Icmp( "ONE_MINUS_SRC_ALPHA" ) == 0 ) { data->glState |= 0x28;
			} else if ( token.Icmp( "DST_ALPHA" ) == 0 ) { data->glState |= 0x30;
			} else if ( token.Icmp( "ONE_MINUS_DST_ALPHA" ) == 0 ) { data->glState |= 0x38;
			} else {
				source->Warning( "bad blend mode %s", token.c_str() );
				return false;
			}
		} else if ( token.Icmp( "blendOp" ) == 0 ) {
			if ( !ReadStateToken( source, token ) ) {
				return false;
			}
			NormalizeStateToken( token );
			if ( token.Icmp( "ADD" ) == 0 ) {
			} else if ( token.Icmp( "MIN" ) == 0 ) { data->glState |= 0x80000;
			} else if ( token.Icmp( "MAX" ) == 0 ) { data->glState |= 0xC0000;
			} else if ( token.Icmp( "SUB" ) == 0 ) { data->glState |= 0x40000;
			} else {
				source->Warning( "bad blend op %s", token.c_str() );
				return false;
			}
		} else if ( token.Icmp( "depthFunc" ) == 0 ) {
			if ( !ReadStateToken( source, token ) ) {
				return false;
			}
			NormalizeStateToken( token );
			if ( token.Icmp( "LEQUAL" ) == 0 ) {
			} else if ( token.Icmp( "GREATER" ) == 0 ) { data->glState |= 0x2000;
			} else if ( token.Icmp( "ALWAYS" ) == 0 ) { data->glState |= 0x4000;
			} else if ( token.Icmp( "EQUAL" ) == 0 ) { data->glState |= 0x6000;
			} else {
				source->Warning( "bad depthFunc %s", token.c_str() );
				return false;
			}
		} else if ( token.Icmp( "wireframe" ) == 0 ) {
			data->glState |= 0x800;
		} else if ( token.Icmp( "alphaCoverage" ) == 0 ) {
			data->glState |= 0x20000;
			data->hasAlphaToCoverage = true;
		} else if ( token.Icmp( "depthMask" ) == 0 ) {
			data->glState |= 0x40;
		} else if ( token.Icmp( "colorMask" ) == 0 ) {
			data->glState |= 0x380;
		} else if ( token.Icmp( "alphaMask" ) == 0 ) {
			data->glState |= 0x400;
		} else if ( token.Icmp( "twoSided" ) == 0 ) {
			data->glState |= 0x8000;
		} else if ( token.Icmp( "backSided" ) == 0 ) {
			data->glState |= 0x10000;
		} else if ( token.Icmp( "polygonOffset" ) == 0 ) {
			data->glState |= 0x1000;
		} else if ( token.Icmp( "stencilOp" ) == 0 ) {
			for ( int shift = 0; shift < 9; shift += 3 ) {
				if ( !ReadStateToken( source, token ) ) {
					return false;
				}
				NormalizeStateToken( token );
				unsigned long long operation = 0;
				if ( token.Icmp( "KEEP" ) == 0 ) { operation = 0;
				} else if ( token.Icmp( "ZERO" ) == 0 ) { operation = 0x8000000000ull;
				} else if ( token.Icmp( "REPLACE" ) == 0 ) { operation = 0x10000000000ull;
				} else if ( token.Icmp( "INCR" ) == 0 ) { operation = 0x18000000000ull;
				} else if ( token.Icmp( "DECR" ) == 0 ) { operation = 0x20000000000ull;
				} else if ( token.Icmp( "INVERT" ) == 0 ) { operation = 0x28000000000ull;
				} else if ( token.Icmp( "INCR_WRAP" ) == 0 ) { operation = 0x30000000000ull;
				} else if ( token.Icmp( "DECR_WRAP" ) == 0 ) { operation = 0x38000000000ull;
				} else {
					source->Warning( "bad stencilOp %s", token.c_str() );
					return false;
				}
				data->glState |= operation << shift;
			}
		} else if ( token.Icmp( "stencilFunc" ) == 0 ) {
			if ( !ReadStateToken( source, token ) ) {
				return false;
			}
			NormalizeStateToken( token );
			if ( token.Icmp( "ALWAYS" ) == 0 ) {
			} else if ( token.Icmp( "LESS" ) == 0 ) { data->glState |= 0x1000000000ull;
			} else if ( token.Icmp( "LEQUAL" ) == 0 ) { data->glState |= 0x2000000000ull;
			} else if ( token.Icmp( "GREATER" ) == 0 ) { data->glState |= 0x3000000000ull;
			} else if ( token.Icmp( "GEQUAL" ) == 0 ) { data->glState |= 0x4000000000ull;
			} else if ( token.Icmp( "EQUAL" ) == 0 ) { data->glState |= 0x5000000000ull;
			} else if ( token.Icmp( "NOTEQUAL" ) == 0 ) { data->glState |= 0x6000000000ull;
			} else if ( token.Icmp( "NEVER" ) == 0 ) { data->glState |= 0x7000000000ull;
			} else {
				source->Warning( "bad stencilFunc %s", token.c_str() );
				return false;
			}
			if ( source->ReadTokenOnLine( &token ) ) {
				data->glState |= ( unsigned long long )( token.type == TT_NUMBER ? token.GetIntValue() : 0 ) << 20;
			}
			if ( source->ReadTokenOnLine( &token ) ) {
				data->glState |= ( unsigned long long )( token.type == TT_NUMBER ? token.GetIntValue() : 0 ) << 28;
			}
		} else {
			source->Warning( "Unknown drawing state option '%s'", token.c_str() );
		}
	}
	source->Warning( "Unexpected EOF in state block" );
	return false;
}

/*
====================
ParseFlags
====================
*/
bool ParseFlags( idParser * source, renderProgData_t * data ) {
	idToken token;
	if ( !source->ExpectTokenString( "{" ) ) {
		return false;
	}
	while ( source->ReadToken( &token ) ) {
		if ( token.Icmp( "}" ) == 0 ) {
			return true;
		}
		if ( token.Icmp( "perforated" ) == 0 ) {
			data->flags |= 1;
		} else if ( token.Icmp( "sky" ) == 0 ) {
			data->flags |= 2;
		} else if ( token.Icmp( "modelfade" ) == 0 ) {
			data->flags |= 4;
		} else {
			source->Warning( "Unknown flag option '%s'", token.c_str() );
		}
	}
	source->Warning( "Unexpected EOF in flags block" );
	return false;
}

/*
====================
ParseShader

Translates the body of an hlsl_vp/hlsl_fp block into the shader construct
info. Render parm references ($parm) become registry lookups, member
references on vertex/fragment/result become attribute table lookups, and
every other token is copied through with its original white space.
====================
*/
bool ParseShader( programType_t programType, idParser * source, const idStr * globalInclude,
				  renderProgData_t * data, shaderConstructInfo_t * shaderInfo ) {
	if ( programType == PROGRAM_TYPE_VERTEX ) {
		data->has16BitScaleBias = false;
		data->hasHardwareSkinning = false;
		data->hasVertexTexture = false;
		data->vertexMask = 0;
		data->vertexParms.Clear();
	} else {
		data->hasClip = false;
		data->hasDepthOutput = false;
		data->hasBranchStripping = false;
		data->fragmentOutputs = 0;
		data->fragmentParms.Clear();
		if ( !data->hasVertexTexture ) {
			data->textureParms.Clear();
			data->samplerParms.Clear();
		}
	}

	attribInfo_t * table = attribsPC;
	int tableCount = 0;
	while ( table[tableCount].name != nullptr ) {
		++tableCount;
	}
	bool inputUsed[64];
	bool outputUsed[64];
	memset( inputUsed, 0, sizeof( inputUsed ) );
	memset( outputUsed, 0, sizeof( outputUsed ) );

	idStaticList<short, 42> & uniformParms = ( programType == PROGRAM_TYPE_VERTEX ) ? data->vertexParms : data->fragmentParms;
	const char * programName = programType == PROGRAM_TYPE_VERTEX ? "vertex" : "fragment";
	bool usesPosition = false;
	bool usesTexcoord0 = false;

	// uniforms declared at body scope (old-style shader bodies) are moved
	// here so they stay at global scope in the generated GLSL
	idStr headerExtra;

	idToken token;
	if ( !source->ExpectTokenString( "{" ) ) {
		return false;
	}
	int braceDepth = 1;
	while ( source->ReadToken( &token ) ) {
		if ( token == "$" ) {
			if ( !source->ReadToken( &token ) ) {
				break;
			}
			idStr renderParmName( token.c_str() );
			const idRenderProgParm * renderParm = renderProgParms.FindParmByName( renderParmName.c_str() );
			if ( renderParm == nullptr ) {
				source->Warning( "render parm '%s' not found for %s program", renderParmName.c_str(), programName );
				return false;
			}

			if ( renderParm->GetType() == PT_VECTOR ) {
				if ( FindParmIndex( uniformParms, renderParm->GetIndex() ) == -1 ) {
					if ( uniformParms.Num() >= uniformParms.Max() ) {
						source->Warning( "more than %d render parms used in %s program", uniformParms.Max(), programName );
						return false;
					}
					uniformParms.Append( static_cast< short >( renderParm->GetIndex() ) );
				}
				// the parm name is the GLSL uniform declared by global.inc
				AppendFormat( shaderInfo->sourceString, " %s", renderParm->GetName() );
			} else if ( renderParm->GetType() >= PT_TEXTURE_2D && renderParm->GetType() <= PT_TEXTURE_CUBE ) {
				const int textureIndex = FindParmIndex( data->textureParms, renderParm->GetIndex() );
				if ( textureIndex == -1 ) {
					if ( data->textureParms.Num() >= data->textureParms.Max() ) {
						source->Warning( "more than %d textures used in %s program", data->textureParms.Max(), programName );
						return false;
					}
					data->textureParms.Append( static_cast< short >( renderParm->GetIndex() ) );
					// GL combines the texture and the sampler, so the same
					// parm index is stored for the sampler
					data->samplerParms.Append( static_cast< short >( renderParm->GetIndex() ) );
				}
				if ( programType == PROGRAM_TYPE_VERTEX ) {
					data->hasVertexTexture = true;
					source->Warning( "sampling a texture in the vertex program" );
				}
				AppendFormat( shaderInfo->sourceString, " samp_%s", renderParm->GetName() );
			} else {
				source->Warning( "invalid render parm '%s' used in %s program", renderParmName.c_str(), programName );
				return false;
			}
			continue;
		}

		if ( token.type == TT_NAME ) {
			if ( programType == PROGRAM_TYPE_VERTEX && token == "vertex" && source->CheckTokenString( "." ) ) {
				idToken attributeName;
				source->ReadToken( &attributeName );
				token += ".";
				token += attributeName;
				AppendShaderAttribute( shaderInfo, tableCount, inputUsed, AT_VS_IN, attributeName, false, source, data );
				usesPosition |= ( attributeName.Cmp( "position" ) == 0 );
				usesTexcoord0 |= ( attributeName.Cmp( "texcoord0" ) == 0 );
			} else if ( programType == PROGRAM_TYPE_VERTEX && token == "result" && source->CheckTokenString( "." ) ) {
				idToken attributeName;
				source->ReadToken( &attributeName );
				token += ".";
				token += attributeName;
				AppendShaderAttribute( shaderInfo, tableCount, outputUsed, AT_VS_OUT, attributeName, true, source, data );
			} else if ( programType == PROGRAM_TYPE_VERTEX && token.Cmpn( "hardwareSkinning", 16 ) == 0 ) {
				data->hasHardwareSkinning = true;
			} else if ( programType == PROGRAM_TYPE_FRAGMENT && token == "fragment" && source->CheckTokenString( "." ) ) {
				idToken attributeName;
				source->ReadToken( &attributeName );
				token += ".";
				token += attributeName;
				AppendShaderAttribute( shaderInfo, tableCount, inputUsed, AT_PS_IN, attributeName, false, source, data );
			} else if ( programType == PROGRAM_TYPE_FRAGMENT && token == "result" && source->CheckTokenString( "." ) ) {
				idToken attributeName;
				source->ReadToken( &attributeName );
				token += ".";
				token += attributeName;
				AppendShaderAttribute( shaderInfo, tableCount, outputUsed, AT_PS_OUT, attributeName, true, source, data );
				if ( attributeName.Cmp( "depth" ) == 0 ) {
					data->hasDepthOutput = true;
				}
			} else if ( programType == PROGRAM_TYPE_FRAGMENT && token == "clip" ) {
				data->hasClip = true;
			} else if ( token == "struct" ) {
				// old style shader bodies declare their own in/out structs;
				// the attribute sets are derived from the body references,
				// but the declared member types are kept: the stock shader
				// files pass narrow texcoords (float2/float3) that the
				// table defaults would widen to float4, which breaks the
				// unswizzled function calls in the bodies
				idToken structName;
				int structAttribType = 0;
				bool recognizedStruct = false;
				if ( source->ReadToken( &structName ) ) {
					if ( structName == "VS_IN" ) {
						structAttribType = AT_VS_IN;
						recognizedStruct = true;
					} else if ( structName == "VS_OUT" ) {
						structAttribType = AT_VS_OUT;
						recognizedStruct = true;
					} else if ( structName == "PS_IN" ) {
						structAttribType = AT_PS_IN;
						recognizedStruct = true;
					} else if ( structName == "PS_OUT" ) {
						structAttribType = AT_PS_OUT;
						recognizedStruct = true;
					} else {
						source->UnreadToken( &structName );
					}
				}
				if ( recognizedStruct ) {
					idList<inOutVariable_t> declaredVariables;
					ParseInOutStruct( source, structAttribType, &declaredVariables );
					idList<declaredMember_t> & declaredMembers =
						( structAttribType == AT_VS_IN || structAttribType == AT_PS_IN )
						? shaderInfo->declaredInMembers : shaderInfo->declaredOutMembers;
					for ( int memberIndex = 0; memberIndex < declaredVariables.Num(); ++memberIndex ) {
						declaredMember_t member;
						member.name = declaredVariables[memberIndex].nameCg;
						member.type = declaredVariables[memberIndex].type;
						declaredMembers.Append( member );
					}
					continue;
				}
				int structDepth = 0;
				idToken skip;
				while ( source->ReadToken( &skip ) ) {
					if ( skip == "{" ) {
						++structDepth;
					} else if ( skip == "}" && --structDepth <= 0 ) {
						break;
					}
				}
				source->CheckTokenString( ";" );
				continue;
			} else if ( token == "void" && source->CheckTokenString( "main" ) ) {
				// old-style bodies wrap the statements in a main function;
				// the generator writes its own main, so only the parameter
				// list is skipped here
				idToken skip;
				while ( source->ReadToken( &skip ) ) {
					if ( skip == ")" ) {
						break;
					}
				}
				continue;
			} else if ( token == "uniform" ) {
				// old-style bodies declare the joint matrix uniform block,
				// the positional samplers, and the user render parms at
				// body scope; all of them are moved to the header so they
				// stay at global scope in GLSL
				idToken next;
				if ( source->PeekTokenString( "matrices_ubo" ) ) {
					source->ReadToken( &next );	// matrices_ubo
					// the capture loop below emits the braces of the block itself;
					// emitting an opening brace here as well doubled the block
					// opener and left the generated GLSL unbalanced
					AppendFormat( headerExtra, "uniform matrices_ubo" );
					int blockDepth = 0;
					while ( source->ReadToken( &next ) ) {
						if ( next == "{" ) {
							++blockDepth;
							AppendFormat( headerExtra, " {" );
						} else if ( next == "}" ) {
							if ( --blockDepth <= 0 ) {
								break;
							}
							AppendFormat( headerExtra, " }" );
						} else {
							AppendFormat( headerExtra, " %s", next.c_str() );
						}
					}
					AppendFormat( headerExtra, " };\n" );
					source->CheckTokenString( ";" );
					// this engine's skinned programs declare matrices_ubo;
					// the TNO hardwareSkinning keyword is never used here
					data->hasHardwareSkinning = true;
					continue;
				}
				// capture the rest of the declaration; the CG types and the
				// register annotations are cleaned up by ConvertCG2GLSL
				AppendFormat( headerExtra, "uniform" );
				while ( source->ReadToken( &next ) ) {
					if ( next == ":" ) {
						// a register annotation runs to the end of the
						// declaration; drop it together with the colon
						source->SkipUntilString( ";" );
						AppendFormat( headerExtra, ";\n" );
						break;
					}
					AppendFormat( headerExtra, " %s", next.c_str() );
					if ( next == ";" ) {
						break;
					}
				}
				AppendFormat( headerExtra, "\n" );
				continue;
			} else {
				// old-style bodies reference the vector parms by their plain
				// registry name instead of with a $ prefix
				const idRenderProgParm * renderParm = renderProgParms.FindParmByName( token.c_str() );
				if ( renderParm == nullptr ) {
					// the global.inc helpers read vector parms the body never
					// names directly; register the parm when such a helper is
					// called so its uniform gets a location and is committed
					static const char * const helperParms[][2] = {
						{ "vposToScreenPosTexCoord", "rpWindowCoord" }
					};
					const int helperCount = static_cast< int >( sizeof( helperParms ) / sizeof( helperParms[0] ) );
					for ( int helperIndex = 0; helperIndex < helperCount; ++helperIndex ) {
						if ( token == helperParms[helperIndex][0] ) {
							renderParm = renderProgParms.FindParmByName( helperParms[helperIndex][1] );
							break;
						}
					}
				}
				if ( renderParm != nullptr && renderParm->GetType() == PT_VECTOR ) {
					if ( FindParmIndex( uniformParms, renderParm->GetIndex() ) == -1 ) {
						if ( uniformParms.Num() >= uniformParms.Max() ) {
							source->Warning( "more than %d render parms used in %s program", uniformParms.Max(), programName );
							return false;
						}
						uniformParms.Append( static_cast< short >( renderParm->GetIndex() ) );
					}
				}
			}
		}

		if ( token == "{" ) {
			++braceDepth;
		} else if ( token == "}" && --braceDepth <= 0 ) {
			break;
		}

		if ( token.linesCrossed > 0 ) {
			shaderInfo->sourceString += '\n';
		}
		if ( HasWhiteSpace( token ) ) {
			shaderInfo->sourceString += ' ';
		}
		if ( token.CmpPrefix( "rp_name_" ) == 0 ) {
			source->Warning( "%s:, \"rp_name_\" is not allowed as prefix (used internally by compiler)", token.c_str() );
		}
		shaderInfo->sourceString += token;
	}

	// The TNO engine pairs this flag with the vertexXYZScale/vertexXYZBias/
	// vertexStScaleBias parms, which this engine does not have, so any
	// program that reads position or texcoord0 cannot use scale/bias.
	if ( programType == PROGRAM_TYPE_VERTEX ) {
		data->has16BitScaleBias = true;
		if ( usesPosition ) {
			data->has16BitScaleBias = false;
		}
		if ( usesTexcoord0 ) {
			data->has16BitScaleBias = false;
		}
	}

	if ( shaderInfo->sourceString.IsEmpty() ) {
		( programType == PROGRAM_TYPE_VERTEX ? data->vertexCode : data->fragmentCode ).Empty();
	} else {
		shaderInfo->parsed = true;
		GenerateHeader( programType, globalInclude, data, shaderInfo );
		// uniforms captured from the body scope go after the shared include
		shaderInfo->headerString += headerExtra;
	}
	return true;
}

/*
====================
ConvertCG2GLSL

Converts the generated CG-style program (struct VS_IN/VS_OUT/PS_IN/PS_OUT
with semantics, a main taking the structs, and HLSL builtins) into plain
GLSL: the structs become in/out declarations with the attrib table names,
member references become plain variables, and CG builtins are renamed.
====================
*/
idStr ConvertCG2GLSL( const idStr & input, const char * name, bool isVertexProgram ) {
	idStr result( isVertexProgram ? vertexInsert : fragmentInsert );
	if ( !isVertexProgram ) {
		result.ReAllocate( input.Length() * 2 + idStr::Length( fragmentInsert ), true );
	}

	idList<inOutVariable_t> variablesIn;
	idList<inOutVariable_t> variablesOut;
	idLexer source( LEXFL_NOFATALERRORS );
	source.LoadMemory( input.c_str(), input.Length(), name );
	bool inMain = false;
	idStr newline( "\n" );

	idToken token;
	while ( source.ReadToken( &token ) ) {
		if ( token == "struct" ) {
			if ( source.CheckTokenString( "VS_IN" ) ) {
				ParseInOutStruct( &source, AT_VS_IN, &variablesIn );
				result += "\n\n";
				for ( int index = 0; index < variablesIn.Num(); ++index ) {
					if ( variablesIn[index].declareInOut ) {
						AppendFormat( result, "in %s %s;\n", variablesIn[index].type.c_str(), variablesIn[index].nameGLSL.c_str() );
					}
				}
				continue;
			}
			if ( source.CheckTokenString( "VS_OUT" ) ) {
				ParseInOutStruct( &source, AT_VS_OUT, &variablesOut );
				result += '\n';
				for ( int index = 0; index < variablesOut.Num(); ++index ) {
					if ( variablesOut[index].declareInOut ) {
						AppendFormat( result, "out %s %s;\n", variablesOut[index].type.c_str(), variablesOut[index].nameGLSL.c_str() );
					}
				}
				continue;
			}
			if ( source.CheckTokenString( "PS_IN" ) ) {
				ParseInOutStruct( &source, AT_PS_IN, &variablesIn );
				result += "\n\n";
				for ( int index = 0; index < variablesIn.Num(); ++index ) {
					if ( variablesIn[index].declareInOut ) {
						AppendFormat( result, "in %s %s;\n", variablesIn[index].type.c_str(), variablesIn[index].nameGLSL.c_str() );
					}
				}
				continue;
			}
			if ( source.CheckTokenString( "PS_OUT" ) ) {
				ParseInOutStruct( &source, AT_PS_OUT, &variablesOut );
				result += '\n';
				for ( int index = 0; index < variablesOut.Num(); ++index ) {
					if ( variablesOut[index].declareInOut ) {
						AppendFormat( result, "out %s %s;\n", variablesOut[index].type.c_str(), variablesOut[index].nameGLSL.c_str() );
					}
				}
				continue;
			}
		}

		if ( token == "static" ) {
			AppendLeadingWhiteSpace( result, token );
			source.SkipWhiteSpace( true );
			continue;
		}

		if ( token == ":" && source.CheckTokenString( "register" ) ) {
			source.SkipUntilString( ";" );
			result += ';';
			continue;
		}

		if ( token == "void" && source.CheckTokenString( "main" ) ) {
			source.ExpectTokenString( "(" );
			while ( source.ReadToken( &token ) && token != ")" ) {
			}
			result += "\nvoid main()";
			inMain = true;
			continue;
		}

		if ( token == "const" && inMain ) {
			AppendLeadingWhiteSpace( result, token );
			source.SkipWhiteSpace( true );
			continue;
		}

		if ( token == "{" ) {
			AppendLeadingWhiteSpace( result, token );
			result += '{';
			if ( newline.Length() < 127 ) {
				newline += '\t';
			}
			continue;
		}
		if ( token == "}" ) {
			if ( !newline.IsEmpty() ) {
				const int length = newline.Length();
				newline.CapLength( length > 1 ? length - 1 : 0 );
			}
			AppendLeadingWhiteSpace( result, token );
			result += '}';
			continue;
		}

		bool converted = false;
		for ( int index = 0; typeConversion[index].typeCG != nullptr; ++index ) {
			if ( token.Cmp( typeConversion[index].typeCG ) == 0 ) {
				AppendLeadingWhiteSpace( result, token );
				result += typeConversion[index].typeGLSL;
				converted = true;
				break;
			}
		}
		if ( converted ) {
			continue;
		}

		if ( source.CheckTokenString( "." ) ) {
			if ( token == "vertex" || token == "fragment" ) {
				idToken member;
				source.ReadToken( &member );
				AppendLeadingWhiteSpace( result, token );
				bool found = false;
				for ( int index = 0; index < variablesIn.Num(); ++index ) {
					if ( member.Cmp( variablesIn[index].nameCg.c_str() ) == 0 ) {
						result += variablesIn[index].nameGLSL;
						found = true;
						break;
					}
				}
				if ( !found ) {
					source.Error( "invalid input parameter %s.%s", token.c_str(), member.c_str() );
					result += token;
					result += '.';
					result += member;
				}
				continue;
			}
			if ( token == "result" ) {
				idToken member;
				source.ReadToken( &member );
				AppendLeadingWhiteSpace( result, token );
				bool found = false;
				for ( int index = 0; index < variablesOut.Num(); ++index ) {
					if ( member.Cmp( variablesOut[index].nameCg.c_str() ) == 0 ) {
						result += variablesOut[index].nameGLSL;
						found = true;
						break;
					}
				}
				if ( !found ) {
					source.Error( "invalid output parameter %s.%s", token.c_str(), member.c_str() );
					result += token;
					result += '.';
					result += member;
				}
				continue;
			}
			AppendLeadingWhiteSpace( result, token );
			result += token;
			result += '.';
			continue;
		}

		for ( int index = 0; builtinConversion[index].nameCG != nullptr; ++index ) {
			if ( token.Cmp( builtinConversion[index].nameCG ) == 0 ) {
				AppendLeadingWhiteSpace( result, token );
				result += builtinConversion[index].nameGLSL;
				converted = true;
				break;
			}
		}
		if ( converted ) {
			continue;
		}

		AppendLeadingWhiteSpace( result, token );
		result += token;
	}
	return result;
}
