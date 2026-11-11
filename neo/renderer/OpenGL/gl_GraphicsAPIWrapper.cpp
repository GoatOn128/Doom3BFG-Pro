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

#include "../tr_local.h"

/*
====================
GL_SelectTexture
====================
*/
void GL_SelectTexture( int unit ) {
	if ( backEnd.glState.currenttmu == unit ) {
		return;
	}

	if ( unit < 0 || unit >= glConfig.maxTextureImageUnits ) {
		common->Warning( "GL_SelectTexture: unit = %i", unit );
		return;
	}

	RENDERLOG_PRINTF( "GL_SelectTexture( %i );\n", unit );

	backEnd.glState.currenttmu = unit;
}

/*
====================
GL_Cull

This handles the flipping needed when the view being
rendered is a mirored view.
====================
*/
void GL_Cull( int cullType ) {
	if ( backEnd.glState.faceCulling == cullType ) {
		return;
	}

	if ( cullType == CT_TWO_SIDED ) {
		glDisable( GL_CULL_FACE );
	} else  {
		if ( backEnd.glState.faceCulling == CT_TWO_SIDED ) {
			glEnable( GL_CULL_FACE );
		}

		if ( cullType == CT_BACK_SIDED ) {
			if ( backEnd.viewDef->isMirror ) {
				glCullFace( GL_FRONT );
			} else {
				glCullFace( GL_BACK );
			}
		} else {
			if ( backEnd.viewDef->isMirror ) {
				glCullFace( GL_BACK );
			} else {
				glCullFace( GL_FRONT );
			}
		}
	}

	backEnd.glState.faceCulling = cullType;
}

/*
====================
GL_Scissor
====================
*/
void GL_Scissor( int x /* left*/, int y /* bottom */, int w, int h ) {
	glScissor( x, y, w, h );
}

/*
====================
GL_Viewport
====================
*/
void GL_Viewport( int x /* left */, int y /* bottom */, int w, int h ) {
	glViewport( x, y, w, h );
}

/*
====================
GL_PolygonOffset
====================
*/
void GL_PolygonOffset( float scale, float bias ) {
	backEnd.glState.polyOfsScale = scale;
	backEnd.glState.polyOfsBias = bias;
	if ( backEnd.glState.glStateBits & GLS_POLYGON_OFFSET ) {
		glPolygonOffset( scale, bias );
	}
}

/*
========================
GL_DepthBoundsTest
========================
*/
void GL_DepthBoundsTest( const float zmin, const float zmax ) {
	if ( !glConfig.depthBoundsTestAvailable || zmin > zmax ) {
		return;
	}

	if ( zmin == 0.0f && zmax == 0.0f ) {
		glDisable( GL_DEPTH_BOUNDS_TEST_EXT );
	} else {
		glEnable( GL_DEPTH_BOUNDS_TEST_EXT );
		glDepthBoundsEXT( zmin, zmax );
	}
}

/*
========================
GL_StartDepthPass
========================
*/
void GL_StartDepthPass( const idScreenRect & rect ) {
}

/*
========================
GL_FinishDepthPass
========================
*/
void GL_FinishDepthPass() {
}

/*
========================
GL_Flush
========================
*/
void GL_Flush() {
	glFlush();
}

/*
========================
GL_Finish
========================
*/
void GL_Finish() {
	glFinish();
}

/*
========================
GL_GetDepthPassRect
========================
*/
void GL_GetDepthPassRect( idScreenRect & rect ) {
	rect.Clear();
}

/*
====================
GL_Color
====================
*/
void GL_Color( float * color ) {
	if ( color == NULL ) {
		return;
	}
	GL_Color( color[0], color[1], color[2], color[3] );
}

/*
====================
GL_Color
====================
*/
void GL_Color( float r, float g, float b ) {
	GL_Color( r, g, b, 1.0f );
}

/*
====================
GL_Color
====================
*/
void GL_Color( float r, float g, float b, float a ) {
	float parm[4];
	parm[0] = idMath::ClampFloat( 0.0f, 1.0f, r );
	parm[1] = idMath::ClampFloat( 0.0f, 1.0f, g );
	parm[2] = idMath::ClampFloat( 0.0f, 1.0f, b );
	parm[3] = idMath::ClampFloat( 0.0f, 1.0f, a );
	renderProgManager.SetRenderParm( RENDERPARM_COLOR, parm );
}

