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

#ifndef __DECLRENDERPROGATTRIBS_H__
#define __DECLRENDERPROGATTRIBS_H__

/*
Render program attribute tables and shader construction info for the
renderprog system migration.

Adapted from the Wolfenstein: The New Order idDeclRenderProgAttribs
reconstructed under references/wolfnto-reversedengineered
(declrenderprogattribs.h). That material is decompiled proprietary code and
is not GPL-compatible; this is a re-implementation on this engine's idlib.
The console attribute tables (PC_D3D, 360, PS3, Orbis) of the original are
dropped; PC GLSL is the only target.
*/

#include "../idlib/Str.h"
#include "../idlib/containers/StaticList.h"
#include "../idlib/containers/List.h"

enum renderProgAttribType_t {
	AT_VS_IN = 1 << 1,
	AT_VS_OUT = 1 << 2,
	AT_PS_IN = 1 << 3,
	AT_PS_OUT = 1 << 4,
	AT_PS_INOUT = 1 << 5,
};

struct attribInfo_t {
	const char *	type;
	const char *	name;
	const char *	semantic;
	const char *	glsl;
	int				bind;
	int				flags;
	int				vertexMask;
};

extern attribInfo_t attribsPC[];

enum programType_t {
	PROGRAM_TYPE_VERTEX = 0,
	PROGRAM_TYPE_FRAGMENT = 1,
};

// An in/out member type declared by an old style body's own struct; it
// overrides the wider table default when the generated GLSL is written.
struct declaredMember_t {
	idStr	name;
	idStr	type;
};

struct shaderConstructInfo_t {
	shaderConstructInfo_t() { Clear(); }

	void			Clear();

	bool			parsed;
	idStr			headerString;
	idStr			sourceString;
	idStr			outputAttribInitString;
	idStaticList<attribInfo_t, 16>	inAttribs;
	idStaticList<attribInfo_t, 16>	outAttribs;
	idList<declaredMember_t>	declaredInMembers;
	idList<declaredMember_t>	declaredOutMembers;
};

#endif
