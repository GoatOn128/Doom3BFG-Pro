#include "engine/renderer/rendersystem_ogl_pc.h"

#include "engine/renderer/gl_api.h"
#include "engine/renderer/glconfig.h"
#include "engine/renderer/image.h"
#include "engine/renderer/imagemanager.h"
#include "engine/renderer/jobs/render/graphicsapiwrapper.h"
#include "engine/renderer/renderdestination.h"
#include "engine/renderer/renderlocal.h"
#include "engine/renderer/renderthread.h"
#include "engine/sys/win32/win_local.h"
#include "engine/sys/win32/win_main.h"
#include "engine/sys/win32/win_wndproc.h"
#include "shared/idlib/csystems/autocomplete.h"
#include "shared/idlib/csystems/cvarsystemlocal.h"
#include "shared/idlib/lib.h"
#include "shared/idlib/sys/sys_alloc.h"
#include "shared/idlib/sys/sys_time.h"
#include "shared/idlib/text/parser.h"

#include <algorithm>
#include <array>
#include <cctype>
#include <cmath>
#include <cstdio>
#include <cstring>
#include <new>
#include <numeric>
#include <string>

__forceinline void* operator new(std::size_t size, memTag_t tag) noexcept {
  return idMem::Singleton()->AllocWithLocation(
      "w:\\build\\tech5\\shared\\idlib\\Heap.h(95) : tag", size, tag,
      false, ALIGN_16, HEAP_DEFAULTHEAP);
}

__forceinline void operator delete(void* pointer, memTag_t) noexcept {
  idMem::Singleton()->Free(pointer, ALIGN_16);
}

namespace {

constexpr unsigned GL_NO_ERROR_VALUE = 0;
constexpr unsigned GL_INVALID_ENUM_VALUE = 0x0500;
constexpr unsigned GL_INVALID_VALUE_VALUE = 0x0501;
constexpr unsigned GL_INVALID_OPERATION_VALUE = 0x0502;
constexpr unsigned GL_STACK_OVERFLOW_VALUE = 0x0503;
constexpr unsigned GL_STACK_UNDERFLOW_VALUE = 0x0504;
constexpr unsigned GL_OUT_OF_MEMORY_VALUE = 0x0505;
constexpr unsigned GL_INVALID_FRAMEBUFFER_OPERATION_VALUE = 0x0506;
constexpr unsigned GL_VENDOR_VALUE = 0x1F00;
constexpr unsigned GL_RENDERER_VALUE = 0x1F01;
constexpr unsigned GL_VERSION_VALUE = 0x1F02;
constexpr unsigned GL_EXTENSIONS_VALUE = 0x1F03;
constexpr unsigned GL_NUM_EXTENSIONS_VALUE = 0x821D;

using wglGetExtensionsStringARBProc_t = const char* (WINAPI*)(HDC);
using wglSwapIntervalEXTProc_t = BOOL(WINAPI*)(int);
using wglGetPixelFormatAttribivARBProc_t = BOOL(WINAPI*)(
    HDC, int, int, unsigned, const int*, int*);
using wglGetPixelFormatAttribfvARBProc_t = BOOL(WINAPI*)(
    HDC, int, int, unsigned, const int*, float*);
using wglChoosePixelFormatARBProc_t = BOOL(WINAPI*)(
    HDC, const int*, const float*, unsigned, int*, unsigned*);
using wglCreateContextAttribsARBProc_t = HGLRC(WINAPI*)(HDC, HGLRC,
                                                       const int*);

wglGetExtensionsStringARBProc_t wglGetExtensionsStringARB{};
wglSwapIntervalEXTProc_t wglSwapIntervalEXT{};
wglGetPixelFormatAttribivARBProc_t wglGetPixelFormatAttribivARB{};
wglGetPixelFormatAttribfvARBProc_t wglGetPixelFormatAttribfvARB{};
wglChoosePixelFormatARBProc_t wglChoosePixelFormatARB{};
wglCreateContextAttribsARBProc_t wglCreateContextAttribsARB{};

bool glOutOfMemory{};
bool insideDisplayChange{};
bool glInStudio{};
int lastSwapTime{};
std::array<int, 128> frameTimes{};
int frameTimeIndex{};

constexpr PIXELFORMATDESCRIPTOR fakePixelFormat = {
    sizeof(PIXELFORMATDESCRIPTOR),
    1,
    PFD_DRAW_TO_WINDOW | PFD_SUPPORT_OPENGL | PFD_DOUBLEBUFFER,
    PFD_TYPE_RGBA,
    32,
    0,
    0,
    0,
    0,
    0,
    0,
    8,
    0,
    0,
    0,
    0,
    0,
    0,
    24,
    8,
    0,
    PFD_MAIN_PLANE,
    0,
    0,
    0,
    0,
};

bool HasExtension(const char* extensions, const char* extension) {
  return std::strstr(extensions, extension) != nullptr;
}

void ReportExtension(const char* name, bool present) {
  if (present) {
    idLib::Printf("---using %s\n", name);
  } else {
    idLib::Printf("X..%s not found\n", name);
  }
}

template <typename Function>
Function LoadExtension(const char* name) {
  return reinterpret_cast<Function>(renderSystem->ExtensionPointer(name));
}

int GreatestCommonDivisor(int lhs, int rhs) {
  lhs = std::abs(lhs);
  rhs = std::abs(rhs);
  while (rhs != 0) {
    const int remainder = lhs % rhs;
    lhs = rhs;
    rhs = remainder;
  }
  return lhs == 0 ? 1 : lhs;
}

void NormalizeAspect(int width, int height, int& numerator, int& denominator) {
  const int divisor = GreatestCommonDivisor(width, height);
  numerator = width / divisor;
  denominator = height / divisor;
  if (denominator == 5) {
    numerator *= 2;
    denominator = 10;
  }
}

void NoOpClampColor(unsigned, unsigned) {}

}  // namespace

extern idCVar r_useSMP;
extern idCVar r_swapInterval;
extern idCVar r_useFloatBuffers;
extern idCVar r_useUniformArrays;
extern idCVar r_generateMipMaps;
extern bool forceIdListsToReallocateEveryAppend;

extern "C" unsigned int glewInit();

idCVar r_debugContext(
    "r_debugContext", "0", 16386,
    "Init context for ARB_debug_output extension, 2 = force sync mode, 3 = also low priority messages");
idCVar r_ignoreGLErrors(
    "r_ignoreGLErrors", "2", 2,
    "1 = ignore except forcePrint, 2 = ignore GL errors, -1 = fatal error");
idCVar r_glMajorVersion("r_glMajorVersion", "3", 16386,
                        "OpenGL Major Version");
idCVar r_glMinorVersion("r_glMinorVersion", "2", 16386,
                        "OpenGL Minor Version");
idCVar r_glSingleContext("r_glSingleContext", "0", 16385,
                         "Only use one gl-context (ati-workaround).");
idCVar r_useGPUTimer(
    "r_useGPUTimer", "1", 18,
    "Use the GL_ext_timer_query extension, 2 for force on (else is disabled on amd)");
idCVar r_glUseDirectStateAccess(
    "r_glUseDirectStateAccess", "1", 1,
    "Use the GL_EXT_direct_state_access for buffer management");
idCVar r_multiSamples(
    "r_multiSamples", "0", 65538,
    "number of anti-aliasing samples (0 = no multi-sampled FBO)", 0.0F,
    64.0F);
idCVar r_multiSamplesGuiClamp(
    "r_multiSamplesGuiClamp", "8", 65538,
    "maximum number of anti-aliasing samples choosable from the option gui",
    0.0F, 64.0F);
idCVar r_mode(
    "r_mode", "-1", 65538,
    "Select the resolution and monitor to use in fullscreen mode (hit TAB to view available modes)",
    R_ModeCompletion);
idCVar r_initialMonitor("r_initialMonitor", "0", 65538,
                        "The initial monitor used to initialize the render system");
idCVar r_displayRefresh("r_displayRefresh", "0", 2,
                        "optional display refresh rate option for vid mode",
                        0.0F, 200.0F);
idCVar r_fullscreen("r_fullscreen", "1", 65537,
                    "0 = windowed, 1 = full screen");
idCVar r_fullscreenWindowed("r_fullscreenWindowed", "0", 65537,
                            "0 = actual fullscreen, 1 = use desktop");
idCVar r_stealFocus("r_stealFocus", "1", 65537,
                    "If set, gl system will steal focus during setup");
idCVar r_windowWidth("r_windowWidth", "0", 65538,
                     "window width when running in a window");
idCVar r_windowHeight("r_windowHeight", "0", 65538,
                      "window height when running in a window");
idCVar r_windowPosX("r_windowPosX", "0", 65538,
                    "window X position when running in a window");
idCVar r_windowPosY("r_windowPosY", "0", 65538,
                    "window Y position when running in a window");

sys3D_t sys3D;
glTexMultiSubImage2DAMDProc_t glTexMultiSubImage2DAMD =
    qglTexMultiSubImage2DAMD;
glCompressedTexMultiSubImage2DAMDProc_t
    glCompressedTexMultiSubImage2DAMD = qglCompressedTexMultiSubImage2DAMD;

void qglBindMultiTextureEXT(unsigned textureUnit, unsigned target,
                            unsigned texture) {
  _glewActiveTextureARB(textureUnit);
  glBindTexture(target, texture);
}