/*
========================
GL_Clear
========================
*/
void GL_Clear( bool color, bool depth, bool stencil, byte stencilValue, float r, float g, float b, float a ) {
	int clearFlags = 0;
	if ( color ) {
		glClearColor( r, g, b, a );
		clearFlags |= GL_COLOR_BUFFER_BIT;
	}
	if ( depth ) {
		clearFlags |= GL_DEPTH_BUFFER_BIT;
	}
	if ( stencil ) {
		glClearStencil( stencilValue );
		clearFlags |= GL_STENCIL_BUFFER_BIT;
	}
	glClear( clearFlags );
}

/*
========================
GL_SetDefaultState

This should initialize all GL state that any part of the entire program
may touch, including the editor.
========================
*/
void GL_SetDefaultState() {
	RENDERLOG_PRINTF( "--- GL_SetDefaultState ---\n" );

	glClearDepth( 1.0f );

	// make sure our GL state vector is set correctly
	memset( &backEnd.glState, 0, sizeof( backEnd.glState ) );
	GL_State( 0, true );

	// These are changed by GL_Cull
	glCullFace( GL_FRONT_AND_BACK );
	glEnable( GL_CULL_FACE );

	// These are changed by GL_State
	glColorMask( GL_TRUE, GL_TRUE, GL_TRUE, GL_TRUE );
	glBlendFunc( GL_ONE, GL_ZERO );
	glDepthMask( GL_TRUE );
	glDepthFunc( GL_LESS );
	glDisable( GL_STENCIL_TEST );
	glDisable( GL_POLYGON_OFFSET_FILL );
	glDisable( GL_POLYGON_OFFSET_LINE );
	glPolygonMode( GL_FRONT_AND_BACK, GL_FILL );

	// These should never be changed
	glEnable( GL_DEPTH_TEST );
	glEnable( GL_BLEND );
	glEnable( GL_SCISSOR_TEST );
	glDrawBuffer( GL_BACK );
	glReadBuffer( GL_BACK );

	if ( r_useScissor.GetBool() ) {
		glScissor( 0, 0, renderSystem->GetWidth(), renderSystem->GetHeight() );
	}
}

