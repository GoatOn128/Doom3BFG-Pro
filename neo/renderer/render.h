#ifndef __RENDER_H__
#define __RENDER_H__

class idRenderWorldCommitted;
class idRenderWorldLocal;
struct viewDef_t;
struct renderView_s;

// Renderer-side consumer of committed worlds and views. World publication and
// post-commit fixups are completed here before any render jobs inspect them.
class idRender {
public:
	void RenderScene( idRenderWorldLocal * renderWorld, idRenderWorldCommitted * committedWorld, const renderView_s * renderView );
	void RenderSingleView( idRenderWorldCommitted * renderWorld, viewDef_t * renderView );
	void RenderPostProcess( viewDef_t * renderView );
};

extern idRender render;

#endif
