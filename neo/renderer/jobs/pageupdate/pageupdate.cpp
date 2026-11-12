#include "../idlib/precompiled.h"
#pragma hdrstop

#include "pageupdate.h"
#include "../../Image.h"

/*
TNO pageupdate job semantics on this engine's image API. The reference
uses PBO mapped shadows and the AMD multi sub image extensions, which
this GL 3.3 core engine does not expose, so the shadow writes stay on
the CPU and the uploads go through idImage::SubImageUpload.
*/

void CopyImageRect( byte * destination, const byte * source, int numScanlines, int byteWidth, int bytePitch ) {
	int offset = 0;
	for ( int row = 0; row < numScanlines; ++row ) {
		memcpy( destination + offset, source + offset, byteWidth );
		offset += bytePitch;
	}
}

void UpdatePageMappings_PC( const pageImagesUpdateParms_t * parms ) {
	for ( int index = 0; index < parms->numUpdates; ++index ) {
		pageImagesUpdate_t & update = parms->updates[index];
		if ( update.cancelled || update.physicalMappingsImageInfo.bits == NULL ) {
			continue;
		}
		byte * entry = update.physicalMappingsImageInfo.bits +
			update.physicalMappingsImageInfo.bytePitch * update.physY + 16 * update.physX;
		memcpy( entry, update.map, sizeof( update.map ) );
	}
}

void UpdatePageTables_PC( const pageImagesUpdateParms_t * parms ) {
	for ( int index = 0; index < parms->numUpdates; ++index ) {
		pageImagesUpdate_t & update = parms->updates[index];
		if ( update.cancelled || update.effectivePageLevel < 0 || update.pageTableImageInfo[0].bits == NULL ) {
			continue;
		}
		for ( int level = 0; level <= update.effectivePageLevel; ++level ) {
			const int shift = update.effectivePageLevel - level;
			const int size = 1 << shift;
			const pageImageInfo_t & info = update.pageTableImageInfo[level];
			if ( info.bits == NULL ) {
				continue;
			}
			// this engine keeps a single level page table with four byte
			// entries, the reference mip pyramid stays for callers that feed it
			byte * row = info.bits + info.bytePitch * ( update.spotY << shift ) + 4 * ( update.spotX << shift );
			for ( int y = 0; y < size; ++y ) {
				for ( int x = 0; x < size; ++x ) {
					memcpy( row + 4 * x, update.pageSpot, 4 );
				}
				row += info.bytePitch;
			}
		}
	}
}

void UpdatePageImagesJob( const pageImagesUpdateParms_t * parms ) {
	UpdatePageMappings_PC( parms );
	UpdatePageTables_PC( parms );
}

void UploadPhysicalPages( const pageImagesUpdateParms_t ** parms, int numParms ) {
	for ( int parmIndex = 0; parmIndex < numParms; ++parmIndex ) {
		const pageImagesUpdateParms_t & currentParms = *parms[parmIndex];
		if ( currentParms.skipPhysicalTextureUploads ) {
			continue;
		}
		for ( int index = 0; index < currentParms.numUpdates; ++index ) {
			const pageImagesUpdate_t & update = currentParms.updates[index];
			if ( update.cancelled || update.pageData == NULL || update.pageTexels <= 0 ) {
				continue;
			}
			if ( update.physicalPagesImage0 != NULL ) {
				update.physicalPagesImage0->SubImageUpload( update.mipLevel, update.physX, update.physY, 0,
					update.pageTexels, update.pageTexels, update.pageData );
			}
			if ( update.physicalPagesImage1 != NULL ) {
				update.physicalPagesImage1->SubImageUpload( update.mipLevel, update.physX, update.physY, 0,
					update.pageTexels, update.pageTexels, update.pageData + update.pagePlaneBytes );
			}
			if ( update.physicalPagesImage2 != NULL ) {
				update.physicalPagesImage2->SubImageUpload( update.mipLevel, update.physX, update.physY, 0,
					update.pageTexels, update.pageTexels, update.pageData + update.pagePlaneBytes * 2 );
			}
		}
	}
}

void UploadPageMappings( const pageImagesUpdateParms_t ** parms, int numParms ) {
	for ( int parmIndex = 0; parmIndex < numParms; ++parmIndex ) {
		const pageImagesUpdateParms_t & currentParms = *parms[parmIndex];
		for ( int index = 0; index < currentParms.numUpdates; ++index ) {
			const pageImagesUpdate_t & update = currentParms.updates[index];
			if ( update.cancelled || update.physicalMappingsImage == NULL ) {
				continue;
			}
			// sixteen bytes of mapping per page slot, the engine runtime has
			// no mappings image so this path stays unused today
			update.physicalMappingsImage->SubImageUpload( 0, 16 * update.physX, update.physY, 0, 16, 1, update.map );
		}
	}
}

void UploadPageTables( const pageImagesUpdateParms_t ** parms, int numParms ) {
	for ( int parmIndex = 0; parmIndex < numParms; ++parmIndex ) {
		const pageImagesUpdateParms_t & currentParms = *parms[parmIndex];
		for ( int index = 0; index < currentParms.numUpdates; ++index ) {
			const pageImagesUpdate_t & update = currentParms.updates[index];
			if ( update.cancelled || update.pageTableImage == NULL || update.pageTableImageInfo[0].bits == NULL ) {
				continue;
			}
			// upload the dirty leaf rectangle from the shadow copy instead of
			// the whole table
			const pageImageInfo_t & info = update.pageTableImageInfo[0];
			const int width = update.tableMaxX - update.tableMinX;
			const int height = update.tableMaxY - update.tableMinY;
			if ( width <= 0 || height <= 0 ) {
				continue;
			}
			idList< byte > staged;
			staged.SetNum( width * height * 4 );
			CopyImageRect( staged.Ptr(),
				info.bits + info.bytePitch * update.tableMinY + 4 * update.tableMinX,
				height, width * 4, info.bytePitch );
			update.pageTableImage->SubImageUpload( 0, update.tableMinX, update.tableMinY, 0, width, height, staged.Ptr() );
		}
	}
}

void UploadPageImages( const pageImagesUpdateParms_t * parms1, const pageImagesUpdateParms_t * parms2 ) {
	const pageImagesUpdateParms_t * parms[2] = { parms1, parms2 };
	const int numParms = parms2 != NULL ? 2 : 1;
	for ( int index = 0; index < numParms; ++index ) {
		if ( !parms[index]->submittedJob ) {
			UpdatePageMappings_PC( parms[index] );
			UpdatePageTables_PC( parms[index] );
		}
	}
	UploadPhysicalPages( parms, numParms );
	UploadPageMappings( parms, numParms );
	UploadPageTables( parms, numParms );
}
