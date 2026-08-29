/*
===========================================================================

Doom 3 BFG Edition GPL Source Code
Copyright (C) 1993-2012 id Software LLC, a ZeniMax Media company. 

This file is part of the Doom 3 BFG Edition GPL Source Code ("Doom 3 BFG Edition Source Code").  

Doom 3 BFG Edition Source Code is free software: you can redistribute it and/or modify
it under the terms of the GNU General Public License as published by
the Free Software Foundation, either version 3 of the License, or
(at your option) any later version.

Doom 3 BFG Edition Source Code is distributed in the hope that it will be useful,
but WITHOUT ANY WARRANTY; without even the implied warranty of
MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
GNU General Public License for more details.

You should have received a copy of the GNU General Public License
along with Doom 3 BFG Edition Source Code.  If not, see <http://www.gnu.org/licenses/>.

In addition, the Doom 3 BFG Edition Source Code is also subject to certain additional terms. You should have received a copy of these additional terms immediately following the terms and conditions of the GNU General Public License which accompanied the Doom 3 BFG Edition Source Code.  If not, please request a copy in writing from id Software at the address below.

If you have questions concerning this license or the applicable additional terms, you may contact in writing id Software LLC, c/o ZeniMax Media Inc., Suite 120, Rockville, Maryland 20850 USA.

===========================================================================
*/

#ifndef __RENDERWORLD_H__
#define __RENDERWORLD_H__

/*
===============================================================================

	Render World

===============================================================================
*/

#define PROC_FILE_EXT				"proc"
#define	PROC_FILE_ID				"mapProcFile003"

// shader parms
const int SHADERPARM_RED			= 0;
const int SHADERPARM_GREEN			= 1;
const int SHADERPARM_BLUE			= 2;
const int SHADERPARM_ALPHA			= 3;
const int SHADERPARM_TIMESCALE		= 3;
const int SHADERPARM_TIMEOFFSET		= 4;
const int SHADERPARM_DIVERSITY		= 5;	// random between 0.0 and 1.0 for some effects (muzzle flashes, etc)
const int SHADERPARM_MODE			= 7;	// for selecting which shader passes to enable
const int SHADERPARM_TIME_OF_DEATH	= 7;	// for the monster skin-burn-away effect enable and time offset

// model parms
const int SHADERPARM_MD3_FRAME		= 8;
const int SHADERPARM_MD3_LASTFRAME	= 9;
const int SHADERPARM_MD3_BACKLERP	= 10;

const int SHADERPARM_BEAM_END_X		= 8;	// for _beam models
const int SHADERPARM_BEAM_END_Y		= 9;
const int SHADERPARM_BEAM_END_Z		= 10;
const int SHADERPARM_BEAM_WIDTH		= 11;

const int SHADERPARM_SPRITE_WIDTH		= 8;
const int SHADERPARM_SPRITE_HEIGHT		= 9;

const int SHADERPARM_PARTICLE_STOPTIME = 8;	// don't spawn any more particles after this time

// guis
const int MAX_RENDERENTITY_GUI		= 3;

// the renderEntity_s::joints array needs to point at enough memory to store the number of joints rounded up to two for SIMD
ID_INLINE int SIMD_ROUND_JOINTS( int numJoints )							{ return ( ( numJoints + 1 ) & ~1 ); }
ID_INLINE void SIMD_INIT_LAST_JOINT( idJointMat * joints, int numJoints )	{ if ( numJoints & 1 ) { joints[numJoints] = joints[numJoints - 1]; } }

#include "RenderEntity.h"
#include "RenderLight.h"

class idRenderWorldCommitted;



