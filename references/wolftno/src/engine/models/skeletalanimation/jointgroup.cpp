#include "engine/models/skeletalanimation/jointgroup.h"

#include "engine/common/surfacetypes.h"
#include "engine/models/skeletalanimation/md6util.h"
#include "shared/idlib/text/parser.h"

#include <algorithm>

const char* idJointGroup::jointGroupNames[JOINTGROUP_MAX] = {
    "damageGroup",          "painGroup",        "twitchGroup",
    "deathGroup",           "deathCollisionGroup", "limblossGroup",
    "headTrackingGroup",    "focusGroup",       "orientationGroup",
    "hitTestGroup",         "eyeGroup",         "feetGroup",
    "boundsGroup"};

const char* idJointGroup::jointGroupNiceNames[JOINTGROUP_MAX] = {
    "Location-based Damage", "Pain Anims",      "Twitch Anims",
    "Death Anims",           "Death Collision", "Limb-loss",
    "Head Tracking",         "Focus Joint",     "Orientation Joint",
    "Hit Test",              "Eyes",            "Feet",
    "Bounds"};

const char* idJointGroup::jointGroupScalarNames[JOINTGROUP_MAX] = {
    "", "", "", "", "", "index", "weight", "", "", "radius", "", "",
    "radius"};

int idJointGroupCollection::curHandle = 0;
jointGroupHandle_t NULL_JOINT_GROUP{};

namespace {

bool IsValidType(idJointGroup::jointGroup_t type) {
  return type >= idJointGroup::JOINTGROUP_DAMAGE &&
         type < idJointGroup::JOINTGROUP_MAX;
}

const char* DeclName(const idDeclMD6* decl) {
  return decl != nullptr ? decl->name.str : "";
}

idJointIndex_t JointIndexForName(const idDeclMD6* decl, const char* name) {
  if (decl == nullptr || decl->model == nullptr ||
      decl->model->skeleton == nullptr)
    return idJointIndex_t(-1);
  return decl->model->skeleton->GetJointIndex(name);
}

const char* JointNameForIndex(const idDeclMD6* decl, idJointIndex_t index) {
  if (decl == nullptr || decl->model == nullptr ||
      decl->model->skeleton == nullptr || index.value < 0 ||
      index.value >= decl->model->skeleton->NumJoints())
    return "";
  return decl->model->skeleton->GetJointName(index.value);
}

struct SurfaceName {
  const char* name;
  int value;
};

// Retail strips "SURFTYPE_" and every "_" from reflected enum names before
// the case-insensitive comparison. These entries are that exact normalization.
constexpr SurfaceName kSurfaceNames[] = {
    {"INVALID", SURFTYPE_INVALID},
    {"NONE", SURFTYPE_NONE},
    {"METAL", SURFTYPE_METAL},
    {"STONE", SURFTYPE_STONE},
    {"FLESH", SURFTYPE_FLESH},
    {"WOOD", SURFTYPE_WOOD},
    {"CARDBOARD", SURFTYPE_CARDBOARD},
    {"LIQUID", SURFTYPE_LIQUID},
    {"GLASS", SURFTYPE_GLASS},
    {"PLASTIC", SURFTYPE_PLASTIC},
    {"ASPHALT", SURFTYPE_ASPHALT},
    {"DIRT", SURFTYPE_DIRT},
    {"CONCRETE", SURFTYPE_CONCRETE},
    {"FOLIAGE", SURFTYPE_FOLIAGE},
    {"LINOLEUM", SURFTYPE_LINOLEUM},
    {"FABRIC", SURFTYPE_FABRIC},
    {"RUBBER", SURFTYPE_RUBBER},
    {"ROCK", SURFTYPE_ROCK},
    {"STEAMPIPE", SURFTYPE_STEAM_PIPE},
    {"WATERPIPE", SURFTYPE_WATER_PIPE},
    {"ARMOR", SURFTYPE_ARMOR},
    {"SLUDGE", SURFTYPE_SLUDGE},
    {"IMPNEST", SURFTYPE_IMP_NEST},
    {"HOLLOWMETAL", SURFTYPE_HOLLOW_METAL},
    {"FLESHPLAYER", SURFTYPE_FLESH_PLAYER},
    {"FLESHDARKANGEL", SURFTYPE_FLESH_DARKANGEL},
    {"BLOODPOOL", SURFTYPE_BLOOD_POOL},
    {"ASH", SURFTYPE_ASH},
    {"FLESHMUTANT", SURFTYPE_FLESH_MUTANT},
    {"THICKPADDING", SURFTYPE_THICK_PADDING},
    {"PLAYERAPCMETAL", SURFTYPE_PLAYER_APC_METAL},
    {"RICKETYMETAL", SURFTYPE_RICKETY_METAL},
    {"RICKETYWOOD", SURFTYPE_RICKETY_WOOD},
    {"SHEETMETAL", SURFTYPE_SHEET_METAL},
    {"PORCELAIN", SURFTYPE_PORCELAIN},
    {"PLASTER", SURFTYPE_PLASTER},
    {"VENTMETAL", SURFTYPE_VENT_METAL},
    {"ELECTRONICS", SURFTYPE_ELECTRONICS},
    {"OILPIPE", SURFTYPE_OIL_PIPE},
    {"PANZERMETAL", SURFTYPE_PANZER_METAL},
    {"BREAKABLEARMOR", SURFTYPE_BREAKABLE_ARMOR},
    {"ELECTRICSHIELD", SURFTYPE_ELECTRIC_SHIELD},
};

} // namespace