/*
====================
GL_State

This routine is responsible for setting the most commonly changed state
====================
*/
void GL_State( uint64 stateBits, bool forceGlState ) {
	uint64 diff = stateBits ^ backEnd.glState.glStateBits;
	
	if ( !r_useStateCaching.GetBool() || forceGlState ) {
		// make sure everything is set all the time, so we
		// can see if our delta checking is screwing up
		diff = 0xFFFFFFFFFFFFFFFF;
	} else if ( diff == 0 ) {
		return;
	}

	//
	// check depthFunc bits
	//
	if ( diff & GLS_DEPTHFUNC_BITS ) {
		switch ( stateBits & GLS_DEPTHFUNC_BITS ) {
			case GLS_DEPTHFUNC_EQUAL:	glDepthFunc( GL_EQUAL ); break;
			case GLS_DEPTHFUNC_ALWAYS:	glDepthFunc( GL_ALWAYS ); break;
			case GLS_DEPTHFUNC_LESS:	glDepthFunc( GL_LEQUAL ); break;
			case GLS_DEPTHFUNC_GREATER:	glDepthFunc( GL_GEQUAL ); break;
		}
	}

	//
	// check blend bits
	//
	if ( diff & ( GLS_SRCBLEND_BITS | GLS_DSTBLEND_BITS ) ) {
		GLenum srcFactor = GL_ONE;
		GLenum dstFactor = GL_ZERO;

		switch ( stateBits & GLS_SRCBLEND_BITS ) {
			case GLS_SRCBLEND_ZERO:					srcFactor = GL_ZERO; break;
			case GLS_SRCBLEND_ONE:					srcFactor = GL_ONE; break;
			case GLS_SRCBLEND_DST_COLOR:			srcFactor = GL_DST_COLOR; break;
			case GLS_SRCBLEND_ONE_MINUS_DST_COLOR:	srcFactor = GL_ONE_MINUS_DST_COLOR; break;
			case GLS_SRCBLEND_SRC_ALPHA:			srcFactor = GL_SRC_ALPHA; break;
			case GLS_SRCBLEND_ONE_MINUS_SRC_ALPHA:	srcFactor = GL_ONE_MINUS_SRC_ALPHA; break;
			case GLS_SRCBLEND_DST_ALPHA:			srcFactor = GL_DST_ALPHA; break;
			case GLS_SRCBLEND_ONE_MINUS_DST_ALPHA:	srcFactor = GL_ONE_MINUS_DST_ALPHA; break;
			default:
				assert( !"GL_State: invalid src blend state bits\n" );
				break;
		}

		switch ( stateBits & GLS_DSTBLEND_BITS ) {
			case GLS_DSTBLEND_ZERO:					dstFactor = GL_ZERO; break;
			case GLS_DSTBLEND_ONE:					dstFactor = GL_ONE; break;
			case GLS_DSTBLEND_SRC_COLOR:			dstFactor = GL_SRC_COLOR; break;
			case GLS_DSTBLEND_ONE_MINUS_SRC_COLOR:	dstFactor = GL_ONE_MINUS_SRC_COLOR; break;
			case GLS_DSTBLEND_SRC_ALPHA:			dstFactor = GL_SRC_ALPHA; break;
			case GLS_DSTBLEND_ONE_MINUS_SRC_ALPHA:	dstFactor = GL_ONE_MINUS_SRC_ALPHA; break;
			case GLS_DSTBLEND_DST_ALPHA:			dstFactor = GL_DST_ALPHA; break;
			case GLS_DSTBLEND_ONE_MINUS_DST_ALPHA:  dstFactor = GL_ONE_MINUS_DST_ALPHA; break;
			default:
				assert( !"GL_State: invalid dst blend state bits\n" );
				break;
		}

		// Only actually update GL's blend func if blending is enabled.
		if ( srcFactor == GL_ONE && dstFactor == GL_ZERO ) {
			glDisable( GL_BLEND );
		} else {
			glEnable( GL_BLEND );
			glBlendFunc( srcFactor, dstFactor );
		}
	}

	//
	// check depthmask
	//
	if ( diff & GLS_DEPTHMASK ) {
		if ( stateBits & GLS_DEPTHMASK ) {
			glDepthMask( GL_FALSE );
		} else {
			glDepthMask( GL_TRUE );
		}
	}

	//
	// check colormask
	//
	if ( diff & (GLS_REDMASK|GLS_GREENMASK|GLS_BLUEMASK|GLS_ALPHAMASK) ) {
		GLboolean r = ( stateBits & GLS_REDMASK ) ? GL_FALSE : GL_TRUE;
		GLboolean g = ( stateBits & GLS_GREENMASK ) ? GL_FALSE : GL_TRUE;
		GLboolean b = ( stateBits & GLS_BLUEMASK ) ? GL_FALSE : GL_TRUE;
		GLboolean a = ( stateBits & GLS_ALPHAMASK ) ? GL_FALSE : GL_TRUE;
		glColorMask( r, g, b, a );
	}

	//
	// fill/line mode
	//
	if ( diff & GLS_POLYMODE_LINE ) {
		if ( stateBits & GLS_POLYMODE_LINE ) {
			glPolygonMode( GL_FRONT_AND_BACK, GL_LINE );
		} else {
			glPolygonMode( GL_FRONT_AND_BACK, GL_FILL );
		}
	}

	//
	// polygon offset
	//
	if ( diff & GLS_POLYGON_OFFSET ) {
		if ( stateBits & GLS_POLYGON_OFFSET ) {
			glPolygonOffset( backEnd.glState.polyOfsScale, backEnd.glState.polyOfsBias );
			glEnable( GL_POLYGON_OFFSET_FILL );
			glEnable( GL_POLYGON_OFFSET_LINE );
		} else {
			glDisable( GL_POLYGON_OFFSET_FILL );
			glDisable( GL_POLYGON_OFFSET_LINE );
		}
	}

	//
	// stencil
	//
	if ( diff & ( GLS_STENCIL_FUNC_BITS | GLS_STENCIL_OP_BITS ) ) {
		if ( ( stateBits & ( GLS_STENCIL_FUNC_BITS | GLS_STENCIL_OP_BITS ) ) != 0 ) {
			glEnable( GL_STENCIL_TEST );
		} else {
			glDisable( GL_STENCIL_TEST );
		}
	}
	if ( diff & ( GLS_STENCIL_FUNC_BITS | GLS_STENCIL_FUNC_REF_BITS | GLS_STENCIL_FUNC_MASK_BITS ) ) {
		GLuint ref = GLuint( ( stateBits & GLS_STENCIL_FUNC_REF_BITS ) >> GLS_STENCIL_FUNC_REF_SHIFT );
		GLuint mask = GLuint( ( stateBits & GLS_STENCIL_FUNC_MASK_BITS ) >> GLS_STENCIL_FUNC_MASK_SHIFT );
		GLenum func = 0;

		switch ( stateBits & GLS_STENCIL_FUNC_BITS ) {
			case GLS_STENCIL_FUNC_NEVER:		func = GL_NEVER; break;
			case GLS_STENCIL_FUNC_LESS:			func = GL_LESS; break;
			case GLS_STENCIL_FUNC_EQUAL:		func = GL_EQUAL; break;
			case GLS_STENCIL_FUNC_LEQUAL:		func = GL_LEQUAL; break;
			case GLS_STENCIL_FUNC_GREATER:		func = GL_GREATER; break;
			case GLS_STENCIL_FUNC_NOTEQUAL:		func = GL_NOTEQUAL; break;
			case GLS_STENCIL_FUNC_GEQUAL:		func = GL_GEQUAL; break;
			case GLS_STENCIL_FUNC_ALWAYS:		func = GL_ALWAYS; break;
		}
		glStencilFunc( func, ref, mask );
	}
	if ( diff & ( GLS_STENCIL_OP_FAIL_BITS | GLS_STENCIL_OP_ZFAIL_BITS | GLS_STENCIL_OP_PASS_BITS ) ) {
		GLenum sFail = 0;
		GLenum zFail = 0;
		GLenum pass = 0;

		switch ( stateBits & GLS_STENCIL_OP_FAIL_BITS ) {
			case GLS_STENCIL_OP_FAIL_KEEP:		sFail = GL_KEEP; break;
			case GLS_STENCIL_OP_FAIL_ZERO:		sFail = GL_ZERO; break;
			case GLS_STENCIL_OP_FAIL_REPLACE:	sFail = GL_REPLACE; break;
			case GLS_STENCIL_OP_FAIL_INCR:		sFail = GL_INCR; break;
			case GLS_STENCIL_OP_FAIL_DECR:		sFail = GL_DECR; break;
			case GLS_STENCIL_OP_FAIL_INVERT:	sFail = GL_INVERT; break;
			case GLS_STENCIL_OP_FAIL_INCR_WRAP: sFail = GL_INCR_WRAP; break;
			case GLS_STENCIL_OP_FAIL_DECR_WRAP: sFail = GL_DECR_WRAP; break;
		}
		switch ( stateBits & GLS_STENCIL_OP_ZFAIL_BITS ) {
			case GLS_STENCIL_OP_ZFAIL_KEEP:		zFail = GL_KEEP; break;
			case GLS_STENCIL_OP_ZFAIL_ZERO:		zFail = GL_ZERO; break;
			case GLS_STENCIL_OP_ZFAIL_REPLACE:	zFail = GL_REPLACE; break;
			case GLS_STENCIL_OP_ZFAIL_INCR:		zFail = GL_INCR; break;
			case GLS_STENCIL_OP_ZFAIL_DECR:		zFail = GL_DECR; break;
			case GLS_STENCIL_OP_ZFAIL_INVERT:	zFail = GL_INVERT; break;
			case GLS_STENCIL_OP_ZFAIL_INCR_WRAP:zFail = GL_INCR_WRAP; break;
			case GLS_STENCIL_OP_ZFAIL_DECR_WRAP:zFail = GL_DECR_WRAP; break;
		}
		switch ( stateBits & GLS_STENCIL_OP_PASS_BITS ) {
			case GLS_STENCIL_OP_PASS_KEEP:		pass = GL_KEEP; break;
			case GLS_STENCIL_OP_PASS_ZERO:		pass = GL_ZERO; break;
			case GLS_STENCIL_OP_PASS_REPLACE:	pass = GL_REPLACE; break;
			case GLS_STENCIL_OP_PASS_INCR:		pass = GL_INCR; break;
			case GLS_STENCIL_OP_PASS_DECR:		pass = GL_DECR; break;
			case GLS_STENCIL_OP_PASS_INVERT:	pass = GL_INVERT; break;
			case GLS_STENCIL_OP_PASS_INCR_WRAP:	pass = GL_INCR_WRAP; break;
			case GLS_STENCIL_OP_PASS_DECR_WRAP:	pass = GL_DECR_WRAP; break;
		}
		glStencilOp( sFail, zFail, pass );
	}

	backEnd.glState.glStateBits = stateBits;
}

