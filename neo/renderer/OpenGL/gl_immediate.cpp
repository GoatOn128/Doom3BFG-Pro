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

#pragma hdrstop
#include "../../idlib/precompiled.h"

#include "glcore.h"

#include <cstring>
#include <cstdlib>
#include <cmath>

// 8x8 bitmap font for glCallLists text, public domain font8x8 by
// Daniel Hepper / Marcel Sondaar.
static const unsigned char glimFont8x8[128][8] = {
	{ 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00 },
	{ 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00 },
	{ 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00 },
	{ 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00 },
	{ 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00 },
	{ 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00 },
	{ 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00 },
	{ 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00 },
	{ 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00 },
	{ 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00 },
	{ 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00 },
	{ 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00 },
	{ 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00 },
	{ 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00 },
	{ 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00 },
	{ 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00 },
	{ 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00 },
	{ 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00 },
	{ 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00 },
	{ 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00 },
	{ 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00 },
	{ 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00 },
	{ 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00 },
	{ 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00 },
	{ 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00 },
	{ 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00 },
	{ 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00 },
	{ 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00 },
	{ 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00 },
	{ 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00 },
	{ 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00 },
	{ 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00 },
	{ 0x18, 0x3C, 0x3C, 0x18, 0x18, 0x00, 0x18, 0x00 },
	{ 0x36, 0x36, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00 },
	{ 0x36, 0x36, 0x7F, 0x36, 0x7F, 0x36, 0x36, 0x00 },
	{ 0x0C, 0x3E, 0x03, 0x1E, 0x30, 0x1F, 0x0C, 0x00 },
	{ 0x00, 0x63, 0x33, 0x18, 0x0C, 0x66, 0x63, 0x00 },
	{ 0x1C, 0x36, 0x1C, 0x6E, 0x3B, 0x33, 0x6E, 0x00 },
	{ 0x06, 0x06, 0x03, 0x00, 0x00, 0x00, 0x00, 0x00 },
	{ 0x18, 0x0C, 0x06, 0x06, 0x06, 0x0C, 0x18, 0x00 },
	{ 0x06, 0x0C, 0x18, 0x18, 0x18, 0x0C, 0x06, 0x00 },
	{ 0x00, 0x66, 0x3C, 0xFF, 0x3C, 0x66, 0x00, 0x00 },
	{ 0x00, 0x0C, 0x0C, 0x3F, 0x0C, 0x0C, 0x00, 0x00 },
	{ 0x00, 0x00, 0x00, 0x00, 0x00, 0x0C, 0x0C, 0x06 },
	{ 0x00, 0x00, 0x00, 0x3F, 0x00, 0x00, 0x00, 0x00 },
	{ 0x00, 0x00, 0x00, 0x00, 0x00, 0x0C, 0x0C, 0x00 },
	{ 0x60, 0x30, 0x18, 0x0C, 0x06, 0x03, 0x01, 0x00 },
	{ 0x3E, 0x63, 0x73, 0x7B, 0x6F, 0x67, 0x3E, 0x00 },
	{ 0x0C, 0x0E, 0x0C, 0x0C, 0x0C, 0x0C, 0x3F, 0x00 },
	{ 0x1E, 0x33, 0x30, 0x1C, 0x06, 0x33, 0x3F, 0x00 },
	{ 0x1E, 0x33, 0x30, 0x1C, 0x30, 0x33, 0x1E, 0x00 },
	{ 0x38, 0x3C, 0x36, 0x33, 0x7F, 0x30, 0x78, 0x00 },
	{ 0x3F, 0x03, 0x1F, 0x30, 0x30, 0x33, 0x1E, 0x00 },
	{ 0x1C, 0x06, 0x03, 0x1F, 0x33, 0x33, 0x1E, 0x00 },
	{ 0x3F, 0x33, 0x30, 0x18, 0x0C, 0x0C, 0x0C, 0x00 },
	{ 0x1E, 0x33, 0x33, 0x1E, 0x33, 0x33, 0x1E, 0x00 },
	{ 0x1E, 0x33, 0x33, 0x3E, 0x30, 0x18, 0x0E, 0x00 },
	{ 0x00, 0x0C, 0x0C, 0x00, 0x00, 0x0C, 0x0C, 0x00 },
	{ 0x00, 0x0C, 0x0C, 0x00, 0x00, 0x0C, 0x0C, 0x06 },
	{ 0x18, 0x0C, 0x06, 0x03, 0x06, 0x0C, 0x18, 0x00 },
	{ 0x00, 0x00, 0x3F, 0x00, 0x00, 0x3F, 0x00, 0x00 },
	{ 0x06, 0x0C, 0x18, 0x30, 0x18, 0x0C, 0x06, 0x00 },
	{ 0x1E, 0x33, 0x30, 0x18, 0x0C, 0x00, 0x0C, 0x00 },
	{ 0x3E, 0x63, 0x7B, 0x7B, 0x7B, 0x03, 0x1E, 0x00 },
	{ 0x0C, 0x1E, 0x33, 0x33, 0x3F, 0x33, 0x33, 0x00 },
	{ 0x3F, 0x66, 0x66, 0x3E, 0x66, 0x66, 0x3F, 0x00 },
	{ 0x3C, 0x66, 0x03, 0x03, 0x03, 0x66, 0x3C, 0x00 },
	{ 0x1F, 0x36, 0x66, 0x66, 0x66, 0x36, 0x1F, 0x00 },
	{ 0x7F, 0x46, 0x16, 0x1E, 0x16, 0x46, 0x7F, 0x00 },
	{ 0x7F, 0x46, 0x16, 0x1E, 0x16, 0x06, 0x0F, 0x00 },
	{ 0x3C, 0x66, 0x03, 0x03, 0x73, 0x66, 0x7C, 0x00 },
	{ 0x33, 0x33, 0x33, 0x3F, 0x33, 0x33, 0x33, 0x00 },
	{ 0x1E, 0x0C, 0x0C, 0x0C, 0x0C, 0x0C, 0x1E, 0x00 },
	{ 0x78, 0x30, 0x30, 0x30, 0x33, 0x33, 0x1E, 0x00 },
	{ 0x67, 0x66, 0x36, 0x1E, 0x36, 0x66, 0x67, 0x00 },
	{ 0x0F, 0x06, 0x06, 0x06, 0x46, 0x66, 0x7F, 0x00 },
	{ 0x63, 0x77, 0x7F, 0x7F, 0x6B, 0x63, 0x63, 0x00 },
	{ 0x63, 0x67, 0x6F, 0x7B, 0x73, 0x63, 0x63, 0x00 },
	{ 0x1C, 0x36, 0x63, 0x63, 0x63, 0x36, 0x1C, 0x00 },
	{ 0x3F, 0x66, 0x66, 0x3E, 0x06, 0x06, 0x0F, 0x00 },
	{ 0x1E, 0x33, 0x33, 0x33, 0x3B, 0x1E, 0x38, 0x00 },
	{ 0x3F, 0x66, 0x66, 0x3E, 0x36, 0x66, 0x67, 0x00 },
	{ 0x1E, 0x33, 0x07, 0x0E, 0x38, 0x33, 0x1E, 0x00 },
	{ 0x3F, 0x2D, 0x0C, 0x0C, 0x0C, 0x0C, 0x1E, 0x00 },
	{ 0x33, 0x33, 0x33, 0x33, 0x33, 0x33, 0x3F, 0x00 },
	{ 0x33, 0x33, 0x33, 0x33, 0x33, 0x1E, 0x0C, 0x00 },
	{ 0x63, 0x63, 0x63, 0x6B, 0x7F, 0x77, 0x63, 0x00 },
	{ 0x63, 0x63, 0x36, 0x1C, 0x1C, 0x36, 0x63, 0x00 },
	{ 0x33, 0x33, 0x33, 0x1E, 0x0C, 0x0C, 0x1E, 0x00 },
	{ 0x7F, 0x63, 0x31, 0x18, 0x4C, 0x66, 0x7F, 0x00 },
	{ 0x1E, 0x06, 0x06, 0x06, 0x06, 0x06, 0x1E, 0x00 },
	{ 0x03, 0x06, 0x0C, 0x18, 0x30, 0x60, 0x40, 0x00 },
	{ 0x1E, 0x18, 0x18, 0x18, 0x18, 0x18, 0x1E, 0x00 },
	{ 0x08, 0x1C, 0x36, 0x63, 0x00, 0x00, 0x00, 0x00 },
	{ 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0xFF },
	{ 0x0C, 0x0C, 0x18, 0x00, 0x00, 0x00, 0x00, 0x00 },
	{ 0x00, 0x00, 0x1E, 0x30, 0x3E, 0x33, 0x6E, 0x00 },
	{ 0x07, 0x06, 0x06, 0x3E, 0x66, 0x66, 0x3B, 0x00 },
	{ 0x00, 0x00, 0x1E, 0x33, 0x03, 0x33, 0x1E, 0x00 },
	{ 0x38, 0x30, 0x30, 0x3e, 0x33, 0x33, 0x6E, 0x00 },
	{ 0x00, 0x00, 0x1E, 0x33, 0x3f, 0x03, 0x1E, 0x00 },
	{ 0x1C, 0x36, 0x06, 0x0f, 0x06, 0x06, 0x0F, 0x00 },
	{ 0x00, 0x00, 0x6E, 0x33, 0x33, 0x3E, 0x30, 0x1F },
	{ 0x07, 0x06, 0x36, 0x6E, 0x66, 0x66, 0x67, 0x00 },
	{ 0x0C, 0x00, 0x0E, 0x0C, 0x0C, 0x0C, 0x1E, 0x00 },
	{ 0x30, 0x00, 0x30, 0x30, 0x30, 0x33, 0x33, 0x1E },
	{ 0x07, 0x06, 0x66, 0x36, 0x1E, 0x36, 0x67, 0x00 },
	{ 0x0E, 0x0C, 0x0C, 0x0C, 0x0C, 0x0C, 0x1E, 0x00 },
	{ 0x00, 0x00, 0x33, 0x7F, 0x7F, 0x6B, 0x63, 0x00 },
	{ 0x00, 0x00, 0x1F, 0x33, 0x33, 0x33, 0x33, 0x00 },
	{ 0x00, 0x00, 0x1E, 0x33, 0x33, 0x33, 0x1E, 0x00 },
	{ 0x00, 0x00, 0x3B, 0x66, 0x66, 0x3E, 0x06, 0x0F },
	{ 0x00, 0x00, 0x6E, 0x33, 0x33, 0x3E, 0x30, 0x78 },
	{ 0x00, 0x00, 0x3B, 0x6E, 0x66, 0x06, 0x0F, 0x00 },
	{ 0x00, 0x00, 0x3E, 0x03, 0x1E, 0x30, 0x1F, 0x00 },
	{ 0x08, 0x0C, 0x3E, 0x0C, 0x0C, 0x2C, 0x18, 0x00 },
	{ 0x00, 0x00, 0x33, 0x33, 0x33, 0x33, 0x6E, 0x00 },
	{ 0x00, 0x00, 0x33, 0x33, 0x33, 0x1E, 0x0C, 0x00 },
	{ 0x00, 0x00, 0x63, 0x6B, 0x7F, 0x7F, 0x36, 0x00 },
	{ 0x00, 0x00, 0x63, 0x36, 0x1C, 0x36, 0x63, 0x00 },
	{ 0x00, 0x00, 0x33, 0x33, 0x33, 0x3E, 0x30, 0x1F },
	{ 0x00, 0x00, 0x3F, 0x19, 0x0C, 0x26, 0x3F, 0x00 },
	{ 0x38, 0x0C, 0x0C, 0x07, 0x0C, 0x0C, 0x38, 0x00 },
	{ 0x18, 0x18, 0x18, 0x00, 0x18, 0x18, 0x18, 0x00 },
	{ 0x07, 0x0C, 0x0C, 0x38, 0x0C, 0x0C, 0x07, 0x00 },
	{ 0x6E, 0x3B, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00 },
	{ 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00 }
};