idJointGroup::idJointGroup(jointGroup_t groupType)
    : type(groupType), recoveredTypePadding(0), groupName(), joints(),
      jointOffsets(), jointScalars(), args(), groupOffset{}, handle{} {}

idJointGroup::idJointGroup(const char* name, jointGroup_t groupType,
                           const jointGroupHandle_t& groupHandle)
    : type(groupType), recoveredTypePadding(0), groupName(name), joints(),
      jointOffsets(), jointScalars(), args(), groupOffset{},
      handle(groupHandle) {}

idJointGroup::idJointGroup(const idJointGroup& other,
                           const jointGroupHandle_t& groupHandle)
    : type(other.type), recoveredTypePadding(0), groupName(other.groupName),
      joints(), jointOffsets(), jointScalars(), args(other.args),
      handle(groupHandle) {
  joints = other.joints;
  jointOffsets = other.jointOffsets;
  jointScalars = other.jointScalars;
}

idJointGroup::~idJointGroup() = default;

bool idJointGroup::IsJointInGroup(idJointIndex_t joint) const {
  for (int index = 0; index < joints.Num(); ++index) {
    if (joints[index].value == joint.value)
      return true;
  }
  return false;
}

int idJointGroup::AddJoint(idJointIndex_t joint) {
  if (!AllowMultipleJoints() && joints.Num() > 0)
    return 0;
  if (!AllowDuplicateJoints()) {
    for (int index = 0; index < joints.Num(); ++index) {
      if (joints[index].value == joint.value)
        return index;
    }
  }
  return joints.Append(joint);
}

void idJointGroup::RemoveJoint(int index) {
  if (index < 0 || index >= joints.Num())
    return;
  joints.RemoveIndex(index);
  if (index < jointOffsets.Num())
    jointOffsets.RemoveIndex(index);
  if (index < jointScalars.Num())
    jointScalars.RemoveIndex(index);
}

void idJointGroup::ParseSurfType(idParser& source, const idDeclMD6*,
                                 int& loadErrors) {
  idToken token;
  source.ReadToken(token);
  for (const SurfaceName& surface : kSurfaceNames) {
    idStr test(surface.name);
    if (test.Icmp(token.data) == 0) {
      args.surfType = surface.value;
      return;
    }
  }
  source.Warning("Unknown surface type %s", token.data);
  ++loadErrors;
}

