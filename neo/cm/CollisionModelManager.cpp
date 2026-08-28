/*
===========================================================================

	idTech 5-style collision manager/query boundary over the BFG collision
	model and narrow-phase implementation.

===========================================================================
*/

#pragma hdrstop
#include "../idlib/precompiled.h"

#include "CollisionModel_local.h"
#include "CollisionQuery.h"
#include "CollisionQueryJobManager.h"

static idCVar cm_showQueryStats( "cm_showQueryStats", "0", CVAR_GAME | CVAR_BOOL,
	"show collision query counts and CPU time for each game frame" );

void idCollisionModelManagerLocal::StartQueryFrame() {
	queryFrameActive = true;
	pendingQueryStats = false;
	memset( queryCount, 0, sizeof( queryCount ) );
	memset( queryTimeMicroSec, 0, sizeof( queryTimeMicroSec ) );
	memset( queryParallelBatchCount, 0, sizeof( queryParallelBatchCount ) );
	memset( queryParallelJobCount, 0, sizeof( queryParallelJobCount ) );
}

void idCollisionModelManagerLocal::SubmitQueries() {
	// Immediate game-physics queries submit at their explicit batch boundary.
	// This frame hook remains the flush point for future fire-and-collect users.
}

void idCollisionModelManagerLocal::WaitForAllQueries() {
	queryJobManager.WaitForAllQueries();
	if ( pendingQueryStats ) {
		queryTimeMicroSec[pendingQueryType] += Sys_Microseconds() - pendingQueryStartTime;
		pendingQueryStats = false;
	}
}

void idCollisionModelManagerLocal::EndQueryFrame() {
	if ( cm_showQueryStats.GetBool() ) {
		common->Printf( "cm queries: translation %llu (%llu us), rotation %llu (%llu us), "
			"contents %llu (%llu us), contacts %llu (%llu us)\n",
			queryCount[CM_QUERY_TRANSLATION], queryTimeMicroSec[CM_QUERY_TRANSLATION],
			queryCount[CM_QUERY_ROTATION], queryTimeMicroSec[CM_QUERY_ROTATION],
			queryCount[CM_QUERY_CONTENTS], queryTimeMicroSec[CM_QUERY_CONTENTS],
			queryCount[CM_QUERY_CONTACTS], queryTimeMicroSec[CM_QUERY_CONTACTS] );
		common->Printf( "cm worker jobs: translation %llu/%llu, rotation %llu/%llu, "
			"contents %llu/%llu, contacts %llu/%llu (batches/queries)\n",
			queryParallelBatchCount[CM_QUERY_TRANSLATION], queryParallelJobCount[CM_QUERY_TRANSLATION],
			queryParallelBatchCount[CM_QUERY_ROTATION], queryParallelJobCount[CM_QUERY_ROTATION],
			queryParallelBatchCount[CM_QUERY_CONTENTS], queryParallelJobCount[CM_QUERY_CONTENTS],
			queryParallelBatchCount[CM_QUERY_CONTACTS], queryParallelJobCount[CM_QUERY_CONTACTS] );
	}
	queryFrameActive = false;
}

void idCollisionModelManagerLocal::Translation( trace_t *results, const idVec3 &start, const idVec3 &end,
		const idTraceModel *trm, const idMat3 &trmAxis, int contentMask, cmHandle_t model,
		const idVec3 &modelOrigin, const idMat3 &modelAxis ) {
	cmQueryParms_t parms;
	parms.type = CM_QUERY_TRANSLATION;
	parms.start = start;
	parms.end = end;
	parms.traceModel = trm;
	parms.traceModelAxis = trmAxis;
	parms.contentsMask = contentMask;
	parms.model = model;
	parms.modelOrigin = modelOrigin;
	parms.modelAxis = modelAxis;
	const bool trackStats = queryFrameActive && cm_showQueryStats.GetBool();
	const uint64 startTime = trackStats ? Sys_Microseconds() : 0;
	const idCollisionQuery query = idCollisionQueryExecute::Translation( *this, parms );
	if ( trackStats ) {
		queryCount[CM_QUERY_TRANSLATION]++;
		queryTimeMicroSec[CM_QUERY_TRANSLATION] += Sys_Microseconds() - startTime;
	}
	*results = query.trace;
}

