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

#pragma hdrstop
#include "../idlib/precompiled.h"

#include <cstring>

#include "ParmBlockParser.h"
#include "DeclRenderProg.h"
#include "../idlib/Parser.h"

/*
Adapted from the Wolfenstein: The New Order idParmBlockParser
reconstructed under references/wolfnto-reversedengineered
(parmblockparser.cpp). That material is decompiled proprietary code and is
not GPL-compatible; this is a re-implementation on this engine's idlib.
Table lookups (parm[index]) and sampler/image assignments are not
supported: this engine's registry only carries vector and texture parms.
*/

static const int MAX_PARMBLOCK_OPS = 256;
static const int MAX_EXPRESSION_TEMPS = 32;

// returned by GetExpressionOp when the op list is full; writes to it are
// harmless because the op is never part of the block
static progOp_t s_dummyOperation = { 0, INVALID_PARM_INDEX, INVALID_PARM_INDEX, INVALID_PARM_INDEX };

static bool IsTemporaryParmIndex( int index ) {
	return index >= RENDERPROG_FIRST_TEMPORARY_PARM_INDEX && index < INVALID_PARM_INDEX;
}

static int ToLowerChar( int c ) {
	return ( c >= 'A' && c <= 'Z' ) ? c + ( 'a' - 'A' ) : c;
}

// Parse a constant vector: either a braced list of up to four floats or a
// single float broadcast to all four components. Remaining components are
// zero. A leading minus is handled per component because the lexer reads
// it as a separate token.
static void ParseVectorConstant( idParser & source, float values[4] ) {
	for ( int i = 0; i < 4; i++ ) {
		values[i] = 0.0f;
	}
	if ( source.CheckTokenString( "{" ) ) {
		int numValues = 0;
		while ( numValues < 4 ) {
			const bool negative = source.CheckTokenString( "-" );
			const float value = source.ParseFloat();
			values[numValues] = negative ? -value : value;
			numValues++;
			if ( !source.CheckTokenString( "," ) ) {
				break;
			}
		}
		source.ExpectTokenString( "}" );
	} else {
		const bool negative = source.CheckTokenString( "-" );
		const float value = source.ParseFloat();
		for ( int i = 0; i < 4; i++ ) {
			values[i] = negative ? -value : value;
		}
	}
}

/*
====================
idParmBlockParser::CleanupTemporaries

Remove redundant operations that only write temporary parms, and release
temporary parms that are no longer the destination of any operation.
====================
*/
bool idParmBlockParser::CleanupTemporaries() {
	if ( ops->Num() > MAX_PARMBLOCK_OPS ) {
		return false;
	}
	bool opIsRedundant[MAX_PARMBLOCK_OPS];
	bool anyRedundant = false;
	for ( int index = ops->Num() - 1; index >= 0; --index ) {
		bool usedByLaterOperation = false;
		bool overwrittenByLaterOperation = false;
		opIsRedundant[index] = false;
		for ( int later = index + 1; later < ops->Num(); ++later ) {
			if ( opIsRedundant[later] ) {
				continue;
			}
			if ( ( *ops )[later].parmIndexA == ( *ops )[index].parmIndexDest ||
				 ( *ops )[later].parmIndexB == ( *ops )[index].parmIndexDest ) {
				usedByLaterOperation = true;
			}
			const int currentMask = ( ( *ops )[index].type >> 9 ) & 15;
			const int laterMask = ( ( *ops )[later].type >> 9 ) & 15;
			if ( ( *ops )[later].parmIndexDest == ( *ops )[index].parmIndexDest &&
				 ( currentMask & ~laterMask & 15 ) == 0 ) {
				overwrittenByLaterOperation = true;
				break;
			}
		}
		// an operation that only writes a temporary parm nobody reads can
		// go as well; the temps are allocated below INVALID_PARM_INDEX
		if ( !usedByLaterOperation &&
			 ( overwrittenByLaterOperation || IsTemporaryParmIndex( ( *ops )[index].parmIndexDest ) ) ) {
			opIsRedundant[index] = true;
			anyRedundant = true;
		}
	}

	if ( anyRedundant ) {
		int retained = 0;
		for ( int index = 0; index < ops->Num(); ++index ) {
			if ( !opIsRedundant[index] ) {
				( *ops )[retained] = ( *ops )[index];
				( *constants )[retained] = ( *constants )[index];
				++retained;
			}
		}
		ops->SetNum( retained );
		constants->SetNum( retained );
	}

	bool releasedTemporary = false;
	for ( int tempIndex = 0; tempIndex < MAX_EXPRESSION_TEMPS; ++tempIndex ) {
		if ( !usedTempParms[tempIndex] ) {
			continue;
		}
		const short tempParmIndex = static_cast< short >( RENDERPROG_FIRST_TEMPORARY_PARM_INDEX + tempIndex );
		bool stillUsed = false;
		for ( int opIndex = 0; opIndex < ops->Num(); ++opIndex ) {
			if ( ( *ops )[opIndex].parmIndexDest == tempParmIndex ) {
				stillUsed = true;
				break;
			}
		}
		if ( !stillUsed ) {
			usedTempParms[tempIndex] = false;
			releasedTemporary = true;
		}
	}
	return releasedTemporary;
}

