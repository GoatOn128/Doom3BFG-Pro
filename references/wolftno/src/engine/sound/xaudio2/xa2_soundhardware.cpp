#include "engine/sound/sound_runtime.h"

#include "shared/idlib/lib.h"

#include <algorithm>
#include <cmath>

namespace {

constexpr int kHardwareVoiceCount = 128;
constexpr long kSuccess = 0;
constexpr long kNoInterface = static_cast<long>(0x80004002UL);
constexpr long kInvalidArgument = static_cast<long>(0x80070057UL);

int BusIndex(soundShaderBus_t bus) {
  return bus == SSB_MASTER ? 13 : static_cast<int>(bus);
}

bool ValidBus(soundShaderBus_t bus) {
  return bus >= SSB_BUS0 && bus <= SSB_MASTER;
}

float DecibelsToLinear(float decibels) {
  return decibels <= -120.0F ? 0.0F : std::exp(decibels * 0.11512925F);
}

void CopyProcessResult(const xapoProcessBuffer_t& input,
                       xapoProcessBuffer_t& output) {
  output.validFrameCount = input.validFrameCount;
  output.bufferFlags = input.bufferFlags;
}

}  // namespace

unsigned long CXAPOBase::AddRef() {
  return referenceCount.fetch_add(1, std::memory_order_relaxed) + 1;
}

unsigned int CXAPOBase::CalcInputFrames(unsigned int frames) { return frames; }

long CXAPOBase::QueryInterface(const void* iid, void** object) {
  if (object == nullptr) return kInvalidArgument;
  *object = nullptr;
  if (iid == nullptr) return kNoInterface;
  *object = this;
  AddRef();
  return kSuccess;
}

unsigned long CXAPOBase::Release() {
  unsigned long count = referenceCount.load(std::memory_order_relaxed);
  while (count != 0 &&
         !referenceCount.compare_exchange_weak(
             count, count - 1, std::memory_order_acq_rel,
             std::memory_order_relaxed)) {
  }
  return count == 0 ? 0 : count - 1;
}

mgXAPOEffect::~mgXAPOEffect() = default;

long mgXAPOEffect::LockForProcess(unsigned int channels,
                                  unsigned int bitsPerSample) {
  if (channels == 0 || bitsPerSample == 0 || (bitsPerSample & 7U) != 0)
    return kInvalidArgument;
  numChannels = channels;
  bytesPerSample = bitsPerSample >> 3U;
  return kSuccess;
}

const xapoRegistrationProperties_t&
mgXAPOSampleCounter::GetStaticRegistrationProperties() {
  static const xapoRegistrationProperties_t properties{
      "mgXAPOSampleCounter", 0x3FU, 1, 8, 1, 8};
  return properties;
}

void mgXAPOSampleCounter::Process(const xapoProcessBuffer_t& input,
                                  xapoProcessBuffer_t& output) {
  CopyProcessResult(input, output);
  sampleClock += input.validFrameCount;
}

const xapoRegistrationProperties_t&
mgXAPOLimiter::GetStaticRegistrationProperties() {
  static const xapoRegistrationProperties_t properties{
      "mgXAPOLimiter", 0x1FU, 1, 8, 1, 8};
  return properties;
}

const xapoRegistrationProperties_t&
mgXAPOLimiterApply::GetStaticRegistrationProperties() {
  static const xapoRegistrationProperties_t properties{
      "mgXAPOLimiterApply", 0x1FU, 1, 8, 1, 8};
  return properties;
}

const xapoRegistrationProperties_t&
mgXAPOCompressor::GetStaticRegistrationProperties() {
  static const xapoRegistrationProperties_t properties{
      "mgXAPOCompressor", 0x1FU, 1, 8, 1, 8};
  return properties;
}

const xapoRegistrationProperties_t&
mgXAPOCompressorApply::GetStaticRegistrationProperties() {
  static const xapoRegistrationProperties_t properties{
      "mgXAPOCompressorApply", 0x1FU, 1, 8, 1, 8};
  return properties;
}

