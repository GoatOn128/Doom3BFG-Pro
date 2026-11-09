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
