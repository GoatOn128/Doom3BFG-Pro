#include "engine/models/skeletalanimation/taginfo.h"

#include "shared/idlib/lib.h"

#include <cstring>

const char* idPropInfo::INFO_PROP_NAME = "info";
const char* idPropInfo::CAMERA_PROP_NAME = "camera";
const char* idPropInfo::LOCALLIGHTPOSITION_TAG_NAME = "localLightPosition";

namespace {

const tagData_t& BlankTag() {
  static const tagData_t blank{};
  return blank;
}

void ShrinkList(idList<idTagInfo, 82>& list) {
  list.ResizeForPort(list.Num());
}

void ShrinkList(idList<idPropInfo, 82>& list) {
  list.ResizeForPort(list.Num());
}

} // namespace

bool idTagInfo::transEqual(const tagData_t& other) const {
  return std::memcmp(&tagData, &other, sizeof(tagData_t)) == 0;
}

int idTagInfo::Parse(idParser& source, const idDeclMD6* decl) {
  tagData.trans = vec3_origin;
  tagData.rot = {0.0f, 0.0f, 0.0f, 0.0f};
  idToken token;
  source.ExpectTokenType(TT_STRING, 0, token);
  tagName.Set(token.data);
  source.ExpectTokenString("{");
  while (source.ReadToken(token)) {
    if (token == "}")
      break;
    if (token == "offset" || token == "trans") {
      source.Parse1DMatrix(3, &tagData.trans.x);
    } else if (token == "quat" || token == "rot") {
      source.Parse1DMatrix(4, &tagData.rot.x);
      tagData.rot.Normalize();
    } else if (token == "actor_joint" || token == "parent") {
      source.ExpectTokenType(TT_STRING, 0, token);
      token.ToLower();
      const unsigned short handle = tagJointLookupCallback != nullptr
                                        ? tagJointLookupCallback(token.data)
                                        : static_cast<unsigned short>(0xffff);
      if (handle == 0xffff) {
        idLib::Warning("%s - joint <%s> not found on tag <%s> for <%s>",
                       "idTagInfo::FindJoint", token.data, tagName.str,
                       decl != nullptr ? decl->name.str : "");
      }
      tagData.parentJoint.value = handle;
    } else if (token == "movemode") {
      source.ExpectTokenType(TT_STRING, 0, token);
    } else {
      source.Error("Unknown token '%s' in modeldef '%s'", token.data,
                   decl != nullptr ? decl->name.str : "");
    }
  }
  return 0;
}

const tagData_t& idPropsCollection::GetTag(const char* propName,
                                           const char* tagName) const {
  for (int propIndex = 0; propIndex < props.Num(); ++propIndex) {
    const idPropInfo& prop = props[propIndex];
    if (idStr::Icmp(prop.propName.data, propName) != 0)
      continue;
    for (int tagIndex = 0; tagIndex < prop.tags.Num(); ++tagIndex) {
      if (idStr::Icmp(prop.tags[tagIndex].tagName.str, tagName) == 0)
        return prop.tags[tagIndex].tagData;
    }
    break;
  }
  return BlankTag();
}

const tagData_t& idPropsCollection::GetTagByIndices(int propIndex,
                                                    int tagIndex) const {
  if (propIndex >= 0 && propIndex < props.Num() && tagIndex >= 0 &&
      tagIndex < props[propIndex].tags.Num())
    return props[propIndex].tags[tagIndex].tagData;
  return BlankTag();
}

int idPropsCollection::FindPropIndex(const char* propName) const {
  for (int index = 0; index < props.Num(); ++index) {
    if (idStr::Icmp(props[index].propName.data, propName) == 0)
      return index;
  }
  return -1;
}

const idTagInfo* idPropsCollection::FindTag(int propIndex,
                                            const char* tagName) const {
  if (tagName == nullptr || *tagName == '\0')
    return nullptr;
  const idPropInfo& prop = props[propIndex];
  for (int index = 0; index < prop.tags.Num(); ++index) {
    if (idStr::Icmp(prop.tags[index].tagName.str, tagName) == 0)
      return &prop.tags[index];
  }
  return nullptr;
}

