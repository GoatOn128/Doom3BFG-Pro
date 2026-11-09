#include "engine/renderer/image.h"

#include "engine/framework/resourcemanager.h"
#include "engine/framework/streamfilecache.h"
#include "engine/renderer/binaryimage.h"
#include "engine/renderer/declrenderprog.h"
#include "engine/renderer/gl_api.h"
#include "engine/renderer/imagedata.h"
#include "engine/renderer/imagetools/image_files.h"
#include "engine/renderer/imagetools/image_program.h"
#include "engine/renderer/jobs/render/graphicsapiwrapper.h"
#include "engine/renderer/texturesampler.h"
#include "shared/idlib/filesystem/filesystemlocal.h"
#include "shared/idlib/lib.h"
#include "shared/idlib/sys/sys_alloc.h"

#include <algorithm>
#include <cstddef>
#include <cstdint>
#include <cstring>
#include <new>

namespace {

struct imageOptName_t {
  const char* name;
  int length;
};

constexpr imageOptName_t imageOpts[] = {
    {"alpha", 5},
    {"borderclamp", 11},
    {"clamp", 5},
    {"clamps", 6},
    {"clampt", 6},
    {"cubefilter", 10},
    {"float", 5},
    {"hqcompress", 10},
    {"hqcompressnormal", 16},
    {"intensity", 9},
    {"linear", 6},
    {"linearmipnearest", 16},
    {"luminance", 9},
    {"luminancealpha", 14},
    {"mipbias", 7},
    {"mirror", 6},
    {"nearest", 7},
    {"nearestmipnearest", 17},
    {"uncompressed", 12},
    {"ycocgdxt5", 9},
    {nullptr, 0},
};

} // namespace

idTypedResourceList<idImage> idImage::resourceList("image");

int BitsForFormat(textureFormat_t format) {
  switch (format) {
    case FMT_RGBA32F:
      return 128;
    case FMT_RGBA16F:
      return 64;
    case FMT_RGBA8:
    case FMT_ARGB8:
    case FMT_DEPTH:
    case FMT_DEPTH_STENCIL:
    case FMT_X32F:
    case FMT_Y16F_X16F:
    case FMT_Y16_X16:
      return 32;
    case FMT_ALPHA:
    case FMT_LUM8:
    case FMT_INT8:
    case FMT_DXT5:
      return 8;
    case FMT_L8A8:
    case FMT_RG8:
    case FMT_X16:
    case FMT_RGB565:
      return 16;
    case FMT_DXT1:
      return 4;
    default:
      idLib::Error("BitsForInternalFormat: Unknown format: %d\n", format);
  }
}

idImage::~idImage() { PurgeImage(); }

int idImage::BitsForInternalFormat() const { return BitsForFormat(opts.format); }

bool idImage::IsCompressed() const {
  return opts.format == FMT_DXT1 || opts.format == FMT_DXT5;
}

void idImage::ModifySampler(const samplerOptions_t& options) {
  std::memmove(&opts.sampler, &options, sizeof(opts.sampler));
  textureSamplerManager->DeleteSampler(textureSampler);
  textureSampler = textureSamplerManager->CreateSampler(&opts.sampler, nullptr);
  GL_ResetTextureState();
}

const char* idImage::SkipImageOpts(const char* imageName) {
  do {
    if (*imageName == '\0') {
      break;
    }
    const char* previous = imageName;
    for (const imageOptName_t* option = imageOpts; option->name != nullptr;
         ++option) {
      if (idStr::Icmpn(option->name, imageName, option->length) == 0 &&
          imageName[option->length] == ' ') {
        imageName += option->length + 1;
        break;
      }
    }
    if (previous == imageName) {
      break;
    }
  } while (true);
  return imageName;
}

idResourceList* idImage::GetResourceList() const { return &resourceList; }

