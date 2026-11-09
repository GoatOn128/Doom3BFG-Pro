#include "engine/renderer/imagetools/image_files.h"

#include "engine/renderer/imagetools/image_process.h"
#include "engine/renderer/imagetools/image_program.h"
#include "engine/renderer/jobs/transcode/image/baredct/baredct.h"
#include "shared/idlib/csystems/cvarsystemlocal.h"
#include "shared/idlib/filesystem/file_buffered.h"
#include "shared/idlib/filesystem/filesystemlocal.h"
#include "shared/idlib/lib.h"
#include "shared/idlib/sys/sys_alloc.h"

#include <png.h>

#include <algorithm>
#include <array>
#include <cstddef>
#include <cstdint>
#include <cstring>
#include <string>
#include <vector>

idCVar image_screenshotQuality(
    "image_screenshotQuality", "75", CVAR_ARCHIVE | CVAR_INTEGER,
    "sets the compression quality for jpeg screenshots, 100 is best quality");
idCVar image_watermark("image_watermark", "", CVAR_ALL,
                       "places a watermark each image with a watermark");
idCVar image_pngCompressionLevel(
    "image_pngCompressionLevel", "1", CVAR_ARCHIVE | CVAR_INTEGER,
    "CompressionLevel for png [0-9], or -1 for default");

const char* imageExtensions[] = {"tga", "png", "bmp", "dct", nullptr};

namespace {

void* Allocate(std::size_t size, memTag_t tag, const char* location) {
  return idMem::Singleton()->AllocWithLocation(location, size, tag, false,
                                                ALIGN_16, HEAP_DEFAULTHEAP);
}

void Free(void* pointer) {
  if (pointer != nullptr)
    idMem::Singleton()->Free(pointer, ALIGN_16);
}

std::uint16_t ReadU16(const std::uint8_t* data) {
  return static_cast<std::uint16_t>(data[0] | (data[1] << 8));
}

std::uint32_t ReadU32(const std::uint8_t* data) {
  return static_cast<std::uint32_t>(data[0] | (data[1] << 8) |
                                    (data[2] << 16) | (data[3] << 24));
}

void WriteU16(std::uint8_t* data, unsigned int value) {
  data[0] = static_cast<std::uint8_t>(value);
  data[1] = static_cast<std::uint8_t>(value >> 8);
}

void SetFileProps(const char* name, idFileProps* properties) {
  if (properties == nullptr)
    return;
  if (!fileSystem->FileExists(name)) {
    properties->timestamp = static_cast<std::uint32_t>(-1);
    properties->writable = false;
    return;
  }
  properties->timestamp = fileSystem->GetTimestamp(name);
  idFileLocal file(fileSystem->OpenFileRead(name));
  properties->writable = file.file != nullptr && file->IsWritable();
}

bool ReadWholeFile(const char* name, std::vector<std::uint8_t>& bytes,
                   idFileProps* properties) {
  if (!fileSystem->ReadFile(name, bytes)) {
    if (properties != nullptr) {
      properties->timestamp = static_cast<std::uint32_t>(-1);
      properties->writable = false;
    }
    return false;
  }
  SetFileProps(name, properties);
  return true;
}

std::string Extension(const char* name) {
  const char* dot = std::strrchr(name, '.');
  if (dot == nullptr || dot[1] == '\0')
    return {};
  std::string extension(dot + 1);
  std::transform(extension.begin(), extension.end(), extension.begin(),
                 [](unsigned char value) {
                   return static_cast<char>(idStr::ToLower(value));
                 });
  return extension;
}

std::string ResolveImageName(const char* input) {
  if (!Extension(input).empty())
    return input;
  idScopedDontTrack scope;
  for (const auto* extension = imageExtensions; *extension != nullptr;
       ++extension) {
    std::string candidate(input);
    candidate += '.';
    candidate += *extension;
    if (fileSystem->FileExists(candidate.c_str()))
      return candidate;
  }
  return {};
}

bool ReadTargaHeader(idFile* file, std::uint8_t header[18]) {
  return file != nullptr && file->Read(header, 18) == 18;
}

void ConfigureDct(idBareDctBase& codec) {
  codec.SetQuality_SSE2(image_screenshotQuality.GetInteger(),
                        image_screenshotQuality.GetInteger(),
                        image_screenshotQuality.GetInteger());
}

void ConfigureDct(idBareDctEncoder& codec) {
  codec.SetQuality_Generic(image_screenshotQuality.GetInteger(),
                           image_screenshotQuality.GetInteger(),
                           image_screenshotQuality.GetInteger());
}

}  // namespace

