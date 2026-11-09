#include "engine/renderer/jobs/pageupdate/pageupdate.h"

#include "engine/renderer/gl_api.h"
#include "engine/renderer/image.h"
#include "engine/renderer/renderlog.h"
#include "engine/renderer/rendersystem_ogl_pc.h"
#include "shared/idlib/lib.h"
#include "shared/idlib/parallelism/paralleljoblist.h"

#include <algorithm>
#include <array>
#include <cstdint>
#include <cstdio>
#include <cstring>
#include <immintrin.h>
#include <utility>

bool useMultiTexSubImage = true;
bool useMappingImageShadow = true;
bool usePageTableImageShadow = true;

class idScopedRenderLogRecorderEvent {
public:
  explicit idScopedRenderLogRecorderEvent(const char* name) {
    RenderEventRecorderLightStartEvent_Impl(name);
  }
  ~idScopedRenderLogRecorderEvent();
};

idScopedRenderLogRecorderEvent::~idScopedRenderLogRecorderEvent() {
  RenderEventRecorderLightStopEvent_Impl();
}

template <typename T> class idSort {
public:
  virtual ~idSort() = default;
  virtual void Sort(T* base, unsigned int num) = 0;
};

template <typename T, typename Derived>
class idSort_Quick : public idSort<T> {
public:
  __declspec(noinline) void Sort(T* base, unsigned int num) override {
    if (num <= 1) {
      return;
    }

    constexpr std::int64_t MAX_LEVELS = 128;
    std::int64_t lo[MAX_LEVELS];
    std::int64_t hi[MAX_LEVELS];
    std::int64_t low = 0;
    std::int64_t high = num - 1;
    std::int64_t level = 0;

    for (;;) {
      std::int64_t i = low;
      std::int64_t j = high;
      if (j - i >= 4 && level < MAX_LEVELS - 1) {
        const std::int64_t pivotIndex = (i + j) / 2;
        std::swap(base[j], base[pivotIndex]);
        T& pivot = base[j--];

        do {
          while (static_cast<const Derived*>(this)->Compare(base[i], pivot) <
                 0) {
            if (++i >= j) {
              break;
            }
          }
          while (static_cast<const Derived*>(this)->Compare(base[j], pivot) >
                 0) {
            if (--j <= i) {
              break;
            }
          }
          if (i >= j) {
            break;
          }
          std::swap(base[i], base[j]);
        } while (++i < --j);

        while (static_cast<const Derived*>(this)->Compare(base[i], pivot) <=
                   0 &&
               i < high) {
          ++i;
        }
        while (static_cast<const Derived*>(this)->Compare(base[j], pivot) >=
                   0 &&
               low < j) {
          --j;
        }

        std::swap(pivot, base[i]);
        if (high - i <= j - low) {
          lo[level] = low;
          hi[level++] = j;
          low = i;
        } else {
          hi[level] = high;
          lo[level++] = i;
          high = j;
        }
      } else {
        for (; i < j; --j) {
          std::int64_t maximum = i;
          for (std::int64_t index = i + 1; index <= j; ++index) {
            if (static_cast<const Derived*>(this)->Compare(base[index],
                                                            base[maximum]) >
                0) {
              maximum = index;
            }
          }
          std::swap(base[maximum], base[j]);
        }
        if (--level < 0) {
          return;
        }
        low = lo[level];
        high = hi[level];
      }
    }
  }
};