static const int GLIM_MAX_VERTS = 16 * 1024;

struct glimVertex_t {
	float xyz[3];
	float color[4];
	float uv[2];
};

struct glimMatrixStack_t {
	float m[16];
	int sp;
	float stack[16][16];
};

static bool glimInitialized = false;
static bool glimActive = false;		// true between glBegin and glEnd

static GLuint glimProgram = 0;
static GLuint glimVao = 0;
static GLuint glimVbo = 0;
static GLuint glimPixelTexture = 0;
static GLuint glimFontTexture = 0;
static GLint glimMvpLocation = -1;
static GLint glimTexturedLocation = -1;
static GLint glimPointSizeLocation = -1;
static GLint glimEnvModeLocation = -1;

static glimVertex_t glimVerts[GLIM_MAX_VERTS];
static int glimNumVerts = 0;
static GLenum glimMode = GL_TRIANGLES;

static float glimColor[4] = { 1.0f, 1.0f, 1.0f, 1.0f };
static float glimUV[2] = { 0.0f, 0.0f };
static bool glimTextured = false;

static GLenum glimMatrixMode = GL_MODELVIEW;
static glimMatrixStack_t glimModelView;
static glimMatrixStack_t glimProjection;

static float glimPointSize = 1.0f;
static int glimEnvMode = 0;		// 0 = modulate, 1 = replace/decal

// display list emulation
struct glimCmd_t {
	int op;
	float a, b, c, d;
};
enum {
	GLIM_OP_BEGIN,
	GLIM_OP_END,
	GLIM_OP_VERTEX,
	GLIM_OP_COLOR4,
	GLIM_OP_TEXCOORD,
	GLIM_OP_RASTERPOS2,
	GLIM_OP_RASTERPOS3,
	GLIM_OP_LINEWIDTH,
	GLIM_OP_POINTSIZE,
	GLIM_OP_TEX2D,
	GLIM_OP_TEXENV,
	GLIM_OP_GLYPH,
};
struct glimList_t {
	bool valid;
	idList<glimCmd_t> cmds;
};
static idList<glimList_t> glimLists;	// index = list id - 1
static GLuint glimRecordingList = 0;
static bool glimListExecute = false;
static GLuint glimListBase = 0;