idScopedDontTrack::idScopedDontTrack() : oldDontTrack(idLib::dontTrack) {
  idLib::dontTrack = true;
}

idScopedDontTrack::~idScopedDontTrack() { idLib::dontTrack = oldDontTrack; }

void user_read_data(png_struct_def* pngPointer, std::uint8_t* data,
                    std::uint64_t length) {
  static_cast<idFile*>(png_get_io_ptr(pngPointer))->Read(data, length);
}

void R_ReorganizeCubeImages(std::uint8_t** pictures, int size) {
  R_RotatePic(pictures[0], size);
  R_RotatePic(pictures[1], size);
  R_HorizontalFlip(pictures[1], size, size);
  R_VerticalFlip(pictures[1], size, size);
  R_VerticalFlip(pictures[2], size, size);
  R_HorizontalFlip(pictures[3], size, size);
  R_RotatePic(pictures[4], size);
  R_RotatePic(pictures[5], size);
}

void R_WritePNGUserWrite(png_struct_def* pngPointer, std::uint8_t* data,
                         std::uint64_t size) {
  static_cast<idFile*>(png_get_io_ptr(pngPointer))->Write(data, size);
}

void R_WritePNGUserFlush(png_struct_def* pngPointer) {
  static_cast<idFile*>(png_get_io_ptr(pngPointer))->Flush();
}

void* R_WritePNGUserMemAlloc(png_struct_def*, std::uint64_t size) {
  return Allocate(size, TAG_IMAGE, "image_files.cpp:R_WritePNGUserMemAlloc");
}

void R_WritePNGUserMemFree(png_struct_def*, void* address) { Free(address); }

[[noreturn]] void R_WritePNGUserError(png_struct_def*, const char* message) {
  idLib::Error("R_WritePNG: %s", message);
}

void R_WritePNGUserWarning(png_struct_def*, const char* message) {
  idLib::Warning("R_WritePNG: %s", message);
}