void qglTexMultiSubImage2DAMD(unsigned target, int level, int count,
                              const int* xOffsets, const int* yOffsets,
                              const int* widths, const int* heights,
                              unsigned format, unsigned type,
                              const void* const* pixels) {
  for (int index = 0; index < count; ++index) {
    glTexSubImage2D(target, level, xOffsets[index], yOffsets[index],
                    widths[index], heights[index], format, type,
                    pixels[index]);
  }
}

void qglCompressedTexMultiSubImage2DAMD(
    unsigned target, int level, int count, const int* xOffsets,
    const int* yOffsets, const int* widths, const int* heights,
    unsigned format, const int* imageSizes, const void* const* data) {
  for (int index = 0; index < count; ++index) {
    _glewCompressedTexSubImage2DARB(
        target, level, xOffsets[index], yOffsets[index], widths[index],
        heights[index], format, imageSizes[index], data[index]);
  }
}

void qglGetMultiQueryObjectuivAMD(unsigned* ids, unsigned count,
                                  unsigned parameter, unsigned* values) {
  if (static_cast<int>(count) > 0) {
    for (unsigned index = 0; index < count; ++index) {
      _glewGetQueryObjectuivARB(ids[index], parameter, &values[index]);
    }
  }
}

__declspec(noinline) void R_PrintSeparatedWords(char* text) {
  int column = 0;
  if (*text != '\0') {
    do {
      idStr word;
      word.Clear();
      while (*text > 32) {
        word.Append(*text);
        ++text;
      }
      idLib::Printf("%-40s", word.data);
      if (++column >= 2) {
        idLib::Printf("\n");
        column = 0;
      }
      ++text;
    } while (*text != '\0');
    if (column > 0) {
      idLib::Printf("\n");
    }
  }
}

void DebugCallback(unsigned, unsigned, unsigned, unsigned, int,
                   const char* message, const void*) {
  OutputDebugStringA(message);
  OutputDebugStringA("\n");
}

unsigned GL_GetError() {
  const unsigned error = glGetError();
  if (error == GL_OUT_OF_MEMORY_VALUE) {
    glOutOfMemory = true;
  }
  return error;
}

bool GL_OutOfMemory() { return glOutOfMemory; }

void GL_ClearOutOfMemory() { glOutOfMemory = false; }

bool idRenderSystemLocal::IsFullscreen() { return sys3D.vidMode.fullscreen; }

int idRenderSystemLocal::GetWidth() { return sys3D.vidMode.width; }

int idRenderSystemLocal::GetHeight() { return sys3D.vidMode.height; }

bool idRenderSystemLocal::IgnoreSizeMessages() {
  return sys3D.ignoreSizeMessages;
}

stereoRenderMode_t idRenderSystemLocal::GetStereoRenderMode() {
  return sys3D.vidMode.width == 1280 && sys3D.vidMode.height == 1470
             ? STEREO_RENDER_TOP_AND_BOTTOM
             : STEREO_RENDER_OFF;
}

float idRenderSystemLocal::GetPixelAspect() {
  return sys3D.vidMode.pixelAspect;
}

int idRenderSystemLocal::GetMaxSupportedMultisamples() {
  return glConfig.maxSamples;
}

void idRenderSystemLocal::SetGamma(std::uint16_t* red, std::uint16_t* green,
                                   std::uint16_t* blue) {
  if (sys3D.mainThreadContext == nullptr) {
    return;
  }
  std::uint16_t table[3][256]{};
  for (int index = 0; index < 256; ++index) {
    table[0][index] = red[index];
    table[1][index] = green[index];
    table[2][index] = blue[index];
  }
  if (!SetDeviceGammaRamp(sys3D.win32DC, table)) {
    idLib::Printf("WARNING: SetDeviceGammaRamp failed.\n");
  }
}

LRESULT CALLBACK FakeWndProc(HWND window, UINT message, WPARAM wParam,
                             LPARAM lParam) {
  if (message == WM_CREATE) {
    HDC dc = GetDC(window);
    const int pixelFormat = ChoosePixelFormat(dc, &fakePixelFormat);
    ::SetPixelFormat(dc, pixelFormat, &fakePixelFormat);
    ReleaseDC(window, dc);
  }
  return DefWindowProcA(window, message, wParam, lParam);
}

void OnGameWindowCreated() {
  sys3D.win32DC = GetDC(win32.hWnd);
  renderSystem->SetPixelFormat(sys3D.win32DC);
}

void CreateWindowClasses(HINSTANCE instance) {
  WNDCLASSA windowClass{};
  windowClass.style = CS_OWNDC;
  windowClass.hInstance = instance;
  windowClass.lpfnWndProc = MainWndProc;
  windowClass.hIcon = LoadIconA(instance, MAKEINTRESOURCEA(101));
  windowClass.hbrBackground = static_cast<HBRUSH>(GetStockObject(BLACK_BRUSH));
  windowClass.lpszClassName = "Wolfenstein The New Order";
  if (RegisterClassA(&windowClass) == 0) {
    idLib::FatalError("CreateWindowClasses: could not register window class");
  }
  idLib::Printf("---registered window class\n");

  windowClass.lpfnWndProc = FakeWndProc;
  windowClass.lpszClassName = "wolf_WGL_FAKE";
  if (RegisterClassA(&windowClass) == 0) {
    idLib::FatalError("CreateWindowClasses: could not register window class");
  }
  idLib::Printf("---registered fake window class\n");

  windowClass.lpfnWndProc = DefWindowProcA;
  windowClass.lpszClassName = "wolf_CONTEXT";
  if (RegisterClassA(&windowClass) == 0) {
    idLib::FatalError("CreateWindowClasses: could not register window class");
  }
  idLib::Printf("---registered context window class\n");
}

LONG MyChangeDisplaySettings(const char* deviceName, DEVMODEA* mode,
                             HWND window, DWORD flags, void* parameter) {
  DEVMODEA current{};
  current.dmSize = sizeof(current);
  if (EnumDisplaySettingsA(deviceName, ENUM_CURRENT_SETTINGS, &current)) {
    DEVMODEA registry{};
    DEVMODEA* requested = mode;
    if (requested == nullptr) {
      registry.dmSize = sizeof(registry);
      if (EnumDisplaySettingsA(deviceName, ENUM_REGISTRY_SETTINGS, &registry)) {
        requested = &registry;
      }
    }
    if (requested != nullptr) {
      const DWORD commonFields = requested->dmFields & current.dmFields;
      const bool same =
          ((commonFields & DM_BITSPERPEL) == 0 ||
           requested->dmBitsPerPel == current.dmBitsPerPel) &&
          ((commonFields & DM_DISPLAYFLAGS) == 0 ||
           requested->dmDisplayFlags == current.dmDisplayFlags) &&
          ((commonFields & DM_DISPLAYFREQUENCY) == 0 ||
           requested->dmDisplayFrequency == current.dmDisplayFrequency) &&
          ((commonFields & DM_POSITION) == 0 ||
           (requested->dmPosition.x == current.dmPosition.x &&
            requested->dmPosition.y == current.dmPosition.y)) &&
          ((commonFields & DM_PELSWIDTH) == 0 ||
           requested->dmPelsWidth == current.dmPelsWidth) &&
          ((commonFields & DM_PELSHEIGHT) == 0 ||
           requested->dmPelsHeight == current.dmPelsHeight);
      if (same) {
        return DISP_CHANGE_SUCCESSFUL;
      }
    }
  }
  insideDisplayChange = true;
  const LONG result =
      ChangeDisplaySettingsExA(deviceName, mode, window, flags, parameter);
  insideDisplayChange = false;
  return result;
}

void idRenderSystemLocal::ShutdownSystem() {
  idLib::Printf("Shutting down OpenGL subsystem\n");
  if (sys3D.mainThreadContext != nullptr) {
    wglMakeCurrent(nullptr, nullptr);
    wglDeleteContext(sys3D.mainThreadContext);
    sys3D.mainThreadContext = nullptr;
  }
  if (!GetUsingSMP()) {
    idLib::Printf("...releasing DC\n");
    ReleaseDC(win32.hWnd, sys3D.win32DC);
    sys3D.win32DC = nullptr;
    if (win32.hWnd != nullptr) {
      idLib::Printf("...destroying window\n");
      ShowWindow(win32.hWnd, SW_HIDE);
      DestroyWindow(win32.hWnd);
      win32.hWnd = nullptr;
    }
  }
  if (sys3D.oldHardwareGamma[0][255] != 0) {
    const HWND desktop = GetDesktopWindow();
    HDC dc = GetDC(desktop);
    SetDeviceGammaRamp(dc, sys3D.oldHardwareGamma);
    ReleaseDC(GetDesktopWindow(), dc);
  }
}

void (*idRenderSystemLocal::ExtensionPointer(const char* name))() {
  auto function = reinterpret_cast<void (*)()>(wglGetProcAddress(name));
  if (function == nullptr) {
    idLib::Printf("Couldn't find proc address for: %s\n", name);
  }
  return function;
}

void idRenderSystemLocal::GoWindowed() {
  r_fullscreen.SetBool(false, true);
  Restart();
}

void idRenderSystemLocal::GoFullScreen() {
  r_fullscreen.SetBool(true, true);
  Restart();
}

