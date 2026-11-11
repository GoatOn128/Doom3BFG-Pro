#ifndef __RENDERER_GLOBALSHADOWS_H__
#define __RENDERER_GLOBALSHADOWS_H__

/*
================================================================================================

Tech5 style global shadows.

Loads the offline rendered global shadow depth map for the current map and
exposes it to the approximate lighting code. The offline compiler that
produces the file is not part of the engine.

================================================================================================
*/

#include "jobs/approximatelighting/shadowmapcpu.h"

class idGlobalShadows {
public:
	idGlobalShadows();
	~idGlobalShadows();

	void					LoadGlobalShadows( const char * mapName );
	const shadowMapCPU_t *	GetShadowMap();

	shadowMapCPU_t *		shadowMap;
};

idGlobalShadows & R_GetGlobalShadows();

extern idCVar r_globalShadowsCoordinateBias;
extern idCVar r_globalShadowsDepthBias;
extern idCVar r_useGlobalShadows;

#endif // __RENDERER_GLOBALSHADOWS_H__