mgXAPOLimiter::mgXAPOLimiter(
    int sampleRate, const dynamicRangeLimiterParms_t* parameters) {
  limiterDSP.Initialize(static_cast<float>(sampleRate), parameters);
  processSilence = true;
}

mgXAPOLimiterApply::mgXAPOLimiterApply(
    const mgXAPOLimiter* limiterCalculate) {
  limiterApplyDSP.limiter =
      limiterCalculate == nullptr ? nullptr : &limiterCalculate->limiterDSP;
}

void mgXAPOLimiter::Process(const xapoProcessBuffer_t& input,
                            xapoProcessBuffer_t& output, bool enabled) {
  if (enabled && input.bufferFlags == XAPO_BUFFER_VALID &&
      input.buffer != nullptr && output.buffer != nullptr && numChannels > 0) {
    limiterDSP.ProcessInterleaved(
        static_cast<float*>(input.buffer), static_cast<float*>(output.buffer),
        static_cast<int>(input.validFrameCount), static_cast<int>(numChannels));
    processSilence = true;
  } else if (enabled && processSilence) {
    processSilence = limiterDSP.ProcessSilence(input.validFrameCount);
  }
  CopyProcessResult(input, output);
}

void mgXAPOLimiterApply::Process(const xapoProcessBuffer_t& input,
                                 xapoProcessBuffer_t& output, bool enabled) {
  if (enabled && input.bufferFlags == XAPO_BUFFER_VALID &&
      input.buffer != nullptr && output.buffer != nullptr && numChannels > 0) {
    limiterApplyDSP.ProcessInterleaved(
        static_cast<const float*>(input.buffer),
        static_cast<float*>(output.buffer),
        static_cast<int>(input.validFrameCount), static_cast<int>(numChannels));
  }
  CopyProcessResult(input, output);
}

mgXAPOCompressor::mgXAPOCompressor(
    int sampleRate, const dynamicRangeCompressorParms_t* parameters,
    bool apply) {
  compressorDSP.Initialize(static_cast<float>(sampleRate), parameters, apply);
  processSilence = true;
}

mgXAPOCompressorApply::mgXAPOCompressorApply(
    const mgXAPOCompressor* compressorCalculate) {
  compressorApplyDSP.lastLevel = 0.0F;
  compressorApplyDSP.compressorCalculate = compressorCalculate == nullptr
                                               ? nullptr
                                               : &compressorCalculate->compressorDSP;
}

void mgXAPOCompressor::Process(const xapoProcessBuffer_t& input,
                               xapoProcessBuffer_t& output, bool enabled) {
  if (enabled && input.bufferFlags == XAPO_BUFFER_VALID &&
      input.buffer != nullptr && numChannels > 0) {
    compressorDSP.ProcessInterleaved(
        reinterpret_cast<__m128*>(input.buffer),
        static_cast<float*>(output.buffer),
        static_cast<int>(input.validFrameCount), static_cast<int>(numChannels));
    processSilence = true;
  } else if (enabled && processSilence) {
    processSilence = compressorDSP.ProcessSilence(input.validFrameCount);
  }
  CopyProcessResult(input, output);
}

void mgXAPOCompressorApply::Process(const xapoProcessBuffer_t& input,
                                    xapoProcessBuffer_t& output,
                                    bool enabled) {
  if (enabled && input.bufferFlags == XAPO_BUFFER_VALID &&
      input.buffer != nullptr && output.buffer != nullptr && numChannels > 0) {
    compressorApplyDSP.ProcessInterleaved(
        static_cast<const float*>(input.buffer),
        static_cast<float*>(output.buffer),
        static_cast<int>(input.validFrameCount), static_cast<int>(numChannels));
  }
  CopyProcessResult(input, output);
}

void idSoundEngineCallback::OnCriticalError(long error) {
  (void)error;
  soundSystemLocal.needsRestart = true;
}

