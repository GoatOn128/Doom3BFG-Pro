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

#pragma hdrstop
#include "../../idlib/precompiled.h"
#include "../sys_session_local.h"

#include "win_local.h"
#include "../../tools/idstudio/StudioPublic.h"

#define DINPUT_BUFFERSIZE           256

static DIDEVICEOBJECTDATA polled_didod[ DINPUT_BUFFERSIZE ];  // Receives buffered data 

/*
============================================================

  DIRECT INPUT MOUSE CONTROL

============================================================
*/

/*
========================
IN_InitDIMouse
========================
*/
bool IN_InitDIMouse() {
    HRESULT		hr;

	if ( win32.g_pdi == NULL) {
		return false;
	}

	// obtain an interface to the system mouse device.
	hr = win32.g_pdi->CreateDevice( GUID_SysMouse, &win32.g_pMouse, NULL);

	if (FAILED(hr)) {
		common->Printf ("mouse: Couldn't open DI mouse device\n");
		return false;
	}

    // Set the data format to "mouse format" - a predefined data format 
    //
    // A data format specifies which controls on a device we
    // are interested in, and how they should be reported.
    //
    // This tells DirectInput that we will be passing a
    // DIMOUSESTATE2 structure to IDirectInputDevice::GetDeviceState.
    if( FAILED( hr = win32.g_pMouse->SetDataFormat( &c_dfDIMouse2 ) ) ) {
		common->Printf ("mouse: Couldn't set DI mouse format\n");
		return false;
	}
    
	// set the cooperativity level.
	hr = win32.g_pMouse->SetCooperativeLevel( win32.hWnd, DISCL_EXCLUSIVE | DISCL_FOREGROUND);

	if (FAILED(hr)) {
		common->Printf ("mouse: Couldn't set DI coop level\n");
		return false;
	}


    // IMPORTANT STEP TO USE BUFFERED DEVICE DATA!
    //
    // DirectInput uses unbuffered I/O (buffer size = 0) by default.
    // If you want to read buffered data, you need to set a nonzero
    // buffer size.
    //
    // Set the buffer size to SAMPLE_BUFFER_SIZE (defined above) elements.
    //
    // The buffer size is a DWORD property associated with the device.
    DIPROPDWORD dipdw;
    dipdw.diph.dwSize       = sizeof(DIPROPDWORD);
    dipdw.diph.dwHeaderSize = sizeof(DIPROPHEADER);
    dipdw.diph.dwObj        = 0;
    dipdw.diph.dwHow        = DIPH_DEVICE;
    dipdw.dwData            = DINPUT_BUFFERSIZE; // Arbitary buffer size

    if( FAILED( hr = win32.g_pMouse->SetProperty( DIPROP_BUFFERSIZE, &dipdw.diph ) ) ) {
		common->Printf ("mouse: Couldn't set DI buffersize\n");
		return false;
	}

	IN_ActivateMouse();

	// clear any pending samples
	int	mouseEvents[MAX_MOUSE_EVENTS][2];
	Sys_PollMouseInputEvents( mouseEvents );

	common->Printf( "mouse: DirectInput initialized.\n");
	return true;
}


/*
==========================
IN_ConstrainCursorToRenderWindow
==========================
*/
void IN_ConstrainCursorToRenderWindow() {
	if ( win32.hWnd == NULL || !::IsWindow( win32.hWnd ) ) return;
	RECT clip;
	if ( !::GetClientRect( win32.hWnd, &clip ) ) return;
	::MapWindowPoints( win32.hWnd, NULL, reinterpret_cast<POINT *>( &clip ), 2 );
	::ClipCursor( &clip );
}

/*
==========================
IN_ActivateMouse
==========================
*/
void IN_ActivateMouse() {
	int i;
	HRESULT hr;

	if ( !win32.in_mouse.GetBool() || win32.mouseGrabbed || !win32.g_pMouse ) {
		return;
	}

	win32.mouseGrabbed = true;
	for ( i = 0; i < 10; i++ ) {
		if ( ::ShowCursor( false ) < 0 ) {
			break;
		}
	}

	// we may fail to reacquire if the window has been recreated
	hr = win32.g_pMouse->Acquire();
	if (FAILED(hr)) {
		return;
	}

	// set the cooperativity level.
	hr = win32.g_pMouse->SetCooperativeLevel( win32.hWnd, DISCL_EXCLUSIVE | DISCL_FOREGROUND);
	if ( com_editors & EDITOR_RADIANT ) IN_ConstrainCursorToRenderWindow();
}

