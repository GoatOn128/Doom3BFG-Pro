/*
===========================================================================

Darklight OpenGL 3.3 core loader state.

The glad context is filled in once per context creation by GLcore_Load,
which win_glimp calls right after making the context current.

===========================================================================
*/
#include "../../idlib/precompiled.h"
#pragma hdrstop

#include "glcore.h"

GladGLContext rgl;

bool GLcore_Load() {
	memset( &rgl, 0, sizeof( rgl ) );
	// glad's loader opens opengl32.dll and resolves through wglGetProcAddress
	if ( gladLoaderLoadGLContext( &rgl ) == 0 ) {
		return false;
	}
	GLim_Init();
	return true;
}

void GLcore_Unload() {
	GLim_Shutdown();
	gladLoaderUnloadGLContext( &rgl );
}