namespace {

constexpr unsigned int GL_TEXTURE_2D_VALUE = 0x0DE1u;
constexpr unsigned int GL_PIXEL_UNPACK_BUFFER_VALUE = 0x88ECu;
constexpr unsigned int GL_UNPACK_ROW_LENGTH_VALUE = 0x0CF2u;
constexpr unsigned int GL_MAP_WRITE_BIT_VALUE = 0x0002u;
constexpr unsigned int GL_MAP_INVALIDATE_RANGE_BIT_VALUE = 0x0004u;
constexpr unsigned int GL_MAP_UNSYNCHRONIZED_BIT_VALUE = 0x0020u;

unsigned int BufferName(const void* buffer) {
  return static_cast<unsigned int>(reinterpret_cast<std::uintptr_t>(buffer));
}

const void* OffsetPointer(const void* pointer, int offset) {
  return reinterpret_cast<const void*>(
      reinterpret_cast<std::uintptr_t>(pointer) +
      static_cast<std::uintptr_t>(offset));
}

unsigned int PageTableMapFlags(const pageImagesUpdateParms_t& parms) {
  unsigned int flags = GL_MAP_WRITE_BIT_VALUE;
  if (parms.useInvalidateRangeBitForPageTablePBOMap) {
    flags |= GL_MAP_INVALIDATE_RANGE_BIT_VALUE;
  }
  if (parms.useUnsynchronizedPageTablePBOs) {
    flags |= GL_MAP_UNSYNCHRONIZED_BIT_VALUE;
  }
  return flags;
}

template <typename T>
int ComparePointers(const T* left, const T* right) {
  const std::uintptr_t leftValue = reinterpret_cast<std::uintptr_t>(left);
  const std::uintptr_t rightValue = reinterpret_cast<std::uintptr_t>(right);
  return leftValue < rightValue ? -1 : leftValue > rightValue ? 1 : 0;
}

struct imageRect_t {
  int minX;
  int maxX;
  int minY;
  int maxY;
};

void ClearRects(std::array<imageRect_t, 14>& rects) {
  for (imageRect_t& rect : rects) {
    rect.minX = 0x7fffffff;
    rect.maxX = static_cast<int>(0x80000000u);
    rect.minY = 0x7fffffff;
    rect.maxY = static_cast<int>(0x80000000u);
  }
}

bool HasArea(const imageRect_t& rect) {
  return rect.minX < rect.maxX && rect.minY < rect.maxY;
}

}  // namespace

void CopyImageRect(std::uint8_t* destination, const std::uint8_t* source,
                   unsigned int numScanlines, unsigned int byteWidth,
                   int bytePitch) {
  if (static_cast<int>(byteWidth) < 16 ||
      (reinterpret_cast<std::uintptr_t>(destination) & 15u) != 0) {
    int offset = 0;
    if (static_cast<int>(numScanlines) > 0) {
      for (unsigned int row = 0; row < numScanlines; ++row) {
        std::memmove(destination + offset, source + offset,
                     static_cast<std::size_t>(static_cast<int>(byteWidth)));
        offset += bytePitch;
      }
    }
    return;
  }

  int offset = 0;
  if (static_cast<int>(numScanlines) > 0) {
    for (unsigned int row = 0; row < numScanlines; ++row) {
      int scanOffset = offset;
      unsigned int remaining = byteWidth;
      while (remaining >= 64) {
        _mm_stream_si128(
            reinterpret_cast<__m128i*>(destination + scanOffset),
            _mm_load_si128(
                reinterpret_cast<const __m128i*>(source + scanOffset)));
        _mm_stream_si128(
            reinterpret_cast<__m128i*>(destination + scanOffset + 16),
            _mm_load_si128(
                reinterpret_cast<const __m128i*>(source + scanOffset + 16)));
        _mm_stream_si128(
            reinterpret_cast<__m128i*>(destination + scanOffset + 32),
            _mm_load_si128(
                reinterpret_cast<const __m128i*>(source + scanOffset + 32)));
        _mm_stream_si128(
            reinterpret_cast<__m128i*>(destination + scanOffset + 48),
            _mm_load_si128(
                reinterpret_cast<const __m128i*>(source + scanOffset + 48)));
        scanOffset += 64;
        remaining -= 64;
      }
      while (remaining >= 16) {
        _mm_stream_si128(
            reinterpret_cast<__m128i*>(destination + scanOffset),
            _mm_load_si128(
                reinterpret_cast<const __m128i*>(source + scanOffset)));
        scanOffset += 16;
        remaining -= 16;
      }
      offset += bytePitch;
    }
  }
  _mm_sfence();
}

