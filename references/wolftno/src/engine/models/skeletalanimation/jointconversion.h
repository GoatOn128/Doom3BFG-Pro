#pragma once

#include "engine/decls/decl.h"
#include "engine/animation/md6.h"
#include "shared/idlib/containers/staticlist.h"
#include "shared/idlib/filesystem/file.h"

#include <cstddef>
#include <cstdint>

class idParser;

enum invalidJointHandle_t { INVALID_JOINT_HANDLE = -1 };
enum invalidUserChannelHandle_t { INVALID_USER_CHANNEL_HANDLE = -1 };
enum invalidJointConversionHandle_t { INVALID_JOINT_CONVERSION_HANDLE = -1 };
enum invalidCrc_t { INVALID_CRC = -1 };

template <typename T, typename InvalidTag, int InvalidValue> class idHandle;

#define WOLFTNO_DECLARE_USHORT_HANDLE(TagType)                              \
  template <> class idHandle<unsigned short, TagType, 65535> {             \
  public:                                                                   \
    unsigned short value = 0xffffu;                                         \
    idHandle() = default;                                                   \
    explicit constexpr idHandle(unsigned short initial) : value(initial) {} \
    bool operator==(const idHandle& other) const { return value == other.value; } \
    bool operator!=(const idHandle& other) const { return value != other.value; } \
    bool operator!() const { return value == 0xffffu; }                     \
    bool IsValid() const { return value != 0xffffu; }                       \
    void Release() { value = 0xffffu; }                                     \
    void Set(unsigned short next) { value = next; }                         \
    unsigned short& Get() { return value; }                                 \
    const unsigned short& Get() const { return value; }                     \
    bool Equal(unsigned short other) const { return value == other; }       \
    static constexpr unsigned short MaxHandleValue() { return 0xfffeu; }    \
  }

WOLFTNO_DECLARE_USHORT_HANDLE(invalidJointHandle_t);
WOLFTNO_DECLARE_USHORT_HANDLE(invalidUserChannelHandle_t);
WOLFTNO_DECLARE_USHORT_HANDLE(invalidJointConversionHandle_t);
WOLFTNO_DECLARE_USHORT_HANDLE(invalidCrc_t);
#undef WOLFTNO_DECLARE_USHORT_HANDLE

using idJointHandle_t =
    idHandle<unsigned short, invalidJointHandle_t, 65535>;
using idUserChannelHandle_t =
    idHandle<unsigned short, invalidUserChannelHandle_t, 65535>;
using idJointConversionHandle_t =
    idHandle<unsigned short, invalidJointConversionHandle_t, 65535>;
using idCrcHandle_t = idHandle<unsigned short, invalidCrc_t, 65535>;

inline constexpr idJointHandle_t NULL_JOINT{};
inline constexpr idUserChannelHandle_t NULL_USERCHANNEL{};
inline constexpr idJointConversionHandle_t NULL_JOINT_CONVERSION{};
inline constexpr idCrcHandle_t NULL_CRC{};

class idJointConversion;

class idJointConversionAnimation {
public:
  virtual ~idJointConversionAnimation() = default;
  virtual idJointHandle_t RegisterJoint(const char* name) = 0;
  virtual idJointHandle_t FindJointHandle(const char* name) const = 0;
  virtual const char* GetJointName(idJointHandle_t handle) const = 0;
  virtual idUserChannelHandle_t RegisterUserChannel(const char* name) = 0;
  virtual idUserChannelHandle_t FindUserChannelHandle(const char* name) const = 0;
  virtual const char* GetUserChannelName(idUserChannelHandle_t handle) const = 0;
  virtual void UnregisterJointConversion(idJointConversion* conversion) = 0;
};

void SetJointConversionAnimation(idJointConversionAnimation* service);

class idCompressionGroup {
public:
  struct joint_t {
    idJointHandle_t handle;
    std::uint16_t recoveredPadding;
    float modifier = 1.0f;
  };

  struct userChannel_t {
    idUserChannelHandle_t handle;
    std::uint16_t recoveredPadding;
    float modifier = 1.0f;
  };

  idCompressionGroup();
  idCompressionGroup(const idCompressionGroup&) = default;
  idCompressionGroup& operator=(const idCompressionGroup&) = default;
  ~idCompressionGroup() = default;

  const idAtomicString& GetGroupName() const { return groupName; }
  void SetGroupName(const char* name) { groupName.Set(name); }
  int NumJointHandles() const { return jointHandles.Num(); }
  int NumUserChannelHandles() const { return userChannelHandles.Num(); }
  void AddJointHandle(const idJointHandle_t& handle) {
    joint_t value;
    value.handle = handle;
    jointHandles.Append(value);
  }
  void AddUserChannelHandle(const idUserChannelHandle_t& handle) {
    userChannel_t value;
    value.handle = handle;
    userChannelHandles.Append(value);
  }
  void RemoveJointHandle(const idJointHandle_t& handle) {
    for (int index = 0; index < jointHandles.Num(); ++index)
      if (jointHandles[index].handle == handle) {
        jointHandles.RemoveIndex(index);
        return;
      }
  }
  void RemoveUserChannelHandle(const idUserChannelHandle_t& handle) {
    for (int index = 0; index < userChannelHandles.Num(); ++index)
      if (userChannelHandles[index].handle == handle) {
        userChannelHandles.RemoveIndex(index);
        return;
      }
  }
  void SetNumJoints(int count) { jointHandles.SetNum(count); }
  void SetNumUserChannels(int count) { userChannelHandles.SetNum(count); }
  idJointHandle_t& GetJointHandle(int index) { return jointHandles[index].handle; }
  const idJointHandle_t& GetJointHandle(int index) const {
    return jointHandles[index].handle;
  }
  idUserChannelHandle_t& GetUserChannelHandle(int index) {
    return userChannelHandles[index].handle;
  }
  const idUserChannelHandle_t& GetUserChannelHandle(int index) const {
    return userChannelHandles[index].handle;
  }
  const float GetJointModifier(idJointHandle_t handle) const;
  const float GetJointModifier(int index) const {
    return jointHandles[index].modifier;
  }
  void SetJointModifier(int index, float modifier) {
    jointHandles[index].modifier = modifier;
  }
  const float GetUserChannelModifier(idUserChannelHandle_t handle) const;
  const float GetUserChannelModifier(int index) const {
    return userChannelHandles[index].modifier;
  }
  void SetUserChannelModifier(int index, float modifier) {
    userChannelHandles[index].modifier = modifier;
  }
  void Parse(idParser& source);
  void Write(idFile* file) const;
  void Write(idFile_String& file) const { Write(static_cast<idFile*>(&file)); }
  void Read(idFile* file);

