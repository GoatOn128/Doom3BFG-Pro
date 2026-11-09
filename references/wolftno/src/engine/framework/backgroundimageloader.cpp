#include "engine/framework/backgroundimageloader.h"

#include "engine/decls/decl.h"
#include "engine/framework/resourcemanager.h"
#include "engine/framework/streamfilecache.h"
#include "engine/renderer/image.h"
#include "engine/renderer/imagemanager.h"
#include "engine/renderer/imagetools/image_files.h"
#include "shared/idlib/color.h"
#include "shared/idlib/filesystem/file.h"
#include "shared/idlib/filesystem/filesystemlocal.h"
#include "shared/idlib/lib.h"
#include "shared/idlib/parallelism/paralleljoblist.h"
#include "shared/idlib/sys/sys_alloc.h"
#include "shared/idlib/sys/sys_time.h"

bool idBackgroundImageLoader::IsReady() const {
  return nextImage.IsEmpty() && state == 0;
}

void idBackgroundImageLoader::DecodeImageJob(
    const decodeImageJobParms_t* parms) {
  decodeImageJobOutput_t& output = *parms->output;
  output.result = false;
  output.imageData = nullptr;
  output.imageWidth = 0;
  output.imageHeight = 0;

  if (parms->format == FORMAT_DCT) {
    if (R_ReadDCTFromBuffer(
            static_cast<const std::uint8_t*>(parms->inputBuffer),
            parms->inputSize, nullptr, &output.imageWidth,
            &output.imageHeight)) {
      output.imageData = idMem::Singleton()->AllocWithLocation(
          "w:\\build\\tech5\\engine\\framework\\BackgroundImageLoader.cpp(73) : TAG_IMAGE",
          4 * output.imageWidth * output.imageHeight, TAG_IMAGE, true, ALIGN_16,
          HEAP_DEFAULTHEAP);
      if (output.imageData != nullptr &&
          R_ReadDCTFromBuffer(
              static_cast<const std::uint8_t*>(parms->inputBuffer),
              parms->inputSize, static_cast<std::uint8_t*>(output.imageData),
              &output.imageWidth, &output.imageHeight)) {
        output.result = true;
      }
    }
  } else if (parms->format == FORMAT_PNG) {
    idFile_Memory memoryFile;
    memoryFile.SetReadOnlyData(
        const_cast<char*>(static_cast<const char*>(parms->inputBuffer)),
        parms->inputSize);
    if (R_LoadPNGFromFile(
            &memoryFile, reinterpret_cast<std::uint8_t**>(&output.imageData),
            reinterpret_cast<unsigned*>(&output.imageWidth),
            reinterpret_cast<unsigned*>(&output.imageHeight), nullptr)) {
      output.result = true;
    }
  }
}

void idBackgroundImageLoader::SetImage(const char* imagePath) {
  nextImage = imagePath;
}

idBackgroundImageLoader::idBackgroundImageLoader(const char* imageName_)
    : nextImage(), currentImage() {
  imageName = imageName_;
  fileHandle = nullptr;
  jobParms.output = nullptr;
  dataBuffer = nullptr;
  dataSize = 0;
  state = 0;
  RegisterJob(reinterpret_cast<jobRun_t>(DecodeImageJob), "DecodeImageJob");
  jobList = parallelJobManager->AllocJobList(
      JOBLIST_UTILITY, JOBLIST_PRIORITY_LOW, 4, 4, &idColor::colorRed);
}

bool idBackgroundImageLoader::StartRead(const char* textureName) {
  std::int64_t offset = 0;

  if (idLib::production != PROD_PRODUCTION &&
      idLib::production != PROD_LOADED) {
    fileHandle = fileSystem->OpenFileRead(textureName, true, false);
    if (fileHandle == nullptr) {
      return false;
    }

    dataSize = static_cast<std::uint64_t>(fileHandle->Length());
    dataBuffer = idMem::Singleton()->AllocWithLocation(
        "w:\\build\\tech5\\engine\\framework\\BackgroundImageLoader.cpp(277) : TAG_IMAGE",
        dataSize, TAG_IMAGE, false, ALIGN_16, HEAP_DEFAULTHEAP);
    streamFileCache->UncachedBackgroundRead(
        fileHandle, offset, static_cast<int>(dataSize), &dataBuffer, TAG_IMAGE,
        &dataComplete);
    streamFileCache->SignalWork();
    return true;
  }

  std::uint32_t streamOffset = 0;
  std::uint64_t compressedSize = 0;
  std::uint64_t uncompressedSize = 0;
  std::uint8_t fileId = 0;
  if (resourceManager->GetCacheFileInfo(
          textureName, &offset, &compressedSize, &uncompressedSize,
          &streamOffset, &fileId, true)) {
    fileHandle = resourceManager->GetCacheFile(fileId);
  }
  if (fileHandle == nullptr) {
    return false;
  }

  dataSize = uncompressedSize;
  dataBuffer = idMem::Singleton()->AllocWithLocation(
      "w:\\build\\tech5\\engine\\framework\\BackgroundImageLoader.cpp(265) : TAG_IMAGE",
      dataSize, TAG_IMAGE, false, ALIGN_16, HEAP_DEFAULTHEAP);
  streamFileCache->UncachedBackgroundRead(
      fileHandle, offset, static_cast<int>(dataSize), &dataBuffer, TAG_IMAGE,
      &dataComplete);
  streamFileCache->SignalWork();
  fileHandle = nullptr;
  return true;
}

