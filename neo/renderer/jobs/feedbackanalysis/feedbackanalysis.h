#pragma once

struct pageStats_t {
	uint16 numPages;
	uint16 numVisPages;
	uint16 numTopPages;
	uint16 pad;
};

struct pageIndices_t {
	uint64 totalNumPages;
	pageStats_t pageStats[6];
	uint64 indices[1];
};

struct virtualTextureInfo_t {
	uint32 numLevels;
	uint32 baseLevel;
	uint32 pagesWide;
	uint16 physicalPagesPool;
	uint16 priorityBias;
};

struct analyzeParms_t {
	int analyzeFrame;
	pageIndices_t * pageIndices;
	const virtualTextureInfo_t * vtInfo;
	const void * data;
	uint32 width;
	uint32 height;
	uint32 bytePitch;
	const void * data2;
	uint32 data2Width;
	bool forceHighestDetail;
};

void GetUniqueVirtualPages( pageIndices_t * pageIndices, uint16 * temp,
	const virtualTextureInfo_t * vtInfo, const void * data, uint32 width,
	uint32 height, uint32 bytePitch, const void * data2, uint32 data2Width,
	bool forceHighestDetail );
void FeedbackAnalysisJob( const analyzeParms_t * parms );

static_assert( sizeof( pageStats_t ) == 8, "pageStats_t ABI drift" );
static_assert( offsetof( pageIndices_t, indices ) == 0x38, "pageIndices_t ABI drift" );
static_assert( sizeof( virtualTextureInfo_t ) == 0x10, "virtualTextureInfo_t ABI drift" );

