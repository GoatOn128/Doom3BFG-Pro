#pragma once

#include <array>
#include <cmath>
#include <cstdint>

namespace baredct_integer_tables {

using Table = std::array<std::int16_t, 32>;

inline int Fixed16(float value) {
  return static_cast<int>(value * 65536.0f + 0.5f);
}

inline std::int16_t TableValue(float value) {
  return static_cast<std::int16_t>(
      static_cast<int>(value * 32768.0f + 0.5f));
}

inline Table MakeForward(float verticalScale) {
  const std::int16_t dc =
      TableValue(std::cos(0.78539819f) * verticalScale);
  const std::int16_t even2 =
      TableValue(std::cos(0.39269909f) * verticalScale);
  const std::int16_t even6 =
      TableValue(std::cos(1.1780972f) * verticalScale);
  const std::int16_t odd1 =
      TableValue(std::cos(0.19634955f) * verticalScale);
  const std::int16_t odd3 =
      TableValue(std::cos(0.58904862f) * verticalScale);
  const std::int16_t odd5 =
      TableValue(std::cos(0.98174775f) * verticalScale);
  const std::int16_t odd7 =
      TableValue(std::cos(1.3744469f) * verticalScale);
  return {dc,     even2, dc,     even6,  dc,     -even6, -dc,    even2,
          dc,     even6, -dc,    -even2, dc,     -even2, dc,     -even6,
          odd1,   odd3,  odd5,   odd7,   odd5,   -odd1,  odd7,   odd3,
          odd3,   -odd7, -odd1,  -odd5,  odd7,   -odd5,  odd3,   -odd1};
}

inline Table MakeInverse(float verticalScale) {
  const std::int16_t dc =
      TableValue(std::cos(0.78539819f) * verticalScale);
  const std::int16_t even2 =
      TableValue(std::cos(0.39269909f) * verticalScale);
  const std::int16_t even6 =
      TableValue(std::cos(1.1780972f) * verticalScale);
  const std::int16_t odd1 =
      TableValue(std::cos(0.19634955f) * verticalScale);
  const std::int16_t odd3 =
      TableValue(std::cos(0.58904862f) * verticalScale);
  const std::int16_t odd5 =
      TableValue(std::cos(0.98174775f) * verticalScale);
  const std::int16_t odd7 =
      TableValue(std::cos(1.3744469f) * verticalScale);
  return {dc,     dc,     dc,     dc,     even2,  even6,  -even6, -even2,
          dc,     -dc,    -dc,    dc,     even6,  -even2, even2,  -even6,
          odd1,   odd3,   odd5,   odd7,   odd3,   -odd7,  -odd1,  -odd5,
          odd5,   -odd1,  odd7,   odd3,   odd7,   -odd5,  odd3,   -odd1};
}

}  // namespace baredct_integer_tables
