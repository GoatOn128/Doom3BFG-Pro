#pragma once

#include "engine/animation/md6.h"
#include "shared/idlib/containers/list.h"
#include "shared/idlib/math/vector.h"
#include "shared/idlib/text/str.h"

#include <cstddef>
#include <cstdint>

class idFile_String;
class idParser;

enum invalidEyeInfo_t { INVALID_EYE_INFO = 0xffff };

template <typename T, typename InvalidEnum, int InvalidValue> class idHandle;

template <>
class idHandle<std::uint16_t, invalidEyeInfo_t, 0xffff> {
public:
  std::uint16_t value = 0xffff;
  bool operator==(const idHandle& other) const {
    return value == other.value;
  }
  bool operator!=(const idHandle& other) const {
    return value != other.value;
  }
};

using eyeInfoHandle_t =
    idHandle<std::uint16_t, invalidEyeInfo_t, 0xffff>;

class idEyeInfo {
public:
  struct perEyeInfo_t {
    float focusScale = 0.25f;
    float wanderRate = 0.0f;
    float maxWanderPitch = 15.0f;
    float maxWanderYaw = 15.0f;
    float jitterScale = 1.0f;
    float slerpScale = 0.5f;
    idAngles angleOffset{};
  };

  enum eye_t : int {
    EYE_LEFT = 0,
    EYE_RIGHT = 1,
    EYE_MAX = 2,
  };

  idEyeInfo();
  ~idEyeInfo() = default;

  void Parse(const idDeclMD6* declmd6, idParser& source, int& loadErrors);
  void Write(const idDeclMD6* declmd6, idFile_String& file,
             const char* prefix);

  bool IsInherited(const idEyeInfo& other) const {
    return idStr::Icmp(name.data, other.name.data) == 0;
  }
  const char* GetName() const { return name.data; }
  void SetName(const char* value) { name = value; }
  const eyeInfoHandle_t& GetHandle() const { return handle; }
  void SetHandle(const eyeInfoHandle_t& value) { handle = value; }
  idJointIndex_t GetBasisJointIndex() const { return basisJointIndex; }
  void SetBasisJointIndex(const idJointIndex_t& value) {
    basisJointIndex = value;
  }
  float GetMinEyePitch() const { return minEyePitch; }
  void SetMinEyePitch(float value) { minEyePitch = value; }
  float GetMaxEyePitch() const { return maxEyePitch; }
  void SetMaxEyePitch(float value) { maxEyePitch = value; }
  float GetMinEyeYaw() const { return minEyeYaw; }
  void SetMinEyeYaw(float value) { minEyeYaw = value; }
  float GetMaxEyeYaw() const { return maxEyeYaw; }
  void SetMaxEyeYaw(float value) { maxEyeYaw = value; }
  float GetLidMinPitch() const { return lidMinPitch; }
  void SetLidMinPitch(float value) { lidMinPitch = value; }
  float GetLidMaxPitch() const { return lidMaxPitch; }
  void SetLidMaxPitch(float value) { lidMaxPitch = value; }
  float GetLidMinYaw() const { return lidMinYaw; }
  void SetLidMinYaw(float value) { lidMinYaw = value; }
  float GetLidMaxYaw() const { return lidMaxYaw; }
  void SetLidMaxYaw(float value) { lidMaxYaw = value; }
  const perEyeInfo_t& GetPerEyeInfo(eye_t eye) const {
    return perEyeInfo[eye];
  }
  void SetPerEyeInfo(eye_t eye, const perEyeInfo_t& info) {
    perEyeInfo[eye] = info;
  }

  static const char* eyeNames[EYE_MAX];

  idStr name;
  eyeInfoHandle_t handle;
  idJointIndex_t basisJointIndex;
  float minEyePitch;
  float maxEyePitch;
  float minEyeYaw;
  float maxEyeYaw;
  float lidMinPitch;
  float lidMaxPitch;
  float lidMinYaw;
  float lidMaxYaw;
  perEyeInfo_t perEyeInfo[EYE_MAX];
};

class idEyeInfoCollection {
public:
  idEyeInfoCollection();
  ~idEyeInfoCollection();

  void Parse(const idDeclMD6* decl, idParser& source, int& loadErrors);
  void Write(const idDeclMD6* decl, idFile_String& file,
             const char* prefix);
  void DuplicateInherited(const idDeclMD6* declmd6,
                          const idEyeInfoCollection* parentCollection);
  void Free();
  const idEyeInfo* GetEyeInfo(const eyeInfoHandle_t& handle) const;
  idEyeInfo* GetEditableEyeInfo(const eyeInfoHandle_t& handle) {
    return const_cast<idEyeInfo*>(
        static_cast<const idEyeInfoCollection*>(this)->GetEyeInfo(handle));
  }
  eyeInfoHandle_t FindEyeInfoHandle(const char* name) const;
  eyeInfoHandle_t AddEyeInfo(const idEyeInfo& info);
  void RemoveEyeInfo(const eyeInfoHandle_t& handle) {
    const int index = FindEyeInfoIndex(handle);
    if (index >= 0)
      eyeInfos.RemoveIndex(index);
  }

  static const char* DEFAULT_NAME;

  std::uint16_t curHandle;
  std::uint8_t recoveredPadding[6]{};
  idList<idEyeInfo> eyeInfos;

private:
  int FindEyeInfoIndex(const char* name) const;
  int FindEyeInfoIndex(const eyeInfoHandle_t& handle) const {
    for (int index = 0; index < eyeInfos.Num(); ++index) {
      if (eyeInfos[index].handle == handle)
        return index;
    }
    return -1;
  }
  bool IsInherited(const idEyeInfo& info,
                   const idEyeInfoCollection* parent) const {
    return parent != nullptr && parent->FindEyeInfoIndex(info.name.data) >= 0;
  }
};

template <>
bool idList<idEyeInfo, 5>::Resize(int newSize);
template <>
int idList<idEyeInfo, 5>::Append(const idEyeInfo& object);

static_assert(sizeof(eyeInfoHandle_t) == 2, "eye-info handle ABI drift");
static_assert(sizeof(idEyeInfo::perEyeInfo_t) == 36,
              "per-eye info ABI drift");
static_assert(sizeof(idEyeInfo) == 152, "idEyeInfo ABI drift");
static_assert(offsetof(idEyeInfo, handle) == 0x28,
              "idEyeInfo handle offset drift");
static_assert(offsetof(idEyeInfo, perEyeInfo) == 0x4C,
              "idEyeInfo per-eye offset drift");
static_assert(sizeof(idEyeInfoCollection) == 32,
              "idEyeInfoCollection ABI drift");
static_assert(offsetof(idEyeInfoCollection, eyeInfos) == 8,
              "idEyeInfoCollection list offset drift");
