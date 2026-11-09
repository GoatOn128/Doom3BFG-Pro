#include "engine/guis/swf/swf.h"

#include "engine/guis/swf/swf_bitstream.h"
#include "shared/idlib/packing/rectallocator.h"

#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <Windows.h>
#include <wincodec.h>

#include <algorithm>
#include <cstring>
#include <fstream>
#include <vector>

namespace {
struct JPEGState {
  std::vector<std::uint8_t> tables;
};

template <class T> void ReleaseCOM(T*& object) {
  if (object != nullptr) {
    object->Release();
    object = nullptr;
  }
}

std::vector<std::uint8_t> BuildJPEGInput(const JPEGState& state,
                                         const std::uint8_t* input, int inputSize) {
  if (state.tables.empty())
    return std::vector<std::uint8_t>(input, input + inputSize);
  std::vector<std::uint8_t> combined;
  combined.reserve(state.tables.size() + static_cast<std::size_t>(inputSize));
  combined.push_back(0xff);
  combined.push_back(0xd8);
  std::size_t tableStart = state.tables.size() >= 2 && state.tables[0] == 0xff &&
                                   state.tables[1] == 0xd8 ? 2 : 0;
  std::size_t tableEnd = state.tables.size();
  if (tableEnd >= 2 && state.tables[tableEnd - 2] == 0xff && state.tables[tableEnd - 1] == 0xd9)
    tableEnd -= 2;
  combined.insert(combined.end(), state.tables.begin() + tableStart,
                  state.tables.begin() + tableEnd);
  int imageStart = inputSize >= 2 && input[0] == 0xff && input[1] == 0xd8 ? 2 : 0;
  combined.insert(combined.end(), input + imageStart, input + inputSize);
  return combined;
}

bool InflateBytes(idSWF& swf, idSWFBitStream& stream, std::vector<std::uint8_t>& output) {
  const int remaining = static_cast<int>(stream.Length() - stream.Tell());
  const std::uint8_t* input = stream.ReadData(remaining);
  return swf.Inflate(input, static_cast<unsigned int>(remaining), output.data(),
                     static_cast<unsigned int>(output.size()));
}
} // namespace

idSWF::idDecompressJPEG::idDecompressJPEG() : vinfo(new JPEGState()) {}
idSWF::idDecompressJPEG::~idDecompressJPEG() { delete static_cast<JPEGState*>(vinfo); }

std::uint8_t* idSWF::idDecompressJPEG::Load(const std::uint8_t* input, int inputSize,
                                             int& width, int& height) {
  width = height = 0;
  JPEGState& state = *static_cast<JPEGState*>(vinfo);
  std::vector<std::uint8_t> source = BuildJPEGInput(state, input, inputSize);
  const HRESULT initialization = CoInitializeEx(nullptr, COINIT_MULTITHREADED);
  const bool uninitialize = SUCCEEDED(initialization);
  IWICImagingFactory* factory = nullptr;
  IWICStream* stream = nullptr;
  IWICBitmapDecoder* decoder = nullptr;
  IWICBitmapFrameDecode* frame = nullptr;
  IWICFormatConverter* converter = nullptr;
  HRESULT result = CoCreateInstance(CLSID_WICImagingFactory, nullptr, CLSCTX_INPROC_SERVER,
                                    IID_PPV_ARGS(&factory));
  if (SUCCEEDED(result)) result = factory->CreateStream(&stream);
  if (SUCCEEDED(result))
    result = stream->InitializeFromMemory(source.data(), static_cast<DWORD>(source.size()));
  if (SUCCEEDED(result))
    result = factory->CreateDecoderFromStream(stream, nullptr, WICDecodeMetadataCacheOnLoad,
                                              &decoder);
  if (SUCCEEDED(result)) result = decoder->GetFrame(0, &frame);
  if (SUCCEEDED(result)) result = factory->CreateFormatConverter(&converter);
  if (SUCCEEDED(result))
    result = converter->Initialize(frame, GUID_WICPixelFormat32bppRGBA,
                                   WICBitmapDitherTypeNone, nullptr, 0.0,
                                   WICBitmapPaletteTypeCustom);
  UINT decodedWidth = 0, decodedHeight = 0;
  if (SUCCEEDED(result)) result = converter->GetSize(&decodedWidth, &decodedHeight);
  std::uint8_t* output = nullptr;
  if (SUCCEEDED(result) && decodedWidth != 0 && decodedHeight != 0) {
    const UINT stride = decodedWidth * 4;
    const UINT byteCount = stride * decodedHeight;
    output = new std::uint8_t[byteCount];
    result = converter->CopyPixels(nullptr, stride, byteCount, output);
    if (FAILED(result)) {
      delete[] output;
      output = nullptr;
    } else {
      width = static_cast<int>(decodedWidth);
      height = static_cast<int>(decodedHeight);
    }
  }
  ReleaseCOM(converter); ReleaseCOM(frame); ReleaseCOM(decoder); ReleaseCOM(stream); ReleaseCOM(factory);
  if (uninitialize) CoUninitialize();
  if (output == nullptr)
    state.tables.assign(input, input + inputSize);
  return output;
}

