/*
===========================================================================

Darklight OpenGL 3.3 core entry points.

Replaces the old qgl function pointer layer. All engine code calls the plain
gl* names declared here; they forward to the single global glad context that
GLcore_Load fills in after the render context is current.

===========================================================================
*/
#ifndef GL_CORE_H
#define GL_CORE_H

#include <glad/gl.h>

#include "gl_immediate.h"

// The one GL context used by the engine. Loaded by GLcore_Load().
extern GladGLContext rgl;

// Load all entry points for the current context. Returns false on failure.
bool GLcore_Load();
void GLcore_Unload();

// glad was generated with multi-context support, so the entry points live in
// the context struct. These wrappers restore the plain gl* naming the engine
// uses and adapt a few signatures to the old call styles.

inline void glBindTexture( GLenum target, GLuint texture ) { rgl.BindTexture( target, texture ); }
inline void glDeleteTextures( GLsizei n, const GLuint *textures ) { rgl.DeleteTextures( n, textures ); }
inline void glGenTextures( GLsizei n, GLuint *textures ) { rgl.GenTextures( n, textures ); }
inline void glTexParameteri( GLenum target, GLenum pname, GLint param ) { rgl.TexParameteri( target, pname, param ); }
inline void glTexParameterf( GLenum target, GLenum pname, GLfloat param ) { rgl.TexParameterf( target, pname, param ); }
inline void glTexParameterfv( GLenum target, GLenum pname, const GLfloat *params ) { rgl.TexParameterfv( target, pname, params ); }
inline void glTexParameteriv( GLenum target, GLenum pname, const GLint *params ) { rgl.TexParameteriv( target, pname, params ); }
inline void glTexImage2D( GLenum target, GLint level, GLint internalformat, GLsizei width, GLsizei height, GLint border, GLenum format, GLenum type, const void *pixels ) { rgl.TexImage2D( target, level, internalformat, width, height, border, format, type, pixels ); }
inline void glTexSubImage2D( GLenum target, GLint level, GLint xoffset, GLint yoffset, GLsizei width, GLsizei height, GLenum format, GLenum type, const void *pixels ) { rgl.TexSubImage2D( target, level, xoffset, yoffset, width, height, format, type, pixels ); }
inline void glCompressedTexImage2D( GLenum target, GLint level, GLenum internalformat, GLsizei width, GLsizei height, GLint border, GLsizei imageSize, const void *data ) { rgl.CompressedTexImage2D( target, level, internalformat, width, height, border, imageSize, data ); }
inline void glCompressedTexSubImage2D( GLenum target, GLint level, GLint xoffset, GLint yoffset, GLsizei width, GLsizei height, GLenum format, GLsizei imageSize, const void *data ) { rgl.CompressedTexSubImage2D( target, level, xoffset, yoffset, width, height, format, imageSize, data ); }
inline void glGetCompressedTexImage( GLenum target, GLint level, void *img ) { rgl.GetCompressedTexImage( target, level, img ); }
inline void glPixelStorei( GLenum pname, GLint param ) { rgl.PixelStorei( pname, param ); }
inline void glGetIntegerv( GLenum pname, GLint *data ) { rgl.GetIntegerv( pname, data ); }
inline void glGetFloatv( GLenum pname, GLfloat *data ) { rgl.GetFloatv( pname, data ); }
// glEnable/glDisable live in gl_immediate.h; they filter out caps that no
// longer exist in the core profile and track the texture enable state.
inline void glDepthFunc( GLenum func ) { rgl.DepthFunc( func ); }
inline void glDepthMask( GLboolean flag ) { rgl.DepthMask( flag ); }
inline void glCullFace( GLenum mode ) { rgl.CullFace( mode ); }
inline void glPolygonMode( GLenum face, GLenum mode ) { rgl.PolygonMode( face, mode ); }
inline void glPolygonOffset( GLfloat factor, GLfloat units ) { rgl.PolygonOffset( factor, units ); }
inline void glLineWidth( GLfloat width ) { rgl.LineWidth( width ); }
inline void glScissor( GLint x, GLint y, GLsizei width, GLsizei height ) { rgl.Scissor( x, y, width, height ); }
inline void glViewport( GLint x, GLint y, GLsizei width, GLsizei height ) { rgl.Viewport( x, y, width, height ); GLim_SetViewport( x, y, width, height ); }
inline void glClear( GLbitfield mask ) { rgl.Clear( mask ); }
inline void glClearColor( GLfloat red, GLfloat green, GLfloat blue, GLfloat alpha ) { rgl.ClearColor( red, green, blue, alpha ); }
inline void glClearDepth( GLdouble depth ) { rgl.ClearDepth( depth ); }
inline void glClearStencil( GLint s ) { rgl.ClearStencil( s ); }
inline void glColorMask( GLboolean red, GLboolean green, GLboolean blue, GLboolean alpha ) { rgl.ColorMask( red, green, blue, alpha ); }
inline void glBlendFunc( GLenum sfactor, GLenum dfactor ) { rgl.BlendFunc( sfactor, dfactor ); }
inline void glStencilFunc( GLenum func, GLint ref, GLuint mask ) { rgl.StencilFunc( func, ref, mask ); }
inline void glStencilOp( GLenum fail, GLenum zfail, GLenum zpass ) { rgl.StencilOp( fail, zfail, zpass ); }
inline void glStencilFuncSeparate( GLenum face, GLenum func, GLint ref, GLuint mask ) { rgl.StencilFuncSeparate( face, func, ref, mask ); }
inline void glStencilOpSeparate( GLenum face, GLenum sfail, GLenum dpfail, GLenum dppass ) { rgl.StencilOpSeparate( face, sfail, dpfail, dppass ); }
inline void glDrawBuffer( GLenum mode ) { rgl.DrawBuffer( mode ); }
inline void glDrawBuffers( GLsizei n, const GLenum *bufs ) { rgl.DrawBuffers( n, bufs ); }
inline void glReadBuffer( GLenum mode ) { rgl.ReadBuffer( mode ); }
inline void glReadPixels( GLint x, GLint y, GLsizei width, GLsizei height, GLenum format, GLenum type, void *data ) { rgl.ReadPixels( x, y, width, height, format, type, data ); }
inline void glCopyTexImage2D( GLenum target, GLint level, GLenum internalformat, GLint x, GLint y, GLsizei width, GLsizei height, GLint border ) { rgl.CopyTexImage2D( target, level, internalformat, x, y, width, height, border ); }
inline void glFlush() { rgl.Flush(); }
inline void glFinish() { rgl.Finish(); }
inline GLenum glGetError() { return rgl.GetError(); }
inline void glActiveTexture( GLenum texture ) { rgl.ActiveTexture( texture ); }
inline const GLubyte *glGetString( GLenum name ) { return rgl.GetString( name ); }
inline const GLubyte *glGetStringi( GLenum name, GLuint index ) { return rgl.GetStringi( name, index ); }
inline void glDrawArrays( GLenum mode, GLint first, GLsizei count ) { rgl.DrawArrays( mode, first, count ); }
inline void glDrawElements( GLenum mode, GLsizei count, GLenum type, const void *indices ) { rgl.DrawElements( mode, count, type, indices ); }

