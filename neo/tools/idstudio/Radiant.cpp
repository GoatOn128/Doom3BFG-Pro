/*
===========================================================================

Doom 3 GPL Source Code
Copyright (C) 1999-2011 id Software LLC, a ZeniMax Media company. 

This file is part of the Doom 3 GPL Source Code (?Doom 3 Source Code?).  

Doom 3 Source Code is free software: you can redistribute it and/or modify
it under the terms of the GNU General Public License as published by
the Free Software Foundation, either version 3 of the License, or
(at your option) any later version.

Doom 3 Source Code is distributed in the hope that it will be useful,
but WITHOUT ANY WARRANTY; without even the implied warranty of
MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
GNU General Public License for more details.

You should have received a copy of the GNU General Public License
along with Doom 3 Source Code.  If not, see <http://www.gnu.org/licenses/>.

In addition, the Doom 3 Source Code is also subject to certain additional terms. You should have received a copy of these additional terms immediately following the terms and conditions of the GNU General Public License which accompanied the Doom 3 Source Code.  If not, please request a copy in writing from id Software at the address below.

If you have questions concerning this license or the applicable additional terms, you may contact in writing id Software LLC, c/o ZeniMax Media Inc., Suite 120, Rockville, Maryland 20850 USA.

===========================================================================
*/

#include "../../idlib/precompiled.h"
#pragma hdrstop

#include "qe3.h"
#include "radiant.h"
#include "StudioPublic.h"
#include "MainFrm.h"
#include "lightdlg.h"
#include "StudioSplash.h"

#include <process.h>    // for _beginthreadex and _endthreadex
#include <ddeml.h>  // for MSGF_DDEMGR

#ifdef _DEBUG
#define new DEBUG_NEW
#undef THIS_FILE
static char THIS_FILE[] = __FILE__;
#endif

idCVar radiant_entityMode( "radiant_entityMode", "0", CVAR_TOOL | CVAR_ARCHIVE, "" );

/////////////////////////////////////////////////////////////////////////////
// idStudioApp

BEGIN_MESSAGE_MAP(idStudioApp, CWinApp)
	//{{AFX_MSG_MAP(idStudioApp)
	ON_COMMAND(ID_HELP, OnHelp)
	//}}AFX_MSG_MAP
	// Standard file based document commands
	ON_COMMAND(ID_FILE_NEW, CWinApp::OnFileNew)
	ON_COMMAND(ID_FILE_OPEN, CWinApp::OnFileOpen)
	// Standard print setup command
	ON_COMMAND(ID_FILE_PRINT_SETUP, CWinApp::OnFilePrintSetup)
END_MESSAGE_MAP()

/////////////////////////////////////////////////////////////////////////////
// idStudioApp construction

idStudioApp::idStudioApp()
{
	// TODO: add construction code here,
	// Place all significant initialization in InitInstance
}

/////////////////////////////////////////////////////////////////////////////
// The one and only idStudioApp object

idStudioApp theApp;
HINSTANCE g_DoomInstance = NULL;
bool g_editorAlive = false;
static bool g_editorShowPending = false;
static idSysMutex g_radiantPrintMutex;
static idStr g_radiantPendingPrints;

void RadiantPrint( const char *text ) {
	if ( text == NULL || text[0] == '\0' ) return;
	idScopedCriticalSection lock( g_radiantPrintMutex );
	// Keep runaway background diagnostics from growing without bound while the
	// editor is hidden.  The newest output is the useful part of the log.
	if ( g_radiantPendingPrints.Length() > 1024 * 1024 ) {
		g_radiantPendingPrints = g_radiantPendingPrints.Right( 768 * 1024 );
	}
	g_radiantPendingPrints += text;
}

static void RadiantFlushPrints() {
	if ( !g_editorAlive || g_Inspectors == NULL ||
		!g_Inspectors->consoleWnd.GetSafeHwnd() ) return;
	idStr pending;
	{
		idScopedCriticalSection lock( g_radiantPrintMutex );
		pending = g_radiantPendingPrints;
		g_radiantPendingPrints.Clear();
	}
	if ( pending.Length() > 0 ) g_Inspectors->consoleWnd.AddText( pending.c_str() );
}

