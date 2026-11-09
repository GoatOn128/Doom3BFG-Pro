#pragma hdrstop
#include "../idlib/precompiled.h"

#include <cmath>
#include <cstring>
#include "ParmBlock.h"

/*
Adapted from the Wolfenstein: The New Order idParmBlock implementation
reconstructed under references/wolfnto-reversedengineered (parmblock.cpp).
That material is decompiled proprietary code and is not GPL-compatible;
this is a re-implementation on this engine's idlib.
*/

static int c_redundantOperationsRemoved = 0;

const idRenderProgParm * progOp_t::GetDest() const {
	return parmIndexDest >= INVALID_PARM_INDEX ? nullptr : renderProgParms.GetRenderParmForIndex( parmIndexDest );
}

const idRenderProgParm * progOp_t::GetA() const {
	return parmIndexA >= INVALID_PARM_INDEX ? nullptr : renderProgParms.GetRenderParmForIndex( parmIndexA );
}

// Copy components selected by the write mask; a mask of 0 or 0x0F copies
// everything.
static void ApplyWriteMask( parmValue_t & dest, const parmValue_t & src, int writeMask ) {
	if ( writeMask == 0 || writeMask == 0x0F ) {
		for ( int i = 0; i < 4; i++ ) {
			dest.value[i] = src.value[i];
		}
		return;
	}
	if ( writeMask & 1 ) { dest.value[0] = src.value[0]; }
	if ( writeMask & 2 ) { dest.value[1] = src.value[1]; }
	if ( writeMask & 4 ) { dest.value[2] = src.value[2]; }
	if ( writeMask & 8 ) { dest.value[3] = src.value[3]; }
}

// Evaluate a single operation. parmValue is the destination; components not
// covered by a write mask keep whatever the caller left in it, matching the
// TNO behavior.
static void ApplyOp( const progOp_t & operation, const parmValue_t & constant,
					 const parmValue_t & parmA, const parmValue_t & parmB, parmValue_t & parmValue ) {
	const int type = operation.type & 0x1F;
	const int writeMask = ( operation.type >> 9 ) & 0x0F;

	switch ( type ) {
		case PROG_OP_UNMASKED_MOVE_CONSTANT:
			ApplyWriteMask( parmValue, constant, 0 );
			break;
		case PROG_OP_MOVE_CONSTANT:
			ApplyWriteMask( parmValue, constant, writeMask );
			break;
		case PROG_OP_UNMASKED_MOVE:
			ApplyWriteMask( parmValue, parmA, 0 );
			break;
		case PROG_OP_MOVE:
			ApplyWriteMask( parmValue, parmA, writeMask );
			break;
		case PROG_OP_SWIZZLE:
			for ( int i = 0; i < 4; i++ ) {
				const int swizzle = constant.swizzle[i];
				if ( swizzle < 0 || swizzle > 3 ) {
					idLib::Error( "idParmBlock: bad swizzle index %d", swizzle );
				}
				parmValue.value[i] = parmA.value[swizzle];
			}
			break;
		case PROG_OP_ADD:
		case PROG_OP_SUBTRACT:
		case PROG_OP_MULTIPLY:
		case PROG_OP_DIVIDE:
		case PROG_OP_MOD:
		case PROG_OP_GT:
		case PROG_OP_GE:
		case PROG_OP_LT:
		case PROG_OP_LE:
		case PROG_OP_EQ:
		case PROG_OP_NE:
		case PROG_OP_AND:
		case PROG_OP_OR: {
			for ( int i = 0; i < 4; i++ ) {
				const float a = parmA.value[i];
				const float b = parmB.value[i];
				float result = 0.0f;
				switch ( type ) {
					case PROG_OP_ADD:			result = a + b; break;
					case PROG_OP_SUBTRACT:		result = a - b; break;
					case PROG_OP_MULTIPLY:		result = a * b; break;
					case PROG_OP_DIVIDE:		result = b != 0.0f ? a / b : 0.0f; break;
					case PROG_OP_MOD:			result = b != 0.0f ? fmodf( a, b ) : 0.0f; break;
					case PROG_OP_GT:			result = a > b ? 1.0f : 0.0f; break;
					case PROG_OP_GE:			result = a >= b ? 1.0f : 0.0f; break;
					case PROG_OP_LT:			result = a < b ? 1.0f : 0.0f; break;
					case PROG_OP_LE:			result = a <= b ? 1.0f : 0.0f; break;
					case PROG_OP_EQ:			result = a == b ? 1.0f : 0.0f; break;
					case PROG_OP_NE:			result = a != b ? 1.0f : 0.0f; break;
					case PROG_OP_AND:			result = ( a != 0.0f && b != 0.0f ) ? 1.0f : 0.0f; break;
					case PROG_OP_OR:			result = ( a != 0.0f || b != 0.0f ) ? 1.0f : 0.0f; break;
					default: break;
				}
				parmValue.value[i] = result;
			}
			break;
		}
		case PROG_OP_DOT3:
			parmValue.value[1] = 0.0f;
			parmValue.value[2] = 0.0f;
			parmValue.value[3] = 0.0f;
			parmValue.value[0] = parmA.value[0] * parmB.value[0] +
								 parmA.value[1] * parmB.value[1] +
								 parmA.value[2] * parmB.value[2];
			break;
		case PROG_OP_DOT4:
			parmValue.value[1] = 0.0f;
			parmValue.value[2] = 0.0f;
			parmValue.value[3] = 0.0f;
			parmValue.value[0] = parmA.value[0] * parmB.value[0] +
								 parmA.value[1] * parmB.value[1] +
								 parmA.value[2] * parmB.value[2] +
								 parmA.value[3] * parmB.value[3];
			break;
		default:
			idLib::Error( "idParmBlock: unsupported operation type %d", type );
			break;
	}
}

