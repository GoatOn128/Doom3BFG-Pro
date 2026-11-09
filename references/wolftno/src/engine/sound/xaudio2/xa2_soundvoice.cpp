#include "engine/sound/sound_runtime.h"

#include "engine/decls/decltable.h"
#include "shared/idlib/lib.h"
#include "shared/idlib/sys/sys_time.h"

#include <algorithm>
#include <cmath>

namespace {

constexpr int kQueuedBufferCount = 3;
constexpr int kDefaultStreamBlockSize = 128 * 1024;
constexpr int kDefaultDecodeBlockSize = 64 * 1024;
constexpr std::uint16_t kWaveFormatVorbis = 0x674F;

bool ContainsSample(const idList<const idSoundSample*, 38>& entries,
                    const idSoundSample* sample) {
  for (int index = 0; index < entries.Num(); ++index) {
    if (entries[index] == sample) return true;
  }
  return false;
}

int CurrentMilliseconds() {
  return static_cast<int>(Sys_Milliseconds());
}

}  // namespace

idSoundVoice_Base::~idSoundVoice_Base() = default;

idSoundVoice_XAudio2::idSoundVoice_XAudio2() {
  for (bufferContext_t& context : bufferContexts) {
    context.voice = this;
    context.streamID = -1;
  }
}

idSoundVoice_XAudio2::~idSoundVoice_XAudio2() {
  DestroyInternal();
  FreeStreamBuffers();
}

void idSoundVoice_XAudio2::FlushSourceBuffers() {
  if (!sourceVoiceCreated || playing) return;
  std::scoped_lock lock(queueMutex);
  queue.clear();
}

void idSoundVoice_XAudio2::AllocStreamBuffers() {
  FreeStreamBuffers();
  streamBlockSize = kDefaultStreamBlockSize;
  decodeBlockSize = kDefaultDecodeBlockSize;
  for (bufferContext_t& context : bufferContexts) {
    context.voice = this;
    context.sample = nullptr;
    context.buffer.resize(static_cast<std::size_t>(streamBlockSize));
    context.decodeBuffer.resize(static_cast<std::size_t>(decodeBlockSize));
    context.sampleRate = 0;
    context.streamID = -1;
  }
}

void idSoundVoice_XAudio2::FreeStreamBuffers() {
  for (bufferContext_t& context : bufferContexts) {
    context.buffer.clear();
    context.buffer.shrink_to_fit();
    context.decodeBuffer.clear();
    context.decodeBuffer.shrink_to_fit();
    context.sample = nullptr;
    context.sampleRate = 0;
    context.streamID = -1;
  }
  streamBlockSize = 0;
  decodeBlockSize = 0;
}

bool idSoundVoice_XAudio2::CompatibleFormat(
    const idSoundSample* value) const {
  if (!sourceVoiceCreated) return true;
  return value != nullptr && value->format.basic.formatTag == formatTag &&
         value->format.basic.numChannels == numChannels &&
         value->format.basic.bitsPerSample == bitsPerSample;
}

void idSoundVoice_XAudio2::Create(
    const idSoundSample* value,
    const std::vector<const idSoundSample*>& sampleSet,
    const idSoundSample* tail) {
  if (IsPlaying()) {
    Stop();
    return;
  }

  sample = value;
  samples = sampleSet;
  tailSample = tail;
  if (tailSample != nullptr) tailSample->PreCacheStreamData(false);
  if (sample == nullptr) {
    DestroyInternal();
    return;
  }

  if (!CompatibleFormat(sample)) DestroyInternal();
  if (!sourceVoiceCreated) {
    formatTag = sample->format.basic.formatTag;
    numChannels = sample->format.basic.numChannels;
    bitsPerSample = sample->format.basic.bitsPerSample;
    sourceVoiceCreated = numChannels != 0 && sample->format.basic.samplesPerSec != 0;
    if (!sourceVoiceCreated) return;
  }

  sampleRate = sample->format.basic.samplesPerSec;
  sourceVoiceRate = sampleRate;
  samplesPlayed = 0;
  previousSamplesPlayed = 0;
  currentStreamByte = 0;
  currentStreamSampleOffset = 0;
  currentStreamBuffer = 0;
  playing = false;
  paused = true;
  QueueClear();
}

