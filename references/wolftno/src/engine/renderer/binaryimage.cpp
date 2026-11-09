#include "engine/renderer/binaryimage.h"

#include "engine/renderer/imagedata.h"
#include "engine/renderer/imagetools/color/colorspace.h"
#include "engine/renderer/imagetools/image_files.h"
#include "engine/renderer/imagetools/image_process.h"
#include "engine/renderer/imagetools/image_program.h"
#include "engine/renderer/jobs/transcode/image/dxt/dxtencoder.h"
#include "shared/idlib/color.h"
#include "shared/idlib/csystems/cvarsystemlocal.h"
#include "shared/idlib/filesystem/filesystemlocal.h"
#include "shared/idlib/lib.h"
#include "shared/idlib/parallelism/paralleljoblist.h"
#include "shared/idlib/text/parser.h"

#include <algorithm>
#include <array>
#include <cstdint>
#include <cstring>
#include <new>

idCVar r_envBlendPowerFilterSize(
    "r_envBlendPowerFilterSize", "0", CVAR_FLOAT,
    "Radius of specular power bleeding filter");
idCVar r_parallelHQDXTEncode(
    "r_parallelHQDXTEncode", "1", CVAR_BOOL,
    "Use parallel jobs for very slow HQ DXT encoding");
idCVar image_binaryImageDebugSaveTGA(
    "image_binaryImageDebugSaveTGA", "0", CVAR_INTEGER,
    "Write an tga next to the .bimage as debug tool for intermediate results\n"
    "0: disable\n"
    "1:write first mip\n"
    "2: write all mips");

template <typename T>
void* idListArrayNew(int count, memTag_t tag, bool zeroBuffer) {
  if (count <= 0) {
    return nullptr;
  }
  const char* location =
      zeroBuffer
          ? "w:\\build\\tech5\\shared\\idlib\\containers/List.h(27) : tag"
          : "w:\\build\\tech5\\shared\\idlib\\containers/List.h(29) : tag";
  auto* storage = static_cast<T*>(idMem::Singleton()->AllocWithLocation(
      location, sizeof(T) * static_cast<std::size_t>(count), tag, zeroBuffer,
      ALIGN_16, HEAP_DEFAULTHEAP));
  if (storage != nullptr) {
    for (int index = 0; index < count; ++index) {
      new (&storage[index]) T;
    }
  }
  return storage;
}

template <typename T>
void idListArrayDelete(void* pointer, int count) {
  auto* storage = static_cast<T*>(pointer);
  for (int index = 0; index < count; ++index) {
    storage[index].~T();
  }
  idMem::Singleton()->Free(storage, ALIGN_16);
}

template <typename T>
void* idListArrayResize(void* oldPointer, int oldCount, int newCount,
                        memTag_t tag, bool zeroBuffer) {
  T* replacement = nullptr;
  if (newCount > 0) {
    replacement =
        static_cast<T*>(idListArrayNew<T>(newCount, tag, zeroBuffer));
    if (replacement == nullptr) {
      return nullptr;
    }
    auto* oldStorage = static_cast<T*>(oldPointer);
    const int copyCount = std::min(oldCount, newCount);
    for (int index = 0; index < copyCount; ++index) {
      replacement[index] = oldStorage[index];
    }
  }
  idListArrayDelete<T>(oldPointer, oldCount);
  return replacement;
}

template void* idListArrayNew<idBinaryImage::idBinaryImageData>(
    int, memTag_t, bool);
template void idListArrayDelete<idBinaryImage::idBinaryImageData>(void*, int);
template void* idListArrayResize<idBinaryImage::idBinaryImageData>(
    void*, int, int, memTag_t, bool);

