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

#ifndef __PARMBLOCK_H__
#define __PARMBLOCK_H__

#include "RenderProgParm.h"
#include "../idlib/containers/List.h"
#include "../idlib/math/Vector.h"

/*
Parm block expression system for the renderprog migration: a list of
operations that write render parms, with constants, write masks, and
inheritance through parent blocks.

Adapted from the Wolfenstein: The New Order idParmBlock reconstructed under
references/wolfnto-reversedengineered (parmblock.h/parmblock.cpp). That
material is decompiled proprietary code and is not GPL-compatible; this is a
re-implementation on this engine's idlib.

The operation types and the op record are named progOpType_t / progOp_t /
PROG_OP_* instead of the TNO names so they do not collide with the
expOpType_t / expOp_t / OP_TYPE_* of the material system in Material.h.
*/

// Operation types; the numbering matches the TNO ordering.
enum progOpType_t {
	PROG_OP_UNMASKED_MOVE_CONSTANT = 0,
	PROG_OP_UNMASKED_MOVE = 1,
	PROG_OP_MOVE_CONSTANT = 2,
	PROG_OP_MOVE = 3,
	PROG_OP_SWIZZLE = 4,
	PROG_OP_ADD = 5,
	PROG_OP_SUBTRACT = 6,
	PROG_OP_MULTIPLY = 7,
	PROG_OP_DOT3 = 8,
	PROG_OP_DOT4 = 9,
	PROG_OP_DIVIDE = 10,
	PROG_OP_MOD = 11,
	PROG_OP_GT = 12,
	PROG_OP_GE = 13,
	PROG_OP_LT = 14,
	PROG_OP_LE = 15,
	PROG_OP_EQ = 16,
	PROG_OP_NE = 17,
	PROG_OP_AND = 18,
	PROG_OP_OR = 19,
	PROG_OP_TABLE = 20
};

// Any parm index at or above this value means "no parm".
static const short INVALID_PARM_INDEX = 2175;

// Bits 0-4 hold the operation type, bits 5-8 the parm type, bits 9-12 the
// write mask.
struct progOp_t {
	unsigned short	type;
	short			parmIndexDest;
	short			parmIndexA;
	short			parmIndexB;

	const idRenderProgParm *	GetDest() const;
	const idRenderProgParm *	GetA() const;
};

class idParmBlock {
public:
	int					NumOps() const { return ops.Num(); }
	const progOp_t &	GetOp( int index ) const { return ops[index]; }

	bool				SetsRenderParm( const idRenderProgParm * parm ) const;
	void				Clear();
	void				CopyFrom( const idParmBlock & other );
	void				Append( const idParmBlock & other );
	void				AddOp( const progOp_t & operation, const parmValue_t & constant );
	void				RemoveRedundantOperations();
	void				ClearParm( const idRenderProgParm * parm );

	void				SetParm( const idRenderProgParm * parm, const parmValue_t & value );
	void				SetParm( const idRenderProgParm * parm, const idRenderProgParm * sourceParm );
	void				SetParm( const idRenderProgParm * parm, const idVec4 & value );
	void				SetParm( const idRenderProgParm * parm, float value );
	void				SetFloat( const idRenderProgParm * parm, float value );
	void				SetVec4( const idRenderProgParm * parm, const idVec4 & value );
	void				SetImage( const idRenderProgParm * parm, const idImage * image );

	float				GetFloat( const idRenderProgParm * parm, const idParmBlock * parentBlock = nullptr ) const;
	int					GetInteger( const idRenderProgParm * parm, const idParmBlock * parentBlock = nullptr ) const;
	idVec4				GetVector( const idRenderProgParm * parm, const idParmBlock * parentBlock = nullptr ) const;
	const idImage *		GetImage( const idRenderProgParm * parm, const idParmBlock * parentBlock = nullptr ) const;

	bool				IsEqualSimple( const idParmBlock & other ) const;
	bool				operator==( const idParmBlock & other ) const;
	bool				operator!=( const idParmBlock & other ) const { return !( *this == other ); }

	idList<progOp_t>	ops;
	idList<parmValue_t>	constants;

private:
	void				GetRenderParmFromOps_r( parmValue_t & parmValue, int numOps, int parmIndex, const idParmBlock * parentBlock ) const;
};

#endif
