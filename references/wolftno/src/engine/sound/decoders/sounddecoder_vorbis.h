#pragma once

#include <cstddef>
#include <cstdint>

struct OggVorbis_File;
class idSoundSample;

struct vorbisMemoryFile_t {
  unsigned char* curPtr;
  unsigned char* filePtr;
  std::uint64_t fileSize;
};

std::size_t ReadOgg(void* destination, std::size_t elementSize, std::size_t elementCount,
                    void* dataSource);
int SeekOgg(void* dataSource, std::int64_t offset, int origin);
int CloseOgg(void* dataSource);
long TellOgg(void* dataSource);

class idSoundDecoder_Vorbis {
public:
  idSoundDecoder_Vorbis();
  ~idSoundDecoder_Vorbis();

  bool Open(const idSoundSample* newSample);
  void Close();
  void Seek(int samplePosition);
  std::int64_t Read(char* buffer, int bufferSize);
  bool IsEOS() const;

  vorbisMemoryFile_t memFile;
  const idSoundSample* sample;
  OggVorbis_File* vorbisFile;
};

static_assert(sizeof(vorbisMemoryFile_t) == 24);
static_assert(sizeof(idSoundDecoder_Vorbis) == 40,
              "idSoundDecoder_Vorbis ABI drift");

