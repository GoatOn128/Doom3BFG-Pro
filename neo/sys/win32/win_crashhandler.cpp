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

#pragma hdrstop
#include "../../idlib/precompiled.h"

#include <DbgHelp.h>
#include <time.h>

#include "win_local.h"
#include "win_crashhandler.h"

idCVar com_enableCrashHandler( "com_enableCrashHandler", "1", CVAR_SYSTEM | CVAR_BOOL | CVAR_ARCHIVE, "install the unhandled exception filter" );
idCVar win_crashDmp_path( "win_crashDmp_path", "", CVAR_SYSTEM | CVAR_ARCHIVE, "directory in which crash reports are written" );
idCVar win_silentCrash( "win_silentCrash", "0", CVAR_SYSTEM | CVAR_BOOL | CVAR_ARCHIVE, "suppress the crash message box and terminate quietly" );
idCVar win_terminateOnCrash( "win_terminateOnCrash", "0", CVAR_SYSTEM | CVAR_BOOL, "terminate immediately without storing crash data" );
idCVar sys_minidumpEnable( "sys_minidumpEnable", "1", CVAR_SYSTEM | CVAR_BOOL | CVAR_ARCHIVE, "write a minidump when the engine crashes" );
idCVar sys_minidumpBigDump( "sys_minidumpBigDump", "0", CVAR_SYSTEM | CVAR_BOOL | CVAR_ARCHIVE, "include full process memory in the minidump" );

static idStr crashReportPath;

/*
========================
CrashDirectory

Reports go to the cvar override directory or the working directory
========================
*/
static idStr CrashDirectory() {
	const char *configured = win_crashDmp_path.GetString();
	if ( configured != NULL && configured[0] != '\0' ) {
		CreateDirectoryA( configured, NULL );
		return idStr( configured );
	}
	return idStr( Sys_Cwd() );
}

/*
========================
CrashBaseName

<directory>\\Darklight-crash-YYYYMMDD-HHMMSS
========================
*/
static idStr CrashBaseName() {
	time_t now = time( NULL );
	tm localTime;
#ifdef _MSC_VER
	localtime_s( &localTime, &now );
#else
	localTime = *localtime( &now );
#endif
	char stamp[64];
	strftime( stamp, sizeof( stamp ), "%Y%m%d-%H%M%S", &localTime );

	idStr result = CrashDirectory();
	if ( result.Length() > 0 ) {
		char last = result[result.Length() - 1];
		if ( last != '\\' && last != '/' ) {
			result.Append( "\\" );
		}
	}
	result.Append( "Darklight-crash-" );
	result.Append( stamp );
	return result;
}

/*
========================
WriteFileLine
========================
*/
static void WriteFileLine( HANDLE file, const char *text ) {
	if ( file == NULL || file == INVALID_HANDLE_VALUE || text == NULL ) {
		return;
	}
	DWORD written = 0;
	WriteFile( file, text, (DWORD)strlen( text ), &written, NULL );
	WriteFile( file, "\r\n", 2, &written, NULL );
}

/*
========================
idCrashHandler::idCrashHandler
========================
*/
idCrashHandler::idCrashHandler() {
	originalHandler = SetUnhandledExceptionFilter( UnhandledExceptionFilterProc );
}

/*
========================
idCrashHandler::~idCrashHandler
========================
*/
idCrashHandler::~idCrashHandler() {
	if ( originalHandler != NULL ) {
		SetUnhandledExceptionFilter( originalHandler );
		originalHandler = NULL;
	}
}

/*
========================
idCrashHandler::WriteMiniDump
========================
*/
bool idCrashHandler::WriteMiniDump( HANDLE file, EXCEPTION_POINTERS *exceptionInfo, bool fullMemory ) {
	if ( file == NULL || file == INVALID_HANDLE_VALUE ) {
		return false;
	}
	MINIDUMP_EXCEPTION_INFORMATION dumpException;
	dumpException.ThreadId = GetCurrentThreadId();
	dumpException.ExceptionPointers = exceptionInfo;
	dumpException.ClientPointers = FALSE;
	MINIDUMP_TYPE type = (MINIDUMP_TYPE)( MiniDumpWithDataSegs | MiniDumpWithIndirectlyReferencedMemory | MiniDumpIgnoreInaccessibleMemory );
	if ( fullMemory ) {
		type = (MINIDUMP_TYPE)( type | MiniDumpWithPrivateReadWriteMemory );
	}
	return MiniDumpWriteDump( GetCurrentProcess(), GetCurrentProcessId(), file, type,
		exceptionInfo != NULL ? &dumpException : NULL, NULL, NULL ) != FALSE;
}

