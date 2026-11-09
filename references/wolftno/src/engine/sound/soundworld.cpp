#include "engine/sound/sound_runtime.h"

#include "shared/idlib/lib.h"

#include <algorithm>

idSoundWorld::~idSoundWorld() = default;

idSoundBSPFile::idSoundBSPFile() { Clear(); }
idSoundBSPFile::~idSoundBSPFile() = default;

listener_t::listener_t() = default;
listener_t::~listener_t() = default;

soundEnvironment_t::soundEnvironment_t() = default;
soundEnvironment_t& soundEnvironment_t::operator=(const soundEnvironment_t& other) {
  ambiences = other.ambiences;
  DryGain = other.DryGain;
  RoomGain = other.RoomGain;
  RoomHFGain = other.RoomHFGain;
  DecayTime = other.DecayTime;
  DecayHFTime = other.DecayHFTime;
  ReflectionsGain = other.ReflectionsGain;
  ReflectionsDelay = other.ReflectionsDelay;
  ReverbGain = other.ReverbGain;
  ReverbDelay = other.ReverbDelay;
  Diffusion = other.Diffusion;
  Density = other.Density;
  HFReference = other.HFReference;
  return *this;
}
soundEnvironment_t::ambiences_t& soundEnvironment_t::ambiences_t::operator=(
    const ambiences_t&) = default;
void soundEnvironment_t::ambiences_t::Start(bool startSilent) {
  (void)startSilent;
}

idSoundWorldLocal::idSoundWorldLocal() = default;

idSoundWorldLocal::~idSoundWorldLocal() {
  StopAllSounds();
  for (idSoundEmitterLocal* emitter : emitters) delete emitter;
  emitters.clear();
}

void idSoundWorldLocal::SetNumListeners(int num) {
  numListeners = std::clamp(num, 1, 2);
  listenerIndex = 0;
}

void idSoundWorldLocal::AllocRouting(listener_t* listener) { (void)listener; }

void idSoundWorldLocal::CurrentShakeAmplitude(float* amplitude,
                                               float* controller,
                                               int listenerId) {
  (void)listenerId;
  if (amplitude != nullptr) *amplitude = 0.0F;
  if (controller != nullptr) *controller = 0.0F;
}

void idSoundWorldLocal::PlaceListener(const idVec3& origin, const idMat3& axis,
                                       const idVec3& velocity, int listenerId) {
  (void)origin;
  (void)axis;
  (void)velocity;
  listenerIndex = std::clamp(listenerId, 0, numListeners - 1);
}

void idSoundWorldLocal::BlendEnv(const soundEnvironment_t* env, float amount,
                                 int listenerId) {
  (void)env;
  (void)amount;
  (void)listenerId;
}

void idSoundWorldLocal::DrawDebug(idRenderWorld* rw) { (void)rw; }

void idSoundWorldLocal::OnReloadSound(const idDecl* shader) {
  (void)shader;
  for (idSoundEmitterLocal* emitter : emitters) emitter->OnReloadSound(nullptr);
}

void idSoundWorldLocal::Duck(const idDeclDuck* duck, int startTime, int endTime,
                             int listenerId) {
  (void)duck;
  (void)startTime;
  (void)endTime;
  (void)listenerId;
}

void idSoundWorldLocal::DuckBlend(const idDeclDuck* duck, float amount) {
  (void)duck;
  (void)amount;
}

int idSoundWorldLocal::NumEmitters() const { return static_cast<int>(emitters.size()); }

idSoundEmitter* idSoundWorldLocal::EmitterForIndex(int emitterIndex) {
  if (emitterIndex < 0 || emitterIndex >= NumEmitters()) return nullptr;
  return emitters[emitterIndex];
}

void idSoundWorldLocal::StopAllSounds() {
  for (idSoundEmitterLocal* emitter : emitters) emitter->StopSound(SND_CHANNEL_ANY);
  for (idSoundChannel* channel : channels) delete channel;
  channels.clear();
}

void idSoundWorldLocal::Fade(soundShaderGroups_t groups, float to, int over,
                             bool linear) {
  (void)groups;
  (void)to;
  (void)over;
  (void)linear;
}

void idSoundWorldLocal::StopChannel(soundChannel_t channel) {
  for (idSoundEmitterLocal* emitter : emitters) emitter->StopSound(channel);
}

int idSoundWorldLocal::FindEnvironment(const idVec3* origin) const {
  (void)origin;
  return 0;
}

const soundEnvironment_t* idSoundWorldLocal::GetEnvironmentParams(int index) const {
  (void)index;
  static const soundEnvironment_t environment;
  return &environment;
}

void idSoundWorldLocal::BlendEnvironmentParamsAmount(float amount,
                                                     int listenerId) {
  (void)amount;
  (void)listenerId;
}

