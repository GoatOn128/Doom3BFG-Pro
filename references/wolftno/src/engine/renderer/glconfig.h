#pragma once

#include "shared/idlib/text/str.h"

#include <cstddef>
#include <cstdint>

enum graphicsVendor_t : int {
  VENDOR_NVIDIA = 0,
  VENDOR_AMD = 1,
  VENDOR_INTEL = 2,
};

struct glConfig_t {
  const char* renderer_string;
  const char* vendor_string;
  const char* version_string;
  idStr extensions_string;
  const char* wgl_extensions_string;
  float glVersion;
  graphicsVendor_t vendor;
  bool isInitialized;
  unsigned char alignmentPadding[3];
  int maxTextureSize;
  int maxTextureUnits;
  int maxTextureImageUnits;
  int maxTextureAnisotropy;
  int maxSamples;
  int maxColorTextureSamples;
  int maxDepthTextureSamples;
  int uniformBufferOffsetAlignment;
  bool textureMultisampleAvailable;
  bool textureCompressionAvailable;
  bool textureNonPowerOfTwoAvailable;
  bool anisotropicAvailable;
  bool directStateAccess;
  bool samplerObjectsAvailable;
  bool vertexBufferObjectAvailable;
  bool mapBufferRangeAvailable;
  bool drawElementsBaseVertexAvailable;
  bool vertexArrayObjectAvailable;
  bool vertexProgramAvailable;
  bool getProgramBinaryAvailable;
  bool uniformBufferAvailable;
  bool occlusionQueryAvailable;
  bool timerQueryAvailable;
  bool framebufferObjectAvailable;
  bool framebufferBlitAvailable;
  bool pixelBufferObjectAvailable;
  bool depthBoundsTestAvailable;
  bool debugOutputAvailable;
  bool syncAvailable;
  bool swapControlAvailable;
  bool swapControlTearAvailable;
  unsigned char alignmentPadding2[5];
  std::int64_t freeVRamAtInit;
};

extern glConfig_t glConfig;

static_assert(offsetof(glConfig_t, extensions_string) == 0x18,
              "GL config extension-string offset drift");
static_assert(offsetof(glConfig_t, syncAvailable) == 0x88,
              "GL config sync flag offset drift");
static_assert(offsetof(glConfig_t, freeVRamAtInit) == 0x90,
              "GL config VRAM offset drift");
static_assert(sizeof(glConfig_t) == 0x98, "GL config ABI drift");