int idPropsCollection::FindTagIndex(int propIndex, const char* tagName) const {
  if (tagName == nullptr || *tagName == '\0')
    return -1;
  const idPropInfo& prop = props[propIndex];
  for (int index = 0; index < prop.tags.Num(); ++index) {
    if (idStr::Icmp(prop.tags[index].tagName.str, tagName) == 0)
      return index;
  }
  return -1;
}

idPropInfo& idPropInfo::operator=(const idPropInfo& other) {
  if (this != &other) {
    propName = other.propName;
    tags = other.tags;
  }
  return *this;
}

bool idPropInfo::Equal(const idPropInfo& other) const {
  if (idStr::Cmp(propName.data, other.propName.data) != 0 ||
      tags.Num() != other.tags.Num())
    return false;
  for (int index = 0; index < tags.Num(); ++index) {
    if (tags[index] != other.tags[index])
      return false;
  }
  return true;
}

int idPropInfo::Parse(idParser& source, const idDeclMD6* decl) {
  int errors = 0;
  idToken token;
  source.ExpectTokenType(TT_STRING, 0, token);
  propName = token;
  if (source.CheckTokenType(TT_NUMBER, 0, token)) {
    token.NumberValue();
    if (static_cast<int>(token.intvalue) > tags.size)
      tags.ResizeForPort(static_cast<int>(token.intvalue));
  }
  source.ExpectTokenString("{");
  while (source.ReadToken(token)) {
    if (token == "}")
      break;
    if (token == "tag") {
      idTagInfo* tag = tags.Alloc();
      if (tag != nullptr)
        errors += tag->Parse(source, decl);
    } else {
      source.Error("Unknown token '%s' while parsing prop info for modeldef '%s'",
                   token.data, decl != nullptr ? decl->name.str : "");
    }
  }
  ShrinkList(tags);
  return errors;
}

void idPropsCollection::Free() { props.ClearFree(); }

idPropsCollection::idPropsCollection() = default;

idPropsCollection::~idPropsCollection() { Free(); }

void idPropsCollection::DuplicateInherited(const idPropsCollection* other) {
  if (other == nullptr)
    return;
  for (int otherPropIndex = 0; otherPropIndex < other->props.Num();
       ++otherPropIndex) {
    const idPropInfo& inheritedProp = other->props[otherPropIndex];
    const int localPropIndex = FindPropIndex(inheritedProp.propName.data);
    if (localPropIndex < 0) {
      props.Append(inheritedProp);
      continue;
    }
    idPropInfo& localProp = props[localPropIndex];
    const int originalTagCount = localProp.tags.Num();
    for (int inheritedTagIndex = 0;
         inheritedTagIndex < inheritedProp.tags.Num(); ++inheritedTagIndex) {
      const idTagInfo& inheritedTag = inheritedProp.tags[inheritedTagIndex];
      int matchIndex = 0;
      while (matchIndex < originalTagCount &&
             inheritedTag.tagName.str != localProp.tags[matchIndex].tagName.str)
        ++matchIndex;
      if (matchIndex == originalTagCount)
        localProp.tags.Append(inheritedTag);
    }
  }
}

int idPropsCollection::Parse(idParser& source, const idDeclMD6* decl) {
  int errors = 0;
  idToken token;
  if (source.CheckTokenType(TT_NUMBER, 0, token)) {
    token.NumberValue();
    if (static_cast<int>(token.intvalue) > props.size)
      props.ResizeForPort(static_cast<int>(token.intvalue));
  }
  source.ExpectTokenString("{");
  while (source.ReadToken(token)) {
    if (token == "}")
      break;
    if (token == "prop") {
      idPropInfo* prop = props.Alloc();
      if (prop != nullptr)
        errors += prop->Parse(source, decl);
    } else {
      source.Error(
          "Unknown token '%s' while loading props collection for modeldef '%s'",
          token.data, decl != nullptr ? decl->name.str : "");
    }
  }
  ShrinkList(props);
  return errors;
}

template idList<idTagInfo, 82>&
idList<idTagInfo, 82>::operator=(const idList<idTagInfo, 82>&);
template bool idList<idTagInfo, 82>::Resize(int);
template int idList<idTagInfo, 82>::Append(const idTagInfo&);
template idTagInfo* idList<idTagInfo, 82>::Alloc();
template bool idList<idPropInfo, 82>::Resize(int);
template int idList<idPropInfo, 82>::Append(const idPropInfo&);
template idPropInfo* idList<idPropInfo, 82>::Alloc();
