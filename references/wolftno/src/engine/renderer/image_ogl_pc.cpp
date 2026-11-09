#include "engine/renderer/image.h"

#include "engine/renderer/declrenderprog.h"
#include "engine/renderer/gl_api.h"
#include "engine/renderer/glconfig.h"
#include "engine/renderer/texturesampler.h"
#include "shared/idlib/lib.h"
#include "shared/idlib/sys/sys_alloc.h"
#include "shared/idlib/sys/sys_time.h"

#define WIN32_LEAN_AND_MEAN
#define NOMINMAX
#include <Windows.h>

#include <algorithm>
#include <cstddef>
#include <cstdint>
#include <cstring>

std::uintptr_t glImageHeap = 0;

void idImage::UnmapTexture() {
  if (_glewUnmapTexture2DINTEL == nullptr) {
    return;
  }
  for (int level = 0; level < opts.numLevels; ++level) {
    if (buffers[level] != nullptr) {
      _glewUnmapTexture2DINTEL(oglTexture, level);
      buffers[level] = nullptr;
      pitches[level] = 0;
      tiling[level] = INTEL_LINEAR;
    }
  }
}

bool idImage::IsMapped() const {
  return buffers[0] != nullptr;
}

void idImage::SubImageUpload(int mipLevel, int x, int y, int z, int width,
                             int height, const void* pic,
                             int pixelPitch) const {
  if (idLib::headless) {
    return;
  }

  int imageSize = 0;
  int uploadWidth = width;
  int uploadHeight = height;
  if (IsCompressed()) {
    imageSize = ((height + 3) & ~3) * ((width + 3) & ~3) *
                BitsForInternalFormat() / 8;
    if (x + width > opts.width) {
      uploadWidth = opts.width - x;
    }
    if (y + height > opts.height) {
      uploadHeight = opts.height - x;
    }
  }

  if (pixelPitch != 0) {
    glPixelStorei(0x0CF2U, pixelPitch);
  }
  if (glConfig.directStateAccess) {
    if (uploadTarget == 0x806FU) {
      if (IsCompressed()) {
        _glewCompressedTextureSubImage3DEXT(
            oglTexture, uploadTarget, mipLevel, x, y, z, uploadWidth,
            uploadHeight, 1, internalFormat, imageSize, pic);
      } else {
        _glewTextureSubImage3DEXT(
            oglTexture, uploadTarget, mipLevel, x, y, z, uploadWidth,
            uploadHeight, 1, dataFormat, dataType, pic);
      }
    } else if (IsCompressed()) {
      _glewCompressedTextureSubImage2DEXT(
          oglTexture, uploadTarget + z, mipLevel, x, y, uploadWidth,
          uploadHeight, internalFormat, imageSize, pic);
    } else {
      _glewTextureSubImage2DEXT(oglTexture, uploadTarget + z, mipLevel, x, y,
                                uploadWidth, uploadHeight, dataFormat,
                                dataType, pic);
    }
  } else {
    idDeclRenderProg::BindTexture(0, this, nullptr);
    if (uploadTarget == 0x806FU) {
      if (IsCompressed()) {
        _glewCompressedTexSubImage3DARB(
            uploadTarget, mipLevel, x, y, z, uploadWidth, uploadHeight, 1,
            internalFormat, imageSize, pic);
      } else {
        _glewTexSubImage3D(uploadTarget, mipLevel, x, y, z, uploadWidth,
                           uploadHeight, 1, dataFormat, dataType, pic);
      }
    } else if (IsCompressed()) {
      _glewCompressedTexSubImage2DARB(
          uploadTarget + z, mipLevel, x, y, uploadWidth, uploadHeight,
          internalFormat, imageSize, pic);
    } else {
      glTexSubImage2D(uploadTarget + z, mipLevel, x, y, uploadWidth,
                      uploadHeight, dataFormat, dataType, pic);
    }
  }
  if (pixelPitch != 0) {
    glPixelStorei(0x0CF2U, 0);
  }
}

void idImage::SetPixel(int mipLevel, int x, int y, const void* data) {
  SubImageUpload(mipLevel, x, y, 0, 1, 1, data, 0);
}