void R_LoadBMP(const char* name, std::uint8_t** picture,
               unsigned int* width, unsigned int* height,
               idFileProps* fileProps) {
  if (picture == nullptr) {
    SetFileProps(name, fileProps);
    return;
  }
  *picture = nullptr;
  std::vector<std::uint8_t> fileData;
  if (!ReadWholeFile(name, fileData, fileProps))
    return;
  if (fileData.size() < 54)
    idLib::Error("LoadBMP: incomplete header (%s)\n", name);

  const std::uint32_t fileSize = ReadU32(fileData.data() + 2);
  const int bitmapWidth = static_cast<int>(ReadU32(fileData.data() + 18));
  const int signedHeight = static_cast<int>(ReadU32(fileData.data() + 22));
  const int bitmapHeight = signedHeight < 0 ? -signedHeight : signedHeight;
  const unsigned int bitsPerPixel = ReadU16(fileData.data() + 28);
  const std::uint32_t compression = ReadU32(fileData.data() + 30);
  if (fileData[0] != 'B' && fileData[1] != 'M')
    idLib::Error("LoadBMP: only Windows-style BMP files supported (%s)\n",
                 name);
  if (fileSize != fileData.size()) {
    idLib::Error(
        "LoadBMP: header size does not match file size (%d vs. %d) (%s)\n",
        fileSize, static_cast<int>(fileData.size()), name);
  }
  if (compression != 0)
    idLib::Error("LoadBMP: only uncompressed BMP files supported (%s)\n",
                 name);
  if (bitsPerPixel < 8)
    idLib::Error("LoadBMP: monochrome and 4-bit BMP files not supported (%s)\n",
                 name);
  if (width != nullptr)
    *width = bitmapWidth;
  if (height != nullptr)
    *height = bitmapHeight;

  const std::size_t pixelCount =
      static_cast<std::size_t>(bitmapWidth) * bitmapHeight;
  *picture = static_cast<std::uint8_t*>(
      Allocate(4 * pixelCount, TAG_IMAGE, "image_files.cpp:R_LoadBMP"));
  const std::size_t sourceOffset = bitsPerPixel == 8 ? 1078 : 54;
  const std::size_t sourceStride =
      (static_cast<std::size_t>(bitmapWidth) * bitsPerPixel / 8 + 3) & ~3ULL;
  if (sourceOffset + sourceStride * bitmapHeight > fileData.size())
    idLib::Error("LoadBMP: incomplete pixel data (%s)\n", name);

  const std::uint8_t* palette = fileData.data() + 54;
  for (int sourceRow = 0; sourceRow < bitmapHeight; ++sourceRow) {
    const std::uint8_t* source =
        fileData.data() + sourceOffset + sourceStride * sourceRow;
    std::uint8_t* destination =
        *picture + 4ULL * bitmapWidth * (bitmapHeight - 1 - sourceRow);
    for (int x = 0; x < bitmapWidth; ++x) {
      switch (bitsPerPixel) {
        case 8: {
          const std::uint8_t* entry = palette + 4 * *source++;
          destination[0] = entry[2];
          destination[1] = entry[1];
          destination[2] = entry[0];
          destination[3] = 255;
          break;
        }
        case 16: {
          const std::uint16_t value = ReadU16(source);
          source += 2;
          destination[0] = static_cast<std::uint8_t>((value >> 7) & 0xF8);
          destination[1] = static_cast<std::uint8_t>((value >> 2) & 0xF8);
          destination[2] = static_cast<std::uint8_t>((value << 3) & 0xF8);
          destination[3] = 255;
          break;
        }
        case 24:
          destination[0] = source[2];
          destination[1] = source[1];
          destination[2] = source[0];
          destination[3] = 255;
          source += 3;
          break;
        case 32:
          destination[0] = source[2];
          destination[1] = source[1];
          destination[2] = source[0];
          destination[3] = source[3];
          source += 4;
          break;
        default:
          idLib::Error("LoadBMP: illegal pixel_size '%d' in file '%s'\n",
                       bitsPerPixel, name);
      }
      destination += 4;
    }
  }
}

