#include "tungsten/game/ai/aidefs.h"

#include "tungsten/game/game_runtime.h"

namespace {

std::uint32_t randomSeed = 0;
int gameMilliseconds = 0;

std::uint32_t NextRandomSample() {
  randomSeed = 1664525U * randomSeed + 1013904223U;
  return (randomSeed >> 10U) & 0x7FFFU;
}

float NextRandomUnit() {
  return static_cast<float>(NextRandomSample()) * 0.000030518509F;
}

int GameMillisecondsPerRealSecond() {
  return gameLocal != nullptr ? gameLocal->GetGameMsPerRealSec() : 1000;
}

}  // namespace

void SetAIDefsRandomSeed(std::uint32_t seed) { randomSeed = seed; }

std::uint32_t GetAIDefsRandomSeed() { return randomSeed; }

void SetAIDefsGameMilliseconds(int milliseconds) {
  gameMilliseconds = milliseconds;
}

// DIA: GetEndTimeByFrames
int GetEndTimeByFrames(int frameCount) {
  if (frameCount <= 0) return 0;
  return gameMilliseconds +
         frameCount * GameMillisecondsPerRealSecond() / 30;
}

// DIA: GetRandomInterval
unsigned int GetRandomInterval(unsigned int minimum, int maximum) {
  if (maximum <= static_cast<int>(minimum)) return minimum;
  return minimum + NextRandomSample() %
                       static_cast<unsigned int>(maximum - minimum);
}

// DIA: GetRandomInterval
float GetRandomInterval(float minimum, float maximum) {
  if (minimum >= maximum) return minimum;
  return minimum + NextRandomUnit() * (maximum - minimum);
}

// DIA: GetRandomIntervalBySeconds
int GetRandomIntervalBySeconds(float minimumSeconds, float maximumSeconds) {
  return static_cast<int>(static_cast<float>(GameMillisecondsPerRealSecond()) *
                          (minimumSeconds +
                           NextRandomUnit() *
                               (maximumSeconds - minimumSeconds)));
}

// DIA: RandomInt
int RandomInt(int maximum) {
  if (maximum <= 0) return 0;
  return static_cast<int>(NextRandomSample() %
                          static_cast<std::uint32_t>(maximum));
}

// DIA: RandomInt
unsigned int RandomInt(unsigned int minimum, int maximum) {
  if (static_cast<int>(minimum) >= maximum) return minimum;
  return minimum + NextRandomSample() %
                       static_cast<unsigned int>(maximum - minimum + 1);
}

// DIA: RandomFloat
float RandomFloat() { return NextRandomUnit(); }

// DIA: RandomFloat
float RandomFloat(float maximum) { return NextRandomUnit() * maximum; }

// DIA: RandomFloat
float RandomFloat(float minimum, float maximum) {
  return minimum + NextRandomUnit() * (maximum - minimum);
}

// DIA: SecondsToTicks
idGameTimeTicks SecondsToTicks(float seconds) {
  return {static_cast<int>(static_cast<float>(
                               GameMillisecondsPerRealSecond()) *
                           seconds)};
}
