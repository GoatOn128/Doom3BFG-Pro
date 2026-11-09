#pragma once

#include "tungsten/game/game_runtime.h"
#include "tungsten/game/declvoiceover.h"
#include "engine/animation/md6.h"

#include <array>
#include <string>
#include <vector>

class idFaceMgr;

enum mood_t : int {
  MOOD_NONE = 0,
  MOOD_CLEAR = 1,
  MOOD_HAPPY = 2,
  MOOD_SAD = 3,
  MOOD_ANGRY = 4,
  MOOD_SUSPICIOUS = 5,
  MOOD_DRUNK = 6,
  MOOD_SLEEPY = 7,
  MOOD_NERVOUS = 8,
  MOOD_DEAD = 9,
  MOOD_MAX = 10,
};

class idFaceAnimator {
public:
  idFaceAnimator();
  virtual ~idFaceAnimator();
  void Shutdown(idEntity* owner);
  void PlayWebAnimation(const std::string& animation, int blendMs = 0);
  bool OwnsAnimator(const void* animator) const;
  void SetEnableAnimation(idEntity* owner, bool enable);
  void SetEnableGestures(idEntity* owner, bool enable);
  void Update(idFaceMgr* manager, idEntity* owner, void* treeAnimator);
  void StartLipSync(idFaceMgr* manager, idEntity* owner, void* treeAnimator);
  void AnimateMood(idFaceMgr* manager, idEntity* owner, void* treeAnimator);
  void Reset(idFaceMgr* manager, idEntity* owner, void* treeAnimator);
  void LipSyncFinished(idFaceMgr* manager, idEntity* owner, void* treeAnimator);
  void PlayAnimation(const std::string& animation, float weight = 1.0F);
  void AnimateEyes(const idVec3& focusPoint);
  void Blink(idFaceMgr* manager, idEntity* owner, void* treeAnimator);
  void Glance(idFaceMgr* manager, idEntity* owner, void* treeAnimator, int index);
  void Fidget(idFaceMgr* manager, idEntity* owner, void* treeAnimator, int index);
  bool Init(idFaceMgr* manager, idEntity* owner, void* treeAnimator);

  void SetOwnedAnimator(const void* value) { ownedAnimator = value; }
  bool IsAnimationEnabled() const { return animationEnabled; }
  bool AreGesturesEnabled() const { return gesturesEnabled; }
  bool IsLipSyncPlaying() const { return lipSyncPlaying; }
  int GetBlinkCount() const { return blinkCount; }
  int GetUpdateCount() const { return updateCount; }
  const std::string& GetCurrentAnimation() const { return currentAnimation; }
  const idVec3& GetEyeFocus() const { return eyeFocus; }

private:
  const void* ownedAnimator{nullptr};
  std::string currentAnimation;
  idVec3 eyeFocus{};
  float currentWeight{0.0F};
  int blendTimeMs{0};
  int blinkCount{0};
  int updateCount{0};
  bool initialized{false};
  bool animationEnabled{true};
  bool gesturesEnabled{true};
  bool lipSyncPlaying{false};
};

class idFaceMgr {
public:
  idFaceMgr();
  virtual ~idFaceMgr();

  void SetEnableEyeTracking(idEntity* owner, bool enable);
  void SetEnableFaceSetupEyeTrackingForAI(idEntity* owner, bool enable) {
    (void)owner;
    faceSetupEyeTrackingEnabled = enable;
  }
  void SetMood(mood_t value) { currentMood = value; }
  mood_t GetMoodForTest() const { return currentMood; }
  mood_t GetDefaultMood() const { return defaultMood; }
  void SetDefaultMoodForTest(mood_t value) {
    defaultMood = value;
    currentMood = value;
  }
  void SuppressBlinking(bool suppress);
  void SuppressLidDeformation(bool suppress);
  void Shutdown(idEntity* owner);
  void DormantEnd(idEntity* owner, int timeDormant);
  void SetEnableAnimation(idEntity* owner, bool enable);
  void SetEnableMoodAnimation(idEntity* owner, bool enable);
  void SetEnableLipSyncAnimation(idEntity* owner, bool enable);
  void SetEnableGestureAnimation(idEntity* owner, bool enable);
  void SetEnableBlinking(idEntity* owner, bool enable);
  void SetEnableLidDeformation(idEntity* owner, bool enable);
  void StartLipsync();
  idVec3 CalcEyeFocusPoint(const idVec3& eyeOrigin,
                           const idVec3& requestedFocus) const;
  idVec3 AlignEye(const idVec3& eyeOrigin, const idVec3& focusPoint,
                  bool leftEye) const;
  void CloseEyes(idEntity* owner);
  int GetVoiceTrackTime() const;
  bool LipSyncFinished() const;
  void LipSyncFinished(idEntity* owner, void* treeAnimator);
  void StopLipsync(idEntity* owner, void* treeAnimator);
  void ReallyStartVoiceOver(idEntity* owner, void* treeAnimator);
  void Reset(idEntity* owner, void* treeAnimator);
  bool RecentlyPlayedVoiceOver(int thresholdMs) const;
  bool StartVOIfStreamedSampleIsReady(idEntity* owner, void* treeAnimator);
  int SelectNewGlanceIndex(idEntity* owner, void* treeAnimator);
  int SelectNewFidgetIndex(idEntity* owner, void* treeAnimator);
  void UpdateLipSync(idEntity* owner, void* treeAnimator);
  void Update(idEntity* owner, void* treeAnimator, idVec3* focusPoint);
  void LoadVisemeSetAnims(const std::vector<std::string>& animations);
  bool Init(idEntity* owner, void* treeAnimator);
  bool StartVoiceTrack(idEntity* owner, void* treeAnimator,
                       const idDeclVoiceOver* voiceOver, int trackIndex);
  bool StartVoiceOver(idEntity* owner, void* treeAnimator,
                      const idDeclVoiceOver* voiceOver);