/*
====================
idParmBlockParser::GetExpressionTemporaryParm
====================
*/
const idRenderProgParm * idParmBlockParser::GetExpressionTemporaryParm() {
	int index = 0;
	while ( index < MAX_EXPRESSION_TEMPS && usedTempParms[index] ) {
		index++;
	}
	if ( index >= MAX_EXPRESSION_TEMPS ) {
		if ( CleanupTemporaries() ) {
			index = 0;
			while ( index < MAX_EXPRESSION_TEMPS && usedTempParms[index] ) {
				index++;
			}
		}
		if ( index >= MAX_EXPRESSION_TEMPS ) {
			idLib::FatalError( "idParmBlockParser::GetExpressionTemporaryParm: MAX_EXPRESSION_TEMPS" );
		}
	}
	usedTempParms[index] = true;
	const idRenderProgParm * parm = renderProgParms.GetRenderParmForIndex( RENDERPROG_FIRST_TEMPORARY_PARM_INDEX + index );
	if ( parm == nullptr ) {
		idLib::FatalError( "idParmBlockParser::GetExpressionTemporaryParm: temporary parm %i missing from the registry", index + 1 );
	}
	return parm;
}

/*
====================
idParmBlockParser::GetExpressionOp
====================
*/
progOp_t * idParmBlockParser::GetExpressionOp( idParser & source, const parmValue_t & constant ) {
	if ( ops->Num() >= MAX_PARMBLOCK_OPS ) {
		source.Warning( "GetExpressionOp: hit MAX_PARMBLOCK_OPS" );
		return &s_dummyOperation;
	}
	progOp_t operation;
	memset( &operation, 0, sizeof( operation ) );
	operation.parmIndexDest = INVALID_PARM_INDEX;
	operation.parmIndexA = INVALID_PARM_INDEX;
	operation.parmIndexB = INVALID_PARM_INDEX;
	ops->Append( operation );
	constants->Append( constant );
	return &( *ops )[ops->Num() - 1];
}

/*
====================
idParmBlockParser::GetConstantVector
====================
*/
const idRenderProgParm * idParmBlockParser::GetConstantVector( idParser & source, const float * value ) {
	parmValue_t constant;
	memcpy( constant.value, value, sizeof( constant.value ) );
	const idRenderProgParm * temporary = GetExpressionTemporaryParm();
	progOp_t * operation = GetExpressionOp( source, constant );
	operation->type = static_cast< unsigned short >( ( operation->type & 0xFFE0u ) | ( 15 << 9 ) );
	operation->parmIndexDest = static_cast< short >( temporary->GetIndex() );
	operation->type = static_cast< unsigned short >( ( temporary->GetType() << 5 ) | ( operation->type & 0xFE1Fu ) );
	return temporary;
}