idImageData::idImageData(int w, int h) : width(w), height(h) {
  idMem* memory = idMem::Singleton();
  const std::size_t byteCount = static_cast<std::size_t>(
      static_cast<std::int64_t>(w) * static_cast<std::int64_t>(h) * 4);
  data = static_cast<std::uint8_t*>(memory->AllocWithLocation(
      "w:\\build\\tech5\\engine\\renderer\\ImageData.h(18) : TAG_IMAGE",
      byteCount, TAG_IMAGE, true, ALIGN_16, HEAP_DEFAULTHEAP));
  floatData = nullptr;
}

void ComputeNumLevels(idImageOpts* options, const char* imageName) {
  if (options->numLevels != 0) {
    return;
  }

  const int widthValue = options->width;
  const int heightValue = options->height;
  if ((*imageName != '_' &&
       ((((widthValue - 1) & widthValue) != 0 || widthValue <= 0) ||
        (((heightValue - 1) & heightValue) != 0 || heightValue <= 0))) ||
      options->sampler.filter < TF_NEAREST_MIPMAP_NEAREST) {
    options->numLevels = 1;
    return;
  }

  int width = options->width;
  int height = options->height;
  int depth = options->depth;
  options->numLevels = 1;
  while (width > 1 || height > 1 || depth > 1) {
    const textureFormat_t format = options->format;
    width >>= 1;
    height >>= 1;
    depth >>= 1;
    if ((format == FMT_DXT1 || format == FMT_DXT5) &&
        ((width & 3) != 0 || (height & 3) != 0)) {
      break;
    }
    ++options->numLevels;
  }
}

idImage::idImage() {
  opts = idImageOpts{};
  opts.packedTail = true;
  opts.sampler.filter = TF_DEFAULT;
  opts.sampler.repeat = TR_REPEAT;
  opts.sampler.lodMaxClamp = 13.0F;
  sourceFileTime = ~0U;
  binaryFileTime = ~0U;
  backgroundReadData = nullptr;
  backgroundReadComplete = true;
  frameUsed = 0;
  bindCount = 0;
  optsHasBeenSet = false;
  defaulted = false;
  allocationFailed = false;
  physicalMemoryBytes = 0;
  imageBuffer = nullptr;
  textureSampler = nullptr;
  oglTexture = ~0U;
  internalFormat = 3;
  dataFormat = 0x1908;
  dataType = 0x1401;
  target = 0x0DE1;
  bindTarget = 0x0DE1;
  uploadTarget = 0x0DE1;
  std::memset(buffers, 0, sizeof(buffers));
  std::memset(pitches, 0, sizeof(pitches));
  std::memset(tiling, 0, sizeof(tiling));
}

void idImage::DeriveNumLevels() { ComputeNumLevels(&opts, name.str); }

void idImage::LoadFromBinaryImage(const idBinaryImage& binaryImage) {
  PurgeImage();
  sourceFileTime = binaryImage.sourceFileTime;
  binaryFileTime = binaryImage.binaryFileTime;
  std::memmove(&opts, &binaryImage.opts, sizeof(opts));
  AllocImage(opts);
  for (int index = 0; index < binaryImage.images.Num(); ++index) {
    const idBinaryImage::idBinaryImageData& image = binaryImage.images[index];
    SubImageUpload(image.level, 0, 0, image.destZ, image.width, image.height,
                   image.data.list, 0);
  }
}

int idImage::StorageSize() const {
  if (oglTexture == ~0U) {
    return 0;
  }

  int pixels;
  if (opts.textureType == TT_3D) {
    pixels = opts.width * opts.height * opts.depth;
  } else if (opts.textureType == TT_CUBIC) {
    pixels = 6 * opts.width * opts.width;
  } else {
    pixels = opts.width * opts.height;
  }

  std::int64_t levelBytes =
      static_cast<std::int64_t>(pixels) * BitsForFormat(opts.format) / 8;
  unsigned int result = 0;
  unsigned int levels = opts.numLevels > 0
                            ? static_cast<unsigned int>(opts.numLevels)
                            : 1U;
  do {
    result += static_cast<unsigned int>(levelBytes);
    levelBytes >>= 2;
  } while (--levels != 0);
  return static_cast<int>(result);
}

