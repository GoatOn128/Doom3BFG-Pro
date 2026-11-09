#include "engine/renderer/image_intrinsic.h"

#include "engine/renderer/image.h"
#include "engine/renderer/imagemanager.h"
#include "shared/idlib/sys/sys_alloc.h"

#include <algorithm>
#include <cmath>
#include <cstddef>
#include <cstdint>
#include <cstring>

namespace {

constexpr int DEFAULT_SIZE = 16;
constexpr std::size_t DITHER_256_BYTES = 0x20000U;

int s1 = 55555;
int s2 = 99999;

idImageOpts IntrinsicOpts(textureFormat_t format, int width, int height,
                          textureFilter_t filter = TF_DEFAULT,
                          textureRepeat_t repeat = TR_REPEAT) {
  idImageOpts opts{};
  opts.textureType = TT_2D;
  opts.format = format;
  opts.colorFormat = CFM_DEFAULT;
  opts.width = width;
  opts.height = height;
  opts.depth = 0;
  opts.numLevels = 1;
  opts.numSamples = 0;
  opts.packedTail = true;
  opts.textureBindType = TB_SHADER;
  opts.textureUsage = TU_DEFAULT;
  opts.textureOptions = TO_NONE;
  opts.sampler.filter = filter;
  opts.sampler.repeat = repeat;
  opts.sampler.lodMaxClamp = 13.0F;
  return opts;
}

std::uint8_t ClampByte(int value) {
  return static_cast<std::uint8_t>(std::clamp(value, 0, 255));
}

template <int Dimension>
void BuildOrderedDither(std::uint8_t (&dither)[Dimension][Dimension],
                        int initialBlockSize, int initialOffset) {
  std::memset(dither, 1, sizeof(dither));
  int offset = initialOffset;
  for (int blockSize = initialBlockSize; blockSize >= 1;
       blockSize /= 2) {
    const int stride = blockSize * 2;
    for (int y = 0; y < Dimension; y += stride) {
      for (int x = 0; x < Dimension; x += stride) {
        const std::uint8_t base = dither[y][x];
        dither[y + blockSize][x + blockSize] =
            static_cast<std::uint8_t>(base + offset);
        dither[y + blockSize][x] =
            static_cast<std::uint8_t>(base + 2 * offset);
        dither[y][x + blockSize] =
            static_cast<std::uint8_t>(base + 3 * offset);
      }
    }
    offset *= 4;
  }
}

}  // namespace

float uni() {
  const int quotient1 = s1 / 53668;
  std::int64_t next1 =
      static_cast<std::int64_t>(40014) * s1 -
      static_cast<std::int64_t>(2147483563) * quotient1;
  s1 = static_cast<int>(next1);
  if (s1 < 0) {
    s1 += 2147483563;
  }

  const int quotient2 = s2 / 52774;
  std::int64_t next2 =
      static_cast<std::int64_t>(40692) * s2 -
      static_cast<std::int64_t>(2147483399) * quotient2;
  s2 = static_cast<int>(next2);
  if (s2 < 0) {
    s2 += 2147483399;
  }

  int result = static_cast<int>(static_cast<std::int64_t>(s1) + s2 -
                                2147483563LL);
  if (result < 1) {
    result += 2147483562;
  }
  return static_cast<float>(result) * 4.6566134e-10F;
}

void idImageIntrinsic::WaveImage() {
  std::uint8_t data[256];
  for (int index = 0; index < 256; ++index) {
    float angle = static_cast<float>(index) * (1.0F / 256.0F) *
                  6.2831855F;
    if (angle < 0.0F || angle >= 6.2831855F) {
      angle -= std::floor(angle * 0.15915494F) * 6.2831855F;
    }
    const float wave =
        std::pow((std::cos(angle) + 1.0F) * 0.5F, 1.0F);
    data[index] = ClampByte(static_cast<int>(wave * 256.0F));
  }

  idImageOpts opts = IntrinsicOpts(FMT_LUM8, 256, 1, TF_LINEAR);
  opts.textureBindType = TB_RENDER_TARGET;
  opts.sampler.border.x = 1.0F;
  idImage* image = globalImages->ScratchImage("_wave", opts);
  image->SubImageUpload(0, 0, 0, 0, opts.width, opts.height, data, 0);
}

float PHBeckmann(float ndoth, float m) {
  float angle;
  if (ndoth > -1.0F) {
    angle = ndoth < 1.0F ? std::acos(ndoth) : 0.0F;
  } else {
    angle = 3.1415927F;
  }
  const float mSquared = m * m;
  const float tangent = std::tan(angle);
  const float scale = 1.0F / (std::pow(ndoth, 4.0F) * mSquared);
  return scale * std::exp(-(tangent * tangent / mSquared));
}

