#pragma once

#include "engine/renderer/jobs/transcode/transcode.h"

#include <cstddef>
#include <cstdint>

class idImage;

enum pageSource_t {
  PAGESOURCE_INVALID,
  PAGESOURCE_ALLOC0,
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
  QUADRANT_LOWER_LEFT,
  QUADRANT_LOWER_RIGHT,
  QUADRANT_UPPER_LEFT,
  QUADRANT_UPPER_RIGHT
};

struct texturePage_t {
  std::uint8_t mapped : 1;
  std::uint8_t locked : 1;
  bool dirty;
  std::uint8_t maxLevel;
  std::uint8_t pageLevel;
  std::uint16_t pageX;
  std::uint16_t pageY;
  pageSource_t pageSource;
  pageCompression_t pageCompression;
  pageQuadrant_t quadrant;
  unsigned int finerDiskOffset[4];
  std::uint16_t finerDiskLength[4];
  std::int16_t physicalPageX;
  std::int16_t physicalPageY;
  std::int16_t physicalPageNum;
  std::int16_t coarserPageNum;
  std::int16_t finerPageNum[4];
  int pad;
};

struct pageMinLodUpdate_t {
  const texturePage_t* pageArray;
  std::int16_t rootPageNum;
  std::uint8_t* pageFade;
  int fadeInSteps;
  idImage* pageMinLodImage;
  int pageMinLodImageSize;
  void* pageMinLodBits;
  int pageMinLodBytePitch;
  const void* pageMinLodPBO;
};

void UpdateMinLod_r(char* minLodTexture, int bytePitch,
                    const texturePage_t* pageArray, std::int16_t pageNum,
                    std::uint8_t* pageFade, int fadeSteps);
void UpdatePageMinLodJob(const pageMinLodUpdate_t* parms);
void UploadPageMinLod(const pageMinLodUpdate_t* parms);

static_assert(offsetof(texturePage_t, pageLevel) == 0x3,
              "texturePage_t level offset drift");
static_assert(offsetof(texturePage_t, finerDiskOffset) == 0x14,
              "texturePage_t disk offset drift");
static_assert(offsetof(texturePage_t, finerPageNum) == 0x34,
              "texturePage_t finer page offset drift");
static_assert(sizeof(texturePage_t) == 0x40, "texturePage_t ABI drift");
static_assert(offsetof(pageMinLodUpdate_t, pageFade) == 0x10,
              "pageMinLodUpdate_t fade offset drift");
static_assert(offsetof(pageMinLodUpdate_t, pageMinLodImage) == 0x20,
              "pageMinLodUpdate_t image offset drift");
static_assert(offsetof(pageMinLodUpdate_t, pageMinLodPBO) == 0x40,
              "pageMinLodUpdate_t PBO offset drift");
static_assert(sizeof(pageMinLodUpdate_t) == 0x48,
              "pageMinLodUpdate_t ABI drift");
