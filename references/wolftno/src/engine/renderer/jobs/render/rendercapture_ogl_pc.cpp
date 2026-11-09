#include "engine/renderer/jobs/render/rendercapture.h"

#include "engine/renderer/bufferobject.h"
#include "engine/renderer/declrenderprog.h"
#include "engine/renderer/gl_api.h"
#include "engine/renderer/image.h"
#include "engine/renderer/jobs/render/graphicsapiwrapper.h"
#include "engine/renderer/jobs/render/parmstate.h"
#include "engine/renderer/render.h"
#include "engine/renderer/renderdestination.h"

#include <cstddef>
#include <cstdint>
#include <cstring>

void RenderEventRecorderLightStartEvent_Impl(const char* name);
void RenderEventRecorderLightStopEvent_Impl();

void GL_ViewportAndScissor(int x, int y, int width, int height) {
  GL_Viewport(x, y, width, height);
  GL_Scissor(x, y, width, height);
}

namespace {

constexpr unsigned int GL_TEXTURE_2D_VALUE = 0x0DE1;
constexpr unsigned int GL_READ_FRAMEBUFFER_VALUE = 0x8CA8;
constexpr unsigned int GL_DRAW_FRAMEBUFFER_VALUE = 0x8CA9;
constexpr unsigned int GL_PIXEL_PACK_BUFFER_VALUE = 0x88EB;
constexpr unsigned int GL_DEPTH_BUFFER_BIT_VALUE = 0x0100;
constexpr unsigned int GL_COLOR_BUFFER_BIT_VALUE = 0x4000;
constexpr unsigned int GL_NEAREST_VALUE = 0x2600;
constexpr unsigned int GL_LINEAR_VALUE = 0x2601;
constexpr unsigned int GL_RGBA_VALUE = 0x1908;
constexpr unsigned int GL_BGRA_VALUE = 0x80E1;
constexpr unsigned int GL_UNSIGNED_BYTE_VALUE = 0x1401;

const idTextureSampler* ImageAsSampler(const idImage* image) {
  return reinterpret_cast<const idTextureSampler*>(image);
}

void RestoreDefaultCaptureView(const captureParms_t* parameters) {
  GL_SetRenderDestination({parameters->renderDestDefault}, 0, 0);
  GL_ViewportAndScissor(0, 0, parameters->renderWidth,
                        parameters->renderHeight);
  renderThreadParmState->SetFragmentPositionToViewTexture(
      parameters->renderWidth, parameters->renderHeight,
      parameters->windowWidth, parameters->windowHeight);
}

}  // namespace

void CreateColorMips(const captureParms_t* parameters,
                     const idRenderDestination* texture) {
  const idImage* image = texture->targetImage[0];
  if (parameters->settings->generateMipMaps >= 1) {
    idDeclRenderProg::BindTexture(0, image, nullptr);
    _glewGenerateMipmapEXT(GL_TEXTURE_2D_VALUE);
    return;
  }

  renderThreadParmState->SetParmSampler(parameters->rpViewColorMap,
                                        ImageAsSampler(image));
  GL_SetRenderDestination({parameters->renderDestMip}, 0, 0);
  int width = image->opts.width >> 1;
  int height = image->opts.height >> 1;
  renderThreadParmState->SetSrcTexClamp(
      parameters->renderWidth, parameters->renderHeight,
      parameters->windowWidth, parameters->windowHeight);
  for (int level = 1;
       width >= 1 && height >= 1 && level < image->opts.numLevels;
       ++level) {
    GL_ViewportAndScissor(0, 0, width, height);
    renderThreadParmState->SetFragmentPositionToViewTexture(
        width, height, width, height);
    renderThreadParmState->SetParmSampler(
        parameters->rpMipMapSampler,
        parameters->mipMapSamplers[level - 1]);
    GL_DrawElements({parameters->progOverlappedDownSample},
                    {parameters->unitSquareTris}, 0, false);
    idDeclRenderProg::BindTexture(0, image, nullptr);
    glCopyTexSubImage2D(GL_TEXTURE_2D_VALUE, level, 0, 0, 0, 0,
                        width, height);
    width >>= 1;
    height >>= 1;
  }
  renderThreadParmState->ResetSrcTexClamp();
  RestoreDefaultCaptureView(parameters);
}

void CaptureViewDepth(const captureParms_t* parameters) {
  GL_State(UINT64_C(0x4000), false);
  GL_Scissor(0, 0, parameters->renderWidth, parameters->renderHeight);
  GL_SetRenderDestination({parameters->renderDestViewDepth}, 0, 0);
  _glewBindFramebufferEXT(GL_DRAW_FRAMEBUFFER_VALUE,
                          parameters->renderDestViewDepth->FBOhandle);
  _glewBindFramebufferEXT(GL_READ_FRAMEBUFFER_VALUE,
                          parameters->renderDestDefault->FBOhandle);
  _glewBlitFramebufferEXT(
      0, 0, parameters->renderWidth, parameters->renderHeight,
      0, 0, parameters->renderWidth, parameters->renderHeight,
      GL_DEPTH_BUFFER_BIT_VALUE, GL_NEAREST_VALUE);
  GL_SetRenderDestination({parameters->renderDestDefault}, 0, 0);
}