void idImage::Print() {
  if (oglTexture == ~0U) {
    idLib::Printf("<PURGED> %s %s\n", name.str,
                  opts.overlayMemory ? "(OVERLAY)" : "");
    return;
  }

  if (loadedResource) {
    idLib::Printf(defaulted ? "D" : " ");
  } else {
    idLib::Printf("S");
  }

  switch (opts.textureType) {
    case TT_2D: idLib::Printf(" "); break;
    case TT_3D: idLib::Printf("3"); break;
    case TT_CUBIC: idLib::Printf("C"); break;
    default: idLib::Printf("<BAD TYPE:%i>", opts.textureType); break;
  }
  idLib::Printf("%4i %4i %2i ", opts.width, opts.height, opts.numLevels);

  switch (opts.sampler.filter) {
    case TF_LINEAR: idLib::Printf("linr "); break;
    case TF_NEAREST: idLib::Printf("nrst "); break;
    case TF_NEAREST_MIPMAP_NEAREST: idLib::Printf("nmn  "); break;
    case TF_LINEAR_MIPMAP_NEAREST: idLib::Printf("lmn  "); break;
    case TF_TRILINEAR: idLib::Printf("xani "); break;
    case TF_DEFAULT: idLib::Printf("     "); break;
    default: idLib::Printf("<BAD FILTER:%i>", opts.sampler.filter); break;
  }

  switch (opts.format) {
    case FMT_RGBA32F: idLib::Printf("RGBA32   "); break;
    case FMT_RGBA16F: idLib::Printf("RGBA16   "); break;
    case FMT_RGBA8: idLib::Printf("RGBA8    "); break;
    case FMT_ARGB8: idLib::Printf("ARGB8    "); break;
    case FMT_ALPHA: idLib::Printf("A        "); break;
    case FMT_L8A8: idLib::Printf("LA       "); break;
    case FMT_RG8: idLib::Printf("RG       "); break;
    case FMT_LUM8: idLib::Printf("L        "); break;
    case FMT_INT8: idLib::Printf("I        "); break;
    case FMT_DXT1: idLib::Printf("DXT1     "); break;
    case FMT_DXT5: idLib::Printf("DXT5     "); break;
    case FMT_DEPTH: idLib::Printf("D24      "); break;
    case FMT_DEPTH_STENCIL: idLib::Printf("D24S8    "); break;
    case FMT_X32F: idLib::Printf("X32      "); break;
    case FMT_Y16F_X16F: idLib::Printf("Y16F_X16F"); break;
    case FMT_X16: idLib::Printf("X16      "); break;
    case FMT_Y16_X16: idLib::Printf("Y16_X16  "); break;
    case FMT_RGB565: idLib::Printf("RGB565   "); break;
    default: idLib::Printf("<BAD FORMAT:%i>", opts.format); break;
  }

  switch (opts.sampler.repeat) {
    case TR_REPEAT: idLib::Printf("rept"); break;
    case TR_CLAMP: idLib::Printf("clmp"); break;
    case TR_CLAMP_TO_BORDER: idLib::Printf("brdr"); break;
    case TR_MIRROR: idLib::Printf("mirr"); break;
    default: idLib::Printf("<BAD REPEAT:%i>", opts.sampler.repeat); break;
  }
  idLib::Printf("%6ik ", StorageSize() / 1024);
  if (opts.overlayMemory) {
    idLib::Printf("(OVERLAY)");
  }
  if (IsMapped()) {
    idLib::Printf("(DIRECT ACCESS)");
  }
  idLib::Printf("%s\n", name.str);
}

void idImage::PrintCollated() const {
  if (oglTexture == ~0U) {
    idLib::Printf("<PURGED>,%s\n", name.str);
  } else {
    idLib::Printf("%6i,", StorageSize() / 1024);
    idLib::Printf("%s\n", name.str);
  }
}

