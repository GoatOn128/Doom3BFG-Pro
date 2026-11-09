#include "engine/renderer/imagetools/image_program.h"

#include "engine/renderer/image.h"
#include "engine/renderer/imagetools/image_files.h"
#include "engine/renderer/imagetools/image_process.h"
#include "shared/idlib/lib.h"
#include "shared/idlib/sys/sys_alloc.h"

#include <algorithm>
#include <cmath>
#include <cstddef>
#include <cstdint>
#include <cstdlib>
#include <cstring>

namespace {

char parseBuffer[256]{};
bool parseBufferError = false;
constexpr char imgNameBase[] = "";

void* AllocateImage(std::size_t size, const char* location) {
  return idMem::Singleton()->AllocWithLocation(
      location, size, TAG_IMAGE, false, ALIGN_16, HEAP_DEFAULTHEAP);
}

void FreeImage(void* pointer) {
  idMem::Singleton()->Free(pointer, ALIGN_16);
}

std::uint8_t EncodeNormal(float component) {
  return static_cast<std::uint8_t>(std::clamp(
      static_cast<int>((component + 1.0f) * 127.5f + 0.5f), 0, 255));
}

float ReciprocalLength(float x, float y, float z) {
  constexpr float smallestNormal = 1.17549435e-38f;
  return 1.0f /
         std::sqrt(std::max(x * x + y * y + z * z, smallestNormal));
}

int FloorPowerOfTwo(int value) {
  unsigned int bits = static_cast<unsigned int>(value);
  bits |= bits >> 1;
  bits |= bits >> 2;
  bits |= bits >> 4;
  bits |= bits >> 8;
  bits |= bits >> 16;
  return static_cast<int>((bits + 1) >> 1);
}

float TokenFloat(idToken& token) {
  if (token.type != TT_NUMBER)
    return 0.0f;
  if ((token.subtype & TT_VALUESVALID) == 0)
    token.NumberValue();
  return token.floatvalue;
}

int TokenInt(idToken& token) {
  if (token.type != TT_NUMBER)
    return 0;
  if ((token.subtype & TT_VALUESVALID) == 0)
    token.NumberValue();
  return static_cast<int>(token.intvalue);
}

void MergeFileProps(idFileProps* destination, const idFileProps& source) {
  if (destination == nullptr)
    return;
  if (destination->timestamp == static_cast<std::uint32_t>(-1) ||
      source.timestamp > destination->timestamp) {
    destination->timestamp = source.timestamp;
  }
  destination->writable = destination->writable || source.writable;
}

bool IsParseOnly(std::uint8_t** picture, int* width, int* height,
                 idFileProps* fileProps) {
  return picture == nullptr && width == nullptr && height == nullptr &&
         fileProps == nullptr;
}

bool ReadAndAppend(idParser* parser, idToken& token) {
  if (!parser->ReadToken(token))
    return false;
  AppendToken(&token);
  return true;
}

void FreePictures(idList<std::uint8_t*, 5>& pictures, int first) {
  for (int index = first; index < pictures.Num(); ++index) {
    if (pictures[index] != nullptr)
      FreeImage(pictures[index]);
  }
}

}  // namespace

void R_PatchAlpha(std::uint8_t* data1, int width1, int height1,
                  std::uint8_t* data2, int width2, int height2) {
  std::uint8_t* resized = nullptr;
  if (width1 != width2 || height1 != height2) {
    resized = R_Dropsample(data2, width2, height2,
                           static_cast<unsigned int>(width1), height1);
    data2 = resized;
  }
  const int pixels = width1 * height1;
  for (int pixel = 0; pixel < pixels; ++pixel)
    data1[4 * pixel + 3] = data2[4 * pixel];
  if (resized != nullptr)
    FreeImage(resized);
}

void R_ImageAdd(std::uint8_t* data1, int width1, int height1,
                std::uint8_t* data2, int width2, int height2) {
  std::uint8_t* resized = nullptr;
  if (width1 != width2 || height1 != height2) {
    resized = R_Dropsample(data2, width2, height2,
                           static_cast<unsigned int>(width1), height1);
    data2 = resized;
  }
  const int byteCount = 4 * width1 * height1;
  for (int index = 0; index < byteCount; ++index)
    data1[index] = static_cast<std::uint8_t>(
        std::min(static_cast<int>(data1[index]) + data2[index], 255));
  if (resized != nullptr)
    FreeImage(resized);
}

void idImageByteArray::Extrude(int x, int y, int w, int h) {
  if (y > 0) {
    std::memmove(data + 4 * width * (y - 1), data + 4 * width * y,
                 4ULL * width);
  }
  if (y + h < height) {
    std::memmove(data + 4 * width * (y + h),
                 data + 4 * width * (y + h - 1), 4ULL * width);
  }
  if (x > 0) {
    for (int row = 0; row < height; ++row) {
      std::memcpy(data + 4 * (row * width + x - 1),
                  data + 4 * (row * width + x), 4);
    }
  }
  if (x + w < width) {
    for (int row = 0; row < height; ++row) {
      std::memcpy(data + 4 * (row * width + x + w),
                  data + 4 * (row * width + x + w - 1), 4);
    }
  }
}

void AppendToken(idToken* token) {
  const int required = idStr::Length(parseBuffer) + token->Length() + 1;
  if (required < static_cast<int>(sizeof(parseBuffer))) {
    if (parseBuffer[0] != '\0')
      idStr::Append(parseBuffer, sizeof(parseBuffer), " ");
    idStr::Append(parseBuffer, sizeof(parseBuffer), token->data);
  } else if (!parseBufferError) {
    idLib::Warning(
        "image program: token \"%s\" can't be appended to \"%s\", length > MAX_IMAGE_NAME(%d)",
        token->data, parseBuffer, static_cast<int>(sizeof(parseBuffer)));
    parseBufferError = true;
  }
}

