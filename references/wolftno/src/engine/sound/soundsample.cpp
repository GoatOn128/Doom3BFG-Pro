#include "engine/sound/soundsample.h"

#include "shared/idlib/csystems/cvarsystemlocal.h"
#include "shared/idlib/filesystem/filesystemlocal.h"
#include "shared/idlib/lib.h"

#include <algorithm>
#include <cstring>
#include <memory>
#include <mutex>
#include <string>
#include <type_traits>
#include <unordered_map>

namespace {

constexpr std::uint16_t WAVE_FORMAT_PCM = 1;
constexpr std::uint16_t WAVE_FORMAT_ADPCM = 2;
constexpr std::uint16_t WAVE_FORMAT_MP3 = 85;
constexpr std::uint16_t WAVE_FORMAT_XMA2 = 0x0166;
constexpr std::uint16_t WAVE_FORMAT_VORBIS = 0x674F;
constexpr std::uint16_t WAVE_FORMAT_AT9 = 0x42D2;
constexpr std::uint16_t WAVE_FORMAT_EXTENSIBLE = 0xFFFE;

constexpr std::uint32_t FourCC(char a, char b, char c, char d) {
  return (static_cast<std::uint32_t>(static_cast<unsigned char>(a)) << 24) |
         (static_cast<std::uint32_t>(static_cast<unsigned char>(b)) << 16) |
         (static_cast<std::uint32_t>(static_cast<unsigned char>(c)) << 8) |
         static_cast<std::uint32_t>(static_cast<unsigned char>(d));
}

std::mutex ownedStreamMutex;
std::unordered_map<const idSoundSample*, std::unique_ptr<idFile>> ownedStreams;

void SetOwnedStream(const idSoundSample* sample, idFile* file) {
  std::lock_guard<std::mutex> lock(ownedStreamMutex);
  if (file == nullptr) ownedStreams.erase(sample);
  else ownedStreams[sample].reset(file);
}

void ReleaseOwnedStream(const idSoundSample* sample) {
  std::lock_guard<std::mutex> lock(ownedStreamMutex);
  ownedStreams.erase(sample);
}

std::uint32_t FileTimestamp(const char* filename) {
  if (fileSystem == nullptr || filename == nullptr) return static_cast<std::uint32_t>(-1);
  if (!fileSystem->FileExists(filename)) return static_cast<std::uint32_t>(-1);
  return fileSystem->GetTimestamp(filename);
}

int StreamPriority(int queued) {
  if (queued <= 1) return 1015808;
  const int scale = 3 - queued;
  return scale < 0 ? 0 : (4 * scale) << 15;
}

idStr FixLongFilename(const char* basePath, const char* extension, const char* input) {
  const char* source = input ? input : "";
  if (idStr::CmpPrefix(source, "../../") == 0) source += 6;
  idStr full(source);
  full.SetFileExtension(extension);
  idStr fileName;
  full.ExtractFileName(fileName);
  if (fileName.Length() < 38) {
    idStr result(basePath);
    result.AppendPath(source);
    result.SetFileExtension(extension);
    return result;
  }
  idStr filePath;
  full.ExtractFilePath(filePath);
  fileName.StripFileExtension();
  idStr result(basePath);
  result.AppendPath(filePath.data);
  int segmentLength = 0;
  for (int index = 0; index < fileName.Length(); ++index) {
    if (segmentLength >= 30) {
      result.Append('/');
      segmentLength = 0;
    }
    result.Append(fileName[index]);
    ++segmentLength;
  }
  result.SetFileExtension(extension);
  return result;
}

const char* ShaderName(const idSoundShader* shader) {
  if (shader == nullptr) return "";
  return reinterpret_cast<const idResource*>(shader)->GetName();
}

bool ReadAtomicString(idFile* file, idAtomicString& value) {
  std::int32_t length = 0;
  if (file == nullptr || file->Read(&length, sizeof(length)) != sizeof(length) ||
      length < 0 || length > 16 * 1024 * 1024) {
    return false;
  }
  idStr text;
  text.Fill('\0', length);
  if (length > 0 && file->Read(text.data, length) != static_cast<std::size_t>(length)) {
    return false;
  }
  text.data[length] = '\0';
  text.len = length;
  value.Set(text.data);
  return true;
}

}  // namespace

