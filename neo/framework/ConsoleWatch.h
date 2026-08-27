#ifndef __CONSOLEWATCH_H__
#define __CONSOLEWATCH_H__

class idConsoleWatch {
public:
	idConsoleWatch() : drawX( -1 ), drawY( -1 ) {}
	idConsoleWatch( const char * command, int x, int y ) : watchString( command ), drawX( x ), drawY( y ) {}

	idStr	watchString;
	int		drawX;
	int		drawY;
};

extern idList< idConsoleWatch > consoleWatchList;
extern idList< idStr > consoleWatchResults;

void ConsoleWatch_Init();
void ConsoleWatch_Shutdown();

#endif