idImageData* idImage::GetImageData(int mipLevel, int z) const {
  const int width = std::max(opts.width >> mipLevel, 1);
  const int height = std::max(opts.height >> mipLevel, 1);
  idMem* memory = idMem::Singleton();
  void* storage = memory->AllocWithLocation(
      "w:\\build\\tech5\\shared\\idlib\\Heap.h(95) : tag",
      sizeof(idImageData), TAG_IMAGE, false, ALIGN_16, HEAP_DEFAULTHEAP);
  idImageData* imageData = storage != nullptr
                               ? new (storage) idImageData(width, height)
                               : nullptr;
  idDeclRenderProg::BindTexture(0, this, nullptr);
  GL_CheckErrors(false);
  if (opts.textureType == TT_CUBIC) {
    glGetTexImage(0x8515U + static_cast<unsigned int>(z), mipLevel, 0x1908U,
                  0x1401U, imageData->data);
  } else if (opts.textureType != TT_3D) {
    glGetTexImage(0x0DE1U, mipLevel, 0x1908U, 0x1401U, imageData->data);
  }
  GL_CheckErrors(false);
  return imageData;
}

void idImage::MakeGeneratedName(char* binaryFileName, int maxLength,
                                const char* imageName) {
  if (std::strstr(imageName, ".bimage") != nullptr) {
    const std::size_t count = static_cast<std::size_t>(maxLength - 1);
    std::memset(binaryFileName, 0, count);
    std::memcpy(binaryFileName, imageName,
                std::min(count, std::strlen(imageName)));
    return;
  }

  idStr strippedName(imageName);
  idStr fileExtension;
  strippedName.ExtractFileExtension(fileExtension);
  for (int index = 0; imageExtensions[index] != nullptr; ++index) {
    if (fileExtension.Cmp(imageExtensions[index]) == 0) {
      char extension[32];
      idStr::snPrintf(extension, sizeof(extension), ".%s",
                      imageExtensions[index]);
      strippedName.StripTrailingOnce(extension);
      break;
    }
  }

  if (idStr::Cmpn(imageName, "generated", 9) == 0) {
    strippedName += ".bimage";
    const std::size_t count = static_cast<std::size_t>(maxLength - 1);
    std::memset(binaryFileName, 0, count);
    std::memcpy(binaryFileName, strippedName.data,
                std::min(count, std::strlen(strippedName.data)));
    return;
  }

  char sanitizedName[256];
  char* output = sanitizedName;
  const char* input = strippedName.data;
  std::int64_t count = 0;
  const int limit = std::min(maxLength, 256) - 91;
  bool replaced = false;
  while (*input != '\0') {
    if (count > limit) {
      output += idStr::snPrintf(output, 10, "_%8x", idStr::IHash(input));
      break;
    }
    const unsigned char value = static_cast<unsigned char>(*input);
    if ((value >= 'a' && value <= 'z') ||
        (value >= 'A' && value <= 'Z') ||
        (value >= '0' && value <= '9') || value == '/' || value == '\\' ||
        value == '-') {
      replaced = false;
      *output++ = static_cast<char>(value);
      ++count;
    } else if (!replaced) {
      replaced = true;
      *output++ = '_';
      ++count;
    }
    ++input;
  }
  *output = '\0';
  fileSystem->FixLongFilename("generated/textures/", "bimage", sanitizedName,
                              binaryFileName, maxLength);
}

imageState_t idImage::ImageState() const {
  if (oglTexture == ~0U) {
    return IS_PURGED;
  }
  idImage* mutableThis = const_cast<idImage*>(this);
  if (backgroundReadData != nullptr && backgroundReadComplete) {
    SubImageUpload(0, 0, 0, 0, opts.width, opts.height, backgroundReadData, 0);
    GL_Flush();
    idMem::Singleton()->Free(backgroundReadData, ALIGN_16);
    mutableThis->backgroundReadData = nullptr;
  }
  return backgroundReadData != nullptr ? IS_BACKGROUND_READING : IS_READY;
}

