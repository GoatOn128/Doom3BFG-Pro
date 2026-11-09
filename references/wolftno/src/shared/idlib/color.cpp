#include "color.h"

#include "shared/idlib/text/str.h"

#include <algorithm>

const idColor idColor::colorRed{1, 0, 0, 1};
const idColor idColor::colorBlack{0, 0, 0, 1};
const idColor idColor::colorGreen{0, 1, 0, 1};
const idColor idColor::colorLtGrey{0.75F, 0.75F, 0.75F, 1.0F};
const idColor idColor::colorBlue{0, 0, 1, 1};
const idColor idColor::colorCyan{0, 1, 1, 1};
const idColor idColor::colorYellow{1, 1, 0, 1};
const idColor idColor::colorWhite{1, 1, 1, 1};
const idColor idColor::colorOrange{1, 0.5F, 0, 1};
const idColor idColor::colorPurple{0.5F, 0, 0.5F, 1};
const idColor idColor::colorMagenta{1, 0, 1, 1};
const idColor idColor::colorXAxis = idColor::colorBlue;
const idColor idColor::colorYAxis = idColor::colorRed;
const idColor idColor::colorZAxis = idColor::colorGreen;

const char* idColor::ToString(int precision, bool parentheses) const {
  const char* values = idStr::FloatArrayToString(&r, 4, precision);
  if (!parentheses)
    return values;
  static thread_local char buffer[16384];
  idStr::snPrintf(buffer, sizeof(buffer), "( %s )", values);
  return buffer;
}

void idColor::Lerp(const idColor& from, const idColor& to, float fraction) {
  if (fraction <= 0) {
    *this = from;
    return;
  }
  if (fraction >= 1) {
    *this = to;
    return;
  }
  r = (to.r - from.r) * fraction + from.r;
  g = (to.g - from.g) * fraction + from.g;
  b = (to.b - from.b) * fraction + from.b;
  a = (to.a - from.a) * fraction + from.a;
}

std::uint32_t idColor::PackColor(const idVec4& color) {
  auto byte = [](float value) {
    return static_cast<std::uint32_t>(std::clamp(static_cast<int>(value * 255.0f), 0, 255));
  };
  return byte(color.x) | (byte(color.y) << 8) | (byte(color.z) << 16) | (byte(color.w) << 24);
}

void idColor::UnpackColor(std::uint32_t color, idVec4& output) {
  constexpr float scale = 1.0f / 255.0f;
  output.x = (color & 0xFF) * scale;
  output.y = ((color >> 8) & 0xFF) * scale;
  output.z = ((color >> 16) & 0xFF) * scale;
  output.w = ((color >> 24) & 0xFF) * scale;
}