bool idParmBlock::SetsRenderParm( const idRenderProgParm * parm ) const {
	if ( parm == nullptr ) {
		return false;
	}
	for ( int index = 0; index < ops.Num(); index++ ) {
		if ( ops[index].parmIndexDest == parm->GetIndex() ) {
			return true;
		}
	}
	return false;
}

void idParmBlock::Clear() {
	ops.Clear();
	constants.Clear();
}

void idParmBlock::CopyFrom( const idParmBlock & other ) {
	ops.SetNum( other.ops.Num() );
	constants.SetNum( other.constants.Num() );
	for ( int index = 0; index < other.ops.Num(); index++ ) {
		ops[index] = other.ops[index];
		constants[index] = other.constants[index];
	}
}

void idParmBlock::AddOp( const progOp_t & operation, const parmValue_t & constant ) {
	ops.Append( operation );
	constants.Append( constant );
}

void idParmBlock::Append( const idParmBlock & other ) {
	for ( int index = 0; index < other.ops.Num(); index++ ) {
		AddOp( other.ops[index], other.constants[index] );
	}
	RemoveRedundantOperations();
}

void idParmBlock::RemoveRedundantOperations() {
	if ( ops.Num() == 0 || ops.Num() > 256 ) {
		return;
	}
	// An operation is redundant when no later operation reads the parm it
	// writes and a later operation overwrites every component it masks.
	bool opIsRedundant[256];
	bool anyRedundant = false;
	for ( int index = ops.Num() - 1; index >= 0; index-- ) {
		bool usedByLaterOperation = false;
		bool overwrittenByLaterOperation = false;
		opIsRedundant[index] = false;
		for ( int later = index + 1; later < ops.Num(); later++ ) {
			if ( opIsRedundant[later] ) {
				continue;
			}
			if ( ops[later].parmIndexA == ops[index].parmIndexDest ||
				 ops[later].parmIndexB == ops[index].parmIndexDest ) {
				usedByLaterOperation = true;
			}
			const int currentMask = ( ops[index].type >> 9 ) & 0x0F;
			const int laterMask = ( ops[later].type >> 9 ) & 0x0F;
			if ( ops[later].parmIndexDest == ops[index].parmIndexDest &&
				 ( currentMask & ~laterMask & 0x0F ) == 0 ) {
				overwrittenByLaterOperation = true;
				break;
			}
		}
		if ( !usedByLaterOperation && overwrittenByLaterOperation ) {
			opIsRedundant[index] = true;
			anyRedundant = true;
		}
	}
	if ( !anyRedundant ) {
		return;
	}
	int retained = 0;
	for ( int index = 0; index < ops.Num(); index++ ) {
		if ( opIsRedundant[index] ) {
			c_redundantOperationsRemoved++;
		} else {
			ops[retained] = ops[index];
			constants[retained] = constants[index];
			retained++;
		}
	}
	ops.SetNum( retained );
	constants.SetNum( retained );
}

void idParmBlock::ClearParm( const idRenderProgParm * parm ) {
	if ( parm == nullptr ) {
		return;
	}
	bool removedComplexOperation = false;
	for ( int index = 0; index < ops.Num(); index++ ) {
		if ( ops[index].parmIndexDest != parm->GetIndex() ) {
			continue;
		}
		const int type = ops[index].type & 0x1F;
		removedComplexOperation |= ( type >= PROG_OP_SWIZZLE );
		ops.RemoveIndex( index );
		constants.RemoveIndex( index );
		index--;
	}
	if ( removedComplexOperation ) {
		RemoveRedundantOperations();
	}
}