bool R_LoadTGA(const char* name, const std::uint8_t* buffer,
               std::uint64_t bufferSize, std::uint8_t** picture,
               int* width, int* height) {
  *picture = nullptr;
  if (buffer == nullptr || bufferSize < 18)
    return false;
  const int idLength = buffer[0];
  const int colorMapType = buffer[1];
  const int imageType = buffer[2];
  const int columns = ReadU16(buffer + 12);
  const int rows = ReadU16(buffer + 14);
  const int pixelSize = buffer[16];
  const int attributes = buffer[17];
  if (imageType != 2 && imageType != 10 && imageType != 3) {
    idLib::Warning(
        "LoadTGA( %s ): Only type 2 (RGB), 3 (gray), and 10 (RGB) TGA images supported",
        name);
    return false;
  }
  if (colorMapType != 0) {
    idLib::Warning("LoadTGA( %s ): colormaps not supported", name);
    return false;
  }
  if (pixelSize != 32 && pixelSize != 24 && imageType != 3) {
    idLib::Warning(
        "LoadTGA( %s ): Only 32 or 24 bit images supported (no colormaps)",
        name);
    return false;
  }
  const std::size_t dataOffset = 18ULL + idLength;
  if (dataOffset > bufferSize ||
      ((imageType == 2 || imageType == 3) &&
       static_cast<std::uint64_t>(columns) * rows * (pixelSize >> 3) >
           bufferSize - dataOffset)) {
    idLib::Warning("LoadTGA( %s ): incomplete file", name);
    return false;
  }
  if (width != nullptr)
    *width = columns;
  if (height != nullptr)
    *height = rows;
  *picture = static_cast<std::uint8_t*>(Allocate(
      4ULL * columns * rows, TAG_IMAGE, "image_files.cpp:R_LoadTGA"));
  const std::uint8_t* source = buffer + dataOffset;
  const std::uint8_t* end = buffer + bufferSize;
  int x = 0;
  int y = rows - 1;

  auto emit = [&](std::uint8_t blue, std::uint8_t green, std::uint8_t red,
                  std::uint8_t alpha) {
    std::uint8_t* destination = *picture + 4ULL * (y * columns + x);
    destination[0] = red;
    destination[1] = green;
    destination[2] = blue;
    destination[3] = alpha;
    if (++x == columns) {
      x = 0;
      --y;
    }
  };
  auto readPixel = [&](std::uint8_t& blue, std::uint8_t& green,
                       std::uint8_t& red, std::uint8_t& alpha) {
    const int bytes = imageType == 3 ? 1 : pixelSize >> 3;
    if (source + bytes > end)
      return false;
    if (imageType == 3 || pixelSize == 8) {
      red = green = blue = *source++;
      alpha = 255;
    } else {
      blue = *source++;
      green = *source++;
      red = *source++;
      alpha = pixelSize == 32 ? *source++ : 255;
    }
    return true;
  };

  const int totalPixels = columns * rows;
  int emitted = 0;
  if (imageType == 10) {
    while (emitted < totalPixels && source < end) {
      const std::uint8_t packet = *source++;
      const int count = (packet & 0x7F) + 1;
      if ((packet & 0x80) != 0) {
        std::uint8_t b, g, r, a;
        if (!readPixel(b, g, r, a))
          break;
        for (int index = 0; index < count && emitted < totalPixels; ++index) {
          emit(b, g, r, a);
          ++emitted;
        }
      } else {
        for (int index = 0; index < count && emitted < totalPixels; ++index) {
          std::uint8_t b, g, r, a;
          if (!readPixel(b, g, r, a))
            break;
          emit(b, g, r, a);
          ++emitted;
        }
      }
    }
  } else {
    while (emitted < totalPixels) {
      std::uint8_t b, g, r, a;
      if (!readPixel(b, g, r, a))
        break;
      emit(b, g, r, a);
      ++emitted;
    }
  }
  if (emitted != totalPixels) {
    idLib::Warning("LoadTGA( %s ): incomplete file", name);
    Free(*picture);
    *picture = nullptr;
    return false;
  }
  if ((attributes & 0x20) != 0)
    R_VerticalFlip(*picture, columns, rows);
  return true;
}

bool R_LoadTGA(const char* name, std::uint8_t** picture, int* width,
               int* height, idFileProps* fileProps) {
  if (picture == nullptr) {
    SetFileProps(name, fileProps);
    return true;
  }
  *picture = nullptr;
  std::vector<std::uint8_t> buffer;
  if (!ReadWholeFile(name, buffer, fileProps))
    return false;
  return R_LoadTGA(name, buffer.data(), buffer.size(), picture, width, height);
}

bool R_ReadDCTFromBuffer(const std::uint8_t* buffer, std::uint64_t bufferSize,
                         std::uint8_t* output, int* width, int* height) {
  if (buffer == nullptr)
    return false;
  idBareDCTHuffmanTable yDc(HUFFMAN_TYPE_Y_DC, false);
  idBareDCTHuffmanTable yAc(HUFFMAN_TYPE_Y_AC, false);
  idBareDCTHuffmanTable coCgDc(HUFFMAN_TYPE_COCG_DC, false);
  idBareDCTHuffmanTable coCgAc(HUFFMAN_TYPE_COCG_AC, false);
  idBareDctDecoder decoder;
  decoder.SetHuffmanTables(&yDc, &yAc, &coCgDc, &coCgAc, nullptr, nullptr);
  ConfigureDct(decoder);
  if (output != nullptr && width != nullptr && height != nullptr) {
    decoder.DecompressImageRGB_JPEG(buffer, output, *width, *height,
                                    static_cast<int>(bufferSize));
  } else {
    decoder.DecompressImageRGB_JPEG(buffer, nullptr, 0, 0,
                                    static_cast<int>(bufferSize));
    if (width != nullptr)
      *width = decoder.imageWidth;
    if (height != nullptr)
      *height = decoder.imageHeight;
  }
  return true;
}

