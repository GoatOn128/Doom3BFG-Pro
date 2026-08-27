#include "../../../idlib/precompiled.h"
#pragma hdrstop

#include "feedbackanalysis.h"

namespace {
	static const uint64 PAGE_KEY_MASK = 0xFFFFFFFCull;
	static const uint64 PAGE_COUNT_UNIT = 0x100000000ull;
	static const uint64 PAGE_PRIORITY_UNIT = 0x1000000000000ull;
	static const uint64 PAGE_PRIORITY_MASK = 0x1F000000000000ull;
	static const uint64 PAGE_PRIORITY_CLEAR_MASK = 0xFFE0FFFFFFFFFFFFull;
	static const uint32 HASH_BUCKETS = 1024;
	static const uint32 HASH_NEXT_OFFSET = 1024;
	static const uint32 PARENT_OFFSET = 9216;
	static const uint32 MAXIMUM_PAGES = 8192;

	uint32 PageHash( uint32 x, uint32 y, uint32 level ) {
		const uint32 packed = x | ( ( y | ( level << 11 ) ) << 11 );
		const uint32 hash = packed * 1664525u + 1013904223u;
		return ( hash ^ ( ( hash ^ ( hash >> 11 ) ) >> 11 ) ) & ( HASH_BUCKETS - 1 );
	}

	uint64 PageKey( uint32 virtualTexture, uint32 level, uint32 x, uint32 y ) {
		const uint64 textureAndLevel = ( ( static_cast< uint64 >( virtualTexture & 15 ) << 4 ) |
			( level & 15 ) ) << 11;
		return 4ull * ( ( x & 0x7FF ) | ( ( ( y & 0x7FF ) | textureAndLevel ) << 11 ) );
	}

	void SortDescending( uint64 * values, uint16 count ) {
		std::sort( values, values + count, []( uint64 a, uint64 b ) { return a > b; } );
	}
}

