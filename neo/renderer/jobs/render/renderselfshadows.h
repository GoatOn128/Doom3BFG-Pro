#ifndef __JOBS_RENDER_RENDERSELFSHADOWS_H__
#define __JOBS_RENDER_RENDERSELFSHADOWS_H__

/*
================================================================================================

Tech5 style per model self shadows.

Renders a four tile shadow atlas per model from the approximate lighting
light slots and multiplies the model's own surfaces by the filtered shadow
result. Re-implemented on idlib from the Tech5 semantics, nothing was
copied from the original code.

================================================================================================
*/

void RB_RenderSelfShadows();
void RB_ShutdownSelfShadows();

#endif // __JOBS_RENDER_RENDERSELFSHADOWS_H__
