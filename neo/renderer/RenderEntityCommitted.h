#ifndef __RENDERENTITYCOMMITTED_H__
#define __RENDERENTITYCOMMITTED_H__

class idRenderWorldLocal;
class idRenderWorldCommitted;
class idRenderModelDecal;
class idRenderModelOverlay;
struct areaReference_t;
struct viewEntity_t;

class idRenderEntityCommitted {
public:
							idRenderEntityCommitted();

	bool				IsDirectlyVisible() const;
	void				PostCommit();

	renderEntity_t			parms;
	float					modelMatrix[16];
	idRenderMatrix			modelRenderMatrix;
	idRenderMatrix			inverseBaseModelProject;
	idRenderWorldLocal *	world;
	int						index;
	int						lastModifiedFrameNum;
	bool					archived;
	idRenderModel *			dynamicModel;
	int						dynamicModelFrameCount;
	idRenderModel *			cachedDynamicModel;
	idBounds				localReferenceBounds;
	idBounds				globalReferenceBounds;
	int						viewCount;
	viewEntity_t *			viewEntity;
	idRenderModelDecal *	decals;
	idRenderModelOverlay *	overlays;
	areaReference_t *		entityRefs;
	bool					needsPortalSky;
	byte					postCommitMode;
};

#endif