void MatchAndAppendToken(idParser* parser, const char* match) {
  if (!parser->ExpectTokenString(match))
    return;
  const int required = idStr::Length(parseBuffer) + idStr::Length(match) + 1;
  if (required < static_cast<int>(sizeof(parseBuffer))) {
    idStr::Append(parseBuffer, sizeof(parseBuffer), match);
  } else if (!parseBufferError) {
    idLib::Warning(
        "image program: token \"%s\" can't be appended to \"%s\", length > MAX_IMAGE_NAME(%d)",
        match, parseBuffer, static_cast<int>(sizeof(parseBuffer)));
    parseBufferError = true;
  }
}

void R_HeightmapToNormalMap(std::uint8_t* data, int width,
                            unsigned int height, float scale) {
  scale *= 1.0f / 256.0f;
  const int pixelCount = width * static_cast<int>(height);
  auto* depth = static_cast<std::uint8_t*>(AllocateImage(
      pixelCount, "image_program.cpp:R_HeightmapToNormalMap"));
  for (int pixel = 0; pixel < pixelCount; ++pixel) {
    depth[pixel] = static_cast<std::uint8_t>(
        (data[4 * pixel] + data[4 * pixel + 1] + data[4 * pixel + 2]) / 3);
  }

  for (unsigned int y = 0; y < height; ++y) {
    const unsigned int nextY = (y + 1) & (height - 1);
    for (int x = 0; x < width; ++x) {
      const int nextX = (x + 1) & (width - 1);
      const int current = depth[y * width + x];
      const int right = depth[y * width + nextX];
      const int below = depth[nextY * width + x];
      const int belowRight = depth[nextY * width + nextX];
      const float commonY = (current - below) * scale;

      float firstX = (current - right) * scale;
      float firstY = commonY;
      float firstZ = 1.0f;
      const float firstScale = ReciprocalLength(firstX, firstY, firstZ);
      firstX *= firstScale;
      firstY *= firstScale;
      firstZ *= firstScale;

      float secondX = (below - belowRight) * scale;
      float secondY = commonY;
      float secondZ = 1.0f;
      const float secondScale = ReciprocalLength(secondX, secondY, secondZ);
      secondX *= secondScale;
      secondY *= secondScale;
      secondZ *= secondScale;

      float normalX = firstX + secondX;
      float normalY = firstY + secondY;
      float normalZ = firstZ + secondZ;
      const float normalScale = ReciprocalLength(normalX, normalY, normalZ);
      const int destination = 4 * (y * width + x);
      data[destination] = EncodeNormal(normalX * normalScale);
      data[destination + 1] = EncodeNormal(normalY * normalScale);
      data[destination + 2] = EncodeNormal(normalZ * normalScale);
      data[destination + 3] = 255;
    }
  }
  FreeImage(depth);
}

void R_ImageScale(std::uint8_t* data, int width, int height, float* scale) {
  const int byteCount = 4 * width * height;
  for (int index = 0; index < byteCount; ++index) {
    data[index] = static_cast<std::uint8_t>(std::clamp(
        static_cast<int>(data[index] * scale[index & 3]), 0, 255));
  }
}

void R_DivideAlpha(std::uint8_t* data, int width, int height) {
  const int pixelCount = width * height;
  for (int pixel = 0; pixel < pixelCount; ++pixel) {
    std::uint8_t* color = data + 4 * pixel;
    if (color[3] == 0)
      continue;
    const float multiplier = 255.0f / color[3];
    for (int channel = 0; channel < 3; ++channel) {
      color[channel] = static_cast<std::uint8_t>(std::clamp(
          static_cast<int>(color[channel] * multiplier), 0, 255));
    }
  }
}

void R_AddNormalMaps(std::uint8_t* data1, unsigned int width1,
                     unsigned int height1, std::uint8_t* data2, int width2,
                     int height2) {
  std::uint8_t* resized = nullptr;
  if (width2 != static_cast<int>(width1) ||
      height2 != static_cast<int>(height1)) {
    resized = R_Dropsample(data2, width2, height2, width1,
                           static_cast<int>(height1));
    data2 = resized;
  }
  const int pixelCount = static_cast<int>(width1 * height1);
  for (int pixel = 0; pixel < pixelCount; ++pixel) {
    std::uint8_t* first = data1 + 4 * pixel;
    const std::uint8_t* second = data2 + 4 * pixel;
    float x = first[0] * (2.0f / 255.0f) - 1.0f;
    float y = first[1] * (2.0f / 255.0f) - 1.0f;
    float z = first[2] * (2.0f / 255.0f) - 1.0f;
    const float length = std::sqrt(x * x + y * y + z * z);
    if (length < 0.9f)
      z = std::sqrt(std::max(1.0f - x * x - y * y, 1.17549435e-38f));
    x += second[0] * (2.0f / 255.0f) - 1.0f;
    y += second[1] * (2.0f / 255.0f) - 1.0f;
    const float inverseLength = ReciprocalLength(x, y, z);
    first[0] = EncodeNormal(x * inverseLength);
    first[1] = EncodeNormal(y * inverseLength);
    first[2] = EncodeNormal(z * inverseLength);
    first[3] = 255;
  }
  if (resized != nullptr)
    FreeImage(resized);
}

void R_SmoothNormalMap(std::uint8_t* data, int width, int height) {
  const std::size_t byteCount = 4ULL * width * height;
  auto* original = static_cast<std::uint8_t*>(
      AllocateImage(byteCount, "image_program.cpp:R_SmoothNormalMap"));
  std::memmove(original, data, byteCount);
  for (int x = 0; x < width; ++x) {
    for (int y = 0; y < height; ++y) {
      float normal[3]{};
      for (int dx = -1; dx <= 1; ++dx) {
        for (int dy = -1; dy <= 1; ++dy) {
          const std::uint8_t* sample =
              original + 4 * ((((y + dy) & (height - 1)) * width) +
                              ((x + dx) & (width - 1)));
          if ((sample[0] == 0 && sample[1] == 0 && sample[2] == 0) ||
              (sample[0] == 128 && sample[1] == 128 && sample[2] == 128)) {
            continue;
          }
          for (int component = 0; component < 3; ++component)
            normal[component] += sample[component] * (2.0f / 255.0f) - 1.0f;
        }
      }
      const float scale = ReciprocalLength(normal[0], normal[1], normal[2]);
      std::uint8_t* output = data + 4 * (y * width + x);
      output[0] = EncodeNormal(normal[0] * scale);
      output[1] = EncodeNormal(normal[1] * scale);
      output[2] = EncodeNormal(normal[2] * scale);
    }
  }
  FreeImage(original);
}

