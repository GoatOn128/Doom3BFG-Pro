/*
===========================================================================

Doom 3 BFG Edition GPL Source Code
Copyright (C) 1993-2012 id Software LLC, a ZeniMax Media company.

This file is part of the Doom 3 BFG Edition GPL Source Code ("Doom 3 BFG Edition Source Code").

Doom 3 BFG Edition Source Code is free software: you can redistribute it and/or modify
it under the terms of the GNU General Public License as published by
the Free Software Foundation, either version 3 of the License, or
(at your option) any later version.

Doom 3 BFG Edition Source Code is distributed in the hope that it will be useful,
but WITHOUT ANY WARRANTY; without even the implied warranty of
MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
GNU General Public License for more details.

You should have received a copy of the GNU General Public License
along with Doom 3 BFG Edition Source Code.  If not, see <http://www.gnu.org/licenses/>.

In addition, the Doom 3 BFG Edition Source Code is also subject to certain additional terms. You should have received a copy of these additional terms immediately following the terms and conditions of the GNU General Public License which accompanied the Doom 3 BFG Edition Source Code.  If not, please request a copy in writing from id Software at the address below.

If you have questions concerning this license or the applicable additional terms, you may contact in writing id Software LLC, c/o ZeniMax Media Inc., Suite 120, Rockville, Maryland 20850 USA.

===========================================================================
*/

#ifndef __WIN_LOCAL_H__
#define __WIN_LOCAL_H__

#include <windows.h>

// Hand rolled WGL extension types and constants. The old wglext.h header is
// gone; only the entry points the engine actually uses are declared here.
DECLARE_HANDLE( HPBUFFERARB );

typedef const char * ( WINAPI * PFNWGLGETEXTENSIONSSTRINGARBPROC )( HDC hdc );
typedef BOOL ( WINAPI * PFNWGLSWAPINTERVALEXTPROC )( int interval );
typedef BOOL ( WINAPI * PFNWGLGETPIXELFORMATATTRIBIVARBPROC )( HDC hdc, int iPixelFormat, int iLayerPlane, UINT nAttributes, const int *piAttributes, int *piValues );
typedef BOOL ( WINAPI * PFNWGLGETPIXELFORMATATTRIBFVARBPROC )( HDC hdc, int iPixelFormat, int iLayerPlane, UINT nAttributes, const int *piAttributes, FLOAT *pfValues );
typedef BOOL ( WINAPI * PFNWGLCHOOSEPIXELFORMATARBPROC )( HDC hdc, const int *piAttribIList, const FLOAT *pfAttribFList, UINT nMaxFormats, int *piFormats, UINT *nNumFormats );
typedef HGLRC ( WINAPI * PFNWGLCREATECONTEXTATTRIBSARBPROC )( HDC hDC, HGLRC hShareContext, const int *attribList );

#define WGL_SAMPLE_BUFFERS_ARB				0x2041
#define WGL_SAMPLES_ARB						0x2042
#define WGL_DOUBLE_BUFFER_ARB				0x2011
#define WGL_STENCIL_BITS_ARB				0x2023
#define WGL_DEPTH_BITS_ARB					0x2020
#define WGL_RED_BITS_ARB					0x2015
#define WGL_BLUE_BITS_ARB					0x2019
#define WGL_GREEN_BITS_ARB					0x2017
#define WGL_ALPHA_BITS_ARB					0x201B
#define WGL_STEREO_ARB						0x2012

#define WGL_CONTEXT_MAJOR_VERSION_ARB		0x2091
#define WGL_CONTEXT_MINOR_VERSION_ARB		0x2092
#define WGL_CONTEXT_FLAGS_ARB				0x2094
#define WGL_CONTEXT_DEBUG_BIT_ARB			0x0001
#define WGL_CONTEXT_PROFILE_MASK_ARB		0x9126
#define WGL_CONTEXT_CORE_PROFILE_BIT_ARB	0x0001
#define WGL_CONTEXT_COMPATIBILITY_PROFILE_BIT_ARB 0x0002

#define ERROR_INVALID_VERSION_ARB			0x2095
#define ERROR_INVALID_PROFILE_ARB			0x2096

// WGL_ARB_extensions_string
extern	PFNWGLGETEXTENSIONSSTRINGARBPROC wglGetExtensionsStringARB;

