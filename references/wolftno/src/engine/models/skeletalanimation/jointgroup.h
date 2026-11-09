#pragma once

#include "engine/animation/md6.h"
#include "shared/idlib/containers/list.h"
#include "shared/idlib/math/vector.h"
#include "shared/idlib/text/atomicstring.h"

#include <cstddef>

class idDecl;
class idFile_String;
class idParser;

enum invalidJointGroupHandle { INVALID_JOINT_GROUP_HANDLE = 0 };

template <typename T, typename InvalidEnum, int InvalidValue> class idHandle;

template <>
class idHandle<int, invalidJointGroupHandle, 0> {
public:
  static constexpr invalidJointGroupHandle INVALID_VALUE =
      INVALID_JOINT_GROUP_HANDLE;

  int value = 0;

  idHandle() = default;
  explicit idHandle(const int& initialValue) : value(initialValue) {}
  bool operator==(const idHandle& other) const { return value == other.value; }
  bool operator!=(const idHandle& other) const { return value != other.value; }
  bool operator!() const { return value == 0; }
  bool IsValid() const { return value != 0; }
  void Release() { value = 0; }
  void Set(const int& newValue) { value = newValue; }
  int& Get() { return value; }
  const int& Get() const { return value; }
  bool Equal(const int& other) const { return value == other; }
  static constexpr int MaxHandleValue() { return 0x7fffffff; }
};

using jointGroupHandle_t = idHandle<int, invalidJointGroupHandle, 0>;
extern jointGroupHandle_t NULL_JOINT_GROUP;

template <> struct idListAllocReturnsReference<idJointIndex_t> : std::true_type {};

class idJointGroupCollection;

class idJointGroup {
public:
  enum jointGroup_t : int {
    JOINTGROUP_DAMAGE = 0,
    JOINTGROUP_PAIN = 1,
    JOINTGROUP_TWITCH = 2,
    JOINTGROUP_DEATH = 3,
    JOINTGROUP_DEATH_COLLISION = 4,
    JOINTGROUP_LIMBLOSS = 5,
    JOINTGROUP_HEADTRACKING = 6,
    JOINTGROUP_FOCUS = 7,
    JOINTGROUP_ORIENTATION = 8,
    JOINTGROUP_HITTEST = 9,
    JOINTGROUP_EYES = 10,
    JOINTGROUP_FEET = 11,
    JOINTGROUP_BOUNDS = 12,
    JOINTGROUP_MAX = 13,
  };

  struct jointGroupArgs_t {
    idAtomicString userName;
    int surfType;
    bool active;
    unsigned char recoveredPadding[3];

    jointGroupArgs_t() : userName(), surfType(3), active(true), recoveredPadding{} {}
  };

  idJointGroup(const idJointGroup&) = default;
  idJointGroup& operator=(const idJointGroup&) = default;
  ~idJointGroup();

  bool operator==(const idJointGroup& other) const {
    return type == other.type && idStr::Icmp(groupName.str, other.groupName.str) == 0;
  }
  bool Equal(const idDeclMD6*, const idJointGroup& other,
             const idDeclMD6*) const {
    return *this == other;
  }

  void Parse(idParser& source, const idDeclMD6* decl, int& loadErrors);
  void Write(const idDeclMD6*, idFile_String&, const char*) const {}

  int NumJoints() const { return joints.Num(); }
  idJointIndex_t GetJoint(int index) const { return joints[index]; }
  idVec3 GetJointOffset(int index) const {
    return index >= 0 && index < jointOffsets.Num() ? jointOffsets[index]
                                                    : idVec3{};
  }
  void SetJointOffset(int index, const idVec3& offset) {
    if (index < 0)
      return;
    jointOffsets.SetNum(index + 1, idVec3{});
    jointOffsets[index] = offset;
  }
  float GetJointScalar(int index) const {
    return index >= 0 && index < jointScalars.Num() ? jointScalars[index] : 1.0f;
  }
  void SetJointScalar(int index, float scalar) {
    if (index < 0)
      return;
    jointScalars.SetNum(index + 1, 1.0f);
    jointScalars[index] = scalar;
  }
  int GetSurfType() const { return args.surfType; }
  void SetSurfType(int surfType) { args.surfType = surfType; }
  const idAtomicString& GetJointUserName() const { return args.userName; }
  void SetJointUserName(const char* name) { args.userName.Set(name); }
  const idAtomicString& GetGroupName() const { return groupName; }
  void SetGroupName(const char* name) { groupName.Set(name); }
  jointGroup_t GetGroupType() const { return type; }
  bool IsJointInGroup(idJointIndex_t joint) const;
  void Copy(const idDeclMD6* myDecl, const idDeclMD6* otherDecl,
            const idJointGroup& other);
  const jointGroupHandle_t& GetHandle() const { return handle; }
  int AddJoint(idJointIndex_t joint);
  void RemoveJoint(int index);
  bool AllowDuplicateJoints() const { return type == JOINTGROUP_DEATH_COLLISION; }
  bool AllowMultipleJoints() const {
    return type != JOINTGROUP_FOCUS && type != JOINTGROUP_ORIENTATION;
  }
  void Activate(bool activate) { args.active = activate; }
  bool IsActive() const { return args.active; }