bool R_LoadDCT(const char* name, std::uint8_t** picture, int* width,
               int* height, idFileProps* fileProps) {
  if (picture == nullptr) {
    SetFileProps(name, fileProps);
    return true;
  }
  *picture = nullptr;
  *width = 0;
  *height = 0;
  std::vector<std::uint8_t> buffer;
  if (!ReadWholeFile(name, buffer, fileProps))
    return false;
  idBareDCTHuffmanTable yDc(HUFFMAN_TYPE_Y_DC, false);
  idBareDCTHuffmanTable yAc(HUFFMAN_TYPE_Y_AC, false);
  idBareDCTHuffmanTable coCgDc(HUFFMAN_TYPE_COCG_DC, false);
  idBareDCTHuffmanTable coCgAc(HUFFMAN_TYPE_COCG_AC, false);
  idBareDctDecoder decoder;
  decoder.SetHuffmanTables(&yDc, &yAc, &coCgDc, &coCgAc, nullptr, nullptr);
  ConfigureDct(decoder);
  if (!decoder.DecompressImageRGB_JPEG(
          buffer.data(), nullptr, 0, 0, static_cast<int>(buffer.size()))) {
    return false;
  }
  *width = decoder.imageWidth;
  *height = decoder.imageHeight;
  *picture = static_cast<std::uint8_t*>(Allocate(
      4ULL * *width * *height, TAG_IMAGE, "image_files.cpp:R_LoadDCT"));
  if (!decoder.DecompressImageRGB_JPEG(buffer.data(), *picture, *width,
                                       *height,
                                       static_cast<int>(buffer.size()))) {
    Free(*picture);
    *picture = nullptr;
    *width = *height = 0;
    return false;
  }
  return true;
}

bool R_LoadCubeImages(const char* imageName, std::uint8_t** pictures,
                      int* outputSize, idFileProps* fileProps) {
  static constexpr const char* cameraSides[6] = {
      "_forward.tga", "_back.tga", "_left.tga",
      "_right.tga",   "_up.tga",   "_down.tga"};
  static constexpr const char* axisSides[6] = {
      "_px.tga", "_nx.tga", "_py.tga", "_ny.tga", "_pz.tga", "_nz.tga"};
  if (pictures != nullptr)
    std::fill(pictures, pictures + 6, nullptr);
  if (fileProps != nullptr) {
    fileProps->timestamp = static_cast<std::uint32_t>(-1);
    fileProps->writable = false;
  }
  if (outputSize != nullptr)
    *outputSize = 0;
  const char* marker = std::strstr(imageName, "_forward");
  const char* const* sides = cameraSides;
  if (marker == nullptr) {
    marker = std::strstr(imageName, "_px");
    sides = axisSides;
  }
  if (marker == nullptr) {
    idLib::Warning("Cube map file doesn't have _forward or _px: %s",
                   imageName);
    return false;
  }
  const std::size_t baseLength = marker - imageName;
  int size = 0;
  int loaded = 0;
  for (; loaded < 6; ++loaded) {
    std::string fullName(imageName, baseLength);
    fullName += sides[loaded];
    int width = 0;
    int height = 0;
    idFileProps current{static_cast<std::uint32_t>(-1), false};
    R_LoadImageProgram(fullName.c_str(),
                       pictures != nullptr ? &pictures[loaded] : nullptr,
                       (pictures != nullptr || outputSize != nullptr) ? &width
                                                                      : nullptr,
                       (pictures != nullptr || outputSize != nullptr) ? &height
                                                                      : nullptr,
                       &current);
    if (current.timestamp == static_cast<std::uint32_t>(-1)) {
      idLib::Warning("Cube map file not found: %s", fullName.c_str());
      break;
    }
    if (pictures != nullptr || outputSize != nullptr) {
      if (loaded == 0)
        size = width;
      if (width != size || height != size) {
        idLib::Warning("Mismatched sizes on cube map '%s'", imageName);
        break;
      }
    }
    if (fileProps != nullptr) {
      if (fileProps->timestamp == static_cast<std::uint32_t>(-1) ||
          current.timestamp > fileProps->timestamp) {
        fileProps->timestamp = current.timestamp;
      }
      fileProps->writable = fileProps->writable || current.writable;
    }
  }
  if (loaded == 6) {
    if (pictures != nullptr && sides == cameraSides)
      R_ReorganizeCubeImages(pictures, size);
    if (outputSize != nullptr)
      *outputSize = size;
    return true;
  }
  if (pictures != nullptr) {
    for (int index = 0; index < loaded; ++index)
      Free(pictures[index]);
  }
  if (fileProps != nullptr) {
    fileProps->timestamp = static_cast<std::uint32_t>(-1);
    fileProps->writable = false;
  }
  return false;
}

