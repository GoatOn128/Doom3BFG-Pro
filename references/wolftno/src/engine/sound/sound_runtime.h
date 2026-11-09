#pragma once

// The retail sound folder is split across the emitter, world, voice, and
// XAudio2 translation units.  Keep the recovered cross-unit surface in one
// small header so each translation unit emits the same class names and the
// higher-level GUI code can use the sound world without depending on the
// platform backend.

#include "engine/sound/soundsystem.h"
#include "engine/sound/soundbspfile.h"
#include "engine/sound/decoders/sounddecoder_vorbis.h"

#include <array>
#include <atomic>
#include <cstdint>
#include <memory>
#include <mutex>
#include <vector>

class idAutoComplete;
class idCmdArgs;
class idDecl;
class idFile;
class idRenderWorld;
class idRenderModelGui;

struct idSoundFade {
  int fadeStartTime{0};
  int fadeEndTime{0};
  float fadeStartVolume{1.0F};
  float fadeEndVolume{1.0F};
  bool linear{false};

  void Clear();
  void SetVolume(float to);
  void Fade(float to, int length, int soundTime, int delay = 0,
            bool linear_ = false);
  float GetVolume(int soundTime) const;
};

struct soundDebugInfo_t {
  soundDebugInfo_t() = default;
  soundDebugInfo_t(const soundDebugInfo_t&) = default;
  ~soundDebugInfo_t() = default;
  soundDebugInfo_t& operator=(const soundDebugInfo_t&) = default;

  const class idSoundEmitter* emitter{};
  bool hardwareVoice{};
  bool playing{};
  bool looping{};
  unsigned char padding0{};
  float volume{};
  float voiceVolume{};
  float voiceDryVolume{};
  float voiceWetVolume{};
  float distance{};
  float pitch{};
  unsigned int padding1{};
  const class idSoundShader* shader{};
  int channel{};
  unsigned int padding2{};
  idStr sample;
  bool isdefault{};
  bool iscompressed{};
  bool isstereo{};
  unsigned char padding3{};
  int startTime{};
};
static_assert(sizeof(soundDebugInfo_t)==104,"sound debug info ABI drift");
struct listener_t {
  listener_t();
  ~listener_t();
};

struct soundEnvironment_t {
  struct ambiences_t {
    struct randomSound_t {};
    void Start(bool startSilent);
    ambiences_t& operator=(const ambiences_t&);
  } ambiences;

  // Values consumed by the retail XAudio2 reverb bridge.  Gains are in dB,
  // delays and decay values are milliseconds, and Diffusion/Density are
  // normalized to [0, 1].
  float DryGain{0.0F};
  float RoomGain{0.0F};
  float RoomHFGain{0.0F};
  int DecayTime{1000};
  int DecayHFTime{1000};
  float ReflectionsGain{0.0F};
  int ReflectionsDelay{0};
  float ReverbGain{0.0F};
  int ReverbDelay{0};
  float Diffusion{1.0F};
  float Density{1.0F};
  float HFReference{5000.0F};

  soundEnvironment_t();
  soundEnvironment_t& operator=(const soundEnvironment_t&);
};

class idSoundChannel {
public:
  idSoundChannel();
  ~idSoundChannel();

  bool CanMute() const;
  bool IsLooping() const;
  bool CheckForCompletion(int currentTime);
  void SetSilent();
  void Mute();
  void UpdateVolume(int currentTime = 0);
  millisecondUnique_t GetCurrentSampleTime() const;
  void UpdateHardware(float volumeAdd, int currentTime);

  int startTime{0};
  int endTime{0};
  bool keyOff{false};
  bool silent{false};
  soundChannel_t logicalChannel{SND_CHANNEL_ANY};
  const idSoundShader* soundShader{nullptr};
  const idSoundSample* sample{nullptr};
  idSoundFade volumeFade;
  idSoundFade pitchFade;
  float volumeDB{0.0F};
  float pitch{1.0F};
};

class idSoundWorldLocal;

class idSoundEmitterLocal final : public idSoundEmitter {
public:
  idSoundEmitterLocal();
  ~idSoundEmitterLocal() override;