// saved by glPushAttrib / restored by glPopAttrib
struct glimAttribState_t {
	float color[4];
	bool textured;
};
static idList<glimAttribState_t> glimAttribStack;

static int glimViewport[4] = { 0, 0, 0, 0 };
static float glimRasterPos[2] = { 0.0f, 0.0f };

/*
========================
Matrix helpers, column major like the GL convention
========================
*/
static void glimMatrixIdentity( float *out ) {
	memset( out, 0, 16 * sizeof( float ) );
	out[0] = out[5] = out[10] = out[15] = 1.0f;
}

static void glimMatrixMultiply( float *out, const float *a, const float *b ) {
	// out = a * b with column major storage
	float tmp[16];
	for ( int col = 0; col < 4; col++ ) {
		for ( int row = 0; row < 4; row++ ) {
			float sum = 0.0f;
			for ( int k = 0; k < 4; k++ ) {
				sum += a[row + k * 4] * b[k + col * 4];
			}
			tmp[row + col * 4] = sum;
		}
	}
	memcpy( out, tmp, 16 * sizeof( float ) );
}

static glimMatrixStack_t * glimCurrentStack() {
	return ( glimMatrixMode == GL_PROJECTION ) ? &glimProjection : &glimModelView;
}

void GLim_GetFloatv( GLenum pname, GLfloat *data ) {
	if ( data == NULL ) {
		return;
	}
	switch ( pname ) {
		case GL_CURRENT_COLOR:
			memcpy( data, glimColor, sizeof( glimColor ) );
			break;
		case GL_MODELVIEW_MATRIX:
			memcpy( data, glimModelView.m, sizeof( glimModelView.m ) );
			break;
		case GL_PROJECTION_MATRIX:
			memcpy( data, glimProjection.m, sizeof( glimProjection.m ) );
			break;
		default:
			rgl.GetFloatv( pname, data );
			break;
	}
}

/*
========================
GLim_ShaderSource
========================
*/
static const char * glimVertexShader =
	"#version 330 core\n"
	"uniform mat4 imMVP;\n"
	"uniform float imPointSize;\n"
	"layout(location = 0) in vec3 imPos;\n"
	"layout(location = 1) in vec4 imColor;\n"
	"layout(location = 2) in vec2 imUV;\n"
	"out vec4 imColorV;\n"
	"out vec2 imUVV;\n"
	"void main() {\n"
	"  gl_Position = imMVP * vec4( imPos, 1.0 );\n"
	"  gl_PointSize = imPointSize;\n"
	"  imColorV = imColor;\n"
	"  imUVV = imUV;\n"
	"}\n";

static const char * glimFragmentShader =
	"#version 330 core\n"
	"uniform sampler2D imTex;\n"
	"uniform int imTextured;\n"
	"uniform int imEnvMode;\n"
	"in vec4 imColorV;\n"
	"in vec2 imUVV;\n"
	"out vec4 fragColor;\n"
	"void main() {\n"
	"  vec4 tex = texture( imTex, imUVV );\n"
	"  if ( imTextured == 0 ) {\n"
	"    fragColor = imColorV;\n"
	"  } else if ( imEnvMode != 0 ) {\n"
	"    fragColor = vec4( tex.rgb, imColorV.a );\n"
	"  } else {\n"
	"    fragColor = tex * imColorV;\n"
	"  }\n"
	"}\n";

/*
========================
GLim_Init

Creates the shared program and buffers. Requires a current context.
========================
*/
void GLim_Init() {
	if ( glimInitialized ) {
		return;
	}

	const char * vsSources[1] = { glimVertexShader };
	const char * fsSources[1] = { glimFragmentShader };
	GLuint vs = glCreateShader( GL_VERTEX_SHADER );
	glShaderSource( vs, 1, vsSources, NULL );
	glCompileShader( vs );
	GLuint fs = glCreateShader( GL_FRAGMENT_SHADER );
	glShaderSource( fs, 1, fsSources, NULL );
	glCompileShader( fs );

	glimProgram = glCreateProgram();
	glAttachShader( glimProgram, vs );
	glAttachShader( glimProgram, fs );
	glLinkProgram( glimProgram );
	glDeleteShader( vs );
	glDeleteShader( fs );

	GLint linked = GL_FALSE;
	glGetProgramiv( glimProgram, GL_LINK_STATUS, &linked );
	if ( !linked ) {
		char log[1024];
		glGetProgramInfoLog( glimProgram, sizeof( log ), NULL, log );
		idLib::FatalError( "GLim_Init: failed to link immediate shader: %s", log );
	}

	glimMvpLocation = glGetUniformLocation( glimProgram, "imMVP" );
	glimTexturedLocation = glGetUniformLocation( glimProgram, "imTextured" );
	glimPointSizeLocation = glGetUniformLocation( glimProgram, "imPointSize" );
	glimEnvModeLocation = glGetUniformLocation( glimProgram, "imEnvMode" );

	glGenVertexArrays( 1, &glimVao );
	glGenBuffers( 1, &glimVbo );
	glBindVertexArray( glimVao );
	glBindBuffer( GL_ARRAY_BUFFER, glimVbo );
	glBufferData( GL_ARRAY_BUFFER, sizeof( glimVerts ), NULL, GL_DYNAMIC_DRAW );
	glVertexAttribPointer( 0, 3, GL_FLOAT, GL_FALSE, sizeof( glimVertex_t ), (const void *)offsetof( glimVertex_t, xyz ) );
	glEnableVertexAttribArray( 0 );
	glVertexAttribPointer( 1, 4, GL_FLOAT, GL_FALSE, sizeof( glimVertex_t ), (const void *)offsetof( glimVertex_t, color ) );
	glEnableVertexAttribArray( 1 );
	glVertexAttribPointer( 2, 2, GL_FLOAT, GL_FALSE, sizeof( glimVertex_t ), (const void *)offsetof( glimVertex_t, uv ) );
	glEnableVertexAttribArray( 2 );
	glBindVertexArray( 0 );
	glBindBuffer( GL_ARRAY_BUFFER, 0 );

	glGenTextures( 1, &glimPixelTexture );

	glimMatrixIdentity( glimModelView.m );
	glimModelView.sp = 0;
	glimMatrixIdentity( glimProjection.m );
	glimProjection.sp = 0;

	glimInitialized = true;
}