void UpdatePageMappings_PC(const pageImagesUpdateParms_t* parms) {
  for (int index = 0; index < parms->numUpdates; ++index) {
    pageImagesUpdate_t& update = parms->updates[index];
    if (update.cancelled || update.physicalMappingsImageInfo.bits == nullptr) {
      continue;
    }
    std::memcpy(static_cast<std::uint8_t*>(
                    update.physicalMappingsImageInfo.bits) +
                    update.physicalMappingsImageInfo.bytePitch * update.physY +
                    16 * update.physX,
                update.map, sizeof(update.map));
  }
}

void UpdatePageTables_PC(const pageImagesUpdateParms_t* parms) {
  for (int index = 0; index < parms->numUpdates; ++index) {
    pageImagesUpdate_t& update = parms->updates[index];
    if (update.cancelled || update.pageTableImageInfo[0].bits == nullptr) {
      continue;
    }
    for (int level = 0; level <= update.effectivePageLevel; ++level) {
      const int shift = update.effectivePageLevel - level;
      const int size = 1 << shift;
      pageImageInfo_t& info = update.pageTableImageInfo[level];
      std::uint8_t* row = static_cast<std::uint8_t*>(info.bits) +
                          2 * (update.spotX << shift) +
                          info.bytePitch * (update.spotY << shift);
      if (size == 1) {
        std::memcpy(row, update.pageSpot, sizeof(std::uint16_t));
      } else if (size >= 8) {
        const __m128i value = _mm_set1_epi32(
            static_cast<int>(*reinterpret_cast<const std::uint32_t*>(
                update.pageSpot)));
        for (int y = 0; y < size; ++y) {
          auto* destination = reinterpret_cast<__m128i*>(row);
          for (int block = 0; block < (size + 7) / 8; ++block) {
            _mm_storeu_si128(destination++, value);
          }
          row += info.bytePitch;
        }
      } else {
        const std::uint32_t value =
            *reinterpret_cast<const std::uint32_t*>(update.pageSpot);
        for (int y = 0; y < size; ++y) {
          auto* destination = reinterpret_cast<std::uint32_t*>(row);
          for (int block = 0; block < (size + 1) / 2; ++block) {
            destination[block] = value;
          }
          row += info.bytePitch;
        }
      }
    }
  }
}

void UpdatePageImagesJob(const pageImagesUpdateParms_t* parms) {
  UpdatePageMappings_PC(parms);
  UpdatePageTables_PC(parms);
}

