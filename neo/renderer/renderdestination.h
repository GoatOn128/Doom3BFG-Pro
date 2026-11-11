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

#pragma once

class idImage;

enum renderClass_t {
	TC_DEPTH = 0,
	TC_RGBA = 1,
	TC_RGBA_DEPTH = 2,
	TC_FLOAT_RGBA = 3,
	TC_FLOAT_RGBA_DEPTH = 4,
	TC_FLOAT16_RGBA = 5,
	TC_FLOAT16_RGBA_DEPTH = 6,
	TC_NUM_RENDER_CLASS = 7
};

enum resolveTarget_t {
	RESOLVE_TARGET_COLOR0 = 0,
	RESOLVE_TARGET_COLOR1 = 1,
	RESOLVE_TARGET_COLOR2 = 2,
	RESOLVE_TARGET_COLOR3 = 3,
	RESOLVE_TARGET_DEPTH = 4
};

class idRenderDestination {
public:
					idRenderDestination();
					idRenderDestination( const char * name, int width, int height,
						renderClass_t textureClass );
					~idRenderDestination();

	void			SetDefault() { isDefault = true; }
	bool			IsDefault() const { return isDefault; }
	bool			IsValid() const { return isDefault || FBOhandle != 0; }
	int				GetTargetWidth() const { return targetWidth; }
	int				GetTargetHeight() const { return targetHeight; }
	idImage *		GetColorImage() const { return targetImage[0]; }
	idImage *		GetTargetImage( int index ) const { return index >= 0 && index < 4 ? targetImage[index] : NULL; }
	idImage *		GetDepthImage() const { return depthImage; }

	void			Create( const char * name, int width, int height, renderClass_t textureClass );
	void			CreateFromImages( idImage * color, idImage * depth, idImage * stencil );
	void			AddTarget( idImage * target );
	void			Resize( int width, int height );

	bool			isDefault;
	int				targetWidth;
	int				targetHeight;
	idImage *		targetImage[4];
	idImage *		depthImage;
	idImage *		stencilImage;
	// GL-free header: plain unsigned int instead of GLuint
	unsigned int	FBOhandle;
	int				boundSide;
	int				boundLevel;
};

extern idRenderDestination * renderDestScaledLinear;
extern idRenderDestination * renderDestScaledColorWithFeedback;
extern idRenderDestination * renderDestScaledFeedback;
extern idRenderDestination * renderDestScaledDepth;
extern idRenderDestination * renderDestScaledColor2;
extern idRenderDestination * renderDestScaledColor;
extern idRenderDestination * renderDestNative;

// Global render destination lifecycle. Created with the GL context in
// R_InitOpenGL, destroyed before the context goes away.
void			R_CreateRenderDestinations();
void			R_DestroyRenderDestinations();
