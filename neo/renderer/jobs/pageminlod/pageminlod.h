#pragma once

#include "../transcode/transcode.h"

class idImage;

enum pageSource_t {
	PAGESOURCE_INVALID = 0,
	PAGESOURCE_ALLOC0,
	PAGESOURCE_VMTR = PAGESOURCE_ALLOC0,
	PAGESOURCE_ALLOC1,
	PAGESOURCE_ALLOC2,
	PAGESOURCE_ALLOC3,
	PAGESOURCE_ALLOC4,
	PAGESOURCE_ALLOC5,
	PAGESOURCE_ALLOC6,
	PAGESOURCE_ALLOC7,
	PAGESOURCE_ALLOC8,
	PAGESOURCE_ALLOC9,
	PAGESOURCE_ALLOC10,
	PAGESOURCE_ALLOC11,
	PAGESOURCE_ALLOC12,
	PAGESOURCE_ALLOC13,
	PAGESOURCE_ALLOC14,
	PAGESOURCE_TOTAL
};

enum pageQuadrant_t {
	QUADRANT_LOWER_LEFT = 0,
	QUADRANT_LOWER_RIGHT = 1,
	QUADRANT_UPPER_LEFT = 2,
	QUADRANT_UPPER_RIGHT = 3
};

struct texturePage_t {
	uint8		mapped : 1;
	uint8		locked : 1;
	bool		dirty;
	uint8		maxLevel;
	uint8		pageLevel;
	uint16		pageX;
	uint16		pageY;
	pageSource_t pageSource;
	pageCompression_t pageCompression;
	pageQuadrant_t quadrant;
	uint32		finerDiskOffset[4];
	uint16		finerDiskLength[4];
	int16		physicalPageX;
	int16		physicalPageY;
	int16		physicalPageNum;
	int16		coarserPageNum;
	int16		finerPageNum[4];
	int			pad;
};

struct pageMinLodUpdate_t {
	const texturePage_t * pageArray;
	int16		rootPageNum;
	uint8 *	pageFade;
	int			fadeInSteps;
	idImage *	pageMinLodImage;
	int			pageMinLodImageSize;
	void *		pageMinLodBits;
	int			pageMinLodBytePitch;
	const void * pageMinLodPBO;
};

void UpdateMinLod_r( char * minLodTexture, int bytePitch,
	const texturePage_t * pageArray, int16 pageNum, uint8 * pageFade,
	int fadeSteps );
void UpdatePageMinLodJob( const pageMinLodUpdate_t * parms );
void UploadPageMinLod( const pageMinLodUpdate_t * parms );
