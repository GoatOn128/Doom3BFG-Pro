#include "tungsten/game/animation/facemgr.h"

#include <algorithm>

// DIA: idFaceMgr::SetEnableEyeTracking
void idFaceMgr::SetEnableEyeTracking(idEntity*, bool enable) { eyeTrackingEnabled = enable; ++setEyeTrackingCount; }
// DIA: idFaceMgr::SuppressBlinking
void idFaceMgr::SuppressBlinking(bool suppress) { blinkingSuppressed = suppress; }
// DIA: idFaceMgr::SuppressLidDeformation
void idFaceMgr::SuppressLidDeformation(bool suppress) { lidDeformationSuppressed = suppress; }
// DIA: idFaceAnimator::Shutdown
void idFaceAnimator::Shutdown(idEntity*) { Reset(nullptr, nullptr, nullptr); initialized = false; }
// DIA: idFaceAnimator::PlayWebAnimation
void idFaceAnimator::PlayWebAnimation(const std::string& animation, int blendMs) {
  currentAnimation = animation; blendTimeMs = std::max(0, blendMs); currentWeight = 1.0F;
}
// DIA: `idFaceAnimator::PlayWebAnimation'::`1'::dtor$0
// DIA: `idFaceAnimator::PlayWebAnimation'::`1'::dtor$1
// DIA: `idFaceAnimator::PlayWebAnimation'::`1'::dtor$2
// DIA: idFaceAnimator::OwnsAnimator
bool idFaceAnimator::OwnsAnimator(const void* animator) const { return animator != nullptr && animator == ownedAnimator; }
// DIA: idFaceAnimator::SetEnableAnimation
void idFaceAnimator::SetEnableAnimation(idEntity*, bool enable) { animationEnabled = enable; if (!enable) currentAnimation.clear(); }
// DIA: idFaceAnimator::SetEnableGestures
void idFaceAnimator::SetEnableGestures(idEntity*, bool enable) { gesturesEnabled = enable; }
// DIA: idFaceMgr::Shutdown
void idFaceMgr::Shutdown(idEntity* owner) { animator.Shutdown(owner); StopLipsync(owner, nullptr); initialized = false; }
// DIA: idFaceMgr::DormantEnd
void idFaceMgr::DormantEnd(idEntity*, int timeDormant) { if (voicePlaying) { voiceStartTime += timeDormant; voiceEndTime += timeDormant; } }
// DIA: idFaceMgr::SetEnableAnimation
void idFaceMgr::SetEnableAnimation(idEntity* owner, bool enable) { animationEnabled = enable; animator.SetEnableAnimation(owner, enable); }
// DIA: idFaceMgr::SetEnableMoodAnimation
void idFaceMgr::SetEnableMoodAnimation(idEntity*, bool enable) { moodAnimationEnabled = enable; ++setMoodAnimationCount; }
// DIA: idFaceMgr::SetEnableLipSyncAnimation
void idFaceMgr::SetEnableLipSyncAnimation(idEntity*, bool enable) { lipSyncAnimationEnabled = enable; if (!enable) voicePlaying = false; }
// DIA: idFaceMgr::SetEnableGestureAnimation
void idFaceMgr::SetEnableGestureAnimation(idEntity* owner, bool enable) { gestureAnimationEnabled = enable; animator.SetEnableGestures(owner, enable); }
// DIA: idFaceMgr::SetEnableBlinking
void idFaceMgr::SetEnableBlinking(idEntity*, bool enable) { blinkingEnabled = enable; ++setBlinkingCount; }
// DIA: idFaceMgr::SetEnableLidDeformation
void idFaceMgr::SetEnableLidDeformation(idEntity*, bool enable) { lidDeformationEnabled = enable; }
// DIA: idFaceMgr::StartLipsync
void idFaceMgr::StartLipsync() { if (lipSyncAnimationEnabled && face_skipLipsync.GetInteger() == 0) animator.StartLipSync(this, nullptr, nullptr); }
// DIA: idFaceMgr::CalcEyeFocusPoint
idVec3 idFaceMgr::CalcEyeFocusPoint(const idVec3& eyeOrigin, const idVec3& requestedFocus) const {
  idVec3 focus = requestedFocus; focus.z += face_eyeZOffset.GetFloat();
  idVec3 delta = focus - eyeOrigin; const float distance = std::max(delta.Length(), face_jitterMinDist.GetFloat());
  const float jitter = face_eyeJitter.GetFloat() / std::max(distance, 1.0F);
  focus.x += jitter; focus.y -= jitter; return focus;
}
// DIA: idFaceMgr::AlignEye
idVec3 idFaceMgr::AlignEye(const idVec3& eyeOrigin, const idVec3& focusPoint, bool leftEye) const {
  idVec3 direction = focusPoint - eyeOrigin; direction.y += (leftEye ? 1.0F : -1.0F) * face_eyeSpreadScale.GetFloat(); direction.Normalize(); return direction;
}
// DIA: idFaceMgr::CloseEyes
void idFaceMgr::CloseEyes(idEntity*) { eyesClosed = true; animator.PlayAnimation("close_eyes"); }
// DIA: idFaceAnimator::Update
void idFaceAnimator::Update(idFaceMgr*, idEntity*, void*) { if (initialized && animationEnabled) ++updateCount; }
// DIA: idFaceAnimator::StartLipSync
void idFaceAnimator::StartLipSync(idFaceMgr*, idEntity*, void*) { lipSyncPlaying = animationEnabled; }
// DIA: idFaceAnimator::AnimateMood
void idFaceAnimator::AnimateMood(idFaceMgr*, idEntity*, void*) { if (animationEnabled) PlayAnimation("mood"); }
// DIA: idFaceAnimator::Reset
void idFaceAnimator::Reset(idFaceMgr*, idEntity*, void*) { currentAnimation.clear(); currentWeight = 0.0F; lipSyncPlaying = false; }
// DIA: idFaceAnimator::LipSyncFinished
void idFaceAnimator::LipSyncFinished(idFaceMgr*, idEntity*, void*) { lipSyncPlaying = false; }
// DIA: idFaceAnimator::PlayAnimation
void idFaceAnimator::PlayAnimation(const std::string& animation, float weight) { if (animationEnabled) { currentAnimation = animation; currentWeight = std::clamp(weight, 0.0F, 1.0F); } }
// DIA: idFaceAnimator::AnimateEyes
void idFaceAnimator::AnimateEyes(const idVec3& focusPoint) { eyeFocus = focusPoint; }
// DIA: idFaceAnimator::Blink
void idFaceAnimator::Blink(idFaceMgr*, idEntity*, void*) { ++blinkCount; PlayAnimation("blink"); }
// DIA: idFaceAnimator::Glance
void idFaceAnimator::Glance(idFaceMgr*, idEntity*, void*, int index) { if (gesturesEnabled) PlayAnimation("glance_" + std::to_string(index)); }
// DIA: idFaceAnimator::Fidget
void idFaceAnimator::Fidget(idFaceMgr*, idEntity*, void*, int index) { if (gesturesEnabled) PlayAnimation("fidget_" + std::to_string(index)); }
// DIA: idFaceMgr::GetVoiceTrackTime
int idFaceMgr::GetVoiceTrackTime() const { return voicePlaying ? std::max(0, Now() - voiceStartTime) : 0; }
// DIA: idFaceMgr::LipSyncFinished
void idFaceMgr::LipSyncFinished(idEntity* owner, void* treeAnimator) { voicePlaying = false; lastVoiceEndTime = Now(); animator.LipSyncFinished(this, owner, treeAnimator); }
bool idFaceMgr::LipSyncFinished() const { return !voicePlaying || Now() >= voiceEndTime; }
// DIA: idFaceMgr::StopLipsync
void idFaceMgr::StopLipsync(idEntity* owner, void* treeAnimator) { LipSyncFinished(owner, treeAnimator); activeVoiceOver = nullptr; activeTrack = -1; }
// DIA: idFaceMgr::ReallyStartVoiceOver
void idFaceMgr::ReallyStartVoiceOver(idEntity*, void*) { if (activeVoiceOver != nullptr && activeTrack >= 0) { voicePlaying = true; voiceStartTime = Now(); voiceEndTime = voiceStartTime + activeVoiceOver->tracks[static_cast<std::size_t>(activeTrack)].durationMs; StartLipsync(); } }
// DIA: `idFaceMgr::ReallyStartVoiceOver'::`1'::dtor$0
// DIA: idFaceMgr::Reset
void idFaceMgr::Reset(idEntity* owner, void* treeAnimator) { StopLipsync(owner, treeAnimator); animator.Reset(this, owner, treeAnimator); eyesClosed = false; }
// DIA: idFaceMgr::RecentlyPlayedVoiceOver
bool idFaceMgr::RecentlyPlayedVoiceOver(int thresholdMs) const { return Now() - lastVoiceEndTime < thresholdMs; }
// DIA: idFaceAnimator::Init
bool idFaceAnimator::Init(idFaceMgr*, idEntity*, void* treeAnimator) { initialized = treeAnimator != nullptr; ownedAnimator = treeAnimator; return initialized; }
// DIA: `idFaceAnimator::Init'::`1'::dtor$0
// DIA: `idFaceAnimator::Init'::`1'::dtor$1
// DIA: `idFaceAnimator::Init'::`1'::dtor$2
// DIA: idFaceMgr::StartVOIfStreamedSampleIsReady
bool idFaceMgr::StartVOIfStreamedSampleIsReady(idEntity* owner, void* treeAnimator) { if (activeVoiceOver == nullptr || activeTrack < 0 || !activeVoiceOver->tracks[static_cast<std::size_t>(activeTrack)].streamedReady) return false; ReallyStartVoiceOver(owner, treeAnimator); return true; }
// DIA: idFaceMgr::SelectNewGlanceIndex
int idFaceMgr::SelectNewGlanceIndex(idEntity*, void*) { glanceIndex = (glanceIndex + 1) % 4; return glanceIndex; }
// DIA: idFaceMgr::SelectNewFidgetIndex
int idFaceMgr::SelectNewFidgetIndex(idEntity*, void*) { fidgetIndex = (fidgetIndex + 1) % 3; return fidgetIndex; }
// DIA: idFaceAnimator::~idFaceAnimator
idFaceAnimator::~idFaceAnimator() = default;
// DIA: `idFaceAnimator::~idFaceAnimator'::`1'::dtor$0
// DIA: `idFaceAnimator::~idFaceAnimator'::`1'::dtor$1
// DIA: `idFaceAnimator::~idFaceAnimator'::`1'::dtor$2
// DIA: `idFaceAnimator::~idFaceAnimator'::`1'::dtor$3
// DIA: `idFaceAnimator::~idFaceAnimator'::`1'::dtor$4
// DIA: `idFaceAnimator::~idFaceAnimator'::`1'::dtor$5
// DIA: `idFaceAnimator::~idFaceAnimator'::`1'::dtor$6
// DIA: `idFaceAnimator::~idFaceAnimator'::`1'::dtor$7
// DIA: `idFaceAnimator::~idFaceAnimator'::`1'::dtor$8
// DIA: `idFaceAnimator::~idFaceAnimator'::`1'::dtor$9
// DIA: `idFaceAnimator::~idFaceAnimator'::`1'::dtor$10
// DIA: `idFaceAnimator::~idFaceAnimator'::`1'::dtor$11
// DIA: `idFaceAnimator::~idFaceAnimator'::`1'::dtor$12
// DIA: `idFaceAnimator::~idFaceAnimator'::`1'::dtor$13
// DIA: `idFaceAnimator::~idFaceAnimator'::`1'::dtor$14
// DIA: idFaceMgr::~idFaceMgr
idFaceMgr::~idFaceMgr() = default;
// DIA: `idFaceMgr::~idFaceMgr'::`1'::dtor$0
// DIA: `idFaceMgr::~idFaceMgr'::`1'::dtor$1
// DIA: idFaceMgr::UpdateLipSync
void idFaceMgr::UpdateLipSync(idEntity* owner, void* treeAnimator) { if (voicePlaying && Now() >= voiceEndTime) LipSyncFinished(owner, treeAnimator); }
// DIA: `idFaceMgr::UpdateLipSync'::`1'::dtor$0
// DIA: `idFaceMgr::UpdateLipSync'::`1'::dtor$1
// DIA: `idFaceMgr::UpdateLipSync'::`1'::dtor$2
// DIA: idFaceMgr::Update
void idFaceMgr::Update(idEntity* owner, void* treeAnimator, idVec3* focusPoint) { if (!initialized) return; UpdateLipSync(owner, treeAnimator); if (focusPoint != nullptr && eyeTrackingEnabled) animator.AnimateEyes(*focusPoint); animator.Update(this, owner, treeAnimator); ++updateCount; }
// DIA: idFaceAnimator::idFaceAnimator
idFaceAnimator::idFaceAnimator() = default;
// DIA: idFaceAnimator::`scalar deleting destructor'
// DIA: idFaceMgr::idFaceMgr
idFaceMgr::idFaceMgr() = default;
// DIA: idFaceMgr::`scalar deleting destructor'
// DIA: idFaceMgr::LoadVisemeSetAnims
void idFaceMgr::LoadVisemeSetAnims(const std::vector<std::string>& animations) { visemeAnimations = animations; }
// DIA: idFaceMgr::Init
bool idFaceMgr::Init(idEntity* owner, void* treeAnimator) { initialized = animator.Init(this, owner, treeAnimator); return initialized; }
// DIA: idFaceMgr::StartVoiceTrack
bool idFaceMgr::StartVoiceTrack(idEntity* owner, void* treeAnimator, const idDeclVoiceOver* voiceOver, int trackIndex) { if (voiceOver == nullptr || trackIndex < 0 || trackIndex >= static_cast<int>(voiceOver->tracks.size())) return false; activeVoiceOver = voiceOver; activeTrack = trackIndex; return StartVOIfStreamedSampleIsReady(owner, treeAnimator); }
// DIA: `idFaceMgr::StartVoiceTrack'::`1'::dtor$0
// DIA: idFaceMgr::StartVoiceOver
bool idFaceMgr::StartVoiceOver(idEntity* owner, void* treeAnimator, const idDeclVoiceOver* voiceOver) { if (voiceOver == nullptr || voiceOver->tracks.empty()) return false; if (activeVoiceOver == voiceOver && voicePlaying) return true; int track = voiceOver->nextSampleToPlay >= 0 ? voiceOver->nextSampleToPlay % static_cast<int>(voiceOver->tracks.size()) : 0; return StartVoiceTrack(owner, treeAnimator, voiceOver, track); }