void RadiantShutdown( void ) {
	RadiantHide();
}

void RadiantHide( void ) {
	if ( g_pParentWnd != NULL && g_pParentWnd->GetSafeHwnd() ) {
		g_pParentWnd->DetachEngineWindow();
		g_pParentWnd->ShowWindow( SW_HIDE );
	}
	if ( win32.hWnd != NULL ) {
		::ShowWindow( win32.hWnd, SW_SHOW );
		::SetForegroundWindow( win32.hWnd );
	}
	com_editorActive.SetBool( false );
	com_editors &= ~EDITOR_RADIANT;
	if ( common->SW() != NULL ) {
		soundSystem->SetPlayingSoundWorld( common->SW() );
	}
}

/*
=================
RadiantInit

This is also called when you 'quit' in doom
=================
*/
void RadiantInit( void ) {

	// make sure the renderer is initialized
	if ( !renderSystem->IsOpenGLRunning() ) {
		common->Printf( "no OpenGL running\n" );
		return;
	}

	g_editorAlive = true;

	// allocate a renderWorld and a soundWorld
	if ( g_qeglobals.rw == NULL ) {
		g_qeglobals.rw = renderSystem->AllocRenderWorld();
		g_qeglobals.rw->InitFromMap( NULL );
	}
	if ( g_qeglobals.sw == NULL ) {
		g_qeglobals.sw = soundSystem->AllocSoundWorld( g_qeglobals.rw );
	}

	if ( g_DoomInstance ) {
		if ( g_pParentWnd != NULL ) g_pParentWnd->AttachEngineWindow( win32.hWnd );
		g_editorShowPending = true;
	} else {
		idSplashDlg splash;
		Sys_GrabMouseCursor( false );

		g_DoomInstance = win32.hInstance;
		CWinApp* pApp = AfxGetApp();
		CWinThread *pThread = AfxGetThread();

		InitAfx();
		splash.Open( "Initializing idStudio" );
		splash.SetStatus( "Initializing application framework" );

		// App global initializations (rare)
		pApp->InitApplication();

		// Perform specific initializations
		splash.SetStatus( "Creating World Editor and tool panes" );
		if ( !pThread->InitInstance() ) {
			splash.Close();
			common->Warning( "idStudio failed to create its main frame" );
			g_DoomInstance = NULL;
			com_editorActive.SetBool( false );
			com_editors &= ~EDITOR_RADIANT;
			return;
		}

		qglFinish();
		// The engine context is already current on this thread. Rebinding it here
		// can wait inside WGL while the newly queued editor windows activate.
		g_editorShowPending = true;
		splash.Close();
	}
}


extern void Map_VerifyCurrentMap(const char *map);

void RadiantSync( const char *mapName, const idVec3 &viewOrg, const idAngles &viewAngles ) {
	if ( g_DoomInstance == NULL ) {
		RadiantInit();
	}

	if ( g_DoomInstance ) {
		idStr osPath;
		osPath = fileSystem->RelativePathToOSPath( mapName );
		Map_VerifyCurrentMap( osPath );
		idAngles flip = viewAngles;
		flip.pitch = -flip.pitch;
		g_pParentWnd->GetCamera()->SetView( viewOrg, flip );
		g_pParentWnd->SetFocus();
		Sys_UpdateWindows( W_ALL );
		g_pParentWnd->RoutineProcessing();
	}
}

