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
