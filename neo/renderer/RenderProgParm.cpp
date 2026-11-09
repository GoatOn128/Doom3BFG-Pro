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
#include "../idlib/precompiled.h"

#include "RenderProgParm.h"

// The built-in vector parms, in register order. The index of each entry is
// also its register in global.inc and its value in the renderParm_t enum in
// RenderProgs.h; keep all three in sync.
static const char * const s_renderParmNames[RENDERPROG_NUM_BUILTIN_PARMS] = {
	"rpScreenCorrectionFactor",		// c0
	"rpWindowCoord",				// c1
	"rpDiffuseModifier",			// c2
	"rpSpecularModifier",			// c3
	"rpLocalLightOrigin",			// c4
	"rpLocalViewOrigin",				// c5
	"rpLightProjectionS",			// c6
	"rpLightProjectionT",			// c7
	"rpLightProjectionQ",			// c8
	"rpLightFalloffS",				// c9
	"rpBumpMatrixS",					// c10
	"rpBumpMatrixT",					// c11
	"rpDiffuseMatrixS",				// c12
	"rpDiffuseMatrixT",				// c13
	"rpSpecularMatrixS",				// c14
	"rpSpecularMatrixT",				// c15
	"rpVertexColorModulate",		// c16
	"rpVertexColorAdd",				// c17
	"rpColor",						// c18
	"rpViewOrigin",					// c19
	"rpGlobalEyePos",				// c20
	"rpMVPmatrixX",					// c21
	"rpMVPmatrixY",					// c22
	"rpMVPmatrixZ",					// c23
	"rpMVPmatrixW",					// c24
	"rpModelMatrixX",				// c25
	"rpModelMatrixY",				// c26
	"rpModelMatrixZ",				// c27
	"rpModelMatrixW",				// c28
	"rpProjectionMatrixX",			// c29
	"rpProjectionMatrixY",			// c30
	"rpProjectionMatrixZ",			// c31
	"rpProjectionMatrixW",			// c32
	"rpModelViewMatrixX",			// c33
	"rpModelViewMatrixY",			// c34
	"rpModelViewMatrixZ",			// c35
	"rpModelViewMatrixW",			// c36
	"rpTextureMatrixS",				// c37
	"rpTextureMatrixT",				// c38
	"rpTexGen0S",					// c39
	"rpTexGen0T",					// c40
	"rpTexGen0Q",					// c41
	"rpTexGen0Enabled",				// c42
	"rpTexGen1S",					// c43
	"rpTexGen1T",					// c44
	"rpTexGen1Q",					// c45
	"rpTexGen1Enabled",				// c46
	"rpWobbleSkyX",					// c47
	"rpWobbleSkyY",					// c48
	"rpWobbleSkyZ",					// c49
	"rpOverbright",					// c50
	"rpEnableSkinning",				// c51
	"rpAlphaTest",					// c52
	"rpShadowMatrixX",				// c53
	"rpShadowMatrixY",				// c54
	"rpShadowMatrixZ",				// c55
	"rpShadowMatrixW",				// c56
	"rpShadowProjectionX",			// c57
	"rpShadowProjectionY",			// c58
	"rpShadowProjectionZ",			// c59
	"rpShadowProjectionW",			// c60
	"rpShadowParams"				// c61
};

// The temporary expression parm names must outlive the registry entries
// because idRenderProgParm only stores the raw pointer. The same applies
// to the user parm names.
static idStr s_temporaryParmNames[RENDERPROG_NUM_TEMPORARY_PARMS];
static idStr s_userParmNames[RENDERPROG_NUM_USER_PARMS];

idRenderProgParm::idRenderProgParm( const char * name, parmType_t type, int index ) {
	this->name = name;
	parmType = type;
	parmIndex = index;
	memset( &declaredValue, 0, sizeof( declaredValue ) );
}

idRenderProgParmState renderProgParms;

void idRenderProgParmState::EnsureBuilt() {
	if ( parms.Num() > 0 ) {
		return;
	}
	parms.SetGranularity( 64 );
	for ( int index = 0; index < RENDERPROG_NUM_BUILTIN_PARMS; index++ ) {
		parms.Append( idRenderProgParm( s_renderParmNames[index], PT_VECTOR, index ) );
	}
	// User parms referenced directly by name in the legacy shaders.
	for ( int index = 0; index < RENDERPROG_NUM_USER_PARMS; index++ ) {
		s_userParmNames[index].Format( "rpUser%i", index );
		parms.Append( idRenderProgParm( s_userParmNames[index].c_str(), PT_VECTOR,
										RENDERPROG_USER_PARM_BASE + index ) );
	}
	// Temporary expression parms used by the parm block parser; their
	// indices sit just below INVALID_PARM_INDEX.
	for ( int index = 0; index < RENDERPROG_NUM_TEMPORARY_PARMS; index++ ) {
		s_temporaryParmNames[index].Format( "temp%i", index + 1 );
		parms.Append( idRenderProgParm( s_temporaryParmNames[index].c_str(), PT_VECTOR,
										RENDERPROG_FIRST_TEMPORARY_PARM_INDEX + index ) );
	}
}

const idRenderProgParm * idRenderProgParmState::FindParmByName( const char * name ) {
	if ( name == nullptr || name[0] == '\0' ) {
		return nullptr;
	}
	EnsureBuilt();
	for ( int index = 0; index < parms.Num(); index++ ) {
		if ( idStr::Icmp( parms[index].GetName(), name ) == 0 ) {
			return &parms[index];
		}
	}
	return nullptr;
}

const idRenderProgParm * idRenderProgParmState::GetRenderParmForIndex( int index ) {
	EnsureBuilt();
	if ( index < 0 ) {
		return nullptr;
	}
	// The array position no longer equals the parm index now that the
	// temporary parms are appended after the built-in registers, so scan
	// by index.
	for ( int i = 0; i < parms.Num(); i++ ) {
		if ( parms[i].GetIndex() == index ) {
			return &parms[i];
		}
	}
	return nullptr;
}

const char * idRenderProgParmState::GetParmNameForIndex( int index ) {
	const idRenderProgParm * parm = GetRenderParmForIndex( index );
	return parm != nullptr ? parm->GetName() : nullptr;
}
