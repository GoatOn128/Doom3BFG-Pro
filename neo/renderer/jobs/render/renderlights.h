#ifndef __JOBS_RENDER_RENDERLIGHTS_H__
#define __JOBS_RENDER_RENDERLIGHTS_H__

void R_ShadowBounds( const idBounds & modelBounds, const idBounds & lightBounds, const idVec3 & lightOrigin, idBounds & shadowBounds );

ID_INLINE bool R_CullModelBoundsToLight( const idRenderLightCommitted * light, const idBounds & localBounds, const idRenderMatrix & modelRenderMatrix ) {
	idRenderMatrix modelLightProject;
	idRenderMatrix::Multiply( light->baseLightProject, modelRenderMatrix, modelLightProject );
	return idRenderMatrix::CullBoundsToMVP( modelLightProject, localBounds, true );
}

void R_AddLights();
void R_OptimizeViewLightsList();

#endif
