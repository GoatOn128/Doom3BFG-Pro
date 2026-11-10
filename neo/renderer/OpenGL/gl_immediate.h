/*
===========================================================================

Core profile immediate mode emulation.

The debug renderer and the editor viewports draw with old style
glBegin/glEnd blocks. OpenGL 3.3 core removed that pipeline, so this module
implements the used subset on top of a streaming VBO and a small shader.
Only what those callers need is provided; anything else is a stub.

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

void GLim_Init();
void GLim_Shutdown();
// The real glViewport wrapper reports the window size here so raster
// position and glDrawPixels emulation can work in window coordinates.
void GLim_SetViewport( GLint x, GLint y, GLsizei width, GLsizei height );

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
GLUquadric * gluNewQuadric();
void gluDeleteQuadric( GLUquadric * quad );
void gluSphere( GLUquadric * quad, GLfloat radius, GLint slices, GLint stacks );
void gluPerspective( GLdouble fovy, GLdouble aspect, GLdouble zNear, GLdouble zFar );

// Texture 2D enable state is tracked here because core profile removed the
// fixed function flag the editor toggles with glEnable/glDisable.
void GLim_EnableTexture2D( bool enable );
bool GLim_Texture2DEnabled();

#endif // GL_IMMEDIATE_H