void RadiantRun( void ) {
	static bool exceptionErr = false;
	if ( g_editorShowPending ) {
		g_editorShowPending = false;
		// Window activation belongs here, after the console command and MFC frame
		// construction have returned, immediately before entering MFC's pump.
		if ( g_pParentWnd != NULL && g_pParentWnd->GetSafeHwnd() ) {
			g_pParentWnd->ShowWindow( SW_SHOW );
			g_pParentWnd->RedrawWindow( NULL, NULL, RDW_INVALIDATE | RDW_ALLCHILDREN );
		}
	}
	try {
		if ( !exceptionErr ) {
			// Video mode application may restore top-level styles on win32.hWnd
			// after the initial editor attachment.  The check is effectively free
			// while the parent/style are already correct and repairs that transition.
			static int nextDockValidation = 0;
			const int now = Sys_Milliseconds();
			if ( now >= nextDockValidation && g_pParentWnd != NULL && g_pParentWnd->GetSafeHwnd() ) {
				g_pParentWnd->AttachEngineWindow( win32.hWnd );
				nextDockValidation = now + 250;
			}
			//qglPushAttrib(GL_ALL_ATTRIB_BITS);
			qglDepthMask(true);
			theApp.Run();
			//qglPopAttrib();
			//qwglMakeCurrent(0, 0);
			// The Engine tab is the engine's real render HWND.  Its visibility
			// must not control whether idStudio pumps messages: selecting that tab
			// used to make this branch false and froze every editor window.  Editor
			// view paints also borrow the shared context, so always return it to the
			// engine DC before the game frame continues.
			qwglMakeCurrent(win32.hDC, win32.hGLRC);
			RadiantFlushPrints();
		}
	}
	catch( idException &ex ) {
		::MessageBox(NULL, ex.GetError(), "Exception error", MB_OK);
		RadiantShutdown();
	}
}

bool RadiantEngineViewActive() {
	return g_editorAlive && g_pParentWnd != NULL &&
		g_pParentWnd->GetSafeHwnd() != NULL && g_pParentWnd->IsEngineWindowActive();
}

bool RadiantConsolePaneOpen() {
	return g_editorAlive && g_pParentWnd != NULL &&
		g_pParentWnd->GetSafeHwnd() != NULL && g_pParentWnd->IsStudioConsoleOpen();
}

/////////////////////////////////////////////////////////////////////////////
// idStudioApp initialization

HINSTANCE g_hOpenGL32 = NULL;
HINSTANCE g_hOpenGL = NULL;
bool g_bBuildList = false;