void idImage::BackgroundRead(idFile* file, std::uint64_t offset) {
  if (oglTexture == ~0U) {
    AllocImage(opts);
  }
  backgroundReadComplete = false;

  int byteCount = 0;
  if (oglTexture != ~0U) {
    int pixels;
    if (opts.textureType == TT_3D) {
      pixels = opts.width * opts.height * opts.depth;
    } else if (opts.textureType == TT_CUBIC) {
      pixels = 6 * opts.width * opts.width;
    } else {
      pixels = opts.width * opts.height;
    }
    std::int64_t levelBytes =
        static_cast<std::int64_t>(pixels) * BitsForFormat(opts.format) / 8;
    int levels = opts.numLevels > 0 ? opts.numLevels : 1;
    while (levels-- > 0) {
      byteCount += static_cast<int>(levelBytes);
      levelBytes >>= 2;
    }
  }

  backgroundReadData = idMem::Singleton()->AllocWithLocation(
      "w:\\build\\tech5\\engine\\renderer\\Image.cpp(790) : TAG_IMAGE",
      static_cast<std::size_t>(byteCount), TAG_IMAGE, false, ALIGN_16,
      HEAP_DEFAULTHEAP);
  streamFileCache->UncachedBackgroundRead(
      file, static_cast<std::int64_t>(offset), byteCount, &backgroundReadData,
      TAG_IMAGE, &backgroundReadComplete);
  streamFileCache->SignalWork();
}

void idImage::WriteImageOptsStr(const idImageOpts& options, idStr& result) {
  if (result.Length() != 0) {
    result.Append(" ");
  }
  switch (options.sampler.filter) {
    case TF_LINEAR: result.Append("linear "); break;
    case TF_NEAREST: result.Append("nearest "); break;
    case TF_NEAREST_MIPMAP_NEAREST:
      result.Append("nearestMipNearest ");
      break;
    case TF_LINEAR_MIPMAP_NEAREST:
      result.Append("linearMipNearest ");
      break;
    default: break;
  }
  if (options.sampler.lodBias != 0.0F) {
    char bias[96];
    idStr::snPrintf(bias, sizeof(bias), " mipbias(%f) ",
                    options.sampler.lodBias);
    result.Append(bias);
  }
  switch (options.sampler.repeat) {
    case TR_CLAMP: result.Append("clamp "); break;
    case TR_CLAMP_S: result.Append("clampS "); break;
    case TR_CLAMP_T: result.Append("clampT "); break;
    case TR_CLAMP_TO_BORDER: result.Append("borderClamp "); break;
    case TR_MIRROR: result.Append("mirror "); break;
    default: break;
  }
  switch (options.format) {
    case FMT_RGBA16F: result.Append("float "); break;
    case FMT_RGBA8: result.Append("uncompressed "); break;
    case FMT_ALPHA: result.Append("alpha "); break;
    case FMT_L8A8: result.Append("luminanceAlpha "); break;
    case FMT_LUM8: result.Append("luminance "); break;
    case FMT_INT8: result.Append("intensity "); break;
    case FMT_DXT5:
      if (options.colorFormat == CFM_RGBA_HQ_DXT5) {
        result.Append("hqcompress ");
      } else if (options.colorFormat == CFM_NORMAL_HQ_DXT5) {
        result.Append("hqcompressNormal ");
      } else if (options.colorFormat == CFM_YCOCG_DXT5) {
        result.Append("YCoCgDXT5 ");
      }
      break;
    default: break;
  }
  if (options.cubeFilter) {
    result.Append("cubeFilter ");
  }
}