int idFaceMgr::Now() const { return gameLocal != nullptr ? gameLocal->GetGameTimeMs() : 0; }

#define FACE_CVAR(name, value, flags, description) idCVar name(#name, value, flags, description)
// DIA: `dynamic initializer for 'face_lipsyncWeight''
// DIA: `dynamic atexit destructor for 'face_lipsyncWeight''
FACE_CVAR(face_lipsyncWeight, "1.0", 4, "maximum lip-sync weight");
// DIA: `dynamic initializer for 'face_blinkScale''
// DIA: `dynamic atexit destructor for 'face_blinkScale''
FACE_CVAR(face_blinkScale, "0.5", 4, "blink interval scale");
// DIA: `dynamic initializer for 'face_eyeJitter''
// DIA: `dynamic atexit destructor for 'face_eyeJitter''
FACE_CVAR(face_eyeJitter, "2.0", 4, "maximum eye jitter");
// DIA: `dynamic initializer for 'face_jitterMinDist''
// DIA: `dynamic atexit destructor for 'face_jitterMinDist''
FACE_CVAR(face_jitterMinDist, "32.0", 4, "minimum eye jitter distance");
// DIA: `dynamic initializer for 'face_eyeDampen''
// DIA: `dynamic atexit destructor for 'face_eyeDampen''
FACE_CVAR(face_eyeDampen, "0.5", 4, "eye tracking dampening");
// DIA: `dynamic initializer for 'face_eyeSpreadScale''
// DIA: `dynamic atexit destructor for 'face_eyeSpreadScale''
FACE_CVAR(face_eyeSpreadScale, "0.1", 4, "eye spread scale");
#define FACE_BOOL_CVAR(name, description) \
  /* DIA: dynamic initializer and atexit destructor are listed below. */ \
  FACE_CVAR(name, "0", 1, description)