// DSA style multitexture bind kept as a small helper; implemented in
// RenderSystem_init.cpp on top of glActiveTexture + glBindTexture.
void glBindMultiTextureEXT( GLenum texunit, GLenum target, GLuint texture );

inline void glUseProgram( GLuint program ) { rgl.UseProgram( program ); }
inline GLuint glCreateShader( GLenum type ) { return rgl.CreateShader( type ); }
inline void glDeleteShader( GLuint shader ) { rgl.DeleteShader( shader ); }
inline void glShaderSource( GLuint shader, GLsizei count, const GLchar **string, const GLint *length ) { rgl.ShaderSource( shader, count, const_cast<const GLchar *const *>( string ), length ); }
inline void glCompileShader( GLuint shader ) { rgl.CompileShader( shader ); }
inline void glGetShaderiv( GLuint shader, GLenum pname, GLint *params ) { rgl.GetShaderiv( shader, pname, params ); }
inline void glGetShaderInfoLog( GLuint shader, GLsizei bufSize, GLsizei *length, GLchar *infoLog ) { rgl.GetShaderInfoLog( shader, bufSize, length, infoLog ); }
inline GLuint glCreateProgram() { return rgl.CreateProgram(); }
inline void glDeleteProgram( GLuint program ) { rgl.DeleteProgram( program ); }
inline void glAttachShader( GLuint program, GLuint shader ) { rgl.AttachShader( program, shader ); }
inline void glDetachShader( GLuint program, GLuint shader ) { rgl.DetachShader( program, shader ); }
inline void glLinkProgram( GLuint program ) { rgl.LinkProgram( program ); }
inline void glGetProgramiv( GLuint program, GLenum pname, GLint *params ) { rgl.GetProgramiv( program, pname, params ); }
inline void glGetProgramInfoLog( GLuint program, GLsizei bufSize, GLsizei *length, GLchar *infoLog ) { rgl.GetProgramInfoLog( program, bufSize, length, infoLog ); }
inline void glProgramParameteri( GLuint program, GLenum pname, GLint value ) { rgl.ProgramParameteri( program, pname, value ); }
inline void glBindAttribLocation( GLuint program, GLuint index, const GLchar *name ) { rgl.BindAttribLocation( program, index, name ); }
inline void glBindFragDataLocation( GLuint program, GLuint colorNumber, const GLchar *name ) { rgl.BindFragDataLocation( program, colorNumber, name ); }
inline GLint glGetUniformLocation( GLuint program, const GLchar *name ) { return rgl.GetUniformLocation( program, name ); }
inline void glUniform1i( GLint location, GLint v0 ) { rgl.Uniform1i( location, v0 ); }
inline void glUniform1f( GLint location, GLfloat v0 ) { rgl.Uniform1f( location, v0 ); }
inline void glUniform4fv( GLint location, GLsizei count, const GLfloat *value ) { rgl.Uniform4fv( location, count, value ); }
inline void glUniformMatrix4fv( GLint location, GLsizei count, GLboolean transpose, const GLfloat *value ) { rgl.UniformMatrix4fv( location, count, transpose, value ); }
inline GLuint glGetUniformBlockIndex( GLuint program, const GLchar *uniformBlockName ) { return rgl.GetUniformBlockIndex( program, uniformBlockName ); }
inline void glUniformBlockBinding( GLuint program, GLuint uniformBlockIndex, GLuint uniformBlockBinding ) { rgl.UniformBlockBinding( program, uniformBlockIndex, uniformBlockBinding ); }