/*
==========================
IN_DeactivateMouse
==========================
*/
void IN_DeactivateMouse() {
	int i;

	if (!win32.g_pMouse || !win32.mouseGrabbed ) {
		return;
	}

	win32.g_pMouse->Unacquire();
	::ClipCursor( NULL );

	for ( i = 0; i < 10; i++ ) {
		if ( ::ShowCursor( true ) >= 0 ) {
			break;
		}
	}
	win32.mouseGrabbed = false;
}

/*
==========================
IN_DeactivateMouseIfWindowed
==========================
*/
void IN_DeactivateMouseIfWindowed() {
	if ( !win32.cdsFullscreen ) {
		IN_DeactivateMouse();
	}
}

/*
====================
Sys_PollMouseInputEvents
====================
*/
int Sys_PollMouseInputEvents( int mouseEvents[MAX_MOUSE_EVENTS][2] ) {
	DWORD				dwElements;
	HRESULT				hr;

	if ( !win32.g_pMouse || !win32.mouseGrabbed ) {
		return 0;
	}

    dwElements = DINPUT_BUFFERSIZE;
    hr = win32.g_pMouse->GetDeviceData( sizeof(DIDEVICEOBJECTDATA), polled_didod, &dwElements, 0 );

    if( hr != DI_OK ) {
        hr = win32.g_pMouse->Acquire();
		// clear the garbage
		if (!FAILED(hr)) {
			win32.g_pMouse->GetDeviceData( sizeof(DIDEVICEOBJECTDATA), polled_didod, &dwElements, 0 );
		}
    }

    if( FAILED(hr) ) {
        return 0;
	}

	if ( dwElements > MAX_MOUSE_EVENTS ) {
		dwElements = MAX_MOUSE_EVENTS;
	}

	for( DWORD i = 0; i < dwElements; i++ ) {
		mouseEvents[i][0] = M_INVALID;
		mouseEvents[i][1] = 0;

		if ( polled_didod[i].dwOfs >= DIMOFS_BUTTON0 && polled_didod[i].dwOfs <= DIMOFS_BUTTON7 ) {
			const int mouseButton = ( polled_didod[i].dwOfs - DIMOFS_BUTTON0 );
			const bool mouseDown = (polled_didod[i].dwData & 0x80) == 0x80;
			mouseEvents[i][0] = M_ACTION1 + mouseButton;
			mouseEvents[i][1] = mouseDown;
			Sys_QueEvent( SE_KEY, K_MOUSE1 + mouseButton, mouseDown, 0, NULL, 0 );
		} else {
			switch (polled_didod[i].dwOfs) {
			case DIMOFS_X:
				mouseEvents[i][0] = M_DELTAX;
				mouseEvents[i][1] = polled_didod[i].dwData;
				Sys_QueEvent( SE_MOUSE, polled_didod[i].dwData, 0, 0, NULL, 0 );
				break;
			case DIMOFS_Y:
				mouseEvents[i][0] = M_DELTAY;
				mouseEvents[i][1] = polled_didod[i].dwData;
				Sys_QueEvent( SE_MOUSE, 0, polled_didod[i].dwData, 0, NULL, 0 );
				break;
			case DIMOFS_Z:
				mouseEvents[i][0] = M_DELTAZ;
				mouseEvents[i][1] = (int)polled_didod[i].dwData / WHEEL_DELTA;
				{
					const int value = (int)polled_didod[i].dwData / WHEEL_DELTA;
					const int key = value < 0 ? K_MWHEELDOWN : K_MWHEELUP;
					const int iterations = abs( value );
					for ( int i = 0; i < iterations; i++ ) {
						Sys_QueEvent( SE_KEY, key, true, 0, NULL, 0 );
						Sys_QueEvent( SE_KEY, key, false, 0, NULL, 0 );
					}
				}
				break;
			}
		}
	}

	return dwElements;
}