idTypedResourceList<idSoundSample> idSoundSample::resourceList("sample");
void (*soundStopVoicesWithSample)(const idSoundSample*) = nullptr;

void SoundSampleAdoptStream(const idSoundSample* sample, idFile* file) {
  SetOwnedStream(sample, file);
}

idSoundSample::idSoundSample()
    : timestamp(static_cast<std::uint32_t>(-1)), firstValidSample(0), lastValidSample(0),
      loopPoint(0), dataLength(0), staticBuffer(nullptr), streamFile(nullptr),
      streamOffset(0), format{}, xma2SeekTable(nullptr), mp3SeekTableEntries(0),
      mp3SeekTable(nullptr) {}

idSoundSample::~idSoundSample() { FreeData(); }

idWaveFile::~idWaveFile() {
  Close();
}

idResourceList* idSoundSample::GetResourceList() const { return &resourceList; }

bool idSoundSample::GetSeekPosition(int sampleOffset, int* byteOffset,
                                    std::uint32_t* samplesToSkip) const {
  if (byteOffset == nullptr || samplesToSkip == nullptr) return false;
  const int sample = firstValidSample + sampleOffset;
  if (sample < firstValidSample || sample >= lastValidSample) return false;
  switch (format.basic.formatTag) {
    case WAVE_FORMAT_ADPCM:
      if (format.extra.adpcm.samplesPerBlock == 0) return false;
      *byteOffset = format.basic.blockSize * (sample / format.extra.adpcm.samplesPerBlock);
      *samplesToSkip = 0;
      return true;
    case WAVE_FORMAT_VORBIS:
      *byteOffset = 0;
      *samplesToSkip = 0;
      return true;
    case WAVE_FORMAT_PCM:
      *byteOffset = sample * format.basic.blockSize;
      *samplesToSkip = 0;
      return true;
    case WAVE_FORMAT_XMA2: {
      if (xma2SeekTable == nullptr) return false;
      int previous = 0;
      for (int index = 0; index < format.extra.xma2.blockCount; ++index) {
        if (xma2SeekTable[index] > sample) {
          *byteOffset = index * static_cast<int>(format.extra.xma2.bytesPerBlock);
          if (index > 0) previous = xma2SeekTable[index - 1];
          *samplesToSkip = static_cast<std::uint32_t>((sample - previous) & ~127);
          return true;
        }
      }
      return false;
    }
    default:
      break;
  }
  if (format.basic.formatTag == WAVE_FORMAT_AT9) return true;
  if (format.basic.formatTag != WAVE_FORMAT_MP3 || mp3SeekTableEntries <= 0 ||
      mp3SeekTable == nullptr) return false;
  int byte = 0;
  int accumulatedSamples = 0;
  for (int index = 0; index < mp3SeekTableEntries; ++index) {
    const int samples = mp3SeekTable[index].numSamples;
    accumulatedSamples += samples;
    if (accumulatedSamples + samples > sample) {
      *byteOffset = byte;
      *samplesToSkip = static_cast<std::uint32_t>(sample - accumulatedSamples);
      return true;
    }
    byte += mp3SeekTable[index].offset;
  }
  return false;
}

void idSoundSample::MakeDefault() {
  if (!cvarSystem->GetCVarBool("s_noSound", false) && resourceError == nullptr)
    SetResourceError("defaulted");
  timestamp = static_cast<std::uint32_t>(-1);
  firstValidSample = 0;
  lastValidSample = 256;
  std::memset(&format, 0, sizeof(format));
  format.basic.formatTag = WAVE_FORMAT_PCM;
  format.basic.numChannels = 1;
  format.basic.samplesPerSec = 1000;
  format.basic.avgBytesPerSec = 1000;
  format.basic.blockSize = 1;
  format.basic.bitsPerSample = 8;
  auto* data = new unsigned char[256];
  for (int index = 0; index < 256; index += 2) {
    data[index] = 0;
    data[index + 1] = 0xFF;
  }
  staticBuffer = data;
  dataLength = 256;
  loopPoint = firstValidSample;
}

