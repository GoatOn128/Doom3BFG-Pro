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
#ifndef __GRAPHICSAPIWRAPPER_H__
#define __GRAPHICSAPIWRAPPER_H__

#include "ScreenRect.h"

/*
================================================================================================

	Graphics API wrapper/helper functions

	This wraps platform specific graphics API functionality that is used at run-time. This
	functionality is wrapped to avoid excessive conditional compilation and/or code duplication
	throughout the run-time rendering code that is shared on all platforms.

	Most other graphics API functions are called for initialization purposes and are called
	directly from platform specific code implemented in files in the platform specific folders:

	renderer/OpenGL/
	renderer/DirectX/
	renderer/GCM/

================================================================================================
*/

class idImage;
//class idTriangles;
class idRenderModelSurface;
class idDeclRenderProg;
class idRenderTexture;

static const int MAX_OCCLUSION_QUERIES = 4096;
// returned by GL_GetDeferredQueryResult() when the query is from too long ago and the result is no longer available
static const int OCCLUSION_QUERY_TOO_OLD				= -1;

/*
================================================================================================

	Platform Specific Context

================================================================================================
*/




#define USE_CORE_PROFILE

struct wrapperContext_t {
};


/*
================================================
wrapperConfig_t
================================================
*/
struct wrapperConfig_t {
	// rendering options and settings
	bool			disableStateCaching;
	bool			lazyBindPrograms;
	bool			lazyBindParms;
	bool			lazyBindTextures;
	bool			stripFragmentBranches;
	bool			skipDetailTris;
	bool			singleTriangle;
	// values for polygon offset
	float			polyOfsFactor;
	float			polyOfsUnits;
	// global texture filter settings
	int				textureMinFilter;
	int				textureMaxFilter;
	int				textureMipFilter;
	float			textureAnisotropy;
	float			textureLODBias;
};

/*
================================================
wrapperStats_t
================================================
*/
struct wrapperStats_t {
	int				c_queriesIssued;
	int				c_queriesPassed;
	int				c_queriesWaitTime;
	int				c_queriesTooOld;
	int				c_programsBound;
	int				c_drawElements;
	int				c_drawIndices;
	int				c_drawVertices;
};

/*
================================================================================================

	API

================================================================================================
*/

void			GL_SetWrapperContext( const wrapperContext_t & context );
void			GL_SetWrapperConfig( const wrapperConfig_t & config );

void			GL_SetTimeDelta( uint64 delta );	// delta from GPU to CPU microseconds
void			GL_StartFrame( int frame );			// inserts a timing mark for the start of the GPU frame
void			GL_EndFrame();						// inserts a timing mark for the end of the GPU frame
void			GL_WaitForEndFrame();				// wait for the GPU to reach the last end frame marker
void			GL_GetLastFrameTime( uint64 & startGPUTimeMicroSec, uint64 & endGPUTimeMicroSec );	// GPU time between GL_StartFrame() and GL_EndFrame()
void			GL_StartDepthPass( const idScreenRect & rect );
void			GL_FinishDepthPass();
void			GL_GetDepthPassRect( idScreenRect & rect );

void			GL_SetDefaultState();
void			GL_State( uint64 stateVector, bool forceGlState = false );
uint64			GL_GetCurrentState();
uint64			GL_GetCurrentStateMinusStencil();
void			GL_Cull( int cullType );
void			GL_Scissor( int x /* left*/, int y /* bottom */, int w, int h );
void			GL_Viewport( int x /* left */, int y /* bottom */, int w, int h );
ID_INLINE void	GL_Scissor( const idScreenRect & rect ) { GL_Scissor( rect.x1, rect.y1, rect.x2 - rect.x1 + 1, rect.y2 - rect.y1 + 1 ); }
ID_INLINE void	GL_Viewport( const idScreenRect & rect ) { GL_Viewport( rect.x1, rect.y1, rect.x2 - rect.x1 + 1, rect.y2 - rect.y1 + 1 ); }
ID_INLINE void	GL_ViewportAndScissor( int x, int y, int w, int h ) { GL_Viewport( x, y, w, h ); GL_Scissor( x, y, w, h ); }
ID_INLINE void	GL_ViewportAndScissor( const idScreenRect& rect ) { GL_Viewport( rect ); GL_Scissor( rect ); }
void			GL_Clear( bool color, bool depth, bool stencil, byte stencilValue, float r, float g, float b, float a );
void			GL_PolygonOffset( float scale, float bias );
void			GL_DepthBoundsTest( const float zmin, const float zmax );
void			GL_Color( float * color );
void			GL_Color( float r, float g, float b );
void			GL_Color( float r, float g, float b, float a );
void			GL_SelectTexture( int unit );