/*
========================
GLim_Shutdown
========================
*/
void GLim_Shutdown() {
	if ( !glimInitialized ) {
		return;
	}
	if ( glimProgram != 0 ) {
		glDeleteProgram( glimProgram );
		glimProgram = 0;
	}
	if ( glimVao != 0 ) {
		glDeleteVertexArrays( 1, &glimVao );
		glimVao = 0;
	}
	if ( glimVbo != 0 ) {
		glDeleteBuffers( 1, &glimVbo );
		glimVbo = 0;
	}
	if ( glimPixelTexture != 0 ) {
		glDeleteTextures( 1, &glimPixelTexture );
		glimPixelTexture = 0;
	}
	if ( glimFontTexture != 0 ) {
		glDeleteTextures( 1, &glimFontTexture );
		glimFontTexture = 0;
	}
	glimInitialized = false;
}

/*
========================
GLim_SetViewport
========================
*/
void GLim_SetViewport( GLint x, GLint y, GLsizei width, GLsizei height ) {
	glimViewport[0] = x;
	glimViewport[1] = y;
	glimViewport[2] = width;
	glimViewport[3] = height;
}

/*
========================
glimPixelOrtho

Ortho matrix over the current window in pixel coordinates.
========================
*/
static void glimPixelOrtho( float *out ) {
	glimMatrixIdentity( out );
	const float w = ( glimViewport[2] > 0 ) ? ( float )glimViewport[2] : 1.0f;
	const float h = ( glimViewport[3] > 0 ) ? ( float )glimViewport[3] : 1.0f;
	out[0] = 2.0f / w;
	out[5] = 2.0f / h;
	out[10] = -1.0f;
	out[12] = -1.0f;
	out[13] = -1.0f;
	out[14] = -1.0f;
}

/*
========================
glimFlush

Draws the recorded vertices with the immediate shader.
========================
*/
static void glimFlush() {
	if ( glimNumVerts == 0 ) {
		return;
	}

	GLenum drawMode = glimMode;
	int vertCount = glimNumVerts;
	const glimVertex_t * verts = glimVerts;

	// core profile has no quads or polygons; expand to triangles
	static glimVertex_t expanded[GLIM_MAX_VERTS * 2];
	if ( glimMode == GL_QUADS ) {
		int out = 0;
		for ( int i = 0; i + 3 < glimNumVerts; i += 4 ) {
			expanded[out++] = glimVerts[i];
			expanded[out++] = glimVerts[i + 1];
			expanded[out++] = glimVerts[i + 2];
			expanded[out++] = glimVerts[i];
			expanded[out++] = glimVerts[i + 2];
			expanded[out++] = glimVerts[i + 3];
		}
		verts = expanded;
		vertCount = out;
		drawMode = GL_TRIANGLES;
	} else if ( glimMode == GL_POLYGON ) {
		int out = 0;
		for ( int i = 1; i + 1 < glimNumVerts; i++ ) {
			expanded[out++] = glimVerts[0];
			expanded[out++] = glimVerts[i];
			expanded[out++] = glimVerts[i + 1];
		}
		verts = expanded;
		vertCount = out;
		drawMode = GL_TRIANGLES;
	} else if ( glimMode == GL_QUAD_STRIP ) {
		int out = 0;
		for ( int i = 0; i + 3 < glimNumVerts; i += 2 ) {
			expanded[out++] = glimVerts[i];
			expanded[out++] = glimVerts[i + 1];
			expanded[out++] = glimVerts[i + 3];
			expanded[out++] = glimVerts[i];
			expanded[out++] = glimVerts[i + 3];
			expanded[out++] = glimVerts[i + 2];
		}
		verts = expanded;
		vertCount = out;
		drawMode = GL_TRIANGLES;
	}

	GLint previousProgram = 0;
	glGetIntegerv( GL_CURRENT_PROGRAM, &previousProgram );
	GLint previousVao = 0;
	glGetIntegerv( GL_VERTEX_ARRAY_BINDING, &previousVao );
	GLint previousArrayBuffer = 0;
	glGetIntegerv( GL_ARRAY_BUFFER_BINDING, &previousArrayBuffer );

	glUseProgram( glimProgram );

	float mvp[16];
	glimMatrixMultiply( mvp, glimProjection.m, glimModelView.m );
	glUniformMatrix4fv( glimMvpLocation, 1, GL_FALSE, mvp );
	glUniform1i( glimTexturedLocation, glimTextured ? 1 : 0 );
	glUniform1f( glimPointSizeLocation, glimPointSize );
	glUniform1i( glimEnvModeLocation, glimEnvMode );
	glUniform1i( glGetUniformLocation( glimProgram, "imTex" ), 0 );

	glBindVertexArray( glimVao );
	glBindBuffer( GL_ARRAY_BUFFER, glimVbo );
	glBufferData( GL_ARRAY_BUFFER, sizeof( glimVerts ), NULL, GL_DYNAMIC_DRAW );
	glBufferSubData( GL_ARRAY_BUFFER, 0, vertCount * sizeof( glimVertex_t ), verts );

	glDrawArrays( drawMode, 0, vertCount );

	glBindVertexArray( previousVao );
	glBindBuffer( GL_ARRAY_BUFFER, previousArrayBuffer );
	glUseProgram( previousProgram );

	glimNumVerts = 0;
}

/*
========================
Display list recording helpers
========================
*/
static void glimRecord( int op, float a = 0.0f, float b = 0.0f, float c = 0.0f, float d = 0.0f ) {
	if ( glimRecordingList == 0 ) {
		return;
	}
	glimList_t & list = glimLists[glimRecordingList - 1];
	glimCmd_t & cmd = list.cmds.Alloc();
	cmd.op = op;
	cmd.a = a; cmd.b = b; cmd.c = c; cmd.d = d;
}

static bool glimRecording() {
	return glimRecordingList != 0;
}

/*
========================
Immediate mode vertex recording
========================
*/
void glBegin( GLenum mode ) {
	if ( glimRecording() ) {
		glimRecord( GLIM_OP_BEGIN, ( float )mode );
		return;
	}
	glimMode = mode;
	glimNumVerts = 0;
	glimActive = true;
}

void glEnd() {
	if ( glimRecording() ) {
		glimRecord( GLIM_OP_END );
		return;
	}
	glimActive = false;
	glimFlush();
}

static void glimAddVertex( float x, float y, float z ) {
	if ( glimNumVerts >= GLIM_MAX_VERTS ) {
		return;
	}
	glimVertex_t & v = glimVerts[glimNumVerts++];
	v.xyz[0] = x;
	v.xyz[1] = y;
	v.xyz[2] = z;
	memcpy( v.color, glimColor, sizeof( v.color ) );
	memcpy( v.uv, glimUV, sizeof( v.uv ) );
}

void glVertex2f( GLfloat x, GLfloat y ) {
	if ( glimRecording() ) { glimRecord( GLIM_OP_VERTEX, x, y, 0.0f ); return; }
	glimAddVertex( x, y, 0.0f );
}

void glVertex2fv( const GLfloat *v ) {
	glVertex2f( v[0], v[1] );
}

void glVertex3f( GLfloat x, GLfloat y, GLfloat z ) {
	if ( glimRecording() ) { glimRecord( GLIM_OP_VERTEX, x, y, z ); return; }
	glimAddVertex( x, y, z );
}

void glVertex3fv( const GLfloat *v ) {
	glVertex3f( v[0], v[1], v[2] );
}

