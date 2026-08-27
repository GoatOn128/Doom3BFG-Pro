#include "../idlib/precompiled.h"
#pragma hdrstop

#include "ConsoleWatch.h"

idList< idConsoleWatch > consoleWatchList;
idList< idStr > consoleWatchResults;

static void Con_Watch_f( const idCmdArgs & args ) {
	if ( args.Argc() < 2 ) {
		common->Printf( "usage: con_watch <command> [x y]\n" );
		return;
	}

	idConsoleWatch watch( args.Argv( 1 ), args.Argc() > 2 ? atoi( args.Argv( 2 ) ) : -1,
		args.Argc() > 3 ? atoi( args.Argv( 3 ) ) : -1 );
	for ( int i = 0; i < consoleWatchList.Num(); ++i ) {
		if ( consoleWatchList[i].watchString.Icmp( watch.watchString ) == 0 ) {
			consoleWatchList[i] = watch;
			return;
		}
	}
	consoleWatchList.Append( watch );
}

static void Con_Unwatch_f( const idCmdArgs & args ) {
	if ( args.Argc() < 2 || idStr::Icmp( args.Argv( 1 ), "all" ) == 0 ) {
		consoleWatchList.Clear();
		return;
	}
	for ( int i = 0; i < consoleWatchList.Num(); ++i ) {
		if ( consoleWatchList[i].watchString.Icmp( args.Argv( 1 ) ) == 0 ) {
			consoleWatchList.RemoveIndex( i );
			return;
		}
	}
}

void ConsoleWatch_Init() {
	cmdSystem->AddCommand( "con_watch", Con_Watch_f, CMD_FL_SYSTEM,
		"displays the printed output of a command every frame" );
	cmdSystem->AddCommand( "con_unwatch", Con_Unwatch_f, CMD_FL_SYSTEM,
		"removes a console watch; use 'all' to remove every watch" );
}

void ConsoleWatch_Shutdown() {
	cmdSystem->RemoveCommand( "con_watch" );
	cmdSystem->RemoveCommand( "con_unwatch" );
	consoleWatchList.Clear();
	consoleWatchResults.Clear();
}