bool idCollisionModelManagerLocal::BeginTranslation( trace_t *results, const idVec3 &start,
		const idVec3 &end, const idTraceModel *trm, const idMat3 &trmAxis, int contentMask,
		cmHandle_t model, const idVec3 &modelOrigin, const idMat3 &modelAxis ) {
	const bool trackStats = queryFrameActive && cm_showQueryStats.GetBool();
	const uint64 startTime = trackStats ? Sys_Microseconds() : 0;
	const bool queued = queryJobManager.BeginTranslation( *this, results, start, end, trm,
		trmAxis, contentMask, model, modelOrigin, modelAxis, !cm_debugCollision.GetBool() );
	if ( trackStats ) {
		queryCount[CM_QUERY_TRANSLATION]++;
		if ( queued ) {
			queryParallelBatchCount[CM_QUERY_TRANSLATION]++;
			queryParallelJobCount[CM_QUERY_TRANSLATION]++;
			pendingQueryStats = true;
			pendingQueryType = CM_QUERY_TRANSLATION;
			pendingQueryStartTime = startTime;
		} else {
			queryTimeMicroSec[CM_QUERY_TRANSLATION] += Sys_Microseconds() - startTime;
		}
	}
	return queued;
}

void idCollisionModelManagerLocal::TranslationModels( trace_t *results, const int numModels,
		const idVec3 &start, const idVec3 &end, const idTraceModel *trm, const idMat3 &trmAxis,
		int contentMask, const idPositionedCollisionModel *positionedModels ) {
	if ( results == NULL || positionedModels == NULL || numModels <= 0 ) {
		return;
	}

	const bool trackStats = queryFrameActive && cm_showQueryStats.GetBool();
	const uint64 startTime = trackStats ? Sys_Microseconds() : 0;
	const bool ranParallel = queryJobManager.Translation( *this, results, numModels,
		start, end, trm, trmAxis, contentMask, positionedModels, !cm_debugCollision.GetBool() );
	if ( trackStats && ranParallel ) {
		queryParallelBatchCount[CM_QUERY_TRANSLATION]++;
		queryParallelJobCount[CM_QUERY_TRANSLATION] += numModels;
	}

	for ( int i = 0; i < numModels; ++i ) {
		if ( results[i].fraction < 1.0f ) {
			results[i].c.entityNum = positionedModels[i].entityNum;
			results[i].c.id = positionedModels[i].physicsId;
		}
	}

	if ( trackStats ) {
		queryCount[CM_QUERY_TRANSLATION] += numModels;
		queryTimeMicroSec[CM_QUERY_TRANSLATION] += Sys_Microseconds() - startTime;
	}
}

void idCollisionModelManagerLocal::TranslationQueries( idCollisionTranslationQuery *queries,
		const int numQueries ) {
	if ( queries == NULL || numQueries <= 0 ) {
		return;
	}

	const bool trackStats = queryFrameActive && cm_showQueryStats.GetBool();
	const uint64 startTime = trackStats ? Sys_Microseconds() : 0;
	const bool ranParallel = queryJobManager.TranslationQueries( *this, queries, numQueries,
		!cm_debugCollision.GetBool() );
	if ( trackStats ) {
		queryCount[CM_QUERY_TRANSLATION] += numQueries;
		queryTimeMicroSec[CM_QUERY_TRANSLATION] += Sys_Microseconds() - startTime;
		if ( ranParallel ) {
			queryParallelBatchCount[CM_QUERY_TRANSLATION]++;
			queryParallelJobCount[CM_QUERY_TRANSLATION] += numQueries;
		}
	}
}

void idCollisionModelManagerLocal::Rotation( trace_t *results, const idVec3 &start, const idRotation &rotation,
		const idTraceModel *trm, const idMat3 &trmAxis, int contentMask, cmHandle_t model,
		const idVec3 &modelOrigin, const idMat3 &modelAxis ) {
	cmQueryParms_t parms;
	parms.type = CM_QUERY_ROTATION;
	parms.start = start;
	parms.rotation = &rotation;
	parms.traceModel = trm;
	parms.traceModelAxis = trmAxis;
	parms.contentsMask = contentMask;
	parms.model = model;
	parms.modelOrigin = modelOrigin;
	parms.modelAxis = modelAxis;
	const bool trackStats = queryFrameActive && cm_showQueryStats.GetBool();
	const uint64 startTime = trackStats ? Sys_Microseconds() : 0;
	const idCollisionQuery query = idCollisionQueryExecute::Rotation( *this, parms );
	if ( trackStats ) {
		queryCount[CM_QUERY_ROTATION]++;
		queryTimeMicroSec[CM_QUERY_ROTATION] += Sys_Microseconds() - startTime;
	}
	*results = query.trace;
}