void idImage::ParseImageOpts(idParser& parser, idImageOpts& options) {
  idToken token;
  if (parser.EndOfFile()) {
    return;
  }
  while (parser.ReadTokenOnLine(token)) {
    if (token.Icmp("nearest") == 0) {
      options.sampler.filter = TF_NEAREST;
    } else if (token.Icmp("linear") == 0) {
      options.sampler.filter = TF_LINEAR;
    } else if (token.Icmp("linearMipNearest") == 0) {
      options.sampler.filter = TF_LINEAR_MIPMAP_NEAREST;
    } else if (token.Icmp("nearestMipNearest") == 0) {
      options.sampler.filter = TF_NEAREST_MIPMAP_NEAREST;
    } else if (token.Icmp("mipbias") == 0) {
      parser.ExpectTokenString("(");
      options.sampler.lodBias = parser.ParseFloat();
      parser.ExpectTokenString(")");
    } else if (token.Icmp("clamp") == 0) {
      options.sampler.repeat = TR_CLAMP;
    } else if (token.Icmp("borderClamp") == 0) {
      options.sampler.repeat = TR_CLAMP_TO_BORDER;
    } else if (token.Icmp("mirror") == 0) {
      options.sampler.repeat = TR_MIRROR;
    } else if (token.Icmp("clampS") == 0) {
      options.sampler.repeat = TR_CLAMP_S;
    } else if (token.Icmp("clampT") == 0) {
      options.sampler.repeat = TR_CLAMP_T;
    } else if (token.Icmp("uncompressed") == 0) {
      options.format = FMT_RGBA8;
    } else if (token.Icmp("YCoCgDXT5") == 0) {
      options.colorFormat = CFM_YCOCG_DXT5;
      options.format = FMT_DXT5;
    } else if (token.Icmp("hqcompress") == 0) {
      options.colorFormat = CFM_RGBA_HQ_DXT5;
      options.format = FMT_DXT5;
    } else if (token.Icmp("hqcompressNormal") == 0) {
      options.colorFormat = CFM_NORMAL_HQ_DXT5;
      options.format = FMT_DXT5;
    } else if (token.Icmp("LuminanceAlpha") == 0) {
      options.format = FMT_L8A8;
    } else if (token.Icmp("Luminance") == 0) {
      options.format = FMT_LUM8;
    } else if (token.Icmp("intensity") == 0) {
      options.format = FMT_INT8;
    } else if (token.Icmp("alpha") == 0) {
      options.format = FMT_ALPHA;
    } else if (token.Icmp("float") == 0) {
      options.format = FMT_RGBA16F;
    } else if (token.Icmp("cubeFilter") == 0) {
      options.cubeFilter = true;
    } else {
      parser.UnreadToken(token);
      break;
    }
    if (parser.EndOfFile()) {
      break;
    }
  }
}

void idImage::ParsePastImageLineAndMakeCanonical(idParser& parser,
                                                 idStr& result,
                                                 const char* overrideOptions) {
  idImageOpts options{};
  options.packedTail = true;
  options.sampler.filter = TF_DEFAULT;
  options.sampler.repeat = TR_REPEAT;
  options.sampler.lodMaxClamp = 13.0F;
  ParseImageOpts(parser, options);
  if (overrideOptions != nullptr && idStr::Length(overrideOptions) > 0) {
    idParser overrideParser(820);
    overrideParser.LoadMemory(overrideOptions, idStr::Length(overrideOptions),
                              overrideOptions);
    ParseImageOpts(overrideParser, options);
  }
  WriteImageOptsStr(options, result);
  idStr imageProgram;
  result += *R_ParsePastImageProgram(&imageProgram, &parser);
}

void idImage::ImageLineMakeCanonical(const char* source, idStr& result,
                                     const char* overrideOptions) {
  idParser parser(820);
  parser.LoadMemory(source, idStr::Length(source), source);
  ParsePastImageLineAndMakeCanonical(parser, result, overrideOptions);
}