void idSoundSample::FreeData() {
  SetResourceError(nullptr);
  if (staticBuffer != nullptr) {
    if (soundStopVoicesWithSample != nullptr) soundStopVoicesWithSample(this);
    delete[] static_cast<unsigned char*>(staticBuffer);
  }
  delete[] xma2SeekTable;
  delete[] mp3SeekTable;
  ReleaseOwnedStream(this);
  timestamp = static_cast<std::uint32_t>(-1);
  std::memset(&format, 0, sizeof(format));
  firstValidSample = lastValidSample = loopPoint = dataLength = 0;
  staticBuffer = nullptr;
  streamFile = nullptr;
  streamOffset = 0;
  xma2SeekTable = nullptr;
  mp3SeekTableEntries = 0;
  mp3SeekTable = nullptr;
}

int idSoundSample::ReadCachedSoundData(int offset, void* buffer, int length, int queued,
                                       bool blocking) const {
  if (streamFile == nullptr) return 0;
  return SoundReadCachedData(streamFile, static_cast<std::int64_t>(streamOffset) + offset,
                             length, buffer, blocking, StreamPriority(queued),
                             64 << (2 * queued + 2));
}

bool idSoundSample::PinStreamData(pinnedCacheLines_t* lines) const {
  return streamFile != nullptr &&
         SoundPinCachedData(streamFile, streamOffset, 0x10000, lines, true, 1015808, 256);
}

int idSoundSample::PreCacheStreamData(bool highPriority) const {
  if (streamFile == nullptr) return 0x10000;
  return ReadCachedSoundData(0, nullptr, 0x10000, highPriority ? 0 : 3, false);
}

void idSoundSample::List() const {
  const char* channels = format.basic.numChannels > 1 ? "ST" : "  ";
  const int sampleCount = lastValidSample - firstValidSample;
  char status = 'X';
  if (sampleCount > 0) {
    if (timestamp == static_cast<std::uint32_t>(-1)) status = 'D';
    else if (format.basic.formatTag == WAVE_FORMAT_PCM) status = 'U';
    else status = streamFile != nullptr ? 'S' : ' ';
  }
  const int milliseconds = format.basic.samplesPerSec >= 100
                               ? 10 * sampleCount / (format.basic.samplesPerSec / 100)
                               : 0;
  idLib::Printf("%s %2.2fkHz \t%6dms \t%5.1fkB %c %s\n", channels,
                static_cast<float>(format.basic.samplesPerSec) / 1000.0F, milliseconds,
                static_cast<float>(dataLength) / 1024.0F, status, name.str);
}

bool idSoundSample::ReloadIfStale() {
  std::uint32_t sourceTimestamp = static_cast<std::uint32_t>(-1);
  if (cvarSystem->GetCVarBool("s_useCompression", true)) {
    idStr compressed("compressed");
    compressed.AppendPath(name.str);
    compressed.SetFileExtension("vorbis");
    sourceTimestamp = FileTimestamp(compressed.data);
  }
  if (sourceTimestamp == static_cast<std::uint32_t>(-1)) {
    idStr waveName(name.str);
    waveName.SetFileExtension("wav");
    sourceTimestamp = FileTimestamp(waveName.data);
  }
  if (sourceTimestamp != static_cast<std::uint32_t>(-1) &&
      (sourceTimestamp > timestamp || timestamp == static_cast<std::uint32_t>(-1))) {
    LoadResource();
    return true;
  }
  return false;
}