void idSoundVoice_XAudio2::DestroyInternal() {
  sourceVoiceCreated = false;
  playing = false;
  paused = true;
  hasReverb = false;
  vorbisDecoder.Close();
  QueueClear();
  sample = nullptr;
  tailSample = nullptr;
  samples.clear();
  formatTag = 0;
  numChannels = 0;
  bitsPerSample = 0;
  sampleRate = 0;
  sourceVoiceRate = 0;
}

float idSoundVoice_XAudio2::GetAmplitude(
    const idSoundShader* soundShader) const {
  if (playingSample == nullptr || soundShader == nullptr) return 0.0F;

  const idDeclTable* table = soundShader->rmsTable;
  if (ContainsSample(soundShader->preEntries, playingSample)) {
    table = soundShader->rmsPreTable;
  } else if (ContainsSample(soundShader->tailEntries, playingSample)) {
    table = soundShader->rmsTailTable;
  }
  if (table == nullptr || table->table == nullptr) return 0.0F;

  const int rate = static_cast<int>(playingSample->format.basic.samplesPerSec);
  const int sampleCount =
      playingSample->lastValidSample - playingSample->firstValidSample;
  if (rate <= 0 || sampleCount <= 0) return 0.0F;
  const float durationMS = 1000.0F * static_cast<float>(sampleCount) /
                           static_cast<float>(rate);
  float normalized = static_cast<float>(CurrentMilliseconds() -
                                        playingSampleStartMsec) *
                     pitch / durationMS;
  if (normalized > 1.0F) {
    if ((soundShader->parms.soundShaderFlags & SSF_LOOPING) != 0 &&
        sample != nullptr) {
      const int loopLength = sample->lastValidSample - sample->firstValidSample;
      if (loopLength > 0) {
        const float loopStart = static_cast<float>(sample->loopPoint) /
                                static_cast<float>(loopLength);
        const float loopSpan = std::max(0.0001F, 1.0F - loopStart);
        normalized = loopStart + std::fmod(normalized - loopStart, loopSpan);
      }
    } else {
      normalized = 1.0F;
    }
  }
  return table->TableLookup(std::clamp(normalized, 0.0F, 1.0F), true);
}

bool idSoundVoice_XAudio2::IsPlaying() const {
  if (!sourceVoiceCreated) return false;
  if (sample != nullptr && sample->streamFile != nullptr &&
      currentStreamByte < sample->dataLength)
    return true;
  std::scoped_lock lock(queueMutex);
  return playing || !queue.empty();
}

void idSoundVoice_XAudio2::Pause() {
  if (!sourceVoiceCreated || paused) return;
  paused = true;
  pauseTime = CurrentMilliseconds();
}

void idSoundVoice_XAudio2::QueueBuffer(const queuedBuffer_t& buffer) {
  std::scoped_lock lock(queueMutex);
  queue.push_back(buffer);
  if (playingSample == nullptr && buffer.context != nullptr) {
    playingSample = buffer.context->sample;
    playingStreamID = buffer.context->streamID;
    playingSampleStartMsec = CurrentMilliseconds();
  }
  playing = true;
}

void idSoundVoice_XAudio2::QueueClear() {
  std::scoped_lock lock(queueMutex);
  queue.clear();
  playingSample = nullptr;
  playingStreamID = -1;
  playing = false;
}

void idSoundVoice_XAudio2::QueueFlush() {
  std::scoped_lock lock(queueMutex);
  while (!queue.empty()) {
    const bufferContext_t* context = queue.back().context;
    if (context != nullptr && context->streamID == playingStreamID) break;
    queue.pop_back();
    currentStreamBuffer = (currentStreamBuffer + kQueuedBufferCount - 1) %
                          kQueuedBufferCount;
  }
}