namespace {

constexpr std::uint32_t BIMAGE_MAGIC = 0x094D4942U;

std::uint32_t ByteSwap32(std::uint32_t value) {
  return ((value & 0x000000FFU) << 24U) |
         ((value & 0x0000FF00U) << 8U) |
         ((value & 0x00FF0000U) >> 8U) |
         ((value & 0xFF000000U) >> 24U);
}

int ByteSwapInt(int value) {
  return static_cast<int>(ByteSwap32(static_cast<std::uint32_t>(value)));
}

float ByteSwapFloat(float value) {
  std::uint32_t bits = 0;
  std::memcpy(&bits, &value, sizeof(bits));
  bits = ByteSwap32(bits);
  std::memcpy(&value, &bits, sizeof(value));
  return value;
}

void* ImageAlloc(std::size_t size, bool zero, const char* location) {
  return idMem::Singleton()->AllocWithLocation(location, size, TAG_IMAGE, zero,
                                               ALIGN_16, HEAP_DEFAULTHEAP);
}

void* TempAlloc(std::size_t size, const char* location) {
  return idMem::Singleton()->AllocWithLocation(location, size, TAG_TEMP, false,
                                               ALIGN_16, HEAP_DEFAULTHEAP);
}

void MemFree(void* pointer) {
  if (pointer != nullptr) {
    idMem::Singleton()->Free(pointer, ALIGN_16);
  }
}

void SetDataSize(idBinaryImage::idBinaryImageData& image, int size) {
  image.dataSize = size;
  image.data.SetNum(size);
}

void SetImageHeader(idBinaryImage::idBinaryImageData& image, int level,
                    int destinationZ, int width, int height) {
  image.level = level;
  image.destZ = destinationZ;
  image.width = width;
  image.height = height;
}

void WriteBigInt(idFile* file, int value) {
  const int swapped = ByteSwapInt(value);
  file->Write(&swapped, sizeof(swapped));
}

void WriteBigFloat(idFile* file, float value) {
  const float swapped = ByteSwapFloat(value);
  file->Write(&swapped, sizeof(swapped));
}

int ReadBigInt(idFile* file) {
  int value = 0;
  file->Read(&value, sizeof(value));
  return ByteSwapInt(value);
}

float ReadBigFloat(idFile* file) {
  float value = 0.0F;
  file->Read(&value, sizeof(value));
  return ByteSwapFloat(value);
}

}  // namespace

void HQDXTjob_t::Job(void* parameter) {
  const auto* job = static_cast<const HQDXTjob_t*>(parameter);
  for (int chunkIndex = 0; chunkIndex < job->numChunks; ++chunkIndex) {
    alignas(16) std::uint8_t extract[64];
    const std::uint8_t* source = job->chunks[chunkIndex].src;
    for (int row = 0; row < 4; ++row) {
      std::memcpy(extract + row * 16, source + row * job->srcPitch, 16);
    }
    idDxtEncoder encoder;
    if (job->normalMap) {
      encoder.CompressNormalMapDXT5HQ(
          extract, job->chunks[chunkIndex].dest, 4, 4);
    } else {
      encoder.CompressImageDXT5HQ(extract, job->chunks[chunkIndex].dest,
                                  4, 4);
    }
  }
}

