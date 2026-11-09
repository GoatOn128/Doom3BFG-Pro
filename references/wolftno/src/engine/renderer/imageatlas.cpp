#include "engine/renderer/atlasresource.h"

#include "engine/renderer/binaryimage.h"
#include "engine/renderer/image.h"
#include "engine/renderer/imagemanager.h"
#include "engine/renderer/imagetools/image_files.h"
#include "shared/idlib/containers/BitArray.h"
#include "shared/idlib/filesystem/filesystemlocal.h"
#include "shared/idlib/lib.h"
#include "shared/idlib/sys/sys_alloc.h"

#include <algorithm>
#include <cstddef>
#include <cstdint>
#include <cstring>
#include <new>

idTypedResourceList<idAtlasResource> idAtlasResource::resourceList("atlas");

idAtlasResource::idAtlasResource()
    : img_tiles_wide(0), img_tiles_high(0), img_x(0), img_y(0) {}

idAtlasResource::~idAtlasResource() {
  if (globalImages != nullptr && globalImages->transSortImageAtlas != nullptr) {
    FreeBlock(globalImages->transSortImageAtlas->tileAlloc, &img_x, &img_y,
              img_tiles_wide, img_tiles_high);
  }
}

idResourceList* idAtlasResource::GetResourceList() const {
  return &resourceList;
}

bool idAtlasResource::ReloadIfStale() {
  idStr sourceName(name.str);
  idStr extension;
  sourceName.ExtractFileExtension(extension);
  if (extension.IsEmpty()) {
    for (int index = 0; imageExtensions[index] != nullptr; ++index) {
      idStr candidate;
      candidate.Format("%s.%s", sourceName.data, imageExtensions[index]);
      if (fileSystem->FileExists(candidate.data, false)) {
        sourceName = candidate;
        break;
      }
    }
  }

  const unsigned int sourceTimestamp =
      fileSystem->GetTimestamp(sourceName.data, false);
  if (sourceTimestamp == ~0U || sourceTimestamp == binaryTimestamp) {
    return false;
  }

  idStr imageName;
  imageName.Format("atlasBorder( %s )", name.str);
  char binaryFileName[256];
  idImage::MakeGeneratedName(binaryFileName, sizeof(binaryFileName),
                             imageName.data);
  fileSystem->RemoveFile(binaryFileName, FSPATH_BASE);
  LoadResource();
  return true;
}

void idAtlasResource::List() const {
  idImageAtlas* atlas = globalImages->transSortImageAtlas;
  idLib::Printf("%4i , %4i : %4i x %4i %s\n",
                atlas->ATLAS_TILE_WIDTH * img_x,
                atlas->ATLAS_TILE_WIDTH * img_y,
                atlas->ATLAS_TILE_WIDTH * img_tiles_wide,
                atlas->ATLAS_TILE_WIDTH * img_tiles_high, name.str);
}

bool CheckForSpace(const idBitArray& tileAlloc, int x, int y, int width,
                   int height) {
  for (int column = 0; column < width; ++column) {
    for (int row = 0; row < height; ++row) {
      const int index = x + column +
                        (y + row) *
                            globalImages->transSortImageAtlas->ATLAS_TILES_WIDE;
      if (index >= 0 && static_cast<std::size_t>(index) < tileAlloc.bits &&
          (tileAlloc.buffer[static_cast<std::size_t>(index) >> 3U] &
           static_cast<unsigned char>(1U << (index & 7))) != 0) {
        return false;
      }
    }
  }
  return true;
}

