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

#ifndef __DECLRENDERPROGGENERATOR_H__
#define __DECLRENDERPROGGENERATOR_H__

/*
Render prog generator for the renderprog system migration: translates the
HLSL-style hlsl_vp/hlsl_fp blocks of a renderprog decl into GLSL and fills
the shader construct info.

Adapted from the Wolfenstein: The New Order idDeclRenderProgGenerator
reconstructed under references/wolfnto-reversedengineered
(declrenderproggenerator.h/declrenderproggenerator.cpp). That material is
decompiled proprietary code and is not GPL-compatible; this is a
re-implementation on this engine's idlib. The console targets, the dead code
stripper, and the X360 interpolant validation of the original are dropped;
PC GLSL is the only target.
*/

#include "DeclRenderProg.h"
#include "DeclRenderProgAttribs.h"

class idParser;

// GLSL sampler type for a texture parm in the registry.
const char *	TextureTypeForTextureParm( int parmIndex );

void			GenerateHeader( programType_t programType, const idStr * globalInclude,
								const renderProgData_t * data, shaderConstructInfo_t * shaderInfo );
void			GenerateCode( renderProgData_t * data,
								shaderConstructInfo_t * vertexInfo, shaderConstructInfo_t * fragmentInfo );
bool			ParseState( idParser * source, renderProgData_t * data );
bool			ParseFlags( idParser * source, renderProgData_t * data );
bool			ParseShader( programType_t programType, idParser * source,
								const idStr * globalInclude, renderProgData_t * data,
								shaderConstructInfo_t * shaderInfo );
idStr			ConvertCG2GLSL( const idStr & input, const char * name, bool isVertexProgram );

#endif