void idRenderSystemLocal::SetStudioMode(bool inStudio) {
  glInStudio = inStudio;
  LONG style = GetWindowLongA(win32.hWnd, GWL_STYLE);
  style = inStudio ? style & ~static_cast<LONG>(0x00CC0000)
                   : style | static_cast<LONG>(0x00CC0000);
  SetWindowLongA(win32.hWnd, GWL_STYLE, style);
  RedrawWindow(win32.hWnd, nullptr, nullptr,
               RDW_FRAME | RDW_ERASE);
}

void* idRenderSystemLocal::GetGameWindow() { return sys3D.win32DC; }

bool idRenderSystemLocal::MakeCurrent(void* deviceContext) {
  if (!idLib::IsMainThread()) {
    idLib::FatalError("MakeCurrent was called from the wrong thread");
  }
  if (sys3D.mainThreadContext == nullptr) {
    return false;
  }
  if (deviceContext != nullptr) {
    sys3D.lastDC = static_cast<HDC>(deviceContext);
    return wglMakeCurrent(static_cast<HDC>(deviceContext),
                          sys3D.mainThreadContext) != FALSE;
  }
  wglMakeCurrent(nullptr, nullptr);
  return true;
}

void idRenderSystemLocal::HideGameWindow() {
  ShowWindow(win32.hWnd, SW_HIDE);
  MyChangeDisplaySettings(nullptr, nullptr, nullptr, 0, nullptr);
}

void idRenderSystemLocal::SetPixelFormat(void* deviceContext) {
  if (!::SetPixelFormat(static_cast<HDC>(deviceContext), sys3D.pixelFormat,
                        &sys3D.pfd)) {
    idLib::FatalError("SetPixelFormat failed");
  }
}

int idRenderSystemLocal::GetCurrentMonitor() { return sys3D.currentMonitor; }

void InitNonStandardExtensions() {
  const char* extensions = glConfig.extensions_string.data;
  if (HasExtension(extensions, "GL_AMD_multi_texsubimage")) {
    ReportExtension("GL_AMD_multi_texsubimage", true);
    glTexMultiSubImage2DAMD =
        LoadExtension<glTexMultiSubImage2DAMDProc_t>(
            "glTexMultiSubImage2DAMD");
    if (glTexMultiSubImage2DAMD == nullptr) {
      glTexMultiSubImage2DAMD = qglTexMultiSubImage2DAMD;
    }
    glCompressedTexMultiSubImage2DAMD =
        LoadExtension<glCompressedTexMultiSubImage2DAMDProc_t>(
            "glCompressedTexMultiSubImage2DAMD");
    if (glCompressedTexMultiSubImage2DAMD == nullptr) {
      glCompressedTexMultiSubImage2DAMD =
          qglCompressedTexMultiSubImage2DAMD;
    }
  } else {
    ReportExtension("GL_AMD_multi_texsubimage", false);
  }
  glGetMultiQueryObjectuivAMD =
      LoadExtension<glGetMultiQueryObjectuivAMDProc_t>(
          "glGetMultiQueryObjectuivAMD");
  if (glGetMultiQueryObjectuivAMD == nullptr) {
    glGetMultiQueryObjectuivAMD = qglGetMultiQueryObjectuivAMD;
  }
}

void GL_CheckErrors(bool forcePrint) {
  if (r_ignoreGLErrors.GetInteger() == 2) {
    return;
  }
  for (int index = 0; index < 10; ++index) {
    const unsigned error = glGetError();
    if (error == GL_NO_ERROR_VALUE) {
      return;
    }
    const char* errorName = nullptr;
    switch (error) {
      case GL_INVALID_ENUM_VALUE:
        errorName = "GL_INVALID_ENUM ";
        break;
      case GL_INVALID_VALUE_VALUE:
        errorName = "GL_INVALID_VALUE ";
        break;
      case GL_INVALID_OPERATION_VALUE:
        errorName = "GL_INVALID_OPERATION ";
        break;
      case GL_STACK_OVERFLOW_VALUE:
        errorName = "GL_STACK_OVERFLOW ";
        break;
      case GL_STACK_UNDERFLOW_VALUE:
        errorName = "GL_STACK_UNDERFLOW ";
        break;
      case GL_OUT_OF_MEMORY_VALUE:
        glOutOfMemory = true;
        idLib::FatalError("GL_OUT_OF_MEMORY");
      case GL_INVALID_FRAMEBUFFER_OPERATION_VALUE:
        errorName = "GL_INVALID_FRAMEBUFFER_OPERATION_EXT ";
        break;
      default:
        break;
    }
    char numericName[128]{};
    if (errorName == nullptr) {
      idStr::snPrintf(numericName, sizeof(numericName), "%i ",
                      static_cast<int>(error));
      errorName = numericName;
    }
    if (r_ignoreGLErrors.GetInteger() == -1) {
      idLib::FatalError("GL_CheckErrors: %s\n", errorName);
    }
    if (forcePrint || r_ignoreGLErrors.GetInteger() == 0) {
      idLib::Printf("GL_CheckErrors: %s\n", errorName);
    }
  }
}

int idRenderSystemLocal::GetNumMultisamples() {
  return (std::max)(sys3D.numSamples, 1);
}

bool idRenderSystemLocal::GetUsingSMP() {
  return r_useSMP.GetInteger() != 0 && r_glSingleContext.GetInteger() == 0;
}

BOOL GetWGLExtensionsWithFakeWindow() {
  HWND window = CreateWindowExA(
      0, "wolf_WGL_FAKE", "Wolfenstein The New Order",
      WS_OVERLAPPEDWINDOW, 40, 40, 640, 480, nullptr, nullptr,
      win32.hInstance, nullptr);
  if (window == nullptr) {
    idLib::FatalError(
        "GetWGLExtensionsWithFakeWindow: Couldn't create fake window");
  }
  HDC dc = GetDC(window);
  HGLRC context = wglCreateContext(dc);
  wglMakeCurrent(dc, context);

  glConfig.vendor_string =
      reinterpret_cast<const char*>(glGetString(GL_VENDOR_VALUE));
  glConfig.renderer_string =
      reinterpret_cast<const char*>(glGetString(GL_RENDERER_VALUE));
  glConfig.version_string =
      reinterpret_cast<const char*>(glGetString(GL_VERSION_VALUE));
  idLib::Printf("GetWGLExtensionsWithFakeWindow: OpenGL Vendor: \"%s\"\n",
                glConfig.vendor_string != nullptr ? glConfig.vendor_string
                                                  : "<null>");
  idLib::Printf("GetWGLExtensionsWithFakeWindow: OpenGL Renderer: \"%s\"\n",
                glConfig.renderer_string != nullptr ? glConfig.renderer_string
                                                    : "<null>");
  idLib::Printf("GetWGLExtensionsWithFakeWindow: OpenGL version: \"%s\"\n",
                glConfig.version_string != nullptr ? glConfig.version_string
                                                   : "<null>");

  wglGetExtensionsStringARB =
      LoadExtension<wglGetExtensionsStringARBProc_t>(
          "wglGetExtensionsStringARB");
  glConfig.wgl_extensions_string =
      wglGetExtensionsStringARB != nullptr ? wglGetExtensionsStringARB(dc) : "";
  glConfig.swapControlAvailable = HasExtension(
      glConfig.wgl_extensions_string, "WGL_EXT_swap_control");
  ReportExtension("WGL_EXT_swap_control", glConfig.swapControlAvailable);
  wglSwapIntervalEXT =
      LoadExtension<wglSwapIntervalEXTProc_t>("wglSwapIntervalEXT");
  glConfig.swapControlTearAvailable = HasExtension(
      glConfig.wgl_extensions_string, "WGL_EXT_swap_control_tear");
  ReportExtension("WGL_EXT_swap_control_tear",
                  glConfig.swapControlTearAvailable);
  if (!glConfig.swapControlAvailable && r_swapInterval.GetInteger() == -1) {
    r_swapInterval.SetInteger(0, true);
  }

  wglGetPixelFormatAttribivARB =
      LoadExtension<wglGetPixelFormatAttribivARBProc_t>(
          "wglGetPixelFormatAttribivARB");
  wglGetPixelFormatAttribfvARB =
      LoadExtension<wglGetPixelFormatAttribfvARBProc_t>(
          "wglGetPixelFormatAttribfvARB");
  wglChoosePixelFormatARB = LoadExtension<wglChoosePixelFormatARBProc_t>(
      "wglChoosePixelFormatARB");
  wglCreateContextAttribsARB =
      reinterpret_cast<wglCreateContextAttribsARBProc_t>(
          wglGetProcAddress("wglCreateContextAttribsARB"));
  if (wglChoosePixelFormatARB == nullptr) {
    idLib::FatalError("wglChoosePixelFormatARB not available");
  }
  if (wglCreateContextAttribsARB == nullptr) {
    idLib::FatalError("wglCreateContextAttribsARB not available");
  }
  r_swapInterval.flags |= CVAR_MODIFIED;

  wglMakeCurrent(nullptr, nullptr);
  wglDeleteContext(context);
  ReleaseDC(window, dc);
  return DestroyWindow(window);
}

