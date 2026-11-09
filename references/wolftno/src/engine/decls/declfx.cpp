#include "declfx.h"

#include <algorithm>
#include <cstdint>

namespace {
constexpr const char* fxActionTypeDesc[] = {
    "light",          "particle",     "decal",       "decal2",      "model",
    "sound",          "screenShake",  "controllerShake", "wind",    "renderParm",
    "envOverride",    "envChange",    "flare",       "radialBlur",  "ribbon",
    "fadeParent",     "godray"};

template <typename T, int Tag> void ReleaseList(idList<T, Tag>& values) {
  if ((values.listStatic == 0 || values.listStatic == 2) && values.list != nullptr)
    delete[] values.list;
  values.list = nullptr;
  values.num = 0;
  values.size = 0;
  values.listStatic = 0;
}

bool HasCondition(const idFXSingleAction& action, fxCondition_t condition) {
  if (condition == FX_NONE)
    return true;
  for (int i = 0; i < action.startCondition.Num(); ++i) {
    if (action.startCondition[i] == condition)
      return true;
  }
  return false;
}

// idDeclParticle is owned by the particle declaration folder.  The shipping
// inline accessor reads its millisecond duration at this PDB-confirmed offset.
int ParticleDuration(const idDeclParticle* particle) {
  return particle == nullptr
             ? 0
             : *reinterpret_cast<const int*>(reinterpret_cast<const std::uint8_t*>(particle) + 272);
}
} // namespace

idDeclInfoTemplate<idDeclFX> idDeclFX::resourceList("fx", "idDeclFX", "fx", false);

const char* idDeclFX::FXActionTypeToName(int type) {
  return type >= 0 && type < FX_MAX ? fxActionTypeDesc[type] : "FX_MAX (invalid?)";
}

const char* idDeclFX::FXConditionToName(int condition) {
  if (declTypeInfoTools != nullptr) {
    if (const idEnumInfo* info = declTypeInfoTools->FindEnumInfo("fxCondition_t")) {
      if (const idEnumValueInfo* value = declTypeInfoTools->FindEnumValueInfo(*info, condition))
        return value->name.data;
    }
  }
  return "";
}

idFXSingleAction::idFXSingleAction() {
  godrayParms.color = {-1.0f, -1.0f, -1.0f, -1.0f};
  Default();
}

void idFXSingleAction::Default() {
  type = FX_MAX;
  name.Set("<none>");
  filterHighViolence = false;
  multiTagUseType = FX_MULTI_TAG_USE_RND;
  implicit = false;
  ReleaseList(tagNames);
  ReleaseList(startCondition);
  ReleaseList(stopCondition);
  ReleaseList(extraCondition);
  delay = {0.0f, 0.0f};
  duration = 2.0f;
  restart = false;
  looping = false;
  fadeInTime = 0.0f;
  fadeOutTime = 0.0f;
  noshadows = true;
  originType = FX_ORG_START_POS;
  rotationType = FX_ROT_START_AXIS;
  offset = {0.0f, 0.0f, 0.0f};
  rndRotX = {0.0f, 0.0f};
  rndRotY = {0.0f, 0.0f};
  rndRotZ = {0.0f, 0.0f};
  rotOffsetAngles = {0.0f, 0.0f, 0.0f};
  explicitAngles = {0.0f, 0.0f, 0.0f};
  fadeStartDistance = 0.0f;
  distance = -1.0f;
  size = 1.0f;
  color = {1.0f, 1.0f, 1.0f, 1.0f};
  colorTableRGB = nullptr;
  colorTableA = nullptr;
  customTable1 = nullptr;
  customTable2 = nullptr;
  customRenderParm = nullptr;
  screenShakeParms = {0.0f, 1.0f, {0.2f, 0.2f, 0.2f}, {0.2f, 0.2f, 0.2f}};
  controllerShakeParms = {0.5f, 1.0f, 200.0f, 100.0f, nullptr};
  particleParms = {nullptr, 1.0f, 0.0f, 1, 0.0f, -1.0f, false, false, false};
  lightParms = {{0.0f, 0.0f, 0.0f}, 1.0f, nullptr};
  godrayParms = {nullptr, {1.0f, 1.0f, 1.0f, 1.0f}, 0.015f, 512, 128};
  flareParms = {{0.0f, 0.0f, 0.0f}, nullptr, false};
  radialBlurParms.maxScale = 0.3f;
  decalParms = {0.0f, 8.0f, nullptr};
  windParms = {0.0f, 0.1f, {10.0f, 30.0f}};
  soundParms = {SND_CHANNEL_ANY, nullptr};
  rotateTablePitch = nullptr;
  rotateTableYaw = nullptr;
  rotateTableRoll = nullptr;
  axis = mat3_identity;
  rotOffset = mat3_identity;
  flareParms.declFlare = nullptr;
  envParms.declEnv = nullptr;
  envParms.envRenderParm.Clear();
  renderParmParms.declRenderParm = nullptr;
  modelParms.customMaterial = nullptr;
  ribbonParms.declRibbon = nullptr;
  triggered = false;
}

