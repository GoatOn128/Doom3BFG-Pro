#include "../../../idlib/precompiled.h"
#pragma hdrstop

#include "pageminlod.h"
#include "../../Image.h"

void UpdateMinLod_r( char * minLodTexture, int bytePitch,
		const texturePage_t * pageArray, int16 pageNum, uint8 * pageFade,
		int fadeSteps ) {
	if ( pageNum < 0 || fadeSteps <= 0 ) {
		return;
	}
	const bool wasFading = pageFade[pageNum] != 0;
	int fade = pageFade[pageNum];
	if ( fade != 0 ) {
		pageFade[pageNum] = static_cast< uint8 >( --fade );
	}
	const texturePage_t & page = pageArray[pageNum];
	const int pageSize = 1 << page.pageLevel;
	const uint8 minLod = static_cast< uint8 >( 16 * page.pageLevel + 16 * fade / fadeSteps );
	char * row = minLodTexture + bytePitch * ( page.pageY << page.pageLevel ) +
		( page.pageX << page.pageLevel );
	for ( int y = 0; y < pageSize; ++y ) {
		memset( row, minLod, pageSize );
		row += bytePitch;
	}
	if ( !wasFading ) {
		for ( int quadrant = 0; quadrant < 4; ++quadrant ) {
			if ( page.finerPageNum[quadrant] != -1 ) {
				UpdateMinLod_r( minLodTexture, bytePitch, pageArray,
					page.finerPageNum[quadrant], pageFade, fadeSteps );
			}
		}
	}
}

void UpdatePageMinLodJob( const pageMinLodUpdate_t * parms ) {
	UpdateMinLod_r( static_cast< char * >( parms->pageMinLodBits ),
		parms->pageMinLodBytePitch, parms->pageArray, parms->rootPageNum,
		parms->pageFade, parms->fadeInSteps );
}

void UploadPageMinLod( const pageMinLodUpdate_t * parms ) {
	glBindBuffer( GL_PIXEL_UNPACK_BUFFER,
		reinterpret_cast< GLuint >( const_cast< void * >( parms->pageMinLodPBO ) ) );
	parms->pageMinLodImage->SubImageUpload( 0, 0, 0, 0,
		parms->pageMinLodImageSize, parms->pageMinLodImageSize, NULL );
	glBindBuffer( GL_PIXEL_UNPACK_BUFFER, 0 );
}

REGISTER_PARALLEL_JOB( UpdatePageMinLodJob, "UpdatePageMinLodJob" );