/*
====================
idParmBlockParser::EmitOp
====================
*/
const idRenderProgParm * idParmBlockParser::EmitOp( idParser & source, const idRenderProgParm * parmA,
												   const idRenderProgParm * parmB, progOpType_t opType ) {
	const idRenderProgParm * temporary = GetExpressionTemporaryParm();
	progOp_t * operation = GetExpressionOp( source, defaultConstant );
	operation->type = static_cast< unsigned short >( ( operation->type & 0xFFE0u ) | opType | ( 15 << 9 ) );
	operation->parmIndexDest = static_cast< short >( temporary->GetIndex() );
	operation->parmIndexA = static_cast< short >( parmA->GetIndex() );
	operation->parmIndexB = static_cast< short >( parmB->GetIndex() );
	operation->type = static_cast< unsigned short >( ( temporary->GetType() << 5 ) | ( operation->type & 0xFE1Fu ) );
	return temporary;
}

/*
====================
idParmBlockParser::ParseEmitOp
====================
*/
const idRenderProgParm * idParmBlockParser::ParseEmitOp( idParser & source, const idRenderProgParm * parmA,
														 progOpType_t opType, int priority ) {
	const idRenderProgParm * parmB = ParseExpressionPriority( source, priority );
	return EmitOp( source, parmA, parmB, opType );
}

/*
====================
idParmBlockParser::ParseIntrinsic
====================
*/
const idRenderProgParm * idParmBlockParser::ParseIntrinsic( idParser & source, progOpType_t opType ) {
	source.ExpectTokenString( "(" );
	const idRenderProgParm * parmA = ParseExpressionPriority( source, 4 );
	source.ExpectTokenString( "," );
	const idRenderProgParm * parmB = ParseExpressionPriority( source, 4 );
	source.ExpectTokenString( ")" );
	return EmitOp( source, parmA, parmB, opType );
}

/*
====================
idParmBlockParser::ParseTerm
====================
*/
const idRenderProgParm * idParmBlockParser::ParseTerm( idParser & source, idToken * fakeToken ) {
	idToken token;
	if ( fakeToken != nullptr ) {
		// the implicit copy assignment copies the whole token, including the
		// members that are not assignable through the public operators
		token = *fakeToken;
	} else {
		source.ReadToken( &token );
	}

	if ( token == "dot3" ) {
		return ParseIntrinsic( source, PROG_OP_DOT3 );
	}
	if ( token == "dot4" ) {
		return ParseIntrinsic( source, PROG_OP_DOT4 );
	}
	if ( token == "(" ) {
		const idRenderProgParm * result = ParseExpressionPriority( source, 4 );
		source.ExpectTokenString( ")" );
		return result;
	}
	if ( token == "{" || token == "-" || token.type == TT_NUMBER ) {
		source.UnreadToken( &token );
		parmValue_t constantValue;
		memset( &constantValue, 0, sizeof( constantValue ) );
		ParseVectorConstant( source, constantValue.value );
		return GetConstantVector( source, constantValue.value );
	}

	const char * dot = strstr( token.c_str(), "." );
	if ( dot != nullptr ) {
		const int baseLength = static_cast< int >( dot - token.c_str() );
		idStr base = token.Left( baseLength );
		idToken baseToken;
		baseToken = base.c_str();
		baseToken.type = TT_NAME;
		const idRenderProgParm * parmA = ParseTerm( source, &baseToken );
		const idRenderProgParm * temporary = GetExpressionTemporaryParm();
		parmValue_t swizzle;
		memset( &swizzle, 0, sizeof( swizzle ) );
		swizzle.swizzle[0] = 0;
		swizzle.swizzle[1] = 1;
		swizzle.swizzle[2] = 2;
		swizzle.swizzle[3] = 3;
		int count = 0;
		bool badSwizzle = false;
		while ( dot[count + 1] != '\0' && count < 4 ) {
			switch ( ToLowerChar( static_cast< unsigned char >( dot[count + 1] ) ) ) {
				case 'x': case 'r': swizzle.swizzle[count] = 0; break;
				case 'y': case 'g': swizzle.swizzle[count] = 1; break;
				case 'z': case 'b': swizzle.swizzle[count] = 2; break;
				case 'w': case 'a': swizzle.swizzle[count] = 3; break;
				default:
					source.Warning( "bad swizzle '%s'", dot );
					badSwizzle = true;
					break;
			}
			if ( badSwizzle ) {
				break;
			}
			++count;
		}
		for ( int component = count; component < 4; ++component ) {
			swizzle.swizzle[component] = swizzle.swizzle[0];
		}
		progOp_t * operation = GetExpressionOp( source, swizzle );
		operation->type = static_cast< unsigned short >( ( operation->type & 0xFFE4u ) | PROG_OP_SWIZZLE | ( 15 << 9 ) );
		operation->parmIndexDest = static_cast< short >( temporary->GetIndex() );
		operation->parmIndexA = static_cast< short >( parmA->GetIndex() );
		operation->parmIndexB = INVALID_PARM_INDEX;
		operation->type = static_cast< unsigned short >( ( temporary->GetType() << 5 ) | ( operation->type & 0xFE1Fu ) );
		return temporary;
	}

	if ( source.PeekTokenString( "[" ) ) {
		source.Warning( "table lookups (parm[index]) are not supported in parms blocks" );
		return GetExpressionTemporaryParm();
	}

	const idRenderProgParm * parm = renderProgParms.FindParmByName( token.c_str() );
	if ( parm != nullptr ) {
		return parm;
	}

	source.Warning( "Bad term '%s'", token.c_str() );
	return GetExpressionTemporaryParm();
}

