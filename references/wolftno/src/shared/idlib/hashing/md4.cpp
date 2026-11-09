#include "md.h"

#include <cstring>

namespace {
constexpr std::uint8_t kPadding[64] = {0x80};
std::uint32_t Rol(std::uint32_t v, unsigned s) {
  return (v << s) | (v >> (32 - s));
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
constexpr std::uint32_t F(std::uint32_t x, std::uint32_t y, std::uint32_t z) {
  return (x & y) | (~x & z);
}
constexpr std::uint32_t G(std::uint32_t x, std::uint32_t y, std::uint32_t z) {
  return (x & y) | (x & z) | (y & z);
}
constexpr std::uint32_t H(std::uint32_t x, std::uint32_t y, std::uint32_t z) {
  return x ^ y ^ z;
}
} // namespace

void MD4_Transform(std::uint32_t* state, const std::uint8_t* block) {
  std::uint32_t x[16];
  for (int i = 0; i < 16; ++i)
    x[i] = Load(block + i * 4);
  std::uint32_t a = state[0], b = state[1], c = state[2], d = state[3];
#define R1(a, b, c, d, k, s) a = Rol(a + F(b, c, d) + x[k], s)
#define R2(a, b, c, d, k, s) a = Rol(a + G(b, c, d) + x[k] + 0x5A827999u, s)
#define R3(a, b, c, d, k, s) a = Rol(a + H(b, c, d) + x[k] + 0x6ED9EBA1u, s)
  R1(a, b, c, d, 0, 3);
  R1(d, a, b, c, 1, 7);
  R1(c, d, a, b, 2, 11);
  R1(b, c, d, a, 3, 19);
  R1(a, b, c, d, 4, 3);
  R1(d, a, b, c, 5, 7);
  R1(c, d, a, b, 6, 11);
  R1(b, c, d, a, 7, 19);
  R1(a, b, c, d, 8, 3);
  R1(d, a, b, c, 9, 7);
  R1(c, d, a, b, 10, 11);
  R1(b, c, d, a, 11, 19);
  R1(a, b, c, d, 12, 3);
  R1(d, a, b, c, 13, 7);
  R1(c, d, a, b, 14, 11);
  R1(b, c, d, a, 15, 19);
  R2(a, b, c, d, 0, 3);
  R2(d, a, b, c, 4, 5);
  R2(c, d, a, b, 8, 9);
  R2(b, c, d, a, 12, 13);
  R2(a, b, c, d, 1, 3);
  R2(d, a, b, c, 5, 5);
  R2(c, d, a, b, 9, 9);
  R2(b, c, d, a, 13, 13);
  R2(a, b, c, d, 2, 3);
  R2(d, a, b, c, 6, 5);
  R2(c, d, a, b, 10, 9);
  R2(b, c, d, a, 14, 13);
  R2(a, b, c, d, 3, 3);
  R2(d, a, b, c, 7, 5);
  R2(c, d, a, b, 11, 9);
  R2(b, c, d, a, 15, 13);
  R3(a, b, c, d, 0, 3);
  R3(d, a, b, c, 8, 9);
  R3(c, d, a, b, 4, 11);
  R3(b, c, d, a, 12, 15);
  R3(a, b, c, d, 2, 3);
  R3(d, a, b, c, 10, 9);
  R3(c, d, a, b, 6, 11);
  R3(b, c, d, a, 14, 15);
  R3(a, b, c, d, 1, 3);
  R3(d, a, b, c, 9, 9);
  R3(c, d, a, b, 5, 11);
  R3(b, c, d, a, 13, 15);
  R3(a, b, c, d, 3, 3);
  R3(d, a, b, c, 11, 9);
  R3(c, d, a, b, 7, 11);
  R3(b, c, d, a, 15, 15);
#undef R1
#undef R2
#undef R3
  state[0] += a;
  state[1] += b;
  state[2] += c;
  state[3] += d;
}

void MD4_Update(MD4_CTX* ctx, const std::uint8_t* input, std::uint32_t length) {
  const std::uint32_t old = ctx->count[0];
  ctx->count[0] += length << 3;
  if (ctx->count[0] < old)
    ++ctx->count[1];
  ctx->count[1] += length >> 29;
  const std::size_t index = (old >> 3) & 63;
  const std::size_t space = 64 - index;
  std::size_t used = 0;
  if (length >= space) {
    std::memmove(ctx->buffer + index, input, space);
    MD4_Transform(ctx->state, ctx->buffer);
    used = space;
    while (used + 63 < length) {
      MD4_Transform(ctx->state, input + used);
      used += 64;
    }
  }
  std::memmove(ctx->buffer + (used ? 0 : index), input + used, length - used);
}

void MD4_Final(MD4_CTX* ctx, std::uint8_t* digest) {
  std::uint8_t bits[8];
  Store(bits, ctx->count[0]);
  Store(bits + 4, ctx->count[1]);
  const std::size_t index = (ctx->count[0] >> 3) & 63;
  MD4_Update(ctx, kPadding, std::uint32_t(index < 56 ? 56 - index : 120 - index));
  MD4_Update(ctx, bits, 8);
  for (int i = 0; i < 4; ++i)
    Store(digest + i * 4, ctx->state[i]);
  std::memset(ctx, 0, sizeof(*ctx));
}

std::uint32_t MD4_BlockChecksum(const std::uint8_t* data, std::uint32_t length) {
  MD4_CTX ctx{};
  ctx.state[0] = 0x67452301u;
  ctx.state[1] = 0xEFCDAB89u;
  ctx.state[2] = 0x98BADCFEu;
  ctx.state[3] = 0x10325476u;
  MD4_Update(&ctx, data, length);
  std::uint8_t digest[16];
  MD4_Final(&ctx, digest);
  return Load(digest) ^ Load(digest + 4) ^ Load(digest + 8) ^ Load(digest + 12);
}
