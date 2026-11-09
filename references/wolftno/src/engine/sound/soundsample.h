#pragma once

#include "engine/decls/decl.h"
#include "engine/sound/files/wavefile.h"
#include "engine/sound/streamcache.h"

#include <cstdint>

class idSoundShader;

struct idLocalizedSampleOnDisc {
  void Read(idFile* file);

  char lang[16];
  int size;
  int offset;
};

class idSoundSample final : public idResource {
public:
  struct mp3SeekTable_t {
    std::uint16_t numSamples;
    std::uint16_t offset;
  };

  idSoundSample();
  ~idSoundSample() override;

  void LoadResource() override;
  bool ReloadIfStale() override;
  idResourceList* GetResourceList() const override;
  void List() const override;

  int GetLengthInMsec() const;

  bool GetSeekPosition(int sampleOffset, int* byteOffset,
                       std::uint32_t* samplesToSkip) const;
  void MakeDefault();
  void FreeData();
  int ReadCachedSoundData(int offset, void* buffer, int length, int queued,
                          bool blocking) const;
  bool PinStreamData(pinnedCacheLines_t* lines) const;
  int PreCacheStreamData(bool highPriority) const;
  bool LoadWav(const idStr& filename);
  bool LoadBinarySound(const char* extension, bool isSamplePack);

  static idTypedResourceList<idSoundSample> resourceList;

  std::uint32_t timestamp;
  int firstValidSample;
  int lastValidSample;
  int loopPoint;
  int dataLength;
  void* staticBuffer;
  idFile* streamFile;
  std::uint32_t streamOffset;
  idWaveFile::waveFmt_t format;
  int* xma2SeekTable;
  int mp3SeekTableEntries;
  mp3SeekTable_t* mp3SeekTable;
};

class idSoundSamplePack {
public:
  struct sampleInfo_t {
    idAtomicString bsndName;
    int dataOffset;
    int dataSize;
  };

  idSoundSamplePack();
  ~idSoundSamplePack();

  idSoundSample* CreateSample(int index);
  std::uint64_t GetTotalStreamSize() const;
  bool PinCacheData(pinnedCacheLines_t* lines, bool highPriority) const;

  static idStr GetBinaryName(const idSoundShader* soundShader, const char* extension);
  static idSoundSamplePack* Load(const idSoundShader* soundShader, const char* extension,
                                 bool touchFilesOnly);

  idList<sampleInfo_t, 38> sampleInfos;
  idList<idSoundSample*, 38> samples;
  idStr name;
};

extern void (*soundStopVoicesWithSample)(const idSoundSample* sample);

static_assert(sizeof(idLocalizedSampleOnDisc) == 24);
static_assert(sizeof(idSoundSample::mp3SeekTable_t) == 4);
static_assert(sizeof(idSoundSample) == 168, "idSoundSample ABI drift");
static_assert(sizeof(idSoundSamplePack::sampleInfo_t) == 16);
static_assert(sizeof(idSoundSamplePack) == 88, "idSoundSamplePack ABI drift");
