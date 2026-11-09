#include "engine/renderer/rendersystem_commands.h"

#include "engine/decls/declenv.h"
#include "engine/framework/coderesource.h"
#include "engine/framework/common.h"
#include "engine/framework/console.h"
#include "engine/models/rendermodel.h"
#include "engine/models/renderworld.h"
#include "engine/renderer/autorenderparms.h"
#include "engine/renderer/declrenderparm.h"
#include "engine/renderer/declrenderprog.h"
#include "engine/renderer/gl_api.h"
#include "engine/renderer/image.h"
#include "engine/renderer/imagemanager.h"
#include "engine/renderer/imagetools/image_files.h"
#include "engine/renderer/jobs/render/parmstate.h"
#include "engine/renderer/renderlocal.h"
#include "shared/idlib/csystems/autocomplete.h"
#include "shared/idlib/csystems/cmdsystemlocal.h"
#include "shared/idlib/csystems/cvarsystemlocal.h"
#include "shared/idlib/containers/heaparray.h"
#include "shared/idlib/filesystem/filesystemlocal.h"
#include "shared/idlib/lib.h"
#include "shared/idlib/sys/sys_alloc.h"
#include "shared/idlib/sys/sys_time.h"
#include "shared/idlib/text/cmdargs.h"

#include <algorithm>
#include <array>
#include <cmath>
#include <cstdint>
#include <cstdlib>
#include <cstring>

idCodeResource<idDeclRenderParm> cbShowOverlay("showOverlay");

namespace {
int lastRawScreenshotNumber = 0;
int lastScreenshotNumber = 0;

#if defined(WOLF_RENDERER_RENDERSYSTEM_COMMANDS_TEST_ACCESS)
RenderSystemCommandsTestHooks testHooks{};
#endif

const char* ScreenshotExtension(screenshotFormat_t format) {
  switch (format) {
  case SSFMT_TGA:
    return "tga";
  case SSFMT_PNG:
    return "png";
  case SSFMT_JPG:
  default:
    return "jpg";
  }
}

void WriteScreenshot(const char* fileName, const std::uint8_t* pixels,
                     int width, int height, fsPath_t path,
                     screenshotFormat_t format) {
#if defined(WOLF_RENDERER_RENDERSYSTEM_COMMANDS_TEST_ACCESS)
  if (testHooks.writeScreenshot != nullptr) {
    testHooks.writeScreenshot(fileName, pixels, width, height, path, format);
    return;
  }
#endif
  switch (format) {
  case SSFMT_TGA:
    R_WriteTGA(fileName, pixels, width, height, 4, true, true, path);
    break;
  case SSFMT_JPG:
    R_WriteJPG(fileName, pixels, width, height, path, true);
    break;
  case SSFMT_PNG:
    R_WritePNG(fileName, const_cast<std::uint8_t*>(pixels),
               static_cast<unsigned int>(width),
               static_cast<unsigned int>(height), true, path);
    break;
  }
}

idDeclRenderProg* FindRenderProg(const char* name, bool makeDefault,
                                 bool remap) {
#if defined(WOLF_RENDERER_RENDERSYSTEM_COMMANDS_TEST_ACCESS)
  if (testHooks.findRenderProg != nullptr)
    return testHooks.findRenderProg(name, makeDefault, remap);
#endif
  return static_cast<idDeclRenderProg*>(
      idDeclRenderProg::resourceList.FindWithInheritance(name, makeDefault,
                                                         remap));
}

idDeclRenderParm* FindRenderParm(const char* name, bool makeDefault,
                                 bool remap) {
#if defined(WOLF_RENDERER_RENDERSYSTEM_COMMANDS_TEST_ACCESS)
  if (testHooks.findRenderParm != nullptr)
    return testHooks.findRenderParm(name, makeDefault, remap);
#endif
  return static_cast<idDeclRenderParm*>(
      idDeclRenderParm::resourceList.FindWithInheritance(name, makeDefault,
                                                         remap));
}

const idDeclEnv* FindEnvironment(const char* name, bool makeDefault,
                                 bool remap) {
#if defined(WOLF_RENDERER_RENDERSYSTEM_COMMANDS_TEST_ACCESS)
  if (testHooks.findEnvironment != nullptr)
    return testHooks.findEnvironment(name, makeDefault, remap);
#endif
  return static_cast<const idDeclEnv*>(
      idDeclEnv::resourceList.FindWithInheritance(name, makeDefault, remap));
}

idImage* FindImage(const char* name) {
#if defined(WOLF_RENDERER_RENDERSYSTEM_COMMANDS_TEST_ACCESS)
  if (testHooks.imageFromFile != nullptr)
    return testHooks.imageFromFile(name);
#endif
  return globalImages->ImageFromFile(name, nullptr, false);
}

void RunScreenshotEndFrame(idRenderSystemLocal* local, void* window,
                           int width, int height, idRenderWorld* world,
                           idRenderModel** guiList, int numGuis,
                           int renderTime) {
#if defined(WOLF_RENDERER_RENDERSYSTEM_COMMANDS_TEST_ACCESS)
  if (testHooks.endFrame != nullptr) {
    testHooks.endFrame(local, window, width, height, world, guiList, numGuis,
                       false, false, false, renderTime);
    return;
  }
#endif
  local->EndFrame(window, width, height, world, guiList, numGuis, false,
                  false, false, renderTime);
}

void PrintScreenshotPath(const idStr& path, fsPath_t basePath) {
  char osPath[256]{};
  fileSystem->RelativePathToOSPath(path.data, osPath, sizeof(osPath),
                                   basePath);
  idLib::Printf("Wrote %s\n", osPath);
}
} // namespace

