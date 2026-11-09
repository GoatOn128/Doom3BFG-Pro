#include "engine/sound/decoders/sounddecoder_vorbis.h"

#include "engine/sound/soundsample.h"
#include "shared/idlib/lib.h"

#include <algorithm>
#include <cstdio>
#include <cstring>
#define OV_EXCLUDE_STATIC_CALLBACKS
#include <vorbis/vorbisfile.h>

std::size_t ReadOgg(void* destination, std::size_t elementSize,
                    std::size_t elementCount, void* dataSource) {
  auto* file = static_cast<vorbisMemoryFile_t*>(dataSource);
  if (file == nullptr || destination == nullptr || elementSize == 0) return 0;
  const std::size_t requested = elementSize * elementCount;
  const std::size_t current = static_cast<std::size_t>(file->curPtr - file->filePtr);
  const std::size_t available = current < file->fileSize
                                    ? static_cast<std::size_t>(file->fileSize - current)
                                    : 0;
  const std::size_t bytes = std::min(requested, available);
  if (bytes != 0) {
    std::memmove(destination, file->curPtr, bytes);
    file->curPtr += bytes;
  }
  // Retail returns bytes rather than complete element count. Its vorbisfile
  // callbacks only request byte-sized elements.
  return bytes;
}

int SeekOgg(void* dataSource, std::int64_t offset, int origin) {
  auto* file = static_cast<vorbisMemoryFile_t*>(dataSource);
  if (file == nullptr) return -1;
  unsigned char* destination = nullptr;
  switch (origin) {
    case SEEK_SET:
      destination = file->filePtr + offset;
      break;
    case SEEK_CUR:
      destination = file->curPtr + offset;
      break;
    case SEEK_END:
      destination = file->filePtr + file->fileSize - offset;
      break;
    default:
      return -1;
  }
  unsigned char* const end = file->filePtr + file->fileSize;
  if (destination < file->filePtr) {
    file->curPtr = file->filePtr;
    return -1;
  }
  if (destination > end) {
    file->curPtr = end;
    return -1;
  }
  file->curPtr = destination;
  return 0;
}

int CloseOgg(void*) { return 0; }

long TellOgg(void* dataSource) {
  const auto* file = static_cast<const vorbisMemoryFile_t*>(dataSource);
  return file == nullptr ? 0 : static_cast<long>(file->curPtr - file->filePtr);
}

idSoundDecoder_Vorbis::idSoundDecoder_Vorbis()
    : memFile{}, sample(nullptr), vorbisFile(nullptr) {}

idSoundDecoder_Vorbis::~idSoundDecoder_Vorbis() {
  Close();
  delete vorbisFile;
  vorbisFile = nullptr;
}

void idSoundDecoder_Vorbis::Close() {
  if (vorbisFile != nullptr) ov_clear(vorbisFile);
  sample = nullptr;
}

void idSoundDecoder_Vorbis::Seek(int samplePosition) {
  if (vorbisFile != nullptr) ov_pcm_seek(vorbisFile, samplePosition);
}

std::int64_t idSoundDecoder_Vorbis::Read(char* buffer, int bufferSize) {
  if (vorbisFile == nullptr || buffer == nullptr || bufferSize < 0) return -1;
  char* cursor = buffer;
  int currentBitstream = 0;
  while (bufferSize > 0) {
    const int request = std::min(bufferSize, 4096);
    const long decoded = ov_read(vorbisFile, cursor, request, 0, 2, 1,
                                 &currentBitstream);
    if (decoded == 0) return cursor - buffer;
    if (decoded < 0) {
      idLib::Printf("Error in vorbis stream\n");
      return -1;
    }
    bufferSize -= static_cast<int>(decoded);
    cursor += decoded;
  }
  return cursor - buffer;
}

bool idSoundDecoder_Vorbis::IsEOS() const {
  if (vorbisFile == nullptr) return true;
  const ogg_int64_t total = ov_pcm_total(vorbisFile, -1);
  return ov_pcm_tell(vorbisFile) >= total;
}

bool idSoundDecoder_Vorbis::Open(const idSoundSample* newSample) {
  if (newSample == nullptr || newSample->staticBuffer == nullptr ||
      newSample->dataLength <= 0) return false;
  if (newSample == sample) return true;
  if (vorbisFile != nullptr) ov_clear(vorbisFile);
  sample = nullptr;
  memFile.filePtr = static_cast<unsigned char*>(newSample->staticBuffer);
  memFile.curPtr = memFile.filePtr;
  memFile.fileSize = static_cast<std::uint64_t>(newSample->dataLength);
  if (vorbisFile == nullptr) vorbisFile = new OggVorbis_File{};
  ov_callbacks callbacks{};
  callbacks.read_func = ReadOgg;
  callbacks.seek_func = SeekOgg;
  callbacks.close_func = CloseOgg;
  callbacks.tell_func = TellOgg;
  if (ov_open_callbacks(&memFile, vorbisFile, nullptr, 0, callbacks) != 0) {
    idLib::Warning("Failed to open vorbis sample '%s'", newSample->name.str);
    ov_clear(vorbisFile);
    return false;
  }
  sample = newSample;
  return true;
}