/*
=================
GL_GetCurrentState
=================
*/
uint64 GL_GetCurrentState() {
	return backEnd.glState.glStateBits;
}

/*
========================
GL_GetCurrentStateMinusStencil
========================
*/
uint64 GL_GetCurrentStateMinusStencil() {
	return GL_GetCurrentState() & ~(GLS_STENCIL_OP_BITS|GLS_STENCIL_FUNC_BITS|GLS_STENCIL_FUNC_REF_BITS|GLS_STENCIL_FUNC_MASK_BITS);
}

static GLenum BufferTargetGL( wrapperBufferTarget_t target ) {
	static const GLenum targets[] = { GL_ARRAY_BUFFER, GL_ELEMENT_ARRAY_BUFFER, GL_UNIFORM_BUFFER, GL_PIXEL_PACK_BUFFER, GL_PIXEL_UNPACK_BUFFER };
	return targets[target];
}

/*
========================
GL_BindBuffer
========================
*/
void GL_BindBuffer( wrapperBufferTarget_t target, unsigned int buffer ) {
	static const GLenum targets[] = { GL_ARRAY_BUFFER, GL_ELEMENT_ARRAY_BUFFER, GL_UNIFORM_BUFFER, GL_PIXEL_PACK_BUFFER, GL_PIXEL_UNPACK_BUFFER };
	glBindBuffer( targets[target], buffer );
}