idSoundHardware_XAudio2::idSoundHardware_XAudio2() {
  preMasterVoice.bus = SSB_MASTER;
  preMasterVoice.output = SSB_MASTER;
  reverbVoice.bus = SSB_MASTER;
  reverbVoice.output = SSB_MASTER;
  reverbVoice.reverb = true;
}

idSoundVoice_XAudio2* idSoundHardware_XAudio2::AllocateVoice(
    const idSoundSample* leadin, const idSoundSample* looping) {
  if (leadin == nullptr) return nullptr;

  if (looping != nullptr &&
      (leadin->format.basic.formatTag != looping->format.basic.formatTag ||
       leadin->format.basic.numChannels != looping->format.basic.numChannels ||
       leadin->format.basic.bitsPerSample != looping->format.basic.bitsPerSample)) {
    idLib::Warning("Leadin/looping format mismatch: %s & %s",
                   leadin->GetName(), looping->GetName());
    looping = nullptr;
  }

  idSoundVoice_XAudio2* selected = nullptr;
  for (idSoundVoice_XAudio2* candidate : freeVoices) {
    if (candidate->IsPlaying()) continue;
    selected = candidate;
    if (candidate->CompatibleFormat(leadin)) break;
  }
  if (selected == nullptr) {
    idLib::Warning("No hardware voices available");
    return nullptr;
  }

  selected->Create(leadin, {}, looping);
  const auto found = std::find(freeVoices.begin(), freeVoices.end(), selected);
  if (found != freeVoices.end()) {
    *found = freeVoices.back();
    freeVoices.pop_back();
  }
  return selected;
}

void idSoundHardware_XAudio2::FreeVoice(idSoundVoice_XAudio2* voice) {
  if (voice == nullptr) return;
  voice->Stop();
  if (std::find(zombieVoices.begin(), zombieVoices.end(), voice) ==
      zombieVoices.end()) {
    zombieVoices.push_back(voice);
  }
}

void* idSoundHardware_XAudio2::GetInternalVoice(soundShaderBus_t bus,
                                                bool reverb) {
  if (!ValidBus(bus)) return nullptr;
  if (bus == SSB_MASTER)
    return reverb ? static_cast<void*>(&reverbVoice)
                  : static_cast<void*>(&preMasterVoice);
  const int index = static_cast<int>(bus);
  return reverb ? static_cast<void*>(&subbusVoicesReverb[index])
                : static_cast<void*>(&subbusVoices[index]);
}

void* idSoundHardware_XAudio2::GetIXAudio2() const {
  return initialized ? const_cast<idSoundHardware_XAudio2*>(this) : nullptr;
}

std::int64_t idSoundHardware_XAudio2::GetSampleClock() const {
  return sampleCounter == nullptr ? -1 : sampleCounter->sampleClock;
}

void idSoundHardware_XAudio2::ClearBusMixing() {
  if (soundSystem != nullptr) soundSystemLocal.StopAllSounds();
  for (idSoundVoice_XAudio2* voice : voices) voice->DestroyInternal();
  for (busEffectData_t& effects : busEffectData) effects = {};
  for (int index = 0; index < 13; ++index) {
    subbusVoices[index] = {};
    subbusVoices[index].bus = static_cast<soundShaderBus_t>(index);
    subbusVoicesReverb[index] = {};
    subbusVoicesReverb[index].bus = static_cast<soundShaderBus_t>(index);
    subbusVoicesReverb[index].reverb = true;
  }
  currentMixing = nullptr;
}