bool R_LoadPNGFromFile(idFile* file, std::uint8_t** picture,
                       unsigned int* width, unsigned int* height,
                       idFileProps* fileProps) {
  if (picture != nullptr)
    *picture = nullptr;
  if (width != nullptr)
    *width = 0;
  if (height != nullptr)
    *height = 0;
  if (fileProps != nullptr) {
    fileProps->timestamp = file->Timestamp();
    fileProps->writable = file->IsWritable();
  }
  if (picture == nullptr && width == nullptr && height == nullptr)
    return true;
  std::uint8_t signature[8];
  if (file->Read(signature, sizeof(signature)) != sizeof(signature) ||
      png_sig_cmp(signature, 0, sizeof(signature)) != 0) {
    idLib::Warning("%s is not a png file.", file->GetName());
    return false;
  }
  png_structp pngPointer = png_create_read_struct(PNG_LIBPNG_VER_STRING, nullptr,
                                                   nullptr, nullptr);
  if (pngPointer == nullptr)
    return false;
  png_infop infoPointer = png_create_info_struct(pngPointer);
  if (infoPointer == nullptr) {
    png_destroy_read_struct(&pngPointer, nullptr, nullptr);
    return false;
  }
  png_set_sig_bytes(pngPointer, 8);
  png_set_read_fn(pngPointer, file, user_read_data);
  png_read_info(pngPointer, infoPointer);
  png_uint_32 pngWidth = 0;
  png_uint_32 pngHeight = 0;
  int bitDepth = 0;
  int colorType = 0;
  int interlace = 0;
  int compression = 0;
  int filter = 0;
  png_get_IHDR(pngPointer, infoPointer, &pngWidth, &pngHeight, &bitDepth,
               &colorType, &interlace, &compression, &filter);
  if (width != nullptr)
    *width = pngWidth;
  if (height != nullptr)
    *height = pngHeight;
  if (picture != nullptr) {
    if (bitDepth == 16)
      png_set_strip_16(pngPointer);
    if (colorType == PNG_COLOR_TYPE_PALETTE)
      png_set_palette_to_rgb(pngPointer);
    if (colorType == PNG_COLOR_TYPE_RGB)
      png_set_filler(pngPointer, 0xFF, PNG_FILLER_AFTER);
    png_read_update_info(pngPointer, infoPointer);
    *picture = static_cast<std::uint8_t*>(Allocate(
        4ULL * pngWidth * pngHeight, TAG_IMAGE,
        "image_files.cpp:R_LoadPNGFromFile"));
    auto** rows = static_cast<std::uint8_t**>(Allocate(
        sizeof(std::uint8_t*) * pngHeight, TAG_TEMP,
        "image_files.cpp:R_LoadPNGFromFile:rows"));
    for (png_uint_32 row = 0; row < pngHeight; ++row)
      rows[row] = *picture + 4ULL * pngWidth * row;
    png_read_image(pngPointer, rows);
    Free(rows);
  }
  png_destroy_read_struct(&pngPointer, &infoPointer, nullptr);
  return true;
}

bool R_LoadPNG(const char* name, std::uint8_t** picture,
               unsigned int* width, unsigned int* height,
               idFileProps* fileProps) {
  idFileLocal file(fileSystem->OpenFileRead(name));
  return file.file != nullptr &&
         R_LoadPNGFromFile(file.file, picture, width, height, fileProps);
}