void			GL_Flush();		// flush the GPU command buffer
void			GL_Finish();	// wait for the GPU to have executed all commands
void			GL_CheckErrors();

wrapperStats_t	GL_GetCurrentStats();
void			GL_ClearStats();



// Buffer object entry points. Targets, usage and map flags use wrapper enums
// so front end code does not need GL names.
enum wrapperBufferTarget_t {
	BUFFER_TARGET_ARRAY,
	BUFFER_TARGET_ELEMENT,
	BUFFER_TARGET_UNIFORM,
	BUFFER_TARGET_PIXEL_PACK,
	BUFFER_TARGET_PIXEL_UNPACK
};

enum wrapperBufferUsage_t {
	BUFFER_USAGE_STATIC_DRAW,
	BUFFER_USAGE_DYNAMIC_DRAW,
	BUFFER_USAGE_STREAM_DRAW,
	BUFFER_USAGE_STREAM_READ
};

enum wrapperBufferMapFlags_t {
	BUFFER_MAP_READ				= BIT( 0 ),
	BUFFER_MAP_WRITE			= BIT( 1 ),
	BUFFER_MAP_INVALIDATE_RANGE	= BIT( 2 ),
	BUFFER_MAP_UNSYNCHRONIZED	= BIT( 3 )
};

void			GL_BindBuffer( wrapperBufferTarget_t target, unsigned int buffer );
void			GL_GenBuffers( int count, unsigned int * buffers );
void			GL_DeleteBuffers( int count, const unsigned int * buffers );
void			GL_BufferData( wrapperBufferTarget_t target, intptr_t size, const void * data, wrapperBufferUsage_t usage );
void			GL_BufferSubData( wrapperBufferTarget_t target, intptr_t offset, intptr_t size, const void * data );
void *			GL_MapBufferRange( wrapperBufferTarget_t target, intptr_t offset, intptr_t length, unsigned int flags );
bool			GL_UnmapBuffer( wrapperBufferTarget_t target );
unsigned int	GL_GetError();
void			GL_DeleteProgram( unsigned int program );


// Capability toggles for the fixed pipeline states the renderer uses.
enum wrapperCap_t {
	CAP_TEXTURE_CUBE_MAP_SEAMLESS,
	CAP_FRAMEBUFFER_SRGB,
	CAP_MULTISAMPLE,
	CAP_DEBUG_OUTPUT_SYNCHRONOUS,
	CAP_SCISSOR_TEST
};
void			GL_Enable( wrapperCap_t cap );
void			GL_Disable( wrapperCap_t cap );
bool			GL_IsEnabled( wrapperCap_t cap );

// Timer queries.
enum wrapperQueryTarget_t {
	QUERY_TARGET_TIME_ELAPSED
};
void			GL_GenQueries( int count, unsigned int * ids );
void			GL_BeginQuery( wrapperQueryTarget_t target, unsigned int id );
void			GL_EndQuery( wrapperQueryTarget_t target );
void			GL_GetQueryObjectui64v( unsigned int id, unsigned long long * params );

// Texture binding. Units are zero based; GL_GetIntegerv( GET_ACTIVE_TEXTURE )
// returns the zero based unit so it can be restored with GL_ActiveTexture.
enum wrapperTexTarget_t {
	TEX_TARGET_2D,
	TEX_TARGET_CUBE
};
void			GL_ActiveTexture( int unit );
void			GL_BindMultiTexture( int unit, wrapperTexTarget_t target, unsigned int texture );
void			GL_BindTexture( wrapperTexTarget_t target, unsigned int texture );
enum wrapperTexParam_t {
	TEXPARAM_COMPARE_MODE,
	TEXPARAM_COMPARE_FUNC
};
// Raw GL compare values, from the GL specification.
const int		TEX_COMPARE_REF_TO_TEXTURE = 0x884C;
const int		TEX_COMPARE_LEQUAL = 0x0203;
void			GL_TexParameteri( wrapperTexTarget_t target, wrapperTexParam_t pname, int value );

