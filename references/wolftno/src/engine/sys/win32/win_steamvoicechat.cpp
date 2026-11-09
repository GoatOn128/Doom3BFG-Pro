#include "engine/sys/win32/win_steamvoicechat.h"

#include "shared/idlib/csystems/cvarsystemlocal.h"

#include <algorithm>
#include <cstring>

idCVar net_voice("net_voice", "1", CVAR_BOOL | CVAR_ARCHIVE, "enable network voice chat");
idCVar net_voicePushToTalk("net_voicePushToTalk", "0", CVAR_BOOL,
                          "require push-to-talk for voice capture");
idCVar net_voiceVolume("net_voiceVolume", "1.0", CVAR_FLOAT,
                      "voice playback volume", 0.0F, 2.0F);
idCVar net_voice_throttleStart("net_voice_throttleStart", "1000", CVAR_INTEGER,
                              "voice capture throttle threshold");
idCVar net_voiceDebug("net_voiceDebug", "0", CVAR_INTEGER,
                     "print voice-chat diagnostics");

namespace {
idSteamVoicePlatform* voicePlatform = nullptr;
idVoicePlaybackSink* (*playbackSinkFactory)(const lobbyUser_t*) = nullptr;
constexpr int kSteamVoiceSampleRate = 11025;
}

void Win_SetSteamVoicePlatform(idSteamVoicePlatform* platform) { voicePlatform = platform; }
void Win_SetVoicePlaybackSinkFactory(idVoicePlaybackSink* (*factory)(const lobbyUser_t*)) {
  playbackSinkFactory = factory;
}
idSteamVoicePlaybackCallbacks::~idSteamVoicePlaybackCallbacks() = default;

idSteamVoiceBufferPool::idSteamVoiceBufferPool() {
  freeBuffers.reserve(buffers.size()); for (auto& buffer : buffers) freeBuffers.push_back(buffer.data());
}
std::uint8_t* idSteamVoiceBufferPool::Acquire() {
  if (freeBuffers.empty()) return nullptr; auto* result = freeBuffers.back(); freeBuffers.pop_back(); return result;
}
void idSteamVoiceBufferPool::Release(std::uint8_t* buffer) {
  if (buffer != nullptr && std::find(freeBuffers.begin(), freeBuffers.end(), buffer) == freeBuffers.end())
    freeBuffers.push_back(buffer);
}

idSteamVoicePlayback::idSteamVoicePlayback(idSteamVoicePlaybackCallbacks* value,
                                           idVoicePlaybackSink* sinkValue)
    : callbacks(value), sink(sinkValue) {}
idSteamVoicePlayback::~idSteamVoicePlayback() { if (sink != nullptr) sink->Stop(); }
int idSteamVoicePlayback::Decompress(const std::uint8_t* compressed, int size,
    std::int16_t* output, int outputBytes) {
  return voicePlatform != nullptr ? voicePlatform->DecompressVoice(compressed, size, output,
                                                                   outputBytes, kSteamVoiceSampleRate) : 0;
}
bool idSteamVoicePlayback::Play(const std::uint8_t* compressed, int size) {
  if (compressed == nullptr || size <= 0 || sink == nullptr) return false;
  std::array<std::int16_t, 8192> pcm{}; const int bytes = Decompress(compressed, size, pcm.data(),
                                                                   static_cast<int>(pcm.size()*sizeof(std::int16_t)));
  if (bytes <= 0 || !sink->Submit(pcm.data(), bytes / static_cast<int>(sizeof(std::int16_t)),
                                  kSteamVoiceSampleRate)) return false;
  if (!isPlaying) { isPlaying = true; if (callbacks != nullptr) callbacks->OnPlaybackStart(); }
  return true;
}
void idSteamVoicePlayback::OnBufferStart(void*) {
  if (!isPlaying) { isPlaying=true; if(callbacks!=nullptr)callbacks->OnPlaybackStart(); }
}
void idSteamVoicePlayback::OnBufferEnd(void* context) {
  bufferPool.Release(static_cast<std::uint8_t*>(context)); isPlaying=false;
  if(callbacks!=nullptr)callbacks->OnPlaybackStop();
}
void idSteamVoicePlayback::OnVoiceError(void* context,long) { OnBufferEnd(context); }

idSteamVoicePeer::idSteamVoicePeer(const lobbyUser_t* value) : user(value) {}
bool idSteamVoicePeer::AllocPlayback() {
  if(play!=nullptr)return true; if(playbackSinkFactory==nullptr)return false;
  play=std::make_unique<idSteamVoicePlayback>(this,playbackSinkFactory(user)); return true;
}
bool idSteamVoicePeer::QueueCompressedAudio(const std::uint8_t* data,int size) {
  return (play!=nullptr||AllocPlayback())&&play->Play(data,size);
}
void idSteamVoicePeer::OnPlaybackStart(){playing=true;}
void idSteamVoicePeer::OnPlaybackStop(){playing=false;}

idVoiceChatMgrSteam::~idVoiceChatMgrSteam(){Shutdown();}
bool idVoiceChatMgrSteam::Init(void*) { initialized=voicePlatform!=nullptr; return initialized; }
void idVoiceChatMgrSteam::Shutdown(){StopRecording();peers.clear();initialized=false;}
void idVoiceChatMgrSteam::StartRecording(){if(initialized&&!recording&&voicePlatform!=nullptr){voicePlatform->StartVoiceRecording();recording=true;}}
void idVoiceChatMgrSteam::StopRecording(){if(recording&&voicePlatform!=nullptr)voicePlatform->StopVoiceRecording();recording=false;}
bool idVoiceChatMgrSteam::GetLocalChatDataInternal(int,std::uint8_t* data,int& dataSize){
  if(!recording||voicePlatform==nullptr||data==nullptr||dataSize<=0){dataSize=0;return false;}
  const int bytes=voicePlatform->GetVoice(data,dataSize);dataSize=(std::max)(0,bytes);return bytes>0;
}
bool idVoiceChatMgrSteam::TalkerHasData(int){return recording&&voicePlatform!=nullptr&&voicePlatform->AvailableVoiceBytes()>0;}
bool idVoiceChatMgrSteam::RegisterTalkerInternal(int index){
  if(index<0||index>=static_cast<int>(talkers.size()))return false;
  if(talkers[index].isLocal){StartRecording();return true;}
  if(static_cast<int>(peers.size())<=index)peers.resize(index+1);
  peers[index]=std::make_unique<idSteamVoicePeer>(talkers[index].user);return true;
}
void idVoiceChatMgrSteam::UnregisterTalkerInternal(int index){
  if(index<0)return;if(index<static_cast<int>(talkers.size())&&talkers[index].isLocal)StopRecording();
  if(index<static_cast<int>(peers.size()))peers[index].reset();
}
void idVoiceChatMgrSteam::SubmitIncomingChatDataInternal(int index,const std::uint8_t* data,int size){
  if(index>=0&&index<static_cast<int>(peers.size())&&peers[index]!=nullptr)peers[index]->QueueCompressedAudio(data,size);
}
void idVoiceChatMgrSteam::Pump(){if(!net_voice.GetBool())StopRecording();}
void idVoiceChatMgrSteam::ProcessInput(bool pressed){
  if(!net_voice.GetBool()){StopRecording();return;}
  if(!net_voicePushToTalk.GetBool()||pressed)StartRecording();else StopRecording();
}
