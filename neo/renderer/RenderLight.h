#ifndef __RENDERLIGHT_H__
#define __RENDERLIGHT_H__

class idRenderWorld;
class idRenderWorldLocal;
class idRenderWorldCommitted;
class idRenderLightCommitted;

typedef struct renderLight_s {
	idMat3					axis;
	idVec3					origin;
	int						suppressLightInViewID;
	int						allowLightInViewID;
	bool					forceShadows;
	bool					noShadows;
	bool					noSpecular;
	bool					pointLight;
	bool					parallel;
	idVec3					lightRadius;
	idVec3					lightCenter;
	idVec3					target;
	idVec3					right;
	idVec3					up;
	idVec3					start;
	idVec3					end;
	int						lightId;
	const idMaterial *		shader;
	float					shaderParms[ MAX_ENTITY_SHADER_PARMS ];
	idSoundEmitter *		referenceSound;
} renderLight_t;

class idRenderLight : public renderLight_t {
public:
							idRenderLight();
							~idRenderLight();

	void				CommitThisFrame( bool forceUpdate = false );
	void				Commit();
	void				FreeRenderLight();
	void				ForceUpdate();
	int					GetIndex() const { return index; }
	bool				IsRegistered() const { return world != NULL && index >= 0; }

private:
	friend class idRenderWorldLocal;
	friend class idRenderWorldCommitted;
	idRenderWorld *			world;
	int						index;
	idRenderLightCommitted * committed;
	const idMaterial *		stagedLightShader;
	idImage *				stagedFalloffImage;
	bool					needsCommit;
	bool					worldOwned;
};

#endif
