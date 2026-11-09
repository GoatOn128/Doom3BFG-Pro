#include "engine/sound/files/wavefile.h"

#include "shared/idlib/csystems/cvarsystemlocal.h"
#include "shared/idlib/filesystem/filesystemlocal.h"
#include "shared/idlib/lib.h"

#include <algorithm>
#include <cctype>
#include <cstring>
#include <string>

namespace {

constexpr std::uint32_t FourCC(char a, char b, char c, char d) {
  return (static_cast<std::uint32_t>(static_cast<unsigned char>(a)) << 24) |
         (static_cast<std::uint32_t>(static_cast<unsigned char>(b)) << 16) |
         (static_cast<std::uint32_t>(static_cast<unsigned char>(c)) << 8) |
         static_cast<std::uint32_t>(static_cast<unsigned char>(d));
}

bool ReadFourCC(idFile* file, std::uint32_t& value) {
  unsigned char bytes[4]{};
  if (file->Read(bytes, sizeof(bytes)) != sizeof(bytes)) {
    return false;
  }
  value = FourCC(static_cast<char>(bytes[0]), static_cast<char>(bytes[1]),
                 static_cast<char>(bytes[2]), static_cast<char>(bytes[3]));
  return true;
}

std::string LowerSlashes(const char* name) {
  std::string result = name ? name : "";
  std::transform(result.begin(), result.end(), result.begin(), [](unsigned char value) {
    return value == '\\' ? '/' : static_cast<char>(std::tolower(value));
  });
  return result;
}

bool ReplaceSuffix(std::string& value, const char* from, const char* to) {
  const std::size_t position = value.rfind(from);
  if (position == std::string::npos) {
    return false;
  }
  value.replace(position, std::strlen(from), to);
  return true;
}

}  // namespace

const idWaveFile::guid_t idWaveFile::guidPCM = {
    0x00000001U, 0x0000U, 0x0010U, 0x0080U,
    {0x00U, 0xAAU, 0x00U, 0x38U, 0x9BU, 0x71U}};

const idWaveFile::guid_t idWaveFile::guidAT9 = {
    0x47E142D2U, 0x36BAU, 0x4C85U, 0x8685U,
    {0xC9U, 0x0BU, 0xB5U, 0xCDU, 0xC5U, 0x6AU}};

bool idWaveFile::guid_t::operator==(const guid_t& rhs) const {
  return std::memcmp(this, &rhs, sizeof(*this)) == 0;
}

std::int64_t idWaveFile::SeekToChunk(std::uint32_t id) {
  for (int index = 0; index < chunks.Num(); ++index) {
    if (chunks[index].id == id) {
      file->Seek(chunks[index].offset, FS_SEEK_SET);
      return chunks[index].size;
    }
  }
  return 0;
}

bool idWaveFile::ReadLoopData(std::uint32_t* start, std::uint32_t* end) {
  const std::int64_t chunkSize = SeekToChunk(FourCC('s', 'm', 'p', 'l'));
  if (chunkSize < static_cast<std::int64_t>(sizeof(samplerChunk_t))) {
    return false;
  }

  samplerChunk_t sampler{};
  file->Read(&sampler, sizeof(sampler));
  if (sampler.numSampleLoops == 0) {
    return false;
  }

  sampleData_t loop{};
  file->Read(&loop, sizeof(loop));
  if (loop.type != 0) {
    idLib::Warning("Invalid loop type in %s", file->GetName());
    return false;
  }
  *start = loop.start;
  *end = loop.end;
  return true;
}