bool idSoundHardware_XAudio2::CreateCompressorEffect(
    int bus, const soundCompressorSettings_t& settings) {
  if (!settings.enabled || bus < 0 || bus >= 14) return false;

  busEffectData_t& effects = busEffectData[bus];
  if (settings.sideChain && settings.sideChainSourceBus != SSB_NONE) {
    const int source = BusIndex(settings.sideChainSourceBus);
    if (source < 0 || source >= 14 ||
        busEffectData[source].compressor == nullptr) {
      return false;
    }
    effects.compressorApply = std::make_unique<mgXAPOCompressorApply>(
        busEffectData[source].compressor.get());
    effects.compressorApply->LockForProcess(
        static_cast<unsigned int>(outputChannels), 32);
    return true;
  }

  const bool applyHere = !settings.sideChain;
  effects.compressor = std::make_unique<mgXAPOCompressor>(
      outputSampleRate, &settings.parms, applyHere);
  effects.compressor->LockForProcess(static_cast<unsigned int>(outputChannels),
                                     32);
  if (applyHere) {
    effects.compressorApply = std::make_unique<mgXAPOCompressorApply>(
        effects.compressor.get());
    effects.compressorApply->LockForProcess(1, 32);
  }
  return true;
}

bool idSoundHardware_XAudio2::CreateLimiterEffect(
    soundShaderBus_t bus, const soundLimiterSettings_t& settings) {
  const int index = BusIndex(bus);
  if (!settings.enabled || index < 0 || index >= 14) return false;
  busEffectData_t& effects = busEffectData[index];
  effects.limiter =
      std::make_unique<mgXAPOLimiter>(outputSampleRate, &settings.parms);
  effects.limiter->LockForProcess(static_cast<unsigned int>(outputChannels),
                                  32);
  effects.limiterApply =
      std::make_unique<mgXAPOLimiterApply>(effects.limiter.get());
  effects.limiterApply->LockForProcess(1, 32);
  return true;
}

template <typename T>
void idSoundHardware_XAudio2::CreateEffects(int bus, const T& settings,
                                            int currentStage) {
  const bool sideChainDestination =
      settings.compressor.sideChain &&
      settings.compressor.sideChainSourceBus != SSB_NONE;
  if (currentStage != (sideChainDestination ? 1 : 0)) return;
  CreateCompressorEffect(bus, settings.compressor);
  CreateLimiterEffect(static_cast<soundShaderBus_t>(bus), settings.limiter);
}

template void idSoundHardware_XAudio2::CreateEffects<soundMasterBusSettings_t>(
    int, const soundMasterBusSettings_t&, int);
template void idSoundHardware_XAudio2::CreateEffects<soundSubBusSettings_t>(
    int, const soundSubBusSettings_t&, int);

void idSoundHardware_XAudio2::SetBusMixing(
    const idDeclSoundMixing* mixing) {
  ClearBusMixing();
  if (!initialized || mixing == nullptr) return;
  currentMixing = mixing;

  for (int index = 0; index < 13; ++index) {
    const soundSubBusSettings_t& settings = mixing->subBus[index];
    busVoiceState_t& dry = subbusVoices[index];
    busVoiceState_t& wet = subbusVoicesReverb[index];
    dry.bus = static_cast<soundShaderBus_t>(index);
    dry.output = settings.output;
    dry.gain = DecibelsToLinear(settings.gain);
    dry.wetGain = DecibelsToLinear(settings.gain + settings.wetGain);
    dry.filter = settings.filter;
    dry.active = settings.output != SSB_NONE;
    wet = dry;
    wet.reverb = true;

    int stage = 0;
    soundShaderBus_t output = settings.output;
    bool visited[13]{};
    while (output >= SSB_BUS0 && output <= SSB_BUS12) {
      const int outputIndex = static_cast<int>(output);
      if (visited[outputIndex]) {
        dry.active = false;
        wet.active = false;
        break;
      }
      visited[outputIndex] = true;
      ++stage;
      output = mixing->subBus[outputIndex].output;
    }
    dry.processingStage = stage;
    wet.processingStage = stage;
  }

  for (int stage = 0; stage < 2; ++stage) {
    for (int index = 0; index < 13; ++index)
      CreateEffects(index, mixing->subBus[index], stage);
    CreateEffects(13, mixing->masterBus, stage);
  }
}

