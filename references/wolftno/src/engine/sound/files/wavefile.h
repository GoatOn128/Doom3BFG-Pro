#pragma once

#include "shared/idlib/containers/staticlist.h"
#include "shared/idlib/filesystem/file.h"

#include <cstdint>

class idWaveFile {
public:
  struct guid_t {
    std::uint32_t data1;
    std::uint16_t data2;
    std::uint16_t data3;
    std::uint16_t data4;
    std::uint8_t data5[6];

    bool operator==(const guid_t& rhs) const;
  };

#pragma pack(push, 2)
  struct waveFmt_t {
    struct basic_t {
      std::uint16_t formatTag;
      std::uint16_t numChannels;
      std::uint32_t samplesPerSec;
      std::uint32_t avgBytesPerSec;
      std::uint16_t blockSize;
      std::uint16_t bitsPerSample;
    } basic;

    std::uint16_t extraSize;

    union extra_t {
      struct extensible_t {
        std::uint16_t validBitsPerSample;
        std::uint32_t channelMask;
        guid_t subFormat;
      } extensible;
      struct at9_t {
        std::uint16_t samplesPerBlock;
        std::uint32_t channelMask;
        guid_t subFormat;
        std::uint32_t version;
        std::uint32_t configData;
        std::uint32_t reserved;
      } at9;
      struct adpcm_t {
        struct adpcmcoef_t {
          std::int16_t coef1;
          std::int16_t coef2;
        };
        std::uint16_t samplesPerBlock;
        std::uint16_t numCoef;
        adpcmcoef_t aCoef[7];
      } adpcm;
      struct xma2_t {
        std::uint16_t numStreams;
        std::uint32_t channelMask;
        std::uint32_t samplesEncoded;
        std::uint32_t bytesPerBlock;
        std::uint32_t playBegin;
        std::uint32_t playLength;
        std::uint32_t loopBegin;
        std::uint32_t loopLength;
        std::uint8_t loopCount;
        std::uint8_t encoderVersion;
        std::uint16_t blockCount;
      } xma2;
      struct vorbis_t {
        std::uint32_t samplesEncoded;
      } vorbis;
    } extra;
  };
#pragma pack(pop)

  struct chunk_t {
    std::uint32_t id;
    std::uint32_t size;
    std::uint32_t offset;
  };

  struct samplerChunk_t {
    std::uint32_t manufacturer;
    std::uint32_t product;
    std::uint32_t samplePeriod;
    std::uint32_t MIDIUnityNote;
    std::uint32_t MIDIPitchFraction;
    std::uint32_t SMPTEFormat;
    std::uint32_t SMPTEOffset;
    std::uint32_t numSampleLoops;
    std::uint32_t extraSamplerData;
  };

  struct sampleData_t {
    std::uint32_t identifier;
    std::uint32_t type;
    std::uint32_t start;
    std::uint32_t end;
    std::uint32_t fraction;
    std::uint32_t playCount;
  };

  idWaveFile() = default;
  ~idWaveFile();

  std::int64_t SeekToChunk(std::uint32_t id);
  bool ReadLoopData(std::uint32_t* start, std::uint32_t* end);
  static const char* ReadWaveFormatDirect(waveFmt_t* format, idFile* file);
  const char* ReadWaveFormat(waveFmt_t* format);
  void Close();
  bool Open(const char* filename);

  static const guid_t guidPCM;
  static const guid_t guidAT9;

  idFile* file{nullptr};
  idStaticList<chunk_t, 32> chunks;
};

static_assert(sizeof(idWaveFile::guid_t) == 16);
static_assert(sizeof(idWaveFile::waveFmt_t::basic_t) == 16);
static_assert(sizeof(idWaveFile::waveFmt_t::extra_t::extensible_t) == 22);
static_assert(sizeof(idWaveFile::waveFmt_t::extra_t::adpcm_t) == 32);
static_assert(sizeof(idWaveFile::waveFmt_t::extra_t::xma2_t) == 34);
static_assert(sizeof(idWaveFile::waveFmt_t::extra_t::at9_t) == 34);
static_assert(sizeof(idWaveFile::waveFmt_t) == 52);
static_assert(sizeof(idWaveFile::chunk_t) == 12);
static_assert(sizeof(idWaveFile::samplerChunk_t) == 36);
static_assert(sizeof(idWaveFile::sampleData_t) == 24);
static_assert(sizeof(idWaveFile) == 416);
