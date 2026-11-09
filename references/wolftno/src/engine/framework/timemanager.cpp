#include "engine/framework/timemanager.h"

void idTimeManager::SetGameHz(int hertz) {
  if (hertz > 0) {
    gameHz = hertz;
    inverseGameHz = 1.0f / static_cast<float>(hertz);
    gameTimePerFrame.value =
        static_cast<int>(1000.0f / static_cast<float>(hertz));
    gameTimePerSecond.value = hertz * gameTimePerFrame.value;
    inverseGameTimePerSecond =
        1.0f / static_cast<float>(gameTimePerSecond.value);
  }
}

idTimeManager timeManager(60);

