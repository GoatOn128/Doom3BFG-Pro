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

#ifndef __PARMBLOCKPARSER_H__
#define __PARMBLOCKPARSER_H__

/*
Parm block expression parser for the renderprog system migration: parses the
parms {} block of a renderprog decl into a list of operations on a parm
block.

Adapted from the Wolfenstein: The New Order idParmBlockParser reconstructed
under references/wolfnto-reversedengineered (parmblockparser.h/
parmblockparser.cpp). That material is decompiled proprietary code and is
not GPL-compatible; this is a re-implementation on this engine's idlib.
Table lookups, sampler states, and image assignments are not supported; the
registry only carries vector parms.
*/

#include "ParmBlock.h"
#include "../idlib/Token.h"

class idParser;

class idParmBlockParser {
public:
	idParmBlockParser( idParser & source, idParmBlock & block );

private:
	bool						CleanupTemporaries();
	const idRenderProgParm *	GetExpressionTemporaryParm();
	progOp_t *					GetExpressionOp( idParser & source, const parmValue_t & constant );
	const idRenderProgParm *	GetConstantVector( idParser & source, const float * value );
	const idRenderProgParm *	EmitOp( idParser & source, const idRenderProgParm * parmA,
										const idRenderProgParm * parmB, progOpType_t opType );
	const idRenderProgParm *	ParseEmitOp( idParser & source, const idRenderProgParm * parmA,
											progOpType_t opType, int priority );
	const idRenderProgParm *	ParseIntrinsic( idParser & source, progOpType_t opType );
	const idRenderProgParm *	ParseTerm( idParser & source, idToken * fakeToken );
	const idRenderProgParm *	ParseExpressionPriority( idParser & source, int priority );

	bool						usedTempParms[32];
	idList<progOp_t> *			ops;
	idList<parmValue_t> *		constants;
	parmValue_t					defaultConstant;
};

#endif