void idImageIntrinsic::BeckmannImage() {
  std::uint8_t data[256][256];
  for (int y = 0; y < 256; ++y) {
    for (int x = 0; x < 256; ++x) {
      const float beckmann = PHBeckmann(static_cast<float>(x) / 255.0F,
                                        static_cast<float>(y) / 255.0F);
      const int value = static_cast<int>(
          std::pow(beckmann, 0.1F) * 0.5F * 255.0F);
      data[y][x] = ClampByte(value);
    }
  }

  idImageOpts opts = IntrinsicOpts(FMT_LUM8, 256, 256, TF_DEFAULT,
                                   TR_CLAMP);
  idImage* image = globalImages->ScratchImage("_beckmann", opts);
  image->SubImageUpload(0, 0, 0, 0, opts.width, opts.height, data, 0);
}

void idImageIntrinsic::FresnelImage() {
  std::uint8_t data[256];
  for (int index = 0; index < 256; ++index) {
    const float value =
        std::pow(1.0F - static_cast<float>(index) / 255.0F, 5.0F);
    data[index] = ClampByte(static_cast<int>(value * 255.5F));
  }

  idImageOpts opts = IntrinsicOpts(FMT_LUM8, 256, 1, TF_LINEAR);
  opts.textureBindType = TB_RENDER_TARGET;
  opts.sampler.border.x = 1.0F;
  opts.sampler.border.y = 1.0F;
  idImage* image = globalImages->ScratchImage("_fresnel", opts);
  image->SubImageUpload(0, 0, 0, 0, opts.width, opts.height, data, 0);
}

void idImage::MakeDefault() {
  std::uint8_t data[DEFAULT_SIZE][DEFAULT_SIZE];
  std::memset(data, 32, sizeof(data));
  for (int index = 0; index < DEFAULT_SIZE; ++index) {
    data[0][index] = 255;
    data[index][0] = 255;
    data[DEFAULT_SIZE - 1][index] = 255;
    data[index][DEFAULT_SIZE - 1] = 255;
  }

  idImageOpts newOpts =
      IntrinsicOpts(FMT_LUM8, DEFAULT_SIZE, DEFAULT_SIZE);
  if (opts.textureType == TT_CUBIC) {
    newOpts.textureType = TT_CUBIC;
    AllocImage(newOpts);
    for (int face = 0; face < 6; ++face) {
      SubImageUpload(0, 0, 0, face, DEFAULT_SIZE, DEFAULT_SIZE, data, 0);
    }
  } else {
    if (ImageState() == IS_PURGED ||
        newOpts.format != opts.format || newOpts.width != opts.width ||
        newOpts.height != opts.height) {
      AllocImage(newOpts);
    }
    SubImageUpload(0, 0, 0, 0, DEFAULT_SIZE, DEFAULT_SIZE, data, 0);
  }
  defaulted = true;
}

void idImageIntrinsic::DefaultImage() {
  idImageOpts opts =
      IntrinsicOpts(FMT_LUM8, DEFAULT_SIZE, DEFAULT_SIZE);
  idImage* image = globalImages->ScratchImage("_default", opts);
  globalImages->defaultImage = image;
  image->MakeDefault();
}

void idImageIntrinsic::WhiteImage() {
  std::uint8_t data[DEFAULT_SIZE][DEFAULT_SIZE];
  std::memset(data, 255, sizeof(data));
  idImageOpts opts =
      IntrinsicOpts(FMT_LUM8, DEFAULT_SIZE, DEFAULT_SIZE);
  idImage* image = globalImages->ScratchImage("_white", opts);
  image->SubImageUpload(0, 0, 0, 0, opts.width, opts.height, data, 0);
  globalImages->whiteImage = image;
}

void idImageIntrinsic::BlackImage() {
  std::uint8_t data[DEFAULT_SIZE][DEFAULT_SIZE];
  std::memset(data, 0, sizeof(data));
  idImageOpts opts =
      IntrinsicOpts(FMT_LUM8, DEFAULT_SIZE, DEFAULT_SIZE);
  idImage* image = globalImages->ScratchImage("_black", opts);
  image->SubImageUpload(0, 0, 0, 0, opts.width, opts.height, data, 0);
  globalImages->blackImage = image;
}

