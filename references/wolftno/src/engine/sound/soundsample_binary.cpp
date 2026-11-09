#include "engine/sound/soundsample.h"

#include "shared/idlib/csystems/cvarsystemlocal.h"
#include "shared/idlib/filesystem/filesystemlocal.h"
#include "shared/idlib/lib.h"

#include <algorithm>
#include <cstring>
#include <memory>
#include <type_traits>

namespace {

template <typename T> bool ReadBE(idFile* file, T& value) {
  unsigned char bytes[sizeof(T)]{};
  if (file == nullptr || file->Read(bytes, sizeof(bytes)) != sizeof(bytes)) return false;
  value = 0;
  for (unsigned char byte : bytes)
    value = static_cast<T>((static_cast<std::make_unsigned_t<T>>(value) << 8) | byte);
  return true;
}

bool ReadAtomicString(idFile* file, idAtomicString& value) {
  std::int32_t length = 0;
  if (file == nullptr || file->Read(&length, sizeof(length)) != sizeof(length) ||
      length < 0 || length > 16 * 1024 * 1024) return false;
  idStr text;
  text.Fill('\0', length);
  if (length > 0 && file->Read(text.data, length) != static_cast<std::size_t>(length))
    return false;
  text.data[length] = '\0';
  text.len = length;
  value.Set(text.data);
  return true;
}

bool ContainsLanguage(const char* list, const char* language) {
  return idStr::Find(list ? list : "", language ? language : "", false) != -1;
}

}  // namespace

void idLocalizedSampleOnDisc::Read(idFile* file) {
  std::memset(this, 0, sizeof(*this));
  if (file == nullptr || file->Read(lang, sizeof(lang)) != sizeof(lang) ||
      !ReadBE(file, size) || !ReadBE(file, offset)) {
    offset = -1;
  }
  lang[sizeof(lang) - 1] = '\0';
}

bool idSoundSample::LoadBinarySound(const char* extension, bool isSamplePack) {
  idStr filename("generated");
  if (isSamplePack) {
    filename = name.str;
  } else {
    filename.AppendPath(name.str);
    filename.StripFileExtension();
    filename.Append('_');
    filename.Append(extension ? extension : "");
    filename.Append(".bsnd");
  }

  std::unique_ptr<idFile> metadata(fileSystem ? fileSystem->OpenFileRead(filename.data) : nullptr);
  if (!metadata) return false;
  std::uint32_t magic = 0;
  if (!ReadBE(metadata.get(), magic) || magic != 0x62736E66U) {
    SetResourceError("%s had invalid magic", filename.data);
    return false;
  }
  int numHeaders = 0;
  if (!ReadBE(metadata.get(), numHeaders) || numHeaders <= 0) {
    SetResourceError("%s had invalid localized header count", filename.data);
    return false;
  }

  const char* language = "english";
  const char* configuredLanguage = cvarSystem->GetCVarString("sys_lang", "english");
  if (ContainsLanguage(cvarSystem->GetCVarString("vo_localizeLangs", ""),
                       configuredLanguage))
    language = configuredLanguage;

  idLocalizedSampleOnDisc selected{};
  selected.offset = -1;
  bool localized = numHeaders > 1;
  for (int index = 0; index < numHeaders; ++index) {
    idLocalizedSampleOnDisc candidate{};
    candidate.Read(metadata.get());
    if (candidate.offset < 0) break;
    if (numHeaders == 1 && candidate.lang[0] == '\0') {
      selected = candidate;
      localized = false;
      break;
    }
    localized = true;
    if (idStr::Icmp(candidate.lang, language) == 0) {
      selected = candidate;
      break;
    }
  }
  if (selected.offset < 0) {
    SetResourceError("could not find %s version of %s", language, filename.data);
    idLib::Warning("could not find %s version of '%s'", language, filename.data);
    return false;
  }
  if (metadata->Seek(selected.offset, FS_SEEK_SET) != 0) return false;

  if (!ReadBE(metadata.get(), timestamp) || !ReadBE(metadata.get(), firstValidSample) ||
      !ReadBE(metadata.get(), lastValidSample) || !ReadBE(metadata.get(), loopPoint) ||
      !ReadBE(metadata.get(), dataLength))
    return false;
  if (const char* error = idWaveFile::ReadWaveFormatDirect(&format, metadata.get())) {
    SetResourceError("%s", error);
    return false;
  }
  if (format.basic.formatTag == 0x0166) {
    xma2SeekTable = new int[format.extra.xma2.blockCount];
    for (int index = 0; index < format.extra.xma2.blockCount; ++index)
      if (!ReadBE(metadata.get(), xma2SeekTable[index])) return false;
  }
  if (format.basic.formatTag == 85) {
    if (!ReadBE(metadata.get(), mp3SeekTableEntries) || mp3SeekTableEntries < 0)
      return false;
    mp3SeekTable = new mp3SeekTable_t[mp3SeekTableEntries];
    for (int index = 0; index < mp3SeekTableEntries; ++index) {
      if (!ReadBE(metadata.get(), mp3SeekTable[index].numSamples) ||
          !ReadBE(metadata.get(), mp3SeekTable[index].offset)) return false;
    }
  }

  if (!isSamplePack) {
    streamOffset = static_cast<std::uint32_t>(metadata->Tell());
    streamFile = fileSystem->OpenFileRead(filename.data);
    if (streamFile == nullptr) return false;
    // Standalone reconstruction owns this duplicate stream handle. Retail gets
    // the same underlying handle from idResourceManager's stream-file table.
    extern void SoundSampleAdoptStream(const idSoundSample*, idFile*);
    SoundSampleAdoptStream(this, streamFile);
  }
  (void)localized;
  if (format.basic.formatTag == 0x674F && streamFile != nullptr && dataLength > 0) {
    auto* data = new unsigned char[static_cast<std::size_t>(dataLength)];
    if (streamFile->ReadOfs(streamOffset, data, dataLength) !=
        static_cast<std::size_t>(dataLength)) {
      delete[] data;
      return false;
    }
    staticBuffer = data;
    streamFile = nullptr;
    streamOffset = 0;
  }
  return true;
}
