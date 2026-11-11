#pragma hdrstop
#include "../../../idlib/precompiled.h"

#include "approximatelighting.h"

// Tech5 approximate lighting range constants, kept close to the original
// job's behavior
static const float AMBIENT_MAX_RANGE		= 2000.0f;
static const float AMBIENT_FADE_RANGE		= 1200.0f;

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

/*
==================
AmbientOnlyLighting

Fills the six ambient channels from the offline ambient map around the
model center and applies a distance fade so far away models fall back to
the plain ambient pass.
==================
*/
void AmbientOnlyLighting( const approximateLightParms_t & parms, approximateLighting_t & lighting ) {
	lighting.Clear();

	const float distance = ( parms.modelOrigin - parms.viewOrigin ).Length();
	const float fade = 1.0f - idMath::ClampFloat( 0.0f, 1.0f, ( distance - ( AMBIENT_MAX_RANGE - AMBIENT_FADE_RANGE ) ) / AMBIENT_FADE_RANGE );
	if ( fade <= 0.0f ) {
		return;
	}

	ambientColors_t colors;
	memset( &colors, 0, sizeof( colors ) );
	if ( parms.ambientMap != NULL ) {
		parms.ambientMap->AmbientSample( parms.modelOrigin, &colors );
	}

	for ( int i = 0; i < 6; i++ ) {
		lighting.channelLighting[i].x = colors.channels[i][0] * ( 1.0f / 255.0f ) * fade;
		lighting.channelLighting[i].y = colors.channels[i][1] * ( 1.0f / 255.0f ) * fade;
		lighting.channelLighting[i].z = colors.channels[i][2] * ( 1.0f / 255.0f ) * fade;
	}

	lighting.lightingModel.x = LM_PRIME_AND_CHANNEL;
	lighting.lightingModel.y = 0.0f;
}

/*
==================
LocalLighting

Ambient pass plus up to three radial light contributions. The light
origins stay in world space, the self shadow pass transforms them into
model space with the inverse model matrix.
==================
*/
void LocalLighting( const approximateLightParms_t & parms, approximateLighting_t & lighting ) {
	AmbientOnlyLighting( parms, lighting );
	if ( parms.ambientMap == NULL ) {
		return;	// without ambient data the lighting model stays prime only
	}

	lighting.lightingModel.x = LM_LOCAL_LIGHTS;
	lighting.lightingModel.y = 1.0f;

	const int numLights = Min( parms.numContributingLights, MAX_CONTRIBUTING_LIGHTS );
	for ( int i = 0; i < numLights; i++ ) {
		lighting.lightOrigin[i] = parms.contributingLights[i].lightOrigin;
		const idVec3 & lightColor = parms.contributingLights[i].lightColor;
		lighting.lightColor[i] = idVec4( lightColor.x, lightColor.y, lightColor.z, 0.0f );
	}
}