void R_LoadImageDimensions(const char* name, unsigned int* width,
                           unsigned int* height, idFileProps* fileProps) {
  if (width != nullptr && height != nullptr)
    *width = *height = 0;
  if (fileProps != nullptr) {
    fileProps->timestamp = static_cast<std::uint32_t>(-1);
    fileProps->writable = false;
  }
  std::string filename = ResolveImageName(name);
  if (filename.empty())
    return;
  if (filename.find(".png") != std::string::npos) {
    R_LoadPNG(filename.c_str(), nullptr, width, height, fileProps);
    return;
  }
  idFileLocal file(fileSystem->OpenFileRead(filename.c_str()));
  if (file.file == nullptr)
    return;
  if (fileProps != nullptr) {
    fileProps->timestamp = file->Timestamp();
    fileProps->writable = file->IsWritable();
  }
  if (filename.find(".bmp") != std::string::npos) {
    std::array<std::uint8_t, 1080> data{};
    file->Read(data.data(), data.size());
    if (width != nullptr)
      *width = ReadU32(data.data() + 18);
    if (height != nullptr)
      *height = ReadU32(data.data() + 22);
  } else if (filename.find(".tga") != std::string::npos) {
    std::uint8_t header[18]{};
    if (!ReadTargaHeader(file.file, header))
      return;
    const int imageType = header[2];
    const int pixelSize = header[16];
    if (imageType != 2 && imageType != 10 && imageType != 3) {
      idLib::Warning(
          "R_LoadImageDimensions( %s ): Only type 2 (RGB), 3 (gray), and 10 (RGB) TGA images supported",
          name);
    } else if (header[1] != 0) {
      idLib::Warning("R_LoadImageDimensions( %s ): colormaps not supported",
                     name);
    } else if (pixelSize != 32 && pixelSize != 24 && imageType != 3) {
      idLib::Warning(
          "R_LoadImageDimensions( %s ): Only 32 or 24 bit images supported (no colormaps)",
          name);
    } else {
      if (width != nullptr)
        *width = ReadU16(header + 12);
      if (height != nullptr)
        *height = ReadU16(header + 14);
    }
  }
}

void R_LoadImage(const char* name, std::uint8_t** picture,
                 unsigned int* width, unsigned int* height,
                 idFileProps* fileProps) {
  if (picture != nullptr)
    *picture = nullptr;
  if (fileProps != nullptr) {
    fileProps->timestamp = static_cast<std::uint32_t>(-1);
    fileProps->writable = false;
  }
  if (width != nullptr)
    *width = 0;
  if (height != nullptr)
    *height = 0;
  if (picture == nullptr && (width != nullptr || height != nullptr)) {
    R_LoadImageDimensions(name, width, height, fileProps);
    return;
  }
  std::string filename = ResolveImageName(name);
  if (filename.empty())
    return;
  std::transform(filename.begin(), filename.end(), filename.begin(),
                 [](unsigned char value) {
                   return static_cast<char>(idStr::ToLower(value));
                 });
  const std::string extension = Extension(filename.c_str());
  if (extension == "tga") {
    R_LoadTGA(filename.c_str(), picture, reinterpret_cast<int*>(width),
              reinterpret_cast<int*>(height), fileProps);
  } else if (extension == "bmp") {
    R_LoadBMP(filename.c_str(), picture, width, height, fileProps);
  } else if (extension == "png") {
    R_LoadPNG(filename.c_str(), picture, width, height, fileProps);
  } else if (extension == "dct") {
    R_LoadDCT(filename.c_str(), picture, reinterpret_cast<int*>(width),
              reinterpret_cast<int*>(height), fileProps);
  } else {
    idLib::Warning("Unknown image extension on: %s", filename.c_str());
  }
  if (((width != nullptr && static_cast<int>(*width) < 1) ||
       (height != nullptr && static_cast<int>(*height) < 1)) &&
      picture != nullptr && *picture != nullptr) {
    Free(*picture);
    *picture = nullptr;
  }
}

