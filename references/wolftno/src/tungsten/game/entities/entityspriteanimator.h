#pragma once

#include "tungsten/game/game_runtime.h"

enum spriteAnim_t : int {
  SPRITEANIM_IDLE = 0,
  SPRITEANIM_WALK = 1,
  SPRITEANIM_DIE = 2,
};

class idEntitySpriteAnimator : public idEntity {
public:
  static idEntitySpriteAnimator* CastTo(idEntity* entity) {
    return dynamic_cast<idEntitySpriteAnimator*>(entity);
  }
  static const idEntitySpriteAnimator* CastTo(const idEntity* entity) {
    return dynamic_cast<const idEntitySpriteAnimator*>(entity);
  }

  bool IsDead() const { return currentState == SPRITEANIM_DIE; }
  void PlaySpriteAnim(spriteAnim_t state) {
    currentState = state;
    ++playAnimationCount;
  }
  void PostSpriteAnim(bool moving) {
    lastPostedMoving = moving;
    ++postSpriteAnimCount;
  }
  void SetCurrentStateForTest(spriteAnim_t state) { currentState = state; }
  spriteAnim_t GetCurrentStateForTest() const { return currentState; }
  int GetPlayAnimationCountForTest() const { return playAnimationCount; }
  int GetPostSpriteAnimCountForTest() const { return postSpriteAnimCount; }
  bool GetLastPostedMovingForTest() const { return lastPostedMoving; }

private:
  spriteAnim_t currentState{SPRITEANIM_IDLE};
  int playAnimationCount{0};
  int postSpriteAnimCount{0};
  bool lastPostedMoving{false};
};