void idParmBlock::SetParm( const idRenderProgParm * parm, const parmValue_t & value ) {
	if ( parm == nullptr ) {
		return;
	}
	int index = ops.Num() - 1;
	while ( index >= 0 && ops[index].parmIndexDest != parm->GetIndex() ) {
		index--;
	}
	bool replacedComplexOperation = false;
	if ( index < 0 ) {
		index = ops.Num();
		progOp_t operation;
		memset( &operation, 0, sizeof( operation ) );
		parmValue_t constant;
		memset( &constant, 0, sizeof( constant ) );
		ops.Append( operation );
		constants.Append( constant );
	} else {
		const int oldType = ops[index].type & 0x1F;
		replacedComplexOperation = ( oldType >= PROG_OP_SWIZZLE );
	}
	ops[index].type = static_cast< unsigned short >(
		( static_cast< int >( parm->GetType() ) << 5 ) | ( ops[index].type & 0xFE00u ) );
	ops[index].parmIndexDest = static_cast< short >( parm->GetIndex() );
	ops[index].parmIndexA = INVALID_PARM_INDEX;
	ops[index].parmIndexB = INVALID_PARM_INDEX;
	constants[index] = value;
	if ( replacedComplexOperation ) {
		RemoveRedundantOperations();
	}
}

void idParmBlock::SetParm( const idRenderProgParm * parm, const idRenderProgParm * sourceParm ) {
	if ( parm == nullptr || sourceParm == nullptr ) {
		return;
	}
	int index = ops.Num() - 1;
	while ( index >= 0 && ops[index].parmIndexDest != parm->GetIndex() ) {
		index--;
	}
	bool replacedComplexOperation = false;
	if ( index < 0 ) {
		index = ops.Num();
		progOp_t operation;
		memset( &operation, 0, sizeof( operation ) );
		parmValue_t constant;
		memset( &constant, 0, sizeof( constant ) );
		ops.Append( operation );
		constants.Append( constant );
	} else {
		const int oldType = ops[index].type & 0x1F;
		replacedComplexOperation = ( oldType >= PROG_OP_SWIZZLE );
	}
	ops[index].type = static_cast< unsigned short >(
		( static_cast< int >( parm->GetType() ) << 5 ) | ( ops[index].type & 0xFE00u ) | PROG_OP_UNMASKED_MOVE );
	ops[index].parmIndexDest = static_cast< short >( parm->GetIndex() );
	ops[index].parmIndexA = static_cast< short >( sourceParm->GetIndex() );
	ops[index].parmIndexB = INVALID_PARM_INDEX;
	if ( replacedComplexOperation ) {
		RemoveRedundantOperations();
	}
}

void idParmBlock::SetParm( const idRenderProgParm * parm, const idVec4 & value ) {
	SetVec4( parm, value );
}

void idParmBlock::SetParm( const idRenderProgParm * parm, float value ) {
	SetFloat( parm, value );
}

void idParmBlock::SetFloat( const idRenderProgParm * parm, float value ) {
	SetVec4( parm, idVec4( value, value, value, value ) );
}

void idParmBlock::SetVec4( const idRenderProgParm * parm, const idVec4 & value ) {
	if ( parm == nullptr ) {
		return;
	}
	parmValue_t parmValue;
	memset( &parmValue, 0, sizeof( parmValue ) );
	parmValue.value[0] = value.x;
	parmValue.value[1] = value.y;
	parmValue.value[2] = value.z;
	parmValue.value[3] = value.w;
	SetParm( parm, parmValue );
}

void idParmBlock::SetImage( const idRenderProgParm * parm, const idImage * image ) {
	if ( parm == nullptr ) {
		return;
	}
	parmValue_t parmValue;
	memset( &parmValue, 0, sizeof( parmValue ) );
	parmValue.image = image;
	SetParm( parm, parmValue );
}

void idParmBlock::GetRenderParmFromOps_r( parmValue_t & parmValue, int numOps,
										  int parmIndex, const idParmBlock * parentBlock ) const {
	const idParmBlock * block = this;
	const idParmBlock * parent = parentBlock;
	int operationIndex = numOps - 1;
	for ( ;; ) {
		while ( operationIndex >= 0 && block->ops[operationIndex].parmIndexDest != parmIndex ) {
			operationIndex--;
		}
		if ( operationIndex >= 0 ) {
			break;
		}
		if ( parent == nullptr ) {
			const idRenderProgParm * parm = renderProgParms.GetRenderParmForIndex( parmIndex );
			if ( parm == nullptr ) {
				idLib::Error( "idParmBlock: no parm with index %d", parmIndex );
			}
			parmValue = parm->GetDeclaredValue();
			return;
		}
		block = parent;
		operationIndex = block->ops.Num() - 1;
		parent = nullptr;
	}

	parmValue_t parmA;
	parmValue_t parmB;
	memset( &parmA, 0, sizeof( parmA ) );
	memset( &parmB, 0, sizeof( parmB ) );
	const progOp_t & operation = block->ops[operationIndex];
	if ( operation.parmIndexA != INVALID_PARM_INDEX ) {
		block->GetRenderParmFromOps_r( parmA, operationIndex, operation.parmIndexA, parent );
	}
	if ( operation.parmIndexB != INVALID_PARM_INDEX ) {
		block->GetRenderParmFromOps_r( parmB, operationIndex, operation.parmIndexB, parent );
	}
	ApplyOp( operation, block->constants[operationIndex], parmA, parmB, parmValue );
}