void glColor3f( GLfloat red, GLfloat green, GLfloat blue ) {
	glimColor[0] = red;
	glimColor[1] = green;
	glimColor[2] = blue;
	glimColor[3] = 1.0f;
}

void glColor3fv( const GLfloat *v ) {
	glColor3f( v[0], v[1], v[2] );
}

void glColor3ub( GLubyte red, GLubyte green, GLubyte blue ) {
	glColor3f( red / 255.0f, green / 255.0f, blue / 255.0f );
}

void glColor4f( GLfloat red, GLfloat green, GLfloat blue, GLfloat alpha ) {
	if ( glimRecording() ) { glimRecord( GLIM_OP_COLOR4, red, green, blue, alpha ); return; }
	glimColor[0] = red;
	glimColor[1] = green;
	glimColor[2] = blue;
	glimColor[3] = alpha;
}

void glColor4fv( const GLfloat *v ) {
	glColor4f( v[0], v[1], v[2], v[3] );
}

void glColor4ub( GLubyte red, GLubyte green, GLubyte blue, GLubyte alpha ) {
	glColor4f( red / 255.0f, green / 255.0f, blue / 255.0f, alpha / 255.0f );
}

void glColor4ubv( const GLubyte *v ) {
	glColor4ub( v[0], v[1], v[2], v[3] );
}

void glTexCoord2f( GLfloat s, GLfloat t ) {
	if ( glimRecording() ) { glimRecord( GLIM_OP_TEXCOORD, s, t ); return; }
	glimUV[0] = s;
	glimUV[1] = t;
}

void glTexCoord2fv( const GLfloat *v ) {
	glTexCoord2f( v[0], v[1] );
}

/*
========================
Matrix stack emulation
========================
*/
void glMatrixMode( GLenum mode ) {
	glimMatrixMode = mode;
}

void glLoadIdentity() {
	glimMatrixIdentity( glimCurrentStack()->m );
}

void glLoadMatrixf( const GLfloat *m ) {
	memcpy( glimCurrentStack()->m, m, 16 * sizeof( float ) );
}

void glPushMatrix() {
	glimMatrixStack_t * s = glimCurrentStack();
	if ( s->sp < 15 ) {
		memcpy( s->stack[s->sp], s->m, 16 * sizeof( float ) );
		s->sp++;
	}
}

void glPopMatrix() {
	glimMatrixStack_t * s = glimCurrentStack();
	if ( s->sp > 0 ) {
		s->sp--;
		memcpy( s->m, s->stack[s->sp], 16 * sizeof( float ) );
	}
}

void glOrtho( GLdouble left, GLdouble right, GLdouble bottom, GLdouble top, GLdouble zNear, GLdouble zFar ) {
	float o[16];
	glimMatrixIdentity( o );
	o[0] = ( float )( 2.0 / ( right - left ) );
	o[5] = ( float )( 2.0 / ( top - bottom ) );
	o[10] = ( float )( -2.0 / ( zFar - zNear ) );
	o[12] = ( float )( -( right + left ) / ( right - left ) );
	o[13] = ( float )( -( top + bottom ) / ( top - bottom ) );
	o[14] = ( float )( -( zFar + zNear ) / ( zFar - zNear ) );
	glimMatrixStack_t * s = glimCurrentStack();
	float result[16];
	glimMatrixMultiply( result, s->m, o );
	memcpy( s->m, result, 16 * sizeof( float ) );
}

void glFrustum( GLdouble left, GLdouble right, GLdouble bottom, GLdouble top, GLdouble zNear, GLdouble zFar ) {
	float f[16];
	memset( f, 0, sizeof( f ) );
	f[0] = ( float )( ( 2.0 * zNear ) / ( right - left ) );
	f[5] = ( float )( ( 2.0 * zNear ) / ( top - bottom ) );
	f[8] = ( float )( ( right + left ) / ( right - left ) );
	f[9] = ( float )( ( top + bottom ) / ( top - bottom ) );
	f[10] = ( float )( -( zFar + zNear ) / ( zFar - zNear ) );
	f[11] = -1.0f;
	f[14] = ( float )( -( 2.0 * zFar * zNear ) / ( zFar - zNear ) );
	glimMatrixStack_t * s = glimCurrentStack();
	float result[16];
	glimMatrixMultiply( result, s->m, f );
	memcpy( s->m, result, 16 * sizeof( float ) );
}

void glTranslatef( GLfloat x, GLfloat y, GLfloat z ) {
	float t[16];
	glimMatrixIdentity( t );
	t[12] = x;
	t[13] = y;
	t[14] = z;
	glimMatrixStack_t * s = glimCurrentStack();
	float result[16];
	glimMatrixMultiply( result, s->m, t );
	memcpy( s->m, result, 16 * sizeof( float ) );
}

void glRotatef( GLfloat angle, GLfloat x, GLfloat y, GLfloat z ) {
	// normalize the axis and build the rotation in radians
	double len = sqrt( (double)x * x + (double)y * y + (double)z * z );
	if ( len == 0.0 ) {
		return;
	}
	double cx = x / len, cy = y / len, cz = z / len;
	double rad = angle * ( 3.14159265358979323846 / 180.0 );
	double c = cos( rad ), s = sin( rad ), ic = 1.0 - c;
	float r[16];
	r[0]  = ( float )( cx * cx * ic + c );
	r[1]  = ( float )( cy * cx * ic + cz * s );
	r[2]  = ( float )( cx * cz * ic - cy * s );
	r[3]  = 0.0f;
	r[4]  = ( float )( cx * cy * ic - cz * s );
	r[5]  = ( float )( cy * cy * ic + c );
	r[6]  = ( float )( cy * cz * ic + cx * s );
	r[7]  = 0.0f;
	r[8]  = ( float )( cx * cz * ic + cy * s );
	r[9]  = ( float )( cy * cz * ic - cx * s );
	r[10] = ( float )( cz * cz * ic + c );
	r[11] = 0.0f;
	r[12] = 0.0f;
	r[13] = 0.0f;
	r[14] = 0.0f;
	r[15] = 1.0f;
	glimMatrixStack_t * st = glimCurrentStack();
	float result[16];
	glimMatrixMultiply( result, st->m, r );
	memcpy( st->m, result, 16 * sizeof( float ) );
}

// client state and polygon stipple have no core-profile equivalent; the
// editor paths only used them with the fixed-function pipeline
void glEnableClientState( GLenum array ) {
	( void )array;
}

void glDisableClientState( GLenum array ) {
	( void )array;
}

void glPolygonStipple( const GLubyte * mask ) {
	( void )mask;
}

