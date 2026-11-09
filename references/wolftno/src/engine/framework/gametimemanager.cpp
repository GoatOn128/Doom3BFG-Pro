#include "engine/framework/gametimemanager.h"

#include "shared/idlib/csystems/cvarsystemlocal.h"

idCVar g_enableGameTimeScaling(
    "g_enableGameTimeScaling", "1", CVAR_BOOL,
    "Enables or Disables Scaling of Game Time via the GameTimeManager-Turning "
    "this off disables slow motion throughout the game");

template <> float idInterpolate<float>::GetCurrentValue(float valueTime) {
  const float elapsed = valueTime - startTime;
  if (valueTime == currentTime)
    return currentValue;
  currentTime = valueTime;
  if (duration >= 0.0f) {
    if (elapsed <= 0.0f) {
      currentValue = startValue;
      return currentValue;
    }
    if (elapsed < duration) {
      currentValue = (endValue - startValue) * (elapsed / duration) + startValue;
      return currentValue;
    }
  } else {
    if (elapsed >= 0.0f) {
      currentValue = startValue;
      return currentValue;
    }
    if (duration < elapsed) {
      currentValue = (endValue - startValue) * (elapsed / duration) + startValue;
      return currentValue;
    }
  }
  currentValue = endValue;
  return currentValue;
}

idGameTimeManager::idGameTimeManager()
    : framenum(0), hz{}, ticks{}, timeDelta{}, time{}, previousTime{}, scale{},
      timeScaleExtrapolation{}, slowmoParms{}, inSlowMotion(false),
      useInterpolator(false), slowmoDuration(0.0f) {
  slowmoParms.easeInDuration = 0.0f;
  slowmoParms.slowmoDuration = 0.0f;
  slowmoParms.easeOutDuration = 0.0f;
  slowmoParms.timeScale = 1.0f;
  for (int type = 0; type < 2; ++type) {
    hz[type] = timeManager.gameHz;
    ticks[type] = timeManager.gameTimePerFrame.value;
    timeDelta[type] = 1000.0f / static_cast<float>(timeManager.gameHz);
    time[type] = 0;
    previousTime[type] = 0;
    scale[type] = 1.0f;
  }
}

void idGameTimeManager::UpdateGameFrame() { ++framenum; }

void idGameTimeManager::SetGameFrame(const int frame) { framenum = frame; }

void idGameTimeManager::SetTimeScale(const float newScale,
                                     const gameTimeType_t type) {
  useInterpolator = false;
  scale[type] = newScale;
  inSlowMotion = newScale < 1.0f;
}

void idGameTimeManager::UpdateGameMs() {
  previousTime[0] = time[0];
  time[0] += ticks[0];
  previousTime[1] = time[1];
  time[1] += ticks[1];
}

void idGameTimeManager::SetGameHz(const int hertz) {
  timeManager.SetGameHz(hertz);
}

int idGameTimeManager::GetGameMs(gameTimeType_t type) const {
  return g_enableGameTimeScaling.valueInteger != 0 ? time[type] : time[0];
}

int idGameTimeManager::GetPreviousGameMs(gameTimeType_t type) const {
  return g_enableGameTimeScaling.valueInteger != 0 ? previousTime[type]
                                                   : previousTime[0];
}

int idGameTimeManager::GetGameMsPerFrame(gameTimeType_t type) const {
  return g_enableGameTimeScaling.valueInteger != 0 ? ticks[type] : ticks[0];
}

float idGameTimeManager::GetRealMsPerFrame(gameTimeType_t type) const {
  return g_enableGameTimeScaling.valueInteger != 0 ? timeDelta[type]
                                                   : timeDelta[0];
}

int idGameTimeManager::GetGameHz(gameTimeType_t type) const {
  return g_enableGameTimeScaling.valueInteger != 0 ? hz[type] : hz[0];
}

float idGameTimeManager::GetTimeScale(gameTimeType_t type) const {
  return g_enableGameTimeScaling.valueInteger != 0 ? scale[type] : scale[0];
}