  idFaceAnimator& GetAnimator() { return animator; }
  bool IsEyeTrackingEnabled() const { return eyeTrackingEnabled; }
  bool IsFaceSetupEyeTrackingEnabledForTest() const {
    return faceSetupEyeTrackingEnabled;
  }
  bool IsBlinkingEnabled() const { return blinkingEnabled && !blinkingSuppressed; }
  bool IsLidDeformationEnabled() const {
    return lidDeformationEnabled && !lidDeformationSuppressed;
  }
  bool IsVoicePlaying() const { return voicePlaying; }
  bool IsMoodAnimationEnabled() const { return moodAnimationEnabled; }
  bool IsLipSyncAnimationEnabled() const { return lipSyncAnimationEnabled; }
  bool IsGestureAnimationEnabled() const { return gestureAnimationEnabled; }
  int GetSetBlinkingCountForTest() const { return setBlinkingCount; }
  int GetSetEyeTrackingCountForTest() const { return setEyeTrackingCount; }
  int GetSetMoodAnimationCountForTest() const {
    return setMoodAnimationCount;
  }
  int GetUpdateCountForTest() const { return updateCount; }
  int GetGlanceIndex() const { return glanceIndex; }
  int GetFidgetIndex() const { return fidgetIndex; }
  const std::vector<std::string>& GetVisemes() const { return visemeAnimations; }
  short GetEyeJointIndexForAI(int eye) const {
    return eye >= 0 && eye < static_cast<int>(eyeJointIndex.size())
               ? eyeJointIndex[static_cast<std::size_t>(eye)].value
               : static_cast<short>(-1);
  }
  void SetEyeJointIndicesForTest(short leftEye, short rightEye) {
    eyeJointIndex[0] = idIndex<short, invalidJointIndex_t>(leftEye);
    eyeJointIndex[1] = idIndex<short, invalidJointIndex_t>(rightEye);
  }

private:
  int Now() const;

  idFaceAnimator animator;
  std::vector<std::string> visemeAnimations;
  const idDeclVoiceOver* activeVoiceOver{nullptr};
  int activeTrack{-1};
  int voiceStartTime{0};
  int voiceEndTime{0};
  int lastVoiceEndTime{-1000000};
  int glanceIndex{-1};
  int fidgetIndex{-1};
  int updateCount{0};
  bool initialized{false};
  bool animationEnabled{true};
  bool moodAnimationEnabled{true};
  bool lipSyncAnimationEnabled{true};
  bool gestureAnimationEnabled{true};
  bool eyeTrackingEnabled{true};
  bool faceSetupEyeTrackingEnabled{true};
  bool blinkingEnabled{true};
  bool lidDeformationEnabled{true};
  bool blinkingSuppressed{false};
  bool lidDeformationSuppressed{false};
  bool voicePlaying{false};
  bool eyesClosed{false};
  int setBlinkingCount{0};
  int setEyeTrackingCount{0};
  int setMoodAnimationCount{0};
  mood_t currentMood{MOOD_NONE};
  mood_t defaultMood{MOOD_NONE};
  std::array<idIndex<short, invalidJointIndex_t>, 2> eyeJointIndex{};
};

#define FACE_CVAR_LIST(X) \
  X(face_lipsyncWeight) X(face_blinkScale) X(face_eyeJitter) \
  X(face_jitterMinDist) X(face_eyeDampen) X(face_eyeSpreadScale) \
  X(face_skipLidDeform) X(face_skipLipsync) X(face_skipBlink) \
  X(face_skipMood) X(face_skipHead) X(face_skipGestures) \
  X(face_skipProcedural) X(face_debugWeights) X(face_debug) \
  X(face_debugEyes) X(face_eyeZOffset) X(face_voStreamDelay) \
  X(face_debugTextEvent) X(face_mapWeightsToSine) X(face_syncToGameTime) \
  X(face_syncToRealTime) X(face_debugTime) X(face_serialize) \
  X(face_enableGlancing) X(face_enableFidgeting) X(face_debugFluff) \
  X(face_glanceIntervalMin) X(face_glanceIntervalMax) \
  X(face_fidgetIntervalMin) X(face_fidgetIntervalMax) \
  X(face_fluffMinIntervalMin) X(face_fluffMinIntervalMax)

#define DECLARE_FACE_CVAR(name) extern idCVar name;
FACE_CVAR_LIST(DECLARE_FACE_CVAR)
#undef DECLARE_FACE_CVAR
