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

#include "tr_local.h"

idRenderEntity::idRenderEntity() {
	memset( static_cast< renderEntity_t * >( this ), 0, sizeof( renderEntity_t ) );
	world = NULL;
	index = -1;
	committed = NULL;
	needsCommit = false;
	needsCommit = false;
}

idRenderEntity::~idRenderEntity() {
	FreeRenderEntity();
}

void idRenderEntity::CommitThisFrame( bool forceUpdate ) {
	if ( world != NULL && index >= 0 ) {
		if ( forceUpdate ) {
			this->forceUpdate = true;
		}
		world->QueueRenderEntity( this );
	}
}

void idRenderEntity::FreeRenderEntity() {
	if ( world != NULL && index >= 0 ) {
		world->FreeEntityDef( index );
	}
}

void idRenderEntity::ForceUpdate() {
	CommitThisFrame( true );
}

void idRenderEntity::ProjectOverlay( const idPlane localTextureAxis[2], const idMaterial *material ) {
	if ( world != NULL && index >= 0 ) {
		world->ProjectOverlay( index, localTextureAxis, material, 0 );
	}
}

void idRenderEntity::RemoveDecals() {
	if ( world != NULL && index >= 0 ) {
		world->RemoveDecals( index );
	}
}

idRenderLight::idRenderLight() {
	memset( static_cast< renderLight_t * >( this ), 0, sizeof( renderLight_t ) );
	world = NULL;
	index = -1;
	committed = NULL;
}

idRenderLight::~idRenderLight() {
	FreeRenderLight();
}

void idRenderLight::CommitThisFrame( bool forceUpdate ) {
	if ( world != NULL && index >= 0 ) {
		tr.pc.c_lightCommitRequests++;
		needsCommit = true;
	}
}

void idRenderLight::FreeRenderLight() {
	if ( world != NULL && index >= 0 ) {
		world->FreeLightDef( index );
	}
}

void idRenderLight::ForceUpdate() {
	CommitThisFrame( true );
}

idRenderEntityCommitted::idRenderEntityCommitted() {
	memset( &parms, 0, sizeof( parms ) );
	memset( modelMatrix, 0, sizeof( modelMatrix ) );
	owner					= NULL;

	world					= NULL;
	index					= 0;
	lastModifiedFrameNum	= 0;
	archived				= false;
	dynamicModel			= NULL;
	dynamicModelFrameCount	= 0;
	cachedDynamicModel		= NULL;
	localReferenceBounds	= bounds_zero;
	globalReferenceBounds	= bounds_zero;
	viewCount				= 0;
	viewEntity				= NULL;
	decals					= NULL;
	overlays				= NULL;
	entityRefs				= NULL;
	needsPortalSky			= false;
}

//======================================================================

idRenderLightCommitted::idRenderLightCommitted() {
	memset( &parms, 0, sizeof( parms ) );
	memset( lightProject, 0, sizeof( lightProject ) );

	lightHasMoved			= false;
	world					= NULL;
	index					= 0;
	areaNum					= 0;
	lastModifiedFrameNum	= 0;
	archived				= false;
	lightShader				= NULL;
	falloffImage			= NULL;
	globalLightOrigin		= vec3_zero;
	viewCount				= 0;
	viewLight				= NULL;
	references				= NULL;
	needsReferences			= false;
	foggedPortals			= NULL;

	baseLightProject.Zero();
	inverseBaseLightProject.Zero();
}

idRenderLightLocal::idRenderLightLocal() : idRenderLightCommitted() {
	memset( &gameParms, 0, sizeof( gameParms ) );
	owner = NULL;
	stagedLightShader = NULL;
	stagedFalloffImage = NULL;
	needsCommit = false;
	needsPostCommit = false;
}
