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
#ifndef __RENDERPROGPARM_H__
#define __RENDERPROGPARM_H__

// Named render parm registry for the decl renderprog system.
// The parm types and the global index layout follow the id Tech 5
// renderparm design as reconstructed in references/wolftno (unlicensed
// decompiled MachineGames/Bethesda code); this implementation is
// written from scratch on the BFG idlib.

enum parmType_t {
	PT_VECTOR = 0,
	PT_TEXTURE_2D,
	PT_TEXTURE_3D,
	PT_TEXTURE_CUBE,
	PT_TEXTURE_SHADOW_2D,
	PT_TEXTURE_SHADOW_3D,
	PT_TEXTURE_SHADOW_CUBE,
	PT_TEXTURE_DIM_SHADOW_2D,
	PT_TEXTURE_MULTISAMPLE_2D,
	PT_IMAGE_2D_READ,
	PT_IMAGE_2D_WRITE,
	PT_SAMPLER,
	PT_PROGRAM,
	PT_STRING,
	PT_MAX
};

// Global parm index layout. The vector range starts with the engine
// renderParm_t values in enum order, followed by temporaries, then by
// parms allocated on demand. The other ranges are allocated on demand.
const int MAX_RENDER_PARMS				= 2176;
const int PARM_INDEX_INVALID			= 2175;

const int PARM_INDEX_VECTOR_BASE		= 0;
const int PARM_INDEX_VECTOR_MAX			= 1024;
const int PARM_INDEX_TEXTURE_BASE		= 1024;
const int PARM_INDEX_TEXTURE_MAX		= 1536;
const int PARM_INDEX_SAMPLER_BASE		= 1536;
const int PARM_INDEX_SAMPLER_MAX		= 1664;
const int PARM_INDEX_PROGRAM_BASE		= 1664;
const int PARM_INDEX_PROGRAM_MAX		= 2048;
const int PARM_INDEX_STRING_BASE		= 2048;
const int PARM_INDEX_STRING_MAX			= 2175;

const int NUM_RENDER_PARMS_TEMPORARY	= 32;

class idRenderProgParms {
public:
	static void	Init();
	static void	Shutdown();

	// case-insensitive lookup, PARM_INDEX_INVALID when the parm is not registered
	static int	FindParmByName( const char * name );

	// returns the existing index when the parm is already registered,
	// otherwise allocates the next free index in the parm type range
	static int	AllocateParm( const char * name, parmType_t type );

	static const char *	GetParmName( int index );
	static parmType_t	GetParmType( int index );
};

#endif /* !__RENDERPROGPARM_H__ */