#if defined(WOLF_RENDERER_RENDERSYSTEM_COMMANDS_TEST_ACCESS)
void RenderSystemCommandsTestAccess::SetHooks(
    const RenderSystemCommandsTestHooks& hooks) {
  testHooks = hooks;
}

void RenderSystemCommandsTestAccess::ResetScreenshotNumbers() {
  lastRawScreenshotNumber = 0;
  lastScreenshotNumber = 0;
}
#endif

template <typename T>
void idDeclManager::ArgCompletion_Decl(idAutoComplete& completion) {
  declManager->ArgCompletion_DeclName(&completion, &T::resourceList);
}
template void
idDeclManager::ArgCompletion_Decl<idDeclRenderProg>(idAutoComplete&);
template void
idDeclManager::ArgCompletion_Decl<idDeclRenderParm>(idAutoComplete&);

bool idRenderSystemLocal::CaptureCurrentRenderTargetForScreenShot(
    int width, int height, bool fillAlphaWith255, void* resultBuffer) {
  if (width > GetWidth() || height > GetHeight() || resultBuffer == nullptr ||
      width == 0 || height == 0) {
    return false;
  }

  auto* pixels = static_cast<std::uint8_t*>(resultBuffer);
  glReadPixels(0, 0, width, height, 0x1908u, 0x1401u, pixels);
  if (fillAlphaWith255) {
    for (int y = 0; y < height; ++y) {
      for (int x = 0; x < width; ++x)
        pixels[(y * width + x) * 4 + 3] = 255;
    }
  }
  return true;
}

void idRenderSystemLocal::ArgCompletion_ImageName(
    idAutoComplete& completion) {
  for (int index = 0; index < idImage::resourceList.resourceList.Num();
       ++index) {
    idResource* const resource = idImage::resourceList.Index(index);
    const char* const name = resource->GetName();
    if (name[0] == '_') {
      const idStr suggestion(name);
      completion.Append(suggestion, -1);
    }
  }
  cmdSystem->ArgCompletion_FolderExtension(&completion, "/", "tga", true);
}

