#pragma once

#include "engine/gamelib/timeline/timelinekeytypes.h"
#include "shared/idlib/containers/list.h"
#include "shared/idlib/math/vector.h"
#include "shared/idlib/text/str.h"

#include <cstddef>
#include <cstdint>

class idSoundShader;
class idTimelineEnv;

template <> class idStrStatic<33> : public idStr {
public:
  idStrStatic() : idStr() {}
  ~idStrStatic();
};

class mgTimelineKeyAnimData {
public:
  mgTimelineKeyAnimData();
  ~mgTimelineKeyAnimData();

  void PostLoadInit();

  bool GetData(float time, bool* value) const;
  bool GetData(float time, int* value) const;
  bool GetData(float time, float* value) const;
  bool GetData(float time, idVec3* value) const;
  bool GetData(float time, idVec4* value) const;
  bool GetData(float time, idVec6* value) const;

  tlKeyType_t type;
  int keySize;
  idList<float, 5> times;
  idList<char, 5> data;

private:
  template <typename T> bool GetLerped(float time, T* value) const;
};

enum tlEventType_t {
  TL_EVENT_NONE = 0,
};

struct tlEventData_t {
  tlEventData_t();

  float time;
  float duration;
  tlEventType_t type;
  unsigned char padding0[4];
  idStrStatic<33> anim;
  bool useAnimPos;
  bool useAnimScale;
  unsigned char padding1[6];
  const idSoundShader* sound;
  idVec3 worldPos;
  idAngles worldRot;
  tlKeyId_t keyName;
  unsigned char padding2[4];
  idStr keyValue;
  mgTimelineKeyAnimData keyAnimData;
  bool activateStart;
  bool activateEnd;
  unsigned char padding3[6];
  idTimelineEnv* env;
  idStr lightEntDefText;
};

static_assert(sizeof(mgTimelineKeyAnimData) == 0x38,
              "mgTimelineKeyAnimData ABI drift");
static_assert(offsetof(mgTimelineKeyAnimData, type) == 0x00,
              "timeline key type ABI drift");
static_assert(offsetof(mgTimelineKeyAnimData, keySize) == 0x04,
              "timeline key size ABI drift");
static_assert(offsetof(mgTimelineKeyAnimData, times) == 0x08,
              "timeline key times ABI drift");
static_assert(offsetof(mgTimelineKeyAnimData, data) == 0x20,
              "timeline key data ABI drift");
static_assert(sizeof(idStrStatic<33>) == 0x28,
              "idStrStatic<33> ABI drift");
static_assert(offsetof(tlEventData_t, anim) == 0x10,
              "tlEventData_t ABI drift");
static_assert(offsetof(tlEventData_t, sound) == 0x40,
              "tlEventData_t ABI drift");
static_assert(offsetof(tlEventData_t, keyValue) == 0x68,
              "tlEventData_t ABI drift");
static_assert(offsetof(tlEventData_t, keyAnimData) == 0x90,
              "tlEventData_t ABI drift");
static_assert(offsetof(tlEventData_t, activateStart) == 0xC8,
              "tlEventData_t ABI drift");
static_assert(offsetof(tlEventData_t, lightEntDefText) == 0xD8,
              "tlEventData_t ABI drift");
static_assert(sizeof(tlEventData_t) == 0x100, "tlEventData_t ABI drift");