float idParmBlock::GetFloat( const idRenderProgParm * parm, const idParmBlock * parentBlock ) const {
	if ( parm == nullptr || parm->GetType() != PT_VECTOR ) {
		idLib::Error( "idParmBlock::GetFloat: %s is not a vector parm", parm != nullptr ? parm->GetName() : "<null>" );
	}
	parmValue_t value;
	memset( &value, 0, sizeof( value ) );
	GetRenderParmFromOps_r( value, ops.Num(), parm->GetIndex(), parentBlock );
	return value.value[0];
}

int idParmBlock::GetInteger( const idRenderProgParm * parm, const idParmBlock * parentBlock ) const {
	return static_cast< int >( GetFloat( parm, parentBlock ) );
}

idVec4 idParmBlock::GetVector( const idRenderProgParm * parm, const idParmBlock * parentBlock ) const {
	if ( parm == nullptr || parm->GetType() != PT_VECTOR ) {
		idLib::Error( "idParmBlock::GetVector: %s is not a vector parm", parm != nullptr ? parm->GetName() : "<null>" );
	}
	parmValue_t value;
	memset( &value, 0, sizeof( value ) );
	GetRenderParmFromOps_r( value, ops.Num(), parm->GetIndex(), parentBlock );
	return idVec4( value.value[0], value.value[1], value.value[2], value.value[3] );
}

const idImage * idParmBlock::GetImage( const idRenderProgParm * parm, const idParmBlock * parentBlock ) const {
	if ( parm == nullptr || ( parm->GetType() != PT_TEXTURE_2D && parm->GetType() != PT_TEXTURE_3D && parm->GetType() != PT_TEXTURE_CUBE ) ) {
		idLib::Error( "idParmBlock::GetImage: %s is not a texture parm", parm != nullptr ? parm->GetName() : "<null>" );
	}
	parmValue_t value;
	memset( &value, 0, sizeof( value ) );
	GetRenderParmFromOps_r( value, ops.Num(), parm->GetIndex(), parentBlock );
	return value.image;
}

bool idParmBlock::IsEqualSimple( const idParmBlock & other ) const {
	if ( ops.Num() != other.ops.Num() ) {
		return false;
	}
	for ( int index = 0; index < other.ops.Num(); index++ ) {
		const int type = other.ops[index].type & 0x1F;
		if ( type != PROG_OP_UNMASKED_MOVE_CONSTANT && type != PROG_OP_MOVE_CONSTANT ) {
			return false;
		}
	}
	for ( int leftIndex = 0; leftIndex < ops.Num(); leftIndex++ ) {
		const int type = ops[leftIndex].type & 0x1F;
		if ( type != PROG_OP_UNMASKED_MOVE_CONSTANT && type != PROG_OP_MOVE_CONSTANT ) {
			return false;
		}
		int rightIndex = 0;
		while ( rightIndex < other.ops.Num() && other.ops[rightIndex].parmIndexDest != ops[leftIndex].parmIndexDest ) {
			rightIndex++;
		}
		if ( rightIndex == other.ops.Num() ||
			 ( ( other.ops[rightIndex].type ^ ops[leftIndex].type ) & 0x1E00 ) != 0 ||
			 memcmp( &constants[leftIndex], &other.constants[rightIndex], sizeof( parmValue_t ) ) != 0 ) {
			return false;
		}
	}
	return true;
}

bool idParmBlock::operator==( const idParmBlock & other ) const {
	if ( ops.Num() != other.ops.Num() || constants.Num() != other.constants.Num() ) {
		return false;
	}
	for ( int index = 0; index < ops.Num(); index++ ) {
		const progOp_t & left = ops[index];
		const progOp_t & right = other.ops[index];
		if ( left.type != right.type || left.parmIndexDest != right.parmIndexDest ||
			 left.parmIndexA != right.parmIndexA || left.parmIndexB != right.parmIndexB ) {
			return false;
		}
	}
	return constants.Num() == 0 ||
		   memcmp( constants.Ptr(), other.constants.Ptr(),
				   static_cast< size_t >( constants.Num() ) * sizeof( parmValue_t ) ) == 0;
}
