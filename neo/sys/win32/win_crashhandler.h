/*
===========================================================================

Darklight engine - Windows crash handler

Copyright (C) 2026 Darklight engine contributors

This file is part of the Darklight engine source code, which is based on
the Doom 3 BFG Edition GPL Source Code and licensed under the GNU General
Public License, version 3 or later. See the repository license file for
details.

===========================================================================
*/

#ifndef __WIN_CRASHHANDLER_H__
#define __WIN_CRASHHANDLER_H__

#include <windows.h>

class idCrashHandler {
public:
	idCrashHandler();
	~idCrashHandler();

	// Writes a minidump of the current process to an already open file.
	static bool WriteMiniDump( HANDLE file, EXCEPTION_POINTERS *exceptionInfo, bool fullMemory );
	// Short human readable name for an NT exception code.
	static const char *GetExceptionDescription( unsigned int code );
	// Exception code, address and access-violation details.
	static void GetExceptionInfo( EXCEPTION_POINTERS *info, idStr &out );
	// x86 or x64 register dump from a captured context.
	static void GetRegisterInfo( CONTEXT *context, idStr &out );
	// Symbol-resolved callstack of the faulting thread.
	static void GetCallstackInfo( CONTEXT *context, idStr &out );
	// Writes the text crash report and remembers the path for the message box.
	static void StoreCrashData( EXCEPTION_POINTERS *info );

	// The actual filter installed with SetUnhandledExceptionFilter.
	static LONG WINAPI UnhandledExceptionFilterProc( EXCEPTION_POINTERS *info );

	LPTOP_LEVEL_EXCEPTION_FILTER originalHandler;
};

#endif /* !__WIN_CRASHHANDLER_H__ */
