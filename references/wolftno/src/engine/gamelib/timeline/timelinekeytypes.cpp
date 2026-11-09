#include "engine/gamelib/timeline/timelinekeytypes.h"

#include "shared/idlib/text/lexer.h"

#include <cstdlib>

static const tlKeyType_t keyTypeMap[TL_KEY_MAX] = {
    TL_KEYTYPE_BOOL,
    TL_KEYTYPE_COLOR_RGB,
    TL_KEYTYPE_FLOAT6,
    TL_KEYTYPE_FLOAT,
    TL_KEYTYPE_FLOAT,
    TL_KEYTYPE_FLOAT,
    TL_KEYTYPE_FLOAT,
    TL_KEYTYPE_FLOAT,
    TL_KEYTYPE_FLOAT,
    TL_KEYTYPE_FLOAT,
    TL_KEYTYPE_FLOAT,
    TL_KEYTYPE_FLOAT,
    TL_KEYTYPE_FLOAT,
    TL_KEYTYPE_FLOAT,
};

static const int keySizes[TL_KEYTYPE_MAX] = {
    1,
    4,
    4,
    12,
    12,
    16,
    16,
    0,
    24,
};

tlKeyType_t mgTimelineKeyUtil::GetType(tlKeyId_t key) {
  return keyTypeMap[key];
}

int mgTimelineKeyUtil::KeySize(tlKeyType_t type) {
  return keySizes[type];
}

bool mgTimelineKeyUtil::FromString(const idStr& valueStr, bool* value) {
  *value = std::atol(valueStr.c_str()) != 0;
  return true;
}

bool mgTimelineKeyUtil::FromString(const idStr& valueStr, int* value) {
  *value = std::atol(valueStr.c_str());
  return true;
}

int mgTimelineKeyUtil::ParseFloatArray(const idStr& valueStr, float* values,
                                       int count) {
  idLexer source(0);
  source.LoadMemory(valueStr.c_str(),
                    static_cast<std::size_t>(valueStr.Length()), nullptr);

  int parsedCount = 0;
  for (int index = 0; index < count; ++index) {
    bool error;
    values[index] = source.ParseFloat(&error);
    if (error) {
      break;
    }
    ++parsedCount;
  }
  return parsedCount;
}

bool mgTimelineKeyUtil::FromString(const idStr& valueStr, float* value) {
  return ParseFloatArray(valueStr, value, 1) == 1;
}

bool mgTimelineKeyUtil::FromString(const idStr& valueStr, idVec3* value) {
  return ParseFloatArray(valueStr, &value->x, 3) == 3;
}

bool mgTimelineKeyUtil::FromString(const idStr& valueStr, idVec4* value) {
  return ParseFloatArray(valueStr, &value->x, 4) == 4;
}

bool mgTimelineKeyUtil::FromString(const idStr& valueStr, idVec6* value) {
  return ParseFloatArray(valueStr, value->p, 6) == 6;
}
