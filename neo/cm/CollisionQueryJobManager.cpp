/*
===========================================================================

	Collision query worker submission and result collection.

===========================================================================
*/

#pragma hdrstop
#include "../idlib/precompiled.h"

#include "CollisionModel_local.h"
#include "CollisionQueryJobManager.h"

idCVar cm_parallelCollision( "cm_parallelCollision", "1", CVAR_GAME | CVAR_BOOL,
	"run independent positioned collision-model queries on worker threads" );
idCVar cm_parallelCollisionMinModels( "cm_parallelCollisionMinModels", "4", CVAR_GAME | CVAR_INTEGER,
	"minimum positioned collision models required before collision jobs are submitted", 2, 64 );
idCVar cm_asyncWorldCollision( "cm_asyncWorldCollision", "1", CVAR_GAME | CVAR_BOOL,
	"overlap sufficiently expensive world collision queries with game broadphase work" );
idCVar cm_asyncWorldCollisionMinTranslation( "cm_asyncWorldCollisionMinTranslation", "64", CVAR_GAME | CVAR_FLOAT,
	"minimum translation distance before an individual world query is submitted to a worker", 0.0f, 4096.0f );
idCVar cm_asyncWorldCollisionMinRotation( "cm_asyncWorldCollisionMinRotation", "15", CVAR_GAME | CVAR_FLOAT,
	"minimum rotation angle before an individual world query is submitted to a worker", 0.0f, 180.0f );

static const int CM_MAX_BATCHED_COLLISION_MODELS = 4096;

static void CM_ExecuteQueryJob( void *data ) {
	cmExecuteQueryJob_t *job = static_cast<cmExecuteQueryJob_t *>( data );
	switch ( job->parms.type ) {
		case CM_QUERY_TRANSLATION:
			*job->traceResult = idCollisionQueryExecute::Translation( *job->manager, job->parms ).trace;
			break;
		case CM_QUERY_ROTATION:
			*job->traceResult = idCollisionQueryExecute::Rotation( *job->manager, job->parms ).trace;
			break;
		case CM_QUERY_CONTENTS:
			*job->integerResult = idCollisionQueryExecute::Contents( *job->manager, job->parms ).contents;
			break;
		case CM_QUERY_CONTACTS:
			*job->integerResult = idCollisionQueryExecute::Contacts( *job->manager, job->parms ).numContacts;
			break;
	}
}

REGISTER_PARALLEL_JOB( CM_ExecuteQueryJob, "CM_ExecuteQueryJob" );

idCollisionQueryJobManager::idCollisionQueryJobManager() :
	jobList( NULL ),
	jobs( 64 ) {
}

bool idCollisionQueryJobManager::BeginJob( void ( *function )( void * ), void *job,
		bool allowParallel ) {
	const bool runParallel = allowParallel && cm_parallelCollision.GetBool() &&
		parallelJobManager != NULL;
	if ( !runParallel ) {
		function( job );
		return false;
	}

	if ( jobList == NULL ) {
		jobList = parallelJobManager->AllocJobList( JOBLIST_UTILITY,
			JOBLIST_PRIORITY_MEDIUM, CM_MAX_BATCHED_COLLISION_MODELS, 0, NULL );
	}
	assert( !jobList->IsSubmitted() );
	jobList->AddJob( function, job );
	jobList->Submit();
	return true;
}

bool idCollisionQueryJobManager::BeginTranslation( idCollisionModelManagerLocal &manager,
		trace_t *result, const idVec3 &start, const idVec3 &end, const idTraceModel *trm,
		const idMat3 &trmAxis, int contentMask, cmHandle_t model,
		const idVec3 &modelOrigin, const idMat3 &modelAxis, bool allowParallel ) {
	jobs.SetNum( 1 );
	cmExecuteQueryJob_t &job = jobs[0];
	job.manager = &manager;
	job.traceResult = result;
	job.integerResult = NULL;
	job.parms.type = CM_QUERY_TRANSLATION;
	job.parms.start = start;
	job.parms.end = end;
	job.parms.traceModel = trm;
	job.parms.traceModelAxis = trmAxis;
	job.parms.contentsMask = contentMask;
	job.parms.model = model;
	job.parms.modelOrigin = modelOrigin;
	job.parms.modelAxis = modelAxis;
	const float minDistance = cm_asyncWorldCollisionMinTranslation.GetFloat();
	const bool asyncWorthwhile = cm_asyncWorldCollision.GetBool() &&
		( end - start ).LengthSqr() >= minDistance * minDistance;
	return BeginJob( CM_ExecuteQueryJob, &job, allowParallel && asyncWorthwhile );
}

