#pragma once

#include "shared/idlib/containers/list.h"
#include "shared/idlib/filesystem/file.h"
#include "shared/idlib/text/atomicstring.h"
#include "shared/idlib/text/lexer.h"

#include <cstddef>
#include <cstdint>

class idVoiceTrack;

enum mood_t : std::int32_t {
  MOOD_NONE,
  MOOD_CLEAR,
  MOOD_HAPPY,
  MOOD_SAD,
  MOOD_ANGRY,
  MOOD_SUSPICIOUS,
  MOOD_DRUNK,
  MOOD_SLEEPY,
  MOOD_NERVOUS,
  MOOD_DEAD,
  MOOD_MAX
};

enum faceChannel_t : std::int32_t {
  FACE_CHANNEL_BODY,
  FACE_CHANNEL_LEGS,
  FACE_CHANNEL_TORSO,
  FACE_CHANNEL_HEAD,
  FACE_CHANNEL_EYES,
  FACE_CHANNEL_EMOTE_MOUTH,
  FACE_CHANNEL_EMOTE_BROW,
  FACE_CHANNEL_EMOTE_FACE,
  FACE_CHANNEL_FACE,
  FACE_CHANNEL_BODYWEB,
  FACE_CHANNEL_USER,
  FACE_CHANNEL_MAX
};

enum voiceEventType_t : std::int16_t {
  VOICE_EVENT_VISEME_SIMPLE = 0,
  VOICE_EVENT_VISEME_FRAME = 1,
  VOICE_EVENT_GAME = 2,
  VOICE_EVENT_TEXT = 3,
  VOICE_EVENT_PUNCTUATION = 4,
  VOICE_EVENT_ANIM = 5,
  VOICE_EVENT_MOOD = 6,
  VOICE_EVENT_INVALID = 7
};

struct vtrEventHandle_t {
  std::int16_t eventType = VOICE_EVENT_INVALID;
  std::int16_t index = -1;

  static bool WriteArray(idFile* file, const vtrEventHandle_t* handles, int count);
  static bool ReadArray(idFile* file, vtrEventHandle_t* handles, int count);
  bool IsValid() const { return eventType >= 0 && eventType < VOICE_EVENT_INVALID && index >= 0; }
};

class idVoiceEvent {
public:
  idVoiceEvent(voiceEventType_t type = VOICE_EVENT_INVALID)
      : startMS(-1), durationMS(0), eventType(static_cast<std::uint8_t>(type)) {}
  virtual ~idVoiceEvent();
  virtual const char* GetName() const { return "invalid"; }
  virtual float GetWeight(int curTimeMS, int gameMsPerFrame) const;
  virtual bool IsInstantaneous() const { return false; }
  virtual bool IsWithinInterval(int startIntervalMS, int endIntervalMS) const;
  virtual void Write(const idVoiceTrack* track, idFile_String& file) const;
  virtual void Parse(idVoiceTrack* track, idLexer& source);
  virtual bool WriteBinary(idFile* file) const;
  virtual bool LoadBinary(idFile* file);
  virtual std::size_t Memory() const { return sizeof(*this); }

  int startMS;
  std::uint16_t durationMS;
  std::uint8_t eventType;

  static const char* const voiceEventNames[VOICE_EVENT_INVALID];
};

class idVisemeEvent : public idVoiceEvent {
public:
  idVisemeEvent(voiceEventType_t type=VOICE_EVENT_INVALID,std::uint8_t viseme=255):idVoiceEvent(type),visemeType(viseme){}
  bool IsInstantaneous() const override { return true; }
  std::uint8_t visemeType;
};

class idViseme_Simple final : public idVisemeEvent {
public:
  idViseme_Simple();
  const char* GetName() const override;
  float GetWeight(int curTimeMS, int gameMsPerFrame) const override;
  void Write(const idVoiceTrack* track, idFile_String& file) const override;
  void Parse(idVoiceTrack* track, idLexer& source) override;
  bool WriteBinary(idFile* file) const override;
  bool LoadBinary(idFile* file) override;
  std::size_t Memory() const override { return sizeof(*this); }
  virtual bool operator==(const idViseme_Simple& other) const;
  idViseme_Simple& operator=(const idViseme_Simple& other);

  std::uint16_t peakTimeMS;
  std::uint8_t peakWeight;
  static const char* NAME;
};

class idViseme_Frame final : public idVisemeEvent {
public:
  idViseme_Frame();
  idViseme_Frame(const idViseme_Frame& other);
  ~idViseme_Frame() override;
  const char* GetName() const override;
  float GetWeight(int curTimeMS, int gameMsPerFrame) const override;
  void Write(const idVoiceTrack* track, idFile_String& file) const override;
  void Parse(idVoiceTrack* track, idLexer& source) override;
  bool WriteBinary(idFile* file) const override;
  bool LoadBinary(idFile* file) override;
  std::size_t Memory() const override;
  virtual bool operator==(const idViseme_Frame& other) const;
  idViseme_Frame& operator=(const idViseme_Frame& other);

  std::uint8_t numWeights;
  std::uint8_t* weights;
  static const char* NAME;
};

class idVarArgs2 {
public:
  idVarArgs2();
  idVarArgs2(const idVarArgs2& other);
  idVarArgs2& operator=(const idVarArgs2& other);
  ~idVarArgs2();
  void Clear();
  bool Append(const char* value);
  int Num() const { return numArgs; }
  const char* operator[](int index) const;
  std::size_t Memory() const { return buffSize; }
  std::uint16_t numArgs;
  std::uint16_t argSize;
  std::uint16_t buffSize;
  std::uint16_t argOffsets[2];
  std::uint8_t argTypes[2];
  std::uint8_t argExTypes[2];
  std::uint8_t* args;
};