  idAtomicString groupName;
  idList<joint_t, 30> jointHandles;
  idList<userChannel_t, 30> userChannelHandles;
};

template <>
bool idList<idCompressionGroup::userChannel_t, 30>::Resize(int newSize);

class idJointConversion : public idResource {
public:
  idJointConversion();
  ~idJointConversion() noexcept(false) override;

  idResourceList* GetResourceList() const override;
  virtual const char* GetResourceTypeName() const;
  void LoadResource() override;
  bool ReloadIfStale() override;
  void Reload() {
    ClearData();
    LoadResource();
  }
  bool Write();
  void ClearData();

  idJointIndex_t GetJointIndex(const idJointHandle_t& handle) const;
  idUserChannelIndex GetUserChannelIndex(
      const idUserChannelHandle_t& handle) const;
  void SetHandle(idJointConversionHandle_t value) { handle = value; }
  idJointConversionHandle_t GetHandle() const { return handle; }
  idCrcHandle_t GetChecksum() const { return checksum; }
  static void MakeDefault(const char* filename,
                          const idList<idStr, 5>& jointNames,
                          const idList<idStr, 5>& userChannelNames);
  const idCompressionGroup* GetCompressionGroup(const char* groupName) const;
  idCompressionGroup* GetCompressionGroup(const char* groupName) {
    return const_cast<idCompressionGroup*>(
        static_cast<const idJointConversion*>(this)->GetCompressionGroup(
            groupName));
  }
  idCompressionGroup* AllocCompressionGroup(const char* groupName);
  void RemoveCompressionGroup(const char* groupName);
  const idJointHandle_t* GetJointHandleTable() const {
    return tableJoints.list;
  }
  int NumJointHandle() const { return tableJoints.Num(); }
  const idUserChannelHandle_t* GetUserChannelHandleTable() const {
    return tableUserChannels.list;
  }
  int NumUserChannelHandle() const { return tableUserChannels.Num(); }
  static bool IsJointConversionEqual(const idMD6Skel* first,
                                     const idMD6Skel* second);
  static bool IsJointConversionEqual(idJointConversionHandle_t firstHandle,
                                     idCrcHandle_t firstChecksum,
                                     idJointConversionHandle_t secondHandle,
                                     idCrcHandle_t secondChecksum) {
    if (firstHandle.IsValid() && secondHandle.IsValid())
      return firstChecksum == secondChecksum;
    return firstHandle.IsValid() == secondHandle.IsValid();
  }

  static const char* ALL_COMPRESSIONGROUP_NAME;
  static idTypedResourceList<idJointConversion> resourceList;

  idList<idCompressionGroup*, 30> compressionGroups;
  idJointConversionHandle_t handle;
  std::uint8_t recoveredHandlePadding[6];
  idStaticList<idJointHandle_t, 256> tableJoints;
  idStaticList<idUserChannelHandle_t, 256> tableUserChannels;
  idCrcHandle_t checksum;
  std::uint16_t recoveredChecksumPadding;
  std::uint32_t timestamp;

private:
  idJointConversion(const idJointConversion&) = delete;
  idJointConversion& operator=(const idJointConversion&) = delete;
  void GenerateChecksum();
  bool LoadJointConversion_Text();
  bool LoadJointConversion_Binary(const char* filename);
  bool WriteJointConversion_Binary(const char* filename);
};

template <>
idJointConversion*
idTypedResourceList<idJointConversion>::Alloc(const char* name) const;
template <>
int idTypedResourceList<idJointConversion>::AllocSize() const;

static_assert(sizeof(idJointHandle_t) == 2);
static_assert(sizeof(idCompressionGroup::joint_t) == 8);
static_assert(sizeof(idCompressionGroup::userChannel_t) == 8);
static_assert(sizeof(idCompressionGroup) == 0x38);
static_assert(offsetof(idCompressionGroup, jointHandles) == 0x08);
static_assert(offsetof(idCompressionGroup, userChannelHandles) == 0x20);
static_assert(sizeof(idJointConversion) == 0x488);
static_assert(offsetof(idJointConversion, compressionGroups) == 0x30);
static_assert(offsetof(idJointConversion, handle) == 0x48);
static_assert(offsetof(idJointConversion, tableJoints) == 0x50);
static_assert(offsetof(idJointConversion, tableUserChannels) == 0x268);
static_assert(offsetof(idJointConversion, checksum) == 0x480);
static_assert(offsetof(idJointConversion, timestamp) == 0x484);
