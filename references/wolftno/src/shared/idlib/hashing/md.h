#pragma once

#include <cstddef>
#include <cstdint>

struct MD4_CTX {
  std::uint32_t state[4];
  std::uint32_t count[2];
  std::uint8_t buffer[64];
};
struct MD5_CTX {
  std::uint32_t state[4];
  std::uint32_t bits[2];
  std::uint8_t in[64];
};
static_assert(sizeof(MD4_CTX) == 88, "MD4_CTX ABI drift");
static_assert(sizeof(MD5_CTX) == 88, "MD5_CTX ABI drift");

void MD4_Transform(std::uint32_t* state, const std::uint8_t* block);
void MD4_Update(MD4_CTX* context, const std::uint8_t* input, std::uint32_t length);
void MD4_Final(MD4_CTX* context, std::uint8_t* digest);
std::uint32_t MD4_BlockChecksum(const std::uint8_t* data, std::uint32_t length);
void MD5_Transform(std::uint32_t* state, const std::uint8_t* block);
void MD5_Init(MD5_CTX* context);
void MD5_Update(MD5_CTX* context, const std::uint8_t* input, std::size_t length);
void MD5_Final(MD5_CTX* context, std::uint8_t* digest);
std::uint32_t MD5_BlockChecksum(const std::uint8_t* data, std::size_t length);