/*
========================
Raster position and glDrawPixels

The raster position is transformed by the current matrices like the legacy
pipeline did, then the pixels are drawn as a quad in window coordinates.
========================
*/
static void glimSetRasterPos( GLfloat x, GLfloat y, GLfloat z ) {
	float mvp[16];
	glimMatrixMultiply( mvp, glimProjection.m, glimModelView.m );
	float clip[4] = { x, y, z, 1.0f };
	float out[4];
	for ( int i = 0; i < 4; i++ ) {
		out[i] = mvp[i] * clip[0] + mvp[4 + i] * clip[1] + mvp[8 + i] * clip[2] + mvp[12 + i] * clip[3];
	}
	if ( out[3] == 0.0f ) {
		out[3] = 1.0f;
	}
	glimRasterPos[0] = ( out[0] / out[3] * 0.5f + 0.5f ) * glimViewport[2] + glimViewport[0];
	glimRasterPos[1] = ( out[1] / out[3] * 0.5f + 0.5f ) * glimViewport[3] + glimViewport[1];
}

void glRasterPos2f( GLfloat x, GLfloat y ) {
	if ( glimRecording() ) { glimRecord( GLIM_OP_RASTERPOS2, x, y ); return; }
	glimSetRasterPos( x, y, 0.0f );
}

void glRasterPos3f( GLfloat x, GLfloat y, GLfloat z ) {
	if ( glimRecording() ) { glimRecord( GLIM_OP_RASTERPOS3, x, y, z ); return; }
	glimSetRasterPos( x, y, z );
}

void glRasterPos3fv( const GLfloat *v ) {
	glRasterPos3f( v[0], v[1], v[2] );
}

void glDrawPixels( GLsizei width, GLsizei height, GLenum format, GLenum type, const void *pixels ) {
	if ( pixels == NULL || width <= 0 || height <= 0 ) {
		return;
	}
	if ( format != GL_RGBA || type != GL_UNSIGNED_BYTE ) {
		// the callers only ever draw RGBA byte images
		return;
	}

	GLint previousProgram = 0;
	glGetIntegerv( GL_CURRENT_PROGRAM, &previousProgram );
	GLint previousVao = 0;
	glGetIntegerv( GL_VERTEX_ARRAY_BINDING, &previousVao );
	GLint previousArrayBuffer = 0;
	glGetIntegerv( GL_ARRAY_BUFFER_BINDING, &previousArrayBuffer );
	GLint previousTexture = 0;
	glActiveTexture( GL_TEXTURE0 );
	glGetIntegerv( GL_TEXTURE_BINDING_2D, &previousTexture );

	glBindTexture( GL_TEXTURE_2D, glimPixelTexture );
	glTexParameteri( GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST );
	glTexParameteri( GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST );
	glPixelStorei( GL_UNPACK_ROW_LENGTH, 0 );
	glPixelStorei( GL_UNPACK_ALIGNMENT, 1 );
	glTexImage2D( GL_TEXTURE_2D, 0, GL_RGBA8, width, height, 0, GL_RGBA, GL_UNSIGNED_BYTE, pixels );

	glUseProgram( glimProgram );
	glUniform1i( glimTexturedLocation, 1 );
	glUniform1i( glGetUniformLocation( glimProgram, "imTex" ), 0 );

	// identity modelview, pixel sized ortho projection over the window
	float pixelMvp[16];
	glimPixelOrtho( pixelMvp );
	glUniformMatrix4fv( glimMvpLocation, 1, GL_FALSE, pixelMvp );

	glimVertex_t quad[6];
	memset( quad, 0, sizeof( quad ) );
	for ( int i = 0; i < 6; i++ ) {
		quad[i].color[0] = quad[i].color[1] = quad[i].color[2] = quad[i].color[3] = 1.0f;
	}
	const float x0 = glimRasterPos[0];
	const float y0 = glimRasterPos[1];
	const float x1 = x0 + ( float )width;
	const float y1 = y0 + ( float )height;
	quad[0].xyz[0] = x0; quad[0].xyz[1] = y0; quad[0].uv[0] = 0.0f; quad[0].uv[1] = 0.0f;
	quad[1].xyz[0] = x1; quad[1].xyz[1] = y0; quad[1].uv[0] = 1.0f; quad[1].uv[1] = 0.0f;
	quad[2].xyz[0] = x1; quad[2].xyz[1] = y1; quad[2].uv[0] = 1.0f; quad[2].uv[1] = 1.0f;
	quad[3].xyz[0] = x0; quad[3].xyz[1] = y0; quad[3].uv[0] = 0.0f; quad[3].uv[1] = 0.0f;
	quad[4].xyz[0] = x1; quad[4].xyz[1] = y1; quad[4].uv[0] = 1.0f; quad[4].uv[1] = 1.0f;
	quad[5].xyz[0] = x0; quad[5].xyz[1] = y1; quad[5].uv[0] = 0.0f; quad[5].uv[1] = 1.0f;

	glBindVertexArray( glimVao );
	glBindBuffer( GL_ARRAY_BUFFER, glimVbo );
	glBufferData( GL_ARRAY_BUFFER, sizeof( quad ), NULL, GL_DYNAMIC_DRAW );
	glBufferSubData( GL_ARRAY_BUFFER, 0, sizeof( quad ), quad );
	glDrawArrays( GL_TRIANGLES, 0, 6 );

	glBindTexture( GL_TEXTURE_2D, previousTexture );
	glBindVertexArray( previousVao );
	glBindBuffer( GL_ARRAY_BUFFER, previousArrayBuffer );
	glUseProgram( previousProgram );
}

/*
========================
Attrib stack

Only saves what the debug and editor callers actually change between
push and pop.
========================
*/
void glPushAttrib( GLbitfield mask ) {
	glimAttribState_t state;
	memcpy( state.color, glimColor, sizeof( state.color ) );
	state.textured = glimTextured;
	glimAttribStack.Append( state );
}

void glPopAttrib() {
	if ( glimAttribStack.Num() == 0 ) {
		return;
	}
	const glimAttribState_t & state = glimAttribStack[glimAttribStack.Num() - 1];
	memcpy( glimColor, state.color, sizeof( glimColor ) );
	glimTextured = state.textured;
	glimAttribStack.SetNum( glimAttribStack.Num() - 1 );
}

/*
========================
Texture 2D enable tracking
========================
*/
void GLim_EnableTexture2D( bool enable ) {
	glimTextured = enable;
}

bool GLim_Texture2DEnabled() {
	return glimTextured;
}

/*
========================
Filtered enable/disable
========================
*/
void glEnable( GLenum cap ) {
	if ( cap == GL_TEXTURE_2D ) {
		GLim_EnableTexture2D( true );
		return;
	}
	// caps that no longer exist in the core profile
	if ( cap == GL_TEXTURE_1D || cap == GL_MULTISAMPLE_ARB || cap == GL_POLYGON_OFFSET_LINE ) {
		return;
	}
	rgl.Enable( cap );
}

void glDisable( GLenum cap ) {
	if ( cap == GL_TEXTURE_2D ) {
		GLim_EnableTexture2D( false );
		return;
	}
	if ( cap == GL_TEXTURE_1D || cap == GL_MULTISAMPLE_ARB || cap == GL_POLYGON_OFFSET_LINE ) {
		return;
	}
	rgl.Disable( cap );
}