// WGL_EXT_swap_interval
extern	PFNWGLSWAPINTERVALEXTPROC wglSwapIntervalEXT;

// WGL_ARB_pixel_format
extern	PFNWGLGETPIXELFORMATATTRIBIVARBPROC wglGetPixelFormatAttribivARB;
extern	PFNWGLGETPIXELFORMATATTRIBFVARBPROC wglGetPixelFormatAttribfvARB;
extern	PFNWGLCHOOSEPIXELFORMATARBPROC wglChoosePixelFormatARB;

// WGL_ARB_create_context
extern	PFNWGLCREATECONTEXTATTRIBSARBPROC wglCreateContextAttribsARB;

#define	WINDOW_STYLE	(WS_OVERLAPPED|WS_BORDER|WS_CAPTION|WS_VISIBLE | WS_THICKFRAME)

void	Sys_QueEvent( sysEventType_t type, int value, int value2, int ptrLength, void *ptr, int inputDeviceNum );

void	Sys_CreateConsole();
void	Sys_DestroyConsole();

char	*Sys_ConsoleInput ();
char	*Sys_GetCurrentUser();

void	Win_SetErrorText( const char *text );

cpuid_t	Sys_GetCPUId();

// Input subsystem

void	IN_Init ();
void	IN_Shutdown ();
// add additional non keyboard / non mouse movement on top of the keyboard move cmd

void	IN_DeactivateMouseIfWindowed();
void	IN_DeactivateMouse();
void	IN_ActivateMouse();

void	IN_Frame();

void	DisableTaskKeys( BOOL bDisable, BOOL bBeep, BOOL bTaskMgr );

uint64 Sys_Microseconds();

// window procedure
LONG WINAPI MainWndProc( HWND hWnd, UINT uMsg, WPARAM wParam, LPARAM lParam);

void Conbuf_AppendText( const char *msg );

typedef struct {
	HWND			hWnd;
	HINSTANCE		hInstance;

	bool			activeApp;			// changed with WM_ACTIVATE messages
	bool			mouseReleased;		// inhibit mouse grab when dragging the window
	bool			movingWindow;		// inhibit mouse grab when dragging the window
	bool			mouseGrabbed;		// current state of grab and hide

	OSVERSIONINFOEX	osversion;

	cpuid_t			cpuid;

	// when we get a windows message, we store the time off so keyboard processing
	// can know the exact time of an event (not really needed now that we use async direct input)
	int				sysMsgTime;

	bool			windowClassRegistered;

	WNDPROC			wndproc;

	HDC				hDC;							// handle to device context
	HGLRC			hGLRC;						// handle to GL rendering context
	PIXELFORMATDESCRIPTOR pfd;
	int				pixelformat;

	HINSTANCE		hinstOpenGL;	// HINSTANCE for the OpenGL library

	int				desktopBitsPixel;
	int				desktopWidth, desktopHeight;

	int				cdsFullscreen;	// 0 = not fullscreen, otherwise monitor number

	idFileHandle	log_fp;

	unsigned short	oldHardwareGamma[3][256];
	// desktop gamma is saved here for restoration at exit

	static idCVar	sys_arch;
	static idCVar	sys_cpustring;
	static idCVar	in_mouse;
	static idCVar	win_allowAltTab;
	static idCVar	win_notaskkeys;
	static idCVar	win_username;
	static idCVar	win_outputEditString;
	static idCVar	win_viewlog;
	static idCVar	win_timerUpdate;
	static idCVar	win_allowMultipleInstances;

	CRITICAL_SECTION criticalSections[MAX_CRITICAL_SECTIONS];

	HINSTANCE		hInstDI;		// direct input

	LPDIRECTINPUT8			g_pdi;
	LPDIRECTINPUTDEVICE8	g_pMouse;
	LPDIRECTINPUTDEVICE8	g_pKeyboard;
	idJoystickWin32			g_Joystick;

	HANDLE			renderCommandsEvent;
	HANDLE			renderCompletedEvent;
	HANDLE			renderActiveEvent;
	HANDLE			renderThreadHandle;
	unsigned long	renderThreadId;
	void			(*glimpRenderThread)();
	void			*smpData;
	int				wglErrors;
	// SMP acceleration vars

} Win32Vars_t;

extern Win32Vars_t	win32;

#endif /* !__WIN_LOCAL_H__ */