void idJointGroup::ParseArgs(idParser& source, const idDeclMD6* decl,
                             int& loadErrors) {
  idToken token;
  source.ExpectTokenString("{", token);
  while (source.ReadToken(token)) {
    if (token == "}")
      break;
    if (token == "vec3") {
      source.Parse1DMatrix(3, &groupOffset.x);
    } else if (token == "surfType") {
      ParseSurfType(source, decl, loadErrors);
    } else if (token == "userName") {
      source.ExpectTokenType(TT_STRING, 0, token);
      args.userName.Set(token.data);
    } else {
      source.Warning(
          "Unknown token '%s' while parsing args for joint group '%s' in decl '%s'",
          token.data, groupName.str, DeclName(decl));
      ++loadErrors;
    }
  }
}

void idJointGroup::ParseInfo(idParser& source, int index,
                             const idDeclMD6* decl, int& loadErrors) {
  idToken token;
  source.ExpectTokenString("{", token);
  while (source.ReadToken(token)) {
    if (token == "}")
      break;
    if (token == jointGroupScalarNames[type]) {
      const float scalar = source.ParseFloat();
      jointScalars.SetNum(index + 1, 1.0f);
      jointScalars[index] = scalar;
    } else if (token == "offset") {
      idVec3 offset{};
      source.Parse1DMatrix(3, &offset.x);
      jointOffsets.SetNum(index + 1, idVec3{});
      jointOffsets[index] = offset;
    } else if (token == "surfType") {
      ParseSurfType(source, decl, loadErrors);
    } else {
      source.Warning(
          "Unknown token '%s' while parsing joint scalar for joint group '%s', in decl '%s'",
          token.data, groupName.str, DeclName(decl));
      ++loadErrors;
    }
  }
}

void idJointGroup::Parse(idParser& source, const idDeclMD6* decl,
                         int& loadErrors) {
  idToken token;
  idToken peek;
  source.ExpectTokenType(TT_STRING, 0, token);
  groupName.Set(token.data);
  source.ExpectTokenString("{", token);
  while (source.ReadToken(token)) {
    if (token == "}")
      break;
    if (token == "args" && source.PeekTokenString("{")) {
      ParseArgs(source, decl, loadErrors);
      continue;
    }

    idStr jointName(token);
    jointName.ToLower();
    const idJointIndex_t jointIndex = JointIndexForName(decl, jointName.data);
    if (jointIndex.value < 0) {
      source.Warning("Invalid joint name '%s' in joint group '%s' for decl '%s'",
                     jointName.data, groupName.str, DeclName(decl));
      if (source.CheckTokenString("{"))
        source.SkipBracedSection(false);
      ++loadErrors;
      continue;
    }

    idJointIndex_t& added = joints.Alloc();
    added.value = jointIndex.value;
    if (groupOffset.x != 0.0f || groupOffset.y != 0.0f ||
        groupOffset.z != 0.0f) {
      jointOffsets.SetNum(joints.Num(), idVec3{});
      jointOffsets[joints.Num() - 1] = groupOffset;
    }
    if (source.PeekTokenString("{"))
      ParseInfo(source, joints.Num() - 1, decl, loadErrors);
  }
}

void idJointGroup::Copy(const idDeclMD6* myDecl,
                        const idDeclMD6* otherDecl,
                        const idJointGroup& other) {
  type = other.type;
  groupName = other.groupName;
  args = other.args;
  for (int index = 0; index < other.joints.Num(); ++index) {
    const char* name = JointNameForIndex(otherDecl, other.joints[index]);
    const idJointIndex_t jointIndex = JointIndexForName(myDecl, name);
    if (jointIndex.value >= 0)
      joints.Append(jointIndex);
  }
  jointOffsets = other.jointOffsets;
  jointScalars = other.jointScalars;
}

idJointGroupCollection::idJointGroupCollection()
    : jointGroups(), jointGroupsByType{} {}

idJointGroupCollection::~idJointGroupCollection() { Free(); }

const idJointGroup* idJointGroupCollection::GetJointGroup(
    const jointGroupHandle_t& requestedHandle) const {
  for (int index = 0; index < jointGroups.Num(); ++index) {
    if (jointGroups[index]->handle.value == requestedHandle.value)
      return jointGroups[index];
  }
  return nullptr;
}