void UploadPhysicalPages(const pageImagesUpdateParms_t** parms,
                         unsigned int numParms) {
  idScopedRenderLogRecorderEvent event("UploadPhysicalPages");

  if (useMultiTexSubImage && glTexMultiSubImage2DAMD != nullptr &&
      glCompressedTexMultiSubImage2DAMD != nullptr) {
    struct sortedPhysicalImageUpdate_t {
      const void* pageDataPBO;
      idImage* physicalImage;
      const pageImagesUpdate_t* update;
      int dataSize;
      int dataOffset;
      int mipLevel;
      int alignmentPadding;
    };
    static_assert(sizeof(sortedPhysicalImageUpdate_t) == 40,
                  "physical page sort ABI drift");
    class idPageUpdateSort final
        : public idSort_Quick<sortedPhysicalImageUpdate_t,
                              idPageUpdateSort> {
    public:
      int Compare(const sortedPhysicalImageUpdate_t& left,
                  const sortedPhysicalImageUpdate_t& right) const {
        int result = ComparePointers(left.pageDataPBO, right.pageDataPBO);
        if (result == 0) {
          result = ComparePointers(left.physicalImage, right.physicalImage);
        }
        if (result == 0) {
          result = left.mipLevel < right.mipLevel
                       ? -1
                       : left.mipLevel > right.mipLevel ? 1 : 0;
        }
        if (result == 0) {
          result = ComparePointers(left.update, right.update);
        }
        return result;
      }
      ~idPageUpdateSort() override = default;
    };

    std::array<sortedPhysicalImageUpdate_t, 768> sortedUpdates{};
    int numSortedUpdates = 0;
    const auto append = [&](const pageImagesUpdate_t& update, idImage* image,
                            int compressedSize, int uncompressedSize,
                            int compressedOffset, int uncompressedOffset) {
      sortedPhysicalImageUpdate_t& sorted = sortedUpdates[numSortedUpdates++];
      sorted.pageDataPBO = update.pageDataPBO;
      sorted.physicalImage = image;
      sorted.update = &update;
      sorted.dataSize = update.compressed ? compressedSize : uncompressedSize;
      sorted.dataOffset = update.type == PAGE_UPDATE_UNMAP_PRT
                              ? 0
                              : update.compressed ? compressedOffset
                                                  : uncompressedOffset;
      sorted.mipLevel = update.mipLevel;
    };
    for (unsigned int parmIndex = 0; parmIndex < numParms; ++parmIndex) {
      const pageImagesUpdateParms_t& currentParms = *parms[parmIndex];
      if (currentParms.skipPhysicalTextureUploads) {
        continue;
      }
      for (int index = 0; index < currentParms.numUpdates; ++index) {
        const pageImagesUpdate_t& update = currentParms.updates[index];
        if (update.cancelled ||
            (update.pageData == nullptr && update.pageDataPBO == nullptr &&
             update.type != PAGE_UPDATE_UNMAP_PRT)) {
          continue;
        }
        if (update.physicalPagesImage0 != nullptr) {
          append(update, update.physicalPagesImage0, 0x2000, 0x10000, 0x8000,
                 0x20000);
        }
        if (update.physicalPagesImage1 != nullptr) {
          append(update, update.physicalPagesImage1, 0x4000, 0x10000, 0, 0);
        }
        if (update.physicalPagesImage2 != nullptr) {
          append(update, update.physicalPagesImage2, 0x4000, 0x10000, 0x4000,
                 0x10000);
        }
      }
    }
    if (numSortedUpdates == 0) {
      return;
    }

    idPageUpdateSort sorter;
    sorter.Sort(sortedUpdates.data(),
                static_cast<unsigned int>(numSortedUpdates));
    const void* boundPBO = nullptr;
    int first = 0;
    while (first < numSortedUpdates) {
      int end = first + 1;
      while (end < numSortedUpdates &&
             sortedUpdates[end].pageDataPBO ==
                 sortedUpdates[first].pageDataPBO &&
             sortedUpdates[end].physicalImage ==
                 sortedUpdates[first].physicalImage &&
             sortedUpdates[end].mipLevel == sortedUpdates[first].mipLevel) {
        ++end;
      }

      std::array<const void*, 256> imageData{};
      std::array<int, 256> imageHeight{};
      std::array<int, 256> imageWidth{};
      std::array<int, 256> imageXOffset{};
      std::array<int, 256> imageYOffset{};
      std::array<int, 256> imageSize{};
      for (int index = first; index < end; ++index) {
        const int groupIndex = index - first;
        const sortedPhysicalImageUpdate_t& sorted = sortedUpdates[index];
        imageXOffset[groupIndex] = sorted.update->physX << 7;
        imageYOffset[groupIndex] = sorted.update->physY << 7;
        imageWidth[groupIndex] = 128;
        imageHeight[groupIndex] = 128;
        imageSize[groupIndex] = sorted.dataSize;
        imageData[groupIndex] =
            OffsetPointer(sorted.update->pageData, sorted.dataOffset);
      }

      const sortedPhysicalImageUpdate_t& group = sortedUpdates[first];
      if (group.pageDataPBO != boundPBO) {
        _glewBindBufferARB(GL_PIXEL_UNPACK_BUFFER_VALUE,
                           BufferName(group.pageDataPBO));
        boundPBO = group.pageDataPBO;
      }
      glBindTexture(GL_TEXTURE_2D_VALUE, group.physicalImage->oglTexture);
      char tag[64];
      const int count = end - first;
      if (group.physicalImage->IsCompressed()) {
        std::snprintf(tag, sizeof(tag),
                      "glCompressedTexMultiSubImage2DAMD (%d)", count);
        RenderEventRecorderLightStartEventCopyString_Impl(tag);
        glCompressedTexMultiSubImage2DAMD(
            GL_TEXTURE_2D_VALUE, group.mipLevel, count, imageXOffset.data(),
            imageYOffset.data(), imageWidth.data(), imageHeight.data(),
            group.physicalImage->internalFormat, imageSize.data(),
            imageData.data());
      } else {
        std::snprintf(tag, sizeof(tag), "glTexMultiSubImage2DAMD (%d)",
                      count);
        RenderEventRecorderLightStartEventCopyString_Impl(tag);
        glTexMultiSubImage2DAMD(
            GL_TEXTURE_2D_VALUE, group.mipLevel, count, imageXOffset.data(),
            imageYOffset.data(), imageWidth.data(), imageHeight.data(),
            group.physicalImage->dataFormat, group.physicalImage->dataType,
            imageData.data());
      }
      RenderEventRecorderLightStopEvent_Impl();
      first = end;
    }
    if (boundPBO != nullptr) {
      _glewBindBufferARB(GL_PIXEL_UNPACK_BUFFER_VALUE, 0);
    }
    return;
  }

  const void* boundPBO = nullptr;
  for (unsigned int parmIndex = 0; parmIndex < numParms; ++parmIndex) {
    const pageImagesUpdateParms_t& currentParms = *parms[parmIndex];
    for (int index = 0; index < currentParms.numUpdates; ++index) {
      const pageImagesUpdate_t& update = currentParms.updates[index];
      if (update.cancelled ||
          (update.pageData == nullptr && update.pageDataPBO == nullptr)) {
        continue;
      }
      if (update.pageDataPBO != boundPBO) {
        _glewBindBufferARB(GL_PIXEL_UNPACK_BUFFER_VALUE,
                           BufferName(update.pageDataPBO));
        boundPBO = update.pageDataPBO;
      }
      const int x = update.physX << 7;
      const int y = update.physY << 7;
      RenderEventRecorderLightStartEvent_Impl("SubImageUpload(x3)");
      if (update.physicalPagesImage0 != nullptr) {
        update.physicalPagesImage0->SubImageUpload(
            update.mipLevel, x, y, 0, 128, 128,
            OffsetPointer(update.pageData, update.compressed ? 0x8000
                                                             : 0x20000),
            0);
      }
      if (update.physicalPagesImage1 != nullptr) {
        update.physicalPagesImage1->SubImageUpload(update.mipLevel, x, y, 0,
                                                    128, 128, update.pageData,
                                                    0);
      }
      if (update.physicalPagesImage2 != nullptr) {
        update.physicalPagesImage2->SubImageUpload(
            update.mipLevel, x, y, 0, 128, 128,
            OffsetPointer(update.pageData,
                          update.compressed ? 0x4000 : 0x10000),
            0);
      }
      RenderEventRecorderLightStopEvent_Impl();
    }
  }
  if (boundPBO != nullptr) {
    _glewBindBufferARB(GL_PIXEL_UNPACK_BUFFER_VALUE, 0);
  }
}

