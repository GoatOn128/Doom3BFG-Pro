#include "engine/renderer/jobs/transcode/transcode.h"

#include "shared/idlib/parallelism/paralleljoblist.h"

#include <cstring>

namespace {
void CopyPlane(std::uint8_t* target, int targetPitch,
               const std::uint8_t* source, int rowBytes) {
  for (int row = 0; row < 32; ++row)
    std::memmove(target + row * targetPitch, source + row * rowBytes,
                 static_cast<std::size_t>(rowBytes));
}

void ClearPlane(std::uint8_t* target, int targetPitch, int rowBytes) {
  for (int row = 0; row < 32; ++row)
    std::memset(target + row * targetPitch, 0,
                static_cast<std::size_t>(rowBytes));
}
}

void TranscodePage_DXT_DXT(const transcodeParms_t* parms) {
  if (parms->imageFlags == 0)
    return;

  if (parms->inPageData != nullptr) {
    if ((parms->imageFlags & 1) != 0)
      CopyPlane(parms->targetImage[0], parms->targetBytePitch[0],
                parms->inPageData, 0x100);
    if ((parms->imageFlags & 2) != 0)
      CopyPlane(parms->targetImage[1], parms->targetBytePitch[1],
                parms->inPageData + 0x2000, 0x200);
    if ((parms->imageFlags & 4) != 0)
      CopyPlane(parms->targetImage[2], parms->targetBytePitch[2],
                parms->inPageData + 0x6000, 0x200);
    return;
  }

  if ((parms->imageFlags & 1) != 0)
    ClearPlane(parms->targetImage[0], parms->targetBytePitch[0], 0x100);
  if ((parms->imageFlags & 2) != 0)
    ClearPlane(parms->targetImage[1], parms->targetBytePitch[1], 0x200);
  if ((parms->imageFlags & 4) != 0)
    ClearPlane(parms->targetImage[2], parms->targetBytePitch[2], 0x200);
}

idParallelJobRegistration register_TranscodePage_DXT_DXT(
    reinterpret_cast<jobRun_t>(TranscodePage_DXT_DXT),
    "TranscodePage_DXT_DXT");

