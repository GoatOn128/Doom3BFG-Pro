#pragma once

#include "shared/idlib/filesystem/file.h"

class idCmdArgs;
class idDeclRenderParm;
class idDeclRenderProg;
class idDeclEnv;
class idImage;
class idRenderModel;
class idRenderWorld;
class idAutoComplete;
template <typename T> class idCodeResource;

enum screenshotFormat_t : int {
  SSFMT_TGA = 0,
  SSFMT_JPG = 1,
  SSFMT_PNG = 2,
};

void R_TestImage_f(const idCmdArgs& args);
void R_RenameRenderProg_f(const idCmdArgs& args);
void R_RP_f(const idCmdArgs& args);
void R_RawScreenShot_f(const idCmdArgs& args);
void R_ScreenShot_f(const idCmdArgs& args);
void R_EnvShot_f(const idCmdArgs& args);

extern idCodeResource<idDeclRenderParm> cbShowOverlay;

extern class idCVar com_captureSamples;
extern class idCVar com_capturePrimes;
extern class idCVar com_capturePath;
extern class idCVar com_captureFilename;
extern class idCVar com_captureFrames;
extern class idCVar r_jitter;
extern class idCVar r_skipPostProcess;
extern class idCVar r_postProcessDofMode;

#if defined(WOLF_RENDERER_RENDERSYSTEM_COMMANDS_TEST_ACCESS)
struct RenderSystemCommandsTestHooks {
  void (*writeScreenshot)(const char* fileName,
                          const unsigned char* pixels, int width, int height,
                          fsPath_t path, screenshotFormat_t format){};
  void (*endFrame)(class idRenderSystemLocal* renderSystem, void* window,
                   int width, int height, idRenderWorld* world,
                   idRenderModel** guiList, int numGuis, bool, bool, bool,
                   int renderTime){};
  idImage* (*imageFromFile)(const char* name){};
  idDeclRenderProg* (*findRenderProg)(const char* name, bool makeDefault,
                                     bool remap){};
  idDeclRenderParm* (*findRenderParm)(const char* name, bool makeDefault,
                                     bool remap){};
  const idDeclEnv* (*findEnvironment)(const char* name, bool makeDefault,
                                     bool remap){};
};

class RenderSystemCommandsTestAccess {
public:
  static void SetHooks(const RenderSystemCommandsTestHooks& hooks);
  static void ResetScreenshotNumbers();
};
#endif