void R_TestImage_f(const idCmdArgs& inputArgs) {
  int colorMode = 0;
  float lod = 0.0F;
  bool flipX = false;
  bool flipY = false;
  idCmdArgs args;

  tr.testImage = nullptr;
  tr.testMaterial = nullptr;

  for (int index = 0; index < inputArgs.argc; ++index) {
    const char* const argument = inputArgs.Argv(index);
    if (idStr::Icmp(argument, "-flipX") == 0) {
      flipX = true;
    } else if (idStr::Icmp(argument, "-flipY") == 0) {
      flipY = true;
    } else if (idStr::Icmp(argument, "-lod") == 0 ||
               idStr::Icmp(argument, "-l") == 0) {
      lod = static_cast<float>(std::atof(inputArgs.Argv(++index)));
    } else if (idStr::Icmp(argument, "-YCoCg") == 0 ||
               idStr::Icmpn(argument, "-y", 2) == 0) {
      colorMode = 1;
    } else if (idStr::Icmp(argument, "-alpha") == 0 ||
               idStr::Icmp(argument, "-a") == 0) {
      colorMode = 2;
    } else {
      args.AppendArg(argument);
    }
  }

  if (args.argc == 1)
    return;
  if (args.argc != 2 && args.argc != 3) {
    idLib::Printf(
        "usage: testImage [-YCoCg|-y] [-alpha|-a] [-flipX] [-flipY] [-lod|-l val] image\n");
    return;
  }

  FindRenderProg("cubeImageOnly", true, true);
  FindRenderProg("testImage", true, true);
  const char* const imageName = args.Argv(1);
  if (idStr::IsNumeric(imageName)) {
    const int imageIndex = static_cast<int>(std::atol(imageName));
    if (imageIndex >= 0 &&
        imageIndex < idImage::resourceList.resourceList.Num()) {
      tr.testImage = static_cast<idImage*>(
          idImage::resourceList.Index(imageIndex));
    }
  } else {
    tr.testImage = FindImage(imageName);
    if (tr.testImage == nullptr)
      idLib::Printf("Couldn't load '%s'\n", imageName);
  }

  if (args.argc == 3)
    lod = static_cast<float>(std::atof(args.Argv(2)));

  const auto setLod = [lod, colorMode](const char* name) {
    const auto* parm = FindRenderParm(name, true, true);
    if (parm != nullptr) {
      renderThreadParmState->SetParmVector(
          parm, lod, static_cast<float>(colorMode), 0.0F, 0.0F);
    }
  };
  setLod("testImageLOD");
  setLod("testEnvLOD");

  const auto* flipParm = FindRenderParm("testImageFlip", true, true);
  if (flipParm != nullptr) {
    const float flip[4]{flipX ? -1.0F : 1.0F, flipY ? -1.0F : 1.0F, 1.0F,
                        1.0F};
    renderThreadParmState->SetParmVector(flipParm, flip);
  }
}

void R_RenameRenderProg_f(const idCmdArgs& args) {
  if (args.argc < 2 || args.argc > 3) {
    idLib::Printf("USAGE: renameRenderProg <renderProg> [newProg]\n");
    return;
  }

  auto* const program = FindRenderProg(args.Argv(1), false, true);
  if (program == nullptr) {
    idLib::Printf("Renderprog '%s' not found\n", args.Argv(1));
    return;
  }

  if (args.argc == 2) {
    if (program->renamed == program) {
      idLib::Printf("Renderprog '%s' was not renamed.\n", args.Argv(1));
    } else {
      program->renamed = program;
      idLib::Printf("Renderprog '%s' was returned to normal.\n",
                    args.Argv(1));
    }
    return;
  }

  const auto* const replacement = FindRenderProg(args.Argv(2), false, true);
  if (replacement == nullptr) {
    idLib::Printf("Renderprog '%s' not found\n", args.Argv(2));
    return;
  }
  program->renamed = replacement;
  idLib::Printf("Renderprog '%s' was renamed to '%s'.\n", args.Argv(1),
                args.Argv(2));
}

void R_RP_f(const idCmdArgs& args) {
  if (args.argc < 2 || args.argc > 6) {
    idLib::Printf("USAGE: rp <renderParmName> [value]\n");
    return;
  }

  auto* const parm = FindRenderParm(args.Argv(1), false, true);
  if (parm == nullptr) {
    idLib::Printf("RenderParm '%s' not found\n", args.Argv(1));
    return;
  }
  if (args.argc == 2) {
    parm->Print();
    return;
  }
  if (parm->parmType != PT_VECTOR) {
    parmValue_t value{};
    parm->ParseStringToValue(args.Args(2, -1, false, false), value);
    renderThreadParmState->SetParmValue(parm, value);
    return;
  }

  idVec4 value{parm->declaredValue.value[0], parm->declaredValue.value[1],
               parm->declaredValue.value[2], parm->declaredValue.value[3]};
  const float x = static_cast<float>(std::atof(args.Argv(2)));
  value.x = x;
  if (args.argc == 3) {
    value.y = x;
    value.z = x;
    value.w = x;
  } else {
    value.y = static_cast<float>(std::atof(args.Argv(3)));
    if (args.argc > 4)
      value.z = static_cast<float>(std::atof(args.Argv(4)));
    if (args.argc > 5)
      value.w = static_cast<float>(std::atof(args.Argv(5)));
  }
  renderThreadParmState->SetParmVector(parm, &value.x);
}