const char* idWaveFile::ReadWaveFormatDirect(waveFmt_t* format, idFile* source) {
  if (source->Read(&format->basic, sizeof(format->basic)) != sizeof(format->basic)) {
    return "Format chunk too small";
  }
  if (format->basic.formatTag == 1) {
    return nullptr;
  }

  if (source->Read(&format->extraSize, sizeof(format->extraSize)) !=
      sizeof(format->extraSize)) {
    return "Format chunk too small";
  }

  switch (format->basic.formatTag) {
    case 0x674F:
      if (format->extraSize != 4) {
        return "Incorrect number of coefficients in VORBIS file";
      }
      source->Read(&format->extra, 4);
      return nullptr;
    case 2:
      if (format->extraSize != 32) {
        return "Incorrect number of coefficients in ADPCM file";
      }
      source->Read(&format->extra, 32);
      return nullptr;
    case 0x0166:
      if (format->extraSize != 34) {
        return "Incorrect chunk size in XMA2 file";
      }
      source->Read(&format->extra, 34);
      return nullptr;
    case 0xFFFE:
    case 0x42D2:
      if (format->extraSize > 34) {
        return "Incorrect chunk size in extensible wave file";
      }
      source->Read(&format->extra, format->extraSize);
      if (format->extra.extensible.subFormat == guidPCM ||
          format->extra.extensible.subFormat == guidAT9) {
        return nullptr;
      }
      return "Unsupported extensible format type";
    default:
      return "Unsupported wave format tag";
  }
}

const char* idWaveFile::ReadWaveFormat(waveFmt_t* format) {
  std::memset(format, 0, sizeof(*format));
  const std::int64_t size = SeekToChunk(FourCC('f', 'm', 't', ' '));
  if (size == 0) {
    return "No format chunk";
  }
  if (size < static_cast<std::int64_t>(sizeof(waveFmt_t::basic_t))) {
    return "Format chunk too small";
  }
  return ReadWaveFormatDirect(format, file);
}

void idWaveFile::Close() {
  delete file;
  file = nullptr;
  chunks.Clear();
}

bool idWaveFile::Open(const char* filename) {
  Close();
  if (filename == nullptr || filename[0] == '\0') {
    return false;
  }

  const char* localizedName = filename;
  std::string voiceName;
  if (cvarSystem->GetCVarInteger("sys_localization", 0) != 0) {
    voiceName = LowerSlashes(localizedName);
    if (voiceName.find("/vo/") != std::string::npos) {
      ReplaceSuffix(voiceName, ".msadpcm", ".wav");
      file = fileSystem->OpenFileRead(voiceName.c_str());
      if (file == nullptr && ReplaceSuffix(voiceName, ".wav", ".msadpcm")) {
        file = fileSystem->OpenFileRead(voiceName.c_str());
      }
    }
  }
  if (file == nullptr) {
    file = fileSystem->OpenFileRead(localizedName);
  }
  if (file == nullptr || file->Length() == 0) {
    Close();
    return false;
  }

  std::uint32_t riff = 0;
  std::uint32_t riffSize = 0;
  std::uint32_t wave = 0;
  if (!ReadFourCC(file, riff) || file->Read(&riffSize, sizeof(riffSize)) != sizeof(riffSize) ||
      !ReadFourCC(file, wave) || riff != FourCC('R', 'I', 'F', 'F') ||
      wave != FourCC('W', 'A', 'V', 'E') || riffSize < 4) {
    Close();
    idLib::Warning("Header is not RIFF WAVE in %s", localizedName);
    return false;
  }

  const std::uint64_t riffEnd = static_cast<std::uint64_t>(riffSize) + 8U;
  std::uint32_t offset = 12;
  while (offset < riffEnd) {
    std::uint32_t id = 0;
    std::uint32_t size = 0;
    if (!ReadFourCC(file, id) || file->Read(&size, sizeof(size)) != sizeof(size)) {
      break;
    }
    if (chunks.Num() >= 32) {
      Close();
      idLib::Warning("More than %d chunks in %s", 32, localizedName);
      return false;
    }
    chunk_t* chunk = chunks.Alloc();
    chunk->id = id;
    chunk->size = size;
    chunk->offset = offset + 8;
    offset = chunk->offset + size;
    file->Seek(offset, FS_SEEK_SET);
    if (offset >= riffEnd) {
      return true;
    }
  }
  return true;
}