bool idCollisionQueryJobManager::BeginRotation( idCollisionModelManagerLocal &manager,
		trace_t *result, const idVec3 &start, const idRotation &rotation,
		const idTraceModel *trm, const idMat3 &trmAxis, int contentMask, cmHandle_t model,
		const idVec3 &modelOrigin, const idMat3 &modelAxis, bool allowParallel ) {
	jobs.SetNum( 1 );
	cmExecuteQueryJob_t &job = jobs[0];
	job.manager = &manager;
	job.traceResult = result;
	job.integerResult = NULL;
	job.parms.type = CM_QUERY_ROTATION;
	job.parms.start = start;
	job.parms.rotation = &rotation;
	job.parms.traceModel = trm;
	job.parms.traceModelAxis = trmAxis;
	job.parms.contentsMask = contentMask;
	job.parms.model = model;
	job.parms.modelOrigin = modelOrigin;
	job.parms.modelAxis = modelAxis;
	const bool asyncWorthwhile = cm_asyncWorldCollision.GetBool() &&
		idMath::Fabs( rotation.GetAngle() ) >= cm_asyncWorldCollisionMinRotation.GetFloat();
	return BeginJob( CM_ExecuteQueryJob, &job, allowParallel && asyncWorthwhile );
}

bool idCollisionQueryJobManager::ExecuteJobs( void ( *function )( void * ), void *jobs,
		int jobStride, int numJobs, bool allowParallel ) {
	const bool runParallel = allowParallel && cm_parallelCollision.GetBool() &&
		parallelJobManager != NULL && numJobs >= cm_parallelCollisionMinModels.GetInteger() &&
		numJobs <= CM_MAX_BATCHED_COLLISION_MODELS;

	if ( runParallel ) {
		if ( jobList == NULL ) {
			jobList = parallelJobManager->AllocJobList( JOBLIST_UTILITY,
				JOBLIST_PRIORITY_MEDIUM, CM_MAX_BATCHED_COLLISION_MODELS, 0, NULL );
		}
		for ( int i = 0; i < numJobs; ++i ) {
			jobList->AddJob( function, static_cast<byte *>( jobs ) + i * jobStride );
		}
		jobList->Submit();
		jobList->Wait();
	} else {
		for ( int i = 0; i < numJobs; ++i ) {
			function( static_cast<byte *>( jobs ) + i * jobStride );
		}
	}
	return runParallel;
}

bool idCollisionQueryJobManager::Translation( idCollisionModelManagerLocal &manager,
		trace_t *results, const int numModels, const idVec3 &start, const idVec3 &end,
		const idTraceModel *trm, const idMat3 &trmAxis, int contentMask,
		const idPositionedCollisionModel *positionedModels, bool allowParallel ) {
	jobs.SetNum( numModels );
	for ( int i = 0; i < numModels; ++i ) {
		cmExecuteQueryJob_t &job = jobs[i];
		job.manager = &manager;
		job.traceResult = &results[i];
		job.integerResult = NULL;
		job.parms.type = CM_QUERY_TRANSLATION;
		job.parms.start = start;
		job.parms.end = end;
		job.parms.traceModel = trm;
		job.parms.traceModelAxis = trmAxis;
		job.parms.contentsMask = contentMask;
		job.parms.model = positionedModels[i].model;
		job.parms.modelOrigin = positionedModels[i].origin;
		job.parms.modelAxis = positionedModels[i].axis;
	}
	return ExecuteJobs( CM_ExecuteQueryJob, jobs.Ptr(), sizeof( jobs[0] ), numModels, allowParallel );
}

