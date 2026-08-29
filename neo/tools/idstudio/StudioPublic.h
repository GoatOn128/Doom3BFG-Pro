#pragma once

void RadiantInit();
void RadiantShutdown();
void RadiantHide();
void RadiantRun();
bool RadiantEngineViewActive();
bool RadiantConsolePaneOpen();
void RadiantPrint( const char * text );
void RadiantSync( const char * mapName, const idVec3 & viewOrigin, const idAngles & viewAngles );
