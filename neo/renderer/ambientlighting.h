#ifndef __RENDERER_AMBIENTLIGHTING_H__
#define __RENDERER_AMBIENTLIGHTING_H__

/*
================================================================================================

Tech5 style ambient lighting.

Loads the .ambient file for the current map, an octree of ambient color
samples produced by an offline compiler, and exposes it to the approximate
lighting code. The offline compiler that produces the file is not part of
the engine, see TODO.md.

================================================================================================
*/

#include "jobs/approximatelighting/ambientmapcpu.h"

class idRenderWorld;

class idAmbientLighting {
public:
	idAmbientLighting();
	~idAmbientLighting();

	void					LoadAmbientLighting( const char * mapName );
	void					DebugDrawAmbientLightingSamples( idRenderWorld * renderWorld, const idVec3 & position ) const;
	void					FreeData();

	ambientMapCPU_t *		ambientMap;
};

idAmbientLighting & R_GetAmbientLighting();

#endif // __RENDERER_AMBIENTLIGHTING_H__
