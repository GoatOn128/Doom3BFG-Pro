#ifndef __RENDERLIGHT_H__
#define __RENDERLIGHT_H__

class idRenderWorld;
class idRenderWorldLocal;
class idRenderWorldCommitted;
class idRenderLightCommitted;

// Per light contribution class from TNO idRenderLightParms::lightClass.
// LIGHT_NORMAL keeps the engine default behavior for zero initialized lights.
enum lightClass_t {
	LIGHT_NORMAL = 0,
	LIGHT_PRELIGHT_ONLY,			// never drawn as a view light, only feeds approximate lighting
	LIGHT_SLOWLIGHT_DYNAMIC_ONLY,
	LIGHT_SLOWLIGHT_PRIVATE_ONLY,
	LIGHT_DYNAMIC_ONLY,				// drawn in the view, never feeds approximate lighting queries
	LIGHT_PRIVATE_DYNAMIC_ONLY,
	LIGHT_PRIVATE_PRIME,
	LIGHT_BLENDED_ONLY
};

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
	// TNO idRenderLightParms gaps. Defaults keep the previous behavior.
	lightClass_t			lightClass;
	float					maxVisibleRange;		// 0 = unlimited, else the light is culled past this distance
	float					fadeVisibilityOver;		// extra range past maxVisibleRange before the hard cull
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
