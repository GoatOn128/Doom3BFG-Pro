#pragma once

#include <cstddef>
#include <cstdint>

enum pageCompression_t {
  COMP_INVALID,
  COMP_NONE,
  COMP_DXT,
  COMP_LZW,
  COMP_DCT,
  COMP_HDP,
  COMP_JXR,
  COMP_MAX_COMPRESSIONS
};

struct DCTHeader_t {
  std::uint8_t qualityLuma;
  std::uint8_t qualityChroma;
  std::uint8_t qualityNormal;
  std::uint8_t qualitySpecular;
  std::uint8_t qualityPower;
  std::uint8_t flags;
  std::uint16_t diffuseSize;
  std::uint16_t normalSize;
  std::uint16_t specularSize;
  std::uint16_t powerSize;
  std::uint16_t alphaSize;
};

struct HDPHeader_t {
  std::uint8_t qualityDiffuse;
  std::uint8_t qualityNormal;
  std::uint8_t qualitySpecular;
  std::uint8_t qualityPower;
  std::uint8_t flags;
  std::uint8_t pad;
  std::uint16_t diffuseSize;
  std::uint16_t normalSize;
  std::uint16_t specularSize;
  std::uint16_t powerSize;
  std::uint16_t alphaSize;
};

using JXRHeader_t = HDPHeader_t;

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
  std::uint16_t upsampleSharpen;
  std::uint16_t upsampleNoise;
  bool stripPageBorders;
  std::uint8_t tiled : 1;
  std::uint8_t directmapped : 1;
  std::uint8_t dbgShowPageSources : 1;
  std::uint8_t dbgShowPageColors : 1;
  std::uint8_t dbgShowPageNumbers : 1;
  std::uint8_t dbgShowPageInfo : 1;
  std::uint8_t dbgShowPageBorders : 1;
  std::uint8_t dbgShowPageCheckered : 1;
  std::uint8_t dbgShowPageMips;
  std::uint8_t dbgShowPageSizes;
  std::uint8_t dbgShowPageUsage;
  const std::uint8_t* inPageHeader;
  const std::uint8_t* inPageData;
  std::uint8_t* tempData;
  std::uint8_t* targetImage[3];
};

void TranscodePage_DXT_DXT(const transcodeParms_t* parms);
void TranscodePage_DXT_DXT_upsample(const transcodeParms_t* parms);
void TranscodePage_HDP_DXT(const transcodeParms_t* parms);
void TranscodePage_HDP_DXT_upsample(const transcodeParms_t* parms);
void TranscodePage_HDP_RAW(const transcodeParms_t* parms);
void TranscodePage_HDP_RAW_upsample(const transcodeParms_t* parms);
void TranscodePage_JXR_DXT(const transcodeParms_t* parms);
void TranscodePage_JXR_DXT_upsample(const transcodeParms_t* parms);
void TranscodePage_DCT_DXT(const transcodeParms_t* parms);
void TranscodePage_DCT_RAW(const transcodeParms_t* parms);
void TranscodePage_DCT_RAW_upsample(const transcodeParms_t* parms);
void TranscodePage_DCT_DXT_upsample(const transcodeParms_t* parms);
void TranscodePage(const transcodeParms_t* parms);
void UpSample16x16(std::uint8_t* destination, unsigned int destinationX,
                   unsigned int destinationY,
                   unsigned int destinationBytePitch,
                   const std::uint8_t* source, unsigned int sourceX,
                   unsigned int sourceY, unsigned int sourceMaskY,
                   int upsampleLevel, int upsampleFilter, int sharpenScale,
                   int noiseScale);
void StorePower16x16(const std::uint8_t* power, int tileX, int tileY,
                     std::uint8_t* rgba);
void StoreSpecularScale16x16(const std::uint8_t* specularScale, int tileX,
                             int tileY, std::uint8_t* rgba);

static_assert(sizeof(DCTHeader_t) == 0x10, "DCTHeader_t ABI drift");
static_assert(offsetof(DCTHeader_t, diffuseSize) == 0x6,
              "DCTHeader_t diffuse size offset drift");
static_assert(sizeof(HDPHeader_t) == 0x10, "HDPHeader_t ABI drift");
static_assert(offsetof(HDPHeader_t, diffuseSize) == 0x6,
              "HDPHeader_t diffuse size offset drift");

static_assert(offsetof(transcodeParms_t, targetBytePitch) == 0x40,
              "transcodeParms_t target pitch offset drift");
static_assert(offsetof(transcodeParms_t, inPageHeader) == 0x68,
              "transcodeParms_t header offset drift");
static_assert(offsetof(transcodeParms_t, targetImage) == 0x80,
              "transcodeParms_t target image offset drift");
static_assert(sizeof(transcodeParms_t) == 0x98, "transcodeParms_t ABI drift");