/*
========================
GL_GenBuffers
========================
*/
void GL_GenBuffers( int count, unsigned int * buffers ) {
	glGenBuffers( count, buffers );
}

/*
========================
GL_DeleteBuffers
========================
*/
void GL_DeleteBuffers( int count, const unsigned int * buffers ) {
	glDeleteBuffers( count, buffers );
}

/*
========================
GL_BufferData
========================
*/
void GL_BufferData( wrapperBufferTarget_t target, intptr_t size, const void * data, wrapperBufferUsage_t usage ) {
	static const GLenum usages[] = { GL_STATIC_DRAW, GL_DYNAMIC_DRAW, GL_STREAM_DRAW, GL_STREAM_READ };
	glBufferData( BufferTargetGL( target ), size, data, usages[usage] );
}

/*
========================
GL_BufferSubData
========================
*/
void GL_BufferSubData( wrapperBufferTarget_t target, intptr_t offset, intptr_t size, const void * data ) {
	glBufferSubData( BufferTargetGL( target ), offset, size, data );
}

/*
========================
GL_MapBufferRange
========================
*/
void * GL_MapBufferRange( wrapperBufferTarget_t target, intptr_t offset, intptr_t length, unsigned int flags ) {
	GLbitfield access = 0;
	if ( flags & BUFFER_MAP_READ ) { access |= GL_MAP_READ_BIT; }
	if ( flags & BUFFER_MAP_WRITE ) { access |= GL_MAP_WRITE_BIT; }
	if ( flags & BUFFER_MAP_INVALIDATE_RANGE ) { access |= GL_MAP_INVALIDATE_RANGE_BIT; }
	if ( flags & BUFFER_MAP_UNSYNCHRONIZED ) { access |= GL_MAP_UNSYNCHRONIZED_BIT; }
	return glMapBufferRange( BufferTargetGL( target ), offset, length, access );
}

/*
========================
GL_UnmapBuffer
========================
*/
bool GL_UnmapBuffer( wrapperBufferTarget_t target ) {
	return glUnmapBuffer( BufferTargetGL( target ) ) != 0;
}

/*
========================
GL_GetError
========================
*/
unsigned int GL_GetError() {
	return glGetError();
}

/*
========================
GL_DeleteProgram
========================
*/
void GL_DeleteProgram( unsigned int program ) {
	glDeleteProgram( program );
}

