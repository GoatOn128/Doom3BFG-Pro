#include "engine/sound/sound_runtime.h"

#include "engine/decls/declmanager.h"
#include "shared/idlib/lib.h"

idSoundSystemLocal soundSystemLocal;
idSoundSystem* soundSystem = &soundSystemLocal;

idSoundSystem::~idSoundSystem() = default;

idSoundHardware_XAudio2::~idSoundHardware_XAudio2() {
  Shutdown();
}

idSoundSystemLocal::idSoundSystemLocal() = default;

idSoundSystemLocal::~idSoundSystemLocal() {
  Shutdown();
}

void idSoundSystemLocal::Init() {
  needsRestart = false;
  muted = false;
  hardware.Init();
  if (globalSoundWorld == nullptr) globalSoundWorld = AllocSoundWorldLocal();
  currentSoundWorld = globalSoundWorld;
}

void idSoundSystemLocal::Shutdown() {
  if (editorSoundWorld != nullptr && editorSoundWorld != globalSoundWorld) {
    delete editorSoundWorld;
  }
  editorSoundWorld = nullptr;
  if (globalSoundWorld != nullptr) delete globalSoundWorld;
  globalSoundWorld = nullptr;
  currentSoundWorld = nullptr;
  hardware.Shutdown();
}

idSoundWorldLocal* idSoundSystemLocal::AllocSoundWorldLocal() {
  return new idSoundWorldLocal;
}

idSoundWorld* idSoundSystemLocal::AllocSoundWorld(const char* name,
                                                  void* renderWorld) {
  (void)name;
  (void)renderWorld;
  return AllocSoundWorldLocal();
}

void idSoundSystemLocal::FreeSoundWorld(idSoundWorld* world) {
  if (world == nullptr || world == globalSoundWorld || world == editorSoundWorld)
    return;
  delete world;
}

void idSoundSystemLocal::SetPlayingSoundWorld(idSoundWorld* world) {
  currentSoundWorld = static_cast<idSoundWorldLocal*>(world);
}

void idSoundSystemLocal::SetMixingDecl(const idDeclSoundMixing* mixing) {
  currentMixing = mixing;
  hardware.SetBusMixing(mixing);
}

const idDeclSoundMixing* idSoundSystemLocal::GetMixingDecl() {
  return currentMixing;
}

idSoundWorld* idSoundSystemLocal::GetPlayingSoundWorld() {
  return currentSoundWorld;
}

void idSoundSystemLocal::SetEditorSoundWorld(idSoundWorld* world) {
  editorSoundWorld = static_cast<idSoundWorldLocal*>(world);
}

idSoundWorld* idSoundSystemLocal::GetEditorSoundWorld() {
  return editorSoundWorld;
}

idSoundWorld* idSoundSystemLocal::GetGlobalSoundWorld() {
  return globalSoundWorld;
}

void idSoundSystemLocal::Restart() {
  Shutdown();
  Init();
}

void idSoundSystemLocal::Render(bool threaded) {
  (void)threaded;
  if (currentSoundWorld != nullptr) currentSoundWorld->UpdateMainThread(0);
  hardware.Update();
}

void idSoundSystemLocal::RenderDebugPage(idRenderModelGui* gui) { (void)gui; }
void idSoundSystemLocal::WaitForSoundThread() {}
void idSoundSystemLocal::MuteBackgroundMusic(bool mute) { musicMuted = mute; }
void idSoundSystemLocal::SetMute(bool mute) { muted = mute; }
bool idSoundSystemLocal::IsMuted() const { return muted; }

void idSoundSystemLocal::OnReloadSound(const idDecl* sound) {
  if (currentSoundWorld != nullptr) currentSoundWorld->OnReloadSound(sound);
}

void idSoundSystemLocal::StopAllSounds() {
  if (currentSoundWorld != nullptr) currentSoundWorld->StopAllSounds();
}

void idSoundSystemLocal::StopVoicesWithSample(const idSoundSample* sample) {
  for (idSoundVoice_XAudio2* voice : hardware.voices) {
    if (voice != nullptr &&
        (voice->sample == sample || voice->tailSample == sample ||
         voice->playingSample == sample)) {
      voice->Stop();
    }
  }
}

void* idSoundSystemLocal::GetIXAudio2() { return hardware.GetIXAudio2(); }
int idSoundSystemLocal::GetNumSpeakers() const { return hardware.outputChannels; }
int idSoundSystemLocal::GetSpeakerMask() const { return hardware.channelMask; }
std::int64_t idSoundSystemLocal::GetSampleClock() const { return hardware.GetSampleClock(); }
int idSoundSystemLocal::GetSampleRate() const { return hardware.outputSampleRate; }

idSoundVoice_XAudio2* idSoundSystemLocal::AllocateVoice(
    const idSoundSample* leadin, const idSoundSample* looping) {
  return hardware.AllocateVoice(leadin, looping);
}

void idSoundSystemLocal::FreeVoice(idSoundVoice_XAudio2* voice) {
  hardware.FreeVoice(voice);
}

void idSoundThread::Render() {}
int idSoundThread::Run() { return 0; }

void ArgCompletion_SoundName(idAutoComplete* autoComplete) {
  (void)autoComplete;
}

void TestSound_f(const idCmdArgs* args) { (void)args; }
void RestartSound_f(const idCmdArgs* args) {
  (void)args;
  soundSystemLocal.Restart();
}