void UploadPageMappings(const pageImagesUpdateParms_t** parms,
                        unsigned int numParms) {
  if (!useMappingImageShadow) {
    for (unsigned int parmIndex = 0; parmIndex < numParms; ++parmIndex) {
      const pageImagesUpdateParms_t& currentParms = *parms[parmIndex];
      for (int index = 0; index < currentParms.numUpdates; ++index) {
        const pageImagesUpdate_t& update = currentParms.updates[index];
        if (!update.cancelled && update.physicalMappingsImage != nullptr) {
          update.physicalMappingsImage->SetPixel(0, update.physX, update.physY,
                                                  update.map);
        }
      }
    }
    return;
  }

  struct sortedImageUpdate_t {
    idImage* mappingImage;
    const pageImagesUpdate_t* update;
  };
  class idPageUpdateSort final
      : public idSort_Quick<sortedImageUpdate_t, idPageUpdateSort> {
  public:
    int Compare(const sortedImageUpdate_t& left,
                const sortedImageUpdate_t& right) const {
      const int imageOrder =
          ComparePointers(left.mappingImage, right.mappingImage);
      return imageOrder != 0 ? imageOrder
                             : ComparePointers(left.update, right.update);
    }
    ~idPageUpdateSort() override = default;
  };

  std::array<sortedImageUpdate_t, 256> sortedUpdates{};
  int count = 0;
  for (unsigned int parmIndex = 0; parmIndex < numParms; ++parmIndex) {
    const pageImagesUpdateParms_t& currentParms = *parms[parmIndex];
    for (int index = 0; index < currentParms.numUpdates; ++index) {
      const pageImagesUpdate_t& update = currentParms.updates[index];
      if (!update.cancelled && update.physicalMappingsImage != nullptr) {
        sortedUpdates[count++] = {update.physicalMappingsImage, &update};
      }
    }
  }
  if (count == 0) {
    return;
  }
  idPageUpdateSort sorter;
  sorter.Sort(sortedUpdates.data(), static_cast<unsigned int>(count));

  const pageImagesUpdateParms_t& uploadParms = *parms[0];
  int first = 0;
  while (first < count) {
    int end = first + 1;
    int minX = sortedUpdates[first].update->physX;
    int maxX = minX + 1;
    int minY = sortedUpdates[first].update->physY;
    int maxY = minY + 1;
    while (end < count &&
           sortedUpdates[end].mappingImage ==
               sortedUpdates[first].mappingImage) {
      minX = std::min(minX, sortedUpdates[end].update->physX);
      maxX = std::max(maxX, sortedUpdates[end].update->physX + 1);
      minY = std::min(minY, sortedUpdates[end].update->physY);
      maxY = std::max(maxY, sortedUpdates[end].update->physY + 1);
      ++end;
    }
    const pageImagesUpdate_t& update = *sortedUpdates[end - 1].update;
    const int offset = 16 * minX +
                       minY * update.physicalMappingsImageInfo.bytePitch;
    if (uploadParms.usePageTablePBOs) {
      _glewBindBufferARB(GL_PIXEL_UNPACK_BUFFER_VALUE,
                         BufferName(update.physicalMappingsImagePBO));
      auto* mapped = static_cast<std::uint8_t*>(_glewMapBufferRange(
          GL_PIXEL_UNPACK_BUFFER_VALUE, 0,
          update.physicalMappingsImagePBOSize,
          PageTableMapFlags(uploadParms)));
      CopyImageRect(mapped + offset,
                    static_cast<const std::uint8_t*>(
                        update.physicalMappingsImageInfo.bits) +
                        offset,
                    static_cast<unsigned int>(maxY - minY),
                    static_cast<unsigned int>(16 * (maxX - minX)),
                    update.physicalMappingsImageInfo.bytePitch);
      _glewUnmapBufferARB(GL_PIXEL_UNPACK_BUFFER_VALUE);
    }
    idImage* image = sortedUpdates[first].mappingImage;
    glBindTexture(GL_TEXTURE_2D_VALUE, image->oglTexture);
    const void* pixels = uploadParms.usePageTablePBOs
                             ? OffsetPointer(nullptr, offset)
                             : static_cast<const std::uint8_t*>(
                                   update.physicalMappingsImageInfo.bits) +
                                   offset;
    glPixelStorei(GL_UNPACK_ROW_LENGTH_VALUE,
                  update.physicalMappingsImageInfo.bytePitch / 16);
    glTexSubImage2D(GL_TEXTURE_2D_VALUE, 0, minX, minY, maxX - minX,
                    maxY - minY, image->dataFormat, image->dataType, pixels);
    first = end;
  }
  glPixelStorei(GL_UNPACK_ROW_LENGTH_VALUE, 0);
  if (uploadParms.usePageTablePBOs) {
    _glewBindBufferARB(GL_PIXEL_UNPACK_BUFFER_VALUE, 0);
  }
}

