#pragma once

#include "engine/animation/md6.h"
#include "shared/idlib/math/vector.h"
#include "shared/idlib/sys/sys_alloc.h"
#include "shared/idlib/text/parser.h"

#include <cstddef>
#include <cstdint>
#include <cstring>

enum argTypes_t : std::uint8_t {
  ARGTYPE_NONE = 0,
  ARGTYPE_BOOL = 1,
  ARGTYPE_CHAR = 2,
  ARGTYPE_INT = 3,
  ARGTYPE_FLOAT = 4,
  ARGTYPE_VEC3 = 5,
  ARGTYPE_VEC4 = 6,
  ARGTYPE_ANGLES = 7,
  ARGTYPE_STRING = 8,
  ARGTYPE_POINTER = 9,
  ARGTYPE_QUAT = 10,
  ARGTYPE_SPAWNID = 11,
  ARGTYPE_HANDLE = 13,
};

template <int MaxArgs> class idVarArgs {
public:
  using argOffset_t = std::uint16_t;

  __forceinline explicit idVarArgs(int initialSize = 0)
      : numArgs(0), argSize(0), buffSize(0), argOffsets{}, argTypes{},
        argExTypes{}, pad{}, args(nullptr) {
    Grow(static_cast<std::size_t>(initialSize));
  }
  idVarArgs(const idVarArgs& other) : idVarArgs() { Copy(other); }
  ~idVarArgs();

  const idVarArgs& operator=(const idVarArgs& other) {
    Copy(other);
    return *this;
  }
  bool operator==(const idVarArgs& other) const {
    return Equal(other);
  }
  bool operator!=(const idVarArgs& other) const { return !Equal(other); }

  int GetNumArgs() const { return numArgs; }
  argTypes_t GetArgType(int index) const {
    return index >= 0 && index < numArgs
               ? static_cast<argTypes_t>(argTypes[index])
               : ARGTYPE_NONE;
  }
  int GetExType(int index) const {
    return index >= 0 && index < numArgs ? argExTypes[index] : 0;
  }
  void ClearArgs() {
    Free();
    std::memset(argOffsets, 0, sizeof(argOffsets));
    std::memset(argTypes, 0, sizeof(argTypes));
    std::memset(argExTypes, 0, sizeof(argExTypes));
  }

  void AddArg(const void* value, std::uint8_t type,
              std::uint8_t exType = 0);
  void AddArg(const char* value, std::uint8_t exType = 0);
  __forceinline void AddArg(const idAngles& value,
                            std::uint8_t exType = 0) {
    AddValue(value, ARGTYPE_ANGLES, exType);
  }
  __forceinline void AddArg(const idQuat& value, std::uint8_t exType = 0) {
    AddValue(value, ARGTYPE_QUAT, exType);
  }
  __forceinline void AddArg(const idVec4& value, std::uint8_t exType = 0) {
    AddValue(value, ARGTYPE_VEC4, exType);
  }
  __forceinline void AddArg(const idVec3& value, std::uint8_t exType = 0) {
    AddValue(value, ARGTYPE_VEC3, exType);
  }
  void AddArg(float value, std::uint8_t exType = 0);
  void AddArg(int value, std::uint8_t exType = 0);
  __forceinline void AddArg(char value, std::uint8_t exType = 0) {
    AddValue(value, ARGTYPE_CHAR, exType);
  }
  void AddArg(bool value, std::uint8_t exType = 0);
  void AddHandleArg(int value, std::uint8_t type, std::uint8_t exType);

  template <typename T>
  bool GetArg(int index, T& value, const T& defaultValue) const {
    if (index < 0 || index >= numArgs || args == nullptr ||
        argOffsets[index] + sizeof(T) > argSize) {
      value = defaultValue;
      return false;
    }
    std::memcpy(&value, args + argOffsets[index], sizeof(T));
    return true;
  }
  template <typename T> bool GetArg(int index, T& value) const {
    return GetArg(index, value, T{});
  }
  bool GetArg(int index, const char*& value,
              const char* defaultValue = "") const {
    if (index < 0 || index >= numArgs || args == nullptr ||
        argTypes[index] != ARGTYPE_STRING) {
      value = defaultValue;
      return false;
    }
    value = reinterpret_cast<const char*>(args + argOffsets[index]);
    return true;
  }
  bool GetHandleArg(int index, int& value, std::uint8_t type) const {
    if (index < 0 || index >= numArgs || argTypes[index] != type)
      return false;
    return GetArg(index, value, 0);
  }
  bool VerifyArgs(int count, const std::uint8_t* types,
                  const std::uint8_t* exTypes) const {
    if (count != numArgs)
      return false;
    for (int index = 0; index < count; ++index) {
      if ((types != nullptr && types[index] != argTypes[index]) ||
          (exTypes != nullptr && exTypes[index] != argExTypes[index]))
        return false;
    }
    return true;
  }
  std::size_t Memory() const { return buffSize; }

  std::uint16_t numArgs;
  std::uint16_t argSize;
  std::uint16_t buffSize;
  std::uint16_t argOffsets[MaxArgs];
  std::uint8_t argTypes[MaxArgs];
  std::uint8_t argExTypes[MaxArgs];
  std::uint8_t pad[6];
  std::uint8_t* args;

protected:
  friend class idMD6AnimEvent;
  void Free() {
    if (args != nullptr)
      idMem::Singleton()->Free(args, ALIGN_16);
    args = nullptr;
    argSize = 0;
    buffSize = 0;
    numArgs = 0;
  }
  void Grow(std::size_t newArgSize);
  template <typename T>
  __forceinline void AddValue(const T& value, std::uint8_t type,
                              std::uint8_t exType) {
    argTypes[numArgs] = type;
    argExTypes[numArgs] = exType;
    argOffsets[numArgs] = argSize;
    Grow(sizeof(T));
    const int index = numArgs;
    argSize = static_cast<std::uint16_t>(argSize + sizeof(T));
    std::memcpy(args + argOffsets[index], &value, sizeof(T));
    ++numArgs;
  }
  void Copy(const idVarArgs& other);
  bool Equal(const idVarArgs& other) const {
    return numArgs == other.numArgs && argSize == other.argSize &&
           std::memcmp(argOffsets, other.argOffsets, sizeof(argOffsets)) == 0 &&
           std::memcmp(argTypes, other.argTypes, sizeof(argTypes)) == 0 &&
           std::memcmp(argExTypes, other.argExTypes, sizeof(argExTypes)) == 0 &&
           (argSize == 0 ||
            (args != nullptr && other.args != nullptr &&
             std::memcmp(args, other.args, argSize) == 0));
  }
};