std::uint8_t* R_AtlasBorder(std::uint8_t* picture, int* width, int* height) {
  const int sourceWidth = *width;
  const int sourceHeight = *height;
  idImageByteArray destination{
      static_cast<std::uint8_t*>(AllocateImage(
          4ULL * (sourceWidth + 8) * (sourceHeight + 8),
          "image_program.cpp:R_AtlasBorder")),
      sourceWidth + 8, sourceHeight + 8};
  const int copyWidth = std::min(sourceWidth, destination.width - 4);
  const int copyHeight = std::min(sourceHeight, destination.height - 4);
  for (int row = 0; row < copyHeight; ++row) {
    std::memmove(destination.data + 4 * ((row + 4) * destination.width + 4),
                 picture + 4 * row * sourceWidth, 4ULL * copyWidth);
  }
  int growth = 0;
  for (int border = 4; border > 0; --border) {
    destination.Extrude(border, border, sourceWidth + growth,
                        sourceHeight + growth);
    growth += 2;
  }
  FreeImage(picture);
  *width = destination.width;
  *height = destination.height;
  return destination.data;
}

std::uint8_t* R_Resize(std::uint8_t* picture, int sourceWidth,
                       int sourceHeight, int newWidth, int newHeight) {
  const bool targetWidthPowerOfTwo =
      newWidth > 0 && (newWidth & (newWidth - 1)) == 0;
  const bool targetHeightPowerOfTwo =
      newHeight > 0 && (newHeight & (newHeight - 1)) == 0;
  if (!targetWidthPowerOfTwo || !targetHeightPowerOfTwo) {
    idLib::Error("R_Resize: target size %dx%d is not a power of two (code error)",
                 newWidth, newHeight);
  }
  if (sourceWidth < newWidth || sourceHeight < newHeight) {
    idLib::Error("R_Resize: up sampling not implemented in (%dx%d -> %dx%d)",
                 sourceWidth, sourceHeight, newWidth, newHeight);
  }

  int width = sourceWidth;
  int height = sourceHeight;
  const std::uint8_t* current = picture;
  if ((width & (width - 1)) != 0 || (height & (height - 1)) != 0) {
    width = FloorPowerOfTwo(width);
    height = FloorPowerOfTwo(height);
    current = R_ResampleTexture(picture, sourceWidth, sourceHeight, width, height);
    FreeImage(picture);
  }
  while (width != newWidth || height != newHeight) {
    std::uint8_t* next;
    if (width != newWidth && height != newHeight) {
      next = R_MipMap(current, width, height);
      width >>= 1;
      height >>= 1;
    } else {
      const int nextWidth = std::max(width >> 1, newWidth);
      const int nextHeight = std::max(height >> 1, newHeight);
      next = R_ResampleTexture(current, width, height, nextWidth, nextHeight);
      width = nextWidth;
      height = nextHeight;
    }
    FreeImage(const_cast<std::uint8_t*>(current));
    current = next;
  }
  return const_cast<std::uint8_t*>(current);
}

std::uint8_t* R_ImageCompileWrinkleMask(
    int* width, int* height, idList<std::uint8_t*, 5>* pictures) {
  const int sourceWidth = *width;
  *width *= 2;
  if (pictures->num == 0)
    return nullptr;
  const int imageCount = std::min(pictures->num, 6);
  auto* output = static_cast<std::uint8_t*>(AllocateImage(
      4ULL * *width * *height, "image_program.cpp:R_ImageCompileWrinkleMask"));
  for (int x = 0; x < sourceWidth; ++x) {
    for (int y = 0; y < *height; ++y) {
      const int source = 4 * (y * sourceWidth + x);
      std::uint8_t packed[9];
      std::fill(std::begin(packed), std::end(packed),
                static_cast<std::uint8_t>(127));
      for (int image = 0; image < imageCount; image += 2) {
        const int destination = 3 * (image / 2);
        for (int channel = 0; channel < 3; ++channel) {
          if (image + 1 < imageCount) {
            packed[destination + channel] = static_cast<std::uint8_t>(
                (pictures->list[image][source + channel] >> 1) -
                (pictures->list[image + 1][source + channel] >> 1) + 127);
          } else {
            packed[destination + channel] = static_cast<std::uint8_t>(
                (pictures->list[image][source + channel] >> 1) + 127);
          }
        }
      }
      const int left = 4 * (y * *width + x);
      const int right = left + 4 * sourceWidth;
      std::memcpy(output + left, packed, 4);
      std::memcpy(output + right, packed + 4, 4);
    }
  }
  return output;
}

std::uint8_t* R_ImageShuffle(int* sourceImages, char* sourceChannels,
                             std::uint8_t* constants, int* width, int* height,
                             idList<std::uint8_t*, 5>* pictures) {
  std::uint8_t* output = pictures->num > 0
                             ? pictures->list[0]
                             : static_cast<std::uint8_t*>(AllocateImage(
                                   4ULL * *width * *height,
                                   "image_program.cpp:R_ImageShuffle"));
  const int pixelCount = *width * *height;
  for (int pixel = 0; pixel < pixelCount; ++pixel) {
    std::uint8_t result[4];
    for (int channel = 0; channel < 4; ++channel) {
      result[channel] =
          sourceImages[channel] < 0
              ? constants[channel]
              : pictures->list[sourceImages[channel]][4 * pixel +
                                                       sourceChannels[channel]];
    }
    std::memcpy(output + 4 * pixel, result, 4);
  }
  return output;
}

