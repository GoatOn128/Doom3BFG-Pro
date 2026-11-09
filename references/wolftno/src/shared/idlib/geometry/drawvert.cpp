#include "drawvert.h"

#include <algorithm>

namespace {
std::uint8_t ClampWeight(float value, float maximum) {
  const int result = static_cast<int>(std::min(maximum, value));
  return static_cast<std::uint8_t>(result < 0 ? 0 : result > 255 ? 255 : result);
}
} // namespace

// idDrawVert::CompressWeightsToBytes, RVA 0x00A29620.
std::uint16_t idDrawVert::CompressWeightsToBytes(const std::uint8_t* weights) {
  compressedWeights_t result{};
  result.high = ClampWeight(static_cast<float>(weights[1]) + 0.5f, 127.0f);
  const auto middle = ClampWeight(weights[2] * 0.17647059f + 0.5f, 15.0f);
  const auto low = ClampWeight(weights[3] * 0.23529412f + 0.5f, 15.0f);
  result.low = static_cast<std::uint8_t>((middle << 4) | low);
  return static_cast<std::uint16_t>(result.high) | (static_cast<std::uint16_t>(result.low) << 8);
}
