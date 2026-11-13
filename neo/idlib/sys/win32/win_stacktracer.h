/*
===========================================================================

Darklight engine - Windows stack tracer

Copyright (C) 2026 Darklight engine contributors

This file is part of the Darklight engine source code, which is based on
the Doom 3 BFG Edition GPL Source Code and licensed under the GNU General
Public License, version 3 or later. See the repository license file for
details.

===========================================================================
*/

#ifndef __WIN_STACKTRACER_H__
#define __WIN_STACKTRACER_H__

#ifdef _WIN32

#include "../../containers/StaticList.h"
#include "../../Str.h"

// One resolved stack frame, kept as plain text so it survives a crash.
class idStackItem {
public:
	idStackItem();
	void			SetData( const char *text );
	const char *	GetData() const { return data; }
private:
	char			data[256];
};

// Fixed capacity stack capture, safe to use from the exception filter.
class idStackTrace {
public:
	idStackTrace();

	bool			GetItem( int index, const char **text ) const;

	idStaticList< idStackItem, 32 >	stack;
};

// dbghelp backed symbol resolution and stack walking.
class idStackTracer {
public:
	// Initializes the symbol engine. Safe to call more than once.
	static bool		Init( const char *applicationSymbolPath = NULL );
	static void		Shutdown();

	// Captures the calling thread's stack, skipping ignoreFrames frames.
	static int		Trace( idStackTrace &trace, int ignoreFrames = 0, bool outputToConsole = false );

	// Walks a stack from an explicit context, for crash reports. Falls back
	// to a capture of the calling thread when context is NULL.
	static bool		Trace( HANDLE thread, CONTEXT *context, idStackTrace &trace,
							int ignoreFrames = 0, bool outputToConsole = false, bool getSourceFileInfo = true );

	static bool		GetSymbolName( UINT64 address, char *name, int maxLength );
	static bool		GetModuleName( UINT64 address, char *module, int maxLength );
	static bool		GetSource( UINT64 address, char *fileName, int maxFileLength, int *lineNumber );
	// "<exe dir>[;<applicationSymbolPath>]"
	static bool		GetSymbolPath( char *path, int maxLength, const char *applicationSymbolPath = NULL );
};

#endif // _WIN32

#endif // __WIN_STACKTRACER_H__
