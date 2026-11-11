/*
===========================================================================

Doom 3 BFG Edition GPL Source Code
Copyright (C) 1993-2012 id Software LLC, a ZeniMax Media company.

This file is part of the Doom 3 BFG Edition GPL Source Code ("Doom 3 BFG Edition Source Code").

Doom 3 BFG Edition Source Code is free software: you can redistribute it and/or modify
it under the terms of the GNU General Public License as published by
the Free Software Foundation, either version 3 of the License, or
(at your option) any later version.

Doom 3 BFG Edition Source Code is distributed in the hope that it will be useful,
but WITHOUT ANY WARRANTY; without even the implied warranty of
MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
GNU General Public License for more details.

You should have received a copy of the GNU General Public License
along with Doom 3 BFG Edition Source Code.  If not, see <http://www.gnu.org/licenses/>.

In addition, the Doom 3 BFG Edition Source Code is also subject to certain additional terms. You should have received a copy of these additional terms immediately following the terms and conditions of the GNU General Public License which accompanied the Doom 3 BFG Edition Source Code.  If not, please request a copy in writing from id Software at the address below.

If you have questions concerning this license or the applicable additional terms, you may contact in writing id Software LLC, c/o ZeniMax Media Inc., Suite 120, Rockville, Maryland 20850 USA.

===========================================================================
*/

#include "../../idlib/precompiled.h"
#pragma hdrstop

#include "../tr_local.h"
#include "../renderdestination.h"

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
	GL_CheckErrors();
	if ( color == NULL && depth == NULL && stencil == NULL ) {
		idLib::Error( "idRenderDestination::CreateFromImages: no images" );
	}
	// Dimensions come from the color image first, then depth.
	idImage * dimensions = color != NULL ? color : ( depth != NULL ? depth : stencil );
	targetWidth = dimensions->GetUploadWidth();
	targetHeight = dimensions->GetUploadHeight();
	targetImage[0] = color;
	targetImage[1] = NULL;
	targetImage[2] = NULL;
	targetImage[3] = NULL;
	depthImage = depth;
	stencilImage = stencil;
	boundSide = boundLevel = 0;

	if ( FBOhandle != 0 ) {
		glDeleteFramebuffers( 1, &FBOhandle );
		FBOhandle = 0;
	}
	GLint previousFBO = 0;
	glGetIntegerv( GL_FRAMEBUFFER_BINDING, &previousFBO );
	glGenFramebuffers( 1, &FBOhandle );
	glBindFramebuffer( GL_FRAMEBUFFER, FBOhandle );
	if ( color != NULL ) {
		// Cube maps attach face 0, everything else is a plain 2D texture.
		const GLenum colorTarget = ( color->GetOpts().textureType == TT_CUBIC )
			? GL_TEXTURE_CUBE_MAP_POSITIVE_X : GL_TEXTURE_2D;
		glFramebufferTexture2D( GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0,
			colorTarget, color->GetAPIObject(), 0 );
	} else {
		// Attaching texture 0 detaches any stale color attachment.
		glFramebufferTexture2D( GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0,
			GL_TEXTURE_2D, 0, 0 );
		glDrawBuffer( GL_NONE );
		glReadBuffer( GL_NONE );
	}
	// Always touch both attachments so re-creation drops stale images.
	glFramebufferTexture2D( GL_FRAMEBUFFER, GL_DEPTH_ATTACHMENT,
		GL_TEXTURE_2D, depth != NULL ? depth->GetAPIObject() : 0, 0 );
	glFramebufferTexture2D( GL_FRAMEBUFFER, GL_STENCIL_ATTACHMENT,
		GL_TEXTURE_2D, stencil != NULL ? stencil->GetAPIObject() : 0, 0 );
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
	const GLenum status = glCheckFramebufferStatus( GL_FRAMEBUFFER );
	glBindFramebuffer( GL_FRAMEBUFFER, previousFBO );
	if ( status != GL_FRAMEBUFFER_COMPLETE ) {
		idLib::Warning( "render destination incomplete after AddTarget: 0x%x", status );
	}
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

/*
========================
R_CreateRenderDestinations

Creates the global render destinations. The engine currently renders into
the default framebuffer, so the scaled destinations are created as default
destinations; they become FBO backed once callers attach images to them.
========================
*/
void R_CreateRenderDestinations() {
	R_DestroyRenderDestinations();
	renderDestNative = NULL;
	renderDestScaledLinear = NULL;
	renderDestScaledColor = new ( TAG_RENDER ) idRenderDestination();
	renderDestScaledColor->SetDefault();
	renderDestScaledColor2 = new ( TAG_RENDER ) idRenderDestination();
	renderDestScaledColor2->SetDefault();
	renderDestScaledDepth = new ( TAG_RENDER ) idRenderDestination();
	renderDestScaledDepth->SetDefault();
	renderDestScaledFeedback = new ( TAG_RENDER ) idRenderDestination();
	renderDestScaledFeedback->SetDefault();
	renderDestScaledColorWithFeedback = new ( TAG_RENDER ) idRenderDestination();
	renderDestScaledColorWithFeedback->SetDefault();
}

/*
========================
R_DestroyRenderDestinations
========================
*/
void R_DestroyRenderDestinations() {
	idRenderDestination * dests[7] = { renderDestScaledLinear,
		renderDestScaledColorWithFeedback, renderDestScaledFeedback,
		renderDestScaledDepth, renderDestScaledColor2,
		renderDestScaledColor, renderDestNative };
	for ( int i = 0; i < 7; ++i ) {
		if ( dests[i] != NULL ) { delete dests[i]; }
	}
	renderDestScaledLinear = NULL;
	renderDestScaledColorWithFeedback = NULL;
	renderDestScaledFeedback = NULL;
	renderDestScaledDepth = NULL;
	renderDestScaledColor2 = NULL;
	renderDestScaledColor = NULL;
	renderDestNative = NULL;
}