/*
========================
Capability, query, texture, framebuffer, readback and draw entry points
========================
*/
static GLenum CapGL( wrapperCap_t cap ) {
	static const GLenum caps[] = { GL_TEXTURE_CUBE_MAP_SEAMLESS, GL_FRAMEBUFFER_SRGB, GL_MULTISAMPLE_ARB, GL_DEBUG_OUTPUT_SYNCHRONOUS_ARB, GL_SCISSOR_TEST };
	return caps[cap];
}

static GLenum TexTargetGL( wrapperTexTarget_t target ) {
	static const GLenum targets[] = { GL_TEXTURE_2D, GL_TEXTURE_CUBE_MAP_EXT };
	return targets[target];
}

static GLenum AttachmentGL( wrapperAttachment_t attachment ) {
	static const GLenum attachments[] = { GL_COLOR_ATTACHMENT0, GL_COLOR_ATTACHMENT1, GL_COLOR_ATTACHMENT2, GL_COLOR_ATTACHMENT3, GL_DEPTH_ATTACHMENT, GL_STENCIL_ATTACHMENT };
	return attachments[attachment];
}

static GLenum GetPnameGL( wrapperGetPname_t pname ) {
	static const GLenum pnames[] = { GL_ACTIVE_TEXTURE, GL_FRAMEBUFFER_BINDING, GL_DRAW_BUFFER, GL_READ_BUFFER, GL_VIEWPORT, GL_SCISSOR_BOX, GL_MAX_TEXTURE_SIZE, GL_MAX_TEXTURE_IMAGE_UNITS, GL_UNIFORM_BUFFER_OFFSET_ALIGNMENT, GL_NUM_EXTENSIONS, GL_MAX_TEXTURE_MAX_ANISOTROPY_EXT };
	return pnames[pname];
}

void GL_Enable( wrapperCap_t cap ) { glEnable( CapGL( cap ) ); }
void GL_Disable( wrapperCap_t cap ) { glDisable( CapGL( cap ) ); }
bool GL_IsEnabled( wrapperCap_t cap ) { return glIsEnabled( CapGL( cap ) ) != 0; }

void GL_GenQueries( int count, unsigned int * ids ) { glGenQueries( count, ids ); }
void GL_BeginQuery( wrapperQueryTarget_t target, unsigned int id ) { glBeginQuery( GL_TIME_ELAPSED_EXT, id ); }
void GL_EndQuery( wrapperQueryTarget_t target ) { glEndQuery( GL_TIME_ELAPSED_EXT ); }
void GL_GetQueryObjectui64v( unsigned int id, unsigned long long * params ) { glGetQueryObjectui64v( id, GL_QUERY_RESULT, params ); }

void GL_ActiveTexture( int unit ) { glActiveTexture( GL_TEXTURE0 + unit ); }
void GL_BindMultiTexture( int unit, wrapperTexTarget_t target, unsigned int texture ) { glBindMultiTextureEXT( GL_TEXTURE0 + unit, TexTargetGL( target ), texture ); }
void GL_BindTexture( wrapperTexTarget_t target, unsigned int texture ) { glBindTexture( TexTargetGL( target ), texture ); }
void GL_TexParameteri( wrapperTexTarget_t target, wrapperTexParam_t pname, int value ) {
	glTexParameteri( TexTargetGL( target ), pname == TEXPARAM_COMPARE_MODE ? GL_TEXTURE_COMPARE_MODE : GL_TEXTURE_COMPARE_FUNC, value );
}

void GL_GenFramebuffers( int count, unsigned int * ids ) { glGenFramebuffers( count, ids ); }
void GL_DeleteFramebuffers( int count, const unsigned int * ids ) { glDeleteFramebuffers( count, ids ); }
void GL_BindFramebuffer( unsigned int fbo ) { glBindFramebuffer( GL_FRAMEBUFFER, fbo ); }
void GL_FramebufferTexture2D( wrapperAttachment_t attachment, wrapperTexTarget_t texTarget, unsigned int texture, int level ) {
	glFramebufferTexture2D( GL_FRAMEBUFFER, AttachmentGL( attachment ), TexTargetGL( texTarget ), texture, level );
}
bool GL_CheckFramebufferStatus() { return glCheckFramebufferStatus( GL_FRAMEBUFFER ) == GL_FRAMEBUFFER_COMPLETE; }

