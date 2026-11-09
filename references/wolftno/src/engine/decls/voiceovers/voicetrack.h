#pragma once

#include "declvisemeset.h"
#include "engine/decls/decl.h"
#include "engine/sound/streamcache.h"
#include "shared/idlib/langdict.h"
#include "shared/idlib/containers/staticlist.h"
#include "voicetrack_events.h"

#include <cstdint>

class idSoundShader;
class idDeclMD6;

class idVoiceTrack : public idResource {
public:
  enum eventOffset_t { EVENTOFFSET_BACKWARDS = -1, EVENTOFFSET_NONE = 0, EVENTOFFSET_FORWARDS = 1 };
  struct idFrameRef { std::uint16_t offset = 0; std::uint16_t numIndices = 0; };
  struct frameInfo_t { int frame = 0; int loop = 0; };
  struct idSubtitleText { int startMS = 0; idStr text; };
  struct flags_t {
    std::uint8_t cycleAnim : 1;
    std::uint8_t clearAnimAfter : 1;
    std::uint8_t noLipSync : 1;
    std::uint8_t suppressBlinking : 1;
    std::uint8_t compressed : 1;
    std::uint8_t edited : 1;
    std::uint8_t useRealTime : 1;
    std::uint8_t useExactDuration : 1;
  };
  class idFrameRefIndexBuffer {
  public:
    idFrameRefIndexBuffer() = default;
    ~idFrameRefIndexBuffer();
    void Alloc(std::uint16_t count);
    bool Write(idFile* file) const;
    bool Read(idFile* file);
    std::uint16_t curOffset = 0;
    std::uint16_t size = 0;
    std::uint16_t* indices = nullptr;
  };

  explicit idVoiceTrack(const char* defaultVisemeSetName = nullptr);
  ~idVoiceTrack() override;
  void LoadResource() override;
  bool ReloadIfStale() override;
  idResourceList* GetResourceList() const override;
  void Free();
  void Parse(idLexer& source);
  bool LoadText(const char* fileName);
  bool LoadText(const char* text, int length);
  bool LoadBinary(const char* fileName, std::uint32_t sourceTimestamp);
  bool LoadBinary(idFile* file);
  bool WriteBinary(const char* fileName) const;
  bool WriteBinary(idFile* file) const;
  void BuildFrameRefs();
  void OffsetEvents(eventOffset_t offset, bool visemesOnly);
  void LoadAnims();
  void PinStreamedSoundData();
  int FindLastEvent() const;
  int FindNextEventForFrame(int startIndex, int frame) const;
  int FindApproximateDuration() const;
  int FindExactDuration(const idDeclMD6* declMD6) const;
  void GetEvents(int frame, const int* curTimeMS, const int* lastTimeMS, voiceEventType_t type,
                 idStaticList<const idVoiceEvent*,32>* visemeEvents,
                 idStaticList<const idVoiceEvent*,32>* nonVisemeEvents) const;
  void GetEvents(int* curTimeMS, int* lastTimeMS, int* loopLengthMS, voiceEventType_t type,
                 idStaticList<const idVoiceEvent*,32>& visemeEvents,
                 idStaticList<const idVoiceEvent*,32>& nonVisemeEvents) const;
  int GetSubtitles(idList<idSubtitleText>& result, bool mergePunctuation = true) const;
  std::size_t Memory() const;
  std::uint8_t GetVisemeForName(const char* name) const;
  const char* GetVisemeName(std::uint8_t index) const;

  static mood_t GetMoodForName(const char* name);
  static faceChannel_t GetFaceChannelForName(const char* name);
  static const char* GetMoodAnimName(unsigned mood);
  static const char* GetMoodName(unsigned mood);
  static const char* GetFaceChannelName(faceChannel_t channel);

  idList<vtrEventHandle_t, 66> events;
  idVoiceEventManager voiceEventManager;
  idFrameRef* frameRefs;
  std::int16_t numFrameRefs;
  idFrameRefIndexBuffer indexBuffer;
  bool rebuildFrameRefs;
  std::int8_t visemeOffsetState;
  std::int8_t offsetState;
  idAtomicString characterIdentifier;
  idAtomicString textLangKey;
  idStrId textId;
  std::uint32_t textCRC;
  std::uint32_t lipsyncTextCRC;
  idAtomicString subtitleLangKey;
  idStrId subtitleId;
  const idSoundShader* soundShader;
  pinnedCacheLines_t* pinnedStreamData;
  std::uint32_t numPinnedStreamData;
  std::uint32_t sampleTimestamp;
  std::uint32_t sampleCRC;
  idStr animName;
  faceChannel_t faceChannel;
  std::int16_t blendInMS;
  std::int16_t blendOutMS;
  std::uint32_t soundOffsetMS;
  std::uint32_t visemeOffsetMS;
  std::uint32_t animEventOffsetMS;
  std::uint32_t moodEventOffsetMS;
  std::uint32_t gameEventOffsetMS;
  std::uint32_t textEventOffsetMS;
  flags_t flags;
  float weightScale;
  mood_t defaultMood;
  const idDeclVisemeSet* declVisemeSet;
  std::uint32_t timestamp;
  int approximateDuration;

  static idResourceList resourceList;
  static const char* VOICE_TRACK_NAME;
  static const char* DEFAULT_VISEME_SET_NAME;
  static const char* const moodNames[MOOD_MAX];
  static const char* const moodAnimNames[MOOD_MAX];
  static const char* const faceChannelNames[FACE_CHANNEL_MAX];
};

static_assert(sizeof(idVoiceTrack::frameInfo_t)==8,"voice frame-info ABI drift");
static_assert(sizeof(idVoiceTrack::idFrameRefIndexBuffer)==16,"voice frame-index buffer ABI drift");
static_assert(sizeof(idVoiceEventManager)==144,"voice event manager ABI drift");
static_assert(sizeof(idVoiceTrack)==440,"voice track ABI drift");

struct idVoiceTrackFileHeader {
  std::uint32_t timestamp;
  std::uint32_t textCRC;
  std::uint32_t lipsyncTextCRC;
  std::uint32_t sampleTimestamp;
  std::uint32_t sampleCRC;
  std::int16_t blendInMS;
  std::int16_t blendOutMS;
  std::uint32_t soundOffsetMS;
  std::uint32_t visemeOffsetMS;
  std::uint32_t animEventOffsetMS;
  std::uint32_t moodEventOffsetMS;
  std::uint32_t gameEventOffsetMS;
  std::uint32_t textEventOffsetMS;
  float weightScale;
  std::int16_t numFrameRefs;
  std::uint8_t defaultMood;
  std::uint8_t faceChannel;
  std::uint32_t flags;
  std::int32_t approximateDuration;
  std::int32_t numEvents;
  void Swap();
};
static_assert(sizeof(idVoiceTrackFileHeader) == 68, "voice-track binary header ABI drift");
