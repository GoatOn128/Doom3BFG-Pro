/*
===========================================================================

	Deterministic collision query result merging.

===========================================================================
*/

#ifndef __COLLISIONMERGE_H__
#define __COLLISIONMERGE_H__

#include "CollisionQuery.h"

class idCollisionDetectionMerge {
public:
	static bool	MergeTraceResult( trace_t &result, const trace_t &candidate,
					int entityNum, int physicsId );
	static int	MergeContentsResults( int initialContents, const int *results,
					const idPositionedCollisionModel *models, int numModels, int contentMask );
	static int	MergeContactsResults( contactInfo_t *output, int maxContacts, int initialContacts,
					const contactInfo_t *inputs, const int *inputCounts, int contactsPerModel,
					const idPositionedCollisionModel *models, int numModels );
};

#endif /* !__COLLISIONMERGE_H__ */
