#include "tungsten/game/ai/aitest.h"

idCVar aitest_freezeTracker("aitest_freezeTracker", "0", 1,
                            "for testing ai rotation trackers");

// DIA: idAITest_Rotation::idAITest_Rotation
idAITest_Rotation::idAITest_Rotation() = default;

// DIA: idAITest_Rotation::`scalar deleting destructor'
idAITest_Rotation::~idAITest_Rotation() = default;

// DIA: idAITest_Rotation::Think
void idAITest_Rotation::Think() {
  if (gameLocal == nullptr) return;
  idEntity* debugPlayer = gameLocal->GetDebugPlayer();
  if (debugPlayer == nullptr || GetPhysics() == nullptr ||
      debugPlayer->GetPhysics() == nullptr) {
    return;
  }

  idVec3 direction = lastDir;
  if (aitest_freezeTracker.GetInteger() == 0) {
    direction = debugPlayer->GetPhysics()->GetOrigin() -
                GetPhysics()->GetOrigin();
    if (direction.Normalize() == 0.0F) direction = lastDir;
  }

  tracker.Update(lastDir, direction, rate,
                 static_cast<float>(gameLocal->GetGameMsPerFrame()) * 0.001F,
                 true, true);
  lastDir = tracker.GetTrackedDirection();
}

// DIA: `dynamic initializer for 'aitest_freezeTracker''

// DIA: `dynamic atexit destructor for 'aitest_freezeTracker''