void idSWF::JPEGTables(idSWFBitStream& bitstream) {
  const int length = static_cast<int>(bitstream.Length());
  if (length == 0) return;
  int width = 0, height = 0;
  delete[] jpeg.Load(bitstream.ReadData(length), length, width, height);
}

void idSWF::LoadImageA(int characterID, const std::uint8_t* imageData, int width, int height) {
  idSWFDictionaryEntry* entry = AddDictionaryEntry(characterID, SWF_DICT_IMAGE);
  if (entry == nullptr) return;
  imageToPack_t image{};
  image.characterID = characterID;
  image.trueSize = idVec2i{width, height};
  image.imageData = new std::uint8_t[static_cast<std::size_t>(width) * height * 4];
  std::memcpy(image.imageData, imageData, static_cast<std::size_t>(width) * height * 4);
  image.allocSize.x = (width + 3) >> 2;
  image.allocSize.y = (height + 3) >> 2;
  if (image.allocSize.x * 4 - width < 2) ++image.allocSize.x;
  if (image.allocSize.y * 4 - height < 2) ++image.allocSize.y;
  packImages.Append(image);
  entry->material = nullptr;
}

void idSWF::DefineBits(idSWFBitStream& bitstream) {
  const int characterID = bitstream.ReadU16();
  const int length = static_cast<int>(bitstream.Length() - bitstream.Tell());
  int width = 0, height = 0;
  std::uint8_t* data = jpeg.Load(bitstream.ReadData(length), length, width, height);
  if (data != nullptr) { LoadImageA(characterID, data, width, height); delete[] data; }
}

void idSWF::DefineBitsJPEG2(idSWFBitStream& bitstream) {
  const int characterID = bitstream.ReadU16();
  const int length = static_cast<int>(bitstream.Length() - bitstream.Tell());
  idDecompressJPEG localJPEG;
  int width = 0, height = 0;
  std::uint8_t* data = localJPEG.Load(bitstream.ReadData(length), length, width, height);
  if (data != nullptr) { LoadImageA(characterID, data, width, height); delete[] data; }
}

void idSWF::DefineBitsJPEG3(idSWFBitStream& bitstream) {
  const int characterID = bitstream.ReadU16();
  const int jpegSize = static_cast<int>(bitstream.ReadU32());
  idDecompressJPEG localJPEG;
  int width = 0, height = 0;
  std::uint8_t* data = localJPEG.Load(bitstream.ReadData(jpegSize), jpegSize, width, height);
  if (data == nullptr) return;
  std::vector<std::uint8_t> alpha(static_cast<std::size_t>(width) * height);
  if (InflateBytes(*this, bitstream, alpha)) {
    for (std::size_t i = 0; i < alpha.size(); ++i) data[i * 4 + 3] = alpha[i];
    LoadImageA(characterID, data, width, height);
  }
  delete[] data;
}