/*
========================
Small state stubs
========================
*/
void glPointSize( GLfloat size ) {
	glimPointSize = size;
}

void glLineStipple( GLint factor, GLushort pattern ) {
	// core profile has no line stipple; draw solid lines
}

void glShadeModel( GLenum mode ) {
	// the immediate shader always interpolates
}

void glTexEnvf( GLenum target, GLenum pname, GLfloat param ) {
	if ( target == GL_TEXTURE_ENV && pname == GL_TEXTURE_ENV_MODE ) {
		glimEnvMode = ( param == GL_DECAL || param == GL_REPLACE ) ? 1 : 0;
	}
}

void glPixelZoom( GLfloat xfactor, GLfloat yfactor ) {
}

void glCopyPixels( GLint x, GLint y, GLsizei width, GLsizei height, GLenum type ) {
}

void glRectf( GLfloat x1, GLfloat y1, GLfloat x2, GLfloat y2 ) {
	glBegin( GL_QUADS );
	glVertex2f( x1, y1 );
	glVertex2f( x2, y1 );
	glVertex2f( x2, y2 );
	glVertex2f( x1, y2 );
	glEnd();
}

/*
========================
Display list emulation
========================
*/
GLuint glGenLists( GLsizei range ) {
	if ( range <= 0 ) {
		return 0;
	}
	// find a free contiguous run (index 0 means list id 1)
	int base = glimLists.Num();
	for ( int i = 0; i + range <= glimLists.Num(); ) {
		if ( !glimLists[i].valid ) {
			int j = i;
			while ( j < glimLists.Num() && !glimLists[j].valid && j - i < range ) {
				j++;
			}
			if ( j - i >= range ) {
				base = i;
				break;
			}
			i = j + 1;
		} else {
			i++;
		}
	}
	while ( glimLists.Num() < base + range ) {
		glimList_t empty;
		empty.valid = false;
		glimLists.Append( empty );
	}
	for ( int i = base; i < base + range; i++ ) {
		glimLists[i].valid = false;
		glimLists[i].cmds.Clear();
	}
	return base + 1;
}

void glDeleteLists( GLuint list, GLsizei range ) {
	for ( GLsizei i = 0; i < range; i++ ) {
		GLuint id = list + i;
		if ( id >= 1 && id <= ( GLuint )glimLists.Num() ) {
			glimLists[id - 1].valid = false;
			glimLists[id - 1].cmds.Clear();
		}
	}
}

static void glimReplay( const idList<glimCmd_t> & cmds );

void glNewList( GLuint list, GLenum mode ) {
	glimRecordingList = list;
	// remember whether the list should also run once now
	glimListExecute = ( mode == GL_COMPILE_AND_EXECUTE );
	if ( list >= 1 && list <= ( GLuint )glimLists.Num() ) {
		glimLists[list - 1].valid = false;
		glimLists[list - 1].cmds.Clear();
	}
}

void glEndList() {
	if ( glimRecordingList != 0 && glimRecordingList <= ( GLuint )glimLists.Num() ) {
		glimLists[glimRecordingList - 1].valid = true;
		// GL_COMPILE_AND_EXECUTE: replay the fresh list immediately
		if ( glimListExecute ) {
			glimReplay( glimLists[glimRecordingList - 1].cmds );
		}
	}
	glimListExecute = false;
	glimRecordingList = 0;
}

static void glimReplay( const idList<glimCmd_t> & cmds );
static void glimDrawGlyph( int glyph );

void glCallList( GLuint list ) {
	if ( list < 1 || list > ( GLuint )glimLists.Num() ) {
		return;
	}
	const glimList_t & listData = glimLists[list - 1];
	if ( !listData.valid ) {
		return;
	}
	glimReplay( listData.cmds );
}

void glListBase( GLuint base ) {
	glimListBase = base;
}

void glCallLists( GLsizei n, GLenum type, const GLvoid *lists ) {
	if ( lists == NULL || n <= 0 || type != GL_UNSIGNED_BYTE ) {
		return;
	}
	const GLubyte * chars = ( const GLubyte * )lists;
	for ( GLsizei i = 0; i < n; i++ ) {
		glCallList( glimListBase + chars[i] );
	}
}

/*
========================
glimReplay
========================
*/
static void glimReplay( const idList<glimCmd_t> & cmds ) {
	// recording state so nested list calls behave
	const bool wasRecording = glimRecording();
	GLuint savedRecording = glimRecordingList;
	glimRecordingList = 0;
	for ( int i = 0; i < cmds.Num(); i++ ) {
		const glimCmd_t & cmd = cmds[i];
		switch ( cmd.op ) {
			case GLIM_OP_BEGIN:		glBegin( ( GLenum )cmd.a ); break;
			case GLIM_OP_END:		glEnd(); break;
			case GLIM_OP_VERTEX:	glVertex3f( cmd.a, cmd.b, cmd.c ); break;
			case GLIM_OP_COLOR4:	glColor4f( cmd.a, cmd.b, cmd.c, cmd.d ); break;
			case GLIM_OP_TEXCOORD:	glTexCoord2f( cmd.a, cmd.b ); break;
			case GLIM_OP_RASTERPOS2:	glimSetRasterPos( cmd.a, cmd.b, 0.0f ); break;
			case GLIM_OP_RASTERPOS3:	glimSetRasterPos( cmd.a, cmd.b, cmd.c ); break;
			case GLIM_OP_LINEWIDTH:	glLineWidth( cmd.a ); break;
			case GLIM_OP_POINTSIZE:	glPointSize( cmd.a ); break;
			case GLIM_OP_TEX2D:		GLim_EnableTexture2D( cmd.a != 0.0f ); break;
			case GLIM_OP_TEXENV:	glimEnvMode = ( int )cmd.a; break;
			case GLIM_OP_GLYPH:		glimDrawGlyph( ( int )cmd.a ); break;
		}
	}
	glimRecordingList = savedRecording;
}