void UploadPageTables(const pageImagesUpdateParms_t** parms,
                      unsigned int numParms) {
  if (!usePageTableImageShadow) {
    std::array<std::uint32_t, 256> buffer{};
    for (unsigned int parmIndex = 0; parmIndex < numParms; ++parmIndex) {
      const pageImagesUpdateParms_t& currentParms = *parms[parmIndex];
      for (int index = 0; index < currentParms.numUpdates; ++index) {
        const pageImagesUpdate_t& update = currentParms.updates[index];
        if (update.cancelled || update.pageTableImage == nullptr) {
          continue;
        }
        const std::uint32_t value =
            *reinterpret_cast<const std::uint32_t*>(update.pageSpot);
        buffer.fill(value);
        for (int level = 0; level <= update.effectivePageLevel; ++level) {
          const int shift = update.effectivePageLevel - level;
          const int size = 1 << shift;
          const int uploadSize = std::min(size, 16);
          const int baseX = update.spotX << shift;
          const int baseY = update.spotY << shift;
          for (int y = 0; y < size; y += 16) {
            for (int x = 0; x < size; x += 16) {
              update.pageTableImage->SubImageUpload(
                  level, baseX + x, baseY + y, 0, uploadSize, uploadSize,
                  buffer.data(), 0);
            }
          }
        }
      }
    }
    return;
  }

  struct sortedImageUpdate_t {
    idImage* pageTableImage;
    const pageImagesUpdate_t* update;
  };
  class idPageUpdateSort final
      : public idSort_Quick<sortedImageUpdate_t, idPageUpdateSort> {
  public:
    int Compare(const sortedImageUpdate_t& left,
                const sortedImageUpdate_t& right) const {
      const int imageOrder =
          ComparePointers(left.pageTableImage, right.pageTableImage);
      return imageOrder != 0 ? imageOrder
                             : ComparePointers(left.update, right.update);
    }
    ~idPageUpdateSort() override = default;
  };

  std::array<sortedImageUpdate_t, 256> sortedUpdates{};
  int count = 0;
  for (unsigned int parmIndex = 0; parmIndex < numParms; ++parmIndex) {
    const pageImagesUpdateParms_t& currentParms = *parms[parmIndex];
    for (int index = 0; index < currentParms.numUpdates; ++index) {
      const pageImagesUpdate_t& update = currentParms.updates[index];
      if (update.pageTableImage != nullptr) {
        sortedUpdates[count++] = {update.pageTableImage, &update};
      }
    }
  }
  if (count == 0) {
    return;
  }
  idPageUpdateSort sorter;
  sorter.Sort(sortedUpdates.data(), static_cast<unsigned int>(count));

  const pageImagesUpdateParms_t& uploadParms = *parms[0];
  int first = 0;
  while (first < count) {
    std::array<imageRect_t, 14> rects{};
    ClearRects(rects);
    int end = first;
    while (end < count &&
           sortedUpdates[end].pageTableImage ==
               sortedUpdates[first].pageTableImage) {
      const pageImagesUpdate_t& update = *sortedUpdates[end].update;
      for (int level = 0; level <= update.effectivePageLevel; ++level) {
        const int shift = update.effectivePageLevel - level;
        const int minX = update.spotX << shift;
        const int minY = update.spotY << shift;
        const int size = 1 << shift;
        rects[level].minX = std::min(rects[level].minX, minX);
        rects[level].maxX = std::max(rects[level].maxX, minX + size);
        rects[level].minY = std::min(rects[level].minY, minY);
        rects[level].maxY = std::max(rects[level].maxY, minY + size);
      }
      ++end;
    }

    idImage* image = sortedUpdates[first].pageTableImage;
    const pageImagesUpdate_t& update = *sortedUpdates[end - 1].update;
    if (uploadParms.usePageTablePBOs) {
      _glewBindBufferARB(GL_PIXEL_UNPACK_BUFFER_VALUE,
                         BufferName(update.pageTableImagePBO));
      auto* mapped = static_cast<std::uint8_t*>(_glewMapBufferRange(
          GL_PIXEL_UNPACK_BUFFER_VALUE, 0, update.pageTableImagePBOSize,
          PageTableMapFlags(uploadParms)));
      int pboLevelOffset = 0;
      int levelHeight = image->opts.height;
      for (int level = 0; level < image->opts.numLevels; ++level) {
        imageRect_t& rect = rects[level];
        if (!HasArea(rect)) {
          continue;
        }
        rect.minX &= ~7;
        rect.minY &= ~7;
        rect.maxX = std::min((rect.maxX + 7) & ~7,
                             image->opts.width >> level);
        rect.maxY = std::min((rect.maxY + 7) & ~7,
                             image->opts.height >> level);
        const pageImageInfo_t& info = update.pageTableImageInfo[level];
        const int rowOffset = rect.minY * info.bytePitch + 2 * rect.minX;
        CopyImageRect(mapped + pboLevelOffset + rowOffset,
                      static_cast<const std::uint8_t*>(info.bits) + rowOffset,
                      static_cast<unsigned int>(rect.maxY - rect.minY),
                      static_cast<unsigned int>(2 * (rect.maxX - rect.minX)),
                      info.bytePitch);
        pboLevelOffset += levelHeight * info.bytePitch;
        levelHeight >>= 1;
      }
      _glewUnmapBufferARB(GL_PIXEL_UNPACK_BUFFER_VALUE);
    }

    glBindTexture(GL_TEXTURE_2D_VALUE, image->oglTexture);
    int pboLevelOffset = 0;
    int levelHeight = image->opts.height;
    for (int level = 0; level < image->opts.numLevels; ++level) {
      imageRect_t& rect = rects[level];
      if (!HasArea(rect)) {
        continue;
      }
      const pageImageInfo_t& info = update.pageTableImageInfo[level];
      const int rowOffset = rect.minY * info.bytePitch + 2 * rect.minX;
      const void* pixels = uploadParms.usePageTablePBOs
                               ? OffsetPointer(nullptr,
                                               pboLevelOffset + rowOffset)
                               : static_cast<const std::uint8_t*>(info.bits) +
                                     rowOffset;
      glPixelStorei(GL_UNPACK_ROW_LENGTH_VALUE, info.bytePitch / 2);
      glTexSubImage2D(GL_TEXTURE_2D_VALUE, level, rect.minX, rect.minY,
                      rect.maxX - rect.minX, rect.maxY - rect.minY,
                      image->dataFormat, image->dataType, pixels);
      pboLevelOffset += levelHeight * info.bytePitch;
      levelHeight >>= 1;
      rect.minX = 0x7fffffff;
      rect.maxX = static_cast<int>(0x80000000u);
      rect.minY = 0x7fffffff;
      rect.maxY = static_cast<int>(0x80000000u);
    }
    first = end;
  }
  glPixelStorei(GL_UNPACK_ROW_LENGTH_VALUE, 0);
  if (uploadParms.usePageTablePBOs) {
    _glewBindBufferARB(GL_PIXEL_UNPACK_BUFFER_VALUE, 0);
  }
}