bool AllocBlock(idBitArray& tileAlloc, int* x, int* y, int width,
                int height) {
  *y = 0;
  idImageAtlas* atlas = globalImages->transSortImageAtlas;
  if (atlas->ATLAS_TILES_HIGH - height < 0) {
    return false;
  }

  while (true) {
    *x = 0;
    while (*x <= atlas->ATLAS_TILES_WIDE - width) {
      if (CheckForSpace(tileAlloc, *x, *y, width, height)) {
        for (int column = 0; column < width; ++column) {
          for (int row = 0; row < height; ++row) {
            const int index = *x + column +
                              (*y + row) * atlas->ATLAS_TILES_WIDE;
            if (index >= 0 &&
                static_cast<std::size_t>(index) < tileAlloc.bits) {
              tileAlloc.buffer[static_cast<std::size_t>(index) >> 3U] |=
                  static_cast<unsigned char>(1U << (index & 7));
            }
          }
        }
        return true;
      }
      ++*x;
      atlas = globalImages->transSortImageAtlas;
    }
    ++*y;
    atlas = globalImages->transSortImageAtlas;
    if (*y > atlas->ATLAS_TILES_HIGH - height) {
      return false;
    }
  }
}

void FreeBlock(idBitArray& tileAlloc, int* x, int* y, int width, int height) {
  for (int column = 0; column < width; ++column) {
    for (int row = 0; row < height; ++row) {
      const int index = *x + column +
                        (*y + row) *
                            globalImages->transSortImageAtlas->ATLAS_TILES_WIDE;
      if (index >= 0 && static_cast<std::size_t>(index) < tileAlloc.bits) {
        tileAlloc.buffer[static_cast<std::size_t>(index) >> 3U] &=
            static_cast<unsigned char>(~(1U << (index & 7)));
      }
    }
  }
}

idImageAtlas::idImageAtlas(const char* atlasName, int sizeLog2) {
  tileAlloc.buffer = nullptr;
  tileAlloc.bits = 0;
  tileAlloc.memTag = TAG_BITARRAY;
  tileAlloc.free = false;
  ATLAS_HEIGHT_BITS = sizeLog2;
  ATLAS_WIDTH_BITS = sizeLog2;
  ATLAS_MIP_LEVELS = 1;
  ATLAS_TILE_WIDTH_BITS = 3;
  ATLAS_TILE_HEIGHT_BITS = 3;
  ATLAS_WIDTH = 1 << sizeLog2;
  ATLAS_HEIGHT = 1 << sizeLog2;
  ATLAS_TILE_WIDTH = 8;
  ATLAS_TILE_HEIGHT = 8;
  ATLAS_TILES_WIDE = 1 << (sizeLog2 - 3);
  ATLAS_TILES_HIGH = ATLAS_TILES_WIDE;
  ATLAS_TILES = ATLAS_TILES_WIDE * ATLAS_TILES_WIDE;
  tileAlloc.Alloc(static_cast<std::uint64_t>(ATLAS_TILES));
  if (tileAlloc.buffer != nullptr) {
    std::memset(tileAlloc.buffer, 0, (tileAlloc.bits + 7U) >> 3U);
  }

  idImageOpts options{};
  options.textureType = TT_2D;
  options.format = FMT_DXT5;
  options.colorFormat = CFM_DEFAULT;
  options.width = ATLAS_WIDTH;
  options.height = ATLAS_HEIGHT;
  options.numLevels = ATLAS_MIP_LEVELS;
  options.packedTail = true;
  options.textureBindType = TB_SHADER;
  options.textureUsage = TU_DEFAULT;
  options.textureOptions = TO_NONE;
  options.sampler.filter = TF_LINEAR_MIPMAP_NEAREST;
  options.sampler.repeat = TR_REPEAT;
  options.sampler.lodMaxClamp = 13.0F;
  atlas = globalImages->ScratchImage(atlasName, options);

  const int byteCount = options.width * options.height;
  unsigned char* clear = static_cast<unsigned char*>(
      idMem::Singleton()->AllocWithLocation(
          "w:\\build\\tech5\\shared\\idlib\\containers/Array.h(105) : TAG_TEMP",
          static_cast<unsigned int>(byteCount), TAG_TEMP, false, ALIGN_16,
          HEAP_DEFAULTHEAP));
  std::memset(clear, 0, static_cast<unsigned int>(byteCount));
  for (int level = 0; level < options.numLevels; ++level) {
    atlas->SubImageUpload(level, 0, 0, 0, options.width >> level,
                          options.height >> level, clear, 0);
  }
  if (clear != nullptr) {
    idMem::Singleton()->Free(clear, ALIGN_16);
  }
}

