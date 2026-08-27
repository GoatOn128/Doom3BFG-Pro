#pragma once

enum pageCompression_t {
	COMP_INVALID = 0,
	COMP_NONE = 1,
	COMP_DXT = 2,
	COMP_LZW = 3,
	COMP_DCT = 4,
	COMP_HDP = 5,
	COMP_JXR = 6,
	COMP_MAX_COMPRESSIONS = 7
};

// These headers are stored verbatim in .pages containers.
struct DCTHeader_t {
	uint8		qualityLuma;
	uint8		qualityChroma;
	uint8		qualityNormal;
	uint8		qualitySpecular;
	uint8		qualityPower;
	uint8		flags;
	uint16		diffuseSize;
	uint16		normalSize;
	uint16		specularSize;
	uint16		powerSize;
	uint16		alphaSize;
};

struct HDPHeader_t {
	uint8		qualityDiffuse;
	uint8		qualityNormal;
	uint8		qualitySpecular;
	uint8		qualityPower;
	uint8		flags;
	uint8		pad;
	uint16		diffuseSize;
	uint16		normalSize;
	uint16		specularSize;
	uint16		powerSize;
	uint16		alphaSize;
};

typedef HDPHeader_t JXRHeader_t;

struct transcodeParms_t {
	int imageFlags;
	int layoutVersion;
	int numLevels;
	int pageSource;
	int pageLevel;
	int pageX;
	int pageY;
	int physicalPageX;
	int physicalPageY;
	pageCompression_t inPageCompression;
	pageCompression_t outPageCompression;
	int inPageDataLength;
	int outPageDataLength;
	int pageHeaderSize;
	int diskOffsetScale;
	int tempSize;
	int targetBytePitch[3];
	int upsampleX;
	int upsampleY;
	int upsampleLevel;
	int upsampleFilter;
	uint16 upsampleSharpen;
	uint16 upsampleNoise;
	bool stripPageBorders;
	uint8 tiled : 1;
	uint8 directmapped : 1;
	uint8 dbgShowPageSources : 1;
	uint8 dbgShowPageColors : 1;
	uint8 dbgShowPageNumbers : 1;
	uint8 dbgShowPageInfo : 1;
	uint8 dbgShowPageBorders : 1;
	uint8 dbgShowPageCheckered : 1;
	uint8 dbgShowPageMips;
	uint8 dbgShowPageSizes;
	uint8 dbgShowPageUsage;
	const uint8 * inPageHeader;
	const uint8 * inPageData;
	uint8 * tempData;
	uint8 * targetImage[3];
};

void TranscodePage_DXT_DXT( const transcodeParms_t * parms );
void TranscodePage_HDP_DXT( const transcodeParms_t * parms );
void TranscodePage_HDP_RAW( const transcodeParms_t * parms );
void TranscodePage( const transcodeParms_t * parms );

static_assert( sizeof( DCTHeader_t ) == 0x10, "DCTHeader_t disk ABI drift" );
static_assert( sizeof( HDPHeader_t ) == 0x10, "HDPHeader_t disk ABI drift" );
