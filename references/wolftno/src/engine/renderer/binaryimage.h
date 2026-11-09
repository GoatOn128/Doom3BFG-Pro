#pragma once

#include "engine/renderer/image.h"
#include "shared/idlib/containers/list.h"
#include "shared/idlib/sys/sys_alloc.h"

#include <cstddef>
#include <cstdint>

class idFile;
class idCVar;

struct bimageFile_t {
  int sourceFileTime;
  int headerMagic;
  int textureType;
  int width;
  int height;
  int depth;
  int numLevels;
  int cubeFilter;
  int format;
  int colorFormat;
  int filter;
  int repeat;
  float lodBias;
};

struct bimageImage_t {
  int level;
  int destZ;
  int width;
  int height;
  int dataSize;
};

struct HQDXTjob_t {
  struct jobData_t {
    const std::uint8_t* src;
    std::uint8_t* dest;
  };

  static void Job(void* parameter);

  int srcPitch;
  bool normalMap;
  const jobData_t* chunks;
  int numChunks;
};

void JobParallelHQDXTCompress(const void* source, void* destination,
                              int width, int height, bool normalMap);

class idBinaryImage {
public:
  class idBinaryImageData : public bimageImage_t {
  public:
    idList<std::uint8_t, TAG_IMAGE> data;
  };

  explicit idBinaryImage(const char* name);
  ~idBinaryImage();

  void Load2DFromMemory(int width, int height,
                        const std::uint8_t* picture);
  void LoadCubeFromMemory(int width, const std::uint8_t** pictures);
  bool LoadFromGeneratedFile(const char* binaryFileName);
  bool LoadFromSourceFile();
  void WriteGeneratedFile(const char* binaryFileName);

  const char* GetName() const { return imgName.data; }
  int NumImages() const { return images.Num(); }
  const idBinaryImageData& GetImage(int index) const { return images[index]; }

  idStr imgName;
  std::uint32_t sourceFileTime;
  std::uint32_t binaryFileTime;
  idImageOpts opts;
  idList<idBinaryImageData, TAG_IMAGE> images;

private:
  bool LoadFromGeneratedFile(idFile* binaryFile);
};

extern idCVar r_envBlendPowerFilterSize;
extern idCVar r_parallelHQDXTEncode;
extern idCVar image_binaryImageDebugSaveTGA;

static_assert(sizeof(bimageFile_t) == 0x34,
              "binary-image file header ABI drift");
static_assert(sizeof(bimageImage_t) == 0x14,
              "binary-image entry header ABI drift");
static_assert(sizeof(HQDXTjob_t::jobData_t) == 0x10,
              "HQ DXT chunk ABI drift");
static_assert(offsetof(HQDXTjob_t, chunks) == 0x8,
              "HQ DXT job chunk offset drift");
static_assert(sizeof(HQDXTjob_t) == 0x18, "HQ DXT job ABI drift");
static_assert(offsetof(idBinaryImage::idBinaryImageData, data) == 0x18,
              "binary-image entry data offset drift");
static_assert(sizeof(idBinaryImage::idBinaryImageData) == 0x30,
              "binary-image entry ABI drift");
static_assert(offsetof(idBinaryImage, opts) == 0x30,
              "binary-image options offset drift");
static_assert(offsetof(idBinaryImage, images) == 0x98,
              "binary-image list offset drift");
static_assert(sizeof(idBinaryImage) == 0xB0,
              "binary-image ABI drift");
