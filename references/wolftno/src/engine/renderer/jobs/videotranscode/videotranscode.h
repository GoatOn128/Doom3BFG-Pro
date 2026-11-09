#pragma once

#include <cstddef>
#include <cstdint>

class idImage;

struct videoTranscodeParms_t {
  std::uint8_t dctQualityLuma;
  std::uint8_t dctQualityChroma;
  std::uint8_t* dctBuffer;
  int dctBufferSize;
  idImage* image;
  std::uint8_t* dxtBuffer;
  int targetBytePitch;
  bool needSubImageUpload;
};

void VideoTranscodeJob(videoTranscodeParms_t* videoParms);

static_assert(offsetof(videoTranscodeParms_t, dctBuffer) == 0x8,
              "video transcode input offset drift");
static_assert(offsetof(videoTranscodeParms_t, dctBufferSize) == 0x10,
              "video transcode input size offset drift");
static_assert(offsetof(videoTranscodeParms_t, image) == 0x18,
              "video transcode image offset drift");
static_assert(offsetof(videoTranscodeParms_t, dxtBuffer) == 0x20,
              "video transcode output offset drift");
static_assert(offsetof(videoTranscodeParms_t, targetBytePitch) == 0x28,
              "video transcode pitch offset drift");
static_assert(sizeof(videoTranscodeParms_t) == 0x30,
              "videoTranscodeParms_t ABI drift");
