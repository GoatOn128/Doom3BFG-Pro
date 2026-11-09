#pragma once

#include "shared/idlib/math/vector.h"
#include "shared/idlib/text/str.h"

enum tlKeyType_t {
  TL_KEYTYPE_BOOL = 0,
  TL_KEYTYPE_INT = 1,
  TL_KEYTYPE_FLOAT = 2,
  TL_KEYTYPE_FLOAT3 = 3,
  TL_KEYTYPE_COLOR_RGB = 4,
  TL_KEYTYPE_FLOAT4 = 5,
  TL_KEYTYPE_COLOR_RGBA = 6,
  TL_KEYTYPE_STRING = 7,
  TL_KEYTYPE_FLOAT6 = 8,
  TL_KEYTYPE_MAX = 9,
};

enum tlKeyId_t {
  TL_KEY_INVALID = -1,
  TL_KEY_VISIBILITY = 0,
  TL_KEY_LIGHTCOLOR = 1,
  TL_KEY_DOF = 2,
  TL_KEY_FOCAL = 3,
  TL_KEY_NEARBP = 4,
  TL_KEY_FARBP = 5,
  TL_KEY_BSCALE = 6,
  TL_KEY_DOF_NEAR_AMOUNT = 7,
  TL_KEY_DOF_NEAR_END = 8,
  TL_KEY_DOF_FOCUS_CENTER = 9,
  TL_KEY_DOF_FOCUS_RANGE = 10,
  TL_KEY_DOF_FAR_BEGIN = 11,
  TL_KEY_DOF_FAR_AMOUNT = 12,
  TL_KEY_DOF_SKY_AMOUNT = 13,
  TL_KEY_MAX = 14,
};

class mgTimelineKeyUtil {
public:
  static tlKeyType_t GetType(tlKeyId_t key);
  static int KeySize(tlKeyType_t type);

  static bool FromString(const idStr& valueStr, bool* value);
  static bool FromString(const idStr& valueStr, int* value);
  static bool FromString(const idStr& valueStr, float* value);
  static bool FromString(const idStr& valueStr, idVec3* value);
  static bool FromString(const idStr& valueStr, idVec4* value);
  static bool FromString(const idStr& valueStr, idVec6* value);

  static int ParseFloatArray(const idStr& valueStr, float* values, int count);
};

static_assert(sizeof(tlKeyType_t) == 4, "tlKeyType_t ABI drift");
static_assert(sizeof(tlKeyId_t) == 4, "tlKeyId_t ABI drift");
static_assert(sizeof(mgTimelineKeyUtil) == 1, "mgTimelineKeyUtil ABI drift");
