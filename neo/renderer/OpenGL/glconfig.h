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

#ifndef GLCONFIG_H
#define GLCONFIG_H

// GL configuration record, moved here from RenderSystem.h to follow the
// WolfTNO backend layout where the GL layer owns its own config header.
// Self contained: no GL headers, plain C++ types only.

enum stereo3DMode_t {
	STEREO3D_OFF,

	// half-resolution, non-square pixel views
	STEREO3D_SIDE_BY_SIDE_COMPRESSED,
	STEREO3D_TOP_AND_BOTTOM_COMPRESSED,

	// two full resolution views side by side, as for a dual cable display
	STEREO3D_SIDE_BY_SIDE,

	STEREO3D_INTERLACED,

	// OpenGL quad buffer
	STEREO3D_QUAD_BUFFER,

	// two full resolution views stacked with a 30 pixel guard band
	// On the PC this can be configured as a custom video timing, but
	// it definitely isn't a consumer level task.  The quad_buffer
	// support can handle 720P-3D with apropriate driver support.
	STEREO3D_HDMI_720
};

enum graphicsVendor_t {
	VENDOR_NVIDIA,
	VENDOR_AMD,
	VENDOR_INTEL
};

// Contains variables specific to the OpenGL configuration being run right now.
// These are constant once the OpenGL subsystem is initialized.
struct glconfig_t {
	const char *		renderer_string;
	const char *		vendor_string;
	const char *		version_string;
	const char *		extensions_string;
	const char *		wgl_extensions_string;
	const char *		shading_language_string;

	float				glVersion;				// atof( version_string )
	graphicsVendor_t	vendor;

	int					maxTextureSize;			// queried from GL
	int					maxTextureCoords;
	int					maxTextureImageUnits;
	int					uniformBufferOffsetAlignment;
	float				maxTextureAnisotropy;

	int					colorBits;
	int					depthBits;
	int					stencilBits;

	bool				multitextureAvailable;
	bool				directStateAccess;
	bool				textureCompressionAvailable;
	bool				anisotropicFilterAvailable;
	bool				textureLODBiasAvailable;
	bool				seamlessCubeMapAvailable;
	bool				sRGBFramebufferAvailable;
	bool				vertexBufferObjectAvailable;
	bool				mapBufferRangeAvailable;
	bool				vertexArrayObjectAvailable;
	bool				drawElementsBaseVertexAvailable;
	bool				fragmentProgramAvailable;
	bool				glslAvailable;
	bool				uniformBufferAvailable;
	bool				twoSidedStencilAvailable;
	bool				depthBoundsTestAvailable;
	bool				syncAvailable;
	bool				timerQueryAvailable;
	bool				occlusionQueryAvailable;
	bool				debugOutputAvailable;
	bool				swapControlTearAvailable;

	stereo3DMode_t		stereo3Dmode;
	int					nativeScreenWidth; // this is the native screen width resolution of the renderer
	int					nativeScreenHeight; // this is the native screen height resolution of the renderer

	int					displayFrequency;

	int					isFullscreen;					// monitor number
	bool				isStereoPixelFormat;
	bool				stereoPixelFormatAvailable;
	int					multisamples;

	// Screen separation for stereoscopic rendering is set based on this.
	// PC vid code sets this, converting from diagonals / inches / whatever as needed.
	// If the value can't be determined, set something reasonable, like 50cm.
	float				physicalScreenWidthInCentimeters;

	float				pixelAspect;

	// GLuint in the original record; kept as a plain unsigned int so this
	// header stays free of GL includes (GLuint is unsigned int everywhere).
	unsigned int		global_vao;
};

extern glconfig_t		glConfig;		// outside of TR since it shouldn't be cleared during ref re-init

#endif // GLCONFIG_H
