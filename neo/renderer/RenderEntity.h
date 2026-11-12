#ifndef __RENDERENTITY_H__
#define __RENDERENTITY_H__

#include "ScreenRect.h"

struct renderView_s;
struct renderEntity_s;
class idMaterial;
class idRenderWorld;
class idRenderWorldLocal;
class idRenderWorldCommitted;
class idRenderEntityCommitted;

typedef bool ( *deferredEntityCallback_t )( renderEntity_s *, const renderView_s * );

// Per model dim shadow selection from TNO idRenderModelParms::castDimShadows.
// Zero stays the engine default so zero initialized render entities keep the
// current behavior where the global r_dimShadow cvars decide everything.
enum dimShadow_t {
	DIMSHADOW_DEFAULT = 0,		// follow the global r_dimShadow settings
	DIMSHADOW_OFF,				// never cast a dim shadow
	DIMSHADOW_ON,				// cast with the global quality settings
	DIMSHADOW_HIGH_QUALITY		// cast with the high quality draw program
};

// Per model depth sort class from TNO modelDepthSort_t. NORMAL keeps the
// material sort order, every other class moves the whole model into a
// matching sort band. Zero stays the engine default so zero initialized
// render entities keep the previous behavior.
enum modelDepthSort_t {
	MODEL_DEPTH_SORT_FIRST = -6,
	MODEL_DEPTH_SORT_PORTAL_INSIDE = -5,
	MODEL_DEPTH_SORT_PORTAL_PLANE = -4,
	MODEL_DEPTH_SORT_VIEW_HEAD = -3,
	MODEL_DEPTH_SORT_VIEW_BODY = -2,
	MODEL_DEPTH_SORT_SCENE_MODEL = -1,
	MODEL_DEPTH_SORT_NORMAL = 0,
	MODEL_DEPTH_SORT_AFTER_WORLD = 1,
	MODEL_DEPTH_SORT_SKY = 8,
	MODEL_DEPTH_SORT_TRANSPARENT = 9,
	MODEL_DEPTH_SORT_LAST = 10
};

// Per model highlight selection from TNO idRenderModelParms::highlightColor.
// The engine has no highlight render path yet, the value is stored for the
// game side plumbing.
enum highlightColor_t {
	HIGHLIGHT_COLOR_NONE = 0,
	HIGHLIGHT_COLOR_CONSTANT,
	HIGHLIGHT_COLOR_USE,
	HIGHLIGHT_COLOR_ITEM,
	HIGHLIGHT_COLOR_LOOT,
	HIGHLIGHT_COLOR_CORPSE,
	HIGHLIGHT_COLOR_EXPLODE,
	HIGHLIGHT_COLOR_MAXED_HEALTH,
	HIGHLIGHT_COLOR_MAXED_AMMO,
	HIGHLIGHT_COLOR_MAX
};

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
	// TNO idRenderModelParms gaps. Default values keep the previous behavior
	// of the global cvars and bounds.
	int						groupMasterIndex;		// TNO model group master: own index for a master, 0 = not grouped
	dimShadow_t				castDimShadows;
	modelDepthSort_t		modelDepthSort;			// TNO per model depth sort class, NORMAL keeps the material sort
	idScreenRect			modelViewport;			// TNO idRenderModelParms::viewport, all zero = no override
	float					modelFade;				// game driven model fade, no render path consumes it yet
	float					maxVisibleRange;		// 0 = unlimited, else the model is culled past this distance
	float					fadeVisibilityOver;		// extra range past maxVisibleRange before the hard cull
	highlightColor_t		highlightColor;
	idVec3					localLightPositionOverride;	// local space lighting sample point, 0 = bounds center
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