void UploadPageImages(const pageImagesUpdateParms_t* parms1,
                      const pageImagesUpdateParms_t* parms2) {
  if (idLib::headless) {
    return;
  }
  const pageImagesUpdateParms_t* parms[2] = {parms1, parms2};
  const unsigned int numParms = parms2 != nullptr ? 2u : 1u;
  for (unsigned int index = 0; index < numParms; ++index) {
    if (!parms[index]->submittedJob) {
      UpdatePageMappings_PC(parms[index]);
      UpdatePageTables_PC(parms[index]);
    }
  }
  UploadPhysicalPages(parms, numParms);
  UploadPageMappings(parms, numParms);
  UploadPageTables(parms, numParms);
}

const __m128 vector_float_abs_mask = _mm_castsi128_ps(
    _mm_setr_epi32(0x7fffffff, 0x7fffffff, 0x7fffffff, 0x7fffffff));
const __m128 vector_float_sign_bit = _mm_castsi128_ps(
    _mm_setr_epi32(static_cast<int>(0x80000000u),
                   static_cast<int>(0x80000000u),
                   static_cast<int>(0x80000000u),
                   static_cast<int>(0x80000000u)));
const __m128 vector_float_last_sign_bit = _mm_castsi128_ps(
    _mm_setr_epi32(0, 0, 0, static_cast<int>(0x80000000u)));
const __m128 vector_float_first_sign_bit = _mm_castsi128_ps(
    _mm_setr_epi32(static_cast<int>(0x80000000u), 0, 0, 0));
const __m128 vector_float_clear_last = _mm_castsi128_ps(
    _mm_setr_epi32(-1, -1, -1, 0));
const __m128 DC_OFFSET_V128 =
    _mm_castsi128_ps(_mm_set1_epi32(0x26901d7d));

idParallelJobRegistration register_UpdatePageImagesJob(
    reinterpret_cast<jobRun_t>(UpdatePageImagesJob),
    "UpdatePageImagesJob");