void R_OutlineAlpha(std::uint8_t* data, int width, int height,
                    std::uint8_t* alphaData, int count) {
  const std::size_t byteCount = 4ULL * width * height;
  auto* alphaCopy = static_cast<std::uint8_t*>(
      AllocateImage(byteCount, "image_program.cpp:R_OutlineAlpha:alpha"));
  auto* dataCopy = static_cast<std::uint8_t*>(
      AllocateImage(byteCount, "image_program.cpp:R_OutlineAlpha:data"));
  std::memmove(alphaCopy, alphaData, byteCount);
  std::uint8_t* alphaBuffers[2] = {alphaCopy, alphaData};
  while (count != 0) {
    --count;
    std::memmove(dataCopy, data, byteCount);
    const std::uint8_t* readAlpha = alphaBuffers[count & 1];
    std::uint8_t* writeAlpha = alphaBuffers[(count & 1) == 0 ? 1 : 0];
    for (int x = 0; x < width; ++x) {
      for (int y = 0; y < height; ++y) {
        const int destination = 4 * (y * width + x);
        if (readAlpha[destination + 3] != 0)
          continue;
        float sums[4]{};
        int samples = 0;
        for (int dx = -1; dx <= 1; ++dx) {
          for (int dy = -1; dy <= 1; ++dy) {
            const int source =
                4 * ((((y + dy) & (height - 1)) * width) +
                     ((x + dx) & (width - 1)));
            if (readAlpha[source + 3] == 0)
              continue;
            ++samples;
            for (int channel = 0; channel < 4; ++channel)
              sums[channel] += dataCopy[source + channel];
          }
        }
        if (samples != 0) {
          for (int channel = 0; channel < 4; ++channel)
            data[destination + channel] =
                static_cast<std::uint8_t>(sums[channel] / samples);
          writeAlpha[destination + 3] = 255;
        }
      }
    }
  }
  FreeImage(alphaCopy);
  FreeImage(dataCopy);
}

int ParseIntAndAppendToken(idParser* parser) {
  idToken token;
  if (!parser->ReadToken(token)) {
    parser->Error("couldn't read expected integer");
    return 0;
  }
  AppendToken(&token);
  bool negative = false;
  if (token.type == TT_PUNCTUATION && token == "-") {
    negative = true;
    parser->ExpectTokenType(TT_NUMBER, TT_INTEGER, token);
    AppendToken(&token);
  }
  if (token.type != TT_NUMBER || token.subtype == TT_FLOAT) {
    parser->Error("expected integer value, found '%s'",
                  token.data);
  }
  token.NumberValue();
  const int value = static_cast<int>(token.intvalue);
  return negative ? -value : value;
}

