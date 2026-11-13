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

#include "win_local.h"

#define DINPUT_BUFFERSIZE           256

/*
============================================================

DIRECT INPUT KEYBOARD CONTROL

============================================================
*/

bool IN_StartupKeyboard() {
    HRESULT hr;
    bool    bExclusive;
    bool    bForeground;
    bool    bImmediate;
    bool    bDisableWindowsKey;
    DWORD   dwCoopFlags;

	if (!win32.g_pdi) {
		common->Printf("keyboard: DirectInput has not been started\n");
		return false;
	}

	if (win32.g_pKeyboard) {
		win32.g_pKeyboard->Release();
		win32.g_pKeyboard = NULL;
	}

    // Detrimine where the buffer would like to be allocated 
    bExclusive         = false;
    bForeground        = true;
    bImmediate         = false;
    bDisableWindowsKey = true;

    if( bExclusive )
        dwCoopFlags = DISCL_EXCLUSIVE;
    else
        dwCoopFlags = DISCL_NONEXCLUSIVE;

    if( bForeground )
        dwCoopFlags |= DISCL_FOREGROUND;
    else
        dwCoopFlags |= DISCL_BACKGROUND;

    // Disabling the windows key is only allowed only if we are in foreground nonexclusive
    if( bDisableWindowsKey && !bExclusive && bForeground )
        dwCoopFlags |= DISCL_NOWINKEY;

    // Obtain an interface to the system keyboard device.
    if( FAILED( hr = win32.g_pdi->CreateDevice( GUID_SysKeyboard, &win32.g_pKeyboard, NULL ) ) ) {
		common->Printf("keyboard: couldn't find a keyboard device\n");
        return false;
	}
    
    // Set the data format to "keyboard format" - a predefined data format 
    //
    // A data format specifies which controls on a device we
    // are interested in, and how they should be reported.
    //
    // This tells DirectInput that we will be passing an array
    // of 256 bytes to IDirectInputDevice::GetDeviceState.
    if( FAILED( hr = win32.g_pKeyboard->SetDataFormat( &c_dfDIKeyboard ) ) )
        return false;
    
    // Set the cooperativity level to let DirectInput know how
    // this device should interact with the system and with other
    // DirectInput applications.
    hr = win32.g_pKeyboard->SetCooperativeLevel( win32.hWnd, dwCoopFlags );
    if( hr == DIERR_UNSUPPORTED && !bForeground && bExclusive ) {
        common->Printf("keyboard: SetCooperativeLevel() returned DIERR_UNSUPPORTED.\nFor security reasons, background exclusive keyboard access is not allowed.\n");
        return false;
    }

    if( FAILED(hr) ) {
        return false;
	}

    if( !bImmediate ) {
        // IMPORTANT STEP TO USE BUFFERED DEVICE DATA!
        //
        // DirectInput uses unbuffered I/O (buffer size = 0) by default.
        // If you want to read buffered data, you need to set a nonzero
        // buffer size.
        //
        // Set the buffer size to DINPUT_BUFFERSIZE (defined above) elements.
        //
        // The buffer size is a DWORD property associated with the device.
        DIPROPDWORD dipdw;

        dipdw.diph.dwSize       = sizeof(DIPROPDWORD);
        dipdw.diph.dwHeaderSize = sizeof(DIPROPHEADER);
        dipdw.diph.dwObj        = 0;
        dipdw.diph.dwHow        = DIPH_DEVICE;
        dipdw.dwData            = DINPUT_BUFFERSIZE; // Arbitary buffer size

        if( FAILED( hr = win32.g_pKeyboard->SetProperty( DIPROP_BUFFERSIZE, &dipdw.diph ) ) )
            return false;
    }

    // Acquire the newly created device
    win32.g_pKeyboard->Acquire();

	common->Printf( "keyboard: DirectInput initialized.\n");
    return true;
}

/*
==========================
IN_DeactivateKeyboard
==========================
*/
void IN_DeactivateKeyboard() {
	if (!win32.g_pKeyboard) {
		return;
	}
	win32.g_pKeyboard->Unacquire( );
}

//=====================================================================================

static DIDEVICEOBJECTDATA polled_didod[ DINPUT_BUFFERSIZE ];  // Receives buffered data 

