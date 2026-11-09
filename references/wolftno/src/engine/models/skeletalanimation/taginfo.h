#pragma once

#include "engine/animation/md6.h"
#include "shared/idlib/containers/list.h"
#include "shared/idlib/math/vector.h"
#include "shared/idlib/text/atomicstring.h"
#include "shared/idlib/text/parser.h"
#include "shared/idlib/text/str.h"

#include <cstddef>
#include <cstdint>

enum invalidTagJointHandle_t { INVALID_TAG_JOINT_HANDLE = -1 };

template <typename T, typename InvalidTag, T InvalidValue>
class idTagHandle {
public:
  T value = InvalidValue;
};

using idTagJointHandle =
    idTagHandle<unsigned short, invalidTagJointHandle_t, 65535>;

struct tagData_t {
  idVec3 trans{};
  idQuat rot{};
  idTagJointHandle parentJoint{};
  std::uint16_t padding;

  bool IsValid() const { return parentJoint.value != 0xffff; }
};

using idTagJointLookupCallback = unsigned short (*)(const char* jointName);
extern idTagJointLookupCallback tagJointLookupCallback;
void SetTagJointLookupCallback(idTagJointLookupCallback callback);

class idTagInfo {
public:
  int Parse(idParser& source, const idDeclMD6* decl);
  bool Equal(const idTagInfo& other) const {
    return tagName.str == other.tagName.str &&
           transEqual(other.tagData);
  }
  bool operator==(const idTagInfo& other) const { return Equal(other); }
  bool operator!=(const idTagInfo& other) const { return !Equal(other); }

  idAtomicString tagName;
  tagData_t tagData;

private:
  bool transEqual(const tagData_t& other) const;
};

class idPropsCollection;

class idPropInfo {
public:
  static const char* INFO_PROP_NAME;
  static const char* CAMERA_PROP_NAME;
  static const char* LOCALLIGHTPOSITION_TAG_NAME;

  idPropInfo() = default;
  idPropInfo(const idPropInfo& other) { *this = other; }
  idPropInfo& operator=(const idPropInfo& other);
  int Parse(idParser& source, const idDeclMD6* decl);
  bool Equal(const idPropInfo& other) const;
  const idTagInfo& GetTag(int index) const { return tags[index]; }
  idTagInfo& GetTag(int index) { return tags[index]; }

  idStrStatic<256> propName;
  idList<idTagInfo, 82> tags;
};

class idPropsCollection {
public:
  idPropsCollection();
  idPropsCollection(const idPropsCollection& other) : idPropsCollection() {
    props = other.props;
  }
  ~idPropsCollection();

  void Free();
  int Parse(idParser& source, const idDeclMD6* decl);
  const tagData_t& GetTag(const char* propName, const char* tagName) const;
  const tagData_t& GetTagByIndices(int propIndex, int tagIndex) const;
  int GetNumProps() const { return props.Num(); }
  const idPropInfo& GetPropA(int index) const { return props[index]; }
  idPropInfo& GetPropA(int index) { return props[index]; }
  void RemovePropA(int index) { props.RemoveIndex(index); }
  void AddProp(const char* name) {
    idPropInfo* prop = props.Alloc();
    if (prop != nullptr)
      prop->propName = name;
  }
  void DuplicateInherited(const idPropsCollection* other);
  const idPropInfo* FindProp(const char* name) const {
    const int index = FindPropIndex(name);
    return index >= 0 ? &props[index] : nullptr;
  }
  int FindPropIndex(const char* propName) const;
  const idTagInfo* FindTag(int propIndex, const char* tagName) const;
  int FindTagIndex(int propIndex, const char* tagName) const;

  idPropsCollection& operator=(const idPropsCollection& other) {
    props = other.props;
    return *this;
  }

  idList<idPropInfo, 82> props;
};

static_assert(sizeof(idTagJointHandle) == 2);
static_assert(sizeof(tagData_t) == 0x20);
static_assert(offsetof(tagData_t, rot) == 0x0C);
static_assert(offsetof(tagData_t, parentJoint) == 0x1C);
static_assert(sizeof(idTagInfo) == 0x28);
static_assert(offsetof(idTagInfo, tagData) == 0x08);
static_assert(sizeof(idPropInfo) == 0x140);
static_assert(offsetof(idPropInfo, tags) == 0x128);
static_assert(sizeof(idPropsCollection) == 0x18);
