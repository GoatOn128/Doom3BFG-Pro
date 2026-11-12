#ifndef __RENDERLIGHTCOMMITTED_H__
#define __RENDERLIGHTCOMMITTED_H__

class idRenderWorldLocal;
class idRenderWorldCommitted;
struct areaReference_t;
struct doublePortal_s;
struct viewLight_t;

class idRenderLightCommitted {
public:
							idRenderLightCommitted();

	bool				LightCastsShadows() const { return parms.forceShadows || ( !parms.noShadows && lightShader->LightCastsShadows() ); }
	void				PostCommit();

	renderLight_t			parms;
	idRenderWorldLocal *	world;
	int						index;
	int						areaNum;
	int						lastModifiedFrameNum;
	bool					archived;
	idPlane					lightProject[4];
	idRenderMatrix			baseLightProject;
	idRenderMatrix			inverseBaseLightProject;
	const idMaterial *		lightShader;
	idImage *				falloffImage;
	idVec3					globalLightOrigin;
	idBounds				globalLightBounds;
	int						viewCount;
	viewLight_t *			viewLight;
	areaReference_t *		references;
	areaReference_t *		approxReferences;	// TNO split: approximate lighting area references
	bool					needsReferences;
	doublePortal_s *		foggedPortals;
	bool					needsPostCommit;
};

#endif
