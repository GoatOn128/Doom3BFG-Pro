#include "engine/renderer/renderdestination.h"

#include "engine/renderer/gl_api.h"
#include "engine/renderer/image.h"
#include "engine/renderer/imagemanager.h"
#include "shared/idlib/lib.h"
#include "shared/idlib/text/str.h"

#include <cstddef>
#include <intrin.h>

namespace {

constexpr unsigned int GL_NONE_VALUE = 0;
constexpr unsigned int GL_TEXTURE_2D_VALUE = 0x0DE1;
constexpr unsigned int GL_TEXTURE_CUBE_MAP_POSITIVE_X_VALUE = 0x8515;
constexpr unsigned int GL_FRAMEBUFFER_VALUE = 0x8D40;
constexpr unsigned int GL_FRAMEBUFFER_COMPLETE_VALUE = 0x8CD5;
constexpr unsigned int GL_COLOR_ATTACHMENT0_VALUE = 0x8CE0;
constexpr unsigned int GL_DEPTH_ATTACHMENT_VALUE = 0x8D00;
constexpr unsigned int GL_STENCIL_ATTACHMENT_VALUE = 0x8D20;
constexpr unsigned int GL_TEXTURE_2D_MULTISAMPLE_VALUE = 0x9100;

constexpr unsigned int gl_renderTarget_0[4] = {
    GL_COLOR_ATTACHMENT0_VALUE,
    GL_COLOR_ATTACHMENT0_VALUE + 1,
    GL_COLOR_ATTACHMENT0_VALUE + 2,
    GL_COLOR_ATTACHMENT0_VALUE + 3,
};

}  // namespace

static unsigned int CheckFrameBuffer() {
  unsigned int status =
      _glewCheckFramebufferStatusEXT(GL_FRAMEBUFFER_VALUE);
  if (status != GL_FRAMEBUFFER_COMPLETE_VALUE && status != 0)
    status -= GL_FRAMEBUFFER_COMPLETE_VALUE + 1;
  return status;
}

idRenderDestination::idRenderDestination()
    : isDefault(false),
      targetWidth(-1),
      targetHeight(-1),
      targetImage{nullptr, nullptr, nullptr, nullptr},
      depthImage(nullptr),
      stencilImage(nullptr),
      FBOhandle(0),
      boundSide(-1),
      boundLevel(-1) {}

idRenderDestination::~idRenderDestination() {
  if (FBOhandle != 0) {
    _glewDeleteFramebuffersEXT(1, &FBOhandle);
  }
}

void idRenderDestination::Resize(int width, int height) {
  targetWidth = width;
  targetHeight = height;
  for (idImage* image : targetImage) {
    if (image != nullptr) {
      image->Resize(width, height, 0);
    }
  }
  if (depthImage != nullptr) {
    depthImage->Resize(width, height, 0);
  }
  if (stencilImage != nullptr) {
    stencilImage->Resize(width, height, 0);
  }
  boundSide = -1;
  boundLevel = -1;
}