bool R_WritePNGToFile(idFile* file, std::uint8_t* data,
                      unsigned int width, unsigned int height,
                      bool flipVertical) {
  std::vector<std::uint8_t*> rows(height);
  png_structp pngPointer = png_create_write_struct_2(
      PNG_LIBPNG_VER_STRING, nullptr, R_WritePNGUserError,
      R_WritePNGUserWarning, nullptr, R_WritePNGUserMemAlloc,
      R_WritePNGUserMemFree);
  if (pngPointer == nullptr) {
    idLib::Warning(
        "[%s]: writing failed, couldn't create write struct",
        "R_WritePNGToFile");
    return false;
  }
  const int compressionLevel = image_pngCompressionLevel.GetInteger();
  if (compressionLevel != -1)
    png_set_compression_level(pngPointer,
                              std::clamp(compressionLevel, 0, 9));
  png_infop infoPointer = png_create_info_struct(pngPointer);
  if (infoPointer == nullptr) {
    png_destroy_write_struct(&pngPointer, nullptr);
    idLib::Warning("[%s]: writing failed, couldn't create info struct",
                   "R_WritePNGToFile");
    return false;
  }
  png_set_write_fn(pngPointer, file, R_WritePNGUserWrite,
                   R_WritePNGUserFlush);
  png_set_IHDR(pngPointer, infoPointer, width, height, 8,
               PNG_COLOR_TYPE_RGBA, PNG_INTERLACE_NONE,
               PNG_COMPRESSION_TYPE_DEFAULT, PNG_FILTER_TYPE_DEFAULT);
  png_write_info(pngPointer, infoPointer);
  for (unsigned int row = 0; row < height; ++row) {
    const unsigned int sourceRow = flipVertical ? height - 1 - row : row;
    rows[row] = data + 4ULL * width * sourceRow;
  }
  png_write_image(pngPointer, rows.data());
  png_write_end(pngPointer, infoPointer);
  png_destroy_write_struct(&pngPointer, &infoPointer);
  return true;
}

bool R_WritePNG(const char* filename, std::uint8_t* data,
                unsigned int width, unsigned int height, bool flipVertical,
                fsPath_t basePath) {
  idFile_Buffered file(0x8000);
  if (!file.OpenFileWrite(filename, basePath)) {
    idLib::Warning(
        "R_WritePNG: writing '%s' failed, couldn't open output file",
        filename);
    return false;
  }
  const bool wrote =
      R_WritePNGToFile(&file, data, width, height, flipVertical);
  return file.Finalize() && wrote;
}

bool R_WriteJPG(const char* filename, const std::uint8_t* data, int width,
                int height, fsPath_t, bool useMarkers) {
  if (width < 16 || height < 16) {
    idLib::Printf("R_WriteJPG( %s, %i, %i ) : can't write < 16x16 jpgs\n",
                  filename, width, height);
    return false;
  }
  idFileLocal file(fileSystem->OpenFileWrite(filename));
  if (file.file == nullptr) {
    idLib::Warning("R_WriteJPG: writing '%s' failed", filename);
    return false;
  }
  std::vector<std::uint8_t> output(4ULL * width * height);
  idBareDctEncoder encoder;
  ConfigureDct(encoder);
  int outputSize = 0;
  encoder.CompressImageRGB_JPEG(data, output.data(), width, height,
                                &outputSize, useMarkers);
  file->Write(output.data(), outputSize);
  file->Flush();
  return true;
}

bool R_WriteTGA(const char* filename, const std::uint8_t* data, int width,
                int height, int depth, bool flipVertical,
                bool convertToBGR, fsPath_t) {
  if (depth != 1 && depth != 3 && depth != 4) {
    idLib::Warning("R_WriteTGA: unsupported image depth: %d", depth);
    return false;
  }
  idFileLocal file(fileSystem->OpenFileWrite(filename));
  if (file.file == nullptr) {
    idLib::Warning("R_WriteTGA: creating memory file failed for %s",
                   filename);
    return false;
  }
  std::uint8_t header[18]{};
  header[2] = static_cast<std::uint8_t>((depth == 1) + 2);
  WriteU16(header + 12, width);
  WriteU16(header + 14, height);
  header[16] = static_cast<std::uint8_t>(8 * depth);
  if (!flipVertical)
    header[17] = 0x20;
  file->Write(header, sizeof(header));
  std::vector<std::uint8_t> row(static_cast<std::size_t>(width) * depth);
  for (int y = 0; y < height; ++y) {
    if (convertToBGR && (depth == 3 || depth == 4)) {
      for (int x = 0; x < width; ++x) {
        row[depth * x + 0] = data[depth * x + 2];
        row[depth * x + 1] = data[depth * x + 1];
        row[depth * x + 2] = data[depth * x + 0];
        if (depth == 4)
          row[depth * x + 3] = data[depth * x + 3];
      }
      file->Write(row.data(), row.size());
    } else {
      file->Write(data, static_cast<std::size_t>(width) * depth);
    }
    data += static_cast<std::size_t>(width) * depth;
  }
  return true;
}