void idSoundVoice_XAudio2::QueueOnBufferEnd(
    bufferContext_t* bufferContext) {
  std::scoped_lock lock(queueMutex);
  const auto found = std::find_if(
      queue.begin(), queue.end(), [bufferContext](const queuedBuffer_t& value) {
        return value.context == bufferContext;
      });
  if (found != queue.end()) queue.erase(found);
  if (!queue.empty() && queue.front().context != nullptr) {
    playingSample = queue.front().context->sample;
    playingStreamID = queue.front().context->streamID;
    playingSampleStartMsec = CurrentMilliseconds();
  } else if (sample == nullptr || sample->streamFile == nullptr ||
             currentStreamByte >= sample->dataLength) {
    playing = false;
    playingSample = nullptr;
    playingStreamID = -1;
  }
}

int idSoundVoice_XAudio2::RestartAt(int offsetSamples) {
  if (!sourceVoiceCreated || sample == nullptr || offsetSamples < 0) return 0;

  if (sample->format.basic.formatTag == kWaveFormatVorbis) {
    if (!vorbisDecoder.Open(sample)) return 0;
    vorbisDecoder.Seek(offsetSamples);
    currentStreamByte = 0;
    currentStreamSampleOffset = 0;
    return 0;
  }

  int byteOffset = 0;
  std::uint32_t samplesToSkip = 0;
  if (!sample->GetSeekPosition(offsetSamples, &byteOffset, &samplesToSkip))
    return 0;

  if (sample->streamFile != nullptr) {
    currentStreamByte = byteOffset;
    currentStreamSampleOffset = samplesToSkip;
    return 0;
  }

  if (sample->staticBuffer == nullptr || byteOffset >= sample->dataLength)
    return 0;
  bufferContext_t& context = bufferContexts[currentStreamBuffer];
  currentStreamBuffer = (currentStreamBuffer + 1) % kQueuedBufferCount;
  context.voice = this;
  context.sample = sample;
  context.sampleRate = static_cast<int>(sample->format.basic.samplesPerSec);
  context.streamID = nextStreamID;

  queuedBuffer_t buffer;
  buffer.context = &context;
  buffer.audioData = static_cast<const std::uint8_t*>(sample->staticBuffer) +
                     byteOffset;
  buffer.audioBytes = sample->dataLength - byteOffset;
  buffer.playBegin = samplesToSkip;
  buffer.endOfStream = true;
  QueueBuffer(buffer);
  int queuedBytes = buffer.audioBytes;

  if ((startFlags & SSF_LOOPING) != 0) {
    int loopOffset = 0;
    std::uint32_t loopSkip = 0;
    if (sample->GetSeekPosition(sample->loopPoint, &loopOffset, &loopSkip) &&
        loopOffset < sample->dataLength) {
      bufferContext_t& loopContext = bufferContexts[currentStreamBuffer];
      currentStreamBuffer = (currentStreamBuffer + 1) % kQueuedBufferCount;
      loopContext.voice = this;
      loopContext.sample = sample;
      loopContext.sampleRate = context.sampleRate;
      loopContext.streamID = nextStreamID;
      buffer.context = &loopContext;
      buffer.audioData = static_cast<const std::uint8_t*>(sample->staticBuffer) +
                         loopOffset;
      buffer.audioBytes = sample->dataLength - loopOffset;
      buffer.playBegin = loopSkip;
      buffer.loopBegin = loopSkip;
      buffer.looping = true;
      QueueBuffer(buffer);
      queuedBytes += buffer.audioBytes;
    }
  }
  ++nextStreamID;
  return queuedBytes;
}