bool idImage::MapTexture() {
  if (_glewMapTexture2DINTEL == nullptr) {
    return false;
  }
  if (opts.numLevels <= 0) {
    return true;
  }
  for (int level = 0; level < opts.numLevels; ++level) {
    buffers[level] = _glewMapTexture2DINTEL(
        oglTexture, level, 2U, &pitches[level],
        reinterpret_cast<unsigned int*>(&tiling[level]));
    if (buffers[level] == nullptr || tiling[level] != INTEL_LINEAR) {
      UnmapTexture();
      return false;
    }
  }
  return true;
}

void idImage::PurgeImage() {
  SetResourceError(nullptr);
  UnmapTexture();
  while (backgroundReadData != nullptr && !backgroundReadComplete) {
    idLib::Printf("Waiting on background complete for purge of %s\n",
                  name.str);
    Sys_Sleep(10U);
  }
  if (backgroundReadData != nullptr) {
    if (backgroundReadData != imageBuffer) {
      idMem::Singleton()->Free(backgroundReadData, ALIGN_16);
    }
    backgroundReadData = nullptr;
  }
  textureSamplerManager->vtable->DeleteSampler(textureSamplerManager,
                                                textureSampler);
  if (oglTexture != ~0U) {
    glDeleteTextures(1, &oglTexture);
    oglTexture = ~0U;
  }
}