typedef struct renderView_s {
	// player views will set this to a non-zero integer for model suppress / allow
	// subviews (mirrors, cameras, etc) will always clear it to zero
	int						viewID;

	float					fov_x, fov_y;		// in degrees
	idVec3					vieworg;			// has already been adjusted for stereo world seperation
	idVec3					vieworg_weapon;		// has already been adjusted for stereo world seperation
	idMat3					viewaxis;			// transformation matrix, view looks down the positive X axis

	bool					cramZNear;			// for cinematics, we want to set ZNear much lower
	bool					flipProjection;
	bool					forceUpdate;		// for an update 

	// time in milliseconds for shader effects and other time dependent rendering issues
	int						time[2];
	float					shaderParms[MAX_GLOBAL_SHADER_PARMS];		// can be used in any way by shader
	const idMaterial		*globalMaterial;							// used to override everything draw

	// the viewEyeBuffer may be of a different polarity than stereoScreenSeparation if the eyes have been swapped
	int						viewEyeBuffer;				// -1 = left eye, 1 = right eye, 0 = monoscopic view or GUI
	float					stereoScreenSeparation;		// projection matrix horizontal offset, positive or negative based on camera eye

} renderView_t;


// exitPortal_t is returned by idRenderWorld::GetPortal()
typedef struct {
	int					areas[2];		// areas connected by this portal
	const idWinding	*	w;				// winding points have counter clockwise ordering seen from areas[0]
	int					blockingBits;	// PS_BLOCK_VIEW, PS_BLOCK_AIR, etc
	qhandle_t			portalHandle;
} exitPortal_t;


// guiPoint_t is returned by idRenderWorld::GuiTrace()
typedef struct {
	float				x, y;			// 0.0 to 1.0 range if trace hit a gui, otherwise -1
	int					guiId;			// id of gui ( 0, 1, or 2 ) that the trace happened against
} guiPoint_t;


// modelTrace_t is for tracing vs. visual geometry
typedef struct modelTrace_s {
	float					fraction;			// fraction of trace completed
	idVec3					point;				// end point of trace in global space
	idVec3					normal;				// hit triangle normal vector in global space
	const idMaterial *		material;			// material of hit surface
	const renderEntity_t *	entity;				// render entity that was hit
	int						jointNumber;		// md5 joint nearest to the hit triangle
} modelTrace_t;


static const int NUM_PORTAL_ATTRIBUTES = 3;

typedef enum {
	PS_BLOCK_NONE = 0,

	PS_BLOCK_VIEW = 1,
	PS_BLOCK_LOCATION = 2,		// game map location strings often stop in hallways
	PS_BLOCK_AIR = 4,			// windows between pressurized and unpresurized areas

	PS_BLOCK_ALL = (1<<NUM_PORTAL_ATTRIBUTES)-1
} portalConnection_t;


class idRenderWorld {
public:
	virtual					~idRenderWorld() {};

	// The same render world can be reinitialized as often as desired
	// a NULL or empty mapName will create an empty, single area world
	virtual bool			InitFromMap( const char *mapName ) = 0;

	// This fixes a crash when switching between expansion packs in the same game session
	// the modelManager gets reset, which deletes all render models without resetting the localModels list inside renderWorldLocal.
	// Now we'll have a hook to reset the list from here.
	virtual void			ResetLocalRenderModels() = 0;

	//-------------- Entity and Light Defs -----------------

	// entityDefs and lightDefs are added to a given world to determine
	// what will be drawn for a rendered scene.  Most update work is defered
	// until it is determined that it is actually needed for a given view.
	virtual	qhandle_t		AddEntityDef( const renderEntity_t *re ) = 0;
	virtual	void			UpdateEntityDef( qhandle_t entityHandle, const renderEntity_t *re ) = 0;
	virtual	void			FreeEntityDef( qhandle_t entityHandle ) = 0;
	virtual const renderEntity_t *GetRenderEntity( qhandle_t entityHandle ) const = 0;
	virtual qhandle_t		AddRenderEntity( idRenderEntity *entity ) = 0;
	virtual void			QueueRenderEntity( idRenderEntity *entity ) = 0;

	virtual	qhandle_t		AddLightDef( const renderLight_t *rlight ) = 0;
	virtual	void			UpdateLightDef( qhandle_t lightHandle, const renderLight_t *rlight ) = 0;
	virtual	void			FreeLightDef( qhandle_t lightHandle ) = 0;
	virtual const renderLight_t *GetRenderLight( qhandle_t lightHandle ) const = 0;
	virtual qhandle_t		AddRenderLight( idRenderLight *light ) = 0;

