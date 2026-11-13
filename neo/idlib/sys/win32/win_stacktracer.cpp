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

#pragma hdrstop
#include "../../precompiled.h"

#ifdef _WIN32

#include <DbgHelp.h>

#include "win_stacktracer.h"

/*
========================
Local state

The symbol engine is process wide, so every entry point goes through
one critical section and one initialization flag.
========================
*/

static CRITICAL_SECTION	symbolCriticalSection;
static bool				symbolCriticalSectionInitialized = false;
static bool				symbolsInitialized = false;

/*
========================
LockSymbols / UnlockSymbols
========================
*/
static void LockSymbols() {
	if ( !symbolCriticalSectionInitialized ) {
		// One time race is harmless: both threads initialize identical state.
		InitializeCriticalSection( &symbolCriticalSection );
		symbolCriticalSectionInitialized = true;
	}
	EnterCriticalSection( &symbolCriticalSection );
}

static void UnlockSymbols() {
	LeaveCriticalSection( &symbolCriticalSection );
}

/*
========================
idStackItem::idStackItem
========================
*/
idStackItem::idStackItem() {
	data[0] = '\0';
}

/*
========================
idStackItem::SetData
========================
*/
void idStackItem::SetData( const char *text ) {
	idStr::Copynz( data, text != NULL ? text : "", sizeof( data ) );
}

/*
========================
idStackTrace::idStackTrace
========================
*/
idStackTrace::idStackTrace() {
}

/*
========================
idStackTrace::GetItem
========================
*/
bool idStackTrace::GetItem( int index, const char **text ) const {
	if ( index < 0 || index >= stack.Num() || text == NULL ) {
		return false;
	}
	*text = stack[index].GetData();
	return true;
}

/*
========================
idStackTracer::GetSymbolPath

"<applicationSymbolPath>;<directory of the executable>"
========================
*/
bool idStackTracer::GetSymbolPath( char *path, int maxLength, const char *applicationSymbolPath ) {
	if ( path == NULL || maxLength <= 0 ) {
		return false;
	}
	char executable[MAX_OSPATH];
	GetModuleFileNameA( NULL, executable, sizeof( executable ) );
	char *slash = strrchr( executable, '\\' );
	if ( slash != NULL ) {
		*slash = '\0';
	}
	idStr::snPrintf( path, maxLength, "%s%s%s",
		applicationSymbolPath != NULL ? applicationSymbolPath : "",
		applicationSymbolPath != NULL && applicationSymbolPath[0] != '\0' ? ";" : "",
		executable );
	return true;
}

/*
========================
idStackTracer::Init
========================
*/
bool idStackTracer::Init( const char *applicationSymbolPath ) {
	LockSymbols();
	if ( symbolsInitialized ) {
		UnlockSymbols();
		return true;
	}
	char path[2048];
	path[0] = '\0';
	GetSymbolPath( path, sizeof( path ), applicationSymbolPath );
	SymSetOptions( SYMOPT_UNDNAME | SYMOPT_DEFERRED_LOADS | SYMOPT_LOAD_LINES );
	symbolsInitialized = SymInitialize( GetCurrentProcess(), path, TRUE ) != FALSE;
	UnlockSymbols();
	return symbolsInitialized;
}

/*
========================
idStackTracer::Shutdown
========================
*/
void idStackTracer::Shutdown() {
	LockSymbols();
	if ( symbolsInitialized ) {
		SymCleanup( GetCurrentProcess() );
		symbolsInitialized = false;
	}
	UnlockSymbols();
}

/*
========================
idStackTracer::GetSymbolName
========================
*/
bool idStackTracer::GetSymbolName( UINT64 address, char *name, int maxLength ) {
	if ( name == NULL || maxLength <= 0 ) {
		return false;
	}
	name[0] = '\0';
	Init();
	LockSymbols();
	unsigned char storage[sizeof( SYMBOL_INFO ) + 1024];
	memset( storage, 0, sizeof( storage ) );
	SYMBOL_INFO *symbol = (SYMBOL_INFO *)storage;
	symbol->SizeOfStruct = sizeof( SYMBOL_INFO );
	symbol->MaxNameLen = 1023;
	DWORD64 displacement = 0;
	bool found = SymFromAddr( GetCurrentProcess(), address, &displacement, symbol ) != FALSE;
	if ( found ) {
		idStr::Copynz( name, symbol->Name, maxLength );
	} else {
		idStr::snPrintf( name, maxLength, "0x%llx", (unsigned long long)address );
	}
	UnlockSymbols();
	return found;
}

/*
========================
idStackTracer::GetModuleName
========================
*/
bool idStackTracer::GetModuleName( UINT64 address, char *module, int maxLength ) {
	if ( module == NULL || maxLength <= 0 ) {
		return false;
	}
	module[0] = '\0';
	MEMORY_BASIC_INFORMATION info;
	memset( &info, 0, sizeof( info ) );
	if ( VirtualQuery( (void *)(DWORD_PTR)address, &info, sizeof( info ) ) == 0 ) {
		return false;
	}
	char path[MAX_OSPATH];
	if ( GetModuleFileNameA( (HMODULE)info.AllocationBase, path, sizeof( path ) ) == 0 ) {
		return false;
	}
	const char *leaf = strrchr( path, '\\' );
	idStr::Copynz( module, leaf != NULL ? leaf + 1 : path, maxLength );
	return true;
}