  static const char* jointGroupNames[JOINTGROUP_MAX];
  static const char* jointGroupNiceNames[JOINTGROUP_MAX];
  static const char* jointGroupScalarNames[JOINTGROUP_MAX];

  jointGroup_t type;
  unsigned int recoveredTypePadding;
  idAtomicString groupName;
  idList<idJointIndex_t, 30> joints;
  idList<idVec3, 30> jointOffsets;
  idList<float, 30> jointScalars;
  jointGroupArgs_t args;
  idVec3 groupOffset;
  jointGroupHandle_t handle;

private:
  friend class idJointGroupCollection;

  explicit idJointGroup(jointGroup_t type);
  idJointGroup(const char* groupName, jointGroup_t type,
               const jointGroupHandle_t& handle);
  idJointGroup(const idJointGroup& other, const jointGroupHandle_t& handle);
  void ParseArgs(idParser& source, const idDeclMD6* decl, int& loadErrors);
  void ParseInfo(idParser& source, int index, const idDeclMD6* decl,
                 int& loadErrors);
  void ParseSurfType(idParser& source, const idDeclMD6* decl,
                     int& loadErrors);
  void WriteArgs(idFile_String&, const char*) const {}
};

class idJointGroupCollection {
public:
  idJointGroupCollection();
  idJointGroupCollection(const idJointGroupCollection&) = delete;
  idJointGroupCollection& operator=(const idJointGroupCollection&) = delete;
  ~idJointGroupCollection();

  void Free();
  int Parse(idParser& source, const idDeclMD6* decl);
  void Write(idFile_String&, const idDeclMD6*, const char*) const {}
  bool JointGroupIsInherited(const idDeclMD6*, const idDeclMD6*,
                             const idJointGroupCollection* parent,
                             const idJointGroup* group) const;
  void DuplicateInherited(const idDecl* myDecl, const idDecl* parentDecl,
                          const idJointGroupCollection* parentCollection);
  void Condense();
  int NumJointGroups() const { return jointGroups.Num(); }
  const idJointGroup* GetJointGroup(const jointGroupHandle_t& handle) const;
  idJointGroup* GetJointGroup(const jointGroupHandle_t& handle) {
    return const_cast<idJointGroup*>(
        static_cast<const idJointGroupCollection*>(this)->GetJointGroup(handle));
  }
  const idJointGroup* GetJointGroup(int index) const {
    return index >= 0 && index < jointGroups.Num() ? jointGroups[index] : nullptr;
  }
  idJointGroup* GetJointGroup(int index) {
    return const_cast<idJointGroup*>(
        static_cast<const idJointGroupCollection*>(this)->GetJointGroup(index));
  }
  idJointGroup* GetJointGroupForName(idJointGroup::jointGroup_t type,
                                     const char* name);
  const idJointGroup* GetJointGroupForName(idJointGroup::jointGroup_t type,
                                           const char* name) const {
    return const_cast<idJointGroupCollection*>(this)->GetJointGroupForName(type,
                                                                           name);
  }
  idJointGroup* AddJointGroup(const char* groupName,
                              idJointGroup::jointGroup_t type);
  void RemoveJointGroup(const char* groupName,
                        idJointGroup::jointGroup_t type);
  const idJointGroup* GetJointGroupForJoint(idJointGroup::jointGroup_t type,
                                             idJointIndex_t jointIndex) const;
  int GetJointGroupIndexForJoint(idJointGroup::jointGroup_t type,
                                 idJointIndex_t jointIndex) const;
  jointGroupHandle_t GetJointGroupHandle(idJointGroup::jointGroup_t type,
                                         const char* name) const;
  jointGroupHandle_t GetJointGroupHandle(idJointGroup::jointGroup_t type,
                                         idJointIndex_t jointIndex) const;
  int NumJointGroupsForType(idJointGroup::jointGroup_t type) const;
  const idJointGroup* GetJointGroupForIndex(idJointGroup::jointGroup_t type,
                                             int index) const;
  int GetJointGroupIndex(idJointGroup::jointGroup_t type,
                         const char* name) const;

  idList<idJointGroup*, 30> jointGroups;
  idList<idJointGroup*, 30> jointGroupsByType[idJointGroup::JOINTGROUP_MAX];

  static int curHandle;

private:
  bool AddJointGroup(const idJointGroup& group);
};

static_assert(sizeof(idJointGroup::jointGroupArgs_t) == 16);
static_assert(sizeof(idJointGroup) == 120);
static_assert(offsetof(idJointGroup, type) == 0x00);
static_assert(offsetof(idJointGroup, groupName) == 0x08);
static_assert(offsetof(idJointGroup, joints) == 0x10);
static_assert(offsetof(idJointGroup, jointOffsets) == 0x28);
static_assert(offsetof(idJointGroup, jointScalars) == 0x40);
static_assert(offsetof(idJointGroup, args) == 0x58);
static_assert(offsetof(idJointGroup, groupOffset) == 0x68);
static_assert(offsetof(idJointGroup, handle) == 0x74);
static_assert(sizeof(idJointGroupCollection) == 0x150);
static_assert(offsetof(idJointGroupCollection, jointGroupsByType) == 0x18);
