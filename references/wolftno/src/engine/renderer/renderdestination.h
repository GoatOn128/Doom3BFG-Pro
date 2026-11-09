#pragma once

#include <cstddef>

class idImage;

enum renderClass_t : int {
  TC_DEPTH = 0,
  TC_RGBA = 1,
  TC_RGBA_DEPTH = 2,
  TC_FLOAT_RGBA = 3,
  TC_FLOAT_RGBA_DEPTH = 4,
  TC_FLOAT16_RGBA = 5,
  TC_FLOAT16_RGBA_DEPTH = 6,
  TC_NUM_RENDER_CLASS = 7,
};

enum resolveTarget_t : int {
  RESOLVE_TARGET_COLOR0 = 0,
  RESOLVE_TARGET_COLOR1 = 1,
  RESOLVE_TARGET_COLOR2 = 2,
  RESOLVE_TARGET_COLOR3 = 3,
  RESOLVE_TARGET_DEPTH = 4,
};

class idRenderDestination {
public:
  idRenderDestination();
  idRenderDestination(const char* name, int width, int height,
                      renderClass_t textureClass)
      : idRenderDestination() {
    Create(name, width, height, textureClass);
  }
  ~idRenderDestination();

  void SetDefault() { isDefault = true; }
  bool IsDefault() const { return isDefault; }
  bool IsValid() const { return isDefault || FBOhandle != 0; }
  int GetTargetWidth() const { return targetWidth; }
  int GetTargetHeight() const { return targetHeight; }
  idImage* GetColorImage() const { return targetImage[0]; }
  idImage* GetTargetImage(int index) const { return targetImage[index]; }
  idImage* GetDepthImage() const { return depthImage; }

  void Create(const char* name, int width, int height,
              renderClass_t textureClass);
  void CreateFromImages(idImage* color, idImage* depth, idImage* stencil);
  void AddTarget(idImage* target);
  void Resize(int width, int height);

  bool isDefault{};
  int targetWidth{};
  int targetHeight{};
  idImage* targetImage[4]{};
  idImage* depthImage{};
  idImage* stencilImage{};
  unsigned int FBOhandle{};
  int boundSide{};
  int boundLevel{};
};

extern idRenderDestination* renderDestScaledLinear;
extern idRenderDestination* renderDestScaledColorWithFeedback;
extern idRenderDestination* renderDestScaledFeedback;
extern idRenderDestination* renderDestScaledDepth;
extern idRenderDestination* renderDestScaledColor2;
extern idRenderDestination* renderDestScaledColor;
extern idRenderDestination* renderDestNative;

static_assert(offsetof(idRenderDestination, targetWidth) == 0x04,
              "render-destination width offset drift");
static_assert(offsetof(idRenderDestination, targetImage) == 0x10,
              "render-destination image table offset drift");
static_assert(offsetof(idRenderDestination, depthImage) == 0x30,
              "render-destination depth-image offset drift");
static_assert(offsetof(idRenderDestination, stencilImage) == 0x38,
              "render-destination stencil-image offset drift");
static_assert(offsetof(idRenderDestination, FBOhandle) == 0x40,
              "render-destination framebuffer offset drift");
static_assert(offsetof(idRenderDestination, boundSide) == 0x44,
              "render-destination bound-side offset drift");
static_assert(offsetof(idRenderDestination, boundLevel) == 0x48,
              "render-destination bound-level offset drift");
static_assert(sizeof(idRenderDestination) == 0x50,
              "render-destination ABI drift");
