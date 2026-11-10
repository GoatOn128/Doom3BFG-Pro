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

#ifndef __RENDERPROGPARM_H__
#define __RENDERPROGPARM_H__

/*
Render parm registry for the renderprog system migration.

Adapted from the Wolfenstein: The New Order renderparm interfaces
reconstructed under references/wolfnto-reversedengineered (declrenderparm.h,
parmblock.h). That material is decompiled proprietary code and is not
GPL-compatible; this is a re-implementation on this engine's idlib.

The parm indices match the renderParm_t enum in RenderProgs.h and the
uniform register list in global.inc, so the existing shader base keeps
working while the renderprog system is migrated.
*/

#include "../idlib/containers/List.h"

class idImage;
class idDeclRenderProg;

// Parm types. The numeric values follow the TNO ordering; the shadow,
// multisample, sampler, and image-load/store variants in that ordering have
// no GL equivalent in this engine and only the numbering is preserved.
enum parmType_t {
	PT_VECTOR = 0,
	PT_TEXTURE_2D = 1,
	PT_TEXTURE_3D = 2,
	PT_TEXTURE_CUBE = 3,
	PT_PROGRAM = 12,
	PT_STRING = 13,
	PT_MAX = 14
};

union parmValue_t {
	float					value[4];
	const idImage *			image;
	const idDeclRenderProg *program;
	const char *			string;
	int						swizzle[4];
};

class idRenderProgParm {
public:
	idRenderProgParm() = default;
	idRenderProgParm( const char * name, parmType_t type, int index );

	const char *			GetName() const { return name; }
	parmType_t				GetType() const { return parmType; }
	int						GetIndex() const { return parmIndex; }
	const parmValue_t &		GetDeclaredValue() const { return declaredValue; }

private:
	const char *			name = nullptr;
	parmType_t				parmType = PT_VECTOR;
	int						parmIndex = -1;
	parmValue_t				declaredValue;
};

// Must match RENDERPARM_TOTAL in RenderProgs.h and the uniform register
// list in global.inc.
static const int RENDERPROG_NUM_BUILTIN_PARMS = 62;

// User parms set through SetRenderParm with a RENDERPARM_USER offset. Must
// match RENDERPARM_USER and MAX_GLSL_USER_PARMS in RenderProgs.h. Some of
// the legacy shaders in bin/base/renderprogs reference these directly by
// name (rpUser0..rpUser7), so they need to be part of the registry even
// though the decl files never list them as uniform parms.
static const int RENDERPROG_USER_PARM_BASE = 128;
static const int RENDERPROG_NUM_USER_PARMS = 8;

// Temporary expression parms used by the parm block parser. The indices
// continue the TNO numbering just below INVALID_PARM_INDEX (2175), which is
// defined in ParmBlock.h as "no parm".
static const int RENDERPROG_NUM_TEMPORARY_PARMS = 32;
static const int RENDERPROG_FIRST_TEMPORARY_PARM_INDEX = 2175 - RENDERPROG_NUM_TEMPORARY_PARMS;

class idRenderProgParmState {
public:
	// The built-in table is constructed on first use.
	const idRenderProgParm *	FindParmByName( const char * name );
	const idRenderProgParm *	GetRenderParmForIndex( int index );
	const char *			GetParmNameForIndex( int index );
	int						NumParms() { EnsureBuilt(); return parms.Num(); }

private:
	void					EnsureBuilt();

	idList<idRenderProgParm>	parms;
};

extern idRenderProgParmState renderProgParms;

#endif