/*
====================
idParmBlockParser::ParseExpressionPriority
====================
*/
const idRenderProgParm * idParmBlockParser::ParseExpressionPriority( idParser & source, int priority ) {
	if ( priority == 0 ) {
		return ParseTerm( source, nullptr );
	}
	const idRenderProgParm * parmA = ParseExpressionPriority( source, priority - 1 );

	idToken token;
	if ( !source.ReadToken( &token ) ) {
		return parmA;
	}
	switch ( priority ) {
		case 1:
			if ( token == "*" ) { return ParseEmitOp( source, parmA, PROG_OP_MULTIPLY, 1 ); }
			if ( token == "/" ) { return ParseEmitOp( source, parmA, PROG_OP_DIVIDE, 1 ); }
			if ( token == "%" ) { return ParseEmitOp( source, parmA, PROG_OP_MOD, 1 ); }
			break;
		case 2:
			if ( token == "+" ) { return ParseEmitOp( source, parmA, PROG_OP_ADD, 2 ); }
			if ( token == "-" ) { return ParseEmitOp( source, parmA, PROG_OP_SUBTRACT, 2 ); }
			break;
		case 3:
			if ( token == ">" ) { return ParseEmitOp( source, parmA, PROG_OP_GT, 3 ); }
			if ( token == ">=" ) { return ParseEmitOp( source, parmA, PROG_OP_GE, 3 ); }
			if ( token == "<" ) { return ParseEmitOp( source, parmA, PROG_OP_LT, 3 ); }
			if ( token == "<=" ) { return ParseEmitOp( source, parmA, PROG_OP_LE, 3 ); }
			if ( token == "==" ) { return ParseEmitOp( source, parmA, PROG_OP_EQ, 3 ); }
			if ( token == "!=" ) { return ParseEmitOp( source, parmA, PROG_OP_NE, 3 ); }
			break;
		case 4:
			if ( token == "&&" ) { return ParseEmitOp( source, parmA, PROG_OP_AND, 4 ); }
			if ( token == "||" ) { return ParseEmitOp( source, parmA, PROG_OP_OR, 4 ); }
			break;
		default:
			break;
	}
	if ( token != ";" ) {
		source.UnreadToken( &token );
	}
	return parmA;
}