  void Init(int i, idSoundWorldLocal* sw);
  float Update(listener_t* listener, int currentTime);
  bool CheckForCompletion(int currentTime);
  void OnReloadSound(const idSoundShader* decl);
  void StopSound(idSoundChannel* channel);
  void KillChannelWithFadeout(idSoundChannel* channel);
  void SetVolumeAdjustment(float adjust);
  void ClearVolumeAdjustment();
  float GetVolumeAdjustment() const;
  const idSoundShader* GetCurrentSoundShader(soundChannel_t channel) const;
  soundChannel_t GetCurrentSoundChannel(const idSoundShader* shader) const;
  const idSoundSample* GetCurrentSample(soundChannel_t channel) const;
  float GetSubtitleVolume(soundChannel_t channel) const;
  millisecondUnique_t GetCurrentSampleTime(soundChannel_t channel) const;
  millisecondUnique_t GetRemainingSampleTime(soundChannel_t channel) const;
  bool GetDebugInfo(int index, soundDebugInfo_t* info) const;
  void DrawDebugInfo(idRenderWorld* rw);
  void SetDebugName(const char* debugName);
  const char* GetDebugName() const;
  const idVec3* GetPosition() const;

  int Index() override;
  void Free(bool instant) override;
  void Reset() override;
  void UpdateEmitter(const idVec3& origin, const idMat3& axis,
                     const idVec3& velocity, int listenerId,
                     bool virtualize) override;
  millisecondUnique_t StartSound(soundChannel_t channel,
                                 const idSoundShader* shader,
                                 const soundShaderParms_t* parms) override;
  void ModifySound(soundChannel_t channel, const idSoundShader* shader,
                   const soundShaderParms_t* parms) override;
  void StopSound(void* channel) override;
  void StopSound(soundChannel_t channel) override;
  void SetVolume(soundChannel_t channel, float volume) override;
  void SetPitch(soundChannel_t channel, float pitch) override;
  void FadeSound(soundChannel_t channel, float to, float over,
                 float delay, bool linear) override;
  void FadePitch(soundChannel_t channel, float to, float over,
                 float delay) override;
  bool IsCurrentlyPlaying(soundChannel_t channel) override;

private:
  idSoundWorldLocal* soundWorld{nullptr};
  int index{0};
  bool canFree{false};
  float volumeAdjustment{0.0F};
  idVec3 origin{};
  idMat3 axis{};
  idVec3 velocity{};
  const char* debugName{nullptr};
  std::vector<idSoundChannel*> channels;
};

class idSoundWorldLocal final : public idSoundWorld {
public:
  struct idDuckLayer {
    float GetVolume(int soundTime) const;
  };

  idSoundWorldLocal();
  ~idSoundWorldLocal() override;

  void SetNumListeners(int num);
  void AllocRouting(listener_t* listener);
  void CurrentShakeAmplitude(float* amplitude, float* controller,
                             int listenerId);
  void PlaceListener(const idVec3& origin, const idMat3& axis,
                     const idVec3& velocity, int listenerId);
  void BlendEnv(const soundEnvironment_t* env, float amount, int listenerId);
  void DrawDebug(idRenderWorld* rw);
  void OnReloadSound(const idDecl* shader);
  void Duck(const idDeclDuck* duck, int startTime, int endTime, int listenerId);
  void DuckBlend(const idDeclDuck* duck, float amount);
  int NumEmitters() const;
  idSoundEmitter* EmitterForIndex(int index);
  void Fade(soundShaderGroups_t groups, float to, int over, bool linear);
  void StopChannel(soundChannel_t channel);
  int FindEnvironment(const idVec3* origin) const;
  const soundEnvironment_t* GetEnvironmentParams(int index) const;
  void BlendEnvironmentParamsAmount(float amount, int listenerId);
  int FindDoor(const idVec3* origin) const;
  void SetDoor(int doorNum, float value);
  bool Trace(const idVec3* start, const idVec3* end) const;
  void Pause(soundShaderGroups_t groups);
  void UnPause();
  bool IsPaused() const;
  void StepSoundTime();
  std::int64_t GetSoundTime() const;
  idSoundChannel* AllocSoundChannel();
  void FreeSoundChannel(idSoundChannel* channel);
  int PlayGlobalShaderDirectly(const idSoundShader* shader,
                               soundChannel_t channel, float diversity);
  void RemoveChannelPlayingShader(const idSoundShader* shader,
                                  idSoundChannel* channel);
  int GetBestListenerForEmitter(const idSoundEmitterLocal* emitter) const;
  int PlayPositionedShaderDirectly(const idSoundShader* shader,
                                   const idVec3* origin,
                                   const idMat3* axis, float diversity);
  void PlayRandomAmbiences(const listener_t* listener);
  void AddChannelPlayingShader(const idSoundShader* shader,
                               idSoundChannel* channel);
  int GetChannelsPlayingShader(const idSoundShader* shader) const;
  void SetEnvironmentParams(int index, const soundEnvironment_t* env);
  void BlendEnvironmentParams(const soundEnvironment_t* env, float amount,
                              int listenerId);
  void BlendEnvironmentParamsPush(const soundEnvironment_t* env, float amount,
                                  int listenerId);
  void BlendEnvironmentParamsPop(int listenerId);
  void BlendEnvironmentParamsPopAllExceptTop(int listenerId);
  bool LoadMap(const char* mapName);
  void SaveGame(idFile* file);
  bool LoadGame(idFile* file);
  void Update(int currentTime);
  void UpdateForListener(listener_t* listener, int currentTime);
  void UpdateMainThread(int currentTime);

