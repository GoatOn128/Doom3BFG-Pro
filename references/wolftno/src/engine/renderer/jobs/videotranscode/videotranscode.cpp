#include "engine/renderer/jobs/videotranscode/videotranscode.h"

#include "engine/renderer/jobs/transcode/transcode.h"
#include "shared/idlib/parallelism/paralleljoblist.h"

#include <cstdint>
#include <cstring>

void VideoTranscodeJob(videoTranscodeParms_t* videoParms) {
  DCTHeader_t header{};
  header.qualityLuma = videoParms->dctQualityLuma;
  header.qualityChroma = videoParms->dctQualityChroma;
  header.diffuseSize = static_cast<std::uint16_t>(videoParms->dctBufferSize);

  transcodeParms_t parms{};
  parms.imageFlags = 2;
  parms.inPageDataLength = videoParms->dctBufferSize;
  parms.diskOffsetScale = 64;
  parms.tempSize = 5120;
  parms.inPageHeader = reinterpret_cast<const std::uint8_t*>(&header);
  parms.inPageData = videoParms->dctBuffer;

  alignas(128) std::uint8_t temporaryData[5120];
  parms.tempData = temporaryData;
  for (int plane = 0; plane < 3; ++plane) {
    parms.targetImage[plane] = videoParms->dxtBuffer;
    parms.targetBytePitch[plane] = videoParms->targetBytePitch;
  }
  TranscodePage_DCT_DXT(&parms);
}

idParallelJobRegistration register_VideoTranscodeJob(
    reinterpret_cast<jobRun_t>(VideoTranscodeJob), "VideoTranscodeJob");