HGLRC CreateOpenGLContextOnDC(HDC dc, HGLRC shareWithContext) {
  int majorVersion = r_glMajorVersion.GetInteger();
  int minorVersion = r_glMinorVersion.GetInteger();
  const char* vendor =
      reinterpret_cast<const char*>(glGetString(GL_VENDOR_VALUE));
  if (vendor != nullptr && idStr::Icmpn(vendor, "Intel", 5) == 0) {
    majorVersion = 3;
    minorVersion = 1;
  }
  idLib::Printf("CreateOpenGLContextOnDC: Using gl version %d.%d\n",
                majorVersion, minorVersion);
  const int attributes[] = {
      0x2091, majorVersion, 0x2092, minorVersion,
      0x2094, r_debugContext.GetInteger() != 0 ? 1 : 0,
      0x9126, 2, 0};
  HGLRC context =
      wglCreateContextAttribsARB(dc, shareWithContext, attributes);
  if (context == nullptr) {
    const DWORD error = GetLastError();
    if (LOWORD(error) == 0x2095) {
      idLib::Printf("%sERROR_INVALID_VERSION_ARB: 0x%x\n",
                    error == 0x2095 ? "" : "Probably ", error);
    } else if (LOWORD(error) == 0x2096) {
      idLib::Printf("%sERROR_INVALID_PROFILE_ARB: 0x%x\n",
                    error == 0x2096 ? "" : "Probably ", error);
    } else {
      idLib::Printf("unknown error: 0x%x\n", error);
    }
    idLib::FatalError("wglCreateContextAttribsARB failed");
  }
  idLib::Printf("created OpenGL %d.%d context\n", majorVersion,
                minorVersion);
  return context;
}

HGLRC CreateOpenGLContextForCurrentThread(HWND* hiddenContextWindow,
                                          HGLRC shareWithContext) {
  idLib::Printf(
      "CreateOpenGLContextForCurrentThread: Assuming pre-checked Graphics Vendor: \"%s\"\n",
      glConfig.vendor_string);
  *hiddenContextWindow = CreateWindowExA(
      0, "wolf_CONTEXT", "Wolfenstein The New Order", WS_OVERLAPPEDWINDOW,
      0, 0, 32, 32, nullptr, nullptr, win32.hInstance, nullptr);
  if (*hiddenContextWindow == nullptr) {
    idLib::FatalError(
        "CreateOpenGLContextForCurrentThread: Couldn't create context window");
  }
  HDC dc = GetDC(*hiddenContextWindow);
  renderSystem->SetPixelFormat(dc);
  HGLRC context = CreateOpenGLContextOnDC(dc, shareWithContext);
  wglMakeCurrent(dc, context);
  const char* vendor =
      reinterpret_cast<const char*>(glGetString(GL_VENDOR_VALUE));
  idLib::Printf(
      "CreateOpenGLContextForCurrentThread: Actually got context with Vendor: \"%s\"\n",
      vendor);
  return context;
}

idThreadGraphicsContext* AllocThreadGraphicsContextAndShare(
    HGLRC shareWithContext) {
  auto* context = static_cast<idThreadGraphicsContext*>(
      idMem::Singleton()->AllocWithLocation(
          "w:\\build\\tech5\\shared\\idlib\\Heap.h(53) : TAG_NEW",
          sizeof(idThreadGraphicsContext), TAG_NEW, false, ALIGN_16,
          HEAP_DEFAULTHEAP));
  context->glContext = CreateOpenGLContextForCurrentThread(
      &context->contextWindow, shareWithContext);
  wglMakeCurrent(nullptr, nullptr);
  return context;
}

idThreadGraphicsContext* idRenderSystemLocal::AllocThreadGraphicsContext() {
  idThreadGraphicsContext* context =
      AllocThreadGraphicsContextAndShare(sys3D.relayContext->glContext);
  HDC dc = GetDC(context->contextWindow);
  wglMakeCurrent(dc, context->glContext);
  return context;
}

void idRenderSystemLocal::DestroyThreadGraphicsContext(
    idThreadGraphicsContext* context) {
  if (context->glContext != nullptr) {
    wglMakeCurrent(nullptr, nullptr);
    wglDeleteContext(context->glContext);
    DestroyWindow(context->contextWindow);
    idMem::Singleton()->Free(context, ALIGN_16);
  }
}

void idRenderSystemLocal::CreateRenderThreadContext() {
  sys3D.renderThreadContext =
      r_glSingleContext.GetInteger() != 0
          ? nullptr
          : AllocThreadGraphicsContextAndShare(sys3D.relayContext->glContext);
}

void SetupVidMode() {
  int width = 0;
  int height = 0;
  int monitor = 0;

  if (r_mode.GetInteger() < 0) {
    const HWND desktop = GetDesktopWindow();
    HDC dc = GetDC(desktop);
    const int desktopWidth = GetDeviceCaps(dc, HORZRES);
    const int desktopHeight = GetDeviceCaps(dc, VERTRES);
    ReleaseDC(GetDesktopWindow(), dc);

    for (int mode = 0;
         renderSystem->EnumerateModes(mode, &width, &height, &monitor);
         ++mode) {
      if (width == desktopWidth && height == desktopHeight && monitor == 0) {
        r_mode.SetInteger(mode, true);
      }
    }

    if (r_mode.GetInteger() < 0 &&
        renderSystem->EnumerateModes(0, &width, &height, &monitor)) {
      int monitorAspectX = 0;
      int monitorAspectY = 0;
      for (int mode = 0;; ++mode) {
        if (monitorAspectX == 0) {
          NormalizeAspect(sys3D.monitors[monitor].width,
                          sys3D.monitors[monitor].height, monitorAspectX,
                          monitorAspectY);
        }
        int modeAspectX = 0;
        int modeAspectY = 0;
        NormalizeAspect(width, height, modeAspectX, modeAspectY);
        if (modeAspectX == monitorAspectX && modeAspectY == monitorAspectY &&
            sys3D.monitors[monitor].width >= width) {
          r_mode.SetInteger(mode, true);
          break;
        }
        if (!renderSystem->EnumerateModes(mode + 1, &width, &height,
                                          &monitor)) {
          break;
        }
      }
    }
  }

  if (!renderSystem->EnumerateModes(r_mode.GetInteger(), &width, &height,
                                    &monitor)) {
    renderSystem->EnumerateModes(0, &width, &height, &monitor);
  }

  sys3D.vidMode.pixelAspect = 1.0F;
  sys3D.vidMode.monitorNum = monitor;
  sys3D.vidMode.fullscreen = r_fullscreen.GetBool();
  if (sys3D.vidMode.fullscreen) {
    sys3D.vidMode.x = 0;
    sys3D.vidMode.y = 0;
    if (r_fullscreenWindowed.GetBool()) {
      MONITORINFOEXA info{};
      info.cbSize = sizeof(info);
      GetMonitorInfoA(sys3D.monitors[monitor].hMonitor, &info);
      sys3D.vidMode.width = info.rcMonitor.right - info.rcMonitor.left;
      sys3D.vidMode.height = info.rcMonitor.bottom - info.rcMonitor.top;
    } else {
      sys3D.vidMode.width = width;
      sys3D.vidMode.height = height;
    }
  } else {
    sys3D.vidMode.x = r_windowPosX.GetInteger();
    sys3D.vidMode.y = r_windowPosY.GetInteger();
    if (r_windowWidth.GetInteger() >= 1 &&
        r_windowHeight.GetInteger() >= 1) {
      sys3D.vidMode.width = r_windowWidth.GetInteger();
      sys3D.vidMode.height = r_windowHeight.GetInteger();
    } else if (width >= 1 && height >= 1) {
      sys3D.vidMode.width = width;
      sys3D.vidMode.height = height;
    } else {
      sys3D.vidMode.width = 960;
      sys3D.vidMode.height = 540;
    }
  }
  sys3D.curentSwapInterval = 0xFFFF;
}

