#pragma once

#include "shared/idlib/math/vector.h"

#include <cstdint>

class idColor {
public:
  float r, g, b, a;

  // The retail renderer's debug interface is expressed in idVec4.  idColor
  // is layout-compatible and remains the source-facing convenience type used
  // by game/model call sites.
  operator const idVec4&() const {
    return *reinterpret_cast<const idVec4*>(this);
  }

#if defined(WOLFTNO_RETAIL_MUTABLE_IDCOLOR_ABI)
#define IDCOLOR_RETAIL_STORAGE
#else
#define IDCOLOR_RETAIL_STORAGE const
#endif
  static IDCOLOR_RETAIL_STORAGE idColor colorRed;
  static IDCOLOR_RETAIL_STORAGE idColor colorBlack;
  static IDCOLOR_RETAIL_STORAGE idColor colorGreen;
  static IDCOLOR_RETAIL_STORAGE idColor colorLtGrey;
  static IDCOLOR_RETAIL_STORAGE idColor colorBlue;
  static IDCOLOR_RETAIL_STORAGE idColor colorCyan;
  static IDCOLOR_RETAIL_STORAGE idColor colorYellow;
  static IDCOLOR_RETAIL_STORAGE idColor colorWhite;
  static IDCOLOR_RETAIL_STORAGE idColor colorOrange;
  static IDCOLOR_RETAIL_STORAGE idColor colorPurple;
  static IDCOLOR_RETAIL_STORAGE idColor colorMagenta;
  static IDCOLOR_RETAIL_STORAGE idColor colorDarkRed;
  static IDCOLOR_RETAIL_STORAGE idColor colorDarkGreen;
  static IDCOLOR_RETAIL_STORAGE idColor colorPink;
  static IDCOLOR_RETAIL_STORAGE idColor colorBrown;
  static IDCOLOR_RETAIL_STORAGE idColor colorMdGrey;
  static IDCOLOR_RETAIL_STORAGE idColor colorDkGrey;
  static IDCOLOR_RETAIL_STORAGE idColor colorXAxis;
  static IDCOLOR_RETAIL_STORAGE idColor colorYAxis;
  static IDCOLOR_RETAIL_STORAGE idColor colorZAxis;
#undef IDCOLOR_RETAIL_STORAGE

  const char* ToString(int precision = 2, bool parentheses = false) const;
  void Lerp(const idColor& from, const idColor& to, float fraction);
  static std::uint32_t PackColor(const idVec4& color);
  static void UnpackColor(std::uint32_t color, idVec4& unpackedColor);
};

static_assert(sizeof(idColor) == 16, "idColor ABI drift");