struct idCachedJoint {
  idJointIndex_t jointIndex;
  std::int16_t frameNum{};
  idVec3 trans{};
  idQuat rot{};

  bool operator==(const idCachedJoint& other) const {
    return jointIndex.value == other.jointIndex.value &&
           frameNum == other.frameNum &&
           std::memcmp(&trans, &other.trans, sizeof(trans)) == 0 &&
           std::memcmp(&rot, &other.rot, sizeof(rot)) == 0;
  }
  bool operator!=(const idCachedJoint& other) const {
    return !(*this == other);
  }
};

enum invalidAnimEventId_t {};

struct idAnimEventId {
  std::uint32_t value{};
  bool operator==(const idAnimEventId& other) const {
    return value == other.value;
  }
  bool operator!=(const idAnimEventId& other) const {
    return value != other.value;
  }
};

struct idAnimEventCommandInfo {
  const char* name{};
  bool canSkip{};
  bool isClientSafe{};
  int eventNum{-1};
  const char* formatSpec{};
  const char* const* argumentTypeNames{};
  int numArgs{};
};

using idMD6AnimEventLookupCallback =
    const idAnimEventCommandInfo* (*)(const char* eventName);
using idMD6AnimAliasCallback = std::uint16_t (*)(const char* aliasName);

void SetMD6AnimEventParseCallbacks(idMD6AnimEventLookupCallback findEvent,
                                   idMD6AnimAliasCallback addStrongAliasRef);

enum md6EventArgType_t {
  MD6_EVENTARG_NONE = 0,
  MD6_EVENTARG_JOINT_INDEX = 1,
  MD6_EVENTARG_JOINT_TAG = 2,
  MD6_EVENTARG_ENUM = 3,
};

class idMD6AnimEvent {
public:
  static constexpr int MD6_NUM_REQUIRED_ARGS = 1;
  static constexpr int MD6_MAX_OPTIONAL_ARGS = 4;
  static constexpr int MD6_MAX_ARGS = 5;
  static const char* animEventArgTypeNames[4];