void idSoundVoice_XAudio2::Start(int offsetMS, int flags) {
  if (sample == nullptr || !sourceVoiceCreated) return;
  if (sample->timestamp == static_cast<std::uint32_t>(-1)) {
    idLib::Warning("Starting defaulted sound sample %s", sample->GetName());
  }

  startFlags = flags;
  keyOff = false;
  keyOffHandled = false;
  isPreRandomLoopSample =
      (flags & SSF_RANDOM_LOOPING) != 0 &&
      std::find(samples.begin(), samples.end(), sample) == samples.end();
  hasReverb = (flags & SSF_NO_EFFECTS) == 0;
  const bool looping =
      (flags & (SSF_LOOPING | SSF_RANDOM_LOOPING)) != 0;
  const std::int64_t offsetSamples = MsecToSamples(offsetMS, looping);
  if (offsetSamples < 0) return;

  QueueClear();
  RestartAt(static_cast<int>(offsetSamples));
  playingSampleStartMsec = CurrentMilliseconds();
  previousSamplesPlayed = 0;
  samplesPlayed = 0;
  paused = true;
  Update();
  UnPause();
}

void idSoundVoice_XAudio2::Stop() {
  if (!sourceVoiceCreated) return;
  paused = true;
  currentStreamByte = sample == nullptr ? 0 : sample->dataLength;
  currentStreamSampleOffset = 0;
  pauseTime = 0;
  vorbisDecoder.Close();
  QueueClear();
}

void idSoundVoice_XAudio2::UnPause() {
  if (!sourceVoiceCreated || !paused) return;
  paused = false;
  if (pauseTime > 0) {
    playingSampleStartMsec += CurrentMilliseconds() - pauseTime;
    pauseTime = 0;
  }
  if (sample != nullptr) playing = true;
}

