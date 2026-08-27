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
	worldOwned = false;
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

void idRenderEntity::Commit() {
	if ( world == NULL || index < 0 || !needsCommit ) {
		return;
	}
	idRenderWorldLocal * renderWorld = static_cast< idRenderWorldLocal * >( world );
	idRenderEntityCommitted * def = renderWorld->entityDefs[index];
	if ( def == NULL ) {
		def = new (TAG_RENDER_ENTITY) idRenderEntityCommitted;
		def->world = renderWorld;
		def->index = index;
		renderWorld->entityDefs[index] = def;
		def->postCommitMode = 3;
	} else if ( !forceUpdate && !joints && !callbackData && !def->dynamicModel &&
			!memcmp( static_cast< renderEntity_t * >( this ), &def->parms, sizeof( renderEntity_t ) ) ) {
		committed = def;
		needsCommit = false;
		renderWorld->committedWorld->PublishEntity( index, def, false );
		return;
	} else if ( !forceUpdate && callback != NULL && axis == def->parms.axis && origin == def->parms.origin &&
			bounds == def->localReferenceBounds && hModel == def->parms.hModel ) {
		def->postCommitMode = 1;
	} else {
		def->postCommitMode = ( def->parms.hModel == hModel ) ? 2 : 3;
	}
	def->parms = *this;
	def->lastModifiedFrameNum = tr.frameCount;
	committed = def;
	needsCommit = false;
	renderWorld->committedWorld->PublishEntity( index, def, true );
	tr.pc.c_entityCommits++;
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
