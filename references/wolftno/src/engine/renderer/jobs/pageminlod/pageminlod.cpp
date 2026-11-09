#include "engine/renderer/jobs/pageminlod/pageminlod.h"

#include "engine/renderer/gl_api.h"
#include "engine/renderer/image.h"
#include "shared/idlib/parallelism/paralleljoblist.h"

#include <cstdint>
#include <cstring>

void UpdateMinLod_r(char* minLodTexture, int bytePitch,
                    const texturePage_t* pageArray, std::int16_t pageNum,
                    std::uint8_t* pageFade, int fadeSteps) {
  const bool wasFading = pageFade[pageNum] != 0;
  int fade = pageFade[pageNum];
  if (fade != 0)
    pageFade[pageNum] = static_cast<std::uint8_t>(--fade);

  const texturePage_t& page = pageArray[pageNum];
  const int pageSize = 1 << page.pageLevel;
  const std::uint8_t minLod = static_cast<std::uint8_t>(
      16 * page.pageLevel + 16 * fade / fadeSteps);
  char* row = minLodTexture + bytePitch * (page.pageY << page.pageLevel) +
              (page.pageX << page.pageLevel);
  for (int y = 0; y < pageSize; ++y) {
    std::memset(row, minLod, static_cast<std::size_t>(pageSize));
    row += bytePitch;
  }

  if (!wasFading) {
    for (std::int16_t finerPage : page.finerPageNum) {
      if (finerPage != -1)
        UpdateMinLod_r(minLodTexture, bytePitch, pageArray, finerPage,
                       pageFade, fadeSteps);
    }
  }
}

void UpdatePageMinLodJob(const pageMinLodUpdate_t* parms) {
  UpdateMinLod_r(static_cast<char*>(parms->pageMinLodBits),
                 parms->pageMinLodBytePitch, parms->pageArray,
                 parms->rootPageNum, parms->pageFade, parms->fadeInSteps);
}

void UploadPageMinLod(const pageMinLodUpdate_t* parms) {
  _glewBindBufferARB(
      0x88ECu,
      static_cast<unsigned int>(reinterpret_cast<std::uintptr_t>(parms->pageMinLodPBO)));
  glBindTexture(0x0DE1u, parms->pageMinLodImage->oglTexture);
  glTexSubImage2D(0x0DE1u, 0, 0, 0, parms->pageMinLodImageSize,
                  parms->pageMinLodImageSize,
                  parms->pageMinLodImage->dataFormat,
                  parms->pageMinLodImage->dataType, nullptr);
  _glewBindBufferARB(0x88ECu, 0);
}

idParallelJobRegistration register_UpdatePageMinLodJob(
    reinterpret_cast<jobRun_t>(UpdatePageMinLodJob),
    "UpdatePageMinLodJob");