void idGameTimeManager::SetGameMs(const int newTime) {
  time[0] = newTime;
  previousTime[0] = newTime - timeManager.gameTimePerFrame.value;
  previousTime[1] = previousTime[0];
  time[1] = time[0];
}

const idTypesafeNumber<int, millisecondUnique_t>
idGameTimeManager::ConvertTimeGameToReal(
    idTypesafeNumber<int, gameTimeUnique_t> gameTicks) const {
  return idTypesafeNumber<int, millisecondUnique_t>(
      1000 * gameTicks.value / timeManager.gameTimePerSecond.value);
}

const idTypesafeNumber<int, gameTimeUnique_t>
idGameTimeManager::ConvertTimeRealToGame(
    idTypesafeNumber<int, millisecondUnique_t> milliseconds) const {
  return idTypesafeNumber<int, gameTimeUnique_t>(
      timeManager.gameTimePerSecond.value * milliseconds.value / 1000);
}

void idGameTimeManager::StartSlowmotion(const slowmoParms_t& parms) {
  if (g_enableGameTimeScaling.valueInteger == 0)
    return;
  inSlowMotion = true;
  useInterpolator = true;
  slowmoParms = parms;
  slowmoDuration = parms.slowmoDuration;
  const float now = static_cast<float>(time[0]);
  const float current = timeScaleExtrapolation[1].GetCurrentValue(now);
  timeScaleExtrapolation[1].Init(
      now, static_cast<float>(static_cast<int>(parms.easeInDuration * 1000.0f)),
      current, parms.timeScale);
}

void idGameTimeManager::StopSlowmotion() {
  if (!inSlowMotion || g_enableGameTimeScaling.valueInteger == 0)
    return;
  useInterpolator = false;
  inSlowMotion = false;
  slowmoDuration = 0.0f;
  scale[1] = 1.0f;
  const float now = static_cast<float>(time[0]);
  const float current = timeScaleExtrapolation[1].GetCurrentValue(now);
  timeScaleExtrapolation[1].Init(
      now,
      static_cast<float>(static_cast<int>(slowmoParms.easeOutDuration * 1000.0f)),
      current, 1.0f);
}

void idGameTimeManager::UpdateTimeScale() {
  hz[0] = timeManager.gameHz;
  ticks[0] = timeManager.gameTimePerFrame.value;
  timeDelta[0] = 1000.0f / static_cast<float>(timeManager.gameHz);
  if (useInterpolator)
    scale[1] = timeScaleExtrapolation[1].GetCurrentValue(
        static_cast<float>(time[0]));
  hz[1] = static_cast<int>(static_cast<float>(hz[0]) * scale[1]);
  ticks[1] = static_cast<int>(static_cast<float>(ticks[0]) * scale[1]);
  timeDelta[1] = 1000.0f / (static_cast<float>(hz[0]) * scale[1]);

  if (slowmoDuration > 0.0f &&
      static_cast<float>(time[0]) >=
          timeScaleExtrapolation[1].startTime +
              timeScaleExtrapolation[1].duration) {
    slowmoDuration -= static_cast<float>(ticks[0]) * 0.001f;
    if (slowmoDuration <= 0.0f) {
      slowmoDuration = 0.0f;
      StopSlowmotion();
    }
  }
}

void idGameTimeManager::Clear() {
  inSlowMotion = false;
  useInterpolator = false;
  slowmoDuration = 0.0f;
  for (int type = 0; type < 2; ++type) {
    hz[type] = timeManager.gameHz;
    ticks[type] = timeManager.gameTimePerFrame.value;
    timeDelta[type] = 1000.0f / static_cast<float>(timeManager.gameHz);
    time[type] = 0;
    previousTime[type] = 0;
    scale[type] = 1.0f;
    timeScaleExtrapolation[type].Init(0.0f, 0.0f, 1.0f, 1.0f);
  }
  UpdateTimeScale();
}