bool R_ParseImageProgram_r(idParser* parser, std::uint8_t** picture,
                           int* width, int* height,
                           idFileProps* fileProps,
                           const char* imageNameBase) {
  idToken token;
  if (!ReadAndAppend(parser, token))
    return false;

  auto parseUnary = [&](void (*operation)(std::uint8_t*, int, int)) {
    MatchAndAppendToken(parser, "(");
    if (!R_ParseImageProgram_r(parser, picture, width, height, fileProps,
                               imageNameBase)) {
      return false;
    }
    if (picture != nullptr)
      operation(*picture, *width, *height);
    MatchAndAppendToken(parser, ")");
    return true;
  };

  auto parseBinary = [&](void (*operation)(std::uint8_t*, int, int,
                                            std::uint8_t*, int, int)) {
    MatchAndAppendToken(parser, "(");
    if (!R_ParseImageProgram_r(parser, picture, width, height, fileProps,
                               imageNameBase)) {
      return false;
    }
    MatchAndAppendToken(parser, ",");
    std::uint8_t* secondPicture = nullptr;
    int secondWidth = 0;
    int secondHeight = 0;
    if (!R_ParseImageProgram_r(parser,
                               picture != nullptr ? &secondPicture : nullptr,
                               width != nullptr ? &secondWidth : nullptr,
                               height != nullptr ? &secondHeight : nullptr,
                               fileProps, imageNameBase)) {
      if (picture != nullptr) {
        FreeImage(*picture);
        *picture = nullptr;
      }
      return false;
    }
    if (picture != nullptr) {
      operation(*picture, *width, *height, secondPicture, secondWidth,
                secondHeight);
      FreeImage(secondPicture);
    }
    MatchAndAppendToken(parser, ")");
    return true;
  };

  if (token.Icmp("heightmap") == 0) {
    MatchAndAppendToken(parser, "(");
    if (!R_ParseImageProgram_r(parser, picture, width, height, fileProps,
                               imageNameBase)) {
      return false;
    }
    MatchAndAppendToken(parser, ",");
    if (!ReadAndAppend(parser, token))
      return false;
    const float scale = TokenFloat(token);
    if (picture != nullptr)
      R_HeightmapToNormalMap(*picture, *width, *height, scale);
    MatchAndAppendToken(parser, ")");
    return true;
  }

  if (token.Icmp("addnormals") == 0) {
    MatchAndAppendToken(parser, "(");
    if (!R_ParseImageProgram_r(parser, picture, width, height, fileProps,
                               imageNameBase)) {
      return false;
    }
    MatchAndAppendToken(parser, ",");
    std::uint8_t* secondPicture = nullptr;
    int secondWidth = 0;
    int secondHeight = 0;
    if (!R_ParseImageProgram_r(parser,
                               picture != nullptr ? &secondPicture : nullptr,
                               width != nullptr ? &secondWidth : nullptr,
                               height != nullptr ? &secondHeight : nullptr,
                               fileProps, imageNameBase)) {
      if (picture != nullptr) {
        FreeImage(*picture);
        *picture = nullptr;
      }
      return false;
    }
    if (picture != nullptr) {
      R_AddNormalMaps(*picture, *width, *height, secondPicture, secondWidth,
                      secondHeight);
      FreeImage(secondPicture);
    }
    MatchAndAppendToken(parser, ")");
    return true;
  }

  if (token.Icmp("multiplyImages") == 0) {
    MatchAndAppendToken(parser, "(");
    if (!R_ParseImageProgram_r(parser, picture, width, height, fileProps,
                               imageNameBase)) {
      return false;
    }
    MatchAndAppendToken(parser, ",");
    std::uint8_t* secondPicture = nullptr;
    int secondWidth = 0;
    int secondHeight = 0;
    if (!R_ParseImageProgram_r(parser,
                               picture != nullptr ? &secondPicture : nullptr,
                               width != nullptr ? &secondWidth : nullptr,
                               height != nullptr ? &secondHeight : nullptr,
                               fileProps, imageNameBase)) {
      if (picture != nullptr) {
        FreeImage(*picture);
        *picture = nullptr;
      }
      return false;
    }
    if (picture != nullptr && *picture != nullptr && secondPicture != nullptr) {
      const int byteCount = 4 * *width * *height;
      for (int index = 0; index < byteCount; ++index) {
        (*picture)[index] = static_cast<std::uint8_t>(std::clamp(
            static_cast<int>((*picture)[index] * secondPicture[index] *
                             (1.0f / 255.0f)),
            0, 255));
      }
      FreeImage(secondPicture);
    }
    MatchAndAppendToken(parser, ")");
    return true;
  }

  if (token.Icmp("smoothnormals") == 0)
    return parseUnary(R_SmoothNormalMap);
  if (token.Icmp("patchAlpha") == 0)
    return parseBinary(R_PatchAlpha);
  if (token.Icmp("add") == 0)
    return parseBinary(R_ImageAdd);

  if (token.Icmp("constantColor") == 0) {
    MatchAndAppendToken(parser, "(");
    std::uint8_t color[4]{};
    for (int channel = 0; channel < 4; ++channel) {
      if (!ReadAndAppend(parser, token))
        return false;
      color[channel] = static_cast<std::uint8_t>(std::clamp(
          static_cast<int>(TokenFloat(token) * 255.0f), 0, 255));
      if (channel != 3)
        MatchAndAppendToken(parser, ",");
    }
    MatchAndAppendToken(parser, ")");
    if (fileProps != nullptr) {
      fileProps->timestamp = 0;
      fileProps->writable = false;
    }
    if (width != nullptr)
      *width = 4;
    if (height != nullptr)
      *height = 4;
    if (picture != nullptr) {
      *picture = static_cast<std::uint8_t*>(AllocateImage(
          64, "image_program.cpp:R_ParseImageProgram_r:constantColor"));
      for (int pixel = 0; pixel < 16; ++pixel)
        std::memcpy(*picture + 4 * pixel, color, 4);
    }
    return true;
  }

  if (token.Icmp("scale") == 0) {
    MatchAndAppendToken(parser, "(");
    if (!R_ParseImageProgram_r(parser, picture, width, height, fileProps,
                               imageNameBase)) {
      return false;
    }
    float scale[4] = {1.0f, 1.0f, 1.0f, 1.0f};
    int count = 0;
    for (;;) {
      if (!ReadAndAppend(parser, token))
        return false;
      if (token == ")")
        break;
      if (token != ",") {
        parser->Warning("Bad scale format");
        break;
      }
      if (!ReadAndAppend(parser, token))
        return false;
      if (count < 4)
        scale[count] = TokenFloat(token);
      ++count;
      if (count >= 4)
        break;
    }
    if (count == 1)
      scale[1] = scale[2] = scale[3] = scale[0];
    else if (count != 4)
      parser->Warning("Bad scale format");
    if (count == 4)
      MatchAndAppendToken(parser, ")");
    if (picture != nullptr)
      R_ImageScale(*picture, *width, *height, scale);
    return true;
  }

  if (token.Icmp("outlineAlpha") == 0) {
    MatchAndAppendToken(parser, "(");
    if (!R_ParseImageProgram_r(parser, picture, width, height, fileProps,
                               imageNameBase)) {
      return false;
    }
    MatchAndAppendToken(parser, ",");
    std::uint8_t* alphaPicture = nullptr;
    int alphaWidth = 0;
    int alphaHeight = 0;
    if (!R_ParseImageProgram_r(parser,
                               picture != nullptr ? &alphaPicture : nullptr,
                               width != nullptr ? &alphaWidth : nullptr,
                               height != nullptr ? &alphaHeight : nullptr,
                               fileProps, imageNameBase)) {
      if (picture != nullptr) {
        FreeImage(*picture);
        *picture = nullptr;
      }
      return false;
    }
    MatchAndAppendToken(parser, ",");
    if (!ReadAndAppend(parser, token))
      return false;
    const int outlineCount = TokenInt(token);
    if (picture != nullptr) {
      if (*width != alphaWidth || *height != alphaHeight) {
        parser->Warning("mismatched sizes.\n");
        FreeImage(*picture);
        FreeImage(alphaPicture);
        *picture = nullptr;
        return false;
      }
      R_OutlineAlpha(*picture, alphaWidth, alphaHeight, alphaPicture,
                     outlineCount);
      FreeImage(alphaPicture);
    }
    MatchAndAppendToken(parser, ")");
    return true;
  }

  if (token.Icmp("divideAlpha") == 0)
    return parseUnary(R_DivideAlpha);

  if (token.Icmp("invertAlpha") == 0) {
    MatchAndAppendToken(parser, "(");
    if (!R_ParseImageProgram_r(parser, picture, width, height, fileProps,
                               imageNameBase)) {
      return false;
    }
    if (picture != nullptr) {
      for (int pixel = 0; pixel < *width * *height; ++pixel)
        (*picture)[4 * pixel + 3] = 255 - (*picture)[4 * pixel + 3];
    }
    MatchAndAppendToken(parser, ")");
    return true;
  }

  if (token.Icmp("invertColor") == 0) {
    MatchAndAppendToken(parser, "(");
    if (!R_ParseImageProgram_r(parser, picture, width, height, fileProps,
                               imageNameBase)) {
      return false;
    }
    if (picture != nullptr) {
      for (int pixel = 0; pixel < *width * *height; ++pixel) {
        for (int channel = 0; channel < 3; ++channel)
          (*picture)[4 * pixel + channel] =
              255 - (*picture)[4 * pixel + channel];
      }
    }
    MatchAndAppendToken(parser, ")");
    return true;
  }

  if (token.Icmp("makeIntensity") == 0) {
    MatchAndAppendToken(parser, "(");
    if (!R_ParseImageProgram_r(parser, picture, width, height, fileProps,
                               imageNameBase)) {
      return false;
    }
    if (picture != nullptr) {
      for (int pixel = 0; pixel < *width * *height; ++pixel) {
        const std::uint8_t intensity = (*picture)[4 * pixel];
        std::memset(*picture + 4 * pixel, intensity, 4);
      }
    }
    MatchAndAppendToken(parser, ")");
    return true;
  }

  if (token.Icmp("makeAlpha") == 0) {
    MatchAndAppendToken(parser, "(");
    if (!R_ParseImageProgram_r(parser, picture, width, height, fileProps,
                               imageNameBase)) {
      return false;
    }
    if (picture != nullptr) {
      for (int pixel = 0; pixel < *width * *height; ++pixel) {
        std::uint8_t* color = *picture + 4 * pixel;
        color[3] = static_cast<std::uint8_t>(
            (static_cast<unsigned int>(color[0]) + color[1] + color[2]) / 3);
        color[0] = color[1] = color[2] = 255;
      }
    }
    MatchAndAppendToken(parser, ")");
    return true;
  }

  if (token.Icmp("extractAlpha") == 0) {
    MatchAndAppendToken(parser, "(");
    if (!R_ParseImageProgram_r(parser, picture, width, height, fileProps,
                               imageNameBase)) {
      return false;
    }
    if (picture != nullptr) {
      for (int pixel = 0; pixel < *width * *height; ++pixel) {
        std::uint8_t* color = *picture + 4 * pixel;
        color[0] = color[1] = color[2] = color[3];
        color[3] = 255;
      }
    }
    MatchAndAppendToken(parser, ")");
    return true;
  }

  if (token.Icmp("makeDivergence") == 0) {
    MatchAndAppendToken(parser, "(");
    if (!R_ParseImageProgram_r(parser, picture, width, height, fileProps,
                               imageNameBase)) {
      return false;
    }
    if (picture != nullptr)
      R_SetAlphaNormalDivergence(*picture, *width, *height);
    MatchAndAppendToken(parser, ")");
    return true;
  }

  if (token.Icmp("shrink") == 0) {
    MatchAndAppendToken(parser, "(");
    int sourceWidth = 0;
    int sourceHeight = 0;
    if (!R_ParseImageProgram_r(parser, picture,
                               width != nullptr ? &sourceWidth : nullptr,
                               height != nullptr ? &sourceHeight : nullptr,
                               fileProps, imageNameBase)) {
      return false;
    }
    MatchAndAppendToken(parser, ",");
    int targetWidth = ParseIntAndAppendToken(parser);
    int targetHeight = -1;
    if (sourceHeight > sourceWidth) {
      targetHeight = targetWidth;
      targetWidth = -1;
    }
    if (sourceWidth != 0 && sourceHeight != 0) {
      if (targetHeight == -1)
        targetHeight = sourceHeight * targetWidth / sourceWidth;
      else if (targetWidth == -1)
        targetWidth = sourceWidth * targetHeight / sourceHeight;
    }
    if (targetWidth < sourceWidth) {
      if (targetWidth <= 0 || (targetWidth & (targetWidth - 1)) != 0 ||
          targetHeight <= 0 || (targetHeight & (targetHeight - 1)) != 0) {
        parser->Error(
            "new size not a power of to: old: %dx%d, new %dx%d\n",
            sourceWidth, sourceHeight, targetWidth, targetHeight);
        return false;
      }
      if (picture != nullptr)
        *picture = R_Resize(*picture, sourceWidth, sourceHeight, targetWidth,
                            targetHeight);
      if (width != nullptr && height != nullptr) {
        *width = targetWidth;
        *height = targetHeight;
      }
    } else if (width != nullptr && height != nullptr) {
      *width = sourceWidth;
      *height = sourceHeight;
    }
    MatchAndAppendToken(parser, ")");
    return true;
  }

  if (token.Icmp("resize") == 0) {
    MatchAndAppendToken(parser, "(");
    int sourceWidth = 0;
    int sourceHeight = 0;
    if (!R_ParseImageProgram_r(parser, picture,
                               width != nullptr ? &sourceWidth : nullptr,
                               height != nullptr ? &sourceHeight : nullptr,
                               fileProps, imageNameBase)) {
      return false;
    }
    MatchAndAppendToken(parser, ",");
    int targetWidth = ParseIntAndAppendToken(parser);
    int targetHeight = -1;
    if (parser->PeekTokenString(",")) {
      MatchAndAppendToken(parser, ",");
      targetHeight = ParseIntAndAppendToken(parser);
    }
    if (targetWidth == -1 && targetHeight == -1) {
      parser->Error("At least one of target width and height must be specified\n");
      return false;
    }
    if (targetWidth < -1 || targetHeight < -1) {
      parser->Error("invalid target size: %dx%d\n", targetWidth,
                    targetHeight);
      return false;
    }
    if (sourceWidth != 0 && sourceHeight != 0) {
      if (targetHeight == -1)
        targetHeight = sourceHeight * targetWidth / sourceWidth;
      else if (targetWidth == -1)
        targetWidth = sourceWidth * targetHeight / sourceHeight;
    }
    if (picture != nullptr) {
      if (targetWidth <= 0 || (targetWidth & (targetWidth - 1)) != 0 ||
          targetHeight <= 0 || (targetHeight & (targetHeight - 1)) != 0) {
        parser->Error(
            "new size not a power of to: old: %dx%d, new %dx%d\n",
            sourceWidth, sourceHeight, targetWidth, targetHeight);
        return false;
      }
      *picture = R_Resize(*picture, sourceWidth, sourceHeight, targetWidth,
                          targetHeight);
    }
    if (width != nullptr && height != nullptr) {
      *width = targetWidth;
      *height = targetHeight;
    }
    MatchAndAppendToken(parser, ")");
    return true;
  }

  if (token.Icmp("atlasBorder") == 0) {
    MatchAndAppendToken(parser, "(");
    if (!R_ParseImageProgram_r(parser, picture, width, height, fileProps,
                               imageNameBase)) {
      return false;
    }
    if (picture != nullptr)
      *picture = R_AtlasBorder(*picture, width, height);
    MatchAndAppendToken(parser, ")");
    return true;
  }

  if (token.Icmp("wrinkleMask") == 0) {
    MatchAndAppendToken(parser, "(");
    idToken baseToken;
    if (!parser->ExpectTokenType(TT_NAME, 0, baseToken))
      return false;
    AppendToken(&baseToken);
    idStr baseName(baseToken);
    MatchAndAppendToken(parser, ",");
    if (!parser->ExpectTokenType(TT_NUMBER, TT_INTEGER, token))
      return false;
    AppendToken(&token);
    const int count = TokenInt(token);
    idList<std::uint8_t*, 5> pictures;
    bool failed = false;
    int loadedWidth = 0;
    int loadedHeight = 0;
    if (!IsParseOnly(picture, width, height, fileProps)) {
      for (int index = 0; index < count; ++index) {
        std::uint8_t** slot = pictures.Alloc();
        *slot = nullptr;
        int imageWidth = 0;
        int imageHeight = 0;
        idFileProps imageProps{static_cast<std::uint32_t>(-1), false};
        idStr imageName(baseName);
        imageName += index;
        R_LoadImage(imageName.data, picture != nullptr ? slot : nullptr,
                    width != nullptr
                        ? reinterpret_cast<unsigned int*>(&imageWidth)
                        : nullptr,
                    height != nullptr
                        ? reinterpret_cast<unsigned int*>(&imageHeight)
                        : nullptr,
                    &imageProps);
        if (imageProps.timestamp == static_cast<std::uint32_t>(-1)) {
          idLib::Warning("Missing wrinklemap '%s%d'", baseName.data, index);
          failed = true;
        }
        if (fileProps != nullptr &&
            (fileProps->timestamp == static_cast<std::uint32_t>(-1) ||
             imageProps.timestamp > fileProps->timestamp)) {
          fileProps->timestamp = imageProps.timestamp;
        }
        if (!failed && width != nullptr && height != nullptr) {
          if (pictures.Num() == 1) {
            loadedWidth = imageWidth;
            loadedHeight = imageHeight;
            *width = imageWidth;
            *height = imageHeight;
          } else if (imageWidth != loadedWidth || imageHeight != loadedHeight) {
            idLib::Warning(
                "image sizes mismatch, first image: %dx%d, %d:th image %dx%d\n",
                loadedWidth, loadedHeight, pictures.Num(), imageWidth,
                imageHeight);
            failed = true;
          }
        }
      }
    }
    if (failed) {
      FreePictures(pictures, 0);
      parser->Error("There were errors parsing the image '%s'", baseName.data);
      return false;
    }
    if (width != nullptr && height != nullptr) {
      std::uint8_t* result = R_ImageCompileWrinkleMask(width, height, &pictures);
      if (picture != nullptr)
        *picture = result;
    }
    MatchAndAppendToken(parser, ")");
    FreePictures(pictures, 0);
    return true;
  }

  if (token.Icmp("chanShuffle") == 0) {
    MatchAndAppendToken(parser, "(");
    int sourcePictures[4] = {-1, -1, -1, -1};
    char sourceChannels[4]{};
    std::uint8_t constants[4]{};
    int highestSource = -1;
    constexpr char channelNames[] = "rgba";
    for (int outputChannel = 0; outputChannel < 4; ++outputChannel) {
      if (!ReadAndAppend(parser, token))
        return false;
      if (token.type == TT_NUMBER) {
        sourcePictures[outputChannel] = -1;
        const float value = TokenFloat(token);
        if (value < 0.0f || value > 1.0f) {
          parser->Error(
              "chanShuffle: constant for channel %c is out of range: %f in shuffle spec \"%s\"\n",
              channelNames[outputChannel], value, token.data);
          return false;
        }
        constants[outputChannel] =
            static_cast<std::uint8_t>(value * 255.0f);
      } else {
        if ((token.type != TT_NAME && token.type != TT_STRING) ||
            token.Length() != 2) {
          parser->Error(
              "chanShuffle: invalid shuffle specification: \"%s\", expected [source number][source channel]\n",
              token.data);
          return false;
        }
        token.ToLower();
        int channel = -1;
        for (int index = 0; index < 4; ++index) {
          if (token[0] == channelNames[index]) {
            channel = index;
            break;
          }
        }
        if (channel == -1) {
          parser->Error(
              "chanShuffle: unknown channel '%c' in shuffle spec \"%s\", valid: r,g,b,a\n",
              token[0], token.data);
          return false;
        }
        const int source = token[1] - '0';
        sourceChannels[outputChannel] = static_cast<char>(channel);
        sourcePictures[outputChannel] = source;
        highestSource = std::max(highestSource, source);
      }
      MatchAndAppendToken(parser, ",");
    }

    idList<std::uint8_t*, 5> pictures;
    bool failed = false;
    for (;;) {
      std::uint8_t** slot = pictures.Alloc();
      *slot = nullptr;
      int imageWidth = 0;
      int imageHeight = 0;
      if (!R_ParseImageProgram_r(parser, picture != nullptr ? slot : nullptr,
                                 width != nullptr ? &imageWidth : nullptr,
                                 height != nullptr ? &imageHeight : nullptr,
                                 fileProps, imageNameBase)) {
        failed = true;
        break;
      }
      if (width != nullptr && height != nullptr) {
        if (pictures.Num() == 1) {
          *width = imageWidth;
          *height = imageHeight;
        } else if (imageWidth != *width || imageHeight != *height) {
          parser->Error(
              "image sizes mismatch, first image: %dx%d, %d:th image %dx%d\n",
              *width, *height, pictures.Num(), imageWidth, imageHeight);
          failed = true;
          break;
        }
      }
      if (!parser->PeekTokenString(","))
        break;
      MatchAndAppendToken(parser, ",");
    }
    if (!failed && pictures.Num() <= highestSource) {
      parser->Error("chanShuffle: pattern uses %d but only %d is specified\n",
                    highestSource, pictures.Num());
      failed = true;
    }
    if (failed) {
      FreePictures(pictures, 0);
      return false;
    }
    if (width != nullptr && height != nullptr) {
      std::uint8_t* result = R_ImageShuffle(sourcePictures, sourceChannels,
                                            constants, width, height, &pictures);
      if (picture != nullptr)
        *picture = result;
    }
    MatchAndAppendToken(parser, ")");
    FreePictures(pictures, 1);
    return true;
  }

  if (token.Icmp("nameBase") == 0) {
    MatchAndAppendToken(parser, "(");
    if (!ReadAndAppend(parser, token))
      return false;
    if (token.type != TT_NAME && token.type != TT_STRING) {
      parser->Error("setImgBase: expected string or name, found \"%s\"\n",
                    token.data);
      return false;
    }
    idStr newBase(imageNameBase);
    newBase += token.data;
    MatchAndAppendToken(parser, ",");
    if (!R_ParseImageProgram_r(parser, picture, width, height, fileProps,
                               newBase.data)) {
      return false;
    }
    MatchAndAppendToken(parser, ")");
    return true;
  }

  if (token.Icmp("checker") == 0) {
    MatchAndAppendToken(parser, "(");
    if (!ReadAndAppend(parser, token))
      return false;
    const int checkerWidth = std::atol(token.data);
    MatchAndAppendToken(parser, ",");
    if (!ReadAndAppend(parser, token))
      return false;
    const int checkerHeight = std::atol(token.data);
    if (width != nullptr)
      *width = checkerWidth;
    if (height != nullptr)
      *height = checkerWidth;
    if (picture != nullptr) {
      *picture = static_cast<std::uint8_t*>(AllocateImage(
          4ULL * checkerWidth * checkerHeight,
          "image_program.cpp:R_ParseImageProgram_r:checker"));
      for (int y = 0; y < checkerHeight; ++y) {
        for (int x = 0; x < checkerWidth; ++x) {
          std::uint8_t* output = *picture + 4 * (y * checkerWidth + x);
          output[0] = output[1] = output[2] =
              static_cast<std::uint8_t>(-((x ^ y) & 1));
          output[3] = 255;
        }
      }
    }
    MatchAndAppendToken(parser, ")");
    return true;
  }

  if (IsParseOnly(picture, width, height, fileProps))
    return true;

  auto makeBuiltin = [&](std::uint8_t red, std::uint8_t green,
                         std::uint8_t blue) {
    if (width != nullptr)
      *width = 1;
    if (height != nullptr)
      *height = 1;
    if (picture != nullptr) {
      *picture = static_cast<std::uint8_t*>(AllocateImage(
          4, "image_program.cpp:R_ParseImageProgram_r:builtin"));
      (*picture)[0] = red;
      (*picture)[1] = green;
      (*picture)[2] = blue;
      (*picture)[3] = 255;
    }
    if (fileProps != nullptr) {
      fileProps->timestamp = 0;
      fileProps->writable = false;
    }
    return true;
  };

  if (token.Icmp("_flat") == 0)
    return makeBuiltin(128, 128, 255);
  if (token.Icmp("_black") == 0)
    return makeBuiltin(0, 0, 0);
  if (token.Icmp("_white") == 0 ||
      token.Icmp("_borderedPointWhite") == 0 ||
      token.Icmp("_noFalloff") == 0) {
    return makeBuiltin(255, 255, 255);
  }

  idFileProps loadedProps{static_cast<std::uint32_t>(-1), false};
  idStr fullName(imageNameBase);
  fullName += token.data;
  R_LoadImage(fullName.data, picture,
              reinterpret_cast<unsigned int*>(width),
              reinterpret_cast<unsigned int*>(height), &loadedProps);
  if (loadedProps.timestamp == static_cast<std::uint32_t>(-1))
    return false;
  MergeFileProps(fileProps, loadedProps);
  return true;
}