	// Force the generation of all light / surface interactions at the start of a level
	// If this isn't called, they will all be dynamically generated

	// returns true if this area model needs portal sky to draw
	virtual bool			CheckAreaForPortalSky( int areaNum ) = 0;

	//-------------- Decals and Overlays  -----------------

	// Creates decals on all world surfaces that the winding projects onto.
	// The projection origin should be infront of the winding plane.
	// The decals are projected onto world geometry between the winding plane and the projection origin.
	// The decals are depth faded from the winding plane to a certain distance infront of the
	// winding plane and the same distance from the projection origin towards the winding.
	virtual void			ProjectDecalOntoWorld( const idFixedWinding &winding, const idVec3 &projectionOrigin, const bool parallel, const float fadeDepth, const idMaterial *material, const int startTime ) = 0;

	// Creates decals on static models.
	virtual void			ProjectDecal( qhandle_t entityHandle, const idFixedWinding &winding, const idVec3 &projectionOrigin, const bool parallel, const float fadeDepth, const idMaterial *material, const int startTime ) = 0;

	// Creates overlays on dynamic models.
	virtual void			ProjectOverlay( qhandle_t entityHandle, const idPlane localTextureAxis[2], const idMaterial *material, const int startTime ) = 0;

	// Removes all decals and overlays from the given entity def.
	virtual void			RemoveDecals( qhandle_t entityHandle ) = 0;

	//-------------- Scene Rendering -----------------

	// some calls to material functions use the current renderview time when servicing cinematics.  this function
	// ensures that any parms accessed (such as time) are properly set.
	virtual void			SetRenderView( const renderView_t *renderView ) = 0;

	// rendering a scene may actually render multiple subviews for mirrors and portals, and
	// may render composite textures for gui console screens and light projections
	// It would also be acceptable to render a scene multiple times, for "rear view mirrors", etc
	virtual void			RenderScene( const renderView_t *renderView ) = 0;
	virtual void			Commit( bool isHeadless = false ) = 0;
	virtual idRenderWorldCommitted * GetCommittedWorld() const = 0;

	//-------------- Portal Area Information -----------------

	// returns the number of portals
	virtual int				NumPortals() const = 0;

	// returns 0 if no portal contacts the bounds
	// This is used by the game to identify portals that are contained
	// inside doors, so the connection between areas can be topologically
	// terminated when the door shuts.
	virtual	qhandle_t		FindPortal( const idBounds &b ) const = 0;

	// doors explicitly close off portals when shut
	// multiple bits can be set to block multiple things, ie: ( PS_VIEW | PS_LOCATION | PS_AIR )
	virtual	void			SetPortalState( qhandle_t portal, int blockingBits ) = 0;
	virtual int				GetPortalState( qhandle_t portal ) = 0;

	// returns true only if a chain of portals without the given connection bits set
	// exists between the two areas (a door doesn't separate them, etc)
	virtual	bool			AreasAreConnected( int areaNum1, int areaNum2, portalConnection_t connection ) const = 0;

	// returns the number of portal areas in a map, so game code can build information
	// tables for the different areas
	virtual	int				NumAreas() const = 0;

	// Will return -1 if the point is not in an area, otherwise
	// it will return 0 <= value < NumAreas()
	virtual int				PointInArea( const idVec3 &point ) const = 0;

	// fills the *areas array with the numbers of the areas the bounds cover
	// returns the total number of areas the bounds cover
	virtual int				BoundsInAreas( const idBounds &bounds, int *areas, int maxAreas ) const = 0;

	// Used by the sound system to do area flowing
	virtual	int				NumPortalsInArea( int areaNum ) = 0;

	// returns one portal from an area
	virtual exitPortal_t	GetPortal( int areaNum, int portalNum ) = 0;

	//-------------- Tracing  -----------------

