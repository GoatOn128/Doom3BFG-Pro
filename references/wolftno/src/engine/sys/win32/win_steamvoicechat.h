#pragma once

#include "engine/sys/sys_voicechat.h"

#include <array>
#include <cstdint>
#include <deque>
#include <memory>
#include <vector>

class idSteamVoicePlatform {
public:
  virtual ~idSteamVoicePlatform() = default;
  virtual void StartVoiceRecording() = 0;
  virtual void StopVoiceRecording() = 0;
  virtual int AvailableVoiceBytes() = 0;
  virtual int GetVoice(std::uint8_t* output, int capacity) = 0;
  virtual int DecompressVoice(const std::uint8_t* compressed, int compressedSize,
                              std::int16_t* output, int outputBytes, int sampleRate) = 0;
};

class idVoicePlaybackSink {
public:
  virtual ~idVoicePlaybackSink() = default;
  virtual bool Submit(const std::int16_t* samples, int sampleCount, int sampleRate) = 0;
  virtual void Stop() = 0;
};

void Win_SetSteamVoicePlatform(idSteamVoicePlatform* platform);
void Win_SetVoicePlaybackSinkFactory(idVoicePlaybackSink* (*factory)(const lobbyUser_t* user));

class idSteamVoicePlaybackCallbacks {
public:
  virtual ~idSteamVoicePlaybackCallbacks();
  virtual void OnPlaybackStart() {}
  virtual void OnPlaybackStop() {}
};

class idSteamVoiceBufferPool {
public:
  idSteamVoiceBufferPool();
  std::uint8_t* Acquire();
  void Release(std::uint8_t* buffer);
private:
  std::array<std::array<std::uint8_t, 8192>, 16> buffers{};
  std::vector<std::uint8_t*> freeBuffers;
};

class idSteamVoicePlayback {
public:
  explicit idSteamVoicePlayback(idSteamVoicePlaybackCallbacks* callbacksValue,
                                idVoicePlaybackSink* sinkValue);
  ~idSteamVoicePlayback();
  bool Play(const std::uint8_t* compressed, int size);
  int Decompress(const std::uint8_t* compressed, int size, std::int16_t* output,
                 int outputBytes);
  void OnBufferStart(void* context);
  void OnBufferEnd(void* context);
  void OnVoiceError(void* context, long error);
private:
  idSteamVoiceBufferPool bufferPool;
  idSteamVoicePlaybackCallbacks* callbacks{};
  std::unique_ptr<idVoicePlaybackSink> sink;
  bool isPlaying{};
};

class idSteamVoicePeer final : public idSteamVoicePlaybackCallbacks {
public:
  explicit idSteamVoicePeer(const lobbyUser_t* userValue);
  ~idSteamVoicePeer() override = default;
  bool AllocPlayback();
  bool QueueCompressedAudio(const std::uint8_t* data, int size);
  void OnPlaybackStart() override;
  void OnPlaybackStop() override;
  const lobbyUser_t* user{};
  std::unique_ptr<idSteamVoicePlayback> play;
  bool playing{};
};

class idVoiceChatMgrSteam final : public idVoiceChatMgr {
public:
  idVoiceChatMgrSteam();
  ~idVoiceChatMgrSteam() override;
  bool Init(void* soundSystem = nullptr);
  void Shutdown();
  void StartRecording();
  void StopRecording();
  bool GetLocalChatDataInternal(int talkerIndex, std::uint8_t* data, int& dataSize) override;
  bool TalkerHasData(int talkerIndex) override;
  bool RegisterTalkerInternal(int index) override;
  void UnregisterTalkerInternal(int index) override;
  void SubmitIncomingChatDataInternal(int talkerIndex, const std::uint8_t* data,
                                      int dataSize) override;
  void Pump();
  void ProcessInput(bool pushToTalkPressed);
private:
  std::vector<std::unique_ptr<idSteamVoicePeer>> peers;
  bool initialized{};
  bool recording{};
};