bool idImage::ReloadIfStale() {
  if (!loadedResource) {
    return false;
  }

  char binaryFileName[256];
  MakeGeneratedName(binaryFileName, sizeof(binaryFileName), name.str);
  if (fileSystem->GetTimestamp(binaryFileName, false) != binaryFileTime) {
    LoadResource();
    return true;
  }
  if (std::strstr(name.str, ".bimage") != nullptr) {
    return false;
  }

  idParser parser(0);
  parser.LoadMemory(name.str, idStr::Length(name.str), name.str);
  parser.SetFlags(564);
  idImageOpts options{};
  options.packedTail = true;
  options.sampler.filter = TF_DEFAULT;
  options.sampler.repeat = TR_REPEAT;
  options.sampler.lodMaxClamp = 13.0F;
  ParseImageOpts(parser, options);

  idFileProps properties{~0U, false};
  if (options.textureType == TT_CUBIC ||
      std::strstr(name.str, "_forward") != nullptr ||
      std::strstr(name.str, "_px") != nullptr) {
    idToken token;
    parser.ReadToken(token);
    R_LoadCubeImages(token.data, nullptr, nullptr, &properties);
  } else {
    R_LoadImageProgram(&parser, nullptr, nullptr, nullptr, &properties);
  }

  if (properties.timestamp == sourceFileTime || properties.timestamp == ~0U) {
    return false;
  }
  fileSystem->RemoveFile(binaryFileName, FSPATH_BASE);
  LoadResource();
  return true;
}

template <>
idImage* idTypedResourceList<idImage>::Alloc(const char* resourceName) const {
  idMem* memory = idMem::Singleton();
  void* storage = memory->AllocWithLocation(
      "w:\\build\\tech5\\shared\\idlib\\Heap.h(95) : tag",
      sizeof(idImage), TAG_RESOURCE, false, ALIGN_16, HEAP_DEFAULTHEAP);
  idImage* image = storage != nullptr ? new (storage) idImage : nullptr;
  image->SetName(resourceName);
  return image;
}

void idImage::LoadResource() {
  if (name.str[0] == '_') {
    idImageOpts temporary{};
    temporary.textureType = TT_2D;
    temporary.format = FMT_RGBA8;
    temporary.colorFormat = CFM_DEFAULT;
    temporary.width = 8;
    temporary.height = 8;
    temporary.packedTail = true;
    temporary.textureBindType = TB_SHADER;
    temporary.textureUsage = TU_DEFAULT;
    temporary.textureOptions = TO_NONE;
    temporary.sampler.filter = TF_DEFAULT;
    temporary.sampler.repeat = TR_REPEAT;
    temporary.sampler.lodMaxClamp = 13.0F;
    AllocImage(temporary);
    loadedResource = false;
    return;
  }
  if (!loadedResource) {
    return;
  }

  PurgeImage();
  char binaryFileName[256];
  MakeGeneratedName(binaryFileName, sizeof(binaryFileName), name.str);
  idBinaryImage binaryImage(name.str);
  if (binaryImage.LoadFromGeneratedFile(binaryFileName)) {
    if (binaryImage.opts.width > 4096) {
      SetResourceError("Width > 4096 for idImage '%s'.", name.str);
      MakeDefault();
    } else if (binaryImage.opts.height > 4096) {
      SetResourceError("Height > 4096 for idImage '%s'.", name.str);
      MakeDefault();
    } else if (resourceManager->ExcludeImage(binaryFileName)) {
      MakeDefault();
    } else {
      binaryImage.opts.sampler.filter = TF_NEAREST;
      LoadFromBinaryImage(binaryImage);
    }
    return;
  }

  if (std::strstr(name.str, "bimage") != nullptr) {
    SetResourceError("Explicit binary file load failed for idImage %s.",
                     name.str);
    MakeDefault();
    return;
  }
  if (!binaryImage.LoadFromSourceFile()) {
    SetResourceError("Image source file load failed for idImage '%s'.",
                     name.str);
    MakeDefault();
    return;
  }

  binaryImage.WriteGeneratedFile(binaryFileName);
  if (!binaryImage.LoadFromGeneratedFile(binaryFileName)) {
    SetResourceError(
        "Failed to load after generating file for idImage '%s'.",
        binaryFileName);
    MakeDefault();
    return;
  }

  LoadFromBinaryImage(binaryImage);
  if (opts.width > 4096) {
    SetResourceError("Width > 4096 for idImage '%s.", name.str);
    MakeDefault();
  } else if (opts.height > 4096) {
    SetResourceError("Height > 4096 for idImage %s.", name.str);
    MakeDefault();
  }
}