static int diFetch;
static byte toggleFetch[2][ 256 ];


#if 1
// I tried doing the full-state get to address a keyboard problem on one system,
// but it didn't make any difference

/*
====================
Sys_PollKeyboardInputEvents
====================
*/
int Sys_PollKeyboardInputEvents() {
    DWORD              dwElements;
    HRESULT            hr;

    if( win32.g_pKeyboard == NULL ) {
        return 0;
	}
    
    dwElements = DINPUT_BUFFERSIZE;
    hr = win32.g_pKeyboard->GetDeviceData( sizeof(DIDEVICEOBJECTDATA),
                                     polled_didod, &dwElements, 0 );
    if( hr != DI_OK ) 
    {
        // We got an error or we got DI_BUFFEROVERFLOW.
        //
        // Either way, it means that continuous contact with the
        // device has been lost, either due to an external
        // interruption, or because the buffer overflowed
        // and some events were lost.
        hr = win32.g_pKeyboard->Acquire();

		

		// nuke the garbage
		if (!FAILED(hr)) {
			//Bug 951: The following command really clears the garbage input.
			//The original will still process keys in the buffer and was causing
			//some problems.
			win32.g_pKeyboard->GetDeviceData( sizeof(DIDEVICEOBJECTDATA), NULL, &dwElements, 0 );
			dwElements = 0;
		}
        // hr may be DIERR_OTHERAPPHASPRIO or other errors.  This
        // may occur when the app is minimized or in the process of 
        // switching, so just try again later 
    }

    if( FAILED(hr) ) {
        return 0;
	}

	return dwElements;
}

#else

/*
====================
Sys_PollKeyboardInputEvents

Fake events by getting the entire device state
and checking transitions
====================
*/
int Sys_PollKeyboardInputEvents() {
    HRESULT            hr;

    if( win32.g_pKeyboard == NULL ) {
        return 0;
	}
    
	hr = win32.g_pKeyboard->GetDeviceState( sizeof( toggleFetch[ diFetch ] ), toggleFetch[ diFetch ] );
    if( hr != DI_OK ) 
    {
        // We got an error or we got DI_BUFFEROVERFLOW.
        //
        // Either way, it means that continuous contact with the
        // device has been lost, either due to an external
        // interruption, or because the buffer overflowed
        // and some events were lost.
        hr = win32.g_pKeyboard->Acquire();

		// nuke the garbage
		if (!FAILED(hr)) {
			hr = win32.g_pKeyboard->GetDeviceState( sizeof( toggleFetch[ diFetch ] ), toggleFetch[ diFetch ] );
		}
        // hr may be DIERR_OTHERAPPHASPRIO or other errors.  This
        // may occur when the app is minimized or in the process of 
        // switching, so just try again later 
    }

    if( FAILED(hr) ) {
        return 0;
	}

	// build faked events
	int		numChanges = 0;

	for ( int i = 0 ; i < 256 ; i++ ) {
		if ( toggleFetch[0][i] != toggleFetch[1][i] ) {
			polled_didod[ numChanges ].dwOfs = i;
			polled_didod[ numChanges ].dwData = toggleFetch[ diFetch ][i] ? 0x80 : 0;
			numChanges++;
		}
	}

	diFetch ^= 1;

	return numChanges;
}

#endif

/*
====================
Sys_PollKeyboardInputEvents
====================
*/
int Sys_ReturnKeyboardInputEvent( const int n, int &ch, bool &state ) {
	ch = polled_didod[ n ].dwOfs;
	state = ( polled_didod[ n ].dwData & 0x80 ) == 0x80;
	if ( ch == K_PRINTSCREEN || ch == K_LCTRL || ch == K_LALT || ch == K_RCTRL || ch == K_RALT ) {
		// for windows, add a keydown event for print screen here, since
		// windows doesn't send keydown events to the WndProc for this key.
		// ctrl and alt are handled here to get around windows sending ctrl and
		// alt messages when the right-alt is pressed on non-US 102 keyboards.
		Sys_QueEvent( SE_KEY, ch, state, 0, NULL, 0 );
	}
	return ch;
}


void Sys_EndKeyboardInputEvents() {
}