void GetUniqueVirtualPages( pageIndices_t * pageIndices, uint16 * temp,
		const virtualTextureInfo_t * vtInfo, const void * data, uint32 width,
		uint32 height, uint32 bytePitch, const void * data2, uint32 data2Width,
		bool forceHighestDetail ) {
	memset( temp, 0, 17408 * sizeof( temp[0] ) );
	uint16 * hashHeads = temp;
	uint16 * hashNext = temp + HASH_NEXT_OFFSET;
	uint16 * parents = temp + PARENT_OFFSET;
	uint64 * indices = pageIndices->indices;
	const uint8 * feedback = static_cast< const uint8 * >( data );
	const uint8 * explicitFeedback = static_cast< const uint8 * >( data2 );
	const uint32 detailMask = forceHighestDetail ? 0 : 31;
	uint16 totalPages = 0;
	uint16 currentPage = 0;
	uint16 currentFinestPage = 0;
	int lastX = -1, lastY = -1, lastLevel = -1, lastTexture = -1;

	const int effectiveHeight = bytePitch == 0 ? 0 : static_cast< int >( height );
	for ( int row = -1; row < effectiveHeight; ++row ) {
		const uint8 * rowData = row == -1 ? explicitFeedback : feedback + static_cast< size_t >( bytePitch ) * row;
		const uint32 rowWidth = row == -1 ? data2Width : width;
		if ( rowData == NULL ) {
			continue;
		}
		for ( uint32 pixelIndex = 0; pixelIndex < rowWidth; ++pixelIndex ) {
			const uint8 * pixel = rowData + pixelIndex * 4;
			const uint32 pageX = pixel[0] | ( static_cast< uint32 >( pixel[2] & 0x0F ) << 8 );
			const uint32 pageY = pixel[1] | ( static_cast< uint32 >( pixel[2] & 0xF0 ) << 4 );
			const uint32 texture = pixel[3] >> 4;
			const uint32 feedbackLevel = pixel[3] & 0x0F;
			if ( texture == 0 || texture >= 16 ) {
				continue;
			}
			if ( lastX == static_cast< int >( pageX ) && lastY == static_cast< int >( pageY ) &&
					lastLevel == static_cast< int >( feedbackLevel ) && lastTexture == static_cast< int >( texture ) ) {
				for ( uint16 page = currentPage; page != 0; page = parents[page] ) {
					indices[page - 1] += PAGE_COUNT_UNIT;
				}
				continue;
			}

			const virtualTextureInfo_t & info = vtInfo[texture];
			const uint32 numLevels = info.numLevels;
			if ( numLevels == 0 || pageX >= info.pagesWide || pageY >= info.pagesWide ) {
				continue;
			}
			uint32 baseLevel = info.baseLevel;
			const uint32 requestedLevel = detailMask & feedbackLevel;
			if ( requestedLevel >= baseLevel ) {
				baseLevel = Min( requestedLevel, numLevels - 1 );
			}
			if ( ( info.pagesWide >> baseLevel ) == 0 ) {
				continue;
			}
			lastX = pageX; lastY = pageY; lastLevel = feedbackLevel; lastTexture = texture;

			uint32 levelX = pageX >> baseLevel;
			uint32 levelY = pageY >> baseLevel;
			uint64 levelKeys[16] = {};
			uint16 levelHashes[16] = {};
			uint16 foundPage = 0;
			uint32 foundLevel = baseLevel;
			for ( ; foundLevel < numLevels && foundLevel < 16; ++foundLevel ) {
				const uint32 hash = PageHash( levelX, levelY, foundLevel );
				const uint64 key = PageKey( texture, foundLevel, levelX, levelY );
				levelHashes[foundLevel] = static_cast< uint16 >( hash );
				levelKeys[foundLevel] = key;
				uint16 page = hashHeads[hash];
				while ( page != 0 && key != ( indices[page - 1] & PAGE_KEY_MASK ) ) {
					page = hashNext[page];
				}
				if ( page != 0 ) {
					foundPage = page;
					currentFinestPage = page;
					break;
				}
				levelX >>= 1; levelY >>= 1;
			}
			if ( baseLevel < foundLevel && static_cast< uint32 >( totalPages ) + 16 >= MAXIMUM_PAGES ) {
				currentPage = currentFinestPage;
				continue;
			}

			uint64 priority = ( row == -1 ? 16ull : 0ull ) +
				static_cast< uint64 >( info.priorityBias ) - baseLevel + foundLevel + 1;
			uint16 parentPage = foundPage;
			if ( baseLevel < foundLevel ) {
				uint64 newPriority = priority - 1;
				for ( uint32 level = foundLevel; level-- > baseLevel; ) {
					++totalPages;
					indices[totalPages - 1] = levelKeys[level] | ( newPriority << 48 ) | PAGE_COUNT_UNIT;
					const uint16 hash = levelHashes[level];
					hashNext[totalPages] = hashHeads[hash];
					hashHeads[hash] = totalPages;
					parents[totalPages] = parentPage;
					parentPage = totalPages;
					--newPriority;
				}
				currentFinestPage = totalPages;
			}
			currentPage = currentFinestPage;
			for ( uint16 page = foundPage; page != 0; page = parents[page], ++priority ) {
				const uint64 oldPriority = ( indices[page - 1] >> 48 ) & 0x1F;
				if ( priority > oldPriority ) {
					indices[page - 1] = ( priority << 48 ) | ( indices[page - 1] & PAGE_PRIORITY_CLEAR_MASK );
				}
			}
			if ( currentPage != 0 ) {
				indices[currentPage - 1] |= 1;
				for ( uint16 page = parents[currentPage]; page != 0; page = parents[page] ) {
					indices[page - 1] |= 2;
				}
			}
			for ( uint16 page = foundPage; page != 0; page = parents[page] ) {
				indices[page - 1] += PAGE_COUNT_UNIT;
			}
		}
	}

	pageIndices->totalNumPages = totalPages;
	memset( pageIndices->pageStats, 0, sizeof( pageIndices->pageStats ) );
	for ( uint16 i = 0; i < totalPages; ++i ) {
		const uint64 page = indices[i];
		const uint32 texture = static_cast< uint32 >( page >> 28 ) & 15;
		const uint16 pool = vtInfo[texture].physicalPagesPool;
		if ( pool >= 6 ) { continue; }
		pageStats_t & stats = pageIndices->pageStats[pool];
		++stats.numPages;
		stats.numVisPages += static_cast< uint16 >( page & 1 );
		stats.numTopPages += static_cast< uint16 >( ( page & 2 ) == 0 );
	}
	for ( uint16 i = 0; i < totalPages; ++i ) {
		const uint32 priority = static_cast< uint32 >( ( indices[i] & PAGE_PRIORITY_MASK ) >> 48 );
		const uint32 count = static_cast< uint32 >( ( indices[i] >> 32 ) & 0xFFFF );
		if ( priority != 0 && ( priority & 16 ) == 0 && count <= ( 1u << priority ) ) {
			indices[i] -= PAGE_PRIORITY_UNIT;
		}
	}
	SortDescending( indices, totalPages );
}

void FeedbackAnalysisJob( const analyzeParms_t * parms ) {
	uint16 temp[17408];
	GetUniqueVirtualPages( parms->pageIndices, temp, parms->vtInfo, parms->data,
		parms->width, parms->height, parms->bytePitch, parms->data2,
		parms->data2Width, parms->forceHighestDetail );
}

REGISTER_PARALLEL_JOB( FeedbackAnalysisJob, "FeedbackAnalysisJob" );
