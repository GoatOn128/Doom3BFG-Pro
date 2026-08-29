#include "../../idlib/precompiled.h"
#pragma hdrstop

#include "StudioPublic.h"

int com_editors = EDITOR_NONE;
idCVar com_editorActive( "com_editorActive", "0", CVAR_SYSTEM | CVAR_BOOL,
	"true while idStudio owns keyboard and mouse focus" );

static void idStudio_f( const idCmdArgs & args ) {
	if ( !renderSystem->IsOpenGLRunning() ) {
		common->Warning( "idStudio requires an initialized OpenGL renderer" );
		return;
	}

	com_editors |= EDITOR_RADIANT;
	com_editorActive.SetBool( true );
	RadiantInit();
}

static idCommandLink idStudioCommand(
	"idstudio", idStudio_f,
	"opens the integrated idStudio level editor" );