void idImageIntrinsic::FlatNormalImage() {
  const std::uint8_t data[2][2][4] = {
      {{128, 128, 255, 255}, {128, 128, 255, 255}},
      {{128, 128, 255, 255}, {128, 128, 255, 255}},
  };
  idImageOpts opts = IntrinsicOpts(FMT_RGBA8, 2, 2);
  idImage* image = globalImages->ScratchImage("_flat", opts);
  image->SubImageUpload(0, 0, 0, 0, opts.width, opts.height, data, 0);
}

void idImageIntrinsic::DefaultCubeImage() {
  auto* storage = static_cast<std::uint8_t*>(
      idMem::Singleton()->AllocWithLocation(
          "w:\\build\\tech5\\engine\\renderer\\Image_intrinsic.cpp(728) : TAG_IMAGE",
          384U, TAG_IMAGE, false, ALIGN_16, HEAP_DEFAULTHEAP));
  std::uint8_t* pixels[6];
  for (int face = 0; face < 6; ++face) {
    pixels[face] = storage + face * 64;
    const int color = face + 1;
    for (int pixel = 0; pixel < 16; ++pixel) {
      pixels[face][pixel * 4 + 0] =
          static_cast<std::uint8_t>(255 * (color & 1));
      pixels[face][pixel * 4 + 1] =
          static_cast<std::uint8_t>(255 * ((color >> 1) & 1));
      pixels[face][pixel * 4 + 2] =
          static_cast<std::uint8_t>(255 * ((color >> 2) & 1));
      pixels[face][pixel * 4 + 3] = 255;
    }
  }

  idImageOpts opts = IntrinsicOpts(FMT_DXT1, 4, 4);
  opts.textureType = TT_CUBIC;
  idImage* image = globalImages->ScratchImage("_defaultCube", opts);
  for (int face = 0; face < 6; ++face) {
    image->SubImageUpload(0, 0, 0, face, 4, 4, pixels[face], 0);
  }
  idMem::Singleton()->Free(storage, ALIGN_16);
}

std::uint8_t* GenerateColorCorrectionTex(int cubeDim, int texDim) {
  const int byteCount = 4 * texDim * texDim * texDim;
  auto* result = static_cast<std::uint8_t*>(
      idMem::Singleton()->AllocWithLocation(
          "w:\\build\\tech5\\engine\\renderer\\Image_intrinsic.cpp(768) : TAG_IMAGE",
          static_cast<std::size_t>(byteCount), TAG_IMAGE, false, ALIGN_16,
          HEAP_DEFAULTHEAP));
  for (int blue = 0; blue < texDim; ++blue) {
    for (int green = 0; green < texDim; ++green) {
      for (int red = 0; red < texDim; ++red) {
        const std::size_t pixel =
            static_cast<std::size_t>(green * texDim * texDim +
                                     blue * texDim + red) * 4U;
        if (red >= cubeDim || green >= cubeDim || blue >= cubeDim) {
          result[pixel + 0] = 0;
          result[pixel + 1] = 0;
          result[pixel + 2] = 0;
        } else {
          const int divisor = cubeDim - 1;
          result[pixel + 0] = static_cast<std::uint8_t>(
              std::min((red * 256) / divisor, 255));
          result[pixel + 1] = static_cast<std::uint8_t>(
              std::min((green * 256) / divisor, 255));
          result[pixel + 2] = static_cast<std::uint8_t>(
              std::min((blue * 256) / divisor, 255));
        }
        result[pixel + 3] = 255;
      }
    }
  }
  return result;
}

void idImageIntrinsic::DefaultColorCorrectionTex() {
  std::uint8_t* data = GenerateColorCorrectionTex(16, 16);
  idImageOpts opts = IntrinsicOpts(FMT_RGBA8, 256, 16);
  idImage* image = globalImages->ScratchImage("_defaultCC", opts);
  image->SubImageUpload(0, 0, 0, 0, opts.width, opts.height, data, 0);
  idMem::Singleton()->Free(data, ALIGN_16);
}

void idImageIntrinsic::Dither2Image() {
  const std::uint8_t data[2][2] = {{64, 196}, {255, 128}};
  idImageOpts opts =
      IntrinsicOpts(FMT_INT8, 2, 2, TF_NEAREST, TR_REPEAT);
  idImage* image = globalImages->ScratchImage("_dither2", opts);
  image->SubImageUpload(0, 0, 0, 0, opts.width, opts.height, data, 0);
}

