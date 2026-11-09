#pragma once

#include <cstdint>

void CRC16_InitChecksum(std::uint16_t* crcValue);
void CRC16_UpdateChecksum(std::uint16_t* crcValue, std::uint8_t* data, int length);
void CRC32_InitChecksum(std::uint32_t* crcValue);
void CRC32_UpdateChecksum(std::uint32_t* crcValue, char* data, int length);
void CRC32_FinishChecksum(std::uint32_t* crcValue);
std::uint32_t CRC32_BlockChecksum(char* data, int length);
std::uint32_t MurMur32_HashData(std::uint8_t* key, std::uint32_t length, std::uint32_t seed);
std::uint64_t MurMur64_HashData(int* key, std::uint32_t length, std::uint32_t seed);