void JobParallelHQDXTCompress(const void* source, void* destination,
                              int width, int height, bool normalMap) {
  RegisterJob(&HQDXTjob_t::Job, "HQDXTjob");

  const int numBlocks = height * (width / 4) / 4;
  if (numBlocks <= 0) {
    return;
  }
  const int jobLimit = std::min(numBlocks, 128);
  const int numJobs = jobLimit + 1;
  const int chunksPerJob =
      (numJobs + numBlocks - 2) / (numJobs - 1);

  auto* chunks = static_cast<HQDXTjob_t::jobData_t*>(TempAlloc(
      sizeof(HQDXTjob_t::jobData_t) * static_cast<std::size_t>(numBlocks + 1),
      "w:\\build\\tech5\\shared\\idlib\\containers/Array.h(105) : TAG_TEMP"));
  auto* jobs = static_cast<HQDXTjob_t*>(TempAlloc(
      sizeof(HQDXTjob_t) * static_cast<std::size_t>(numJobs),
      "w:\\build\\tech5\\shared\\idlib\\containers/Array.h(105) : TAG_TEMP"));

  idParallelJobList* jobList = parallelJobManager->AllocJobList(
      JOBLIST_UTILITY, JOBLIST_PRIORITY_MEDIUM,
      static_cast<unsigned>(numJobs), 1, &idColor::colorRed);

  int chunkIndex = 0;
  int jobIndex = 0;
  jobs[0] = {width * 4, normalMap, chunks, 0};
  const auto* input = static_cast<const std::uint8_t*>(source);
  auto* output = static_cast<std::uint8_t*>(destination);
  for (int blockY = 0; blockY < height; blockY += 4) {
    const std::uint8_t* blockSource =
        input + static_cast<std::size_t>(blockY) * width * 4;
    for (int blockX = 0; blockX < width; blockX += 4) {
      chunks[chunkIndex].src = blockSource + blockX * 4;
      chunks[chunkIndex].dest = output + chunkIndex * 16;
      ++chunkIndex;
      ++jobs[jobIndex].numChunks;
      if (jobs[jobIndex].numChunks == chunksPerJob) {
        jobList->AddJob(&HQDXTjob_t::Job, &jobs[jobIndex]);
        ++jobIndex;
        jobs[jobIndex] = {width * 4, normalMap,
                          chunkIndex < numBlocks ? &chunks[chunkIndex]
                                                 : nullptr,
                          0};
      }
    }
  }
  if (jobs[jobIndex].numChunks != 0) {
    jobList->AddJob(&HQDXTjob_t::Job, &jobs[jobIndex]);
  }
  jobList->Submit(nullptr, -2);
  jobList->Wait();
  parallelJobManager->FreeJobList(jobList);

  MemFree(jobs);
  MemFree(chunks);
}

idBinaryImage::idBinaryImage(const char* name)
    : imgName(name),
      sourceFileTime(~0U),
      binaryFileTime(~0U),
      opts{},
      images() {
  opts.sampler.filter = TF_DEFAULT;
  opts.sampler.repeat = TR_REPEAT;
  opts.sampler.lodMaxClamp = 13.0F;
  opts.packedTail = true;
  opts.textureOptions = TO_NONE;
}

idBinaryImage::~idBinaryImage() = default;

void idBinaryImage::WriteGeneratedFile(const char* binaryFileName) {
  idFileLocal file(fileSystem->OpenFileWrite(binaryFileName));
  if (file.file == nullptr) {
    idLib::Warning("idBinaryImage: Could not open file '%s'", binaryFileName);
    return;
  }

  idLib::Printf("Writing %s\n", binaryFileName);
  WriteBigInt(file.file, static_cast<int>(sourceFileTime));
  WriteBigInt(file.file, static_cast<int>(BIMAGE_MAGIC));
  WriteBigInt(file.file, static_cast<int>(opts.textureType));
  WriteBigInt(file.file, opts.width);
  WriteBigInt(file.file, opts.height);
  WriteBigInt(file.file, opts.depth);
  WriteBigInt(file.file, opts.numLevels);
  WriteBigInt(file.file, opts.cubeFilter ? 1 : 0);
  WriteBigInt(file.file, static_cast<int>(opts.format));
  WriteBigInt(file.file, static_cast<int>(opts.colorFormat));
  WriteBigInt(file.file, static_cast<int>(opts.sampler.filter));
  WriteBigInt(file.file, static_cast<int>(opts.sampler.repeat));
  WriteBigFloat(file.file, opts.sampler.lodBias);

  for (int index = 0; index < images.Num(); ++index) {
    const idBinaryImageData& image = images[index];
    WriteBigInt(file.file, image.level);
    WriteBigInt(file.file, image.destZ);
    WriteBigInt(file.file, image.width);
    WriteBigInt(file.file, image.height);
    WriteBigInt(file.file, image.dataSize);
    file.file->Write(image.data.list,
                     static_cast<std::size_t>(image.dataSize));
  }

  delete file.Release();
  binaryFileTime = fileSystem->GetTimestamp(binaryFileName);

  if (image_binaryImageDebugSaveTGA.valueInteger != 0) {
    idStr baseName(binaryFileName);
    baseName.StripFileExtension();
    for (int level = 0; level < images.Num(); ++level) {
      idImage image;
      image.LoadFromBinaryImage(*this);
      idImageData* imageData = image.GetImageData(level, 0);
      if (imageData != nullptr) {
        idStr outputName;
        outputName.Format("%s_%d_%d.tga", baseName.data, level,
                          images.Num());
        imageData->WriteTGA(outputName.data);
        MemFree(imageData->data);
        MemFree(imageData->floatData);
        MemFree(imageData);
      }
      if (image_binaryImageDebugSaveTGA.valueInteger == 1) {
        break;
      }
    }
  }
}