void idSWF::DefineBitsLossless(idSWFBitStream& bitstream) {
  const int characterID = bitstream.ReadU16();
  const int format = bitstream.ReadU8();
  const int width = bitstream.ReadU16(), height = bitstream.ReadU16();
  std::vector<std::uint8_t> rgba(static_cast<std::size_t>(width) * height * 4, 255);
  if (format == 3) {
    const int colors = bitstream.ReadU8() + 1;
    const int rowStride = (width + 3) & ~3;
    std::vector<std::uint8_t> decoded(static_cast<std::size_t>(colors) * 3 +
                                      static_cast<std::size_t>(rowStride) * height);
    if (!InflateBytes(*this, bitstream, decoded)) return;
    const std::uint8_t* indices = decoded.data() + colors * 3;
    for (int y=0;y<height;++y) for (int x=0;x<width;++x) {
      const int color = indices[y*rowStride+x] * 3; std::uint8_t* out=&rgba[(y*width+x)*4];
      out[0]=decoded[color]; out[1]=decoded[color+1]; out[2]=decoded[color+2];
    }
  } else if (format == 4) {
    const int rowStride = (width * 2 + 3) & ~3;
    std::vector<std::uint8_t> decoded(static_cast<std::size_t>(rowStride) * height);
    if (!InflateBytes(*this, bitstream, decoded)) return;
    for (int y=0;y<height;++y) for(int x=0;x<width;++x) {
      const std::uint16_t pixel=static_cast<std::uint16_t>(decoded[y*rowStride+x*2]<<8|decoded[y*rowStride+x*2+1]);
      std::uint8_t* out=&rgba[(y*width+x)*4]; out[0]=static_cast<std::uint8_t>((pixel>>10)&31); out[1]=static_cast<std::uint8_t>((pixel>>5)&31); out[2]=static_cast<std::uint8_t>(pixel&31);
    }
  } else if (format == 5) {
    std::vector<std::uint8_t> decoded(static_cast<std::size_t>(width) * height * 4);
    if (!InflateBytes(*this, bitstream, decoded)) return;
    for (int i=0;i<width*height;++i) { rgba[i*4]=decoded[i*4+1]; rgba[i*4+1]=decoded[i*4+2]; rgba[i*4+2]=decoded[i*4+3]; }
  }
  LoadImageA(characterID, rgba.data(), width, height);
}

void idSWF::DefineBitsLossless2(idSWFBitStream& bitstream) {
  const int characterID = bitstream.ReadU16();
  const int format = bitstream.ReadU8();
  const int width = bitstream.ReadU16(), height = bitstream.ReadU16();
  std::vector<std::uint8_t> rgba(static_cast<std::size_t>(width) * height * 4, 255);
  if (format == 3) {
    const int colors=bitstream.ReadU8()+1, rowStride=(width+3)&~3;
    std::vector<std::uint8_t> decoded(static_cast<std::size_t>(colors)*4 + static_cast<std::size_t>(rowStride)*height);
    if (!InflateBytes(*this,bitstream,decoded)) return;
    const std::uint8_t* indices=decoded.data()+colors*4;
    for(int y=0;y<height;++y) for(int x=0;x<width;++x) std::memcpy(&rgba[(y*width+x)*4],&decoded[indices[y*rowStride+x]*4],4);
  } else if (format == 5) {
    std::vector<std::uint8_t> decoded(static_cast<std::size_t>(width)*height*4);
    if (!InflateBytes(*this,bitstream,decoded)) return;
    for(int i=0;i<width*height;++i) { rgba[i*4]=decoded[i*4+1]; rgba[i*4+1]=decoded[i*4+2]; rgba[i*4+2]=decoded[i*4+3]; rgba[i*4+3]=decoded[i*4]; }
  }
  LoadImageA(characterID,rgba.data(),width,height);
}

