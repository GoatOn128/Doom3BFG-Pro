#ifndef __JOBS_RENDER_RENDERGATHER_H__
#define __JOBS_RENDER_RENDERGATHER_H__

bool R_IssueEntityDefCallback( idRenderEntityCommitted * def );
idRenderModel * R_EntityDefDynamicModel( idRenderEntityCommitted * def );
void R_ClearEntityDefDynamicModel( idRenderEntityCommitted * def );

void R_SetupDrawSurfShader( drawSurf_t * drawSurf, const idMaterial * shader, const renderEntity_t * renderEntity );
void R_SetupDrawSurfJoints( drawSurf_t * drawSurf, const srfTriangles_t * tri, const idMaterial * shader );
void R_LinkDrawSurfToView( drawSurf_t * drawSurf, viewDef_t * viewDef );

void R_AddModels();

#endif
