#pragma once

// idStudio and the game renderer intentionally share one OpenGL context.  A
// viewport paint must therefore isolate the compatibility state it needs and
// return both the state and the context to the engine before game execution
// resumes.
class idStudioGLPaintScope {
public:
	explicit idStudioGLPaintScope( HDC targetDC ) : active( false ), dc( targetDC ),
		program( 0 ), stateBits( 0 ) {
		if ( dc == NULL || !qwglMakeCurrent( dc, win32.hGLRC ) ) return;
		active = true;
		stateBits = GL_GetCurrentState();
		qglGetIntegerv( GL_CURRENT_PROGRAM, &program );

		// The editor drawing code is fixed-function.  A program left bound by a
		// rendered game frame turns its colors/textures into undefined output.
		qglUseProgram( 0 );
		GL_State( GLS_DEFAULT, true );
	}

	~idStudioGLPaintScope() {
		if ( !active ) return;
		// Keep the driver's common state synchronized with the renderer cache,
		// then restore the program binding not represented by GL_State.
		GL_State( stateBits, true );
		qglUseProgram( program );
		qwglMakeCurrent( win32.hDC, win32.hGLRC );
	}

	bool IsActive() const { return active; }

private:
	bool active;
	HDC dc;
	GLint program;
	uint64 stateBits;
};