void idSWF::WriteSwfImageAtlas(const char* outputFilename) {
  idList<idVec2i> inputSizes;
  inputSizes.SetNum(packImages.Num());
  for (int i = 0; i < packImages.Num(); ++i)
    inputSizes[i] = packImages[i].allocSize;
  idList<idVec2i> outputPositions;
  idVec2i totalSize{};
  if (!RectAllocator(inputSizes, outputPositions, totalSize))
    return;
  int packedAtlasWidth = std::max(4, totalSize.x * 4);
  const int packedAtlasHeight = std::max(4, totalSize.y * 4);
  packedAtlasWidth = (packedAtlasWidth + 127) & ~127;
  std::vector<std::uint8_t> atlas(
      static_cast<std::size_t>(packedAtlasWidth) * packedAtlasHeight * 4);
  for (std::size_t i = 0; i < atlas.size(); i += 4) {
    atlas[i] = 255; atlas[i + 1] = 0; atlas[i + 2] = 0; atlas[i + 3] = 255;
  }

  for (int imageIndex = 0; imageIndex < packImages.Num(); ++imageIndex) {
    imageToPack_t& packed = packImages[imageIndex];
    int maximum[4] = {0, 0, 0, 0};
    const int pixelCount = packed.trueSize.x * packed.trueSize.y;
    for (int pixel = 0; pixel < pixelCount; ++pixel)
      for (int channel = 0; channel < 4; ++channel)
        maximum[channel] = std::max(maximum[channel], static_cast<int>(packed.imageData[pixel * 4 + channel]));
    for (int channel = 0; channel < 4; ++channel)
      if (maximum[channel] == 0) maximum[channel] = 1;
    for (int pixel = 0; pixel < pixelCount; ++pixel)
      for (int channel = 0; channel < 4; ++channel)
        packed.imageData[pixel * 4 + channel] = static_cast<std::uint8_t>(
            packed.imageData[pixel * 4 + channel] * 255 / maximum[channel]);

    const int atlasX = outputPositions[imageIndex].x * 4;
    const int atlasY = outputPositions[imageIndex].y * 4;
    for (int destinationY = 0; destinationY < packed.allocSize.y * 4; ++destinationY) {
      const int sourceY = std::clamp(destinationY - 1, 0, packed.trueSize.y - 1);
      for (int destinationX = 0; destinationX < packed.allocSize.x * 4; ++destinationX) {
        const int sourceX = std::clamp(destinationX - 1, 0, packed.trueSize.x - 1);
        std::memcpy(&atlas[(static_cast<std::size_t>(atlasY + destinationY) * packedAtlasWidth +
                            atlasX + destinationX) * 4],
                    &packed.imageData[(sourceY * packed.trueSize.x + sourceX) * 4], 4);
      }
    }
    idSWFDictionaryEntry* entry = FindDictionaryEntry(packed.characterID);
    if (entry != nullptr) {
      entry->imageSize = packed.trueSize;
      entry->imageAtlasOffset = idVec2i{atlasX + 1, atlasY + 1};
      entry->channelScale = idVec4{maximum[0] / 255.0f, maximum[1] / 255.0f,
                                   maximum[2] / 255.0f, maximum[3] / 255.0f};
    }
    delete[] packed.imageData;
    packed.imageData = nullptr;
  }

  if (outputFilename == nullptr || outputFilename[0] == '\0')
    return;
  std::ofstream output(outputFilename, std::ios::binary);
  if (!output)
    return;
  std::uint8_t header[18]{};
  header[2] = 2;
  header[12] = static_cast<std::uint8_t>(packedAtlasWidth);
  header[13] = static_cast<std::uint8_t>(packedAtlasWidth >> 8);
  header[14] = static_cast<std::uint8_t>(packedAtlasHeight);
  header[15] = static_cast<std::uint8_t>(packedAtlasHeight >> 8);
  header[16] = 32;
  header[17] = 0x20;
  output.write(reinterpret_cast<const char*>(header), sizeof(header));
  for (int y = 0; y < packedAtlasHeight; ++y) for (int x = 0; x < packedAtlasWidth; ++x) {
    const std::uint8_t* pixel =
        &atlas[(static_cast<std::size_t>(y) * packedAtlasWidth + x) * 4];
    const std::uint8_t bgra[4] = {pixel[2], pixel[1], pixel[0], pixel[3]};
    output.write(reinterpret_cast<const char*>(bgra), 4);
  }
}