int CreateDefaultFBO(int requestedSamples, bool useFloatBuffers) {
  int samples = _glewSampleMaski != nullptr ? requestedSamples : 0;
  samples = (std::max)(samples, 0);
  samples = (std::min)(samples, glConfig.maxColorTextureSamples);
  samples = (std::min)(samples, glConfig.maxDepthTextureSamples);

  idImage* frontColor = nullptr;
  idImage* frontScratch = nullptr;
  idImage* frontFeedback = nullptr;
  idImage* frontDepthStencil = nullptr;
  idImage* frontNormal = nullptr;

  GL_SetDefaultState();
  for (int attempt = 0;; ++attempt) {
    idImageOpts colorOptions{};
    colorOptions.textureType = TT_2D;
    colorOptions.format = useFloatBuffers ? FMT_RGBA16F : FMT_RGBA8;
    colorOptions.colorFormat = CFM_DEFAULT;
    colorOptions.width = renderSystem->GetWidth();
    colorOptions.height = renderSystem->GetHeight();
    colorOptions.depth = 1;
    colorOptions.numLevels = 1;
    colorOptions.numSamples = samples;
    colorOptions.packedTail = true;
    colorOptions.textureBindType = TB_SHADER;
    colorOptions.textureUsage = TU_DEFAULT;
    colorOptions.textureOptions = TO_NONE;
    colorOptions.sampler.filter = TF_NEAREST;
    colorOptions.sampler.repeat = TR_CLAMP;
    colorOptions.sampler.lodMaxClamp = 0.0F;
    idImageOpts rgbaOptions = colorOptions;
    rgbaOptions.format = FMT_RGBA8;
    idImageOpts depthOptions = colorOptions;
    depthOptions.format = FMT_DEPTH_STENCIL;
    frontColor = globalImages->ScratchImage("_frontColor", colorOptions);
    frontScratch = globalImages->ScratchImage("_frontScratch", colorOptions);
    frontFeedback =
        globalImages->ScratchImage("_frontFeedback", rgbaOptions);
    frontDepthStencil =
        globalImages->ScratchImage("_frontDepthStencil", depthOptions);
    frontNormal = globalImages->ScratchImage("_frontNormal", rgbaOptions);

    const bool allocationFailed =
        frontColor->allocationFailed || frontFeedback->allocationFailed ||
        frontDepthStencil->allocationFailed ||
        (frontNormal != nullptr && frontNormal->allocationFailed);
    if (attempt != 0 || samples == 0 || !allocationFailed) {
      break;
    }
    frontColor->PurgeImage();
    frontFeedback->PurgeImage();
    frontDepthStencil->PurgeImage();
    if (frontNormal != nullptr) {
      frontNormal->PurgeImage();
    }
    samples = 0;
  }

  idMem::Singleton()->PushHeap(HEAP_SYSTEMHEAP);
  renderDestNative = nullptr;
  renderDestScaledColor = new (TAG_RENDERER) idRenderDestination();
  renderDestScaledColor2 = new (TAG_RENDERER) idRenderDestination();
  renderDestScaledDepth = new (TAG_RENDERER) idRenderDestination();
  renderDestScaledFeedback = new (TAG_RENDERER) idRenderDestination();
  renderDestScaledColorWithFeedback =
      new (TAG_RENDERER) idRenderDestination();
  renderDestScaledLinear = nullptr;
  idMem::Singleton()->PopHeap();

  renderDestScaledColor->SetDefault();
  renderDestScaledColor->CreateFromImages(frontColor, frontDepthStencil,
                                           frontDepthStencil);
  renderDestScaledColor2->SetDefault();
  renderDestScaledColor2->CreateFromImages(frontScratch, frontDepthStencil,
                                            frontDepthStencil);
  renderDestScaledDepth->SetDefault();
  renderDestScaledDepth->CreateFromImages(nullptr, frontDepthStencil,
                                           frontDepthStencil);
  renderDestScaledFeedback->SetDefault();
  renderDestScaledFeedback->CreateFromImages(frontFeedback,
                                              frontDepthStencil,
                                              frontDepthStencil);
  renderDestScaledColorWithFeedback->SetDefault();
  renderDestScaledColorWithFeedback->CreateFromImages(
      frontColor, frontDepthStencil, frontDepthStencil);
  renderDestScaledColorWithFeedback->AddTarget(frontFeedback);
  if (frontNormal != nullptr) {
    renderDestScaledColorWithFeedback->AddTarget(frontNormal);
  }
  GL_CheckErrors(false);
  return samples;
}

void idRenderSystemLocal::InitContext() {
  idLib::Printf("Initializing OpenGL subsystem\n");
  HWND window = CreateWindowExA(
      0, "Wolfenstein The New Order", "Wolfenstein",
      static_cast<DWORD>(0x80080000u), 0, 0, 32, 32, nullptr, nullptr,
      win32.hInstance, nullptr);
  if (window == nullptr) {
    idLib::FatalError("CreateWindow() - CreateWindowEx failed\n");
  }
  win32.hWnd = window;
  if (idLib::IsMainThread()) {
    wglMakeCurrent(sys3D.win32DC, sys3D.mainThreadContext);
  } else {
    wglMakeCurrent(sys3D.win32DC, sys3D.renderThreadContext->glContext);
  }
}

void idRenderSystemLocal::Restart() {
  if (idLib::headless) {
    return;
  }
  if (renderThread != nullptr && renderSystem->GetUsingSMP() &&
      idLib::IsMainThread()) {
    renderThread->deferredVidRestart = true;
    return;
  }
  SetupVidMode();
  if (IsWindowVisible(win32.hWnd)) {
    ShowGameWindow();
  }
  WindowChangeMultisamples(r_multiSamples.GetInteger(),
                           r_useFloatBuffers.GetBool());
  if (glInStudio) {
    SetStudioMode(true);
    ShowWindow(win32.hWnd, SW_SHOW);
    SendMessageA(win32.hWnd, WM_ACTIVATE, WA_ACTIVE, 0);
  }
  InvalidateRect(nullptr, nullptr, TRUE);
}

void idRenderSystemLocal::SwapBuffers(void* deviceContext, bool forceVsync) {
  GL_CheckErrors(false);
  pc.startSwapMicroSec = Sys_Microseconds();

  if (r_swapInterval.GetInteger() == -1 &&
      !glConfig.swapControlTearAvailable) {
    idLib::Warning(
        "swap-tear vsync turned off because the swap-tear extension is not available!");
    r_swapInterval.SetInteger(0, true);
  }

  if (r_swapInterval.GetInteger() != 0 && Sys_IsGameWindowActive() &&
      !win32.monitorPoweredOff) {
    const int now = static_cast<int>(Sys_Milliseconds());
    frameTimes[frameTimeIndex & 127] = now;
    ++frameTimeIndex;
    if (frameTimeIndex > 128) {
      const int newest = (frameTimeIndex - 1) & 127;
      const int oldest = frameTimeIndex & 127;
      const int elapsed = frameTimes[newest] - frameTimes[oldest];
      if (elapsed / 128 < 14) {
        idLib::Warning(
            "vsync turned off because the game is running too fast!");
        r_swapInterval.SetInteger(0, true);
      }
    }
  }

  if (r_swapInterval.GetInteger() == -1 &&
      static_cast<int>(Sys_Milliseconds()) - lastSwapTime < 12) {
    do {
      Sys_Sleep(1);
    } while (static_cast<int>(Sys_Milliseconds()) - lastSwapTime < 12);
  }

  if (forceVsync) {
    r_swapInterval.flags |= CVAR_MODIFIED;
    if (wglSwapIntervalEXT != nullptr && sys3D.curentSwapInterval != 1) {
      wglSwapIntervalEXT(1);
      sys3D.curentSwapInterval = 1;
    }
  } else if ((r_swapInterval.flags & CVAR_MODIFIED) != 0) {
    r_swapInterval.flags &= ~CVAR_MODIFIED;
    if (wglSwapIntervalEXT != nullptr &&
        sys3D.curentSwapInterval != r_swapInterval.GetInteger()) {
      wglSwapIntervalEXT(r_swapInterval.GetInteger());
      sys3D.curentSwapInterval = r_swapInterval.GetInteger();
    }
  }

  HDC dc = static_cast<HDC>(deviceContext);
  if (dc == nullptr) {
    dc = sys3D.lastDC;
  }
  ::SwapBuffers(dc);
  lastSwapTime = static_cast<int>(Sys_Milliseconds());
  pc.endSwapMicroSec = Sys_Microseconds();
}

void idRenderSystemLocal::WindowSizeDragged(int width, int height) {
  if (width > 0 && height > 0 && sys3D.mainThreadContext != nullptr &&
      !insideDisplayChange) {
    sys3D.vidMode.width = (std::max)(width, 1);
    sys3D.vidMode.height = (std::max)(height, 1);
  }
}