/*
========================
idCrashHandler::GetExceptionDescription
========================
*/
const char *idCrashHandler::GetExceptionDescription( unsigned int code ) {
	switch ( code ) {
		case EXCEPTION_ACCESS_VIOLATION:		return "Access Violation";
		case EXCEPTION_DATATYPE_MISALIGNMENT:	return "Address Misaligned";
		case EXCEPTION_BREAKPOINT:				return "Breakpoint";
		case EXCEPTION_SINGLE_STEP:				return "Single Step";
		case EXCEPTION_ILLEGAL_INSTRUCTION:		return "Illegal Instruction";
		case EXCEPTION_ARRAY_BOUNDS_EXCEEDED:	return "Array Out of Bounds";
		case EXCEPTION_FLT_DENORMAL_OPERAND:	return "Floating Point Denormal Operand";
		case EXCEPTION_FLT_DIVIDE_BY_ZERO:		return "Floating Point Divide by Zero";
		case EXCEPTION_FLT_INVALID_OPERATION:	return "Invalid Floating Point Operation";
		case EXCEPTION_FLT_OVERFLOW:			return "Floating Point Overflow";
		case EXCEPTION_FLT_UNDERFLOW:			return "Floating Point Underflow";
		case EXCEPTION_INT_DIVIDE_BY_ZERO:		return "Integer Divide by Zero";
		case EXCEPTION_INT_OVERFLOW:			return "Integer Overflow";
		case EXCEPTION_PRIV_INSTRUCTION:		return "Privileged Instruction";
		case EXCEPTION_STACK_OVERFLOW:			return "Stack Overflow";
		case 0xC0000008:						return "Invalid Handle";
		case 0xC0000017:						return "Out of Memory";
		default:								return "Undefined Exception";
	}
}

/*
========================
idCrashHandler::GetExceptionInfo
========================
*/
void idCrashHandler::GetExceptionInfo( EXCEPTION_POINTERS *info, idStr &out ) {
	if ( info == NULL || info->ExceptionRecord == NULL ) {
		out = "No exception record available";
		return;
	}
	const EXCEPTION_RECORD &record = *info->ExceptionRecord;
	out.Format( "Exception 0x%08X (%s) at 0x%p", record.ExceptionCode,
		GetExceptionDescription( record.ExceptionCode ), record.ExceptionAddress );
	if ( record.ExceptionCode == EXCEPTION_ACCESS_VIOLATION && record.NumberParameters >= 2 ) {
		const char *operation = record.ExceptionInformation[0] == 0 ? "read"
			: ( record.ExceptionInformation[0] == 1 ? "write" : "execute" );
		idStr detail;
		detail.Format( "Attempted to %s address 0x%llX", operation,
			( unsigned long long )record.ExceptionInformation[1] );
		out.Append( "\n" );
		out.Append( detail );
	}
}

/*
========================
idCrashHandler::GetRegisterInfo
========================
*/
void idCrashHandler::GetRegisterInfo( CONTEXT *context, idStr &out ) {
	if ( context == NULL ) {
		out = "No register context available";
		return;
	}
#if defined( _M_X64 )
	out.Format(
		"RAX=%016llX RBX=%016llX RCX=%016llX RDX=%016llX\n"
		"RSI=%016llX RDI=%016llX RBP=%016llX RSP=%016llX\n"
		"R8 =%016llX R9 =%016llX R10=%016llX R11=%016llX\n"
		"R12=%016llX R13=%016llX R14=%016llX R15=%016llX\n"
		"RIP=%016llX EFlags=%08lX",
		context->Rax, context->Rbx, context->Rcx, context->Rdx,
		context->Rsi, context->Rdi, context->Rbp, context->Rsp,
		context->R8, context->R9, context->R10, context->R11,
		context->R12, context->R13, context->R14, context->R15,
		context->Rip, context->EFlags );
#else
	out.Format(
		"EAX=%08lX EBX=%08lX ECX=%08lX EDX=%08lX\n"
		"EIP=%08lX ESP=%08lX EBP=%08lX EFlags=%08lX",
		context->Eax, context->Ebx, context->Ecx, context->Edx,
		context->Eip, context->Esp, context->Ebp, context->EFlags );
#endif
}