bool idSoundVoice_XAudio2::Update() {
  if (!sourceVoiceCreated || sample == nullptr) return false;

  const int now = CurrentMilliseconds();
  if (playing && !paused && sampleRate != 0) {
    const int elapsedMS = std::max(0, now - playingSampleStartMsec);
    const std::uint64_t currentSamples =
        static_cast<std::uint64_t>(elapsedMS) * sampleRate / 1000U;
    if (currentSamples >= previousSamplesPlayed)
      samplesPlayed += currentSamples - previousSamplesPlayed;
    previousSamplesPlayed = currentSamples;
  }

  if (keyOff && !keyOffHandled && tailSample != nullptr &&
      playingStreamID != nextStreamID) {
    QueueFlush();
    sample = tailSample;
    RestartAt(0);
    keyOffHandled = true;
  }

  std::size_t queuedCount = 0;
  {
    std::scoped_lock lock(queueMutex);
    queuedCount = queue.size();
  }

  if (sample->format.basic.formatTag == kWaveFormatVorbis) {
    if (vorbisDecoder.sample == nullptr && !vorbisDecoder.Open(sample))
      return false;
    while (queuedCount < kQueuedBufferCount) {
      bufferContext_t& context = bufferContexts[currentStreamBuffer];
      if (context.decodeBuffer.empty()) break;
      const std::int64_t bytes = vorbisDecoder.Read(
          reinterpret_cast<char*>(context.decodeBuffer.data()), decodeBlockSize);
      const bool endOfStream = vorbisDecoder.IsEOS();
      if (bytes > 0) {
        currentStreamBuffer =
            (currentStreamBuffer + 1) % kQueuedBufferCount;
        context.voice = this;
        context.sample = sample;
        context.streamID = nextStreamID;
        context.sampleRate = static_cast<int>(sample->format.basic.samplesPerSec);
        queuedBuffer_t buffer;
        buffer.context = &context;
        buffer.audioData = context.decodeBuffer.data();
        buffer.audioBytes = static_cast<int>(bytes);
        buffer.endOfStream = endOfStream;
        QueueBuffer(buffer);
        ++queuedCount;
      }
      if (!endOfStream) {
        if (bytes <= 0) break;
        continue;
      }
      ++nextStreamID;
      if (!keyOff && (startFlags & SSF_RANDOM_LOOPING) != 0) {
        sample = SelectSample(sample);
        vorbisDecoder.Close();
        vorbisDecoder.Open(sample);
        vorbisDecoder.Seek(sample->loopPoint);
      } else if (!keyOff && (startFlags & SSF_LOOPING) != 0) {
        vorbisDecoder.Seek(sample->loopPoint);
      }
      isPreRandomLoopSample = false;
      break;
    }
  } else if (sample->streamFile != nullptr) {
    while (queuedCount < kQueuedBufferCount &&
           currentStreamByte < sample->dataLength) {
      const int remaining = sample->dataLength - currentStreamByte;
      int bytesToRead = std::min(streamBlockSize, remaining);
      const int blockSize = std::max(1, static_cast<int>(sample->format.basic.blockSize));
      bytesToRead -= bytesToRead % blockSize;
      if (bytesToRead <= 0) break;

      bufferContext_t& context = bufferContexts[currentStreamBuffer];
      if (static_cast<int>(context.buffer.size()) < bytesToRead)
        context.buffer.resize(static_cast<std::size_t>(bytesToRead));
      const int bytesRead = sample->ReadCachedSoundData(
          currentStreamByte, context.buffer.data(), bytesToRead,
          static_cast<int>(queuedCount),
          (startFlags & SSF_BLOCK_ON_LOAD) != 0);
      if (bytesRead != bytesToRead) {
        if (bytesRead > 0)
          idLib::Warning("Sound read failure: %i != %i!", bytesRead,
                         bytesToRead);
        break;
      }

      currentStreamBuffer =
          (currentStreamBuffer + 1) % kQueuedBufferCount;
      context.voice = this;
      context.sample = sample;
      context.streamID = nextStreamID;
      context.sampleRate = static_cast<int>(sample->format.basic.samplesPerSec);
      queuedBuffer_t buffer;
      buffer.context = &context;
      buffer.audioData = context.buffer.data();
      buffer.audioBytes = bytesRead;
      buffer.playBegin = currentStreamSampleOffset;
      currentStreamSampleOffset = 0;
      currentStreamByte += bytesRead;
      buffer.endOfStream = currentStreamByte >= sample->dataLength;
      QueueBuffer(buffer);
      ++queuedCount;
      if (buffer.endOfStream) {
        ++nextStreamID;
        if (!keyOff && (startFlags & SSF_RANDOM_LOOPING) != 0) {
          sample = SelectSample(sample);
          RestartAt(sample->loopPoint);
        } else if (!keyOff && (startFlags & SSF_LOOPING) != 0) {
          RestartAt(sample->loopPoint);
        }
        isPreRandomLoopSample = false;
      }
    }
  }

  const int validSamples = sample->lastValidSample - sample->firstValidSample;
  if (sample->streamFile == nullptr && validSamples > 0 && !paused &&
      samplesPlayed >= static_cast<std::uint64_t>(validSamples)) {
    if (!keyOff && (startFlags & (SSF_LOOPING | SSF_RANDOM_LOOPING)) != 0) {
      if ((startFlags & SSF_RANDOM_LOOPING) != 0) sample = SelectSample(sample);
      QueueClear();
      RestartAt(sample->loopPoint);
      samplesPlayed = 0;
      previousSamplesPlayed = 0;
      playingSampleStartMsec = now;
    } else {
      QueueClear();
    }
  }

  amplitude = IsPlaying() ? gain : 0.0F;
  return true;
}

void StreamingVoiceContext::OnBufferStart(void* value) {
  auto* context = static_cast<idSoundVoice_XAudio2::bufferContext_t*>(value);
  if (context == nullptr || context->voice == nullptr) return;
  idSoundVoice_XAudio2* voice = context->voice;
  voice->playingSample = context->sample;
  voice->playingStreamID = context->streamID;
  voice->playingSampleStartMsec = CurrentMilliseconds();
  if (context->sampleRate > 0)
    voice->sampleRate = static_cast<std::uint32_t>(context->sampleRate);
}

void StreamingVoiceContext::OnBufferEnd(void* value) {
  auto* context = static_cast<idSoundVoice_XAudio2::bufferContext_t*>(value);
  if (context != nullptr && context->voice != nullptr)
    context->voice->QueueOnBufferEnd(context);
}

void StreamingVoiceContext::OnVoiceError(void* source, long error) {
  idLib::Warning("VOICE ERROR!!! source=%p error=%ld", source, error);
}