void idRenderSystemLocal::ShowGameWindow() {
  const char* screenMode = IsFullscreen() ? "full screen" : "windowed";
  const char* pixelDescription =
      std::fabs(sys3D.vidMode.pixelAspect - 1.0F) > 0.01F
          ? ", non-square pixels"
          : "";
  idLib::Printf("ShowGameWindow: (%i, %i) %i x %i, %s%s\n",
                sys3D.vidMode.x, sys3D.vidMode.y, sys3D.vidMode.width,
                sys3D.vidMode.height, screenMode, pixelDescription);

  int currentMonitor = sys3D.currentMonitor;
  sys3D.ignoreSizeMessages = true;
  if (currentMonitor == -1) {
    currentMonitor = r_initialMonitor.GetInteger();
  }
  sys3D.currentMonitor = currentMonitor;
  if (currentMonitor != sys3D.vidMode.monitorNum ||
      sys3D.currentFullscreen != sys3D.vidMode.fullscreen) {
    sys3D.currentMonitor = sys3D.vidMode.monitorNum;
    r_initialMonitor.SetInteger(sys3D.vidMode.monitorNum, true);
    const char* oldDevice = nullptr;
    MONITORINFOEXA oldInfo{};
    if (sys3D.currentFullscreen) {
      oldInfo.cbSize = sizeof(oldInfo);
      GetMonitorInfoA(sys3D.monitors[currentMonitor].hMonitor, &oldInfo);
      oldDevice = oldInfo.szDevice;
    }
    MyChangeDisplaySettings(oldDevice, nullptr, nullptr, 0, nullptr);
    if (renderThread != nullptr) {
      glFinish();
    }
  }
  sys3D.currentFullscreen = sys3D.vidMode.fullscreen;

  if (IsFullscreen()) {
    MONITORINFOEXA info{};
    info.cbSize = sizeof(info);
    GetMonitorInfoA(sys3D.monitors[sys3D.vidMode.monitorNum].hMonitor,
                    &info);
    if (r_fullscreenWindowed.GetBool()) {
      SetWindowLongA(win32.hWnd, GWL_STYLE,
                     static_cast<LONG>(0x800A0000u));
      SetWindowLongA(win32.hWnd, GWL_EXSTYLE, 0);
      SetWindowPos(win32.hWnd, HWND_NOTOPMOST, info.rcMonitor.left,
                   info.rcMonitor.top, sys3D.vidMode.width,
                   sys3D.vidMode.height,
                   SWP_FRAMECHANGED | SWP_SHOWWINDOW);
    } else {
      DEVMODEA mode{};
      mode.dmSize = sizeof(mode);
      mode.dmFields = DM_PELSWIDTH | DM_PELSHEIGHT | DM_BITSPERPEL;
      mode.dmPelsWidth = sys3D.vidMode.width;
      mode.dmPelsHeight = sys3D.vidMode.height;
      mode.dmBitsPerPel = 32;
      if (r_displayRefresh.GetInteger() != 0) {
        mode.dmDisplayFrequency = r_displayRefresh.GetInteger();
        mode.dmFields |= DM_DISPLAYFREQUENCY;
      }
      const LONG change = MyChangeDisplaySettings(
          info.szDevice, &mode, nullptr, CDS_FULLSCREEN, nullptr);
      if (change != DISP_CHANGE_SUCCESSFUL) {
        const char* message = "Unknown error";
        switch (change) {
          case DISP_CHANGE_BADDUALVIEW:
            message = "The settings change was unsuccessful because the system is DualView capable.";
            break;
          case DISP_CHANGE_BADPARAM:
            message = "An invalid parameter was passed in. This can include an invalid flag or combination of flags.";
            break;
          case DISP_CHANGE_BADFLAGS:
            message = "An invalid set of flags was passed in.";
            break;
          case DISP_CHANGE_NOTUPDATED:
            message = "Windows NT/2000/XP: Unable to write settings to the registry.";
            break;
          case DISP_CHANGE_BADMODE:
            message = "The graphics mode is not supported.";
            break;
          case DISP_CHANGE_FAILED:
            message = "The display driver failed the specified graphics mode.";
            break;
          case DISP_CHANGE_RESTART:
            message = "The computer must be restarted for the graphics mode to work.";
            break;
          default:
            break;
        }
        idLib::Printf("ChangeDisplaySettings failed: %s\n", message);
      } else {
        GetMonitorInfoA(sys3D.monitors[sys3D.vidMode.monitorNum].hMonitor,
                        &info);
        SetWindowLongA(win32.hWnd, GWL_STYLE,
                       static_cast<LONG>(0x800A0000u));
        SetWindowLongA(win32.hWnd, GWL_EXSTYLE, 0);
        SetWindowPos(win32.hWnd, HWND_NOTOPMOST, info.rcMonitor.left,
                     info.rcMonitor.top, sys3D.vidMode.width,
                     sys3D.vidMode.height,
                     SWP_FRAMECHANGED | SWP_SHOWWINDOW);
      }
      if (renderThread != nullptr) {
        glFinish();
      }
    }
  } else {
    MONITORINFOEXA info{};
    info.cbSize = sizeof(info);
    GetMonitorInfoA(sys3D.monitors[sys3D.vidMode.monitorNum].hMonitor,
                    &info);
    RECT rectangle{sys3D.vidMode.x, sys3D.vidMode.y,
                   sys3D.vidMode.x + sys3D.vidMode.width,
                   sys3D.vidMode.y + sys3D.vidMode.height};
    AdjustWindowRect(&rectangle, 0x00CE0000u, FALSE);
    if (rectangle.right - rectangle.left >
        info.rcWork.right - info.rcWork.left) {
      rectangle.left = info.rcWork.left;
      rectangle.right = info.rcWork.right;
    }
    if (rectangle.bottom - rectangle.top >
        info.rcWork.bottom - info.rcWork.top) {
      rectangle.top = info.rcWork.top;
      rectangle.bottom = info.rcWork.bottom;
    }
    if (rectangle.left < info.rcWork.left) {
      rectangle.right += info.rcWork.left - rectangle.left;
      rectangle.left = info.rcWork.left;
    }
    if (rectangle.right > info.rcWork.right) {
      rectangle.left -= rectangle.right - info.rcWork.right;
      rectangle.right = info.rcWork.right;
    }
    if (rectangle.top < info.rcWork.top) {
      rectangle.bottom += info.rcWork.top - rectangle.top;
      rectangle.top = info.rcWork.top;
    }
    if (rectangle.bottom > info.rcWork.bottom) {
      rectangle.top -= rectangle.bottom - info.rcWork.bottom;
      rectangle.bottom = info.rcWork.bottom;
    }
    SetWindowLongA(win32.hWnd, GWL_STYLE, 0x00CE0000);
    SetWindowLongA(win32.hWnd, GWL_EXSTYLE, 0);
    SetWindowPos(win32.hWnd, HWND_NOTOPMOST, rectangle.left, rectangle.top,
                 rectangle.right - rectangle.left,
                 rectangle.bottom - rectangle.top,
                 SWP_FRAMECHANGED | SWP_SHOWWINDOW);
  }

  if (r_stealFocus.GetBool()) {
    const DWORD currentThread = GetCurrentThreadId();
    const DWORD foregroundThread =
        GetWindowThreadProcessId(GetForegroundWindow(), nullptr);
    if (currentThread == foregroundThread) {
      SetForegroundWindow(win32.hWnd);
    } else {
      AttachThreadInput(currentThread, foregroundThread, TRUE);
      SetForegroundWindow(win32.hWnd);
      AttachThreadInput(currentThread, foregroundThread, FALSE);
    }
  }
  sys3D.ignoreSizeMessages = false;
}

bool idRenderSystemLocal::EnumerateModes(int mode, int* width, int* height,
                                         int* monitor) {
  *width = 1;
  *height = 1;
  *monitor = 0;
  if (mode < 0) {
    return false;
  }
  if (sys3D.monitors.Num() == 0) {
    *width = 0;
    *height = 0;
    return true;
  }
  int localMode = mode;
  for (int monitorIndex = 0; monitorIndex < sys3D.monitors.Num();
       ++monitorIndex) {
    const monitor_t& entry = sys3D.monitors[monitorIndex];
    if (localMode < entry.supportedModes.Num()) {
      const unsigned packed = entry.supportedModes[localMode];
      *monitor = monitorIndex;
      *width = static_cast<int>(packed >> 16U);
      *height = static_cast<int>(packed & 0xFFFFU);
      return true;
    }
    localMode -= entry.supportedModes.Num();
  }
  return false;
}

int idRenderSystemLocal::GetNumMonitors() { return sys3D.monitors.Num(); }

void idRenderSystemLocal::GetMonitorSize(int monitor, int* width,
                                         int* height) {
  *width = 1;
  *height = 1;
  if (monitor < sys3D.monitors.Num()) {
    *width = sys3D.monitors[monitor].width;
    *height = sys3D.monitors[monitor].height;
  }
}

void R_ModeCompletion(idAutoComplete* autoComplete) {
  const char* format = sys3D.monitors.Num() == 1
                           ? "%d %s(%4d x %4d [%d:%d])^0"
                           : "%d %s(%4d x %4d [%d:%d]) on monitor %d^0";
  int width = 0;
  int height = 0;
  int monitor = 0;
  for (int mode = 0;
       renderSystem->EnumerateModes(mode, &width, &height, &monitor);
       ++mode) {
    int monitorAspectX = 0;
    int monitorAspectY = 0;
    int modeAspectX = 0;
    int modeAspectY = 0;
    NormalizeAspect(sys3D.monitors[monitor].width,
                    sys3D.monitors[monitor].height, monitorAspectX,
                    monitorAspectY);
    NormalizeAspect(width, height, modeAspectX, modeAspectY);
    const char* color = monitorAspectX == modeAspectX &&
                                monitorAspectY == modeAspectY
                            ? "^7"
                            : "^8";
    idStr suggestion(va(format, mode, color, width, height, modeAspectX,
                        modeAspectY, monitor));
    autoComplete->Append(suggestion, -1);
  }
}