  void StopAllSounds() override;
  idSoundEmitter* AllocSoundEmitter() override;
  idSoundEmitterLocal* AllocEmitter();
  std::vector<idSoundEmitterLocal*> emitters;
  std::vector<idSoundChannel*> channels;
  int numListeners{1};
  int listenerIndex{0};
  std::int64_t soundTime{0};
  bool paused{false};
};

class idSoundVoice_Base {
public:
  idSoundVoice_Base();
  virtual ~idSoundVoice_Base();

  static void InitSurround(int outputChannels, int channelMask);
  void ReleaseKey();
  void CalculateChannelSurround(int sourceChannel, int sourceChannels,
                                float* levelMatrix, const idVec3& direction,
                                float centerScale, float scale);
  std::int64_t MsecToSamples(int offsetMS, bool looping) const;
  void CalculateSurround(int sourceChannels, float* levelMatrix, float scale,
                         int startFlags);
  const idSoundSample* SelectSample(const idSoundSample* previousSample);

  idVec3 position{};
  idVec3 velocity{};
  idMat3 axis{};
  idMat3 invListenerAxis{};
  const idSoundSample* sample{nullptr};
  const idSoundSample* tailSample{nullptr};
  std::vector<const idSoundSample*> samples;
  float gain{1.0F};
  float dryGain{1.0F};
  float wetGain{1.0F};
  float gainLFE{1.0F};
  float centerChannel{0.0F};
  float pitch{1.0F};
  float innerRadius{32.0F};
  float occlusion{1.0F};
  float lowPassCutoff{20000.0F};
  float innerSampleRange{0.0F};
  float outerSampleRange{0.0F};
  soundShaderBus_t outputBus{SSB_MASTER};
  bool keyOff{false};
  bool keyOffHandled{false};
  bool isPreRandomLoopSample{false};
  static int dstChannels;
  static int dstMask;
  static int dstLFE;
  static int dstCenter;
  static int dstMap[8];
  static int invMap[11];
  static int speakerLeft[11];
  static int speakerRight[11];
  static float omniLevel;
};

class idSoundVoice_XAudio2 : public idSoundVoice_Base {
public:
  struct bufferContext_t {
    idSoundVoice_XAudio2* voice{nullptr};
    const idSoundSample* sample{nullptr};
    std::vector<std::uint8_t> buffer;
    std::vector<std::uint8_t> decodeBuffer;
    int sampleRate{0};
    int streamID{-1};
  };

  struct queuedBuffer_t {
    bufferContext_t* context{nullptr};
    const std::uint8_t* audioData{nullptr};
    int audioBytes{0};
    std::uint32_t playBegin{0};
    std::uint32_t loopBegin{0};
    bool endOfStream{false};
    bool looping{false};
  };

  idSoundVoice_XAudio2();
  ~idSoundVoice_XAudio2() override;

  void FlushSourceBuffers();
  void AllocStreamBuffers();
  void FreeStreamBuffers();
  bool CompatibleFormat(const idSoundSample* sample) const;
  void Create(const idSoundSample* sample,
              const std::vector<const idSoundSample*>& samples,
              const idSoundSample* tailSample);
  void DestroyInternal();
  float GetAmplitude(const idSoundShader* soundShader) const;
  bool IsPlaying() const;
  void Pause();
  void QueueBuffer(const queuedBuffer_t& buffer);
  void QueueClear();
  void QueueFlush();
  void QueueOnBufferEnd(bufferContext_t* bufferContext);
  int RestartAt(int offsetSamples);
  void Start(int offsetMS, int startFlags);
  void Stop();
  void UnPause();
  bool Update();

