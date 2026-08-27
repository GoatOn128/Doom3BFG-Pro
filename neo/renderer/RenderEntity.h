#ifndef __RENDERENTITY_H__
#define __RENDERENTITY_H__

struct renderView_s;
struct renderEntity_s;
class idMaterial;
class idRenderWorld;
class idRenderWorldLocal;
class idRenderWorldCommitted;
class idRenderEntityCommitted;

typedef bool ( *deferredEntityCallback_t )( renderEntity_s *, const renderView_s * );

typedef struct renderEntity_s {
	idRenderModel *			hModel;
	int						entityNum;
	int						bodyId;
	idBounds				bounds;
	deferredEntityCallback_t callback;
	void *					callbackData;
	int						suppressSurfaceInViewID;
	int						suppressShadowInViewID;
	int						suppressShadowInLightID;
	int						allowSurfaceInViewID;
	idVec3					origin;
	idMat3					axis;
	const idMaterial *		customShader;
	const idMaterial *		referenceShader;
	const idDeclSkin *		customSkin;
	class idSoundEmitter *	referenceSound;
	float					shaderParms[ MAX_ENTITY_SHADER_PARMS ];
	class idUserInterface * gui[ MAX_RENDERENTITY_GUI ];
	struct renderView_s *	remoteRenderView;
	int						numJoints;
	idJointMat *			joints;
	float					modelDepthHack;
	bool					noSelfShadow;
	bool					noShadow;
	bool					noDynamicInteractions;
	bool					weaponDepthHack;
	bool					noOverlays;
	bool					skipMotionBlur;
	int						forceUpdate;
	int						timeGroup;
	int						xrayIndex;
} renderEntity_t;

class idRenderEntity : public renderEntity_t {
public:
							idRenderEntity();
							~idRenderEntity();

	void				CommitThisFrame( bool forceUpdate = false );
	void				Commit();
	void				FreeRenderEntity();
	void				ForceUpdate();
	int					GetIndex() const { return index; }
	bool				IsRegistered() const { return world != NULL && index >= 0; }
	void				ProjectOverlay( const idPlane localTextureAxis[2], const idMaterial * material );
	void				RemoveDecals();

private:
	friend class idRenderWorldLocal;
	friend class idRenderWorldCommitted;
	idRenderWorld *			world;
	int						index;
	idRenderEntityCommitted * committed;
	bool					needsCommit;
	bool					worldOwned;
};

#endif