void idBinaryImage::Load2DFromMemory(int width, int height,
                                     const std::uint8_t* picture) {
  const int originalPixelCount = width * height;
  auto* current = static_cast<std::uint8_t*>(ImageAlloc(
      static_cast<std::size_t>(originalPixelCount) * 4, false,
      "w:\\build\\tech5\\engine\\renderer\\BinaryImage.cpp(267) : TAG_IMAGE"));
  std::memmove(current, picture,
               static_cast<std::size_t>(originalPixelCount) * 4);

  if (opts.colorFormat == CFM_YCOCG_DXT5) {
    idColorSpace::ConvertRGBToCoCg_Y(current, current, width, height);
  } else if (opts.colorFormat == CFM_YCOCGA_DXT5 ||
             opts.colorFormat == CFM_YCOCGA_HQ_DXT5) {
    idColorSpace::ConvertRGBAToCoACg_Y(current, current, width, height);
  }

  if (opts.format == FMT_NONE) {
    int alphaAnd = 255;
    for (int pixel = 0; pixel < originalPixelCount; ++pixel) {
      alphaAnd &= current[pixel * 4 + 3];
    }
    opts.format = alphaAnd == 255 ? FMT_DXT1 : FMT_DXT5;
  }

  opts.width = width;
  opts.height = height;
  ComputeNumLevels(&opts, imgName.data);
  images.SetNum(opts.numLevels);

  int scaledWidth = width;
  int scaledHeight = height;
  for (int level = 0; level < images.Num(); ++level) {
    idBinaryImageData& image = images[level];
    SetImageHeader(image, level, 0, scaledWidth, scaledHeight);

    std::uint8_t* compressionInput = current;
    int compressionWidth = 0;
    int compressionHeight = 0;
    if (opts.format == FMT_DXT1 || opts.format == FMT_DXT5) {
      if ((scaledWidth & 3) != 0 || (scaledHeight & 3) != 0) {
        compressionWidth = (scaledWidth + 3) & ~3;
        compressionHeight = (scaledHeight + 3) & ~3;
        compressionInput = static_cast<std::uint8_t*>(ImageAlloc(
            static_cast<std::size_t>(compressionWidth) * compressionHeight * 4,
            true,
            "w:\\build\\tech5\\engine\\renderer\\BinaryImage.cpp(331) : TAG_IMAGE"));
        for (int row = 0; row < scaledHeight; ++row) {
          std::memmove(compressionInput + row * compressionWidth * 4,
                       current + row * scaledWidth * 4,
                       static_cast<std::size_t>(scaledWidth) * 4);
        }
      } else {
        compressionWidth = scaledWidth;
        compressionHeight = scaledHeight;
      }
    }

    idDxtEncoder encoder;
    switch (opts.format) {
      case FMT_DXT1: {
        SetDataSize(image, compressionWidth * compressionHeight / 2);
        if (opts.colorFormat == CFM_RGB_HQ_DXT1) {
          encoder.CompressImageDXT1HQ(compressionInput, image.data.list,
                                      compressionWidth, compressionHeight);
        } else {
          encoder.CompressImageDXT1Fast_SSE2(
              compressionInput, image.data.list, compressionWidth,
              compressionHeight);
        }
        break;
      }
      case FMT_DXT5: {
        SetDataSize(image, compressionWidth * compressionHeight);
        if (opts.colorFormat == CFM_RGBA_HQ_DXT5 ||
            opts.colorFormat == CFM_YCOCGA_HQ_DXT5) {
          idLib::Printf(
              "Image %s uses high quality compression, writing generated will take additional time\n",
              imgName.data);
          if (r_parallelHQDXTEncode.valueInteger != 0) {
            JobParallelHQDXTCompress(compressionInput, image.data.list,
                                     compressionWidth, compressionHeight,
                                     false);
          } else {
            encoder.CompressImageDXT5HQ(compressionInput, image.data.list,
                                        compressionWidth,
                                        compressionHeight);
          }
        } else if (opts.colorFormat == CFM_NORMAL_HQ_DXT5) {
          idLib::Printf(
              "Image %s uses high quality compression, writing generated will take additional time\n",
              imgName.data);
          if (r_parallelHQDXTEncode.valueInteger != 0) {
            JobParallelHQDXTCompress(compressionInput, image.data.list,
                                     compressionWidth, compressionHeight,
                                     true);
          } else {
            encoder.CompressNormalMapDXT5HQ(
                compressionInput, image.data.list, compressionWidth,
                compressionHeight);
          }
        } else if (opts.colorFormat == CFM_YCOCG_DXT5) {
          encoder.CompressYCoCgDXT5Fast_SSE2(
              compressionInput, image.data.list, compressionWidth,
              compressionHeight);
        } else if (opts.colorFormat == CFM_YCOCGA_DXT5) {
          encoder.CompressYCoCgAlphaDXT5Fast(
              compressionInput, image.data.list, compressionWidth,
              compressionHeight);
        } else {
          encoder.CompressImageDXT5Fast_SSE2(
              compressionInput, image.data.list, compressionWidth,
              compressionHeight);
        }
        break;
      }
      case FMT_LUM8:
      case FMT_INT8: {
        SetDataSize(image, scaledWidth * scaledHeight);
        for (int pixel = 0; pixel < image.data.Num(); ++pixel) {
          image.data[pixel] = current[pixel * 4];
        }
        break;
      }
      case FMT_ALPHA: {
        SetDataSize(image, scaledWidth * scaledHeight);
        for (int pixel = 0; pixel < image.data.Num(); ++pixel) {
          image.data[pixel] = current[pixel * 4 + 3];
        }
        break;
      }
      case FMT_L8A8: {
        SetDataSize(image, scaledWidth * scaledHeight * 2);
        for (int pixel = 0; pixel < image.data.Num() / 2; ++pixel) {
          image.data[pixel * 2] = current[pixel * 4];
          image.data[pixel * 2 + 1] = current[pixel * 4 + 3];
        }
        break;
      }
      case FMT_RG8: {
        SetDataSize(image, scaledWidth * scaledHeight * 2);
        for (int pixel = 0; pixel < image.data.Num() / 2; ++pixel) {
          image.data[pixel * 2] = current[pixel * 4];
          image.data[pixel * 2 + 1] = current[pixel * 4 + 1];
        }
        break;
      }
      case FMT_RGBA32F: {
        const int componentCount = scaledWidth * scaledHeight * 4;
        SetDataSize(image, componentCount * static_cast<int>(sizeof(float)));
        auto* floats = reinterpret_cast<float*>(image.data.list);
        for (int component = 0; component < componentCount; ++component) {
          floats[component] = static_cast<float>(current[component]);
        }
        break;
      }
      default: {
        SetDataSize(image, scaledWidth * scaledHeight * 4);
        std::memmove(image.data.list, current,
                     static_cast<std::size_t>(image.data.Num()));
        break;
      }
    }

    if (compressionInput != current) {
      MemFree(compressionInput);
    }
    std::uint8_t* next = R_MipMap(current, scaledWidth, scaledHeight);
    MemFree(current);
    current = next;
    scaledWidth = std::max(1, scaledWidth >> 1);
    scaledHeight = std::max(1, scaledHeight >> 1);
  }
  MemFree(current);
}