idJointGroup* idJointGroupCollection::GetJointGroupForName(
    idJointGroup::jointGroup_t type, const char* name) {
  if (!IsValidType(type))
    return nullptr;
  for (int index = 0; index < jointGroupsByType[type].Num(); ++index) {
    idJointGroup* group = jointGroupsByType[type][index];
    if (idStr::Icmp(group->groupName.str, name) == 0)
      return group;
  }
  return nullptr;
}

int idJointGroupCollection::GetJointGroupIndex(
    idJointGroup::jointGroup_t type, const char* name) const {
  if (!IsValidType(type))
    return -1;
  for (int index = 0; index < jointGroupsByType[type].Num(); ++index) {
    if (idStr::Icmp(jointGroupsByType[type][index]->groupName.str, name) == 0)
      return index;
  }
  return -1;
}

int idJointGroupCollection::NumJointGroupsForType(
    idJointGroup::jointGroup_t type) const {
  return IsValidType(type) ? jointGroupsByType[type].Num() : 0;
}

const idJointGroup* idJointGroupCollection::GetJointGroupForIndex(
    idJointGroup::jointGroup_t type, int index) const {
  if (!IsValidType(type) || index < 0 ||
      index >= jointGroupsByType[type].Num())
    return nullptr;
  return jointGroupsByType[type][index];
}

const idJointGroup* idJointGroupCollection::GetJointGroupForJoint(
    idJointGroup::jointGroup_t type, idJointIndex_t jointIndex) const {
  if (!IsValidType(type))
    return nullptr;
  for (int index = 0; index < jointGroupsByType[type].Num(); ++index) {
    const idJointGroup* group = jointGroupsByType[type][index];
    if (group->IsJointInGroup(jointIndex))
      return group;
  }
  return nullptr;
}

int idJointGroupCollection::GetJointGroupIndexForJoint(
    idJointGroup::jointGroup_t type, idJointIndex_t jointIndex) const {
  if (!IsValidType(type))
    return -1;
  for (int index = 0; index < jointGroupsByType[type].Num(); ++index) {
    if (jointGroupsByType[type][index]->IsJointInGroup(jointIndex))
      return index;
  }
  return -1;
}

jointGroupHandle_t idJointGroupCollection::GetJointGroupHandle(
    idJointGroup::jointGroup_t type, const char* name) const {
  const idJointGroup* group = GetJointGroupForName(type, name);
  return group != nullptr ? group->handle : NULL_JOINT_GROUP;
}

jointGroupHandle_t idJointGroupCollection::GetJointGroupHandle(
    idJointGroup::jointGroup_t type, idJointIndex_t jointIndex) const {
  const idJointGroup* group = GetJointGroupForJoint(type, jointIndex);
  return group != nullptr ? group->handle : NULL_JOINT_GROUP;
}

void idJointGroupCollection::Free() {
  for (idList<idJointGroup*, 30>& groups : jointGroupsByType)
    groups.ClearFree();
  jointGroups.DeleteContents();
}

void idJointGroupCollection::Condense() {
  jointGroups.ResizeForPort(jointGroups.Num());
  for (idList<idJointGroup*, 30>& groups : jointGroupsByType)
    groups.ResizeForPort(groups.Num());
}

bool idJointGroupCollection::AddJointGroup(const idJointGroup& source) {
  if (GetJointGroupForName(source.type, source.groupName.str) != nullptr)
    return false;
  const jointGroupHandle_t newHandle(curHandle);
  curHandle = curHandle == -1 ? 1 : curHandle + 1;
  idJointGroup* group = new idJointGroup(source, newHandle);
  jointGroups.Append(group);
  jointGroupsByType[group->type].Append(group);
  return true;
}