bool idCollisionModelManagerLocal::BeginRotation( trace_t *results, const idVec3 &start,
		const idRotation &rotation, const idTraceModel *trm, const idMat3 &trmAxis,
		int contentMask, cmHandle_t model, const idVec3 &modelOrigin, const idMat3 &modelAxis ) {
	const bool trackStats = queryFrameActive && cm_showQueryStats.GetBool();
	const uint64 startTime = trackStats ? Sys_Microseconds() : 0;
	const bool queued = queryJobManager.BeginRotation( *this, results, start, rotation, trm,
		trmAxis, contentMask, model, modelOrigin, modelAxis, !cm_debugCollision.GetBool() );
	if ( trackStats ) {
		queryCount[CM_QUERY_ROTATION]++;
		if ( queued ) {
			queryParallelBatchCount[CM_QUERY_ROTATION]++;
			queryParallelJobCount[CM_QUERY_ROTATION]++;
			pendingQueryStats = true;
			pendingQueryType = CM_QUERY_ROTATION;
			pendingQueryStartTime = startTime;
		} else {
			queryTimeMicroSec[CM_QUERY_ROTATION] += Sys_Microseconds() - startTime;
		}
	}
	return queued;
}

void idCollisionModelManagerLocal::RotationModels( trace_t *results, const int numModels,
		const idVec3 &start, const idRotation &rotation, const idTraceModel *trm, const idMat3 &trmAxis,
		int contentMask, const idPositionedCollisionModel *positionedModels ) {
	if ( results == NULL || positionedModels == NULL || numModels <= 0 ) {
		return;
	}

	const bool trackStats = queryFrameActive && cm_showQueryStats.GetBool();
	const uint64 startTime = trackStats ? Sys_Microseconds() : 0;
	const bool ranParallel = queryJobManager.Rotation( *this, results, numModels,
		start, rotation, trm, trmAxis, contentMask, positionedModels, !cm_debugCollision.GetBool() );
	if ( trackStats && ranParallel ) {
		queryParallelBatchCount[CM_QUERY_ROTATION]++;
		queryParallelJobCount[CM_QUERY_ROTATION] += numModels;
	}

	for ( int i = 0; i < numModels; ++i ) {
		if ( results[i].fraction < 1.0f ) {
			results[i].c.entityNum = positionedModels[i].entityNum;
			results[i].c.id = positionedModels[i].physicsId;
		}
	}

	if ( trackStats ) {
		queryCount[CM_QUERY_ROTATION] += numModels;
		queryTimeMicroSec[CM_QUERY_ROTATION] += Sys_Microseconds() - startTime;
	}
}

void idCollisionModelManagerLocal::RotationQueries( idCollisionRotationQuery *queries,
		const int numQueries ) {
	if ( queries == NULL || numQueries <= 0 ) {
		return;
	}

	const bool trackStats = queryFrameActive && cm_showQueryStats.GetBool();
	const uint64 startTime = trackStats ? Sys_Microseconds() : 0;
	const bool ranParallel = queryJobManager.RotationQueries( *this, queries, numQueries,
		!cm_debugCollision.GetBool() );
	if ( trackStats ) {
		queryCount[CM_QUERY_ROTATION] += numQueries;
		queryTimeMicroSec[CM_QUERY_ROTATION] += Sys_Microseconds() - startTime;
		if ( ranParallel ) {
			queryParallelBatchCount[CM_QUERY_ROTATION]++;
			queryParallelJobCount[CM_QUERY_ROTATION] += numQueries;
		}
	}
}