bool idSoundSample::LoadWav(const idStr& filename) {
  idWaveFile wave;
  if (!wave.Open(filename.data)) return false;
  if (const char* error = wave.ReadWaveFormat(&format)) {
    SetResourceError("%s", error);
    return false;
  }
  timestamp = wave.file->Timestamp();
  dataLength = static_cast<int>(wave.SeekToChunk(FourCC('d', 'a', 't', 'a')));
  const std::uint16_t tag = format.basic.formatTag;
  const bool extensiblePCM = tag == WAVE_FORMAT_EXTENSIBLE &&
                             format.extra.extensible.subFormat == idWaveFile::guidPCM;
  int samples = 0;
  if (tag == WAVE_FORMAT_PCM || extensiblePCM) {
    if (format.basic.bitsPerSample != 8 && format.basic.bitsPerSample != 16) {
      SetResourceError("Unsupported bits per sample (%d)", format.basic.bitsPerSample);
      return false;
    }
    if (format.basic.blockSize == 0) return false;
    samples = dataLength / format.basic.blockSize;
  } else if (tag == WAVE_FORMAT_VORBIS) {
    samples = static_cast<int>(format.extra.vorbis.samplesEncoded);
  } else if (tag == WAVE_FORMAT_ADPCM) {
    if (format.basic.blockSize == 0) return false;
    samples = format.extra.adpcm.samplesPerBlock * (dataLength / format.basic.blockSize);
  } else if (tag == WAVE_FORMAT_XMA2) {
    const int seekSize = static_cast<int>(wave.SeekToChunk(FourCC('s', 'e', 'e', 'k')));
    if (seekSize != 4 * format.extra.xma2.blockCount) {
      SetResourceError("Wrong number of entries in seek table");
      return false;
    }
    xma2SeekTable = new int[format.extra.xma2.blockCount];
    for (int index = 0; index < format.extra.xma2.blockCount; ++index) {
      unsigned char bytes[4];
      if (wave.file->Read(bytes, 4) != 4) return false;
      xma2SeekTable[index] = static_cast<int>((std::uint32_t(bytes[0]) << 24) |
                                             (std::uint32_t(bytes[1]) << 16) |
                                             (std::uint32_t(bytes[2]) << 8) | bytes[3]);
    }
    firstValidSample = static_cast<int>(format.extra.xma2.playBegin);
    loopPoint = static_cast<int>(format.extra.xma2.loopBegin);
    lastValidSample = static_cast<int>(format.extra.xma2.playBegin +
                                       format.extra.xma2.playLength);
    const int loopEnd = static_cast<int>(format.extra.xma2.loopBegin +
                                         format.extra.xma2.loopLength);
    if (loopEnd > 0 && loopEnd < lastValidSample) lastValidSample = loopEnd;
  } else if (tag == WAVE_FORMAT_EXTENSIBLE &&
             format.extra.extensible.subFormat == idWaveFile::guidAT9) {
    if (wave.SeekToChunk(FourCC('f', 'a', 'c', 't')) != 12) {
      SetResourceError("No fact chunk");
      return false;
    }
    std::uint32_t fact[3]{};
    if (wave.file->Read(fact, sizeof(fact)) != sizeof(fact)) return false;
    samples = static_cast<int>(fact[0]);
  } else {
    SetResourceError("Unsupported wave format %d", tag);
    return false;
  }

  if (tag != WAVE_FORMAT_XMA2) {
    firstValidSample = 0;
    lastValidSample = samples;
    loopPoint = 0;
    std::uint32_t loopStart = 0;
    std::uint32_t loopEnd = static_cast<std::uint32_t>(samples);
    if (wave.ReadLoopData(&loopStart, &loopEnd)) {
      loopPoint = static_cast<int>(loopStart);
      lastValidSample = static_cast<int>(loopEnd);
    }
  }
  loopPoint = std::max(loopPoint, firstValidSample);
  auto* data = new unsigned char[static_cast<std::size_t>(std::max(dataLength, 0))];
  staticBuffer = data;
  wave.SeekToChunk(FourCC('d', 'a', 't', 'a'));
  if (dataLength > 0 && wave.file->Read(data, dataLength) != static_cast<std::size_t>(dataLength))
    return false;
  wave.Close();
  if (format.basic.formatTag == WAVE_FORMAT_EXTENSIBLE)
    format.basic.formatTag = static_cast<std::uint16_t>(format.extra.extensible.subFormat.data1);
  return true;
}

void idSoundSample::LoadResource() {
  FreeData();
  if (cvarSystem->GetCVarBool("s_noSound", false)) {
    MakeDefault();
    return;
  }
  if (idLib::production >= PROD_PRODUCTION &&
      !cvarSystem->GetCVarBool("sys_localization", false)) {
    if (!LoadBinarySound("vorbis", false)) MakeDefault();
    return;
  }
  if (cvarSystem->GetCVarBool("s_useCompression", true)) {
    idStr compressed("compressed");
    compressed.AppendPath(name.str);
    compressed.SetFileExtension("vorbis");
    if (LoadWav(compressed)) return;
  }
  idStr uncompressed(name.str);
  uncompressed.SetFileExtension("wav");
  if (!LoadWav(uncompressed)) MakeDefault();
}