bool idBackgroundImageLoader::Update() {
  bool updatedImage = false;
  int oldState;
  do {
    oldState = state;
    switch (state) {
      case 0:
        if (!nextImage.IsEmpty()) {
          dataComplete = false;
          if (StartRead(nextImage.data)) {
            currentImage = nextImage;
            state = 1;
          }
          nextImage.Empty();
        }
        break;

      case 1:
        if (dataComplete) {
          if (!nextImage.IsEmpty()) {
            state = 3;
            break;
          }

          jobParms.inputBuffer = dataBuffer;
          jobParms.inputSize = dataSize;
          jobParms.format = FORMAT_UNKNOWN;
          jobParms.output = &jobOutput;
          if (currentImage.EndsWith(".dct")) {
            jobParms.format = FORMAT_DCT;
          } else if (currentImage.EndsWith(".png")) {
            jobParms.format = FORMAT_PNG;
          }
          if (jobParms.format == FORMAT_UNKNOWN) {
            state = 3;
            break;
          }

          jobList->AddJob(reinterpret_cast<jobRun_t>(DecodeImageJob), this);
          jobList->Submit(nullptr, -3);
          state = 2;
        }
        break;

      case 2:
        if (jobList->TryWait()) {
          if (jobParms.output->result) {
            idImageOpts options{};
            options.textureType = TT_2D;
            options.format = FMT_RGBA8;
            options.width = jobParms.output->imageWidth;
            options.height = jobParms.output->imageHeight;
            options.numLevels = 1;
            options.packedTail = true;
            options.linear = true;
            options.textureBindType = TB_SHADER;
            options.textureUsage = TU_DEFAULT;
            options.textureOptions = TO_NONE;
            options.sampler.filter = TF_DEFAULT;
            options.sampler.repeat = TR_REPEAT;
            options.sampler.lodMaxClamp = 13.0f;

            idImage* image = globalImages->ScratchImage(imageName, options);
            if (image != nullptr) {
              image->SubImageUpload(0, 0, 0, 0,
                                    jobParms.output->imageWidth,
                                    jobParms.output->imageHeight,
                                    jobParms.output->imageData, 0);
            }
          }
          updatedImage = true;
          state = 3;
        }
        break;

      case 3:
        if (fileHandle != nullptr) {
          delete fileHandle;
          fileHandle = nullptr;
        }
        if (dataBuffer != nullptr) {
          idMem::Singleton()->Free(dataBuffer, ALIGN_16);
          dataBuffer = nullptr;
        }
        if (jobParms.output->imageData != nullptr) {
          idMem::Singleton()->Free(jobParms.output->imageData, ALIGN_16);
          jobParms.output->imageData = nullptr;
        }
        state = 0;
        break;

      default:
        break;
    }
  } while (state != oldState);

  return updatedImage;
}

void idBackgroundImageLoader::Shutdown() {
  nextImage.Empty();
  while (!IsReady()) {
    Update();
    if (!IsReady()) {
      Sys_Sleep(10);
    }
  }

  idImageOpts options{};
  options.textureType = TT_2D;
  options.format = FMT_RGBA8;
  options.width = 1;
  options.height = 1;
  options.numLevels = 1;
  options.packedTail = true;
  options.textureBindType = TB_SHADER;
  options.textureUsage = TU_DEFAULT;
  options.textureOptions = TO_NONE;
  options.sampler.filter = TF_DEFAULT;
  options.sampler.repeat = TR_REPEAT;
  options.sampler.lodMaxClamp = 13.0f;

  idImage* image = globalImages->ScratchImage(imageName, options);
  if (image != nullptr) {
    const std::uint32_t blackPixel = 0;
    image->SubImageUpload(0, 0, 0, 0, 1, 1, &blackPixel, 0);
  }
}

idBackgroundImageLoader::~idBackgroundImageLoader() {
  Shutdown();
  parallelJobManager->FreeJobList(jobList);
}