int idSoundWorldLocal::FindDoor(const idVec3* origin) const {
  (void)origin;
  return -1;
}

void idSoundWorldLocal::SetDoor(int doorNum, float value) {
  (void)doorNum;
  (void)value;
}

bool idSoundWorldLocal::Trace(const idVec3* start, const idVec3* end) const {
  return start != nullptr && end != nullptr;
}

void idSoundWorldLocal::Pause(soundShaderGroups_t groups) {
  (void)groups;
  paused = true;
}

void idSoundWorldLocal::UnPause() { paused = false; }
bool idSoundWorldLocal::IsPaused() const { return paused; }
void idSoundWorldLocal::StepSoundTime() { ++soundTime; }
std::int64_t idSoundWorldLocal::GetSoundTime() const { return soundTime; }

idSoundChannel* idSoundWorldLocal::AllocSoundChannel() {
  auto* channel = new idSoundChannel;
  channels.push_back(channel);
  return channel;
}

void idSoundWorldLocal::FreeSoundChannel(idSoundChannel* channel) {
  auto iterator = std::find(channels.begin(), channels.end(), channel);
  if (iterator != channels.end()) {
    delete *iterator;
    channels.erase(iterator);
  }
}

int idSoundWorldLocal::PlayGlobalShaderDirectly(const idSoundShader* shader,
                                                soundChannel_t channel,
                                                float diversity) {
  (void)diversity;
  idSoundEmitter* emitter = AllocSoundEmitter();
  return emitter == nullptr ? 0 : emitter->StartSound(channel, shader, nullptr).value;
}

void idSoundWorldLocal::RemoveChannelPlayingShader(const idSoundShader* shader,
                                                   idSoundChannel* channel) {
  (void)shader;
  FreeSoundChannel(channel);
}

int idSoundWorldLocal::GetBestListenerForEmitter(
    const idSoundEmitterLocal* emitter) const {
  (void)emitter;
  return listenerIndex;
}

int idSoundWorldLocal::PlayPositionedShaderDirectly(const idSoundShader* shader,
                                                    const idVec3* origin,
                                                    const idMat3* axis,
                                                    float diversity) {
  (void)origin;
  (void)axis;
  return PlayGlobalShaderDirectly(shader, SND_CHANNEL_ANY, diversity);
}

void idSoundWorldLocal::PlayRandomAmbiences(const listener_t* listener) {
  (void)listener;
}

void idSoundWorldLocal::AddChannelPlayingShader(const idSoundShader* shader,
                                                idSoundChannel* channel) {
  (void)shader;
  if (channel != nullptr &&
      std::find(channels.begin(), channels.end(), channel) == channels.end())
    channels.push_back(channel);
}

int idSoundWorldLocal::GetChannelsPlayingShader(const idSoundShader* shader) const {
  return static_cast<int>(std::count_if(
      channels.begin(), channels.end(), [shader](const idSoundChannel* channel) {
        return channel != nullptr && channel->soundShader == shader;
      }));
}

void idSoundWorldLocal::SetEnvironmentParams(int index, const soundEnvironment_t* env) {
  (void)index;
  (void)env;
}

void idSoundWorldLocal::BlendEnvironmentParams(const soundEnvironment_t* env,
                                               float amount, int listenerId) {
  (void)env;
  (void)amount;
  (void)listenerId;
}

void idSoundWorldLocal::BlendEnvironmentParamsPush(
    const soundEnvironment_t* env, float amount, int listenerId) {
  BlendEnvironmentParams(env, amount, listenerId);
}

void idSoundWorldLocal::BlendEnvironmentParamsPop(int listenerId) {
  (void)listenerId;
}

void idSoundWorldLocal::BlendEnvironmentParamsPopAllExceptTop(int listenerId) {
  (void)listenerId;
}

bool idSoundWorldLocal::LoadMap(const char* mapName) {
  return mapName != nullptr && mapName[0] != '\0';
}

void idSoundWorldLocal::Update(int currentTime) {
  for (idSoundEmitterLocal* emitter : emitters) emitter->Update(nullptr, currentTime);
}

void idSoundWorldLocal::UpdateForListener(listener_t* listener, int currentTime) {
  for (idSoundEmitterLocal* emitter : emitters) emitter->Update(listener, currentTime);
}

void idSoundWorldLocal::UpdateMainThread(int currentTime) { Update(currentTime); }

idSoundEmitter* idSoundWorldLocal::AllocSoundEmitter() {
  return AllocEmitter();
}

idSoundEmitterLocal* idSoundWorldLocal::AllocEmitter() {
  auto* emitter = new idSoundEmitterLocal;
  emitter->Init(static_cast<int>(emitters.size()), this);
  emitters.push_back(emitter);
  return emitter;
}