bool idCollisionQueryJobManager::Rotation( idCollisionModelManagerLocal &manager,
		trace_t *results, const int numModels, const idVec3 &start, const idRotation &rotation,
		const idTraceModel *trm, const idMat3 &trmAxis, int contentMask,
		const idPositionedCollisionModel *positionedModels, bool allowParallel ) {
	jobs.SetNum( numModels );
	for ( int i = 0; i < numModels; ++i ) {
		cmExecuteQueryJob_t &job = jobs[i];
		job.manager = &manager;
		job.traceResult = &results[i];
		job.integerResult = NULL;
		job.parms.type = CM_QUERY_ROTATION;
		job.parms.start = start;
		job.parms.rotation = &rotation;
		job.parms.traceModel = trm;
		job.parms.traceModelAxis = trmAxis;
		job.parms.contentsMask = contentMask;
		job.parms.model = positionedModels[i].model;
		job.parms.modelOrigin = positionedModels[i].origin;
		job.parms.modelAxis = positionedModels[i].axis;
	}
	return ExecuteJobs( CM_ExecuteQueryJob, jobs.Ptr(), sizeof( jobs[0] ), numModels, allowParallel );
}

bool idCollisionQueryJobManager::Contents( idCollisionModelManagerLocal &manager, int *results,
		const int numModels, const idVec3 &start, const idTraceModel *trm,
		const idMat3 &trmAxis, int contentMask,
		const idPositionedCollisionModel *positionedModels, bool allowParallel ) {
	jobs.SetNum( numModels );
	for ( int i = 0; i < numModels; ++i ) {
		cmExecuteQueryJob_t &job = jobs[i];
		job.manager = &manager;
		job.traceResult = NULL;
		job.integerResult = &results[i];
		job.parms.type = CM_QUERY_CONTENTS;
		job.parms.start = start;
		job.parms.end = start;
		job.parms.traceModel = trm;
		job.parms.traceModelAxis = trmAxis;
		job.parms.contentsMask = contentMask;
		job.parms.model = positionedModels[i].model;
		job.parms.modelOrigin = positionedModels[i].origin;
		job.parms.modelAxis = positionedModels[i].axis;
	}
	return ExecuteJobs( CM_ExecuteQueryJob, jobs.Ptr(), sizeof( jobs[0] ), numModels, allowParallel );
}

bool idCollisionQueryJobManager::Contacts( idCollisionModelManagerLocal &manager,
		contactInfo_t *contacts, int *numContacts, const int maxContactsPerModel,
		const int numModels, const idVec3 &start, const idVec6 &dir, const float depth,
		const idTraceModel *trm, const idMat3 &trmAxis, int contentMask,
		const idPositionedCollisionModel *positionedModels, bool allowParallel ) {
	jobs.SetNum( numModels );
	for ( int i = 0; i < numModels; ++i ) {
		cmExecuteQueryJob_t &job = jobs[i];
		job.manager = &manager;
		job.traceResult = NULL;
		job.integerResult = &numContacts[i];
		job.parms.type = CM_QUERY_CONTACTS;
		job.parms.start = start;
		job.parms.direction = dir;
		job.parms.depth = depth;
		job.parms.traceModel = trm;
		job.parms.traceModelAxis = trmAxis;
		job.parms.contentsMask = contentMask;
		job.parms.model = positionedModels[i].model;
		job.parms.modelOrigin = positionedModels[i].origin;
		job.parms.modelAxis = positionedModels[i].axis;
		job.parms.contacts = contacts + i * maxContactsPerModel;
		job.parms.maxContacts = maxContactsPerModel;
	}
	return ExecuteJobs( CM_ExecuteQueryJob, jobs.Ptr(), sizeof( jobs[0] ), numModels, allowParallel );
}

void idCollisionQueryJobManager::WaitForAllQueries() {
	if ( jobList != NULL && jobList->IsSubmitted() ) {
		jobList->Wait();
	}
}

void idCollisionQueryJobManager::Shutdown() {
	jobs.Clear();
	if ( jobList == NULL ) {
		return;
	}
	WaitForAllQueries();
	if ( parallelJobManager != NULL ) {
		parallelJobManager->FreeJobList( jobList );
	}
	jobList = NULL;
}
