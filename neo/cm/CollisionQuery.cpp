/*
===========================================================================

	Collision query execution boundary.

===========================================================================
*/

#pragma hdrstop
#include "../idlib/precompiled.h"

#include "CollisionModel_local.h"
#include "CollisionQuery.h"

namespace {

static const int CM_QUERY_SCRATCH_SLOTS = 2;

struct cm_queryScratchSlot_t {
	ALIGN16( cm_traceWork_t traceWork );
	cm_queryCache_t queryCache;
	bool inUse;

	cm_queryScratchSlot_t() : inUse( false ) {
	}
};

static thread_local cm_queryScratchSlot_t cm_queryScratchSlots[CM_QUERY_SCRATCH_SLOTS];

}

cm_queryScratchScope_t::cm_queryScratchScope_t() :
	traceWork( NULL ),
	queryCache( NULL ),
	slotIndex( -1 ) {
	for ( int i = 0; i < CM_QUERY_SCRATCH_SLOTS; ++i ) {
		if ( !cm_queryScratchSlots[i].inUse ) {
			cm_queryScratchSlots[i].inUse = true;
			traceWork = &cm_queryScratchSlots[i].traceWork;
			queryCache = &cm_queryScratchSlots[i].queryCache;
			slotIndex = i;
			break;
		}
	}

	// Two slots cover the normal query plus the optional debug start-solid
	// query.  Retain a safe path for deeper re-entry instead of sharing state.
	if ( traceWork == NULL ) {
		traceWork = static_cast<cm_traceWork_t *>( Mem_Alloc16( sizeof( *traceWork ), TAG_COLLISION ) );
		queryCache = new ( TAG_COLLISION ) cm_queryCache_t;
	}

	memset( traceWork, 0, sizeof( *traceWork ) );
	queryCache->Reset();
	traceWork->queryCache = queryCache;
}

cm_queryScratchScope_t::~cm_queryScratchScope_t() {
	if ( slotIndex >= 0 ) {
		cm_queryScratchSlots[slotIndex].inUse = false;
		return;
	}

	delete queryCache;
	Mem_Free16( traceWork );
}

cmQueryParms_t::cmQueryParms_t() :
	type( CM_QUERY_TRANSLATION ),
	rotation( NULL ),
	depth( 0.0f ),
	traceModel( NULL ),
	traceModelAxis( mat3_identity ),
	contentsMask( -1 ),
	model( 0 ),
	modelAxis( mat3_identity ),
	contacts( NULL ),
	maxContacts( 0 ) {
}

idCollisionQuery::idCollisionQuery() :
	contents( 0 ),
	numContacts( 0 ),
	completed( false ) {
	memset( &trace, 0, sizeof( trace ) );
	trace.fraction = 1.0f;
	trace.endAxis = mat3_identity;
}

idCollisionQuery idCollisionQueryExecute::Translation( idCollisionModelManagerLocal &manager, const cmQueryParms_t &parms ) {
	idCollisionQuery query;
	manager.TranslationInternal( &query.trace, parms.start, parms.end, parms.traceModel,
		parms.traceModelAxis, parms.contentsMask, parms.model, parms.modelOrigin,
		parms.modelAxis, NULL, 0, NULL );
	query.completed = true;
	return query;
}

idCollisionQuery idCollisionQueryExecute::Rotation( idCollisionModelManagerLocal &manager, const cmQueryParms_t &parms ) {
	idCollisionQuery query;
	if ( parms.rotation != NULL ) {
		manager.RotationInternal( &query.trace, parms.start, *parms.rotation, parms.traceModel,
			parms.traceModelAxis, parms.contentsMask, parms.model, parms.modelOrigin, parms.modelAxis );
	}
	query.completed = true;
	return query;
}

idCollisionQuery idCollisionQueryExecute::Contents( idCollisionModelManagerLocal &manager, const cmQueryParms_t &parms ) {
	idCollisionQuery query;
	query.contents = manager.ContentsInternal( parms.start, parms.traceModel, parms.traceModelAxis,
		parms.contentsMask, parms.model, parms.modelOrigin, parms.modelAxis );
	query.completed = true;
	return query;
}

idCollisionQuery idCollisionQueryExecute::Contacts( idCollisionModelManagerLocal &manager, const cmQueryParms_t &parms ) {
	idCollisionQuery query;
	query.numContacts = manager.ContactsInternal( parms.contacts, parms.maxContacts, parms.start,
		parms.direction, parms.depth, parms.traceModel, parms.traceModelAxis, parms.contentsMask,
		parms.model, parms.modelOrigin, parms.modelAxis );
	query.completed = true;
	return query;
}
