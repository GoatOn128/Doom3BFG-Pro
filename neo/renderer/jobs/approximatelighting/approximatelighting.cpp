#pragma hdrstop
#include "../../../idlib/precompiled.h"

#include "approximatelighting.h"

/*
==================
approximateLighting_t::Clear
==================
*/
void approximateLighting_t::Clear() {
	lightingModel.Zero();
	primeLightDir.Zero();
	for ( int i = 0; i < 3; i++ ) {
		lightOrigin[i].Zero();
		lightColor[i].Zero();
	}
	for ( int i = 0; i < 6; i++ ) {
		channelLighting[i].Zero();
	}
}