void idImageIntrinsic::Dither4Image() {
  std::uint8_t data[4][4];
  BuildOrderedDither(data, 2, 16);
  idImageOpts opts =
      IntrinsicOpts(FMT_INT8, 4, 4, TF_NEAREST, TR_REPEAT);
  idImage* image = globalImages->ScratchImage("_dither4", opts);
  image->SubImageUpload(0, 0, 0, 0, opts.width, opts.height, data, 0);
}

void idImageIntrinsic::Dither16Image() {
  std::uint8_t data[16][16];
  BuildOrderedDither(data, 8, 1);
  for (auto& row : data) {
    for (std::uint8_t& value : row) {
      if (value == 0) {
        value = 1;
      }
    }
  }
  idImageOpts opts =
      IntrinsicOpts(FMT_INT8, 16, 16, TF_NEAREST, TR_REPEAT);
  idImage* image = globalImages->ScratchImage("_dither16", opts);
  image->SubImageUpload(0, 0, 0, 0, opts.width, opts.height, data, 0);
}

void idImageIntrinsic::Dither256Image() {
  auto* data = static_cast<std::uint8_t*>(
      idMem::Singleton()->AllocWithLocation(
          "w:\\build\\tech5\\engine\\renderer\\Image_intrinsic.cpp(1082) : TAG_IMAGE",
          DITHER_256_BYTES, TAG_IMAGE, false, ALIGN_16,
          HEAP_DEFAULTHEAP));
  for (int row = 0; row < 256; ++row) {
    std::uint8_t* rowData = data + row * 512;
    for (int column = 0; column < 256; ++column) {
      rowData[column * 2 + 0] = static_cast<std::uint8_t>(column);
      rowData[column * 2 + 1] = static_cast<std::uint8_t>(column);
    }
    for (int column = 0; column < 256; ++column) {
      const int randomColumn = static_cast<int>(uni() * 256.0F);
      std::swap(rowData[column * 2], rowData[randomColumn * 2]);
    }
    for (int column = 0; column < 256; ++column) {
      const int randomColumn = static_cast<int>(uni() * -256.0F);
      const int randomOffset = 1 - 2 * randomColumn;
      std::swap(rowData[column * 2 + 1], rowData[randomOffset]);
    }
  }

  idImageOpts opts =
      IntrinsicOpts(FMT_L8A8, 256, 256, TF_NEAREST, TR_REPEAT);
  idImage* image = globalImages->ScratchImage("_dither256", opts);
  image->SubImageUpload(0, 0, 0, 0, opts.width, opts.height, data, 0);
  idMem::Singleton()->Free(data, ALIGN_16);
}

void idImageIntrinsic::BorderedPointWhiteImage() {
  std::uint8_t data[DEFAULT_SIZE][DEFAULT_SIZE];
  std::memset(data, 255, sizeof(data));
  idImageOpts opts = IntrinsicOpts(FMT_LUM8, DEFAULT_SIZE, DEFAULT_SIZE,
                                   TF_NEAREST, TR_REPEAT);
  idImage* image =
      globalImages->ScratchImage("_borderedPointWhite", opts);
  image->SubImageUpload(0, 0, 0, 0, opts.width, opts.height, data, 0);
  globalImages->borderedPointWhiteImage = image;
}

void idImageIntrinsic::NoFalloffImage() {
  std::uint8_t data[4][4];
  std::memset(data, 255, sizeof(data));
  idImageOpts opts = IntrinsicOpts(FMT_LUM8, 4, 4, TF_DEFAULT,
                                   TR_CLAMP_TO_BORDER);
  idImage* image = globalImages->ScratchImage("_noFalloff", opts);
  image->SubImageUpload(0, 0, 0, 0, opts.width, opts.height, data, 0);
}

void idImageManager::CreateIntrinsicImages() {
  idImageIntrinsic::DefaultImage();
  idImageIntrinsic::WhiteImage();
  idImageIntrinsic::BorderedPointWhiteImage();
  idImageIntrinsic::BlackImage();
  idImageIntrinsic::FlatNormalImage();
  idImageIntrinsic::WaveImage();
  idImageIntrinsic::BeckmannImage();
  idImageIntrinsic::FresnelImage();
  idImageIntrinsic::DefaultCubeImage();
  idImageIntrinsic::DefaultColorCorrectionTex();
  idImageIntrinsic::NoFalloffImage();
  idImageIntrinsic::Dither2Image();
  idImageIntrinsic::Dither4Image();
  idImageIntrinsic::Dither16Image();
  idImageIntrinsic::Dither256Image();
  globalImages->ps3ReplacementImage = nullptr;
}
