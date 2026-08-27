/*
===========================================================================

	Deterministic collision query result merging.

===========================================================================
*/

#pragma hdrstop
#include "../idlib/precompiled.h"

#include "CollisionMerge.h"

bool idCollisionDetectionMerge::MergeTraceResult( trace_t &result, const trace_t &candidate,
		int entityNum, int physicsId ) {
	if ( candidate.fraction >= result.fraction ) {
		return false;
	}
	result = candidate;
	result.c.entityNum = entityNum;
	result.c.id = physicsId;
	return true;
}

int idCollisionDetectionMerge::MergeContentsResults( int contents, const int *results,
		const idPositionedCollisionModel *models, int numModels, int contentMask ) {
	for ( int i = 0; i < numModels; ++i ) {
		if ( results[i] != 0 ) {
			contents |= models[i].contentsOverride & contentMask;
		}
	}
	return contents;
}

int idCollisionDetectionMerge::MergeContactsResults( contactInfo_t *output, int maxContacts,
		int numContacts, const contactInfo_t *inputs, const int *inputCounts,
		int contactsPerModel, const idPositionedCollisionModel *models, int numModels ) {
	for ( int i = 0; i < numModels && numContacts < maxContacts; ++i ) {
		const int copyCount = Min( inputCounts[i], maxContacts - numContacts );
		const contactInfo_t *source = inputs + i * contactsPerModel;
		for ( int j = 0; j < copyCount; ++j ) {
			output[numContacts] = source[j];
			output[numContacts].entityNum = models[i].entityNum;
			output[numContacts].id = models[i].physicsId;
			numContacts++;
		}
	}
	return numContacts;
}
