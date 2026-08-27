#pragma once

struct pageSortParms_t {
	int frameNum;
	const int * pagePriority;
	const int * pageFrameNum;
	const int64 * physicalPageNums;
	int * firstPageIndex;
	int * lastPageIndex;
	int indexMask;
	int64 * sortedPhysicalPageNums;
	volatile int * done;
};

void PageListQuickSort( int64 * values, int64 count );
void PageSortJob( const pageSortParms_t * parms );