void idRenderSystemLocal::TakeRawScreenshot(
    const char* fileName, fsPath_t path, screenshotFormat_t format,
    bool fillAlphaWith255) {
  const int width = GetWidth();
  const int height = GetHeight();
  idHeapArray<std::uint8_t> pixels;
  pixels.Alloc(static_cast<unsigned int>(4 * width * height));
  CaptureCurrentRenderTargetForScreenShot(width, height, fillAlphaWith255,
                                          pixels.buffer);
  WriteScreenshot(fileName, pixels.buffer, width, height, path, format);
}

void idRenderSystemLocal::TakeScreenshotToBuffer(
    int width, int height, idRenderWorld* world, idRenderModel** guiList,
    int numGuis, int blends, unsigned char* outputBuffer, int primes,
    bool fillAlphaWith255, bool skipPostProcessing) {
  const bool oldJitter = r_jitter.GetBool();
  if (blends > 1)
    r_jitter.SetBool(true, true);
  const int oldSkipPostProcess = r_skipPostProcess.GetInteger();
  r_skipPostProcess.SetInteger(skipPostProcessing ? 1 : 0, true);

  const int pixelCount = width * height;
  const int captureWidth = width & ~3;
  blends = blends > 1 ? blends : 1;
  idHeapArray<std::uint16_t> accumulated;
  if (blends > 1) {
    accumulated.Alloc(static_cast<unsigned int>(4 * pixelCount));
    std::memset(accumulated.buffer, 0,
                static_cast<std::size_t>(2) * accumulated.num);
  }

  const int renderTime = world != nullptr
                             ? world->RenderViewForIndex(0)->r.renderTime
                             : 0;
  {
    idScopeCVar dofMode(&r_postProcessDofMode, 5);
    const int primeStart = static_cast<int>(Sys_Milliseconds());
    for (int prime = 0; prime < primes; ++prime) {
      RunScreenshotEndFrame(this, renderSystem->GetGameWindow(), captureWidth,
                            height, world, guiList, numGuis, renderTime);
    }
    idLib::Printf(
        "%.2f seconds to endFrame for primes.\n",
        static_cast<float>(static_cast<int>(Sys_Milliseconds()) - primeStart) *
            0.001F);
  }

  const int blendStart = static_cast<int>(Sys_Milliseconds());
  for (int blend = 0; blend < blends; ++blend) {
    RunScreenshotEndFrame(this, renderSystem->GetGameWindow(), captureWidth,
                          height, world, guiList, numGuis, renderTime);
    CaptureCurrentRenderTargetForScreenShot(captureWidth, height, false,
                                            outputBuffer);
    if (blends > 1) {
      for (int index = 0; index < 4 * pixelCount; ++index)
        accumulated.buffer[index] += outputBuffer[index];
    }
  }
  idLib::Printf("%.2f seconds to endFrame for blends.\n",
                static_cast<float>(static_cast<int>(Sys_Milliseconds()) -
                                   blendStart) *
                    0.001F);

  if (blends > 1) {
    for (int index = 0; index < 4 * pixelCount; ++index) {
      outputBuffer[index] =
          static_cast<std::uint8_t>(accumulated.buffer[index] / blends);
    }
  }
  if (fillAlphaWith255) {
    for (int y = 0; y < height; ++y) {
      for (int x = 0; x < captureWidth; ++x)
        outputBuffer[(y * captureWidth + x) * 4 + 3] = 255;
    }
  }
  r_jitter.SetBool(oldJitter, true);
  r_skipPostProcess.SetInteger(oldSkipPostProcess, true);
}