void idBinaryImage::LoadCubeFromMemory(
    int width, const std::uint8_t** pictures) {
  opts.width = width;
  opts.height = width;

  if (opts.cubeFilter) {
    if (width < 64) {
      opts.width = 64;
      opts.height = 64;
    }
    int scaledWidth = opts.width;
    std::array<std::uint8_t*, 6> faces{};
    for (int side = 0; side < 6; ++side) {
      if (width < 64) {
        faces[side] =
            R_ResampleTexture(pictures[side], width, width, 64, 64);
      } else {
        faces[side] = static_cast<std::uint8_t*>(ImageAlloc(
            static_cast<std::size_t>(scaledWidth) * scaledWidth * 4, false,
            "w:\\build\\tech5\\engine\\renderer\\BinaryImage.cpp(485) : TAG_IMAGE"));
        std::memmove(faces[side], pictures[side],
                     static_cast<std::size_t>(scaledWidth) * scaledWidth * 4);
      }
    }

    images.SetNum(30);
    for (int level = 0; level < 5; ++level) {
      int filterSize = r_envBlendPowerFilterSize.valueInteger;
      if (level == 4) {
        filterSize *= 2;
      }
      R_FilterCube(scaledWidth, faces.data(), filterSize);
      R_BlendCubeBorders(scaledWidth, faces.data());

      for (int side = 0; side < 6; ++side) {
        idBinaryImageData& image = images[level * 6 + side];
        SetImageHeader(image, level, side, scaledWidth, scaledWidth);
        idDxtEncoder encoder;
        if (opts.format == FMT_DXT1) {
          SetDataSize(image, scaledWidth * scaledWidth / 2);
          encoder.CompressImageDXT1Fast_SSE2(
              faces[side], image.data.list, scaledWidth, scaledWidth);
        } else if (opts.format == FMT_DXT5) {
          SetDataSize(image, scaledWidth * scaledWidth);
          encoder.CompressImageDXT5Fast_SSE2(
              faces[side], image.data.list, scaledWidth, scaledWidth);
        } else {
          SetDataSize(image, scaledWidth * scaledWidth * 4);
          std::memmove(image.data.list, faces[side],
                       static_cast<std::size_t>(image.data.Num()));
        }
      }

      for (int side = 0; side < 6; ++side) {
        std::uint8_t* previous = faces[side];
        faces[side] = R_MipMap(previous, scaledWidth, scaledWidth);
        MemFree(previous);
      }
      scaledWidth >>= 1;
    }
    for (std::uint8_t* face : faces) {
      MemFree(face);
    }
    return;
  }

  ComputeNumLevels(&opts, imgName.data);
  images.SetNum(opts.numLevels * 6);
  for (int side = 0; side < 6; ++side) {
    const std::uint8_t* original = pictures[side];
    const std::uint8_t* current = original;
    int scaledWidth = opts.width;
    for (int level = 0; level < opts.numLevels; ++level) {
      alignas(16) std::uint8_t padBlock[64];
      int compressionWidth = scaledWidth;
      const std::uint8_t* compressionInput = current;
      if (scaledWidth < 4 &&
          (opts.format == FMT_DXT1 || opts.format == FMT_DXT5)) {
        for (int y = 0; y < 4; ++y) {
          for (int x = 0; x < 4; ++x) {
            const int sourcePixel =
                (y % scaledWidth) * scaledWidth + (x % scaledWidth);
            std::memcpy(&padBlock[(y * 4 + x) * 4],
                        &current[sourcePixel * 4], 4);
          }
        }
        compressionWidth = 4;
        compressionInput = padBlock;
      }

      idBinaryImageData& image = images[level * 6 + side];
      SetImageHeader(image, level, side,
                     opts.format == FMT_DXT1 || opts.format == FMT_DXT5
                         ? compressionWidth
                         : scaledWidth,
                     opts.format == FMT_DXT1 || opts.format == FMT_DXT5
                         ? compressionWidth
                         : scaledWidth);
      idDxtEncoder encoder;
      if (opts.format == FMT_DXT1) {
        SetDataSize(image, compressionWidth * compressionWidth / 2);
        encoder.CompressImageDXT1Fast_SSE2(
            compressionInput, image.data.list, compressionWidth,
            compressionWidth);
      } else if (opts.format == FMT_DXT5) {
        SetDataSize(image, compressionWidth * compressionWidth);
        encoder.CompressImageDXT5Fast_SSE2(
            compressionInput, image.data.list, compressionWidth,
            compressionWidth);
      } else {
        SetDataSize(image, scaledWidth * scaledWidth * 4);
        std::memmove(image.data.list, current,
                     static_cast<std::size_t>(image.data.Num()));
      }

      std::uint8_t* next = R_MipMap(current, scaledWidth, scaledWidth);
      if (current != original) {
        MemFree(const_cast<std::uint8_t*>(current));
      }
      current = next;
      scaledWidth = std::max(1, scaledWidth >> 1);
    }
    if (current != original) {
      MemFree(const_cast<std::uint8_t*>(current));
    }
  }
}

