#include "md.h"

#include <cstring>

namespace {
constexpr std::uint8_t kPadding[64] = {0x80};
constexpr std::uint32_t k[64] = {
    0xd76aa478u, 0xe8c7b756u, 0x242070dbu, 0xc1bdceeeu, 0xf57c0fafu, 0x4787c62au, 0xa8304613u,
    0xfd469501u, 0x698098d8u, 0x8b44f7afu, 0xffff5bb1u, 0x895cd7beu, 0x6b901122u, 0xfd987193u,
    0xa679438eu, 0x49b40821u, 0xf61e2562u, 0xc040b340u, 0x265e5a51u, 0xe9b6c7aau, 0xd62f105du,
    0x02441453u, 0xd8a1e681u, 0xe7d3fbc8u, 0x21e1cde6u, 0xc33707d6u, 0xf4d50d87u, 0x455a14edu,
    0xa9e3e905u, 0xfcefa3f8u, 0x676f02d9u, 0x8d2a4c8au, 0xfffa3942u, 0x8771f681u, 0x6d9d6122u,
    0xfde5380cu, 0xa4beea44u, 0x4bdecfa9u, 0xf6bb4b60u, 0xbebfbc70u, 0x289b7ec6u, 0xeaa127fau,
    0xd4ef3085u, 0x04881d05u, 0xd9d4d039u, 0xe6db99e5u, 0x1fa27cf8u, 0xc4ac5665u, 0xf4292244u,
    0x432aff97u, 0xab9423a7u, 0xfc93a039u, 0x655b59c3u, 0x8f0ccc92u, 0xffeff47du, 0x85845dd1u,
    0x6fa87e4fu, 0xfe2ce6e0u, 0xa3014314u, 0x4e0811a1u, 0xf7537e82u, 0xbd3af235u, 0x2ad7d2bbu,
    0xeb86d391u};
constexpr unsigned s[64] = {7, 12, 17, 22, 7, 12, 17, 22, 7, 12, 17, 22, 7, 12, 17, 22,
                            5, 9,  14, 20, 5, 9,  14, 20, 5, 9,  14, 20, 5, 9,  14, 20,
                            4, 11, 16, 23, 4, 11, 16, 23, 4, 11, 16, 23, 4, 11, 16, 23,
                            6, 10, 15, 21, 6, 10, 15, 21, 6, 10, 15, 21, 6, 10, 15, 21};
std::uint32_t Rol(std::uint32_t v, unsigned n) {
  return (v << n) | (v >> (32 - n));
}
std::uint32_t Load(const std::uint8_t* p) {
  return p[0] | (std::uint32_t(p[1]) << 8) | (std::uint32_t(p[2]) << 16) |
         (std::uint32_t(p[3]) << 24);
}
void Store(std::uint8_t* p, std::uint32_t v) {
  p[0] = std::uint8_t(v);
  p[1] = std::uint8_t(v >> 8);
  p[2] = std::uint8_t(v >> 16);
  p[3] = std::uint8_t(v >> 24);
}
} // namespace

void MD5_Transform(std::uint32_t* state, const std::uint8_t* block) {
  std::uint32_t x[16];
  for (int i = 0; i < 16; ++i)
    x[i] = Load(block + i * 4);
  std::uint32_t a = state[0], b = state[1], c = state[2], d = state[3];
  for (unsigned i = 0; i < 64; ++i) {
    std::uint32_t f;
    unsigned g;
    if (i < 16) {
      f = (b & c) | (~b & d);
      g = i;
    } else if (i < 32) {
      f = (d & b) | (~d & c);
      g = (5 * i + 1) & 15;
    } else if (i < 48) {
      f = b ^ c ^ d;
      g = (3 * i + 5) & 15;
    } else {
      f = c ^ (b | ~d);
      g = (7 * i) & 15;
    }
    const auto next = d;
    d = c;
    c = b;
    b += Rol(a + f + k[i] + x[g], s[i]);
    a = next;
  }
  state[0] += a;
  state[1] += b;
  state[2] += c;
  state[3] += d;
}

void MD5_Init(MD5_CTX* ctx) {
  ctx->state[0] = 0x67452301u;
  ctx->state[1] = 0xEFCDAB89u;
  ctx->state[2] = 0x98BADCFEu;
  ctx->state[3] = 0x10325476u;
  ctx->bits[0] = ctx->bits[1] = 0;
}
void MD5_Update(MD5_CTX* ctx, const std::uint8_t* input, std::size_t length) {
  const std::uint32_t old = ctx->bits[0];
  ctx->bits[0] += std::uint32_t(length << 3);
  if (ctx->bits[0] < old)
    ++ctx->bits[1];
  ctx->bits[1] += std::uint32_t(length >> 29);
  const std::size_t index = (old >> 3) & 63, space = 64 - index;
  std::size_t used = 0;
  if (length >= space) {
    std::memmove(ctx->in + index, input, space);
    MD5_Transform(ctx->state, ctx->in);
    used = space;
    while (used + 63 < length) {
      MD5_Transform(ctx->state, input + used);
      used += 64;
    }
  }
  std::memmove(ctx->in + (used ? 0 : index), input + used, length - used);
}
void MD5_Final(MD5_CTX* ctx, std::uint8_t* digest) {
  std::uint8_t bits[8];
  Store(bits, ctx->bits[0]);
  Store(bits + 4, ctx->bits[1]);
  const std::size_t index = (ctx->bits[0] >> 3) & 63;
  MD5_Update(ctx, kPadding, index < 56 ? 56 - index : 120 - index);
  MD5_Update(ctx, bits, 8);
  for (int i = 0; i < 4; ++i)
    Store(digest + i * 4, ctx->state[i]);
  std::memset(ctx, 0, sizeof(*ctx));
}
std::uint32_t MD5_BlockChecksum(const std::uint8_t* data, std::size_t length) {
  MD5_CTX ctx;
  MD5_Init(&ctx);
  MD5_Update(&ctx, data, length);
  std::uint8_t digest[16];
  MD5_Final(&ctx, digest);
  return Load(digest) ^ Load(digest + 4) ^ Load(digest + 8) ^ Load(digest + 12);
}
