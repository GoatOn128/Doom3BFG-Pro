#include "engine/renderer/globalshadows.h"

#include "engine/framework/fileresource.h"
#include "shared/idlib/csystems/cvarsystemlocal.h"
#include "shared/idlib/filesystem/file.h"
#include "shared/idlib/lib.h"
#include "shared/idlib/sys/sys_alloc.h"
#include "shared/idlib/text/str.h"

#include <cstring>

idCVar r_globalShadowsCoordinateBias(
    "r_globalShadowsCoordinateBias", "0.5", CVAR_FLOAT,
    "Bias to the shadow check coordinate calculation");
idCVar r_globalShadowsDepthBias(
    "r_globalShadowsDepthBias", "0", CVAR_FLOAT,
    "Bias to the shadow check depth calculation");
idCVar r_useGlobalShadows(
    "r_useGlobalShadows", "1", CVAR_BOOL,
    "If false, always return 1.0 for shadow tests");

idGlobalShadows::idGlobalShadows() {
  shadowMap = static_cast<shadowMapCPU_t*>(
      idMem::Singleton()->AllocWithLocation(
          "w:\\build\\tech5\\shared\\idlib\\Heap.h(95) : tag",
          sizeof(shadowMapCPU_t), TAG_RENDERER, false, ALIGN_16,
          HEAP_DEFAULTHEAP));
  shadowMap->width = 1024;
  shadowMap->map = nullptr;
  std::memset(&shadowMap->mvp, 0, sizeof(shadowMap->mvp));
  shadowMap->mvp.m[15] = 1.0F;
  shadowMap->coordinateBias = -0.5F;
  shadowMap->depthBias = -40.0F;
  shadowMap->useShadowMap = true;
}

idGlobalShadows::~idGlobalShadows() {
  if (shadowMap->map != nullptr) {
    idMem::Singleton()->Free(shadowMap->map, ALIGN_16);
    shadowMap->map = nullptr;
  }
  idMem::Singleton()->Free(shadowMap, ALIGN_16);
}

void idGlobalShadows::LoadGlobalShadows(const char* mapName) {
  if (shadowMap->map != nullptr) {
    idMem::Singleton()->Free(shadowMap->map, ALIGN_16);
    shadowMap->map = nullptr;
  }
  std::memset(&shadowMap->mvp, 0, sizeof(shadowMap->mvp));
  shadowMap->mvp.m[15] = 1.0F;

  idStr shadowFilename(mapName);
  shadowFilename += "/globalShadows2.bin";
  if (!idFileResource::FileExists(shadowFilename.data)) {
    return;
  }

  idFileResource* resource = static_cast<idFileResource*>(
      idFileResource::resourceList.Load(shadowFilename.data, false, false));
  if (resource == nullptr || static_cast<int>(resource->length) <= 0) {
    return;
  }

  idFile_Memory* file = resource->GetFile();
  if (file == nullptr) {
    return;
  }
  if (file->Length() <= 0) {
    delete file;
    return;
  }

  idLib::Printf("Loading globalShadows.\n");
  file->Read(&shadowMap->mvp, sizeof(shadowMap->mvp));
  shadowMap->width = 1024;
  shadowMap->map = static_cast<std::uint16_t*>(
      idMem::Singleton()->AllocWithLocation(
          "w:\\build\\tech5\\engine\\renderer\\GlobalShadows.cpp(92) : TAG_RENDERWORLD",
          0x200000U, TAG_RENDERWORLD, false, ALIGN_16, HEAP_DEFAULTHEAP));
  file->Read(shadowMap->map, 0x200000U);
  delete file;

  shadowMap->coordinateBias = r_globalShadowsCoordinateBias.valueFloat;
  shadowMap->depthBias = r_globalShadowsDepthBias.valueFloat;
  shadowMap->useShadowMap = r_useGlobalShadows.valueInteger != 0;
  delete resource;
}

const shadowMapCPU_t* idGlobalShadows::GetShadowMap() {
  shadowMap->coordinateBias = r_globalShadowsCoordinateBias.valueFloat;
  shadowMap->depthBias = r_globalShadowsDepthBias.valueFloat;
  shadowMap->useShadowMap = r_useGlobalShadows.valueInteger != 0;
  return shadowMap;
}