void GL_Init() {
  idLib::Printf("----- GL_Init -----\n");
  if (glewInit() != 0) {
    idLib::FatalError("glewInit() failed");
  }

  glConfig.vendor_string =
      reinterpret_cast<const char*>(glGetString(GL_VENDOR_VALUE));
  glConfig.renderer_string =
      reinterpret_cast<const char*>(glGetString(GL_RENDERER_VALUE));
  glConfig.version_string =
      reinterpret_cast<const char*>(glGetString(GL_VERSION_VALUE));
  if (glConfig.renderer_string == nullptr ||
      glConfig.version_string == nullptr) {
    idLib::FatalError("Couldn't get glConfig strings");
  }
  glConfig.glVersion =
      static_cast<float>(std::atof(glConfig.version_string));
  idLib::Printf("OpenGL version %3.1f\n", glConfig.glVersion);
  if (glConfig.glVersion < 3.0F) {
    idLib::Error("glConfig.glVersion %f < 3.0", glConfig.glVersion);
  }

  _glewGetStringi =
      LoadExtension<glGetStringiProc_t>("glGetStringi");
  int numExtensions = 0;
  glGetIntegerv(GL_NUM_EXTENSIONS_VALUE, &numExtensions);
  glConfig.extensions_string.Clear();
  for (int index = 0; index < numExtensions; ++index) {
    const char* extension = reinterpret_cast<const char*>(
        _glewGetStringi(GL_EXTENSIONS_VALUE, index));
    glConfig.extensions_string.Append(extension);
    if (index + 1 < numExtensions) {
      glConfig.extensions_string.Append(' ');
    }
  }
  glConfig.extensions_string.Append('\0');
  const char* extensions = glConfig.extensions_string.data;

  if (idStr::Icmpn(glConfig.renderer_string, "ATI ", 4) == 0 ||
      idStr::Icmpn(glConfig.renderer_string, "AMD ", 4) == 0) {
    r_generateMipMaps.SetInteger(0, true);
    if (r_useGPUTimer.GetInteger() == 1) {
      r_useGPUTimer.SetInteger(0, true);
    }
    glConfig.vendor = VENDOR_AMD;
    idParser::AddGlobalDefine("ATI");
    const bool tessellation =
        HasExtension(extensions, "GL_ARB_tessellation_shader");
    ReportExtension("GL_ARB_tessellation_shader", tessellation);
    if (tessellation) {
      idParser::AddGlobalDefine("ATI_CLAMPED_ANISO");
    }
  } else if (idStr::Icmpn(glConfig.renderer_string, "NVIDIA", 6) == 0) {
    glConfig.vendor = VENDOR_NVIDIA;
  } else if (idStr::Icmpn(glConfig.renderer_string, "Intel", 5) == 0) {
    glConfig.vendor = VENDOR_INTEL;
  }

  glGetIntegerv(0x0D33, &glConfig.maxTextureSize);
  glGetIntegerv(0x8B4D, &glConfig.maxTextureImageUnits);
  glGetIntegerv(0x8D57, &glConfig.maxSamples);
  glGetIntegerv(0x910E, &glConfig.maxColorTextureSamples);
  glGetIntegerv(0x910F, &glConfig.maxDepthTextureSamples);
  R_PrintSeparatedWords(glConfig.extensions_string.data);
  InitNonStandardExtensions();

  auto extensionFlag = [&](const char* name) {
    const bool available = HasExtension(extensions, name);
    ReportExtension(name, available);
    return available;
  };

  glConfig.textureMultisampleAvailable =
      extensionFlag("GL_ARB_texture_multisample");
  const bool compression =
      extensionFlag("GL_ARB_texture_compression");
  glConfig.textureCompressionAvailable =
      compression && extensionFlag("GL_EXT_texture_compression_s3tc");
  glConfig.anisotropicAvailable =
      extensionFlag("GL_EXT_texture_filter_anisotropic");
  if (glConfig.anisotropicAvailable) {
    glGetIntegerv(0x84FF, &glConfig.maxTextureAnisotropy);
  } else {
    glConfig.maxTextureAnisotropy = 1;
  }
  glConfig.directStateAccess =
      extensionFlag("GL_EXT_direct_state_access") &&
      r_glUseDirectStateAccess.GetBool();
  glConfig.samplerObjectsAvailable =
      extensionFlag("GL_ARB_sampler_objects");
  glConfig.vertexBufferObjectAvailable =
      extensionFlag("GL_ARB_vertex_buffer_object");
  glConfig.mapBufferRangeAvailable =
      extensionFlag("GL_ARB_map_buffer_range");
  _glewPrimitiveRestartIndex =
      LoadExtension<glPrimitiveRestartIndexProc_t>(
          "glPrimitiveRestartIndex");
  glConfig.drawElementsBaseVertexAvailable =
      extensionFlag("GL_ARB_draw_elements_base_vertex");
  glConfig.vertexArrayObjectAvailable =
      extensionFlag("GL_ARB_vertex_array_object");
  glConfig.vertexProgramAvailable =
      extensionFlag("GL_ARB_vertex_program");
  glConfig.getProgramBinaryAvailable =
      extensionFlag("GL_ARB_get_program_binary");
  glConfig.uniformBufferAvailable =
      extensionFlag("GL_ARB_uniform_buffer_object");
  if (glConfig.uniformBufferAvailable) {
    glGetIntegerv(0x8A34, &glConfig.uniformBufferOffsetAlignment);
    glConfig.uniformBufferOffsetAlignment =
        (std::max)(glConfig.uniformBufferOffsetAlignment, 256);
  }

  _glewBindMultiTextureEXT =
      LoadExtension<glBindMultiTextureProc_t>("glBindMultiTextureEXT");
  if (_glewBindMultiTextureEXT == nullptr) {
    _glewBindMultiTextureEXT = qglBindMultiTextureEXT;
  }
  glConfig.occlusionQueryAvailable =
      extensionFlag("GL_ARB_occlusion_query");
  glConfig.timerQueryAvailable = extensionFlag("GL_EXT_timer_query");
  glConfig.framebufferObjectAvailable =
      extensionFlag("GL_EXT_framebuffer_object");
  glConfig.framebufferBlitAvailable =
      extensionFlag("GL_EXT_framebuffer_blit");
  const bool arbPixelBuffer =
      extensionFlag("GL_ARB_pixel_buffer_object");
  const bool extPixelBuffer =
      extensionFlag("GL_EXT_pixel_buffer_object");
  glConfig.pixelBufferObjectAvailable = arbPixelBuffer || extPixelBuffer;
  glConfig.depthBoundsTestAvailable =
      extensionFlag("GL_EXT_depth_bounds_test");
  glConfig.syncAvailable = extensionFlag("GL_ARB_sync");
  glConfig.debugOutputAvailable = extensionFlag("GL_ARB_debug_output");

  if (glConfig.debugOutputAvailable) {
    if (r_debugContext.GetInteger() >= 1) {
      _glewDebugMessageCallbackARB(DebugCallback, nullptr);
    }
    if (r_debugContext.GetInteger() >= 2) {
      glEnable(0x8242);
    }
    if (r_debugContext.GetInteger() >= 3) {
      _glewDebugMessageControlARB(0x1100, 0x1100, 0x1100, 0, nullptr, 1);
    }
  }
  if (_glewClampColorARB == nullptr) {
    _glewClampColorARB = NoOpClampColor;
  }

  if (!glConfig.framebufferObjectAvailable) {
    idLib::FatalError("GL_EXT_framebuffer_object not available");
  }
  if (!glConfig.framebufferBlitAvailable) {
    idLib::FatalError("GL_EXT_framebuffer_blit not available");
  }
  if (!glConfig.vertexBufferObjectAvailable) {
    idLib::FatalError("GL_ARB_vertex_buffer_object not available");
  }
  if (!glConfig.drawElementsBaseVertexAvailable) {
    idLib::FatalError("GL_ARB_draw_elements_base_vertex not available");
  }
  if (!glConfig.mapBufferRangeAvailable) {
    idLib::FatalError("GL_ARB_map_buffer_range not available");
  }
  if (!glConfig.syncAvailable) {
    idLib::FatalError("GL_ARB_sync not available");
  }
  if (!glConfig.textureCompressionAvailable) {
    idLib::FatalError("GL_ARB_texture_compression not available");
  }

  glConfig.isInitialized = true;
  int freeMemoryKilobytes = 0;
  if (glConfig.vendor == VENDOR_AMD) {
    glGetIntegerv(0x87FC, &freeMemoryKilobytes);
  } else if (glConfig.vendor == VENDOR_NVIDIA) {
    glGetIntegerv(0x9049, &freeMemoryKilobytes);
  }
  glConfig.freeVRamAtInit =
      static_cast<std::int64_t>(freeMemoryKilobytes) << 10;
  idLib::Printf("Free vram: %.2fMiB\n",
                static_cast<double>(glConfig.freeVRamAtInit) /
                    (1024.0 * 1024.0));
  GL_CheckErrors(true);
}

void idRenderSystemLocal::CreateMainThreadContext() {
  HWND hiddenContextWindow = nullptr;
  sys3D.mainThreadContext =
      CreateOpenGLContextForCurrentThread(&hiddenContextWindow, nullptr);
  if (r_glSingleContext.GetInteger() != 0) {
    sys3D.relayContext = nullptr;
  } else {
    wglMakeCurrent(nullptr, nullptr);
    sys3D.relayContext =
        AllocThreadGraphicsContextAndShare(sys3D.mainThreadContext);
    HDC dc = GetDC(hiddenContextWindow);
    wglMakeCurrent(dc, sys3D.mainThreadContext);
  }

  GL_Init();
  sys3D.useFloatBuffers = r_useFloatBuffers.GetBool();
  sys3D.numSamples =
      CreateDefaultFBO(sys3D.numSamples, sys3D.useFloatBuffers);

  wrapperContext_t context;
  context.disableDepthBoundsTest = !glConfig.depthBoundsTestAvailable;
  context.useUniformArrays = r_useUniformArrays.GetBool();
  context.useTimerQuery =
      glConfig.timerQueryAvailable && r_useGPUTimer.GetInteger() != 0;
  for (auto& queryGroup : context.queries) {
    _glewGenQueries(256, queryGroup);
  }
  GL_SetWrapperContext(context);
}

template <typename T> class idSort {
public:
  virtual ~idSort() = default;
  virtual void Sort(T* base, unsigned int num) const = 0;
};

