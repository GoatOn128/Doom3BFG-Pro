#include "../idlib/precompiled.h"
#pragma hdrstop

#include "tr_local.h"
#include "renderdestination.h"

idRenderDestination * renderDestScaledLinear = NULL;
idRenderDestination * renderDestScaledColorWithFeedback = NULL;
idRenderDestination * renderDestScaledFeedback = NULL;
idRenderDestination * renderDestScaledDepth = NULL;
idRenderDestination * renderDestScaledColor2 = NULL;
idRenderDestination * renderDestScaledColor = NULL;
idRenderDestination * renderDestNative = NULL;

idRenderDestination::idRenderDestination()
	: isDefault( false ), targetWidth( -1 ), targetHeight( -1 ),
	  depthImage( NULL ), stencilImage( NULL ), FBOhandle( 0 ),
	  boundSide( -1 ), boundLevel( -1 ) {
	memset( targetImage, 0, sizeof( targetImage ) );
}

idRenderDestination::idRenderDestination( const char * name, int width,
		int height, renderClass_t textureClass ) : idRenderDestination() {
	Create( name, width, height, textureClass );
}

idRenderDestination::~idRenderDestination() {
	if ( FBOhandle != 0 ) {
		glDeleteFramebuffers( 1, &FBOhandle );
		FBOhandle = 0;
	}
}

void idRenderDestination::Resize( int width, int height ) {
	targetWidth = width;
	targetHeight = height;
	for ( int i = 0; i < 4; ++i ) {
		if ( targetImage[i] != NULL ) { targetImage[i]->Resize( width, height ); }
	}
	if ( depthImage != NULL ) { depthImage->Resize( width, height ); }
	if ( stencilImage != NULL ) { stencilImage->Resize( width, height ); }
	boundSide = boundLevel = -1;
}

void idRenderDestination::CreateFromImages( idImage * color, idImage * depth,
		idImage * stencil ) {
	if ( color == NULL && depth == NULL && stencil == NULL ) {
		idLib::Error( "idRenderDestination::CreateFromImages: no images" );
	}
	idImage * dimensions = color != NULL ? color : ( depth != NULL ? depth : stencil );
	targetWidth = dimensions->GetUploadWidth();
	targetHeight = dimensions->GetUploadHeight();
	targetImage[0] = color;
	depthImage = depth;
	stencilImage = stencil;
	boundSide = boundLevel = 0;

	if ( FBOhandle != 0 ) {
		glDeleteFramebuffers( 1, &FBOhandle );
	}
	GLint previousFBO = 0;
	glGetIntegerv( GL_FRAMEBUFFER_BINDING, &previousFBO );
	glGenFramebuffers( 1, &FBOhandle );
	glBindFramebuffer( GL_FRAMEBUFFER, FBOhandle );
	if ( color != NULL ) {
		glFramebufferTexture2D( GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0,
			GL_TEXTURE_2D, color->GetAPIObject(), 0 );
	} else {
		glDrawBuffer( GL_NONE );
		glReadBuffer( GL_NONE );
	}
	if ( depth != NULL ) {
		glFramebufferTexture2D( GL_FRAMEBUFFER, GL_DEPTH_ATTACHMENT,
			GL_TEXTURE_2D, depth->GetAPIObject(), 0 );
	}
	if ( stencil != NULL ) {
		glFramebufferTexture2D( GL_FRAMEBUFFER, GL_STENCIL_ATTACHMENT,
			GL_TEXTURE_2D, stencil->GetAPIObject(), 0 );
	}
	const GLenum status = glCheckFramebufferStatus( GL_FRAMEBUFFER );
	glBindFramebuffer( GL_FRAMEBUFFER, previousFBO );
	if ( status != GL_FRAMEBUFFER_COMPLETE ) {
		idLib::Warning( "render destination framebuffer incomplete: 0x%x", status );
	}
}

void idRenderDestination::AddTarget( idImage * target ) {
	int index = 0;
	while ( index < 4 && targetImage[index] != NULL ) { ++index; }
	if ( index >= 4 || target == NULL || FBOhandle == 0 ) { return; }
	targetImage[index] = target;
	GLint previousFBO = 0;
	glGetIntegerv( GL_FRAMEBUFFER_BINDING, &previousFBO );
	glBindFramebuffer( GL_FRAMEBUFFER, FBOhandle );
	glFramebufferTexture2D( GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0 + index,
		GL_TEXTURE_2D, target->GetAPIObject(), 0 );
	GLenum buffers[4] = { GL_COLOR_ATTACHMENT0, GL_COLOR_ATTACHMENT1,
		GL_COLOR_ATTACHMENT2, GL_COLOR_ATTACHMENT3 };
	glDrawBuffers( index + 1, buffers );
	glBindFramebuffer( GL_FRAMEBUFFER, previousFBO );
}

void idRenderDestination::Create( const char * name, int width, int height,
		renderClass_t textureClass ) {
	idImageOpts opts;
	opts.width = width;
	opts.height = height;
	opts.numLevels = 1;
	idImage * color = NULL;
	idImage * depth = NULL;
	if ( textureClass == TC_DEPTH || textureClass == TC_RGBA_DEPTH ||
			textureClass == TC_FLOAT_RGBA_DEPTH || textureClass == TC_FLOAT16_RGBA_DEPTH ) {
		opts.format = FMT_DEPTH;
		depth = globalImages->ScratchImage( va( "_%s_depth", name ), &opts,
			TF_NEAREST, TR_CLAMP, TD_DEPTH );
	}
	if ( textureClass != TC_DEPTH ) {
		opts.format = ( textureClass == TC_FLOAT_RGBA || textureClass == TC_FLOAT_RGBA_DEPTH )
			? FMT_RGBA32F : ( textureClass == TC_FLOAT16_RGBA || textureClass == TC_FLOAT16_RGBA_DEPTH )
			? FMT_RGBA16F : FMT_RGBA8;
		color = globalImages->ScratchImage( va( "_%s", name ), &opts,
			textureClass == TC_RGBA || textureClass == TC_RGBA_DEPTH ? TF_LINEAR : TF_NEAREST,
			TR_CLAMP, TD_LOOKUP_TABLE_RGBA );
	}
	CreateFromImages( color, depth, NULL );
}