// DIA: `dynamic initializer for 'face_skipLidDeform''
// DIA: `dynamic atexit destructor for 'face_skipLidDeform''
FACE_BOOL_CVAR(face_skipLidDeform, "skip lid deformation");
// DIA: `dynamic initializer for 'face_skipLipsync''
// DIA: `dynamic atexit destructor for 'face_skipLipsync''
FACE_BOOL_CVAR(face_skipLipsync, "skip lip sync");
// DIA: `dynamic initializer for 'face_skipBlink''
// DIA: `dynamic atexit destructor for 'face_skipBlink''
FACE_BOOL_CVAR(face_skipBlink, "skip blinking");
// DIA: `dynamic initializer for 'face_skipMood''
// DIA: `dynamic atexit destructor for 'face_skipMood''
FACE_BOOL_CVAR(face_skipMood, "skip mood animation");
// DIA: `dynamic initializer for 'face_skipHead''
// DIA: `dynamic atexit destructor for 'face_skipHead''
FACE_BOOL_CVAR(face_skipHead, "skip head animation");
// DIA: `dynamic initializer for 'face_skipGestures''
// DIA: `dynamic atexit destructor for 'face_skipGestures''
FACE_BOOL_CVAR(face_skipGestures, "skip gestures");
// DIA: `dynamic initializer for 'face_skipProcedural''
// DIA: `dynamic atexit destructor for 'face_skipProcedural''
FACE_BOOL_CVAR(face_skipProcedural, "skip procedural face animation");
#undef FACE_BOOL_CVAR
#define FACE_DEBUG_CVAR(name, flags, description) FACE_CVAR(name, "0", flags, description)
// DIA: `dynamic initializer for 'face_debugWeights''
// DIA: `dynamic atexit destructor for 'face_debugWeights''
FACE_DEBUG_CVAR(face_debugWeights, 2, "show face weights");
// DIA: `dynamic initializer for 'face_debug''
// DIA: `dynamic atexit destructor for 'face_debug''
FACE_DEBUG_CVAR(face_debug, 2, "show face debug information");
// DIA: `dynamic initializer for 'face_debugEyes''
// DIA: `dynamic atexit destructor for 'face_debugEyes''
FACE_DEBUG_CVAR(face_debugEyes, 2, "show eye alignment");
// DIA: `dynamic initializer for 'face_eyeZOffset''
// DIA: `dynamic atexit destructor for 'face_eyeZOffset''
FACE_CVAR(face_eyeZOffset, "0", 4, "eye focus vertical offset");
// DIA: `dynamic initializer for 'face_voStreamDelay''
// DIA: `dynamic atexit destructor for 'face_voStreamDelay''
FACE_CVAR(face_voStreamDelay, "250", 2, "voice streaming delay");
// DIA: `dynamic initializer for 'face_debugTextEvent''
// DIA: `dynamic atexit destructor for 'face_debugTextEvent''
FACE_CVAR(face_debugTextEvent, "", 0, "debug text event filter");
// DIA: `dynamic initializer for 'face_mapWeightsToSine''
// DIA: `dynamic atexit destructor for 'face_mapWeightsToSine''
FACE_DEBUG_CVAR(face_mapWeightsToSine, 1, "map viseme weights to sine");
// DIA: `dynamic initializer for 'face_syncToGameTime''
// DIA: `dynamic atexit destructor for 'face_syncToGameTime''
FACE_DEBUG_CVAR(face_syncToGameTime, 1, "sync face animation to game time");
// DIA: `dynamic initializer for 'face_syncToRealTime''
// DIA: `dynamic atexit destructor for 'face_syncToRealTime''
FACE_DEBUG_CVAR(face_syncToRealTime, 1, "sync face animation to real time");
// DIA: `dynamic initializer for 'face_debugTime''
// DIA: `dynamic atexit destructor for 'face_debugTime''
FACE_DEBUG_CVAR(face_debugTime, 2, "show face timing");
// DIA: `dynamic initializer for 'face_serialize''
// DIA: `dynamic atexit destructor for 'face_serialize''
FACE_DEBUG_CVAR(face_serialize, 1, "serialize face animators");
// DIA: `dynamic initializer for 'face_enableGlancing''
// DIA: `dynamic atexit destructor for 'face_enableGlancing''
FACE_CVAR(face_enableGlancing, "1", 1, "enable glancing");
// DIA: `dynamic initializer for 'face_enableFidgeting''
// DIA: `dynamic atexit destructor for 'face_enableFidgeting''
FACE_CVAR(face_enableFidgeting, "1", 1, "enable fidgeting");
// DIA: `dynamic initializer for 'face_debugFluff''
// DIA: `dynamic atexit destructor for 'face_debugFluff''
FACE_DEBUG_CVAR(face_debugFluff, 2, "face fluff debug entity");
// DIA: `dynamic initializer for 'face_glanceIntervalMin''
// DIA: `dynamic atexit destructor for 'face_glanceIntervalMin''
FACE_CVAR(face_glanceIntervalMin, "3", 4, "minimum glance interval");
// DIA: `dynamic initializer for 'face_glanceIntervalMax''
// DIA: `dynamic atexit destructor for 'face_glanceIntervalMax''
FACE_CVAR(face_glanceIntervalMax, "5", 4, "maximum glance interval");
// DIA: `dynamic initializer for 'face_fidgetIntervalMin''
// DIA: `dynamic atexit destructor for 'face_fidgetIntervalMin''
FACE_CVAR(face_fidgetIntervalMin, "6", 4, "minimum fidget interval");
// DIA: `dynamic initializer for 'face_fidgetIntervalMax''
// DIA: `dynamic atexit destructor for 'face_fidgetIntervalMax''
FACE_CVAR(face_fidgetIntervalMax, "10", 4, "maximum fidget interval");
// DIA: `dynamic initializer for 'face_fluffMinIntervalMin''
// DIA: `dynamic atexit destructor for 'face_fluffMinIntervalMin''
FACE_CVAR(face_fluffMinIntervalMin, "1", 4, "minimum face fluff interval");
// DIA: `dynamic initializer for 'face_fluffMinIntervalMax''
// DIA: `dynamic atexit destructor for 'face_fluffMinIntervalMax''
FACE_CVAR(face_fluffMinIntervalMax, "3", 4, "maximum face fluff interval");
#undef FACE_DEBUG_CVAR
#undef FACE_CVAR