void R_RawScreenShot_f(const idCmdArgs& args) {
  screenshotFormat_t format = SSFMT_PNG;
  bool fillAlphaWith255 = true;
  idStr fileName;
  idStr shotName("screenshots/");

  for (int index = 1; index < args.argc; ++index) {
    const char* const argument = args.Argv(index);
    if (idStr::Icmp(argument, "tga") == 0) {
      format = SSFMT_TGA;
    } else if (idStr::Icmp(argument, "png") == 0) {
      format = SSFMT_PNG;
    } else if (idStr::Icmp(argument, "alpha") == 0) {
      fillAlphaWith255 = false;
    } else if (index >= args.argc - 1) {
      fileName = argument;
    } else {
      idLib::Printf(
          "usage: rawscreenshot [noGuis] [loadTextures] [blends <num>] [tga] [filename]\n");
      return;
    }
  }

  shotName += com_capturePath.GetString();
  idStr outputPath;
  if (!fileName.IsEmpty()) {
    if (fileName.Find(".tga", false) >= 0) {
      format = SSFMT_TGA;
    } else if (fileName.Find(".png", false) >= 0) {
      format = SSFMT_PNG;
    } else {
      fileName.SetFileExtension("jpg");
    }
    if (fileName.Find("/", false) >= 0)
      shotName.Clear();
    outputPath = shotName + fileName;
  } else {
    shotName.AppendPath("rawshot");
    char numberedBase[256]{};
    fileSystem->RelativePathToOSPath(shotName.data, numberedBase,
                                     sizeof(numberedBase), FSPATH_SAVE);
    fileSystem->GetNumberedFilename(&lastRawScreenshotNumber, numberedBase,
                                    ScreenshotExtension(format), &outputPath,
                                    true);
  }
  tr.TakeRawScreenshot(outputPath.data, FSPATH_SAVE, format,
                       fillAlphaWith255);
  PrintScreenshotPath(outputPath, FSPATH_SAVE);
}

void idRenderSystemLocal::TakeScreenshot(
    int width, int height, idRenderWorld* world, idRenderModel** guiList,
    int numGuis, const char* fileName, int blends, fsPath_t path,
    screenshotFormat_t format, int primes, bool fillAlphaWith255,
    bool skipPostProcessing) {
  const int captureWidth = width & ~3;
  idHeapArray<std::uint8_t> pixels;
  pixels.Alloc(static_cast<unsigned int>(4 * height * captureWidth));
  TakeScreenshotToBuffer(captureWidth, height, world, guiList, numGuis, blends,
                         pixels.buffer, primes, fillAlphaWith255,
                         skipPostProcessing);
  WriteScreenshot(fileName, pixels.buffer, captureWidth, height, path, format);
}