// Framebuffer objects.
enum wrapperAttachment_t {
	ATTACHMENT_COLOR0,
	ATTACHMENT_COLOR1,
	ATTACHMENT_COLOR2,
	ATTACHMENT_COLOR3,
	ATTACHMENT_DEPTH,
	ATTACHMENT_STENCIL
};
void			GL_GenFramebuffers( int count, unsigned int * ids );
void			GL_DeleteFramebuffers( int count, const unsigned int * ids );
void			GL_BindFramebuffer( unsigned int fbo );
void			GL_FramebufferTexture2D( wrapperAttachment_t attachment, wrapperTexTarget_t texTarget, unsigned int texture, int level );
bool			GL_CheckFramebufferStatus();

// Draw and read buffer selection. Values are opaque handles as returned by
// GL_GetIntegerv( GET_DRAW_BUFFER / GET_READ_BUFFER ).
const int		BUFFER_SELECT_NONE = 0;
const int		BUFFER_SELECT_FRONT = 0x0404;	// GL_FRONT
const int		BUFFER_SELECT_BACK = 0x0405;	// GL_BACK
const int		BUFFER_SELECT_COLOR_ATTACHMENT0 = 0x8CE0;	// GL_COLOR_ATTACHMENT0
void			GL_DrawBuffer( int mode );
void			GL_ReadBuffer( int mode );

// State queries.
enum wrapperGetPname_t {
	GET_ACTIVE_TEXTURE,
	GET_FRAMEBUFFER_BINDING,
	GET_DRAW_BUFFER,
	GET_READ_BUFFER,
	GET_VIEWPORT,
	GET_SCISSOR_BOX,
	GET_MAX_TEXTURE_SIZE,
	GET_MAX_TEXTURE_IMAGE_UNITS,
	GET_UNIFORM_BUFFER_OFFSET_ALIGNMENT,
	GET_NUM_EXTENSIONS,
	GET_MAX_TEXTURE_MAX_ANISOTROPY
};
void			GL_GetIntegerv( wrapperGetPname_t pname, int * params );
void			GL_GetFloatv( wrapperGetPname_t pname, float * params );
enum wrapperGetStringName_t {
	STRING_VENDOR,
	STRING_RENDERER,
	STRING_VERSION,
	STRING_SHADING_LANGUAGE_VERSION,
	STRING_EXTENSIONS
};
const char *	GL_GetString( wrapperGetStringName_t name );
const char *	GL_GetStringi( int index );

// Pixel readback.
enum wrapperPixelFormat_t {
	PIXELFORMAT_RGB,
	PIXELFORMAT_RGBA,
	PIXELFORMAT_DEPTH_FLOAT,
	PIXELFORMAT_STENCIL_INDEX
};
void			GL_ReadPixels( int x, int y, int width, int height, wrapperPixelFormat_t format, void * data );

// Vertex layout and draws for the global VAO path.
enum wrapperVertexAttribType_t {
	VERTEX_ATTRIB_FLOAT,
	VERTEX_ATTRIB_UBYTE,
	VERTEX_ATTRIB_HALF_FLOAT
};
void			GL_EnableVertexAttribArray( unsigned int index );
void			GL_VertexAttribPointer( unsigned int index, int size, wrapperVertexAttribType_t type, bool normalized, int stride, intptr_t offset );
void			GL_DrawElementsBaseVertex( int count, const void * indices, int baseVertex );

// Misc state.
void			GL_BindBufferRange( wrapperBufferTarget_t target, unsigned int index, unsigned int buffer, intptr_t offset, intptr_t size );
void			GL_BindVertexArray( unsigned int vao );
void			GL_GenVertexArrays( int count, unsigned int * ids );
void			GL_UseProgram( unsigned int program );
void			GL_Uniform4fv( int location, int count, const float * value );
void			GL_ColorMask( bool r, bool g, bool b, bool a );
void			GL_DepthMask( bool enable );
void			GL_ClearColor( float r, float g, float b, float a );
void			GL_ClearDepth( double depth );

// Debug output control: dont-care source and type, low severity messages.
typedef void ( *GLDebugMessageProc_t )( unsigned int source, unsigned int type, unsigned int id, unsigned int severity, int length, const char * message, const void * userParam );
void			GL_DebugMessageCallback( GLDebugMessageProc_t callback, const void * userParam );
void			GL_DebugMessageControl( bool enabled );

#endif // !__GRAPHICSAPIWRAPPER_H__