int idCollisionModelManagerLocal::Contents( const idVec3 &start, const idTraceModel *trm,
		const idMat3 &trmAxis, int contentMask, cmHandle_t model, const idVec3 &modelOrigin,
		const idMat3 &modelAxis ) {
	cmQueryParms_t parms;
	parms.type = CM_QUERY_CONTENTS;
	parms.start = start;
	parms.end = start;
	parms.traceModel = trm;
	parms.traceModelAxis = trmAxis;
	parms.contentsMask = contentMask;
	parms.model = model;
	parms.modelOrigin = modelOrigin;
	parms.modelAxis = modelAxis;
	const bool trackStats = queryFrameActive && cm_showQueryStats.GetBool();
	const uint64 startTime = trackStats ? Sys_Microseconds() : 0;
	const int result = idCollisionQueryExecute::Contents( *this, parms ).contents;
	if ( trackStats ) {
		queryCount[CM_QUERY_CONTENTS]++;
		queryTimeMicroSec[CM_QUERY_CONTENTS] += Sys_Microseconds() - startTime;
	}
	return result;
}

void idCollisionModelManagerLocal::ContentsModels( int *results, const int numModels,
		const idVec3 &start, const idTraceModel *trm, const idMat3 &trmAxis, int contentMask,
		const idPositionedCollisionModel *positionedModels ) {
	if ( results == NULL || positionedModels == NULL || numModels <= 0 ) {
		return;
	}

	const bool trackStats = queryFrameActive && cm_showQueryStats.GetBool();
	const uint64 startTime = trackStats ? Sys_Microseconds() : 0;
	const bool ranParallel = queryJobManager.Contents( *this, results, numModels,
		start, trm, trmAxis, contentMask, positionedModels, !cm_debugCollision.GetBool() );
	if ( trackStats && ranParallel ) {
		queryParallelBatchCount[CM_QUERY_CONTENTS]++;
		queryParallelJobCount[CM_QUERY_CONTENTS] += numModels;
	}

	if ( trackStats ) {
		queryCount[CM_QUERY_CONTENTS] += numModels;
		queryTimeMicroSec[CM_QUERY_CONTENTS] += Sys_Microseconds() - startTime;
	}
}

int idCollisionModelManagerLocal::Contacts( contactInfo_t *contacts, const int maxContacts,
		const idVec3 &start, const idVec6 &dir, const float depth, const idTraceModel *trm,
		const idMat3 &trmAxis, int contentMask, cmHandle_t model, const idVec3 &modelOrigin,
		const idMat3 &modelAxis ) {
	cmQueryParms_t parms;
	parms.type = CM_QUERY_CONTACTS;
	parms.start = start;
	parms.direction = dir;
	parms.depth = depth;
	parms.traceModel = trm;
	parms.traceModelAxis = trmAxis;
	parms.contentsMask = contentMask;
	parms.model = model;
	parms.modelOrigin = modelOrigin;
	parms.modelAxis = modelAxis;
	parms.contacts = contacts;
	parms.maxContacts = maxContacts;
	const bool trackStats = queryFrameActive && cm_showQueryStats.GetBool();
	const uint64 startTime = trackStats ? Sys_Microseconds() : 0;
	const int result = idCollisionQueryExecute::Contacts( *this, parms ).numContacts;
	if ( trackStats ) {
		queryCount[CM_QUERY_CONTACTS]++;
		queryTimeMicroSec[CM_QUERY_CONTACTS] += Sys_Microseconds() - startTime;
	}
	return result;
}

void idCollisionModelManagerLocal::ContactsModels( contactInfo_t *contacts, int *numContacts,
		const int maxContactsPerModel, const int numModels, const idVec3 &start,
		const idVec6 &dir, const float depth, const idTraceModel *trm, const idMat3 &trmAxis,
		int contentMask, const idPositionedCollisionModel *positionedModels ) {
	if ( contacts == NULL || numContacts == NULL || positionedModels == NULL ||
		numModels <= 0 || maxContactsPerModel <= 0 ) {
		return;
	}

	const bool trackStats = queryFrameActive && cm_showQueryStats.GetBool();
	const uint64 startTime = trackStats ? Sys_Microseconds() : 0;
	const bool ranParallel = queryJobManager.Contacts( *this, contacts, numContacts,
		maxContactsPerModel, numModels, start, dir, depth, trm, trmAxis, contentMask,
		positionedModels, !cm_debugCollision.GetBool() );
	if ( trackStats && ranParallel ) {
		queryParallelBatchCount[CM_QUERY_CONTACTS]++;
		queryParallelJobCount[CM_QUERY_CONTACTS] += numModels;
	}

	if ( trackStats ) {
		queryCount[CM_QUERY_CONTACTS] += numModels;
		queryTimeMicroSec[CM_QUERY_CONTACTS] += Sys_Microseconds() - startTime;
	}
}
