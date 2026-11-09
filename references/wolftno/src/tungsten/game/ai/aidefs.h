#pragma once

#include <cstdint>

struct idGameTimeTicks {
  int value{0};
};

void SetAIDefsRandomSeed(std::uint32_t seed);
std::uint32_t GetAIDefsRandomSeed();
void SetAIDefsGameMilliseconds(int milliseconds);

int GetEndTimeByFrames(int frameCount);
unsigned int GetRandomInterval(unsigned int minimum, int maximum);
float GetRandomInterval(float minimum, float maximum);
int GetRandomIntervalBySeconds(float minimumSeconds, float maximumSeconds);
int RandomInt(int maximum);
unsigned int RandomInt(unsigned int minimum, int maximum);
float RandomFloat();
float RandomFloat(float maximum);
float RandomFloat(float minimum, float maximum);
idGameTimeTicks SecondsToTicks(float seconds);