void CaptureViewColor(const captureParms_t* parameters) {
  GL_State(UINT64_C(0x4000), false);
  GL_Scissor(0, 0, parameters->renderWidth, parameters->renderHeight);
  GL_SetRenderDestination({parameters->renderDestViewColor}, 0, 0);
  _glewBindFramebufferEXT(GL_DRAW_FRAMEBUFFER_VALUE,
                          parameters->renderDestViewColor->FBOhandle);
  _glewBindFramebufferEXT(GL_READ_FRAMEBUFFER_VALUE,
                          parameters->renderDestDefault->FBOhandle);
  _glewBlitFramebufferEXT(
      0, 0, parameters->renderWidth, parameters->renderHeight,
      0, 0, parameters->renderWidth, parameters->renderHeight,
      GL_COLOR_BUFFER_BIT_VALUE, GL_LINEAR_VALUE);
  GL_SetRenderDestination({parameters->renderDestDefault}, 0, 0);
  if (parameters->createMipMaps)
    CreateColorMips(parameters, parameters->renderDestViewColor);
}

void CaptureFeedback(const captureParms_t* parameters) {
  if (parameters->settings->skipFeedback)
    return;

  const int feedbackHeight = 120 / parameters->settings->numViews;
  const int feedbackOffset =
      640 * feedbackHeight * parameters->viewIndex;
  const idImage* feedbackImage =
      parameters->renderDestDefault->targetImage[1];
  GL_SetRenderDestination({parameters->renderDestFeedback}, 0, 0);
  GL_ViewportAndScissor(0, 0, 160, feedbackHeight);

  const idDeclRenderParm* feedbackParm = parameters->rpFeedbackColor;
  if (feedbackParm != nullptr) {
    renderThreadParmState->SetFragmentPositionToViewTexture(
        parameters->renderWidth, parameters->renderHeight,
        parameters->windowWidth, parameters->windowHeight);
  } else {
    renderThreadParmState->SetFragmentPositionToViewTexture(
        160, feedbackHeight, 160, feedbackHeight);
    feedbackParm = parameters->rpViewColorMap;
  }
  renderThreadParmState->SetParmSampler(feedbackParm,
                                        ImageAsSampler(feedbackImage));
  GL_DrawElements({parameters->progFeedbackDownSample},
                  {parameters->unitSquareTris}, 0, false);

  void* pixels = nullptr;
  if (parameters->forceFullVirtualTextureLoad ||
      parameters->feedbackBufferObject == nullptr) {
    RenderEventRecorderLightStartEvent_Impl(
        "MRB_CAPTURE_VIEW_FEEDBACK: read feedback pixels blocking");
    pixels = parameters->feedbackBuffer->buffer + feedbackOffset;
  } else {
    RenderEventRecorderLightStartEvent_Impl(
        "MRB_CAPTURE_VIEW_FEEDBACK: MapBuffer");
    const void* mapped =
        parameters->feedbackBufferObject->MapBuffer(BM_READ);
    RenderEventRecorderLightStopEvent_Impl();
    RenderEventRecorderLightStartEvent_Impl(
        "MRB_CAPTURE_VIEW_FEEDBACK: memcpy pixels");
    std::memmove(parameters->feedbackBuffer->buffer + feedbackOffset,
                 mapped, static_cast<std::size_t>(640 * feedbackHeight));
    RenderEventRecorderLightStopEvent_Impl();
    parameters->feedbackBufferObject->UnmapBuffer();
    _glewBindBufferARB(
        GL_PIXEL_PACK_BUFFER_VALUE,
        static_cast<unsigned int>(reinterpret_cast<std::uintptr_t>(
            parameters->feedbackBufferObject->apiObject)));
    RenderEventRecorderLightStartEvent_Impl(
        "MRB_CAPTURE_VIEW_FEEDBACK: glReadPixels");
  }
  glReadPixels(0, 0, 160, feedbackHeight,
               parameters->settings->feedbackBGRA ? GL_BGRA_VALUE
                                                   : GL_RGBA_VALUE,
               GL_UNSIGNED_BYTE_VALUE, pixels);
  RenderEventRecorderLightStopEvent_Impl();
  RestoreDefaultCaptureView(parameters);
}

void CaptureGlareMap(const captureParms_t* parameters) {
  CaptureViewColor(parameters);
  const int width = parameters->renderDestMip3->targetWidth;
  const int height = parameters->renderDestMip3->targetHeight;
  renderThreadParmState->SetFragmentPositionToViewTexture(
      width, height, width, height);
  GL_SetRenderDestination({parameters->renderDestMip3}, 0, 0);
  GL_ViewportAndScissor(0, 0, width, height);
  renderThreadParmState->SetParmSampler(
      parameters->rpGlareMap,
      ImageAsSampler(parameters->renderDestViewColor->targetImage[0]));
  GL_DrawElements({parameters->progGlareScale},
                  {parameters->unitSquareTris}, 0, false);
  // The retail call at this point resolves to the linker-folded no-op RVA.
  RestoreDefaultCaptureView(parameters);
}

void CaptureGuiImage(const captureParms_t* parameters) {
  CreateColorMips(parameters, parameters->renderDestGui);
}
