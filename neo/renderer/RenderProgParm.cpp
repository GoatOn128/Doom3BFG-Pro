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
along with Doom 3 BFG Edition Source Code.  If not, please request a copy in writing from id Software at the address below.

If you have questions concerning this license or the additional terms, you should contact id Software LLC at the address below.

id Software LLC, c/o ZeniMax Media Inc., Suite 120, Rockville, Maryland 20850 USA

===========================================================================
*/
#pragma hdrstop
#include "../idlib/precompiled.h"

#include "tr_local.h"

// Engine parm table. One entry per renderParm_t value, in exact enum
// order, so a registry index and a renderParm_t value are the same
// integer. Names are lowercase and follow the id Tech 5 style used by
// the decl renderprog system.
struct parmDescriptor_t {
	const char *	name;
	parmType_t		type;
};

static const parmDescriptor_t engineParms[] = {
	{ "screencorrectionfactor",	PT_VECTOR },
	{ "windowcoord",			PT_VECTOR },
	{ "diffusemodifier",		PT_VECTOR },
	{ "specularmodifier",		PT_VECTOR },
	{ "locallightorigin",		PT_VECTOR },
	{ "localvieworigin",		PT_VECTOR },
	{ "lightprojections",		PT_VECTOR },
	{ "lightprojectiont",		PT_VECTOR },
	{ "lightprojectionq",		PT_VECTOR },
	{ "lightfalloffs",			PT_VECTOR },
	{ "bumpmatrixs",			PT_VECTOR },
	{ "bumpmatrixt",			PT_VECTOR },
	{ "diffusematrixs",			PT_VECTOR },
	{ "diffusematrixt",		PT_VECTOR },
	{ "specularmatrixs",		PT_VECTOR },
	{ "specularmatrixt",		PT_VECTOR },
	{ "vertexcolormodulate",	PT_VECTOR },
	{ "vertexcoloradd",		PT_VECTOR },
	{ "color",					PT_VECTOR },
	{ "vieworigin",				PT_VECTOR },
	{ "globaleyepos",			PT_VECTOR },
	{ "mvpmatrixx",				PT_VECTOR },
	{ "mvpmatrixy",				PT_VECTOR },
	{ "mvpmatrixz",				PT_VECTOR },
	{ "mvpmatrixw",				PT_VECTOR },
	{ "modelmatrixx",			PT_VECTOR },
	{ "modelmatrixy",			PT_VECTOR },
	{ "modelmatrixz",			PT_VECTOR },
	{ "modelmatrixw",			PT_VECTOR },
	{ "projmatrixx",			PT_VECTOR },
	{ "projmatrixy",			PT_VECTOR },
	{ "projmatrixz",			PT_VECTOR },
	{ "projmatrixw",			PT_VECTOR },
	{ "modelviewmatrixx",		PT_VECTOR },
	{ "modelviewmatrixy",		PT_VECTOR },
	{ "modelviewmatrixz",		PT_VECTOR },
	{ "modelviewmatrixw",		PT_VECTOR },
	{ "texturematrixs",		PT_VECTOR },
	{ "texturematrixt",		PT_VECTOR },
	{ "texgen0s",				PT_VECTOR },
	{ "texgen0t",				PT_VECTOR },
	{ "texgen0q",				PT_VECTOR },
	{ "texgen0enabled",		PT_VECTOR },
	{ "texgen1s",				PT_VECTOR },
	{ "texgen1t",				PT_VECTOR },
	{ "texgen1q",				PT_VECTOR },
	{ "texgen1enabled",		PT_VECTOR },
	{ "wobbleskyx",				PT_VECTOR },
	{ "wobbleskyy",				PT_VECTOR },
	{ "wobbleskyz",				PT_VECTOR },
	{ "overbright",				PT_VECTOR },
	{ "enableskinning",		PT_VECTOR },
	{ "alphatest",				PT_VECTOR },
	{ "shadowmatrixx",			PT_VECTOR },
	{ "shadowmatrixy",			PT_VECTOR },
	{ "shadowmatrixz",			PT_VECTOR },
	{ "shadowmatrixw",			PT_VECTOR },
	{ "shadowprojectionx",		PT_VECTOR },
	{ "shadowprojectiony",		PT_VECTOR },
	{ "shadowprojectionz",		PT_VECTOR },
	{ "shadowprojectionw",		PT_VECTOR },
	{ "shadowparams",			PT_VECTOR }
};

static_assert( sizeof( engineParms ) / sizeof( engineParms[0] ) == RENDERPARM_TOTAL,
	"engineParms must have one entry per renderParm_t value in enum order" );

static const char * tempParmNames[NUM_RENDER_PARMS_TEMPORARY] = {
	"temp1", "temp2", "temp3", "temp4", "temp5", "temp6", "temp7", "temp8",
	"temp9", "temp10", "temp11", "temp12", "temp13", "temp14", "temp15", "temp16",
	"temp17", "temp18", "temp19", "temp20", "temp21", "temp22", "temp23", "temp24",
	"temp25", "temp26", "temp27", "temp28", "temp29", "temp30", "temp31", "temp32"
};

// parms allocated at runtime, outside the engine table and temporaries
struct dynamicParm_t {
	idStr			name;
	int				index;
	parmType_t		type;
};
static idList<dynamicParm_t, TAG_RENDER> dynamicParms;