bool R_LoadImageProgram(idParser* parser, std::uint8_t** picture,
                        int* width, int* height, idFileProps* fileProps) {
  parseBuffer[0] = '\0';
  parseBufferError = false;
  if (fileProps != nullptr) {
    fileProps->timestamp = static_cast<std::uint32_t>(-1);
    fileProps->writable = false;
  }
  if (picture != nullptr)
    *picture = nullptr;
  if (width != nullptr)
    *width = 0;
  if (height != nullptr)
    *height = 0;

  idImageOpts options{};
  options.packedTail = true;
  options.textureOptions = TO_NONE;
  options.sampler.filter = TF_DEFAULT;
  options.sampler.lodMaxClamp = 13.0f;
  idImage::ParseImageOpts(*parser, options);
  const bool parsed = R_ParseImageProgram_r(
      parser, picture, width, height, fileProps, imgNameBase);
  if (parseBufferError)
    parser->Error("Image program/name to long, max %d chars", 256);
  if (parsed && !parser->HadError())
    return true;

  if (fileProps != nullptr) {
    fileProps->timestamp = static_cast<std::uint32_t>(-1);
    fileProps->writable = false;
  }
  if (picture != nullptr && *picture != nullptr) {
    FreeImage(*picture);
    *picture = nullptr;
  }
  if (width != nullptr)
    *width = 0;
  if (height != nullptr)
    *height = 0;
  return false;
}

idStr* R_ParsePastImageProgram(idStr* result, idParser* parser) {
  parseBuffer[0] = '\0';
  parseBufferError = false;
  R_ParseImageProgram_r(parser, nullptr, nullptr, nullptr, nullptr,
                        imgNameBase);
  if (parseBufferError)
    parser->Error("Image program/name to long, max %d chars", 256);
  *result = parseBuffer;
  return result;
}

bool R_LoadImageProgram(const char* name, std::uint8_t** picture,
                        int* width, int* height, idFileProps* fileProps) {
  idParser parser;
  parser.LoadMemory(name, idStr::Length(name), name);
  parser.SetFlags(564);
  return R_LoadImageProgram(&parser, picture, width, height, fileProps);
}