  bool sourceVoiceCreated{false};
  bool paused{true};
  bool playing{false};
  bool hasReverb{false};
  float amplitude{0.0F};
  std::uint16_t formatTag{0};
  std::uint16_t numChannels{0};
  std::uint16_t bitsPerSample{0};
  std::uint32_t sourceVoiceRate{0};
  std::uint32_t sampleRate{0};
  int currentStreamByte{0};
  std::uint32_t currentStreamSampleOffset{0};
  int currentStreamBuffer{0};
  int streamBlockSize{0};
  int decodeBlockSize{0};
  int startFlags{0};
  int nextStreamID{0};
  int playingStreamID{-1};
  int playingSampleStartMsec{0};
  int pauseTime{0};
  std::uint64_t previousSamplesPlayed{0};
  std::uint64_t samplesPlayed{0};
  const idSoundSample* playingSample{nullptr};
  std::array<bufferContext_t, 3> bufferContexts{};
  std::vector<queuedBuffer_t> queue;
  idSoundDecoder_Vorbis vorbisDecoder;
  mutable std::mutex queueMutex;
};

class StreamingVoiceContext {
public:
  void OnBufferStart(void* context);
  void OnBufferEnd(void* context);
  void OnVoiceError(void* source, long error);
};

struct xapoRegistrationProperties_t {
  const char* friendlyName{nullptr};
  unsigned int flags{0};
  unsigned int minInputBufferCount{1};
  unsigned int maxInputBufferCount{8};
  unsigned int minOutputBufferCount{1};
  unsigned int maxOutputBufferCount{8};
};

enum xapoBufferFlags_t : unsigned int {
  XAPO_BUFFER_SILENT = 0,
  XAPO_BUFFER_VALID = 1
};

struct xapoProcessBuffer_t {
  void* buffer{nullptr};
  unsigned int validFrameCount{0};
  xapoBufferFlags_t bufferFlags{XAPO_BUFFER_SILENT};
};

class CXAPOBase {
public:
  virtual ~CXAPOBase() = default;
  unsigned long AddRef();
  unsigned int CalcInputFrames(unsigned int frames);
  long QueryInterface(const void* iid, void** object);
  unsigned long Release();

private:
  std::atomic<unsigned long> referenceCount{1};
};

class mgXAPOEffect : public CXAPOBase {
public:
  ~mgXAPOEffect() override;
  long LockForProcess(unsigned int channels, unsigned int bitsPerSample);

  unsigned int numChannels{0};
  unsigned int bytesPerSample{0};
  bool processSilence{true};
};

class mgXAPOCompressor : public mgXAPOEffect {
public:
  mgXAPOCompressor(int sampleRate,
                   const dynamicRangeCompressorParms_t* parameters,
                   bool apply);
  static const xapoRegistrationProperties_t& GetStaticRegistrationProperties();
  void Process(const xapoProcessBuffer_t& input, xapoProcessBuffer_t& output,
               bool enabled);

  mgDynamicRangeCompressor compressorDSP;
};

class mgXAPOCompressorApply : public mgXAPOEffect {
public:
  explicit mgXAPOCompressorApply(const mgXAPOCompressor* compressorCalculate);
  static const xapoRegistrationProperties_t& GetStaticRegistrationProperties();
  void Process(const xapoProcessBuffer_t& input, xapoProcessBuffer_t& output,
               bool enabled);

  mgDynamicRangeCompressorApply compressorApplyDSP;
};

class mgXAPOLimiter : public mgXAPOEffect {
public:
  mgXAPOLimiter(int sampleRate, const dynamicRangeLimiterParms_t* parameters);
  static const xapoRegistrationProperties_t& GetStaticRegistrationProperties();
  void Process(const xapoProcessBuffer_t& input, xapoProcessBuffer_t& output,
               bool enabled);

  mgDynamicRangeLimiter limiterDSP;
};

class mgXAPOLimiterApply : public mgXAPOEffect {
public:
  explicit mgXAPOLimiterApply(const mgXAPOLimiter* limiterCalculate);
  static const xapoRegistrationProperties_t& GetStaticRegistrationProperties();
  void Process(const xapoProcessBuffer_t& input, xapoProcessBuffer_t& output,
               bool enabled);

  mgDynamicRangeLimiterApply limiterApplyDSP;
};

class mgXAPOSampleCounter : public mgXAPOEffect {
public:
  static const xapoRegistrationProperties_t& GetStaticRegistrationProperties();
  void Process(const xapoProcessBuffer_t& input, xapoProcessBuffer_t& output);

  std::int64_t sampleClock{0};
};

class idSoundHardware_XAudio2 {
public:
  idSoundHardware_XAudio2();
  ~idSoundHardware_XAudio2();

