#pragma once

#include "engine/sys/sys_public.h"

#include <cstdint>

struct cpuCacheInfo_t {
  int count{};
  int associativity{};
  int lineSize{};
  int size{};
};

struct cpuInfo_t {
  int processorPackageCount{};
  int processorCoreCount{};
  int logicalProcessorCount{};
  int numaNodeCount{};
  cpuCacheInfo_t cacheLevel[3]{};
};

unsigned CountSetBits(std::uint64_t bitMask);
bool GetCPUInfo(cpuInfo_t* cpuInfo);
void Sys_CPUCount(int* numLogicalCPUCores, int* numPhysicalCPUCores, int* numCPUPackages);
cpuid_t Sys_GetCPUId();
cpuid_t Sys_GetProcessorId();
const char* Sys_FPU_GetState();
void Sys_FPU_EnableExceptions(int exceptions);
void Sys_FPU_SetDAZ(bool enable);
void Sys_FPU_SetFTZ(bool enable);
const char* Sys_GetProcessorString();
void Sys_DetectCPU();
