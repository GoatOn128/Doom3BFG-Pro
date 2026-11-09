#pragma once

#include <cstdint>
#include "shared/idlib/math/vector.h"

struct compressedWeights_t {
  std::uint8_t high, low;
};

class idDrawVert {
public:
  idVec3 xyz;
  idVec2 st;
  std::uint8_t normal[4];
  std::uint8_t tangent[4];
  std::uint8_t color[4];
  static std::uint16_t CompressWeightsToBytes(const std::uint8_t* weights);
  const idVec3 GetBiTangentRaw() const;
  void SetBiTangent(const idVec3& tangentDirection);
};

static_assert(sizeof(compressedWeights_t) == 2, "compressedWeights_t ABI drift");
static_assert(sizeof(idDrawVert) == 32, "idDrawVert ABI drift");
