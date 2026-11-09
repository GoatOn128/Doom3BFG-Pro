#include "engine/sys/win32/win_cpu.h"
#include "engine/sys/win32/win_local.h"

#include "shared/idlib/csystems/cvarsystemlocal.h"
#include "shared/idlib/lib.h"

#include <intrin.h>
#include <windows.h>
#include <xmmintrin.h>

#include <algorithm>
#include <cstring>
#include <sstream>
#include <string>
#include <vector>

int Sys_GetRamMegabytes();

idCVar sys_cpustring("sys_cpustring", "detect", 0x4000, "CPU feature string");

namespace {
cpuid_t detectedCpu = CPUID_NONE;
}

unsigned CountSetBits(std::uint64_t bitMask) {
  unsigned count = 0;
  while (bitMask != 0) {
    count += static_cast<unsigned>(bitMask & 1U);
    bitMask >>= 1U;
  }
  return count;
}

bool GetCPUInfo(cpuInfo_t* cpuInfo) {
  if (cpuInfo == nullptr) return false;
  *cpuInfo = {};
  DWORD length = 0;
  GetLogicalProcessorInformation(nullptr, &length);
  if (GetLastError() != ERROR_INSUFFICIENT_BUFFER || length == 0) return false;
  std::vector<unsigned char> storage(length);
  auto* records = reinterpret_cast<SYSTEM_LOGICAL_PROCESSOR_INFORMATION*>(storage.data());
  if (!GetLogicalProcessorInformation(records, &length)) return false;
  const DWORD count = length / sizeof(*records);
  for (DWORD i = 0; i < count; ++i) {
    const auto& record = records[i];
    switch (record.Relationship) {
      case RelationProcessorCore:
        ++cpuInfo->processorCoreCount;
        cpuInfo->logicalProcessorCount += CountSetBits(record.ProcessorMask);
        break;
      case RelationNumaNode: ++cpuInfo->numaNodeCount; break;
      case RelationCache: {
        const unsigned level = record.Cache.Level;
        if (level >= 1 && level <= 3) {
          cpuCacheInfo_t& cache = cpuInfo->cacheLevel[level - 1];
          ++cache.count;
          cache.associativity = record.Cache.Associativity;
          cache.lineSize = record.Cache.LineSize;
          cache.size = static_cast<int>(record.Cache.Size);
        }
        break;
      }
      case RelationProcessorPackage: ++cpuInfo->processorPackageCount; break;
      default: break;
    }
  }
  return true;
}

void Sys_CPUCount(int* logical, int* physical, int* packages) {
  cpuInfo_t info{};
  GetCPUInfo(&info);
  if (logical != nullptr) *logical = info.logicalProcessorCount;
  if (physical != nullptr) *physical = info.processorCoreCount;
  if (packages != nullptr) *packages = info.processorPackageCount;
}

cpuid_t Sys_GetCPUId() {
  int registers[4]{};
  __cpuid(registers, 0);
  char vendor[13]{};
  std::memcpy(vendor + 0, &registers[1], 4);
  std::memcpy(vendor + 4, &registers[3], 4);
  std::memcpy(vendor + 8, &registers[2], 4);
  unsigned flags = CPUID_GENERIC;
  if (std::strcmp(vendor, "AuthenticAMD") == 0) flags = CPUID_AMD;
  else if (std::strcmp(vendor, "GenuineIntel") == 0) flags = CPUID_INTEL;

  __cpuid(registers, 1);
  const unsigned ecx = static_cast<unsigned>(registers[2]);
  const unsigned edx = static_cast<unsigned>(registers[3]);
  if ((edx & (1U << 15U)) != 0) flags |= CPUID_CMOV;
  if ((edx & (1U << 23U)) != 0) flags |= CPUID_MMX;
  if ((edx & (1U << 25U)) != 0) flags |= CPUID_SSE;
  if ((edx & (1U << 26U)) != 0) flags |= CPUID_SSE2;
  if ((ecx & (1U << 0U)) != 0) flags |= CPUID_SSE3;
  if ((ecx & (1U << 9U)) != 0) flags |= CPUID_SSSE3;
  if ((ecx & (1U << 19U)) != 0) flags |= CPUID_SSE41;
  if ((ecx & (1U << 20U)) != 0) flags |= CPUID_SSE42;
  if ((ecx & (1U << 28U)) != 0) flags |= CPUID_AVX;
  __cpuid(registers, static_cast<int>(0x80000001U));
  if ((static_cast<unsigned>(registers[3]) & (1U << 29U)) != 0) flags |= CPUID_EM64T;
  cpuInfo_t info{};
  if (GetCPUInfo(&info) && info.logicalProcessorCount > info.processorCoreCount) flags |= CPUID_HTT;
  return static_cast<cpuid_t>(flags);
}

cpuid_t Sys_GetProcessorId() {
  if (win32.cpuid == CPUID_NONE) {
    detectedCpu = Sys_GetCPUId();
    win32.cpuid = static_cast<std::uint32_t>(detectedCpu);
  }
  return static_cast<cpuid_t>(win32.cpuid);
}

