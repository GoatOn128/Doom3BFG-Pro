#pragma once

// idStudio and the game renderer intentionally share one OpenGL context.  A
// viewport paint must therefore isolate the compatibility state it needs and
// return both the state and the context to the engine before game execution
// resumes.
class idStudioGLPaintScope {
public:
	explicit idStudioGLPaintScope( HDC targetDC ) : active( false ), dc( targetDC ), stateBits( 0 ) {
		if ( dc == NULL || !wglMakeCurrent( dc, win32.hGLRC ) ) return;
		active = true;
		stateBits = GL_GetCurrentState();

		// The editor drawing code runs through the immediate-mode shim, which
		// binds its own program; unbind whatever the game renderer left behind.
		glUseProgram( 0 );
		GL_State( GLS_DEFAULT, true );
	}

	~idStudioGLPaintScope() {
		if ( !active ) return;
		// Keep the driver's common state synchronized with the renderer cache.
		GL_State( stateBits, true );
		wglMakeCurrent( win32.hDC, win32.hGLRC );
	}

	bool IsActive() const { return active; }

private:
	bool active;
	HDC dc;
	uint64 stateBits;
};