// one free index counter per parm range
static int nextVectorIndex;
static int nextTextureIndex;
static int nextSamplerIndex;
static int nextProgramIndex;
static int nextStringIndex;

/*
================================================================================================
idRenderProgParms::Init
================================================================================================
*/
void idRenderProgParms::Init() {
	dynamicParms.Clear();
	nextVectorIndex = RENDERPARM_TOTAL + NUM_RENDER_PARMS_TEMPORARY;
	nextTextureIndex = PARM_INDEX_TEXTURE_BASE;
	nextSamplerIndex = PARM_INDEX_SAMPLER_BASE;
	nextProgramIndex = PARM_INDEX_PROGRAM_BASE;
	nextStringIndex = PARM_INDEX_STRING_BASE;
}

/*
================================================================================================
idRenderProgParms::Shutdown
================================================================================================
*/
void idRenderProgParms::Shutdown() {
	dynamicParms.Clear();
}

/*
================================================================================================
idRenderProgParms::FindParmByName
================================================================================================
*/
int idRenderProgParms::FindParmByName( const char * name ) {
	for ( int i = 0; i < RENDERPARM_TOTAL; i++ ) {
		if ( idStr::Icmp( name, engineParms[i].name ) == 0 ) {
			return i;
		}
	}
	for ( int i = 0; i < NUM_RENDER_PARMS_TEMPORARY; i++ ) {
		if ( idStr::Icmp( name, tempParmNames[i] ) == 0 ) {
			return RENDERPARM_TOTAL + i;
		}
	}
	for ( int i = 0; i < dynamicParms.Num(); i++ ) {
		if ( dynamicParms[i].name.Icmp( name ) == 0 ) {
			return dynamicParms[i].index;
		}
	}
	return PARM_INDEX_INVALID;
}

/*
================================================================================================
idRenderProgParms::AllocateParm
================================================================================================
*/
int idRenderProgParms::AllocateParm( const char * name, parmType_t type ) {
	int index = FindParmByName( name );
	if ( index != PARM_INDEX_INVALID ) {
		if ( GetParmType( index ) != type ) {
			idLib::Warning( "Render parm '%s' already registered with a different type", name );
		}
		return index;
	}

	if ( type < PT_VECTOR || type >= PT_MAX ) {
		idLib::Warning( "Invalid render parm type %d for '%s'", ( int ) type, name );
		return PARM_INDEX_INVALID;
	}

	// every type in a range shares the range counter
	if ( type == PT_VECTOR ) {
		index = nextVectorIndex++;
		if ( index >= PARM_INDEX_VECTOR_MAX ) {
			idLib::Warning( "Vector render parm range full, cannot register '%s'", name );
			return PARM_INDEX_INVALID;
		}
	} else if ( type >= PT_TEXTURE_2D && type <= PT_IMAGE_2D_WRITE ) {
		index = nextTextureIndex++;
		if ( index >= PARM_INDEX_TEXTURE_MAX ) {
			idLib::Warning( "Texture render parm range full, cannot register '%s'", name );
			return PARM_INDEX_INVALID;
		}
	} else if ( type == PT_SAMPLER ) {
		index = nextSamplerIndex++;
		if ( index >= PARM_INDEX_SAMPLER_MAX ) {
			idLib::Warning( "Sampler render parm range full, cannot register '%s'", name );
			return PARM_INDEX_INVALID;
		}
	} else if ( type == PT_PROGRAM ) {
		index = nextProgramIndex++;
		if ( index >= PARM_INDEX_PROGRAM_MAX ) {
			idLib::Warning( "Program render parm range full, cannot register '%s'", name );
			return PARM_INDEX_INVALID;
		}
	} else {
		index = nextStringIndex++;
		if ( index >= PARM_INDEX_STRING_MAX ) {
			idLib::Warning( "String render parm range full, cannot register '%s'", name );
			return PARM_INDEX_INVALID;
		}
	}

	dynamicParm_t & parm = dynamicParms.Alloc();
	parm.name = name;
	parm.index = index;
	parm.type = type;
	return index;
}

/*
================================================================================================
idRenderProgParms::GetParmName
================================================================================================
*/
const char * idRenderProgParms::GetParmName( int index ) {
	if ( index < 0 || index >= MAX_RENDER_PARMS ) {
		return "";
	}
	if ( index < RENDERPARM_TOTAL ) {
		return engineParms[index].name;
	}
	if ( index < RENDERPARM_TOTAL + NUM_RENDER_PARMS_TEMPORARY ) {
		return tempParmNames[index - RENDERPARM_TOTAL];
	}
	for ( int i = 0; i < dynamicParms.Num(); i++ ) {
		if ( dynamicParms[i].index == index ) {
			return dynamicParms[i].name.c_str();
		}
	}
	return "";
}

/*
================================================================================================
idRenderProgParms::GetParmType
================================================================================================
*/
parmType_t idRenderProgParms::GetParmType( int index ) {
	if ( index < 0 || index >= MAX_RENDER_PARMS ) {
		return PT_MAX;
	}
	if ( index < RENDERPARM_TOTAL + NUM_RENDER_PARMS_TEMPORARY ) {
		return PT_VECTOR;
	}
	for ( int i = 0; i < dynamicParms.Num(); i++ ) {
		if ( dynamicParms[i].index == index ) {
			return dynamicParms[i].type;
		}
	}
	return PT_MAX;
}
