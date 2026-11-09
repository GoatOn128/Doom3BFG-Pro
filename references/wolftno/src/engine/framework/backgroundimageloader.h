#pragma once

#include "shared/idlib/text/str.h"

#include <cstddef>
#include <cstdint>

class idFile;
class idParallelJobList;

class idBackgroundImageLoader {
public:
  enum imageFormat_t : int {
    FORMAT_UNKNOWN = 0,
    FORMAT_DCT = 1,
    FORMAT_PNG = 2,
  };

  struct decodeImageJobOutput_t {
    bool result;
    void* imageData;
    int imageWidth;
    int imageHeight;
  };

  struct decodeImageJobParms_t {
    const void* inputBuffer;
    std::uint64_t inputSize;
    imageFormat_t format;
    decodeImageJobOutput_t* output;
  };

  explicit idBackgroundImageLoader(const char* imageName_);
  ~idBackgroundImageLoader();

  bool IsReady() const;
  bool Update();
  void Shutdown();
  void SetImage(const char* imagePath);

private:
  static void DecodeImageJob(const decodeImageJobParms_t* parms);
  bool StartRead(const char* textureName);

public:
  decodeImageJobParms_t jobParms;
  decodeImageJobOutput_t jobOutput;
  idParallelJobList* jobList;
  const char* imageName;
  idStr nextImage;
  idStr currentImage;
  idFile* fileHandle;
  int state;
  volatile bool dataComplete;
  std::uint64_t dataSize;
  void* dataBuffer;
};

static_assert(sizeof(idBackgroundImageLoader::decodeImageJobParms_t) == 0x20,
              "decodeImageJobParms_t ABI drift");
static_assert(sizeof(idBackgroundImageLoader::decodeImageJobOutput_t) == 0x18,
              "decodeImageJobOutput_t ABI drift");
static_assert(sizeof(idBackgroundImageLoader) == 0xB8,
              "idBackgroundImageLoader ABI drift");
static_assert(offsetof(idBackgroundImageLoader, jobOutput) == 0x20,
              "idBackgroundImageLoader::jobOutput ABI drift");
static_assert(offsetof(idBackgroundImageLoader, nextImage) == 0x48,
              "idBackgroundImageLoader::nextImage ABI drift");
static_assert(offsetof(idBackgroundImageLoader, state) == 0xA0,
              "idBackgroundImageLoader::state ABI drift");
static_assert(offsetof(idBackgroundImageLoader, dataBuffer) == 0xB0,
              "idBackgroundImageLoader::dataBuffer ABI drift");