void R_ScreenShot_f(const idCmdArgs& args) {
  bool loadTextures = false;
  bool drawConsole = false;
  int blends = com_captureSamples.GetInteger();
  const int primes = com_capturePrimes.GetInteger();
  screenshotFormat_t format = SSFMT_JPG;
  idStr fileName;
  idStr shotName("screenshots/");

  for (int index = 1; index < args.argc; ++index) {
    const char* const argument = args.Argv(index);
    if (idStr::Icmp(argument, "tga") == 0) {
      format = SSFMT_TGA;
    } else if (idStr::Icmp(argument, "png") == 0) {
      format = SSFMT_PNG;
    } else if (idStr::Icmp(argument, "loadTextures") == 0) {
      loadTextures = true;
    } else if (idStr::Icmp(argument, "noGuis") == 0) {
      continue;
    } else if (idStr::Icmp(argument, "drawConsole") == 0) {
      drawConsole = true;
    } else if (idStr::Icmp(argument, "blends") == 0) {
      blends = static_cast<int>(std::atol(args.Argv(++index)));
    } else if (index >= args.argc - 1) {
      fileName = argument;
    } else {
      idLib::Printf(
          "usage: screenshot [noGuis] [loadTextures] [blends <num>] [tga] [filename]\n");
      return;
    }
  }

  shotName += com_capturePath.GetString();
  idStr outputPath;
  if (!fileName.IsEmpty()) {
    if (fileName.Find(".tga", false) >= 0) {
      format = SSFMT_TGA;
    } else if (fileName.Find(".png", false) >= 0) {
      format = SSFMT_PNG;
    } else {
      fileName.SetFileExtension("jpg");
    }
    if (fileName.Find("/", false) >= 0)
      shotName.Clear();
    outputPath = shotName + fileName;
  } else if (com_captureFilename.valueString.IsEmpty()) {
    shotName.AppendPath("shot");
    char numberedBase[256]{};
    fileSystem->RelativePathToOSPath(shotName.data, numberedBase,
                                     sizeof(numberedBase), FSPATH_SAVE);
    fileSystem->GetNumberedFilename(&lastScreenshotNumber, numberedBase,
                                    ScreenshotExtension(format), &outputPath,
                                    true);
  } else {
    fileName = com_captureFilename.GetString();
    fileName.SetFileExtension(ScreenshotExtension(format));
    outputPath = shotName + fileName;
  }

  const bool isViewNote = outputPath.Find("viewnote", false) >= 0;
  if (com_captureFrames.GetInteger() == 0) {
    if (!drawConsole)
      console->Close(!isViewNote);
    common->UpdateConsoleDisplay(true);
    if (isViewNote || drawConsole)
      console->Draw(false, false);
  }

  if ((loadTextures || isViewNote) && tr.primaryWorld != nullptr)
    tr.primaryWorld->RenderViewForIndex(0)->g.forceFullVirtualTextureLoad =
        true;

  tr.TakeScreenshot(tr.GetWidth(), tr.GetHeight(), tr.primaryWorld, nullptr, 0,
                    outputPath.data, blends, FSPATH_SAVE, format, primes, true,
                    r_skipPostProcess.GetBool());

  if ((loadTextures || isViewNote) && tr.primaryWorld != nullptr)
    tr.primaryWorld->RenderViewForIndex(0)->g.forceFullVirtualTextureLoad =
        false;
  PrintScreenshotPath(outputPath, FSPATH_SAVE);
}