bool idBinaryImage::LoadFromGeneratedFile(idFile* binaryFile) {
  binaryFileTime = binaryFile->Timestamp();
  bimageFile_t fileHeader{};
  fileHeader.sourceFileTime = ReadBigInt(binaryFile);
  fileHeader.headerMagic = ReadBigInt(binaryFile);
  if (static_cast<std::uint32_t>(fileHeader.headerMagic) != BIMAGE_MAGIC) {
    idLib::Warning(
        "idBinaryImage::LoadFromGeneratedFile : %s has out of date magic",
        imgName.data);
    return false;
  }

  fileHeader.textureType = ReadBigInt(binaryFile);
  fileHeader.width = ReadBigInt(binaryFile);
  fileHeader.height = ReadBigInt(binaryFile);
  fileHeader.depth = ReadBigInt(binaryFile);
  fileHeader.numLevels = ReadBigInt(binaryFile);
  fileHeader.cubeFilter = ReadBigInt(binaryFile);
  fileHeader.format = ReadBigInt(binaryFile);
  fileHeader.colorFormat = ReadBigInt(binaryFile);
  fileHeader.filter = ReadBigInt(binaryFile);
  fileHeader.repeat = ReadBigInt(binaryFile);
  fileHeader.lodBias = ReadBigFloat(binaryFile);

  sourceFileTime = static_cast<std::uint32_t>(fileHeader.sourceFileTime);
  opts.textureType = static_cast<textureType_t>(fileHeader.textureType);
  opts.width = fileHeader.width;
  opts.height = fileHeader.height;
  opts.depth = fileHeader.depth;
  opts.numLevels = fileHeader.numLevels;
  opts.cubeFilter = fileHeader.cubeFilter != 0;
  opts.format = static_cast<textureFormat_t>(fileHeader.format);
  opts.colorFormat = static_cast<textureColor_t>(fileHeader.colorFormat);
  opts.sampler.filter = static_cast<textureFilter_t>(fileHeader.filter);
  opts.sampler.repeat = static_cast<textureRepeat_t>(fileHeader.repeat);
  opts.sampler.lodBias = fileHeader.lodBias;

  int numImages = opts.numLevels;
  if (opts.textureType == TT_CUBIC) {
    numImages *= 6;
  } else if (opts.textureType == TT_3D) {
    numImages *= opts.depth;
  }
  images.SetNum(numImages);
  for (int index = 0; index < numImages; ++index) {
    idBinaryImageData& image = images[index];
    image.level = ReadBigInt(binaryFile);
    image.destZ = ReadBigInt(binaryFile);
    image.width = ReadBigInt(binaryFile);
    image.height = ReadBigInt(binaryFile);
    image.dataSize = ReadBigInt(binaryFile);
    image.data.SetNum(image.dataSize);
    binaryFile->Read(image.data.list,
                     static_cast<std::size_t>(image.dataSize));
  }
  return true;
}