  static md6EventArgType_t ArgTypeForTypeName(const char* typeName) {
    for (int index = 1; index < 4; ++index) {
      if (idStr::Icmp(typeName, animEventArgTypeNames[index]) == 0)
        return static_cast<md6EventArgType_t>(index);
    }
    return MD6_EVENTARG_NONE;
  }

  idMD6AnimEvent();
  idMD6AnimEvent(const idMD6AnimEvent& other) : idMD6AnimEvent() {
    Copy(other);
  }
  ~idMD6AnimEvent() noexcept(false);
  idMD6AnimEvent& operator=(const idMD6AnimEvent& other) {
    Copy(other);
    return *this;
  }
  bool operator==(const idMD6AnimEvent& other) const {
    return args == other.args && eventNum == other.eventNum &&
           frameNum == other.frameNum && eventId == other.eventId &&
           locked == other.locked && row == other.row &&
           ((cachedJoint == nullptr && other.cachedJoint == nullptr) ||
            (cachedJoint != nullptr && other.cachedJoint != nullptr &&
             *cachedJoint == *other.cachedJoint));
  }
  bool operator!=(const idMD6AnimEvent& other) const {
    return !(*this == other);
  }

  void Copy(const idMD6AnimEvent& other);
  bool Parse(const idDeclMD6* md6, idParser& source, int& loadErrors);
  std::size_t Size() const {
    return sizeof(*this) + args.Memory() +
           (cachedJoint != nullptr ? sizeof(*cachedJoint) : 0);
  }

  idVarArgs<5>& GetArgs() { return args; }
  const idVarArgs<5>& GetArgs() const { return args; }
  void SetArgs(const idVarArgs<5>& value) { args = value; }
  int GetEventNum() const { return eventNum; }
  void SetEventNum(int value) { eventNum = static_cast<std::int16_t>(value); }
  int GetRow() const { return row; }
  void SetRow(int value) { row = static_cast<std::uint8_t>(value); }
  void SetLocked(bool value) { locked = value; }
  bool GetLocked() const { return locked; }
  int GetFrameNum() const { return frameNum; }
  void SetFrameNum(int value) { frameNum = static_cast<std::int16_t>(value); }
  const idCachedJoint* GetCachedJoint() const { return cachedJoint; }
  void SetCachedJoint(const idJointIndex_t& jointIndex, int frame,
                      const idVec3& translation, const idQuat& rotation);
  bool EventOverloadsMe(const idMD6AnimEvent* other) const {
    return other != nullptr && eventNum == other->eventNum &&
           frameNum == other->frameNum && row == other->row;
  }
  idAnimEventId GetEventId() const { return eventId; }
  void SetEventId(idAnimEventId value) { eventId = value; }

  idVarArgs<5> args;
  idCachedJoint* cachedJoint{};
  std::int16_t eventNum{-1};
  std::int16_t frameNum{};
  idAnimEventId eventId{};
  bool locked{};
  std::uint8_t row{};
  std::uint8_t pad[6]{};
};

static_assert(sizeof(idVarArgs<5>) == 0x28);
static_assert(offsetof(idVarArgs<5>, numArgs) == 0x00);
static_assert(offsetof(idVarArgs<5>, argSize) == 0x02);
static_assert(offsetof(idVarArgs<5>, buffSize) == 0x04);
static_assert(offsetof(idVarArgs<5>, argOffsets) == 0x06);
static_assert(offsetof(idVarArgs<5>, argTypes) == 0x10);
static_assert(offsetof(idVarArgs<5>, argExTypes) == 0x15);
static_assert(offsetof(idVarArgs<5>, args) == 0x20);
static_assert(sizeof(idCachedJoint) == 0x20);
static_assert(offsetof(idCachedJoint, frameNum) == 0x02);
static_assert(offsetof(idCachedJoint, trans) == 0x04);
static_assert(offsetof(idCachedJoint, rot) == 0x10);
static_assert(sizeof(idAnimEventId) == 4);
static_assert(sizeof(idMD6AnimEvent) == 0x40);
static_assert(offsetof(idMD6AnimEvent, cachedJoint) == 0x28);
static_assert(offsetof(idMD6AnimEvent, eventNum) == 0x30);
static_assert(offsetof(idMD6AnimEvent, frameNum) == 0x32);
static_assert(offsetof(idMD6AnimEvent, eventId) == 0x34);
static_assert(offsetof(idMD6AnimEvent, locked) == 0x38);
static_assert(offsetof(idMD6AnimEvent, row) == 0x39);
