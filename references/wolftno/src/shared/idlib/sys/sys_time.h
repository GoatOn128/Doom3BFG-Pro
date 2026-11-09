#pragma once

#include "shared/idlib/text/str.h"

#include <cstdint>

std::uint64_t Sys_CurrentSystemTime();
std::uint64_t Sys_TimeSinceUnixEpoch();
int Sys_NumLanguageIds();
const char* Sys_GetLanguageName(unsigned int id);
int Sys_GetLanguageId(const char* language);
char* Sys_TimeStampToLogFormat(int timeStamp);
char* Sys_TimeStampToStr(int timeStamp, bool padded);
char* Sys_DateStr(bool padded);
idStr Sys_SecToStr(int seconds);

void Sys_Sleep(unsigned long milliseconds);
std::int64_t Sys_Milliseconds();
std::int64_t Sys_GetClockTicks();
std::int64_t Sys_ClockTicksPerSecond();
std::int64_t Sys_ClockCyclesPerSecond();
std::uint64_t Sys_Microseconds();
void Sys_InitClockTicks();