class idVoiceGameEvent final : public idVoiceEvent {
public:
  idVoiceGameEvent();
  ~idVoiceGameEvent() override;
  const char* GetName() const override;
  float GetWeight(int curTimeMS, int gameMsPerFrame) const override;
  void Write(const idVoiceTrack* track, idFile_String& file) const override;
  void Parse(idVoiceTrack* track, idLexer& source) override;
  bool WriteBinary(idFile* file) const override;
  bool LoadBinary(idFile* file) override;
  std::size_t Memory() const override;
  virtual bool operator==(const idVoiceGameEvent& other) const;
  idVoiceGameEvent& operator=(const idVoiceGameEvent& other);

  std::int16_t eventNum;
  idVarArgs2 args;
  static const char* NAME;
};

static_assert(sizeof(idVoiceEvent)==16,"voice event ABI drift");
static_assert(sizeof(idVisemeEvent)==24,"viseme event ABI drift");
static_assert(sizeof(idViseme_Simple)==32,"simple viseme ABI drift");
static_assert(sizeof(idViseme_Frame)==40,"frame viseme ABI drift");
static_assert(sizeof(idVarArgs2)==24,"voice game args ABI drift");
static_assert(sizeof(idVoiceGameEvent)==48,"voice game event ABI drift");

class idVoiceTextEvent final : public idVoiceEvent {
public:
  idVoiceTextEvent();
  const char* GetName() const override;
  float GetWeight(int curTimeMS, int gameMsPerFrame) const override;
  void Write(const idVoiceTrack* track, idFile_String& file) const override;
  void Parse(idVoiceTrack* track, idLexer& source) override;
  bool WriteBinary(idFile* file) const override;
  bool LoadBinary(idFile* file) override;
  std::size_t Memory() const override;
  virtual bool operator==(const idVoiceTextEvent& other) const;
  idVoiceTextEvent& operator=(const idVoiceTextEvent& other);

  idStr text;
  static const char* NAME;
};

class idVoicePunctuationEvent final : public idVoiceEvent {
public:
  idVoicePunctuationEvent();
  const char* GetName() const override;
  void Write(const idVoiceTrack* track, idFile_String& file) const override;
  void Parse(idVoiceTrack* track, idLexer& source) override;
  bool WriteBinary(idFile* file) const override;
  bool LoadBinary(idFile* file) override;
  virtual bool operator==(const idVoicePunctuationEvent& other) const;
  idVoicePunctuationEvent& operator=(const idVoicePunctuationEvent& other);

  char punctuation;
  static const char* NAME;
};

class idVoiceAnimEvent final : public idVoiceEvent {
public:
  idVoiceAnimEvent();
  const char* GetName() const override;
  void Write(const idVoiceTrack* track, idFile_String& file) const override;
  void Parse(idVoiceTrack* track, idLexer& source) override;
  bool WriteBinary(idFile* file) const override;
  bool LoadBinary(idFile* file) override;
  std::size_t Memory() const override;
  virtual bool operator==(const idVoiceAnimEvent& other) const;
  idVoiceAnimEvent& operator=(const idVoiceAnimEvent& other);
  float GetRate() const;

  idAtomicString animName;
  std::int16_t channel;
  std::uint16_t blendInDurationMS;
  std::uint16_t blendOutDurationMS;
  std::uint16_t startFrame;
  std::uint16_t rate;
  bool looping;
  static const char* NAME;
};

class idVoiceMoodEvent final : public idVoiceEvent {
public:
  idVoiceMoodEvent();
  const char* GetName() const override;
  void Write(const idVoiceTrack* track, idFile_String& file) const override;
  void Parse(idVoiceTrack* track, idLexer& source) override;
  bool WriteBinary(idFile* file) const override;
  bool LoadBinary(idFile* file) override;
  virtual bool operator==(const idVoiceMoodEvent& other) const;
  idVoiceMoodEvent& operator=(const idVoiceMoodEvent& other);

  mood_t mood;
  static const char* NAME;
};

static_assert(sizeof(idVoiceTextEvent)==56,"voice text event ABI drift");
static_assert(sizeof(idVoicePunctuationEvent)==24,"voice punctuation event ABI drift");
static_assert(sizeof(idVoiceAnimEvent)==40,"voice animation event ABI drift");
static_assert(sizeof(idVoiceMoodEvent)==24,"voice mood event ABI drift");

struct idVoiceEventManagerFileHeader {
  std::uint32_t version;
  std::uint32_t numViseme;
  std::uint32_t numGame;
  std::uint32_t numText;
  std::uint32_t numPunc;
  std::uint32_t numAnim;
  std::uint32_t numMood;
  void Swap();
};

class idVoiceEventManager {
public:
  idVoiceEventManager();
  ~idVoiceEventManager();
  idVoiceEvent* GetEvent(vtrEventHandle_t handle);
  const idVoiceEvent* GetEvent(vtrEventHandle_t handle) const;
  vtrEventHandle_t AllocEvent(const char* name);
  void Free(idList<vtrEventHandle_t, 66>& eventList);
  void Condense(idList<vtrEventHandle_t, 66>& eventList);
  void WriteBinary(idFile* file) const;
  void LoadBinary(idFile* file);

  idList<idViseme_Simple, 66> eventsVisemeSimple;
  idList<idVoiceGameEvent, 66> eventsGame;
  idList<idVoiceTextEvent, 66> eventsText;
  idList<idVoicePunctuationEvent, 66> eventsPunc;
  idList<idVoiceAnimEvent, 66> eventsAnim;
  idList<idVoiceMoodEvent, 66> eventsMood;
};