inline void glGenFramebuffers( GLsizei n, GLuint *ids ) { rgl.GenFramebuffers( n, ids ); }
inline void glBindFramebuffer( GLenum target, GLuint framebuffer ) { rgl.BindFramebuffer( target, framebuffer ); }
inline void glDeleteFramebuffers( GLsizei n, const GLuint *framebuffers ) { rgl.DeleteFramebuffers( n, framebuffers ); }
inline void glFramebufferTexture2D( GLenum target, GLenum attachment, GLenum textarget, GLuint texture, GLint level ) { rgl.FramebufferTexture2D( target, attachment, textarget, texture, level ); }
inline GLenum glCheckFramebufferStatus( GLenum target ) { return rgl.CheckFramebufferStatus( target ); }
inline void glBlitFramebuffer( GLint srcX0, GLint srcY0, GLint srcX1, GLint srcY1, GLint dstX0, GLint dstY0, GLint dstX1, GLint dstY1, GLbitfield mask, GLenum filter ) { rgl.BlitFramebuffer( srcX0, srcY0, srcX1, srcY1, dstX0, dstY0, dstX1, dstY1, mask, filter ); }

inline void glGenBuffers( GLsizei n, GLuint *buffers ) { rgl.GenBuffers( n, buffers ); }
inline void glBindBuffer( GLenum target, GLuint buffer ) { rgl.BindBuffer( target, buffer ); }
inline void glBindBufferRange( GLenum target, GLuint index, GLuint buffer, GLintptr offset, GLsizeiptr size ) { rgl.BindBufferRange( target, index, buffer, offset, size ); }
inline void glDeleteBuffers( GLsizei n, const GLuint *buffers ) { rgl.DeleteBuffers( n, buffers ); }
inline void glBufferData( GLenum target, GLsizeiptr size, const void *data, GLenum usage ) { rgl.BufferData( target, size, data, usage ); }
inline void glBufferSubData( GLenum target, GLintptr offset, GLsizeiptr size, const void *data ) { rgl.BufferSubData( target, offset, size, data ); }
inline void glGetBufferSubData( GLenum target, GLintptr offset, GLsizeiptr size, void *data ) { rgl.GetBufferSubData( target, offset, size, data ); }
inline void *glMapBuffer( GLenum target, GLenum access ) { return rgl.MapBuffer( target, access ); }
inline GLboolean glUnmapBuffer( GLenum target ) { return rgl.UnmapBuffer( target ); }
inline void glGetBufferParameteriv( GLenum target, GLenum pname, GLint *params ) { rgl.GetBufferParameteriv( target, pname, params ); }
inline void glGetBufferPointerv( GLenum target, GLenum pname, void **params ) { rgl.GetBufferPointerv( target, pname, params ); }
inline void *glMapBufferRange( GLenum target, GLintptr offset, GLintptr length, GLbitfield access ) { return rgl.MapBufferRange( target, offset, length, access ); }