/*
====================
idParmBlockParser::idParmBlockParser

Parses "parm = expression;" lines until the closing brace. Write masks
("parm.rgb = ...") restrict which components an assignment changes. When
the expression result is an unused temporary, its producing operation is
retargeted instead of adding a move.
====================
*/
idParmBlockParser::idParmBlockParser( idParser & source, idParmBlock & block )
	: ops( &block.ops ), constants( &block.constants ) {
	memset( usedTempParms, 0, sizeof( usedTempParms ) );
	memset( &defaultConstant, 0, sizeof( defaultConstant ) );

	idToken token;
	if ( !source.ExpectAnyToken( &token ) ) {
		return;
	}
	while ( true ) {
		if ( token == "}" ) {
			return;
		}
		if ( token == "{" ) {
			source.Warning( "Can't have an open brace inside a parmBlock." );
			return;
		}

		int writeMask = 15;
		const char * dot = strstr( token.c_str(), "." );
		if ( dot != nullptr ) {
			idStr base = token.Left( static_cast< int >( dot - token.c_str() ) );
			int nextComponent = 0;
			writeMask = 0;
			const char * mask = dot + 1;
			if ( *mask == '\0' ) {
				source.Warning( "bad write mask" );
				return;
			}
			for ( ; *mask != '\0'; ++mask ) {
				const int character = ToLowerChar( static_cast< unsigned char >( *mask ) );
				int component = -1;
				for ( int candidate = nextComponent; candidate < 4; ++candidate ) {
					const char rgba[] = "rgba";
					const char xyzw[] = "xyzw";
					if ( character == rgba[candidate] || character == xyzw[candidate] ) {
						component = candidate;
						break;
					}
				}
				if ( component < 0 ) {
					source.Warning( "bad write mask" );
					return;
				}
				writeMask |= 1 << component;
				nextComponent = component + 1;
			}
			token = base.c_str();
		}

		const idRenderProgParm * destination = renderProgParms.FindParmByName( token.c_str() );
		if ( destination == nullptr ) {
			source.Warning( "'%s' is an unknown renderParm.", token.c_str() );
			return;
		}
		source.CheckTokenString( "=" );

		if ( destination->GetType() == PT_PROGRAM ) {
			if ( !source.ExpectAnyToken( &token ) ) {
				return;
			}
			parmValue_t constant;
			memset( &constant, 0, sizeof( constant ) );
			constant.program = static_cast< const idDeclRenderProg * >( declManager->FindType( DECL_RENDERPROG, token.c_str(), false ) );
			if ( constant.program == nullptr ) {
				source.Warning( "'%s' is an unknown renderProg.", token.c_str() );
				return;
			}
			progOp_t * operation = GetExpressionOp( source, constant );
			operation->type = static_cast< unsigned short >( ( operation->type & 0xFFE0u ) | ( 15 << 9 ) );
			operation->parmIndexDest = static_cast< short >( destination->GetIndex() );
			operation->parmIndexA = INVALID_PARM_INDEX;
			operation->parmIndexB = INVALID_PARM_INDEX;
			operation->type = static_cast< unsigned short >( ( destination->GetType() << 5 ) | ( operation->type & 0xFE1Fu ) );
			source.CheckTokenString( ";" );
		} else if ( destination->GetType() != PT_VECTOR ) {
			source.Warning( "'%s' is not a vector parm.", destination->GetName() );
			return;
		} else {
			const idRenderProgParm * result = ParseExpressionPriority( source, 4 );
			if ( IsTemporaryParmIndex( result->GetIndex() ) && ops->Num() > 0 &&
				 ( *ops )[ops->Num() - 1].parmIndexDest == static_cast< short >( result->GetIndex() ) ) {
				// final-move optimization: retarget the producing operation
				progOp_t & operation = ( *ops )[ops->Num() - 1];
				operation.type = static_cast< unsigned short >( ( operation.type & 0xE1FFu ) | ( writeMask << 9 ) );
				operation.parmIndexDest = static_cast< short >( destination->GetIndex() );
			} else {
				progOp_t * operation = GetExpressionOp( source, defaultConstant );
				operation->type = static_cast< unsigned short >( ( operation->type & ( writeMask == 15 ? 0xFFE1u : 0xFFE3u ) ) |
								 ( writeMask == 15 ? PROG_OP_UNMASKED_MOVE : PROG_OP_MOVE ) );
				operation->type = static_cast< unsigned short >( ( operation->type & 0xE1FFu ) | ( writeMask << 9 ) );
				operation->parmIndexDest = static_cast< short >( destination->GetIndex() );
				operation->parmIndexA = static_cast< short >( result->GetIndex() );
				operation->parmIndexB = INVALID_PARM_INDEX;
				operation->type = static_cast< unsigned short >( ( destination->GetType() << 5 ) | ( operation->type & 0xFE1Fu ) );
			}
		}

		if ( !source.ExpectAnyToken( &token ) ) {
			return;
		}
	}
}