template <typename T, typename Derived>
class idSort_Quick : public idSort<T> {
public:
  void Sort(T* base, unsigned int num) const override {
    if (num <= 1) {
      return;
    }
    constexpr std::int64_t maxLevels = 128;
    std::int64_t lo[maxLevels];
    std::int64_t hi[maxLevels];
    std::int64_t low = 0;
    std::int64_t high = num - 1;
    std::int64_t level = 0;
    for (;;) {
      std::int64_t i = low;
      std::int64_t j = high;
      if (j - i >= 4 && level < maxLevels - 1) {
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

template <typename T>
class idSort_QuickDefault final
    : public idSort_Quick<T, idSort_QuickDefault<T>> {
public:
  ~idSort_QuickDefault() override;
  int Compare(const T& lhs, const T& rhs) const {
    return lhs < rhs ? -1 : lhs > rhs ? 1 : 0;
  }
};

template <>
idSort_QuickDefault<unsigned int>::~idSort_QuickDefault() {}

void DestroyDefaultFBO() {
  if (renderDestScaledColor != nullptr) {
    renderDestScaledColor->~idRenderDestination();
    idMem::Singleton()->Free(renderDestScaledColor, ALIGN_16);
    renderDestScaledColor = nullptr;
  }
  if (renderDestScaledFeedback != nullptr) {
    renderDestScaledFeedback->~idRenderDestination();
    idMem::Singleton()->Free(renderDestScaledFeedback, ALIGN_16);
    renderDestScaledFeedback = nullptr;
  }
  if (renderDestScaledColorWithFeedback != nullptr) {
    renderDestScaledColorWithFeedback->~idRenderDestination();
    idMem::Singleton()->Free(renderDestScaledColorWithFeedback, ALIGN_16);
    renderDestScaledColorWithFeedback = nullptr;
  }
  GL_CheckErrors(false);
}

bool idRenderSystemLocal::WindowChangeMultisamples(int samples,
                                                    bool useFloatBuffers) {
  if (samples == sys3D.numSamples &&
      useFloatBuffers == sys3D.useFloatBuffers) {
    return true;
  }
  if (idLib::IsMainThread()) {
    SyncRenderThread(true);
  }
  DestroyDefaultFBO();
  const int actualSamples = CreateDefaultFBO(samples, useFloatBuffers);
  sys3D.useFloatBuffers = useFloatBuffers;
  sys3D.numSamples = actualSamples;
  return actualSamples == samples;
}

template <typename T>
T* idListArrayNew(int count, unsigned int tag, bool zeroBuffer) {
  idMem* const memory = idMem::Singleton();
  T* const allocation = static_cast<T*>(memory->AllocWithLocation(
      zeroBuffer
          ? "w:\\build\\tech5\\shared\\idlib\\containers\\List.h(27) : tag"
          : "w:\\build\\tech5\\shared\\idlib\\containers\\List.h(29) : tag",
      sizeof(T) * static_cast<std::size_t>(count),
      static_cast<memTag_t>(tag), zeroBuffer, ALIGN_16, HEAP_DEFAULTHEAP));
  if (allocation == nullptr) {
    return nullptr;
  }
  int constructed = 0;
  try {
    for (; constructed < count; ++constructed) {
      ::new (static_cast<void*>(&allocation[constructed])) T;
    }
  } catch (...) {
    while (constructed > 0) {
      allocation[--constructed].~T();
    }
    memory->Free(allocation, ALIGN_16);
    throw;
  }
  return allocation;
}

template <typename T> void idListArrayDelete(T* pointer, int count) {
  for (int index = 0; index < count; ++index) {
    pointer[index].~T();
  }
  idMem::Singleton()->Free(pointer, ALIGN_16);
}

template <typename T>
T* idListArrayResize(T* oldPointer, int oldCount, int newCount,
                     unsigned int tag, bool zeroBuffer) {
  T* replacement = nullptr;
  if (newCount > 0) {
    replacement = idListArrayNew<T>(newCount, tag, zeroBuffer);
    if (replacement == nullptr) {
      return nullptr;
    }
    const int copyCount = (std::min)(oldCount, newCount);
    for (int index = 0; index < copyCount; ++index) {
      replacement[index] = oldPointer[index];
    }
  }
  idListArrayDelete(oldPointer, oldCount);
  return replacement;
}

template monitor_t* idListArrayNew<monitor_t>(int, unsigned int, bool);
template void idListArrayDelete<monitor_t>(monitor_t*, int);
template monitor_t* idListArrayResize<monitor_t>(monitor_t*, int, int,
                                                  unsigned int, bool);

template <>
bool idList<monitor_t, 62>::Resize(int newSize) {
  if (newSize <= 0) {
    if (listStatic == 0 || listStatic == 2) {
      idListArrayDelete(list, size);
      list = nullptr;
      size = 0;
      num = 0;
      return true;
    }
    num = 0;
    return true;
  }
  if (newSize == size || listStatic != 0) {
    return true;
  }
  monitor_t* replacement =
      idListArrayResize(list, size, newSize, memTag, false);
  if (replacement == nullptr) {
    return false;
  }
  list = replacement;
  size = newSize;
  if (num > newSize) {
    num = newSize;
  }
  return true;
}

template <> monitor_t* idList<monitor_t, 62>::Alloc() {
  const int required = num + 1;
  if (required > size) {
    int newSize = 1;
    if (forceIdListsToReallocateEveryAppend) {
      newSize = required;
    } else if (granularity != 0) {
      newSize = required + granularity - 1;
      newSize -= newSize % granularity;
    } else {
      while (newSize < required) {
        newSize *= 2;
      }
    }
    Resize(newSize);
  }
  if (num >= size) {
    return &list[size - 1];
  }
  return &list[num++];
}

sys3D_t::sys3D_t() {
  vidMode.width = 1;
  vidMode.height = 1;
  vidMode.pixelAspect = 1.0F;
  vidMode.fullscreen = true;
  vidMode.monitorNum = 0;
  vidMode.x = 0;
  vidMode.y = 0;
  win32DC = nullptr;
  currentFullscreen = false;
  currentMonitor = -1;
  numSamples = 1;
  lastDC = nullptr;
  renderThreadContext = nullptr;
  mainThreadContext = nullptr;
  pixelFormat = 0;
  ignoreSizeMessages = true;
}

BOOL CALLBACK MonitorEnumProc(HMONITOR monitor, HDC, RECT*, LPARAM) {
  MONITORINFOEXA info{};
  info.cbSize = sizeof(info);
  GetMonitorInfoA(monitor, &info);
  monitor_t* entry = sys3D.monitors.Alloc();
  entry->hMonitor = monitor;
  return TRUE;
}

void EnumerateMonitors() {
  sys3D.monitors.ClearFree();
  EnumDisplayMonitors(nullptr, nullptr, MonitorEnumProc, 0);

  for (int monitorIndex = 0; monitorIndex < sys3D.monitors.Num();
       ++monitorIndex) {
    monitor_t& monitor = sys3D.monitors[monitorIndex];
    MONITORINFOEXA info{};
    info.cbSize = sizeof(info);
    GetMonitorInfoA(monitor.hMonitor, &info);
    monitor.width = info.rcMonitor.right - info.rcMonitor.left;
    monitor.height = info.rcMonitor.bottom - info.rcMonitor.top;

    DEVMODEA mode{};
    mode.dmSize = sizeof(mode);
    for (DWORD modeIndex = 0;
         EnumDisplaySettingsExA(info.szDevice, modeIndex, &mode, 0);
         ++modeIndex) {
      if (mode.dmBitsPerPel != 32 || mode.dmPelsHeight < 480) {
        continue;
      }
      const unsigned packed =
          (static_cast<unsigned>(mode.dmPelsWidth) << 16U) |
          (static_cast<unsigned>(mode.dmPelsHeight) & 0xFFFFU);
      bool duplicate = false;
      for (int index = 0; index < monitor.supportedModes.Num(); ++index) {
        if (monitor.supportedModes[index] == packed) {
          duplicate = true;
          break;
        }
      }
      if (!duplicate) {
        monitor.supportedModes.Append(packed);
      }
    }
    idSort_QuickDefault<unsigned int> sorter;
    if (monitor.supportedModes.list != nullptr) {
      sorter.Sort(monitor.supportedModes.list,
                  monitor.supportedModes.Num());
    }
    if (monitor.supportedModes.Num() == 0) {
      sys3D.monitors.RemoveIndex(monitorIndex);
    }
  }

  if (sys3D.monitors.Num() == 0) {
    idLib::FatalError("No valid monitors found.");
  }
}

void GLW_WM_DISPLAYCHANGE() {
  if (!insideDisplayChange) {
    insideDisplayChange = true;
    EnumerateMonitors();
    renderSystem->Restart();
    insideDisplayChange = false;
  }
}

void idRenderSystemLocal::PreliminaryRenderSetup() {
  idLib::Printf("PreliminaryRenderSetup\n");
  const HWND desktop = GetDesktopWindow();
  HDC dc = GetDC(desktop);
  if (GetDeviceCaps(dc, BITSPIXEL) < 32) {
    idLib::FatalError(
        "Desktop color quality must be set to 32 bit or higher");
  }

  EnumerateMonitors();
  SetupVidMode();
  HDC gammaDC = GetDC(GetDesktopWindow());
  GetDeviceGammaRamp(gammaDC, sys3D.oldHardwareGamma);
  ReleaseDC(GetDesktopWindow(), gammaDC);
  sys3D.numSamples = r_multiSamples.GetInteger();
  CreateWindowClasses(win32.hInstance);
  GetWGLExtensionsWithFakeWindow();

  const int attributes[] = {
      0x2001, TRUE,
      0x2010, TRUE,
      0x2011, TRUE,
      0x2013, 0x202B,
      0x2014, 32,
      0x2022, 24,
      0x2023, 8,
      0,
  };
  unsigned count = 0;
  wglChoosePixelFormatARB(dc, attributes, nullptr, 1, &sys3D.pixelFormat,
                          &count);
  if (count == 0) {
    idLib::FatalError("wglChoosePixelFormatARB failed.");
  }
  DescribePixelFormat(dc, sys3D.pixelFormat, sizeof(sys3D.pfd), &sys3D.pfd);
  ReleaseDC(GetDesktopWindow(), dc);
  CreateMainThreadContext();
}