/*
========================
Font

Builds 256 single glyph display lists from the embedded 8x8 font. Each
glyph draws a textured quad at the current raster position and advances
the raster position, like the old wglUseFontBitmaps lists.
========================
*/
bool GLim_BuildFontList( GLuint base ) {
	if ( !glimInitialized ) {
		return false;
	}
	if ( glimFontTexture == 0 ) {
		// 128 glyphs, 8 pixels wide and tall, one red channel per texel
		const int atlasWidth = 128 * 8;
		unsigned char * atlas = ( unsigned char * )malloc( atlasWidth * 8 );
		memset( atlas, 0, atlasWidth * 8 );
		for ( int glyph = 0; glyph < 128; glyph++ ) {
			for ( int row = 0; row < 8; row++ ) {
				for ( int col = 0; col < 8; col++ ) {
					// font rows are stored top to bottom, bit 7 first
					if ( glimFont8x8[glyph][row] & ( 1 << ( 7 - col ) ) ) {
						atlas[row * atlasWidth + glyph * 8 + col] = 255;
					}
				}
			}
		}
		glGenTextures( 1, &glimFontTexture );
		glBindTexture( GL_TEXTURE_2D, glimFontTexture );
		glTexParameteri( GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST );
		glTexParameteri( GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST );
		glPixelStorei( GL_UNPACK_ALIGNMENT, 1 );
		glTexImage2D( GL_TEXTURE_2D, 0, GL_R8, atlasWidth, 8, 0, GL_RED, GL_UNSIGNED_BYTE, atlas );
		// route the red channel into alpha so color modulation works
		const GLint swizzle[4] = { GL_ONE, GL_ONE, GL_ONE, GL_RED };
		glTexParameteriv( GL_TEXTURE_2D, GL_TEXTURE_SWIZZLE_RGBA, swizzle );
		free( atlas );
		glBindTexture( GL_TEXTURE_2D, 0 );
	}

	for ( int glyph = 0; glyph < 256; glyph++ ) {
		GLuint id = base + glyph;
		while ( id > ( GLuint )glimLists.Num() ) {
			glimList_t empty;
			empty.valid = false;
			glimLists.Append( empty );
		}
		glimList_t & list = glimLists[id - 1];
		list.cmds.Clear();
		glimCmd_t & glyphCmd = list.cmds.Alloc();
		glyphCmd.op = GLIM_OP_GLYPH;
		glyphCmd.a = ( float )( glyph & 0x7f );
		glyphCmd.b = glyphCmd.c = glyphCmd.d = 0.0f;
		list.valid = true;
	}
	return true;
}

/*
========================
glimDrawGlyph

Draws one font glyph as a textured quad at the raster position and
advances the raster position by the glyph width.
========================
*/
static void glimDrawGlyph( int glyph ) {
	if ( glimFontTexture == 0 ) {
		glimRasterPos[0] += 8.0f;
		return;
	}

	GLint previousProgram = 0;
	glGetIntegerv( GL_CURRENT_PROGRAM, &previousProgram );
	GLint previousVao = 0;
	glGetIntegerv( GL_VERTEX_ARRAY_BINDING, &previousVao );
	GLint previousArrayBuffer = 0;
	glGetIntegerv( GL_ARRAY_BUFFER_BINDING, &previousArrayBuffer );
	GLint previousTexture = 0;
	glActiveTexture( GL_TEXTURE0 );
	glGetIntegerv( GL_TEXTURE_BINDING_2D, &previousTexture );

	glBindTexture( GL_TEXTURE_2D, glimFontTexture );

	glUseProgram( glimProgram );
	glUniform1i( glimTexturedLocation, 1 );
	glUniform1f( glimPointSizeLocation, glimPointSize );
	glUniform1i( glimEnvModeLocation, 1 );
	glUniform1i( glGetUniformLocation( glimProgram, "imTex" ), 0 );

	float mvp[16];
	glimMatrixMultiply( mvp, glimProjection.m, glimModelView.m );
	glUniformMatrix4fv( glimMvpLocation, 1, GL_FALSE, mvp );

	const float x0 = glimRasterPos[0];
	const float y0 = glimRasterPos[1];
	const float x1 = x0 + 8.0f;
	const float y1 = y0 + 8.0f;
	const float u0 = ( float )( glyph * 8 ) / ( 128.0f * 8.0f );
	const float u1 = ( float )( ( glyph + 1 ) * 8 ) / ( 128.0f * 8.0f );

	glimVertex_t quad[6];
	memset( quad, 0, sizeof( quad ) );
	for ( int i = 0; i < 6; i++ ) {
		memcpy( quad[i].color, glimColor, sizeof( quad[i].color ) );
	}
	quad[0].xyz[0] = x0; quad[0].xyz[1] = y0; quad[0].uv[0] = u0; quad[0].uv[1] = 1.0f;
	quad[1].xyz[0] = x1; quad[1].xyz[1] = y0; quad[1].uv[0] = u1; quad[1].uv[1] = 1.0f;
	quad[2].xyz[0] = x1; quad[2].xyz[1] = y1; quad[2].uv[0] = u1; quad[2].uv[1] = 0.0f;
	quad[3].xyz[0] = x0; quad[3].xyz[1] = y0; quad[3].uv[0] = u0; quad[3].uv[1] = 1.0f;
	quad[4].xyz[0] = x1; quad[4].xyz[1] = y1; quad[4].uv[0] = u1; quad[4].uv[1] = 0.0f;
	quad[5].xyz[0] = x0; quad[5].xyz[1] = y1; quad[5].uv[0] = u0; quad[5].uv[1] = 0.0f;

	glBindVertexArray( glimVao );
	glBindBuffer( GL_ARRAY_BUFFER, glimVbo );
	glBufferData( GL_ARRAY_BUFFER, sizeof( quad ), NULL, GL_DYNAMIC_DRAW );
	glBufferSubData( GL_ARRAY_BUFFER, 0, sizeof( quad ), quad );
	glDrawArrays( GL_TRIANGLES, 0, 6 );

	glBindTexture( GL_TEXTURE_2D, previousTexture );
	glBindVertexArray( previousVao );
	glBindBuffer( GL_ARRAY_BUFFER, previousArrayBuffer );
	glUseProgram( previousProgram );

	glimRasterPos[0] = x1;
}

/*
========================
glu replacements
========================
*/
struct GLUquadric {
	int unused;
};
static GLUquadric glimDummyQuadric;

GLUquadric * gluNewQuadric() {
	return &glimDummyQuadric;
}

void gluDeleteQuadric( GLUquadric * quad ) {
}

void gluSphere( GLUquadric * quad, GLfloat radius, GLint slices, GLint stacks ) {
	// wireframe sphere through the immediate path
	for ( int i = 1; i < stacks; i++ ) {
		const float phi = 3.14159265f * ( float )i / ( float )stacks;
		const float z = radius * cosf( phi );
		const float r = radius * sinf( phi );
		glBegin( GL_LINE_LOOP );
		for ( int j = 0; j < slices; j++ ) {
			const float theta = 2.0f * 3.14159265f * ( float )j / ( float )slices;
			glVertex3f( r * cosf( theta ), r * sinf( theta ), z );
		}
		glEnd();
	}
	for ( int j = 0; j < slices; j++ ) {
		const float theta = 2.0f * 3.14159265f * ( float )j / ( float )slices;
		glBegin( GL_LINE_LOOP );
		for ( int i = 1; i < stacks; i++ ) {
			const float phi = 3.14159265f * ( float )i / ( float )stacks;
			const float z = radius * cosf( phi );
			const float r = radius * sinf( phi );
			glVertex3f( r * cosf( theta ), r * sinf( theta ), z );
		}
		glVertex3f( 0.0f, 0.0f, radius );
		glVertex3f( 0.0f, 0.0f, -radius );
		glEnd();
	}
}

void gluPerspective( GLdouble fovy, GLdouble aspect, GLdouble zNear, GLdouble zFar ) {
	const double fH = tan( fovy / 360.0 * 3.14159265 ) * zNear;
	const double fW = fH * aspect;
	glFrustum( -fW, fW, -fH, fH, zNear, zFar );
}