idDeclFX::idDeclFX() : changeId(-1) {
  resourceListPtr = &resourceList;
  FreeData();
}

idDeclFX::~idDeclFX() {
  FreeData();
}

void idDeclFX::FreeData() {
  ReleaseList(editEvents);
  ReleaseList(events);
}

idDeclInfo* idDeclFX::GetDeclInfo() const {
  return &resourceList;
}

int idDeclFX::GetMaxDuration(fxCondition_t startCondition) const {
  int result = 0;
  for (int i = 0; i < events.Num(); ++i) {
    const idFXSingleAction& action = events[i];
    if (!HasCondition(action, startCondition))
      continue;
    int duration = static_cast<int>(action.duration * 1000.0f);
    if (duration <= 0 && action.type == FX_PARTICLE)
      duration = ParticleDuration(action.particleParms.declPrt);
    duration += static_cast<int>(action.delay.y * 1000.0f);
    result = std::max(result, duration);
  }
  return result;
}

int idDeclFX::GetMaxFadeOutTime(fxCondition_t startCondition) const {
  int result = 0;
  for (int i = 0; i < events.Num(); ++i) {
    const idFXSingleAction& action = events[i];
    if (HasCondition(action, startCondition))
      result = std::max(result, static_cast<int>(action.fadeOutTime * 1000.0f));
  }
  return result;
}

void idDeclFX::Parse(idParser& source) {
  FreeData();
  ++changeId;

  // The reflected edit block is decoded by idDeclTypeInfo.  The remaining work
  // here is the authored DeclFX normalization performed after that decode.
  idDeclTypeInfo::Parse(source);

  if (const auto* inherited = dynamic_cast<const idDeclFX*>(parent)) {
    for (int i = 0; i < inherited->events.Num(); ++i)
      events.Append(inherited->events[i]);
  }

  for (int i = 0; i < editEvents.Num(); ++i) {
    idFXSingleAction& action = editEvents[i];
    if (action.startCondition.Num() == 0)
      action.startCondition.Append(FX_NONE);
    if (action.stopCondition.Num() == 0)
      action.stopCondition.Append(FX_NONE);

    events.Append(action);
    if (action.multiTagUseType == FX_MULTI_TAG_USE_ALL && action.tagNames.Num() > 1) {
      for (int tag = 1; tag < action.tagNames.Num(); ++tag) {
        idFXSingleAction expanded(action);
        ReleaseList(expanded.tagNames);
        expanded.tagNames.Append(action.tagNames[tag]);
        expanded.implicit = true;
        events.Append(expanded);
      }
    }
  }

  for (int sourceIndex = 0; sourceIndex < events.Num(); ++sourceIndex) {
    const char* fireName = events[sourceIndex].fire.str;
    for (int targetIndex = 0; targetIndex < events.Num(); ++targetIndex) {
      if (idStr::Icmp(events[targetIndex].name.str, fireName) == 0)
        events[targetIndex].triggered = true;
    }
  }
}