inline void glGenVertexArrays( GLsizei n, GLuint *arrays ) { rgl.GenVertexArrays( n, arrays ); }
inline void glBindVertexArray( GLuint array ) { rgl.BindVertexArray( array ); }
inline void glDeleteVertexArrays( GLsizei n, const GLuint *arrays ) { rgl.DeleteVertexArrays( n, arrays ); }
inline void glVertexAttribPointer( GLuint index, GLint size, GLenum type, GLboolean normalized, GLsizei stride, const void *pointer ) { rgl.VertexAttribPointer( index, size, type, normalized, stride, pointer ); }
inline void glEnableVertexAttribArray( GLuint index ) { rgl.EnableVertexAttribArray( index ); }
inline void glDisableVertexAttribArray( GLuint index ) { rgl.DisableVertexAttribArray( index ); }
inline void glDrawElementsBaseVertex( GLenum mode, GLsizei count, GLenum type, const void *indices, GLint basevertex ) { rgl.DrawElementsBaseVertex( mode, count, type, indices, basevertex ); }

inline GLsync glFenceSync( GLenum condition, GLbitfield flags ) { return rgl.FenceSync( condition, flags ); }
inline GLboolean glIsSync( GLsync sync ) { return rgl.IsSync( sync ); }
inline GLenum glClientWaitSync( GLsync sync, GLbitfield flags, GLuint64 timeout ) { return rgl.ClientWaitSync( sync, flags, timeout ); }
inline void glDeleteSync( GLsync sync ) { rgl.DeleteSync( sync ); }

inline void glGenQueries( GLsizei n, GLuint *ids ) { rgl.GenQueries( n, ids ); }
inline void glDeleteQueries( GLsizei n, const GLuint *ids ) { rgl.DeleteQueries( n, ids ); }
inline GLboolean glIsQuery( GLuint id ) { return rgl.IsQuery( id ); }
inline void glBeginQuery( GLenum target, GLuint id ) { rgl.BeginQuery( target, id ); }
inline void glEndQuery( GLenum target ) { rgl.EndQuery( target ); }
inline void glGetQueryiv( GLenum target, GLenum pname, GLint *params ) { rgl.GetQueryiv( target, pname, params ); }
inline void glGetQueryObjectiv( GLuint id, GLenum pname, GLint *params ) { rgl.GetQueryObjectiv( id, pname, params ); }
inline void glGetQueryObjectuiv( GLuint id, GLenum pname, GLuint *params ) { rgl.GetQueryObjectuiv( id, pname, params ); }
inline void glGetQueryObjectui64v( GLuint id, GLenum pname, GLuint64 *params ) { rgl.GetQueryObjectui64v( id, pname, params ); }

// ARB_debug_output, still the practical debug channel on GL 3.3 drivers.
inline void glDebugMessageControlARB( GLenum source, GLenum type, GLenum severity, GLsizei count, const GLuint *ids, GLboolean enabled ) { rgl.DebugMessageControlARB( source, type, severity, count, ids, enabled ); }
inline void glDebugMessageInsertARB( GLenum source, GLenum type, GLuint id, GLenum severity, GLsizei length, const GLchar *buf ) { rgl.DebugMessageInsertARB( source, type, id, severity, length, buf ); }
inline void glDebugMessageCallbackARB( void ( APIENTRY *callback )( GLenum source, GLenum type, GLuint id, GLenum severity, GLsizei length, const GLchar *message, const void *userParam ), const void *userParam ) { rgl.DebugMessageCallbackARB( callback, userParam ); }
inline GLuint glGetDebugMessageLogARB( GLuint count, GLsizei bufsize, GLenum *sources, GLenum *types, GLuint *ids, GLenum *severities, GLsizei *lengths, GLchar *messageLog ) { return rgl.GetDebugMessageLogARB( count, bufsize, sources, types, ids, severities, lengths, messageLog ); }

// EXT_depth_bounds_test, optional but useful for shadow draws.
inline void glDepthBoundsEXT( GLfloat zmin, GLfloat zmax ) { rgl.DepthBoundsEXT( zmin, zmax ); }

#endif // GL_CORE_H