idSoundSamplePack::idSoundSamplePack() {
  sampleInfos.granularity = 38;
  samples.granularity = 38;
}

idSoundSamplePack::~idSoundSamplePack() {
  for (int index = 0; index < samples.Num(); ++index) delete samples[index];
  samples.ClearFree();
}

idSoundSample* idSoundSamplePack::CreateSample(int index) {
  if (index < 0 || index >= sampleInfos.Num()) return nullptr;
  const sampleInfo_t& info = sampleInfos[index];
  auto* sample = new idSoundSample;
  sample->SetName(info.bsndName.str);
  sample->streamFile = fileSystem ? fileSystem->OpenFileRead(name.data) : nullptr;
  SetOwnedStream(sample, sample->streamFile);
  sample->streamOffset = static_cast<std::uint32_t>(std::max(info.dataOffset, 0));
  if (!sample->LoadBinarySound("", true)) {
    delete sample;
    return nullptr;
  }
  return sample;
}

std::uint64_t idSoundSamplePack::GetTotalStreamSize() const {
  std::uint64_t total = 0;
  for (int index = 0; index < sampleInfos.Num(); ++index)
    total += static_cast<std::uint32_t>(sampleInfos[index].dataSize);
  return total;
}

bool idSoundSamplePack::PinCacheData(pinnedCacheLines_t* lines, bool highPriority) const {
  std::unique_ptr<idFile> file(fileSystem ? fileSystem->OpenFileRead(name.data) : nullptr);
  if (!file) return false;
  const int queued = highPriority ? 0 : 2;
  const bool result = SoundPinCachedData(file.get(), 0, static_cast<int>(GetTotalStreamSize()),
                                         lines, false, StreamPriority(queued), 256);
  if (result) SoundAdoptPinnedFile(lines, file.release());
  return result;
}

idStr idSoundSamplePack::GetBinaryName(const idSoundShader* soundShader,
                                       const char* extension) {
  idStr resourceName(ShaderName(soundShader));
  resourceName.StripFileExtension();
  resourceName.Append('_');
  resourceName.Append(extension ? extension : "");
  return FixLongFilename("generated/sound", "samplepack", resourceName.data);
}

idSoundSamplePack* idSoundSamplePack::Load(const idSoundShader* soundShader,
                                           const char* extension, bool touchFilesOnly) {
  const idStr binaryFilename = GetBinaryName(soundShader, extension);
  std::unique_ptr<idFile> file(fileSystem ? fileSystem->OpenFileRead(binaryFilename.data) : nullptr);
  if (!file) {
    idLib::Warning("Could not open %s", binaryFilename.data);
    return nullptr;
  }
  std::uint32_t magic = 0;
  std::int16_t numSamples = 0;
  if (file->Read(&magic, sizeof(magic)) != sizeof(magic) ||
      file->Read(&numSamples, sizeof(numSamples)) != sizeof(numSamples) ||
      magic != 0x53504B31U || numSamples < 0) {
    idLib::Warning("Corrupt or old sound sample pack");
    return nullptr;
  }
  auto pack = std::make_unique<idSoundSamplePack>();
  pack->name = binaryFilename;
  pack->sampleInfos.SetNum(numSamples);
  for (int index = 0; index < numSamples; ++index) {
    if (!ReadAtomicString(file.get(), pack->sampleInfos[index].bsndName) ||
        file->Read(&pack->sampleInfos[index].dataOffset, 4) != 4 ||
        file->Read(&pack->sampleInfos[index].dataSize, 4) != 4) {
      return nullptr;
    }
  }
  if (touchFilesOnly) {
    for (int index = 0; index < numSamples; ++index) {
      std::unique_ptr<idFile> touched(
          fileSystem->OpenFileRead(pack->sampleInfos[index].bsndName.str));
    }
    return nullptr;
  }
  pack->samples.SetNum(numSamples);
  for (int index = 0; index < numSamples; ++index) {
    pack->samples[index] = pack->CreateSample(index);
  }
  return pack.release();
}