/*
========================
idStackTracer::GetSource
========================
*/
bool idStackTracer::GetSource( UINT64 address, char *fileName, int maxFileLength, int *lineNumber ) {
	Init();
	LockSymbols();
	IMAGEHLP_LINE64 lineInfo;
	memset( &lineInfo, 0, sizeof( lineInfo ) );
	lineInfo.SizeOfStruct = sizeof( lineInfo );
	DWORD displacement = 0;
	bool found = SymGetLineFromAddr64( GetCurrentProcess(), address, &displacement, &lineInfo ) != FALSE;
	UnlockSymbols();
	if ( fileName != NULL && maxFileLength > 0 ) {
		idStr::Copynz( fileName, found ? lineInfo.FileName : "?", maxFileLength );
	}
	if ( lineNumber != NULL ) {
		*lineNumber = found ? (int)lineInfo.LineNumber : 0;
	}
	return found;
}

/*
========================
ResolveFrame

Formats one address as "module!symbol (file:line)".
========================
*/
static void ResolveFrame( UINT64 address, char *text, int maxLength, bool getSourceFileInfo ) {
	char symbol[512];
	char module[128];
	idStackTracer::GetSymbolName( address, symbol, sizeof( symbol ) );
	idStackTracer::GetModuleName( address, module, sizeof( module ) );

	char location[512];
	location[0] = '\0';
	if ( getSourceFileInfo ) {
		char fileName[MAX_OSPATH];
		int lineNumber = 0;
		if ( idStackTracer::GetSource( address, fileName, sizeof( fileName ), &lineNumber ) && lineNumber > 0 ) {
			idStr::snPrintf( location, sizeof( location ), " (%s:%d)", fileName, lineNumber );
		}
	}
	idStr::snPrintf( text, maxLength, "%s!%s%s", module, symbol, location );
}

/*
========================
idStackTracer::Trace

Capture the calling thread's stack.
========================
*/
int idStackTracer::Trace( idStackTrace &trace, int ignoreFrames, bool outputToConsole ) {
	void *frames[32];
	// Skip this function and its caller so the trace starts in user code.
	USHORT count = CaptureStackBackTrace( (DWORD)( ignoreFrames + 2 ), 32, frames, NULL );
	trace.stack.SetNum( 0 );
	for ( USHORT i = 0; i < count; i++ ) {
		char text[256];
		ResolveFrame( (UINT64)(DWORD_PTR)frames[i], text, sizeof( text ), true );
		trace.stack.Alloc().SetData( text );
		if ( outputToConsole ) {
			idLib::Printf( "%s\n", text );
		}
	}
	return (int)count;
}

/*
========================
idStackTracer::Trace

Walk a stack from an explicit register context with StackWalk64. Used by
the crash handler, which runs on the faulting thread and still holds the
exception context.
========================
*/
bool idStackTracer::Trace( HANDLE thread, CONTEXT *context, idStackTrace &trace,
							int ignoreFrames, bool outputToConsole, bool getSourceFileInfo ) {
	trace.stack.SetNum( 0 );
	if ( context == NULL ) {
		Trace( trace, ignoreFrames, outputToConsole );
		return trace.stack.Num() > 0;
	}

	HANDLE process = GetCurrentProcess();
	Init();
	LockSymbols();

	STACKFRAME64 frame;
	memset( &frame, 0, sizeof( frame ) );
#if defined( _M_X64 )
	const DWORD machineType = IMAGE_FILE_MACHINE_AMD64;
	frame.AddrPC.Offset = context->Rip;
	frame.AddrFrame.Offset = context->Rbp;
	frame.AddrStack.Offset = context->Rsp;
#elif defined( _M_IX86 )
	const DWORD machineType = IMAGE_FILE_MACHINE_I386;
	frame.AddrPC.Offset = context->Eip;
	frame.AddrFrame.Offset = context->Ebp;
	frame.AddrStack.Offset = context->Esp;
#else
	UnlockSymbols();
	return false;
#endif
	frame.AddrPC.Mode = AddrModeFlat;
	frame.AddrFrame.Mode = AddrModeFlat;
	frame.AddrStack.Mode = AddrModeFlat;

	HANDLE walkThread = thread != NULL ? thread : GetCurrentThread();
	for ( int i = 0; i < ignoreFrames; i++ ) {
		// Skip requested frames before resolving anything.
		if ( !StackWalk64( machineType, process, walkThread, &frame, context, NULL,
				SymFunctionTableAccess64, SymGetModuleBase64, NULL ) || frame.AddrPC.Offset == 0 ) {
			UnlockSymbols();
			return false;
		}
	}
	bool ok = false;
	for ( int i = 0; i < 32; i++ ) {
		if ( frame.AddrPC.Offset != 0 ) {
			char text[256];
			ResolveFrame( frame.AddrPC.Offset, text, sizeof( text ), getSourceFileInfo );
			trace.stack.Alloc().SetData( text );
			ok = true;
			if ( outputToConsole ) {
				idLib::Printf( "%s\n", text );
			}
		}
		if ( !StackWalk64( machineType, process, walkThread, &frame, context, NULL,
				SymFunctionTableAccess64, SymGetModuleBase64, NULL ) || frame.AddrPC.Offset == 0 ) {
			break;
		}
	}
	UnlockSymbols();
	return ok;
}

#endif // _WIN32