void idImage::AllocImage(const idImageOpts& imageOptions) {
  if (!idLib::headless) {
    GL_CheckErrors(false);
    PurgeImage();
  }
  allocationFailed = false;
  std::memmove(&opts, &imageOptions, sizeof(opts));
  if (imageOptions.textureType == TT_CUBIC &&
      imageOptions.sampler.repeat != TR_CLAMP) {
    opts.sampler.repeat = TR_CLAMP;
  }
  DeriveNumLevels();

  switch (opts.format) {
    case FMT_RGBA32F:
      internalFormat = 0x8814;
      dataFormat = 0x1908;
      dataType = 0x1406;
      break;
    case FMT_RGBA16F:
      internalFormat = 0x881A;
      dataFormat = 0x1908;
      dataType = 0x140B;
      break;
    case FMT_RGBA8:
      internalFormat = 0x8058;
      dataFormat = 0x1908;
      dataType = 0x1401;
      break;
    case FMT_ARGB8:
      internalFormat = 0x80E1;
      dataFormat = 0x1908;
      dataType = 0x1401;
      break;
    case FMT_ALPHA:
      internalFormat = 0x803C;
      dataFormat = 0x1906;
      dataType = 0x1401;
      break;
    case FMT_L8A8:
      internalFormat = 0x8045;
      dataFormat = 0x190A;
      dataType = 0x1401;
      break;
    case FMT_RG8:
      internalFormat = 0x822B;
      dataFormat = 0x8227;
      dataType = 0x1401;
      break;
    case FMT_LUM8:
      internalFormat = 0x8040;
      dataFormat = 0x1909;
      dataType = 0x1401;
      break;
    case FMT_INT8:
      internalFormat = 0x804B;
      dataFormat = 0x1909;
      dataType = 0x1401;
      break;
    case FMT_DXT1:
      internalFormat = 0x83F1;
      dataFormat = 0x1908;
      dataType = 0x1401;
      break;
    case FMT_DXT5:
      internalFormat = 0x83F3;
      dataFormat = 0x1908;
      dataType = 0x1401;
      break;
    case FMT_DEPTH:
      internalFormat = 0x1902;
      dataFormat = 0x1902;
      dataType = 0x1401;
      break;
    case FMT_DEPTH_STENCIL:
      internalFormat = 0x88F0;
      dataFormat = 0x84F9;
      dataType = 0x84FA;
      break;
    case FMT_X32F:
      internalFormat = 0x822E;
      dataFormat = 0x1903;
      dataType = 0x1406;
      break;
    case FMT_Y16F_X16F:
      internalFormat = 0x822F;
      dataFormat = 0x8227;
      dataType = 0x140B;
      break;
    case FMT_X16:
      internalFormat = 0x822A;
      dataFormat = 0x1903;
      dataType = 0x1403;
      break;
    case FMT_Y16_X16:
      internalFormat = 0x822C;
      dataFormat = 0x8227;
      dataType = 0x1403;
      break;
    case FMT_RGB565:
      internalFormat = 0x1907;
      dataFormat = 0x1907;
      dataType = 0x8363;
      break;
    default:
      idLib::Error("Unhandled image format %d in %s\n", opts.format,
                   name.str);
  }

  int sides = 1;
  target = 0x0DE1;
  bindTarget = 0x0DE1;
  uploadTarget = 0x0DE1;
  if (opts.textureType == TT_2D) {
    bindTarget = opts.numSamples > 1 ? 0x9100U : 0x0DE1U;
    uploadTarget = bindTarget;
  } else if (opts.textureType == TT_3D) {
    target = 0x806F;
    bindTarget = 0x806F;
    uploadTarget = 0x806F;
  } else if (opts.textureType == TT_CUBIC) {
    target = 0x8513;
    bindTarget = 0x8513;
    uploadTarget = 0x8515;
    sides = 6;
  } else {
    idLib::Error("Invalid texture type %d in %s", opts.textureType,
                 name.str);
  }

  if (idLib::headless || !glConfig.isInitialized) {
    return;
  }

  glGenTextures(1, &oglTexture);
  if (glImageHeap == 0) {
    glImageHeap = reinterpret_cast<std::uintptr_t>(&glImageHeap);
  }
  _glewBindMultiTextureEXT(0x84C0U, bindTarget, oglTexture);
  if (opts.linear && _glewMapTexture2DINTEL != nullptr) {
    glTexParameteri(target, 0x83FFU, 1);
  }

  for (int side = 0; side < sides; ++side) {
    if (opts.partiallyResident) {
      _glewTexStorageSparseAMD(uploadTarget + side, internalFormat,
                               opts.width, opts.height, 1, 0, 1U);
      continue;
    }

    int width = opts.width;
    int height = opts.textureType == TT_CUBIC ? opts.width : opts.height;
    int depth = opts.depth;
    for (int level = 0; level < opts.numLevels; ++level) {
      GL_GetError();
      if (opts.textureType == TT_3D) {
        if (IsCompressed()) {
          const int imageSize =
              2 * BitsForInternalFormat() * depth * ((height + 3) / 4) *
              ((width + 3) / 4);
          HANDLE heap = GetProcessHeap();
          void* temporary = HeapAlloc(heap, 0, imageSize);
          _glewCompressedTexImage3DARB(
              uploadTarget, level, internalFormat, width, height, depth, 0,
              imageSize, temporary);
          if (temporary != nullptr) {
            HeapFree(GetProcessHeap(), 0, temporary);
          }
        } else {
          _glewTexImage3D(uploadTarget, level, internalFormat, width, height,
                          depth, 0, dataFormat, dataType, nullptr);
        }
      } else if (IsCompressed()) {
        const int imageSize =
            2 * BitsForInternalFormat() * ((height + 3) / 4) *
            ((width + 3) / 4);
        HANDLE heap = GetProcessHeap();
        void* temporary = HeapAlloc(heap, 0, imageSize);
        _glewCompressedTexImage2DARB(uploadTarget + side, level,
                                     internalFormat, width, height, 0,
                                     imageSize, temporary);
        if (temporary != nullptr) {
          HeapFree(GetProcessHeap(), 0, temporary);
        }
      } else if (opts.numSamples > 1) {
        _glewTexImage2DMultisample(uploadTarget + side, opts.numSamples,
                                   internalFormat, width, height, 0);
      } else {
        glTexImage2D(uploadTarget + side, level, internalFormat, width, height,
                     0, dataFormat, dataType, nullptr);
      }

      if (GL_GetError() == 0x0505U) {
        idLib::Warning("texture allocation failed for %s", name.str);
        allocationFailed = true;
      }
      width = std::max(width >> 1, 1);
      height = std::max(height >> 1, 1);
      depth = std::max(depth >> 1, 1);
    }
  }

  glTexParameteri(target, 0x813DU, opts.numLevels - 1);
  if (opts.linear && _glewMapTexture2DINTEL != nullptr) {
    glTexParameteri(target, 0x83FFU, 0);
  }
  GL_CheckErrors(false);
  textureSampler = textureSamplerManager->vtable->CreateSampler(
      textureSamplerManager, &opts.sampler, nullptr);
  GL_CheckErrors(false);
}

bool idImage::Resize(int width, int height, int depth) {
  if (opts.width == width && opts.height == height && opts.depth == depth) {
    return false;
  }
  PurgeImage();
  opts.numLevels = 0;
  opts.width = width;
  opts.height = height;
  opts.depth = depth;
  AllocImage(opts);
  return true;
}