idJointGroup* idJointGroupCollection::AddJointGroup(
    const char* groupName, idJointGroup::jointGroup_t type) {
  idJointGroup* existing = GetJointGroupForName(type, groupName);
  if (existing != nullptr)
    return existing;
  const jointGroupHandle_t newHandle(curHandle);
  curHandle = curHandle == -1 ? 1 : curHandle + 1;
  idJointGroup* group = new idJointGroup(groupName, type, newHandle);
  jointGroups.Append(group);
  jointGroupsByType[type].Append(group);
  return group;
}

void idJointGroupCollection::RemoveJointGroup(
    const char* groupName, idJointGroup::jointGroup_t type) {
  idJointGroup* group = GetJointGroupForName(type, groupName);
  if (group == nullptr)
    return;
  for (int index = 0; index < jointGroupsByType[type].Num(); ++index) {
    if (jointGroupsByType[type][index] == group) {
      jointGroupsByType[type].RemoveIndex(index);
      break;
    }
  }
  for (int index = 0; index < jointGroups.Num(); ++index) {
    if (jointGroups[index] == group) {
      jointGroups.RemoveIndex(index);
      break;
    }
  }
  delete group;
}

bool idJointGroupCollection::JointGroupIsInherited(
    const idDeclMD6*, const idDeclMD6*,
    const idJointGroupCollection* parent, const idJointGroup* group) const {
  return parent != nullptr && group != nullptr &&
         parent->GetJointGroupForName(group->type, group->groupName.str) !=
             nullptr;
}

void idJointGroupCollection::DuplicateInherited(
    const idDecl* myDecl, const idDecl* parentDecl,
    const idJointGroupCollection* parentCollection) {
  if (parentCollection == nullptr)
    return;
  const auto* myMD6 = reinterpret_cast<const idDeclMD6*>(myDecl);
  const auto* parentMD6 = reinterpret_cast<const idDeclMD6*>(parentDecl);
  for (int index = 0; index < parentCollection->jointGroups.Num(); ++index) {
    const idJointGroup& inherited = *parentCollection->jointGroups[index];
    if (GetJointGroupForName(inherited.type, inherited.groupName.str) != nullptr)
      continue;
    idJointGroup* group = AddJointGroup(inherited.groupName.str, inherited.type);
    if (myMD6 != nullptr)
      group->Copy(myMD6, parentMD6, inherited);
  }
}

int idJointGroupCollection::Parse(idParser& source, const idDeclMD6* decl) {
  int loadErrors = 0;
  idToken token;
  source.ExpectTokenString("{", token);
  while (source.ReadToken(token)) {
    if (token == "}")
      break;
    if (token == "{") {
      source.SkipBracedSection(false);
      continue;
    }
    if (token == "jointGroup")
      continue;

    idJointGroup::jointGroup_t type = idJointGroup::JOINTGROUP_DAMAGE;
    int typeIndex = 0;
    while (typeIndex < idJointGroup::JOINTGROUP_MAX &&
           idStr::Icmp(token.data,
                       idJointGroup::jointGroupNames[typeIndex]) != 0 &&
           (typeIndex != idJointGroup::JOINTGROUP_EYES ||
            idStr::Icmp(token.data, "misc") != 0)) {
      ++typeIndex;
      type = static_cast<idJointGroup::jointGroup_t>(type + 1);
    }
    if (typeIndex < idJointGroup::JOINTGROUP_MAX) {
      idJointGroup loader(type);
      loader.Parse(source, decl, loadErrors);
      AddJointGroup(loader);
      continue;
    }

    if (token == "radiusDamageGroup" || token == "headTrackingIKGroup" ||
        token == "reparentGroup" || token == "upgradeGroup" ||
        token == "autoblendGroup") {
      source.SkipBracedSection(true);
    } else {
      source.Warning(
          "Unknown token '%s' while parsing joint group collection for modeldef '%s'",
          token.data, DeclName(decl));
    }
  }
  return loadErrors;
}

template idList<idJointGroup*, 30>::idList();
template void idList<idJointGroup*, 30>::DeleteContents<idJointGroup*>();
template bool idList<idVec3, 30>::SetNum(int, const idVec3&);
template idJointIndex_t& idList<idJointIndex_t, 30>::Alloc();
