successfully downloaded text file (SHA: e4b7a37a6cbb1dfe4beba26a8f060ec3e2eb92d4)#ifndef __JOBS_RENDER_RENDERPASSES_H__
#define __JOBS_RENDER_RENDERPASSES_H__

void RB_ShutdownShadowAtlas();
void RB_RenderDimShadows();
void RB_DrawElementsWithCounters( const drawSurf_t * surf );
void RB_DrawViewInternal( const viewDef_t * viewDef, const int stereoEye );
void RB_DrawView( const void * data, const int stereoEye );
void RB_CopyRender( const void * data );
void RB_PostProcess( const void * data );

#endif