  idSoundVoice_XAudio2* AllocateVoice(const idSoundSample* leadin,
                                      const idSoundSample* looping);
  void FreeVoice(idSoundVoice_XAudio2* voice);
  void* GetInternalVoice(soundShaderBus_t bus, bool reverb);
  void* GetIXAudio2() const;
  std::int64_t GetSampleClock() const;
  void ClearBusMixing();
  bool CreateCompressorEffect(int bus,
                              const soundCompressorSettings_t& settings);
  template <typename T>
  void CreateEffects(int bus, const T& settings, int currentStage);
  bool CreateLimiterEffect(soundShaderBus_t bus,
                           const soundLimiterSettings_t& settings);
  void SetBusMixing(const idDeclSoundMixing* mixing);
  void SetReverb(const soundEnvironment_t* parameters, bool force);
  void Shutdown();
  void Update();
  void Init();

  struct busVoiceState_t {
    soundShaderBus_t bus{SSB_NONE};
    soundShaderBus_t output{SSB_NONE};
    float gain{0.0F};
    float wetGain{0.0F};
    soundFilter_t filter{};
    int processingStage{0};
    bool active{false};
    bool reverb{false};
  };

  struct busEffectData_t {
    std::unique_ptr<mgXAPOCompressor> compressor;
    std::unique_ptr<mgXAPOCompressorApply> compressorApply;
    std::unique_ptr<mgXAPOLimiter> limiter;
    std::unique_ptr<mgXAPOLimiterApply> limiterApply;
  };

  std::vector<idSoundVoice_XAudio2*> voices;
  std::vector<idSoundVoice_XAudio2*> zombieVoices;
  std::vector<idSoundVoice_XAudio2*> freeVoices;
  std::array<busVoiceState_t, 13> subbusVoices{};
  std::array<busVoiceState_t, 13> subbusVoicesReverb{};
  busVoiceState_t preMasterVoice{};
  busVoiceState_t reverbVoice{};
  std::array<busEffectData_t, 14> busEffectData{};
  std::unique_ptr<mgXAPOSampleCounter> sampleCounter;
  const idDeclSoundMixing* currentMixing{nullptr};
  float dryGain{0.0F};
  float masterGain{1.0F};
  int outputSampleRate{0};
  int outputChannels{0};
  int channelMask{0};
  bool initialized{false};
};

class idSoundEngineCallback {
public:
  void OnCriticalError(long error);
};

class idSoundThread {
public:
  void Render();
  int Run();
};

class idSoundSystemLocal final : public idSoundSystem {
public:
  idSoundSystemLocal();
  ~idSoundSystemLocal() override;

  void Init() override;
  void Shutdown() override;
  idSoundWorld* AllocSoundWorld(const char* name, void* renderWorld) override;
  void FreeSoundWorld(idSoundWorld* world) override;
  void SetPlayingSoundWorld(idSoundWorld* world) override;
  void SetMixingDecl(const idDeclSoundMixing* mixing) override;
  const idDeclSoundMixing* GetMixingDecl() override;
  idSoundWorld* GetPlayingSoundWorld() override;
  void SetEditorSoundWorld(idSoundWorld* world) override;
  idSoundWorld* GetEditorSoundWorld() override;
  idSoundWorld* GetGlobalSoundWorld() override;

  void Restart();
  void Render(bool threaded = false) override;
  void RenderDebugPage(idRenderModelGui* gui);
  void WaitForSoundThread() override;
  void MuteBackgroundMusic(bool mute);
  void SetMute(bool mute);
  bool IsMuted() const;
  void OnReloadSound(const idDecl* sound);
  void StopAllSounds() override;
  void StopVoicesWithSample(const idSoundSample* sample);
  void* GetIXAudio2() override;
  int GetNumSpeakers() const;
  int GetSpeakerMask() const override;
  std::int64_t GetSampleClock() const;
  int GetSampleRate() const;
  idSoundVoice_XAudio2* AllocateVoice(const idSoundSample* leadin,
                                      const idSoundSample* looping);
  void FreeVoice(idSoundVoice_XAudio2* voice);
  idSoundWorldLocal* AllocSoundWorldLocal();

  idSoundWorldLocal* globalSoundWorld{nullptr};
  idSoundWorldLocal* editorSoundWorld{nullptr};
  idSoundWorldLocal* currentSoundWorld{nullptr};
  const idDeclSoundMixing* currentMixing{nullptr};
  bool muted{false};
  bool musicMuted{false};
  bool needsRestart{false};
  idSoundHardware_XAudio2 hardware;
};

extern idSoundSystemLocal soundSystemLocal;

void ArgCompletion_SoundName(idAutoComplete* autoComplete);
void TestSound_f(const idCmdArgs* args);
void RestartSound_f(const idCmdArgs* args);

long XAudio2Create(void** xAudio2, unsigned int flags,
                   unsigned int processor);
void listDevices_f(const idCmdArgs* args);