BOOL idStudioApp::InitInstance()
{
	// idStudio initializes a light command-bar paint manager independently of
	// the renderer.  Refresh MFC's cached system brushes here so the embedded
	// editor cannot inherit Darklight's black engine-window brush state.
	afxGlobalData.UpdateSysColors();
	CMFCVisualManager::SetDefaultManager( RUNTIME_CLASS( CMFCVisualManagerOffice2003 ) );
  //g_hOpenGL32 = ::LoadLibrary("opengl32.dll");
	// AfxEnableControlContainer();

	// Standard initialization
	// If you are not using these features and wish to reduce the size
	//  of your final executable, you should remove from the following
	//  the specific initialization routines you do not need.
  //AfxEnableMemoryTracking(FALSE);

#ifdef _AFXDLL
	//Enable3dControls();			// Call this when using MFC in a shared DLL
#else
	//Enable3dControlsStatic();	// Call this when linking to MFC statically
#endif

	// If there's a .INI file in the directory use it instead of registry

	char RadiantPath[_MAX_PATH];
	GetModuleFileName( NULL, RadiantPath, _MAX_PATH );

	// search for exe
	CFileFind Finder;
	Finder.FindFile( RadiantPath );
	Finder.FindNextFile();
	// extract root
	CString Root = Finder.GetRoot();
	// build root\*.ini
	CString IniPath = Root + "\\REGISTRY.INI";
	// search for ini file
	Finder.FindNextFile();
	if (Finder.FindFile( IniPath ))
	{
		Finder.FindNextFile();
		// use the .ini file instead of the registry
		free((void*)m_pszProfileName);
		m_pszProfileName=_tcsdup(_T(Finder.GetFilePath()));
		// look for the registry key for void* buffers storage ( these can't go into .INI files )
		int i=0;
		CString key;
		HKEY hkResult;
		DWORD dwDisp;
		DWORD type;
		char iBuf[3];
		do
		{
			sprintf( iBuf, "%d", i );
			key = "Software\\Q3Radiant\\IniPrefs" + CString(iBuf);
			// does this key exists ?
			if ( RegOpenKeyEx( HKEY_CURRENT_USER, key, 0, KEY_ALL_ACCESS, &hkResult ) != ERROR_SUCCESS )
			{
				// this key doesn't exist, so it's the one we'll use
				strcpy( g_qeglobals.use_ini_registry, key.GetBuffer(0) );
				RegCreateKeyEx( HKEY_CURRENT_USER, key, 0, NULL, 
					REG_OPTION_NON_VOLATILE, KEY_ALL_ACCESS, NULL, &hkResult, &dwDisp );
				RegSetValueEx( hkResult, "RadiantName", 0, REG_SZ, reinterpret_cast<CONST BYTE *>(RadiantPath), strlen( RadiantPath )+1 );
				RegCloseKey( hkResult );
				break;
			}
			else
			{
				char RadiantAux[ _MAX_PATH ];
				unsigned long size = _MAX_PATH;
				// the key exists, is it the one we are looking for ?
				RegQueryValueEx( hkResult, "RadiantName", 0, &type, reinterpret_cast<BYTE *>(RadiantAux), &size );
				RegCloseKey( hkResult );
				if ( !strcmp( RadiantAux, RadiantPath ) )
				{
					// got it !
					strcpy( g_qeglobals.use_ini_registry, key.GetBuffer(0) );
					break;
				}
			}
			i++;
		} while (1);
		g_qeglobals.use_ini = true;
	}
	else
	{
		// Change the registry key under which our settings are stored.
		SetRegistryKey( EDITOR_REGISTRY_KEY );
		g_qeglobals.use_ini = false;
	}

	LoadStdProfileSettings();  // Load standard INI file options (including MRU)


	// Register the application's document templates.  Document templates
	//  serve as the connection between documents, frame windows and views.

//	CMultiDocTemplate* pDocTemplate;
//	pDocTemplate = new CMultiDocTemplate(
//		IDR_RADIANTYPE,
//		RUNTIME_CLASS(CRadiantDoc),
//		RUNTIME_CLASS(idMainFrame), // custom MDI child frame
//		RUNTIME_CLASS(CRadiantView));
//	AddDocTemplate(pDocTemplate);

	// create main MDI Frame window

	g_PrefsDlg.LoadPrefs();

	qglEnableClientState( GL_VERTEX_ARRAY );

	CString strTemp = m_lpCmdLine;
	strTemp.MakeLower();
	if (strTemp.Find("builddefs") >= 0) {
		g_bBuildList = true;
	}

	// LoadFrame creates the inspector children before QE_Init/Map_New.  Their
	// initialization callbacks are allowed to query an empty selection.
	Map_InitializeListHeads();

	idMainFrame* pMainFrame = new idMainFrame;
	if (!pMainFrame->LoadFrame(IDR_MENU_QUAKE3)) {
		return FALSE;
	}

	// DOOMx64 group icon 101 is the idStudio64 frame/taskbar icon.  Load both
	// system sizes so high-DPI title bars do not synthesize a blurred glyph.
	HICON studioLargeIcon = static_cast<HICON>( ::LoadImage( AfxGetResourceHandle(),
		MAKEINTRESOURCE( IDI_IDSTUDIO ), IMAGE_ICON, ::GetSystemMetrics( SM_CXICON ),
		::GetSystemMetrics( SM_CYICON ), LR_DEFAULTCOLOR ) );
	HICON studioSmallIcon = static_cast<HICON>( ::LoadImage( AfxGetResourceHandle(),
		MAKEINTRESOURCE( IDI_IDSTUDIO ), IMAGE_ICON, ::GetSystemMetrics( SM_CXSMICON ),
		::GetSystemMetrics( SM_CYSMICON ), LR_DEFAULTCOLOR ) );
	if ( studioLargeIcon != NULL ) pMainFrame->SetIcon( studioLargeIcon, TRUE );
	if ( studioSmallIcon != NULL ) pMainFrame->SetIcon( studioSmallIcon, FALSE );

	if (pMainFrame->m_hAccelTable) {
		::DestroyAcceleratorTable(pMainFrame->m_hAccelTable);
	}
  
	pMainFrame->LoadAccelTable(MAKEINTRESOURCE(IDR_MINIACCEL));

	m_pMainWnd = pMainFrame;

	return TRUE;
}