/*
========================
idCrashHandler::GetCallstackInfo

Walk the current stack with dbghelp. The filter runs on the faulting
thread, so the crashing frames are still on the stack below us.
========================
*/
void idCrashHandler::GetCallstackInfo( CONTEXT *context, idStr &out ) {
	out.Clear();

	HANDLE process = GetCurrentProcess();
	SymSetOptions( SYMOPT_UNDNAME | SYMOPT_DEFERRED_LOADS | SYMOPT_LOAD_LINES );
	if ( !SymInitialize( process, NULL, TRUE ) ) {
		out = "Symbol resolution unavailable";
		return;
	}

	void *frames[32];
	// skip this function and CaptureStackBackTrace itself
	USHORT count = CaptureStackBackTrace( 2, 32, frames, NULL );
	for ( USHORT i = 0; i < count; i++ ) {
		char symbolBuffer[ sizeof( SYMBOL_INFO ) + 256 ];
		SYMBOL_INFO *symbol = (SYMBOL_INFO *)symbolBuffer;
		symbol->SizeOfStruct = sizeof( SYMBOL_INFO );
		symbol->MaxNameLen = 255;

		idStr line;
		line.Format( "  %2u  %p", (unsigned int)i, frames[i] );

		DWORD64 displacement = 0;
		if ( SymFromAddr( process, (DWORD64)frames[i], &displacement, symbol ) ) {
			idStr name;
			name.Format( "  %s+0x%llx", symbol->Name, (unsigned long long)displacement );
			line.Append( name );
		}

		IMAGEHLP_LINE64 lineInfo;
		memset( &lineInfo, 0, sizeof( lineInfo ) );
		lineInfo.SizeOfStruct = sizeof( lineInfo );
		DWORD lineDisplacement = 0;
		if ( SymGetLineFromAddr64( process, (DWORD64)frames[i], &lineDisplacement, &lineInfo ) ) {
			idStr location;
			location.Format( "  (%s:%lu)", lineInfo.FileName, lineInfo.LineNumber );
			line.Append( location );
		}

		out.Append( line );
		out.Append( "\n" );
	}

	SymCleanup( process );
}

/*
========================
idCrashHandler::StoreCrashData
========================
*/
void idCrashHandler::StoreCrashData( EXCEPTION_POINTERS *info ) {
	idStr reportPath = CrashBaseName();
	reportPath.Append( ".txt" );
	crashReportPath = reportPath;

	HANDLE file = CreateFileA( reportPath.c_str(), GENERIC_WRITE, FILE_SHARE_READ, NULL,
		CREATE_ALWAYS, FILE_ATTRIBUTE_NORMAL, NULL );
	if ( file == INVALID_HANDLE_VALUE ) {
		idLib::Warning( "Unable to create crash report '%s' (error %lu)", reportPath.c_str(), GetLastError() );
		return;
	}

	idStr exception;
	idStr registers;
	idStr callstack;
	GetExceptionInfo( info, exception );
	GetRegisterInfo( info != NULL ? info->ContextRecord : NULL, registers );
	GetCallstackInfo( info != NULL ? info->ContextRecord : NULL, callstack );

	WriteFileLine( file, "Darklight crash report" );
	WriteFileLine( file, exception.c_str() );
	WriteFileLine( file, registers.c_str() );
	WriteFileLine( file, "Call stack:" );
	WriteFileLine( file, callstack.c_str() );
	CloseHandle( file );
}

/*
========================
idCrashHandler::UnhandledExceptionFilterProc
========================
*/
LONG WINAPI idCrashHandler::UnhandledExceptionFilterProc( EXCEPTION_POINTERS *info ) {
	if ( win_terminateOnCrash.GetBool() ) {
		ExitProcess( 0 );
	}
	if ( !com_enableCrashHandler.GetBool() ) {
		return EXCEPTION_CONTINUE_SEARCH;
	}

	StoreCrashData( info );

	if ( sys_minidumpEnable.GetBool() ) {
		idStr dumpPath = CrashBaseName();
		dumpPath.Append( ".dmp" );
		HANDLE dumpFile = CreateFileA( dumpPath.c_str(), GENERIC_WRITE, FILE_SHARE_READ, NULL,
			CREATE_ALWAYS, FILE_ATTRIBUTE_NORMAL, NULL );
		if ( dumpFile == INVALID_HANDLE_VALUE ) {
			idLib::Warning( "Unable to create minidump '%s' (error %lu)", dumpPath.c_str(), GetLastError() );
		} else {
			if ( !WriteMiniDump( dumpFile, info, sys_minidumpBigDump.GetBool() ) ) {
				idLib::Warning( "MiniDumpWriteDump failed for '%s' (error %lu)", dumpPath.c_str(), GetLastError() );
			}
			CloseHandle( dumpFile );
		}
	}

	ShowCursor( TRUE );

	if ( win_silentCrash.GetBool() ) {
		SetErrorMode( SEM_FAILCRITICALERRORS | SEM_NOGPFAULTERRORBOX |
			SEM_NOALIGNMENTFAULTEXCEPT | SEM_NOOPENFILEERRORBOX );
		return EXCEPTION_EXECUTE_HANDLER;
	}

	MessageBoxA( NULL, va( "The engine has crashed. A detailed report is located at:\n\n%s", crashReportPath.c_str() ),
		"Darklight crashed", MB_OK | MB_ICONERROR | MB_TOPMOST );

	return EXCEPTION_CONTINUE_SEARCH;
}