void idImageAtlas::ClearAtlas() {
  if (tileAlloc.buffer != nullptr) {
    std::memset(tileAlloc.buffer, 0, (tileAlloc.bits + 7U) >> 3U);
  }
  const idImageOpts options = atlas->opts;
  const int byteCount = options.width * options.height;
  unsigned char* clear = static_cast<unsigned char*>(
      idMem::Singleton()->AllocWithLocation(
          "w:\\build\\tech5\\shared\\idlib\\containers/Array.h(105) : TAG_TEMP",
          static_cast<unsigned int>(byteCount), TAG_TEMP, false, ALIGN_16,
          HEAP_DEFAULTHEAP));
  std::memset(clear, 0, static_cast<unsigned int>(byteCount));
  for (int level = 0; level < options.numLevels; ++level) {
    atlas->SubImageUpload(level, 0, 0, 0, options.width >> level,
                          options.height >> level, clear, 0);
  }
  if (clear != nullptr) {
    idMem::Singleton()->Free(clear, ALIGN_16);
  }
}

idImageAtlas::~idImageAtlas() { ClearAtlas(); }

template <>
idAtlasResource* idTypedResourceList<idAtlasResource>::Alloc(
    const char* resourceName) const {
  void* storage = idMem::Singleton()->AllocWithLocation(
      "w:\\build\\tech5\\shared\\idlib\\Heap.h(95) : tag",
      sizeof(idAtlasResource), TAG_RESOURCE, false, ALIGN_16,
      HEAP_DEFAULTHEAP);
  idAtlasResource* resource =
      storage != nullptr ? new (storage) idAtlasResource : nullptr;
  resource->SetName(resourceName);
  return resource;
}

