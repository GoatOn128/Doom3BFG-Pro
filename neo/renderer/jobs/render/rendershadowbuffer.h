#ifndef __JOBS_RENDER_RENDERSHADOWBUFFER_H__
#define __JOBS_RENDER_RENDERSHADOWBUFFER_H__

// Shared shadow atlas state. The interaction pass reads the matrices and the
// atlas size to project surfaces into the depth map while sampling it.
extern idRenderMatrix	currentShadowMatrix;
extern idRenderMatrix	currentShadowProjection;
extern float			currentShadowType;
extern int				lightShadowAtlasSize;

void RB_ShutdownShadowAtlas();
bool RB_EnsureShadowAtlas();
bool RB_RenderShadowMap( const viewLight_t * vLight );

#endif
