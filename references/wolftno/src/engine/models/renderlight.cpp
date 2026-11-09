#include "engine/models/renderlight.h"

#include "engine/renderer/declmaterial.h"
#include "shared/idlib/csystems/cvarsystemlocal.h"
#include "shared/idlib/lib.h"

#include <cstring>

extern idCVar r_skipCommits;
extern idCVar r_showCommits;
extern idCVar r_showLightingDebugPrint;
extern idCVar r_shadows;

namespace {

void SerializeInt(idSerializer& serializer, int& value, int bits) {
  if (serializer.writing)
    serializer.msg->WriteBits(value, bits);
  else
    value = serializer.msg->ReadBits(bits);
}

void SerializeFloat(idSerializer& serializer, float& value) {
  int bits = 0;
  if (serializer.writing) {
    std::memcpy(&bits, &value, sizeof(bits));
    serializer.msg->WriteBits(bits, 32);
  } else {
    bits = serializer.msg->ReadBits(32);
    std::memcpy(&value, &bits, sizeof(bits));
  }
}

} // namespace

idRenderLightParms::idRenderLightParms()
    : axis(mat3_identity), origin{}, lightGroupID(-1),
      allowLightOnlyInViewID(0), suppressLightInViewID(0),
      lightType(LIGHT_POINT), lightClass(LIGHT_NORMAL),
      lightFalloff(LIGHT_FALLOFF_TEXTURE), noShadows(false), slowLight(false),
      shader(nullptr), lightRadius{200.0f, 200.0f, 200.0f}, lightCenter{},
      target{}, right{}, up{}, start{}, end{},
      dynamicModelLightingScale{1.0f, 1.0f, 1.0f}, dimShadowStretch(1.0f),
      maxVisibleRange(0.0f), fadeVisibilityOver(400.0f), diffuseScale(1.0f),
      specularScale(1.0f), environmentScale(0.0f), eyeDiffuseScale(1.0f),
      eyeSpecularScale(1.0f), eyeEnvironmentScale(0.0f),
      staticSpecularScale(10.0f), staticSpecularVector{0.0f, 0.70700002f,
                                                       0.70700002f},
      bakedParallelShadows(false), areaLightSize(8.0f), shadowSampleScale(1.0f),
      slowMultiPass(1), forceTwoSidedShadows(false) {}

idRenderLightParms& idRenderLightParms::operator=(
    const idRenderLightParms& other) {
  std::memcpy(this, &other, sizeof(*this));
  return *this;
}

int idRenderLight::GetIndex() const { return committed->index.index >> 8; }

idStaticParmBlock<32>* idRenderLight::GetParmBlock() { return &gameParmBlock; }

void idRenderLight::SetParm(const idDeclRenderParm* parm, const idVec4& value) {
  gameParmBlock.SetParm(parm, value);
}

void idRenderLight::CommitThisFrame() {
  unlinked = false;
  if (world != nullptr) {
    world->CommitLight(GetIndex()) = true;
    world->MarkLightSerialized(this, true, false);
  }
}

idRenderLight::idRenderLight() {
  deleteOnSync = false;
  unlinked = true;
  needWriteToSnapshot = true;
  skipAutoReplication = false;
  serializedChangeCount = 0;
  world = nullptr;
  committed = new idRenderLightCommitted;
}

idRenderLight::~idRenderLight() {
  if (world != nullptr)
    world->CommitLight(GetIndex()) = false;
  delete committed;
}

void idRenderLight::Delete() {
  deleteOnSync = true;
  unlinked = false;
  CommitThisFrame();
  if (world != nullptr)
    world->MarkLightSerialized(this, false, true);
}

void idRenderLight::Unlink() {
  if (!unlinked) {
    CommitThisFrame();
    unlinked = true;
  }
}

void idRenderLight::SerializeSnapshot(idSerializer* serializer) {
  idFile_BitMsg file(serializer->msg);
  if (serializer->writing)
    gameParmBlock.Save(&file);
  else
    gameParmBlock.Load(&file);

  if (serializer->writing) {
    idMaterial::resourceList.WriteResourceToMsg(g.shader, serializer->msg);
  } else {
    g.shader = static_cast<const idMaterial*>(
        idMaterial::resourceList.ReadResourceFromMsg(serializer->msg));
  }
  serializer->SerializeQ(&g.axis, 15);
  serializer->Serialize(&g.origin);
  SerializeInt(*serializer, g.allowLightOnlyInViewID, 32);
  SerializeInt(*serializer, g.suppressLightInViewID, 32);
  int noShadows = g.noShadows ? 1 : 0;
  SerializeInt(*serializer, noShadows, 1);
  g.noShadows = noShadows == 1;
  serializer->Serialize(&g.dynamicModelLightingScale);
  SerializeFloat(*serializer, g.dimShadowStretch);
  int lightType = static_cast<int>(g.lightType);
  SerializeInt(*serializer, lightType, 4);
  g.lightType = static_cast<lightType_t>(lightType);
  int lightClass = static_cast<int>(g.lightClass);
  SerializeInt(*serializer, lightClass, 4);
  g.lightClass = static_cast<lightClass_t>(lightClass);
  serializer->Serialize(&g.lightRadius);
  serializer->Serialize(&g.lightCenter);
  if (g.lightType == LIGHT_SPOT) {
    serializer->Serialize(&g.target);
    serializer->Serialize(&g.right);
    serializer->Serialize(&g.up);
    serializer->Serialize(&g.start);
    serializer->Serialize(&g.end);
  }
  int serializedUnlinked = unlinked ? 1 : 0;
  SerializeInt(*serializer, serializedUnlinked, 1);
  if (!serializer->writing) {
    if (serializedUnlinked != 0)
      Unlink();
    else
      CommitThisFrame();
  }
}

void idRenderLight::Commit() {
  if (r_skipCommits.valueInteger != 0)
    return;
  if (r_showCommits.valueInteger > 1)
    idLib::Printf("light commit: index %i\n", GetIndex());
  committed->renderParmBlock = gameParmBlock;
  committed->r = g;
  const idMaterial* material = g.shader;
  if (material != nullptr && material->IsImplicit()) {
    if (committed->lightMtr != nullptr) {
      idLib::WarningIf(
          r_showLightingDebugPrint.valueInteger != 0,
          "Light shader '%s' defaulted, using standard light (%s)",
          material->GetName(), committed->lightMtr->GetName());
    } else {
      idLib::WarningIf(r_showLightingDebugPrint.valueInteger != 0,
                       "Light shader '%s' defaulted, using standard light",
                       material->GetName());
    }
    material = g.DefaultMaterial();
  } else if (material == nullptr) {
    material = g.DefaultMaterial();
  }
  committed->lightMtr = material;
  committed->name = name.str;
  const bool materialCastsShadows = material->MaterialCastsShadows();
  committed->castsShadows =
      !g.noShadows && materialCastsShadows && r_shadows.valueInteger != 0;
  committed->needsReferences = true;
}