void idAtlasResource::LoadResource() {
  if (globalImages == nullptr || globalImages->transSortImageAtlas == nullptr) {
    idLib::FatalError("idAtlasResource::LoadResource atlas not allocated yet");
  }

  idImageAtlas* imageAtlas = globalImages->transSortImageAtlas;
  FreeBlock(imageAtlas->tileAlloc, &img_x, &img_y, img_tiles_wide,
            img_tiles_high);

  idStr imageName;
  imageName.Format("atlasBorder( %s )", name.str);
  char binaryFileName[256];
  idImage::MakeGeneratedName(binaryFileName, sizeof(binaryFileName),
                             imageName.data);
  idBinaryImage binaryImage(imageName.data);
  binaryImage.opts.format = FMT_DXT5;
  binaryTimestamp = ~0U;
  scaleBias = {1.0F, 1.0F, 0.0F, 0.0F};

  bool loaded = binaryImage.LoadFromGeneratedFile(binaryFileName) &&
                binaryImage.opts.format == FMT_DXT5;
  if (!loaded) {
    binaryImage.opts.format = FMT_DXT5;
    if (!binaryImage.LoadFromSourceFile()) {
      SetResourceError(
          "ImageAtlas:: Couldn't load image '%s' from source, defaulting atlas mappings",
          imageName.data);
      img_tiles_wide = 0;
      img_tiles_high = 0;
      return;
    }
    binaryImage.WriteGeneratedFile(binaryFileName);
  }

  if (binaryImage.opts.format != FMT_DXT5) {
    SetResourceError(
        "ImageAtlas:: Image %s has incompatible compression format. Unable to add to ImageAtlas",
        imageName.data);
    img_tiles_wide = 0;
    img_tiles_high = 0;
    return;
  }

  int width = binaryImage.opts.width;
  int height = binaryImage.opts.height;
  img_w = width;
  img_h = height;
  binaryTimestamp = binaryImage.sourceFileTime;
  if (idLib::headless) {
    return;
  }

  const int requiredWidth =
      (imageAtlas->ATLAS_TILE_WIDTH + width - 1) /
      imageAtlas->ATLAS_TILE_WIDTH;
  const int requiredHeight =
      (imageAtlas->ATLAS_TILE_HEIGHT + height - 1) /
      imageAtlas->ATLAS_TILE_HEIGHT;
  if (requiredWidth == img_tiles_wide &&
      requiredHeight == img_tiles_high) {
    if (CheckForSpace(imageAtlas->tileAlloc, img_x, img_y, img_tiles_wide,
                      img_tiles_high)) {
      for (int column = 0; column < requiredWidth; ++column) {
        for (int row = 0; row < requiredHeight; ++row) {
          const int index = img_x + column +
                            (img_y + row) * imageAtlas->ATLAS_TILES_WIDE;
          if (index >= 0 &&
              static_cast<std::size_t>(index) < imageAtlas->tileAlloc.bits) {
            imageAtlas->tileAlloc
                .buffer[static_cast<std::size_t>(index) >> 3U] |=
                static_cast<unsigned char>(1U << (index & 7));
          }
        }
      }
    } else {
      img_tiles_wide = 0;
    }
  }

  if (img_tiles_wide == 0) {
    img_tiles_wide = requiredWidth;
    img_tiles_high = requiredHeight;
    if (!AllocBlock(imageAtlas->tileAlloc, &img_x, &img_y, img_tiles_wide,
                    img_tiles_high)) {
      SetResourceError(
          "ImageAtlas:: Unable to alloc space for image %s in ImageAtlas",
          imageName.data);
      img_tiles_wide = 0;
      img_tiles_high = 0;
      return;
    }
  }

  int leftBorder = 4;
  if (imageAtlas->ATLAS_WIDTH - width < 4) {
    leftBorder = imageAtlas->ATLAS_WIDTH - width;
  }
  int rightBorder = 4;
  if (imageAtlas->ATLAS_WIDTH - width - leftBorder < 4) {
    rightBorder = imageAtlas->ATLAS_WIDTH - width - leftBorder;
  }
  int topBorder = 4;
  if (imageAtlas->ATLAS_HEIGHT - height < 4) {
    topBorder = imageAtlas->ATLAS_HEIGHT - height;
  }
  int bottomBorder = 4;
  if (imageAtlas->ATLAS_HEIGHT - height - topBorder < 4) {
    bottomBorder = imageAtlas->ATLAS_HEIGHT - height - topBorder;
  }
  scaleBias.x = static_cast<float>(width - rightBorder - leftBorder) /
                static_cast<float>(imageAtlas->ATLAS_WIDTH);
  scaleBias.y = static_cast<float>(height - bottomBorder - topBorder) /
                static_cast<float>(imageAtlas->ATLAS_HEIGHT);
  scaleBias.z = static_cast<float>(img_x) /
                    static_cast<float>(imageAtlas->ATLAS_TILES_WIDE) +
                static_cast<float>(leftBorder) /
                    static_cast<float>(imageAtlas->ATLAS_WIDTH);
  scaleBias.w = static_cast<float>(img_y) /
                    static_cast<float>(imageAtlas->ATLAS_TILES_HIGH) +
                static_cast<float>(topBorder) /
                    static_cast<float>(imageAtlas->ATLAS_HEIGHT);

  int uploadX = img_x * imageAtlas->ATLAS_TILE_WIDTH;
  int uploadY = img_y * imageAtlas->ATLAS_TILE_HEIGHT;
  const int levels =
      std::min(imageAtlas->ATLAS_MIP_LEVELS, binaryImage.images.Num());
  for (int level = 0; level < levels; ++level) {
    imageAtlas->atlas->SubImageUpload(
        level, uploadX, uploadY, 0, width, height,
        binaryImage.images[level].data.list, 0);
    uploadX >>= 1;
    uploadY >>= 1;
    width >>= 1;
    height >>= 1;
  }
}