void idRenderDestination::CreateFromImages(idImage* color, idImage* depth,
                                            idImage* stencil) {
  GL_CheckErrors(false);
  if (color == nullptr && depth == nullptr && stencil == nullptr) {
    idLib::Error("idRenderDestination::CreateFromImages: no images specified");
    __debugbreak();
  }

  // Retail selects dimensions from color first, otherwise depth. A
  // stencil-only destination is accepted by the guard but still dereferences
  // depth; preserve that behavior.
  idImage* dimensions = color != nullptr ? color : depth;
  targetWidth = dimensions->opts.width;
  targetHeight = dimensions->opts.height;
  targetImage[0] = color;
  targetImage[1] = nullptr;
  targetImage[2] = nullptr;
  targetImage[3] = nullptr;
  depthImage = depth;
  stencilImage = stencil;
  boundSide = 0;
  boundLevel = 0;

  if (idLib::headless) {
    return;
  }

  _glewGenFramebuffersEXT(1, &FBOhandle);
  _glewBindFramebufferEXT(GL_FRAMEBUFFER_VALUE, FBOhandle);

  if (color != nullptr) {
    const unsigned int colorTarget =
        color->opts.textureType == TT_CUBIC
            ? GL_TEXTURE_CUBE_MAP_POSITIVE_X_VALUE
            : (color->opts.numSamples > 1
                   ? GL_TEXTURE_2D_MULTISAMPLE_VALUE
                   : GL_TEXTURE_2D_VALUE);
    _glewFramebufferTexture2DEXT(
        GL_FRAMEBUFFER_VALUE, GL_COLOR_ATTACHMENT0_VALUE, colorTarget,
        color->oglTexture, 0);
  } else {
    _glewFramebufferTexture2DEXT(
        GL_FRAMEBUFFER_VALUE, GL_COLOR_ATTACHMENT0_VALUE,
        GL_TEXTURE_2D_VALUE, 0, 0);
    glReadBuffer(GL_NONE_VALUE);
    glDrawBuffer(GL_NONE_VALUE);
  }

  _glewFramebufferTexture2DEXT(
      GL_FRAMEBUFFER_VALUE, GL_DEPTH_ATTACHMENT_VALUE,
      depth != nullptr && depth->opts.numSamples > 1
          ? GL_TEXTURE_2D_MULTISAMPLE_VALUE
          : GL_TEXTURE_2D_VALUE,
      depth != nullptr ? depth->oglTexture : 0, 0);
  _glewFramebufferTexture2DEXT(
      GL_FRAMEBUFFER_VALUE, GL_STENCIL_ATTACHMENT_VALUE,
      stencil != nullptr && stencil->opts.numSamples > 1
          ? GL_TEXTURE_2D_MULTISAMPLE_VALUE
          : GL_TEXTURE_2D_VALUE,
      stencil != nullptr ? stencil->oglTexture : 0, 0);
  CheckFrameBuffer();
  _glewBindFramebufferEXT(GL_FRAMEBUFFER_VALUE, 0);
}

void idRenderDestination::AddTarget(idImage* target) {
  int targetIndex = 0;
  while (targetIndex < 4 && targetImage[targetIndex] != nullptr) {
    ++targetIndex;
  }
  if (targetIndex >= 4) {
    return;
  }

  targetImage[targetIndex] = target;
  _glewBindFramebufferEXT(GL_FRAMEBUFFER_VALUE, FBOhandle);
  _glewFramebufferTexture2DEXT(
      GL_FRAMEBUFFER_VALUE, gl_renderTarget_0[targetIndex],
      target->opts.numSamples > 1 ? GL_TEXTURE_2D_MULTISAMPLE_VALUE
                                  : GL_TEXTURE_2D_VALUE,
      target->oglTexture, 0);
  _glewDrawBuffers(targetIndex + 1, gl_renderTarget_0);
  CheckFrameBuffer();
  _glewBindFramebufferEXT(GL_FRAMEBUFFER_VALUE, 0);
}

void idRenderDestination::Create(const char* name, int width, int height,
                                 renderClass_t textureClass) {
  idImageOpts options{};
  options.width = width;
  options.height = height;
  options.packedTail = true;
  options.colorFormat = CFM_DEFAULT;
  options.textureOptions = TO_NONE;
  options.sampler.lodMaxClamp = 13.0f;
  options.sampler.filter = TF_NEAREST;
  options.sampler.repeat = TR_CLAMP;

  idImage* color = nullptr;
  idImage* depth = nullptr;
  if (textureClass == TC_DEPTH || textureClass == TC_RGBA_DEPTH ||
      textureClass == TC_FLOAT_RGBA_DEPTH ||
      textureClass == TC_FLOAT16_RGBA_DEPTH) {
    options.format = FMT_DEPTH;
    va depthName("%s_d", name);
    depth = globalImages->ScratchImage(depthName.buffer, options);
    GL_CheckErrors(false);
  }

  switch (textureClass) {
    case TC_RGBA:
    case TC_RGBA_DEPTH:
      options.format = FMT_RGBA8;
      options.sampler.filter = TF_LINEAR;
      options.sampler.repeat = TR_CLAMP_TO_BORDER;
      color = globalImages->ScratchImage(name, options);
      break;
    case TC_FLOAT_RGBA:
    case TC_FLOAT_RGBA_DEPTH:
      options.format = FMT_RGBA32F;
      options.sampler.filter = TF_NEAREST;
      options.sampler.repeat = TR_CLAMP;
      color = globalImages->ScratchImage(name, options);
      break;
    case TC_FLOAT16_RGBA:
    case TC_FLOAT16_RGBA_DEPTH:
      options.format = FMT_RGBA16F;
      options.sampler.filter = TF_NEAREST;
      options.sampler.repeat = TR_CLAMP;
      color = globalImages->ScratchImage(name, options);
      break;
    default:
      break;
  }

  GL_CheckErrors(false);
  CreateFromImages(color, depth, nullptr);
}