void GL_DrawBuffer( int mode ) { glDrawBuffer( mode ); }
void GL_ReadBuffer( int mode ) { glReadBuffer( mode ); }

void GL_GetIntegerv( wrapperGetPname_t pname, int * params ) {
	if ( pname == GET_ACTIVE_TEXTURE ) {
		int value = 0;
		glGetIntegerv( GL_ACTIVE_TEXTURE, &value );
		*params = value - GL_TEXTURE0;
		return;
	}
	glGetIntegerv( GetPnameGL( pname ), params );
}
void GL_GetFloatv( wrapperGetPname_t pname, float * params ) { glGetFloatv( GetPnameGL( pname ), params ); }
const char * GL_GetString( wrapperGetStringName_t name ) {
	static const GLenum names[] = { GL_VENDOR, GL_RENDERER, GL_VERSION, GL_SHADING_LANGUAGE_VERSION, GL_EXTENSIONS };
	return (const char *)glGetString( names[name] );
}
const char * GL_GetStringi( int index ) { return (const char *)glGetStringi( GL_EXTENSIONS, index ); }

void GL_ReadPixels( int x, int y, int width, int height, wrapperPixelFormat_t format, void * data ) {
	GLenum glFormat = GL_RGB;
	GLenum glType = GL_UNSIGNED_BYTE;
	if ( format == PIXELFORMAT_RGBA ) { glFormat = GL_RGBA; }
	else if ( format == PIXELFORMAT_DEPTH_FLOAT ) { glFormat = GL_DEPTH_COMPONENT; glType = GL_FLOAT; }
	else if ( format == PIXELFORMAT_STENCIL_INDEX ) { glFormat = GL_STENCIL_INDEX; }
	glReadPixels( x, y, width, height, glFormat, glType, data );
}

void GL_EnableVertexAttribArray( unsigned int index ) { glEnableVertexAttribArray( index ); }
void GL_VertexAttribPointer( unsigned int index, int size, wrapperVertexAttribType_t type, bool normalized, int stride, intptr_t offset ) {
	static const GLenum types[] = { GL_FLOAT, GL_UNSIGNED_BYTE, GL_HALF_FLOAT };
	glVertexAttribPointer( index, size, types[type], normalized ? GL_TRUE : GL_FALSE, stride, (const void *)offset );
}
void GL_DrawElementsBaseVertex( int count, const void * indices, int baseVertex ) {
	glDrawElementsBaseVertex( GL_TRIANGLES, count, sizeof( triIndex_t ) == 2 ? GL_UNSIGNED_SHORT : GL_UNSIGNED_INT, indices, baseVertex );
}

void GL_BindBufferRange( wrapperBufferTarget_t target, unsigned int index, unsigned int buffer, intptr_t offset, intptr_t size ) {
	glBindBufferRange( BufferTargetGL( target ), index, buffer, offset, size );
}
void GL_BindVertexArray( unsigned int vao ) { glBindVertexArray( vao ); }
void GL_GenVertexArrays( int count, unsigned int * ids ) { glGenVertexArrays( count, ids ); }
void GL_UseProgram( unsigned int program ) { glUseProgram( program ); }
void GL_ColorMask( bool r, bool g, bool b, bool a ) { glColorMask( r ? GL_TRUE : GL_FALSE, g ? GL_TRUE : GL_FALSE, b ? GL_TRUE : GL_FALSE, a ? GL_TRUE : GL_FALSE ); }
void GL_DepthMask( bool enable ) { glDepthMask( enable ? GL_TRUE : GL_FALSE ); }
void GL_ClearColor( float r, float g, float b, float a ) { glClearColor( r, g, b, a ); }
void GL_ClearDepth( double depth ) { glClearDepth( depth ); }

void GL_DebugMessageCallback( GLDebugMessageProc_t callback, const void * userParam ) {
	glDebugMessageCallbackARB( (GLDEBUGPROC)callback, userParam );
}
void GL_DebugMessageControl( bool enabled ) {
	glDebugMessageControlARB( GL_DONT_CARE, GL_DONT_CARE, GL_DEBUG_SEVERITY_LOW_ARB, 0, NULL, enabled ? true : false );
}

void GL_Uniform4fv( int location, int count, const float * value ) {
	glUniform4fv( location, count, value );
}

