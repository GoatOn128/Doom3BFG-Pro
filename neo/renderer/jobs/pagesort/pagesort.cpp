#include "../../../idlib/precompiled.h"
#pragma hdrstop

#include "pagesort.h"

void PageListQuickSort( int64 * values, int64 count ) {
	std::sort( values, values + count );
}

void PageSortJob( const pageSortParms_t * parms ) {
	const int first = *parms->firstPageIndex;
	const int last = *parms->lastPageIndex;
	const int count = last - first + 1;
	for ( int output = 0; output < count; ++output ) {
		const int ring = ( first + output ) & parms->indexMask;
		const int64 page = static_cast< int32 >( parms->physicalPageNums[ring] );
		const int score = parms->pageFrameNum[page] + parms->pagePriority[page] - parms->frameNum;
		parms->sortedPhysicalPageNums[output] = page | ( static_cast< int64 >( score ) << 32 );
	}
	*parms->firstPageIndex = 0;
	*parms->lastPageIndex = count - 1;
	PageListQuickSort( parms->sortedPhysicalPageNums, count );
	*parms->done = 1;
}

REGISTER_PARALLEL_JOB( PageSortJob, "PageSortJob" );
