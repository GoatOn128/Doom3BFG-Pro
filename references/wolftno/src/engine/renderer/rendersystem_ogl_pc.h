#pragma once

#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <Windows.h>

#include "shared/idlib/containers/list.h"

#include <cstdint>

class idAutoComplete;
class idCVar;

struct vidMode_t {
  int monitorNum;
  int x;
  int y;
  int width;
  int height;
  float pixelAspect;
  bool fullscreen;
};

struct monitor_t {
  HMONITOR hMonitor;
  int width;
  int height;
  idList<unsigned int, 62> supportedModes;
};

struct idThreadGraphicsContext {
  HWND contextWindow;
  HGLRC glContext;
};

template <> monitor_t* idList<monitor_t, 62>::Alloc();

struct sys3D_t {
  sys3D_t();

  std::uint16_t oldHardwareGamma[3][256];
  HDC win32DC;
  bool currentFullscreen;
  int currentMonitor;
  int numSamples;
  bool useFloatBuffers;
  vidMode_t vidMode;
  idList<monitor_t, 62> monitors;
  HDC lastDC;
  HGLRC mainThreadContext;
  int pixelFormat;
  PIXELFORMATDESCRIPTOR pfd;
  idThreadGraphicsContext* relayContext;
  idThreadGraphicsContext* renderThreadContext;
  bool ignoreSizeMessages;
  int curentSwapInterval;
};

using glTexMultiSubImage2DAMDProc_t = void (*)(
    unsigned int target, int level, int count, const int* xOffsets,
    const int* yOffsets, const int* widths, const int* heights,
    unsigned int format, unsigned int type, const void* const* pixels);
using glCompressedTexMultiSubImage2DAMDProc_t = void (*)(
    unsigned int target, int level, int count, const int* xOffsets,
    const int* yOffsets, const int* widths, const int* heights,
    unsigned int format, const int* imageSizes, const void* const* data);

extern sys3D_t sys3D;
extern glTexMultiSubImage2DAMDProc_t glTexMultiSubImage2DAMD;
extern glCompressedTexMultiSubImage2DAMDProc_t
    glCompressedTexMultiSubImage2DAMD;
extern idCVar r_debugContext;
extern idCVar r_ignoreGLErrors;
extern idCVar r_glMajorVersion;
extern idCVar r_glMinorVersion;
extern idCVar r_glSingleContext;
extern idCVar r_useGPUTimer;
extern idCVar r_glUseDirectStateAccess;
extern idCVar r_multiSamples;
extern idCVar r_multiSamplesGuiClamp;
extern idCVar r_mode;
extern idCVar r_initialMonitor;
extern idCVar r_displayRefresh;
extern idCVar r_fullscreen;
extern idCVar r_fullscreenWindowed;
extern idCVar r_stealFocus;
extern idCVar r_windowWidth;
extern idCVar r_windowHeight;
extern idCVar r_windowPosX;
extern idCVar r_windowPosY;

template <typename T>
T* idListArrayNew(int count, unsigned int tag, bool zeroBuffer);
template <typename T> void idListArrayDelete(T* pointer, int count);
template <typename T>
T* idListArrayResize(T* oldPointer, int oldCount, int newCount,
                     unsigned int tag, bool zeroBuffer);

void qglBindMultiTextureEXT(unsigned int textureUnit, unsigned int target,
                            unsigned int texture);
void qglTexMultiSubImage2DAMD(unsigned int target, int level, int count,
                              const int* xOffsets, const int* yOffsets,
                              const int* widths, const int* heights,
                              unsigned int format, unsigned int type,
                              const void* const* pixels);
void qglCompressedTexMultiSubImage2DAMD(
    unsigned int target, int level, int count, const int* xOffsets,
    const int* yOffsets, const int* widths, const int* heights,
    unsigned int format, const int* imageSizes, const void* const* data);
void R_PrintSeparatedWords(char* text);
void DebugCallback(unsigned int source, unsigned int type, unsigned int id,
                   unsigned int severity, int length, const char* message,
                   const void* userParam);
bool GL_OutOfMemory();
void GL_ClearOutOfMemory();
void InitNonStandardExtensions();
void GL_Init();
void DestroyDefaultFBO();
void EnumerateMonitors();
void GLW_WM_DISPLAYCHANGE();
void OnGameWindowCreated();
void R_ModeCompletion(idAutoComplete* autoComplete);
LRESULT CALLBACK FakeWndProc(HWND window, UINT message, WPARAM wParam,
                             LPARAM lParam);
void CreateWindowClasses(HINSTANCE instance);
LONG MyChangeDisplaySettings(const char* deviceName, DEVMODEA* mode,
                             HWND window, DWORD flags, void* parameter);
BOOL GetWGLExtensionsWithFakeWindow();
HGLRC CreateOpenGLContextOnDC(HDC dc, HGLRC shareWithContext);
HGLRC CreateOpenGLContextForCurrentThread(HWND* hiddenContextWindow,
                                          HGLRC shareWithContext);
idThreadGraphicsContext* AllocThreadGraphicsContextAndShare(
    HGLRC shareWithContext);
void SetupVidMode();
int CreateDefaultFBO(int requestedSamples, bool useFloatBuffers);
BOOL CALLBACK MonitorEnumProc(HMONITOR monitor, HDC monitorDC,
                              RECT* monitorRect, LPARAM data);

static_assert(sizeof(vidMode_t) == 28, "video-mode ABI drift");
static_assert(sizeof(monitor_t) == 40, "monitor ABI drift");
static_assert(sizeof(idThreadGraphicsContext) == 16,
              "thread graphics-context ABI drift");
static_assert(offsetof(sys3D_t, win32DC) == 0x600,
              "sys3D DC offset drift");
static_assert(offsetof(sys3D_t, vidMode) == 0x618,
              "sys3D video-mode offset drift");
static_assert(offsetof(sys3D_t, monitors) == 0x638,
              "sys3D monitor-list offset drift");
static_assert(offsetof(sys3D_t, lastDC) == 0x650,
              "sys3D last-DC offset drift");
static_assert(offsetof(sys3D_t, mainThreadContext) == 0x658,
              "sys3D main-context offset drift");
static_assert(offsetof(sys3D_t, pixelFormat) == 0x660,
              "sys3D pixel-format offset drift");
static_assert(offsetof(sys3D_t, relayContext) == 0x690,
              "sys3D relay-context offset drift");
static_assert(offsetof(sys3D_t, renderThreadContext) == 0x698,
              "sys3D render-context offset drift");
static_assert(offsetof(sys3D_t, curentSwapInterval) == 0x6A4,
              "sys3D swap-interval offset drift");
static_assert(sizeof(sys3D_t) == 0x6A8, "sys3D ABI drift");