/////////////////////////////////////////////////////////////////////////////
// idStudioApp commands

int idStudioApp::ExitInstance() 
{
	return CWinApp::ExitInstance();
}


BOOL idStudioApp::OnIdle(LONG lCount) {
	if (g_pParentWnd) {
		g_pParentWnd->RoutineProcessing();
	}
	return FALSE;
	//return CWinApp::OnIdle(lCount);
}

void idStudioApp::OnHelp()
{
	ShellExecute(m_pMainWnd->GetSafeHwnd(), "open", "http://www.idDevNet.com", NULL, NULL, SW_SHOW);
}

int idStudioApp::Run( void ) 
{
#if _MSC_VER >= 1300
	MSG *msg = AfxGetCurrentMessage();			// TODO Robert fix me!!
#else
	MSG *msg = &m_msgCur;
#endif

	// Run deferred mouse/view work before the pump so any coalesced paint is
	// eligible for this same bounded batch.
	OnIdle( 0 );

	// idStudio is hosted by the game's frame loop, so this must never use the
	// normal CWinApp blocking pump.  The old do/while called PumpMessage even
	// with an empty queue and could stall the entire game until the next Windows
	// message.  Also cap each batch: GL view invalidations and mouse traffic can
	// otherwise keep the queue non-empty forever and starve common->Frame().
	static const int MAX_MESSAGES_PER_GAME_FRAME = 64;
	int pumped = 0;
	while ( pumped < MAX_MESSAGES_PER_GAME_FRAME &&
		::PeekMessage( msg, NULL, 0, 0, PM_NOREMOVE ) ) {
		// pump message, but quit on WM_QUIT
		if (!PumpMessage()) {
			return ExitInstance();
		}
		++pumped;
	}

	return 0;
}


/*
=============================================================

REGISTRY INFO

=============================================================
*/

bool SaveRegistryInfo(const char *pszName, void *pvBuf, long lSize)
{
	SetCvarBinary(pszName, pvBuf, lSize);
	cmdSystem->BufferCommandText( CMD_EXEC_NOW, "writeConfig editor.cfg\n" );
	return true;
}

bool LoadRegistryInfo(const char *pszName, void *pvBuf, long *plSize)
{
	return GetCvarBinary(pszName, pvBuf, *plSize);
}

bool SaveWindowState(HWND hWnd, const char *pszName)
{
	RECT rc;
	GetWindowRect(hWnd, &rc);
	if (hWnd != g_pParentWnd->GetSafeHwnd()) {
	    if (::GetParent(hWnd) != g_pParentWnd->GetSafeHwnd()) {
	      ::SetParent(hWnd, g_pParentWnd->GetSafeHwnd());
	    }
		MapWindowPoints(NULL, g_pParentWnd->GetSafeHwnd(), (POINT *)&rc, 2);
	}
	return SaveRegistryInfo(pszName, &rc, sizeof(rc));
}


bool LoadWindowState(HWND hWnd, const char *pszName)
{
	RECT rc;
	LONG lSize = sizeof(rc);

	if (LoadRegistryInfo(pszName, &rc, &lSize))
	{
		if (rc.left < 0)
			rc.left = 0;
		if (rc.top < 0)
			rc.top = 0;
		if (rc.right < rc.left + 16)
			rc.right = rc.left + 16;
		if (rc.bottom < rc.top + 16)
			rc.bottom = rc.top + 16;

		MoveWindow(hWnd, rc.left, rc.top, rc.right - rc.left, rc.bottom - rc.top, FALSE);
		return true;
	}

	return false;
}

/*
===============================================================

  STATUS WINDOW

===============================================================
*/

void Sys_UpdateStatusBar( void )
{
	extern int   g_numbrushes, g_numentities;

	char numbrushbuffer[100] = "";

	sprintf( numbrushbuffer, "Brushes: %d Entities: %d", g_numbrushes, g_numentities );
	Sys_Status( numbrushbuffer, 2 );
}

void Sys_Status(const char *psz, int part )
{
	if ( part < 0 ) {
		common->Printf("%s", psz);
		part = 0;
	}
	g_pParentWnd->SetStatusText(part, psz);
}
