/*
===========================================================================

	Collision query boundary.

	This keeps the public Doom 3 BFG collision ABI intact while separating
	query description/execution from collision-model storage.  The layout is
	intentionally close to the idTech 5 collision flow so queries can later be
	batched without replacing the proven BFG narrow-phase implementation.

===========================================================================
*/

#ifndef __COLLISIONQUERY_H__
#define __COLLISIONQUERY_H__

#include "CollisionModel.h"

class idCollisionModelManagerLocal;

enum cmQueryType_t {
	CM_QUERY_TRANSLATION,
	CM_QUERY_ROTATION,
	CM_QUERY_CONTENTS,
	CM_QUERY_CONTACTS
};

struct cmQueryParms_t {
	cmQueryType_t			type;
	idVec3					start;
	idVec3					end;
	const idRotation *		rotation;
	idVec6					direction;
	float					depth;
	const idTraceModel *	traceModel;
	idMat3					traceModelAxis;
	int						contentsMask;
	cmHandle_t				model;
	idVec3					modelOrigin;
	idMat3					modelAxis;
	contactInfo_t *			contacts;
	int						maxContacts;

	cmQueryParms_t();
};

// A model plus its world-space placement.  idClip currently performs this
// expansion itself; retaining it here establishes the batching boundary used
// by the idTech 5 collision manager.
struct idPositionedCollisionModel {
	cmHandle_t	model;
	idVec3		origin;
	idMat3		axis;
	int			entityNum;
	int			physicsId;
	int			bodyId;
	int			contentsOverride;
	float		scale;
};

class idCollisionQuery {
public:
	idCollisionQuery();

	trace_t	trace;
	int		contents;
	int		numContacts;
	bool	completed;
};

class idCollisionQueryExecute {
public:
	static idCollisionQuery Translation( idCollisionModelManagerLocal &manager, const cmQueryParms_t &parms );
	static idCollisionQuery Rotation( idCollisionModelManagerLocal &manager, const cmQueryParms_t &parms );
	static idCollisionQuery Contents( idCollisionModelManagerLocal &manager, const cmQueryParms_t &parms );
	static idCollisionQuery Contacts( idCollisionModelManagerLocal &manager, const cmQueryParms_t &parms );
};

#endif /* !__COLLISIONQUERY_H__ */
