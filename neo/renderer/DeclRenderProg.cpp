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
along with Doom 3 BFG Edition Source Code.  If not, please request a copy in writing to id Software at the address below.

id Software LLC, c/o ZeniMax Media Inc., Suite 120, Rockville, Maryland 20850 USA

===========================================================================
*/
#pragma hdrstop
#include "../idlib/precompiled.h"

#include "tr_local.h"
#include "DeclRenderProg.h"

/*
================================================================================================
IsShaderWhitespace
================================================================================================
*/
static ID_INLINE bool IsShaderWhitespace( char c ) {
	return c == ' ' || c == '\t' || c == '\r' || c == '\n';
}

/*
================================================================================================
IsShaderIdentChar
================================================================================================
*/
static ID_INLINE bool IsShaderIdentChar( char c ) {
	return ( c >= 'a' && c <= 'z' ) || ( c >= 'A' && c <= 'Z' ) || ( c >= '0' && c <= '9' ) || c == '_';
}

/*
================================================================================================
SkipCommentsAndWhitespace
================================================================================================
*/
static const char * SkipCommentsAndWhitespace( const char * text, const char * end ) {
	while ( text < end ) {
		char c = text[0];
		if ( IsShaderWhitespace( c ) ) {
			text++;
		} else if ( c == '/' && text + 1 < end && text[1] == '/' ) {
			text += 2;
			while ( text < end && text[0] != '\n' ) {
				text++;
			}
		} else if ( c == '/' && text + 1 < end && text[1] == '*' ) {
			text += 2;
			while ( text < end ) {
				if ( text[0] == '*' && text + 1 < end && text[1] == '/' ) {
					text += 2;
					break;
				}
				text++;
			}
		} else {
			break;
		}
	}
	return text;
}

/*
================================================================================================
MatchKeyword
================================================================================================
*/
static bool MatchKeyword( const char * text, const char * keyword, int keywordLength ) {
	for ( int i = 0; i < keywordLength; i++ ) {
		if ( idStr::ToLower( text[i] ) != idStr::ToLower( keyword[i] ) ) {
			return false;
		}
	}
	return true;
}

/*
================================================================================================
ExtractShaderBlock
Scans the decl text for the shader block with the given keyword and
copies the block contents verbatim, without the outer braces. Comments
are skipped during the scan so braces inside them cannot unbalance it.
================================================================================================
*/
static bool ExtractShaderBlock( const char * text, int textLength, const char * keyword, idStr & out ) {
	const int keywordLength = idStr::Length( keyword );
	const char * end = text + textLength;
	const char * scan = text;

	while ( scan < end ) {
		scan = SkipCommentsAndWhitespace( scan, end );
		if ( scan >= end ) {
			return false;
		}

		const bool startOfWord = ( scan == text ) || !IsShaderIdentChar( scan[-1] );
		const bool endOfWord = ( scan + keywordLength >= end ) || !IsShaderIdentChar( scan[keywordLength] );
		if ( startOfWord && endOfWord && MatchKeyword( scan, keyword, keywordLength ) ) {
			const char * body = SkipCommentsAndWhitespace( scan + keywordLength, end );
			if ( body >= end || body[0] != '{' ) {
				return false;
			}

			// walk the block until the matching closing brace
			int depth = 1;
			const char * p = body + 1;
			while ( p < end ) {
				p = SkipCommentsAndWhitespace( p, end );
				if ( p >= end ) {
					return false;
				}
				if ( p[0] == '{' ) {
					depth++;
				} else if ( p[0] == '}' ) {
					depth--;
					if ( depth == 0 ) {
						break;
					}
				}
				p++;
			}
			if ( depth != 0 ) {
				return false;
			}
			const char * blockEnd = p;

			// copy the block contents without the surrounding whitespace
			const char * first = body + 1;
			while ( first < blockEnd && IsShaderWhitespace( first[0] ) ) {
				first++;
			}
			const char * last = blockEnd;
			while ( last > first && IsShaderWhitespace( last[-1] ) ) {
				last--;
			}
			out.CopyRange( text, first - text, last - text );
			return true;
		}

		scan++;
	}
	return false;
}

/*
================================================================================================
idDeclRenderProg::DefaultDefinition
================================================================================================
*/
const char * idDeclRenderProg::DefaultDefinition() const {
	return "{\n"
		"\thlsl_vp {\n"
		"\t\tstruct VS_IN {\n"
		"\t\t\tfloat4 position : POSITION;\n"
		"\t\t};\n"
		"\t\tstruct VS_OUT {\n"
		"\t\t\tfloat4 position : POSITION;\n"
		"\t\t};\n"
		"\t\tvoid main( VS_IN vertex, out VS_OUT result ) {\n"
		"\t\t\tresult.position.x = dot4( vertex.position, rpMVPmatrixX );\n"
		"\t\t\tresult.position.y = dot4( vertex.position, rpMVPmatrixY );\n"
		"\t\t\tresult.position.z = dot4( vertex.position, rpMVPmatrixZ );\n"
		"\t\t\tresult.position.w = dot4( vertex.position, rpMVPmatrixW );\n"
		"\t\t}\n"
		"\t}\n"
		"\thlsl_fp {\n"
		"\t\tstruct PS_OUT {\n"
		"\t\t\tfloat4 color : COLOR;\n"
		"\t\t};\n"
		"\t\tvoid main( out PS_OUT result ) {\n"
		"\t\t\tresult.color = float4( 0.2, 1.0, 0.4, 1.0 );\n"
		"\t\t}\n"
		"\t}\n"
		"}\n";
}

/*
================================================================================================
idDeclRenderProg::Parse
================================================================================================
*/
bool idDeclRenderProg::Parse( const char *text, const int textLength, bool allowBinaryVersion ) {
	FreeData();

	idStr vertex;
	idStr fragment;
	const bool hasVertex = ExtractShaderBlock( text, textLength, "hlsl_vp", vertex );
	const bool hasFragment = ExtractShaderBlock( text, textLength, "hlsl_fp", fragment );

	if ( !hasVertex || !hasFragment ) {
		idLib::Warning( "renderprog '%s' is missing an %s block", GetName(), hasVertex ? "hlsl_fp" : "hlsl_vp" );
		MakeDefault();
		return false;
	}

	vertexSource = vertex;
	fragmentSource = fragment;
	return true;
}

/*
================================================================================================
idDeclRenderProg::FreeData
================================================================================================
*/
void idDeclRenderProg::FreeData() {
	vertexSource.Clear();
	fragmentSource.Clear();
}

/*
================================================================================================
idDeclRenderProg::Size
================================================================================================
*/
size_t idDeclRenderProg::Size() const {
	return sizeof( *this ) + vertexSource.Allocated() + fragmentSource.Allocated();
}

/*
================================================================================================
idDeclRenderProg::List
================================================================================================
*/
void idDeclRenderProg::List() const {
	idLib::Printf( "%4d %4d %s\n", vertexSource.Length(), fragmentSource.Length(), GetName() );
}

/*
================================================================================================
idDeclRenderProg::Print
================================================================================================
*/
void idDeclRenderProg::Print() const {
	idLib::Printf( "vertex program: %d bytes\n", vertexSource.Length() );
	idLib::Printf( "%s\n", vertexSource.c_str() );
	idLib::Printf( "fragment program: %d bytes\n", fragmentSource.Length() );
	idLib::Printf( "%s\n", fragmentSource.c_str() );
}