const char* Sys_FPU_GetState() { return ""; }

void Sys_FPU_EnableExceptions(int exceptions) {
  unsigned control = 0x8001fU;
  if ((exceptions & 1) != 0) control &= ~0x10U;
  if ((exceptions & 2) != 0) control &= ~0x80000U;
  if ((exceptions & 4) != 0) control &= ~0x8U;
  if ((exceptions & 8) != 0) control &= ~0x4U;
  if ((exceptions & 16) != 0) control &= ~0x2U;
  if ((exceptions & 32) != 0) control &= ~0x1U;
  unsigned previous = 0;
  _controlfp_s(&previous, control, 0x8001fU);
}

void Sys_FPU_SetDAZ(bool enable) {
  _mm_setcsr(enable ? (_mm_getcsr() | 0x40U) : (_mm_getcsr() & ~0x40U));
}

void Sys_FPU_SetFTZ(bool enable) {
  _mm_setcsr(enable ? (_mm_getcsr() | 0x8000U) : (_mm_getcsr() & ~0x8000U));
}

const char* Sys_GetProcessorString() { return sys_cpustring.GetString(); }

void Sys_DetectCPU() {
  win32.osVersion = {};
  win32.osVersion.dwOSVersionInfoSize = sizeof(win32.osVersion);
#pragma warning(push)
#pragma warning(disable : 4996)
  if (!GetVersionExA(&win32.osVersion)) idLib::Error("Couldn't get OS info");
#pragma warning(pop)
  if (win32.osVersion.dwMajorVersion < 4)
    idLib::Error("Wolfenstein The New Order requires Windows version 4 or greater");
  if (win32.osVersion.dwPlatformId == VER_PLATFORM_WIN32s)
    idLib::Error("Wolfenstein The New Order doesn't run on Win32s");
  if (idStr::Icmp(sys_cpustring.GetString(), "detect") != 0) {
    unsigned forced = CPUID_NONE;
    std::istringstream tokens(sys_cpustring.GetString());
    std::string token;
    while (tokens >> token) {
      if (_stricmp(token.c_str(), "generic") == 0) forced |= CPUID_GENERIC;
      else if (_stricmp(token.c_str(), "intel") == 0) forced |= CPUID_INTEL;
      else if (_stricmp(token.c_str(), "amd") == 0) forced |= CPUID_AMD;
      else if (_stricmp(token.c_str(), "mmx") == 0) forced |= CPUID_MMX;
      else if (_stricmp(token.c_str(), "sse") == 0) forced |= CPUID_SSE;
      else if (_stricmp(token.c_str(), "sse2") == 0) forced |= CPUID_SSE2;
      else if (_stricmp(token.c_str(), "sse3") == 0) forced |= CPUID_SSE3;
      else if (_stricmp(token.c_str(), "htt") == 0) forced |= CPUID_HTT;
    }
    detectedCpu = static_cast<cpuid_t>(forced != 0 ? forced : CPUID_GENERIC);
    win32.cpuid = static_cast<std::uint32_t>(detectedCpu);
    return;
  }
  cpuInfo_t info{};
  GetCPUInfo(&info);
  detectedCpu = Sys_GetCPUId();
  win32.cpuid = static_cast<std::uint32_t>(detectedCpu);
  idStr description;
  if ((detectedCpu & CPUID_INTEL) != 0) description = "Intel CPU";
  else if ((detectedCpu & CPUID_AMD) != 0) description = "AMD CPU";
  else description = (detectedCpu & CPUID_UNSUPPORTED) != 0 ? "unsupported CPU" : "generic CPU";
  description += " with ";
  const struct { unsigned bit; const char* name; } features[] = {
      {CPUID_MMX, "MMX & "}, {CPUID_SSE, "SSE & "}, {CPUID_SSE2, "SSE2 & "},
      {CPUID_SSE3, "SSE3 & "}, {CPUID_SSSE3, "SSSE3 & "}, {CPUID_SSE41, "SSE41 & "},
      {CPUID_SSE42, "SSE42 & "}, {CPUID_AVX, "AVX & "}, {CPUID_HTT, "HTT & "}};
  for (const auto& feature : features) if ((detectedCpu & feature.bit) != 0) description += feature.name;
  description.StripTrailing(" & ");
  description.StripTrailing(" with ");
  sys_cpustring.SetString(description.data);
  idLib::Printf("%d CPU package%s, %d physical core%s, %d logical core%s\n",
                info.processorPackageCount, info.processorPackageCount == 1 ? "" : "s",
                info.processorCoreCount, info.processorCoreCount == 1 ? "" : "s",
                info.logicalProcessorCount, info.logicalProcessorCount == 1 ? "" : "s");
  idLib::Printf("%s\n", sys_cpustring.GetString());
  idLib::Printf("%d kB 1st level cache, %d kB 2nd level cache, %d kB 3rd level cache\n",
                info.cacheLevel[0].size / 1024, info.cacheLevel[1].size / 1024,
                info.cacheLevel[2].size / 1024);
  idLib::Printf("%d MB System Memory\n", Sys_GetRamMegabytes());
}