void idSoundHardware_XAudio2::SetReverb(
    const soundEnvironment_t* parameters, bool force) {
  (void)force;
  if (!initialized || parameters == nullptr) return;
  dryGain = parameters->DryGain <= -60.0F
                ? 0.0F
                : DecibelsToLinear(parameters->DryGain);
  reverbVoice.gain = DecibelsToLinear(
      std::clamp(parameters->RoomGain, -100.0F, 0.0F));
  reverbVoice.wetGain = DecibelsToLinear(
      std::clamp(parameters->ReverbGain, -100.0F, 0.0F));
  reverbVoice.filter.type = SND_FILTERTYPE_LOWPASS;
  reverbVoice.filter.frequency = static_cast<int>(std::clamp(
      parameters->HFReference, 20.0F,
      static_cast<float>(std::max(20, outputSampleRate / 2))));
  reverbVoice.active = true;
}

void idSoundHardware_XAudio2::Shutdown() {
  if (!initialized && voices.empty()) return;
  ClearBusMixing();
  for (idSoundVoice_XAudio2* voice : voices) {
    voice->FreeStreamBuffers();
    delete voice;
  }
  voices.clear();
  zombieVoices.clear();
  freeVoices.clear();
  sampleCounter.reset();
  initialized = false;
  outputSampleRate = 0;
  outputChannels = 0;
  channelMask = 0;
  dryGain = 0.0F;
}

void idSoundHardware_XAudio2::Update() {
  if (!initialized) return;

  xapoProcessBuffer_t clockInput{};
  xapoProcessBuffer_t clockOutput{};
  clockInput.validFrameCount =
      static_cast<unsigned int>(std::max(1, outputSampleRate / 60));
  if (sampleCounter != nullptr)
    sampleCounter->Process(clockInput, clockOutput);

  for (idSoundVoice_XAudio2* voice : voices) {
    if (std::find(freeVoices.begin(), freeVoices.end(), voice) ==
        freeVoices.end()) {
      voice->Update();
    }
  }

  for (std::size_t index = 0; index < zombieVoices.size();) {
    idSoundVoice_XAudio2* voice = zombieVoices[index];
    voice->FlushSourceBuffers();
    if (voice->IsPlaying()) {
      ++index;
      continue;
    }
    voice->QueueClear();
    if (std::find(freeVoices.begin(), freeVoices.end(), voice) ==
        freeVoices.end()) {
      freeVoices.push_back(voice);
    }
    zombieVoices[index] = zombieVoices.back();
    zombieVoices.pop_back();
  }
}

void idSoundHardware_XAudio2::Init() {
  if (initialized) return;
  initialized = true;
  outputSampleRate = 48000;
  outputChannels = 2;
  channelMask = 0x3;
  dryGain = 1.0F;
  masterGain = 1.0F;
  idSoundVoice_Base::InitSurround(outputChannels, channelMask);

  sampleCounter = std::make_unique<mgXAPOSampleCounter>();
  sampleCounter->LockForProcess(static_cast<unsigned int>(outputChannels), 32);

  voices.reserve(kHardwareVoiceCount);
  freeVoices.reserve(kHardwareVoiceCount);
  zombieVoices.reserve(kHardwareVoiceCount);
  for (int index = 0; index < kHardwareVoiceCount; ++index) {
    auto* voice = new idSoundVoice_XAudio2;
    voice->AllocStreamBuffers();
    voices.push_back(voice);
    freeVoices.push_back(voice);
  }
  Update();
}

long XAudio2Create(void** xAudio2, unsigned int flags,
                   unsigned int processor) {
  (void)flags;
  (void)processor;
  if (xAudio2 == nullptr) return kInvalidArgument;
  static int portableXAudio2Token = 0;
  *xAudio2 = &portableXAudio2Token;
  return kSuccess;
}

void listDevices_f(const idCmdArgs* args) {
  (void)args;
  if (soundSystemLocal.GetIXAudio2() == nullptr) {
    idLib::Warning("No xaudio object");
    return;
  }
  idLib::Printf("  0: Default Windows audio endpoint\n");
  idLib::Printf("     %d channels, %d Hz\n",
                soundSystemLocal.GetNumSpeakers(),
                soundSystemLocal.GetSampleRate());
}