	// Checks a ray trace against any gui surfaces in an entity, returning the
	// fraction location of the trace on the gui surface, or -1,-1 if no hit.
	// This doesn't do any occlusion testing, simply ignoring non-gui surfaces.
	// start / end are in global world coordinates.
	virtual guiPoint_t		GuiTrace( qhandle_t entityHandle, const idVec3 start, const idVec3 end ) const = 0;

	// Traces vs the render model, possibly instantiating a dynamic version, and returns true if something was hit
	virtual bool			ModelTrace( modelTrace_t &trace, qhandle_t entityHandle, const idVec3 &start, const idVec3 &end, const float radius ) const = 0;

	// Traces vs the whole rendered world. FIXME: we need some kind of material flags.
	virtual bool			Trace( modelTrace_t &trace, const idVec3 &start, const idVec3 &end, const float radius, bool skipDynamic = true, bool skipPlayer = false ) const = 0;

	// Traces vs the world model bsp tree.
	virtual bool			FastWorldTrace( modelTrace_t &trace, const idVec3 &start, const idVec3 &end ) const = 0;

	//-------------- Demo Control  -----------------

	// Writes a loadmap command to the demo, and clears archive counters.
	virtual void			StartWritingDemo( idDemoFile *demo ) = 0;
	virtual void			StopWritingDemo() = 0;

	// Returns true when demoRenderView has been filled in.
	// adds/updates/frees entityDefs and lightDefs based on the current demo file
	// and returns the renderView to be used to render this frame.
	// a demo file may need to be advanced multiple times if the framerate
	// is less than 30hz
	// demoTimeOffset will be set if a new map load command was processed before
	// the next renderScene
	virtual bool			ProcessDemoCommand( idDemoFile *readDemo, renderView_t *demoRenderView, int *demoTimeOffset ) = 0;

	// this is used to regenerate all interactions ( which is currently only done during influences ), there may be a less 
	// expensive way to do it
	virtual void			RegenerateWorld() = 0;

	//-------------- Debug Visualization  -----------------

	// Line drawing for debug visualization
	virtual void			DebugClearLines( int time ) = 0;		// a time of 0 will clear all lines and text
	virtual void			DebugLine( const idVec4 &color, const idVec3 &start, const idVec3 &end, const int lifetime = 0, const bool depthTest = false ) = 0;
	virtual void			DebugArrow( const idVec4 &color, const idVec3 &start, const idVec3 &end, int size, const int lifetime = 0 ) = 0;
	virtual void			DebugWinding( const idVec4 &color, const idWinding &w, const idVec3 &origin, const idMat3 &axis, const int lifetime = 0, const bool depthTest = false ) = 0;
	virtual void			DebugCircle( const idVec4 &color, const idVec3 &origin, const idVec3 &dir, const float radius, const int numSteps, const int lifetime = 0, const bool depthTest = false ) = 0;
	virtual void			DebugSphere( const idVec4 &color, const idSphere &sphere, const int lifetime = 0, bool depthTest = false ) = 0;
	virtual void			DebugBounds( const idVec4 &color, const idBounds &bounds, const idVec3 &org = vec3_origin, const int lifetime = 0 ) = 0;
	virtual void			DebugBox( const idVec4 &color, const idBox &box, const int lifetime = 0 ) = 0;
	virtual void			DebugCone( const idVec4 &color, const idVec3 &apex, const idVec3 &dir, float radius1, float radius2, const int lifetime = 0 ) = 0;
	virtual void			DebugAxis( const idVec3 &origin, const idMat3 &axis ) = 0;

	// Polygon drawing for debug visualization.
	virtual void			DebugClearPolygons( int time ) = 0;		// a time of 0 will clear all polygons
	virtual void			DebugPolygon( const idVec4 &color, const idWinding &winding, const int lifeTime = 0, const bool depthTest = false ) = 0;

	// Text drawing for debug visualization.
	virtual void			DrawText( const char *text, const idVec3 &origin, float scale, const idVec4 &color, const idMat3 &viewAxis, const int align = 1, const int lifetime = 0, bool depthTest = false ) = 0;
};

#endif /* !__RENDERWORLD_H__ */