void R_EnvShot_f(const idCmdArgs& args) {
  if (args.argc < 2 || args.argc > 5) {
    idLib::Printf("USAGE: envshot <basename> [size] [blends] [envDecl]\n");
    return;
  }
  idRenderWorld* const world = tr.primaryWorld;
  if (world == nullptr)
    return;

  for (int index = 0; index < world->GetNumRenderModels(); ++index) {
    idRenderModel* const model = world->RenderModelForIndex(index);
    if (model != nullptr &&
        (model->g.allowSurfaceOnlyInViewID != 0 ||
         model->g.suppressSurfaceInViewID != 0 ||
         model->g.suppressShadowInViewID != 0 ||
         model->g.suppressShadowInLightIndex != -1) &&
        (model->unlinked & 8u) == 0) {
      model->CommitThisFrame();
      model->UnlinkSubclass();
      model->unlinked |= 8u;
    }
  }

  const int height = args.argc > 2 ? static_cast<int>(std::atol(args.Argv(2)))
                                   : 128;
  const int blends = args.argc > 3 ? static_cast<int>(std::atol(args.Argv(3)))
                                   : 16;
  const idDeclEnv* environment = nullptr;
  if (args.argc > 4) {
    environment = FindEnvironment(args.Argv(4), false, true);
  }

  std::array<idMat3, 6> axes{};
  axes[0][0] = {1.0F, 0.0F, 0.0F};
  axes[0][1] = {0.0F, 0.0F, 1.0F};
  axes[0][2] = {0.0F, 1.0F, 0.0F};
  axes[1][0] = {-1.0F, 0.0F, 0.0F};
  axes[1][1] = {0.0F, 0.0F, -1.0F};
  axes[1][2] = {0.0F, 1.0F, 0.0F};
  axes[2][0] = {0.0F, 1.0F, 0.0F};
  axes[2][1] = {-1.0F, 0.0F, 0.0F};
  axes[2][2] = {0.0F, 0.0F, -1.0F};
  axes[3][0] = {0.0F, -1.0F, 0.0F};
  axes[3][1] = {-1.0F, 0.0F, 0.0F};
  axes[3][2] = {0.0F, 0.0F, 1.0F};
  axes[4][0] = {0.0F, 0.0F, 1.0F};
  axes[4][1] = {-1.0F, 0.0F, 0.0F};
  axes[4][2] = {0.0F, 1.0F, 0.0F};
  axes[5][0] = {0.0F, 0.0F, -1.0F};
  axes[5][1] = {1.0F, 0.0F, 0.0F};
  axes[5][2] = {0.0F, 1.0F, 0.0F};
  const char* const extensions[6]{"_px.tga", "_nx.tga", "_py.tga",
                                  "_ny.tga", "_pz.tga", "_nz.tga"};

  idRenderView* const view = world->RenderViewForIndex(0);
  view->g.fov_x = 90.0F;
  view->g.fov_y = 90.0F;
  idStaticParmBlock<80> previousOverride;
  previousOverride.CopyFrom(view->g.envOverrideParmBlock);
  const bool oldForceFullLoad = view->g.forceFullVirtualTextureLoad;
  view->g.forceFullVirtualTextureLoad = true;

  if (environment != nullptr) {
    view->g.envOverrideParmBlock.Clear();
    view->g.envOverrideParmBlock.CopyFrom(environment->parmBlock);
    const idDeclRenderParm* const ignoredParms[]{
        rp_fogColor.Get(), rp_fogScale.Get(), rp_fogStart.Get(),
        rp_fogEnd.Get(), rp_fogSkyAmount.Get(), nullptr};
    for (const idDeclRenderParm* const parm : ignoredParms) {
      if (parm != nullptr && view->g.envOverrideParmBlock.SetsRenderParm(parm))
        view->g.envOverrideParmBlock.ClearParm(parm);
    }
  } else {
    parmValue_t showOverlay{};
    view->g.envOverrideParmBlock.SetParm(cbShowOverlay.Get(), showOverlay);
  }

  idStr outputName;
  const int width = height & ~3;
  for (int face = 0; face < 6; ++face) {
    outputName = "env/";
    outputName += args.Argv(1);
    outputName += extensions[face];
    view->g.viewaxis = axes[face];
    view->g.useExplicitProjectionMatrix = false;
    idHeapArray<std::uint8_t> pixels;
    pixels.Alloc(static_cast<unsigned int>(4 * height * width));
    tr.TakeScreenshotToBuffer(width, height, world, nullptr, 0, blends,
                              pixels.buffer, 0, true,
                              r_skipPostProcess.GetBool());
    WriteScreenshot(outputName.data, pixels.buffer, width, height, FSPATH_BASE,
                    SSFMT_TGA);
  }

  view->g.forceFullVirtualTextureLoad = oldForceFullLoad;
  view->g.envOverrideParmBlock.Clear();
  view->g.envOverrideParmBlock.CopyFrom(previousOverride);
  idLib::Printf("Wrote %s, etc\n", outputName.data);
}

void idRenderSystemLocal::InitCommands() {
  cmdSystem->AddCommand("screenshot",
                        reinterpret_cast<commandFunction_t>(R_ScreenShot_f),
                        "takes a screenshot", nullptr);
  cmdSystem->AddCommand(
      "rawscreenshot",
      reinterpret_cast<commandFunction_t>(R_RawScreenShot_f),
      "takes a raw frame buffer dump as screenshot", nullptr);
  cmdSystem->AddCommand("envshot",
                        reinterpret_cast<commandFunction_t>(R_EnvShot_f),
                        "takes an environment shot", nullptr);
  cmdSystem->AddCommand(
      "testImage", reinterpret_cast<commandFunction_t>(R_TestImage_f),
      "displays the given image centered on screen",
      reinterpret_cast<argCompletion_t>(ArgCompletion_ImageName));
  cmdSystem->AddCommand(
      "renameRenderProg",
      reinterpret_cast<commandFunction_t>(R_RenameRenderProg_f),
      "temporarily replace a renderProg with a different one",
      reinterpret_cast<argCompletion_t>(
          idDeclManager::ArgCompletion_Decl<idDeclRenderProg>));
  cmdSystem->AddCommand(
      "rp", reinterpret_cast<commandFunction_t>(R_RP_f),
      "Displays or modifies a renderparm",
      reinterpret_cast<argCompletion_t>(
          idDeclManager::ArgCompletion_Decl<idDeclRenderParm>));
}
