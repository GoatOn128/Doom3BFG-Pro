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

#ifndef GL_IMMEDIATE_H
#define GL_IMMEDIATE_H

// Matrix modes and primitives that no longer exist in the core headers.
#ifndef GL_MODELVIEW
#define GL_MODELVIEW 0x1700
#endif
#ifndef GL_PROJECTION
#define GL_PROJECTION 0x1701
#endif
#ifndef GL_POLYGON
#define GL_POLYGON 0x0009
#endif
#ifndef GL_ALL_ATTRIB_BITS
#define GL_ALL_ATTRIB_BITS 0xFFFFFFFF
#endif
// texture env enums that the core header dropped
#ifndef GL_TEXTURE_ENV
#define GL_TEXTURE_ENV 0x1702
#endif
#ifndef GL_TEXTURE_ENV_MODE
#define GL_TEXTURE_ENV_MODE 0x2200
#endif
#ifndef GL_DECAL
#define GL_DECAL 0x2101
#endif
// legacy enums still referenced by editor and debug code
#ifndef GL_MODELVIEW_MATRIX
#define GL_MODELVIEW_MATRIX 0x0BA6
#endif
#ifndef GL_PROJECTION_MATRIX
#define GL_PROJECTION_MATRIX 0x0BA7
#endif
#ifndef GL_CURRENT_COLOR
#define GL_CURRENT_COLOR 0x0B00
#endif
#ifndef GL_LIGHTING
#define GL_LIGHTING 0x0B50
#endif
#ifndef GL_FLAT
#define GL_FLAT 0x1D00
#endif
#ifndef GL_SMOOTH
#define GL_SMOOTH 0x1D01
#endif
#ifndef GL_TEXTURE_FILTER_CONTROL
#define GL_TEXTURE_FILTER_CONTROL 0x8500
#endif
#ifndef GL_COMPILE
#define GL_COMPILE 0x1300
#endif
#ifndef GL_COMPILE_AND_EXECUTE
#define GL_COMPILE_AND_EXECUTE 0x1301
#endif
#ifndef GL_QUAD_STRIP
#define GL_QUAD_STRIP 0x0008
#endif
#ifndef GL_LINE_STIPPLE
#define GL_LINE_STIPPLE 0x0B24
#endif

void GLim_Init();
void GLim_Shutdown();
// The real glViewport wrapper reports the window size here so raster
// position and glDrawPixels emulation can work in window coordinates.
void GLim_SetViewport( GLint x, GLint y, GLsizei width, GLsizei height );
// glGetFloatv shim: serves the legacy pnames the core driver cannot
// (current color, modelview/projection matrices), forwards the rest.
void GLim_GetFloatv( GLenum pname, GLfloat *data );

// Fixed function emulation. These mirror the legacy entry points the debug
// and editor code was written against.
void glBegin( GLenum mode );
void glEnd();
// Filtered enable/disable: GL_TEXTURE_2D toggles the emulation state,
// removed core caps (multisample, polygon offset line, texture 1D) are
// ignored, everything else forwards to the driver.
void glEnable( GLenum cap );
void glDisable( GLenum cap );
void glVertex2f( GLfloat x, GLfloat y );
void glVertex2fv( const GLfloat *v );
void glVertex3f( GLfloat x, GLfloat y, GLfloat z );
void glVertex3fv( const GLfloat *v );
void glColor3f( GLfloat red, GLfloat green, GLfloat blue );
void glColor3fv( const GLfloat *v );
void glColor3ub( GLubyte red, GLubyte green, GLubyte blue );
void glColor4f( GLfloat red, GLfloat green, GLfloat blue, GLfloat alpha );
void glColor4fv( const GLfloat *v );
void glColor4ub( GLubyte red, GLubyte green, GLubyte blue, GLubyte alpha );
void glColor4ubv( const GLubyte *v );
void glTexCoord2f( GLfloat s, GLfloat t );
void glTexCoord2fv( const GLfloat *v );
void glMatrixMode( GLenum mode );
void glLoadIdentity();
void glLoadMatrixf( const GLfloat *m );
void glPushMatrix();
void glPopMatrix();
void glOrtho( GLdouble left, GLdouble right, GLdouble bottom, GLdouble top, GLdouble zNear, GLdouble zFar );
void glFrustum( GLdouble left, GLdouble right, GLdouble bottom, GLdouble top, GLdouble zNear, GLdouble zFar );
void glTranslatef( GLfloat x, GLfloat y, GLfloat z );
void glRotatef( GLfloat angle, GLfloat x, GLfloat y, GLfloat z );
void glEnableClientState( GLenum array );
void glDisableClientState( GLenum array );
void glPolygonStipple( const GLubyte * mask );
void glRasterPos2f( GLfloat x, GLfloat y );
void glRasterPos3f( GLfloat x, GLfloat y, GLfloat z );
void glRasterPos3fv( const GLfloat *v );
void glDrawPixels( GLsizei width, GLsizei height, GLenum format, GLenum type, const void *pixels );
void glPushAttrib( GLbitfield mask );
void glPopAttrib();
// Small extras the editor and debug code still calls.
void glPointSize( GLfloat size );
void glLineStipple( GLint factor, GLushort pattern );
void glShadeModel( GLenum mode );
void glRectf( GLfloat x1, GLfloat y1, GLfloat x2, GLfloat y2 );
void glTexEnvf( GLenum target, GLenum pname, GLfloat param );
void glPixelZoom( GLfloat xfactor, GLfloat yfactor );
void glCopyPixels( GLint x, GLint y, GLsizei width, GLsizei height, GLenum type );

// Display list emulation. Only the immediate drawing commands listed above
// are recorded; state calls inside a list are dropped.
GLuint glGenLists( GLsizei range );
void glDeleteLists( GLuint list, GLsizei range );
void glNewList( GLuint list, GLenum mode );
void glEndList();
void glCallList( GLuint list );
void glListBase( GLuint base );
void glCallLists( GLsizei n, GLenum type, const GLvoid *lists );

// Replaces the wglUseFontBitmaps font setup. Builds 256 single glyph lists
// from an embedded bitmap font so the editor text calls keep working.
bool GLim_BuildFontList( GLuint base );

// glu replacements (glu32 is gone). gluPerspective is pure math, the
// quadric functions draw wireframe spheres through the immediate path.
struct GLUquadric;
// legacy glu typedef still used by the editor
typedef GLUquadric GLUquadricObj;
GLUquadric * gluNewQuadric();
void gluDeleteQuadric( GLUquadric * quad );
void gluSphere( GLUquadric * quad, GLfloat radius, GLint slices, GLint stacks );
void gluPerspective( GLdouble fovy, GLdouble aspect, GLdouble zNear, GLdouble zFar );

// Texture 2D enable state is tracked here because core profile removed the
// fixed function flag the editor toggles with glEnable/glDisable.
void GLim_EnableTexture2D( bool enable );
bool GLim_Texture2DEnabled();

#endif // GL_IMMEDIATE_H