bool idBinaryImage::LoadFromGeneratedFile(const char* binaryFileName) {
  idFileLocal binaryFile(fileSystem->OpenFileRead(binaryFileName));
  return binaryFile.file != nullptr &&
         LoadFromGeneratedFile(binaryFile.file);
}

void idImage::WriteBinaryImage(const char* imageName, idImageOpts options,
                               const std::uint8_t* imageData,
                               std::uint32_t sourceTimestamp) {
  char binaryFileName[256];
  MakeGeneratedName(binaryFileName, sizeof(binaryFileName), imageName);
  idBinaryImage binaryImage(imageName);
  binaryImage.opts = options;
  binaryImage.sourceFileTime = sourceTimestamp;
  binaryImage.Load2DFromMemory(options.width, options.height, imageData);
  binaryImage.WriteGeneratedFile(binaryFileName);
}

bool idBinaryImage::LoadFromSourceFile() {
  idParser parser(0);
  parser.LoadMemory(imgName.data, imgName.Length(), imgName.data);
  parser.SetFlags(564);
  idImage::ParseImageOpts(parser, opts);

  if (std::strstr(imgName.data, "_forward") != nullptr ||
      std::strstr(imgName.data, "_px") != nullptr) {
    opts.textureType = TT_CUBIC;
    opts.sampler.repeat = TR_CLAMP;
  }

  if (opts.textureType == TT_CUBIC) {
    if (opts.cubeFilter) {
      opts.numLevels = 5;
      opts.format = FMT_DXT1;
      opts.colorFormat = CFM_RGB_HQ_DXT1;
    }

    idToken token;
    parser.ReadToken(token);
    std::uint8_t* pictures[6]{};
    int width = 0;
    idFileProps fileProps{~0U, false};
    if (!R_LoadCubeImages(token.data, pictures, &width, &fileProps)) {
      idLib::Warning("Couldn't load cube image: %s", imgName.data);
      return false;
    }
    sourceFileTime = fileProps.timestamp;

    if (opts.format == FMT_NONE) {
      opts.format = FMT_DXT1;
      for (int side = 0; side < 6 && opts.format != FMT_DXT5; ++side) {
        const int pixelCount = width * width;
        for (int pixel = 0; pixel < pixelCount; ++pixel) {
          if (pictures[side][pixel * 4 + 3] != 255) {
            opts.format = FMT_DXT5;
            break;
          }
        }
      }
    }
    opts.width = width;
    opts.height = width;
    LoadCubeFromMemory(width, const_cast<const std::uint8_t**>(pictures));
    for (std::uint8_t* picture : pictures) {
      MemFree(picture);
    }
    return true;
  }

  std::uint8_t* picture = nullptr;
  int width = 0;
  int height = 0;
  idFileProps fileProps{~0U, false};
  R_LoadImageProgram(&parser, &picture, &width, &height, &fileProps);
  sourceFileTime = fileProps.timestamp;
  if (picture == nullptr) {
    idLib::Warning("Couldn't load image: %s", imgName.data);
    return false;
  }
  Load2DFromMemory(width, height, picture);
  MemFree(picture);
  return true;
}
