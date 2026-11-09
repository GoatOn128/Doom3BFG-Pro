#include "engine/renderer/declmaterial.h"

#include "engine/decls/declskins.h"
#include "engine/framework/coderesource.h"
#include "engine/renderer/atlasresource.h"
#include "engine/renderer/autorenderparms.h"
#include "engine/renderer/declrenderprog.h"
#include "engine/renderer/imagemanager.h"
#include "engine/renderer/imagetools/image_program.h"
#include "engine/renderer/parmblockoptimizer.h"
#include "engine/renderer/parmblockparser.h"
#include "shared/idlib/csystems/cvarsystemlocal.h"
#include "shared/idlib/filesystem/filesystemlocal.h"
#include "shared/idlib/lib.h"

#include <algorithm>
#include <cmath>
#include <cstring>

namespace {

constexpr const char* AUTO_MATERIAL_PREFIX = "generated/automaterials/";
constexpr int MATERIAL_PARSER_FLAGS = 156216;
constexpr float MIN_ATLAS_SCALE = 0.000001f;

struct genericMtr_t {
  const char* name;
  idMaterial* mtr;
};

genericMtr_t genericMaterials[] = {
    {"genericvmtr", nullptr},
    {"genericgrazingvmtr", nullptr},
    {"genericdetailvmtr", nullptr},
    {"genericcharactervmtr", nullptr},
    {"genericcharacterdecalvmtr", nullptr},
    {"genericweaponvmtr", nullptr},
    {"genericweapongrazingvmtr", nullptr},
    {nullptr, nullptr},
};

const float defaultScaleBias[4] = {1.0f, 1.0f, 0.0f, 0.0f};

template <typename T>
const T* Resource(const idCodeResource<T>& resource) {
  return resource.Get();
}

bool CVarBool(const char* name, bool fallback = false) {
  return cvarSystem != nullptr ? cvarSystem->GetCVarBool(name, fallback)
                               : fallback;
}

void SetVector(idParmBlock& block, const idDeclRenderParm* parm,
               const idVec4& vector) {
  parmValue_t value{};
  value.value[0] = vector.x;
  value.value[1] = vector.y;
  value.value[2] = vector.z;
  value.value[3] = vector.w;
  block.SetParm(parm, value);
}

void AddUsedParm(idParmBlockOptimizer& optimizer,
                 const idDeclRenderParm* parm) {
  if (parm == nullptr)
    return;
  const int index = parm->parmIndex;
  for (int i = 0; i < optimizer.numUsedParms; ++i) {
    if (optimizer.usedParms[i] == index)
      return;
  }
  optimizer.usedParms[optimizer.numUsedParms++] = index;
}

bool RenderParmStrDiffer(const idParmBlock& a, const idParmBlock& b,
                         const idDeclRenderParm* parm) {
  const bool aSets = a.SetsRenderParm(parm);
  if (aSets != b.SetsRenderParm(parm))
    return true;
  if (!aSets)
    return false;
  return idStr::Icmp(a.GetString(parm), b.GetString(parm)) != 0;
}

const idDeclRenderProg* FindRenderProg(const char* name) {
  return static_cast<const idDeclRenderProg*>(
      idDeclRenderProg::resourceList.FindWithInheritance(name, true, true));
}

void DefaultVirtualTextureRenderParms(idParmBlock& parmBlock);
void SetupVirtualTextureRenderParms(idParmBlock& parmBlock,
                                    idVirtualTexture& virtualTexture);

} // namespace

#define WOLF_MATERIAL_RENDER_PARMS(X)                                      \
  X(rp_ambientProgram, "ambientProgram")                                  \
  X(rp_interactionProgram, "interactionProgram")                          \
  X(rp_shadowProgram, "shadowProgram")                                    \
  X(rp_sssProgram, "sssProgram")                                          \
  X(rp_sssMaskProgram, "sssMaskProgram")                                  \
  X(rp_addProgram, "addProgram")                                          \
  X(rp_blendProgram, "blendProgram")                                      \
  X(rp_stageProgram, "stageProgram")                                      \
  X(rp_fadePerturbProgram, "fadePerturbProgram")                          \
  X(rp_stageSortAdjust, "stageSortAdjust")                                \
  X(rp_setTextureAxis, "setTextureAxis")                                  \
  X(rp_allowBlendedPassMerge, "allowBlendedPassMerge")                    \
  X(rp_useClipSpheres, "useClipSpheres")                                  \
  X(rp_autoSpriteType, "autoSpriteType")                                  \
  X(rp_decalProjType, "decalProjType")                                    \
  X(rp_transSortMap, "transSortMap")                                      \
  X(rp_transAtlasMap, "transAtlasMap")                                    \
  X(rp_transAtlasScaleBias, "transAtlasScaleBias")                        \
  X(rp_landDefinitionFile, "landDefinitionFile")                          \
  X(rp_virtualTextureFeedbackFloat, "virtualTextureFeedbackFloat")        \
  X(rp_physicalFilterParms, "physicalFilterParms")                        \
  X(rp_minLodMap, "minLodMap")                                            \
  X(rp_pageTableMap, "pageTableMap")                                      \
  X(rp_physicalMappingsMap, "physicalMappingsMap")                        \
  X(rp_vmtrPageTableMap, "vmtrPageTableMap")                              \
  X(rp_editorMap, "editorMap")                                            \
  X(rp_physicalPagesMap0, "physicalPagesMap0")                            \
  X(rp_physicalPagesMap1, "physicalPagesMap1")                            \
  X(rp_physicalPagesMap2, "physicalPagesMap2")                            \
  X(rp_guiSurface, "guiSurface")                                          \
  X(rp_useVirtualMapping, "useVirtualMapping")                            \
  X(rp_landDimensions, "landDimensions")                                  \
  X(rp_landDiffuseOnly, "landDiffuseOnly")                                \
  X(rp_lightRotation, "lightRotation")                                    \
  X(rp_lightTranslation, "lightTranslation")                              \
  X(rp_lightOscillationAmp, "lightOscillationAmp")                        \
  X(rp_lightOscillationFreq, "lightOscillationFreq")                      \
  X(rp_lightOscillationPhase, "lightOscillationPhase")                    \
  X(rp_transAtlasScaleBiasInv, "transAtlasScaleBiasInv")                  \
  X(rp_materialCoverage, "materialCoverage")                              \
  X(rp_skinSSSPostSpecular, "skinSSSPostSpecular")

#define WOLF_DEFINE_MATERIAL_RENDER_PARM(variable, resourceName) \
  idCodeResource<idDeclRenderParm> variable(resourceName);
WOLF_MATERIAL_RENDER_PARMS(WOLF_DEFINE_MATERIAL_RENDER_PARM)
#undef WOLF_DEFINE_MATERIAL_RENDER_PARM

idCVar decl_mergeGenericMaterials(
    "decl_mergeGenericMaterials", "1", 16385,
    "set to 0 to not merge generic materials on model load");
idCVar warn_perforatedMaterials(
    "warn_perforatedMaterials", "0", 65537,
    "Warn when parsing materials is perforated flag differs between old detection and actual flag");
idCVar r_useHardwareTextures(
    "r_useHardwareTextures", "0", 1,
    "Use hardware textures instead of virtual textures for implicit gui images");

idDeclInfoTemplate<idMaterial> idMaterial::resourceList(
    "material", "idMaterial", "mtr", true);

const char* idContents::contentsNames[34] = {
    "solid",          "opaque",          "water",
    "playerclip",     "monsterclip",     "unused_bit_5",
    "moveableclip",   "shotclip",        "ikclip",
    "aiaware",        "ai",              "projectile",
    "corpse",         "breakable",       "trigger",
    "player",         "unused_bit_16",   "obstacle",
    "unused_bit_18",  "playercoverclip", "monstercoverclip",
    "playerfocus",    "pushable",        "shield",
    "unused_bit_24",  "aas_fly",         "aas_solid",
    "aas_obstacle",   "aas_clusterportal", "aas_walkablewall",
    "nocover",        "do_not_use",      "none",
    "all",
};

idContents::idContents() {
  bytes[2] &= 0x80u;
  bytes[0] = 0;
  bytes[1] = 0;
}

int idContents::ToFlags() const {
  unsigned flags = 0;
  if (solid) flags |= CONTENTS_SOLID;
  if (opaque) flags |= CONTENTS_OPAQUE;
  if (water) flags |= CONTENTS_WATER;
  if (playerClip) flags |= CONTENTS_PLAYERCLIP;
  if (monsterClip) flags |= CONTENTS_MONSTERCLIP;
  if (vehicleClip) flags |= CONTENTS_VEHICLECLIP;
  if (moveableClip) flags |= CONTENTS_MOVEABLECLIP;
  if (shotClip) flags |= CONTENTS_SHOTCLIP;
  if (ikClip) flags |= CONTENTS_IKCLIP;
  if (aiAware) flags |= CONTENTS_AIAWARE;
  if (ai) flags |= CONTENTS_AI;
  if (projectile) flags |= CONTENTS_PROJECTILE;
  if (corpse) flags |= CONTENTS_CORPSE;
  if (trigger) flags |= CONTENTS_TRIGGER;
  if (player) flags |= CONTENTS_PLAYER;
  if (vehicle) flags |= CONTENTS_VEHICLE;
  if (obstacle) flags |= CONTENTS_OBSTACLE;
  if (playerCoverClip) flags |= CONTENTS_PLAYERCOVERCLIP;
  if (monsterCoverClip) flags |= CONTENTS_MONSTERCOVERCLIP;
  if (playerFocus) flags |= CONTENTS_PLAYERFOCUS;
  if (pushable) flags |= CONTENTS_PUSHABLE;
  if (shield) flags |= CONTENTS_SHIELD;
  if (noCover) flags |= CONTENTS_NOCOVER;
  return static_cast<int>(flags);
}

void idContents::FromFlags(const int sourceFlags) {
  const unsigned flags = static_cast<unsigned>(sourceFlags);
  const std::uint8_t preservedBit = bytes[2] & 0x80u;
  bytes[0] = 0;
  bytes[1] = 0;
  bytes[2] = preservedBit;
  solid = (flags & CONTENTS_SOLID) != 0;
  opaque = (flags & CONTENTS_OPAQUE) != 0;
  water = (flags & CONTENTS_WATER) != 0;
  playerClip = (flags & CONTENTS_PLAYERCLIP) != 0;
  monsterClip = (flags & CONTENTS_MONSTERCLIP) != 0;
  vehicleClip = (flags & CONTENTS_VEHICLECLIP) != 0;
  moveableClip = (flags & CONTENTS_MOVEABLECLIP) != 0;
  shotClip = (flags & CONTENTS_SHOTCLIP) != 0;
  ikClip = (flags & CONTENTS_IKCLIP) != 0;
  aiAware = (flags & CONTENTS_AIAWARE) != 0;
  ai = (flags & CONTENTS_AI) != 0;
  projectile = (flags & CONTENTS_PROJECTILE) != 0;
  corpse = (flags & CONTENTS_CORPSE) != 0;
  trigger = (flags & CONTENTS_TRIGGER) != 0;
  player = (flags & CONTENTS_PLAYER) != 0;
  vehicle = (flags & CONTENTS_VEHICLE) != 0;
  obstacle = (flags & CONTENTS_OBSTACLE) != 0;
  playerCoverClip = (flags & CONTENTS_PLAYERCOVERCLIP) != 0;
  monsterCoverClip = (flags & CONTENTS_MONSTERCOVERCLIP) != 0;
  playerFocus = (flags & CONTENTS_PLAYERFOCUS) != 0;
  pushable = (flags & CONTENTS_PUSHABLE) != 0;
  shield = (flags & CONTENTS_SHIELD) != 0;
  noCover = (flags & CONTENTS_NOCOVER) != 0;
}

int idContents::ContentsFromString(const char* text, const char* separator) {
  if (text == nullptr)
    return 0;

  idLexer lexer;
  lexer.LoadMemory(text, static_cast<std::size_t>(idStr::Length(text)),
                   "idContents::ContentsFromString");
  idToken token;
  unsigned flags = 0;
  while (lexer.ReadToken(token)) {
    if (token.Icmp(contentsNames[32]) == 0) {
      flags = 0;
      continue;
    }
    if (token.Icmp(contentsNames[33]) == 0) {
      flags = ~0u;
      continue;
    }
    if (token.Icmp(separator) == 0)
      continue;

    int index = 0;
    while (index < 32 && idStr::Icmp(contentsNames[index], token.data) != 0)
      ++index;
    if (index >= 32)
      break;
    flags |= 1u << index;
  }
  return static_cast<int>(flags);
}

bool idMaterial::IsValidMaterialName(const char* materialName,
                                     bool allowUnderscore,
                                     bool allowAmpersand) {
  const int first = *materialName;
  if (!idStr::CharIsAlpha(first) && (!allowUnderscore || first != '_'))
    return false;
  for (const char* character = materialName + 1; *character != '\0'; ++character) {
    if (idStr::CharIsAlpha(*character) || idStr::CharIsNumeric(*character))
      continue;
    if (*character != '_' && *character != '/' && *character != '\\' &&
        *character != '.' && (*character != '&' || !allowAmpersand))
      return false;
  }
  return true;
}

const char* idMaterial::DefaultDefinition() const {
  return "{ // DEFAULTED\n\ttransMap _default\n   stageProgram guiBlend\n}\n";
}

std::uint32_t idMaterial::GetDeclTimestamp() const {
  if (fileSystem == nullptr)
    return 0;
  if (IsImplicit()) {
    idStr imageName(GetFileName());
    imageName.SetFileExtension("tga");
    return fileSystem->GetTimestamp(imageName.data);
  }
  return fileSystem->GetTimestamp(GetFileName());
}

void idParmBlockOptimizer::AddRenderProg(const idDeclRenderProg* prog) {
  if (prog == nullptr)
    return;
  const auto appendUnique = [this](const auto& parms) {
    for (int i = 0; i < parms.Num(); ++i) {
      const int index = parms[i];
      int used = 0;
      while (used < numUsedParms && usedParms[used] != index)
        ++used;
      if (used == numUsedParms)
        usedParms[numUsedParms++] = index;
    }
  };
  appendUnique(prog->data.vertexParms);
  appendUnique(prog->data.fragmentParms);
  appendUnique(prog->data.textureParms);
  appendUnique(prog->data.samplerParms);
}

void idMaterial::Clear() {
  flags1 &= 0x80u;
  flags0 = 0;
  materialAutospriteType = AUTOSPRITE_NONE;
  materialDecalProjType = DECALPROJ_PLANAR;
  compatibleGenericMaterial = nullptr;
  editorImage = nullptr;
  devVmtr = nullptr;
  atlasScaleBias = {1.0f, 1.0f, 1.0f, 0.0f};
  virtualMapping = {0.0f, 0.0f, 0.0f, 0.0f};
  pageSource = PAGESOURCE_ALLOC0;
  pageFlavor = ID_VTF_VMTR;
  pageFileName = nullptr;
  physicalPagesPool = PHYSICAL_PAGES_POOL_VMTR;
  materialMap = nullptr;
  coverage = MC_BAD;
  for (int i = 0; i < MPR_MAX; ++i) {
    progs[i] = nullptr;
    stageSort[i] = 0.0f;
  }
  stageSortAdjust = 0.0f;
  parmBlock.Clear();
  renderParmBlock.Clear();
}

void idMaterial::OptimizeRenderParmBlock() {
  renderParmBlock.CopyFrom(parmBlock);
  idParmBlockOptimizer optimizer{};
  for (const idDeclRenderProg* prog : progs)
    optimizer.AddRenderProg(prog);

  AddUsedParm(optimizer, Resource(rp_lightColor));
  AddUsedParm(optimizer, Resource(rp_lightRotation));
  AddUsedParm(optimizer, Resource(rp_lightTranslation));
  AddUsedParm(optimizer, Resource(rp_lightScale));
  AddUsedParm(optimizer, Resource(rp_lightOscillationAmp));
  AddUsedParm(optimizer, Resource(rp_lightOscillationFreq));
  AddUsedParm(optimizer, Resource(rp_lightOscillationPhase));
  AddUsedParm(optimizer, Resource(rp_lightDiffuseScale));
  AddUsedParm(optimizer, Resource(rp_lightSpecularScale));
  AddUsedParm(optimizer, Resource(rp_lightEnvironmentScale));
  AddUsedParm(optimizer, Resource(rp_lightEyeDiffuseScale));
  AddUsedParm(optimizer, Resource(rp_lightEyeSpecularScale));
  AddUsedParm(optimizer, Resource(rp_lightEyeEnvironmentScale));
  AddUsedParm(optimizer, Resource(rp_landPageFile));
  AddUsedParm(optimizer, Resource(rp_textureMapShadow));
  optimizer.OptimizeParmBlockForUsedParms(renderParmBlock);
}

bool idMaterial::SetImplicitText() {
  char generated[2048];
  const bool validName = IsValidMaterialName(name.str, false, false);
  const bool isFont = idStr::Icmpn(name.str, "fonts/", 6) == 0;

  if (!validName || (!isFont && r_useHardwareTextures.GetBool())) {
    idStr::snPrintf(
        generated, sizeof(generated),
        "{ // IMPLICITLY GENERATED\ntransMap clamp linear \"%s\"\nstageProgram\tguiBlend\n}\n",
        name.str);
  } else if (!isFont) {
    idStr::snPrintf(
        generated, sizeof(generated),
        "{ // IMPLICITLY GENERATED\nvirtualTransMap \"%s\"\nstageProgram\tvirtualGuiBlend\n}\n",
        name.str);
  } else if (CVarBool("r_useDistanceFont")) {
    idStr declaration("{ //IMPLICITLY GENERATED\r\n");
    idStr line;
    line.Format("transmap alpha borderClamp \"%s\"\n", name.str);
    declaration.Append(line);
    declaration.Append("stageprogram FontOutlineGlowShadow\n");
    declaration.Append("fontFXParms { 0.000, 0.000, 0.000, -2.000 }\n");
    declaration.Append("fontFXOutlineColor { 0.000, 0.000, 0.000, 1.000 }\n");
    declaration.Append("fontFXGlowColor { 0.000, 0.000, 0.000, 1.000 }\n");
    declaration.Append("fontFXGlowParms { 0.000, 0.000, 0.000, 0.000 }\n");
    declaration.Append("}\r\n");
    idStr::snPrintf(generated, sizeof(generated), "%s", declaration.data);
  } else {
    idStr::snPrintf(
        generated, sizeof(generated),
        "{ // IMPLICITLY GENERATED\ntransmap alpha borderClamp \"%s\"\nstageprogram FontBiasAndGamma\n}\n",
        name.str);
  }

  SetText(generated);
  return true;
}

void idMaterial::List() const {
  idLib::Printf("%s\n", name.str);
}

idStr* idMaterial::Write() const {
  idStr* result = new idStr("{\n");
  idStr* blockText = parmBlock.WriteString();
  if (blockText != nullptr) {
    *result += *blockText;
    delete blockText;
  }
  *result += "}\n";
  return result;
}

void idMaterial::Print() const {
  idLib::Printf("---- Write output ----\n");
  idStr* output = Write();
  idLib::Printf("%s", output != nullptr ? output->data : "");
  idLib::Printf("Coverage: %i\n", coverage);
  idLib::Printf("coverageProgram: %s\n",
                progs[MPR_COVERAGE] != nullptr ? progs[MPR_COVERAGE]->name.str
                                              : "NULL");
  idLib::Printf("interactionProgram: %s\n",
                progs[MPR_INTERACTION] != nullptr
                    ? progs[MPR_INTERACTION]->name.str
                    : "NULL");
  idLib::Printf("shadowProgram: %s\n",
                progs[MPR_SHADOW] != nullptr ? progs[MPR_SHADOW]->name.str
                                            : "NULL");
  idLib::Printf("addProgram: %s\n",
                progs[MPR_ADD] != nullptr ? progs[MPR_ADD]->name.str : "NULL");
  idLib::Printf("blendProgram: %s\n",
                progs[MPR_BLEND] != nullptr ? progs[MPR_BLEND]->name.str
                                           : "NULL");
  idLib::Printf("stageProgram: %s\n",
                progs[MPR_STAGE] != nullptr ? progs[MPR_STAGE]->name.str
                                           : "NULL");
  // The shipping function does not release the Write() result.
}

namespace {

void DefaultVirtualTextureRenderParms(idParmBlock& parmBlock) {
  parmValue_t feedback{};
  parmBlock.SetParm(Resource(rp_virtualTextureFeedbackFloat), feedback);
  parmBlock.SetParm(Resource(rp_physicalFilterParms),
                    Resource(rp_physicalFilterParms));
  parmBlock.SetImage(Resource(rp_minLodMap), globalImages->blackImage);
  parmBlock.SetImage(Resource(rp_pageTableMap), globalImages->blackImage);
  parmBlock.SetImage(Resource(rp_physicalMappingsMap), globalImages->blackImage);
  parmBlock.SetImage(Resource(rp_physicalPagesMap0), globalImages->blackImage);
  parmBlock.SetImage(Resource(rp_physicalPagesMap1), globalImages->blackImage);
  parmBlock.SetImage(Resource(rp_physicalPagesMap2), globalImages->blackImage);
}

void SetupVirtualTextureRenderParms(idParmBlock& parmBlock,
                                    idVirtualTexture& virtualTexture) {
  SetVector(parmBlock, Resource(rp_virtualTextureFeedbackFloat),
            virtualTexture.GetFeedbackVec4());
  parmBlock.SetParm(Resource(rp_physicalFilterParms),
                    virtualTexture.physicalPages->GetFilterParms());

  const auto imageOrBlack = [](idImage* image) -> idImage* {
    return image != nullptr ? image : globalImages->blackImage;
  };
  parmBlock.SetImage(Resource(rp_minLodMap),
                     imageOrBlack(virtualTexture.pageMinLodImage));
  parmBlock.SetImage(Resource(rp_pageTableMap),
                     imageOrBlack(virtualTexture.pageTableImage));
  parmBlock.SetImage(
      Resource(rp_physicalMappingsMap),
      imageOrBlack(virtualTexture.physicalPages->physicalMappingsImage));
  parmBlock.SetImage(
      Resource(rp_physicalPagesMap0),
      imageOrBlack(virtualTexture.physicalPages->physicalPagesImage0));
  parmBlock.SetImage(
      Resource(rp_physicalPagesMap1),
      imageOrBlack(virtualTexture.physicalPages->physicalPagesImage1));
  parmBlock.SetImage(
      Resource(rp_physicalPagesMap2),
      imageOrBlack(virtualTexture.physicalPages->physicalPagesImage2));
}

} // namespace

void idMaterial::SetupVirtualTexture() const {
  if (pageFileName == nullptr)
    return;
  idVirtualTexture* virtualTexture =
      virtualTextureSystem.VirtualTextureForMaterial(this);
  if (virtualTexture == nullptr || virtualTexture->pageFile == nullptr)
    return;
  pageSource = virtualTexture->pageSource;
  pageFlavor = virtualTexture->flavor;
  if (!CVarBool("r_skipVmtrs")) {
    SetupVirtualTextureRenderParms(parmBlock, *virtualTexture);
    const_cast<idMaterial*>(this)->OptimizeRenderParmBlock();
  }
}

surfTypes_t idMaterial::GetSurfaceType() const {
  return static_cast<surfTypes_t>(
      parmBlock.GetInteger(Resource(rp_surfaceType)));
}

surfaceFlags_t idMaterial::GetSurfaceFlags() const {
  return static_cast<surfaceFlags_t>(
      parmBlock.GetInteger(Resource(rp_surfaceFlags)));
}

fractureInfo_t idMaterial::GetFractureInfo() const {
  return static_cast<fractureInfo_t>(
      parmBlock.GetInteger(Resource(rp_fractureInfo)));
}

idVec4 idMaterial::GetFractureColorType() const {
  return parmBlock.GetVector(Resource(rp_fractureColorType));
}

idVec4 idMaterial::GetFractureColorContent() const {
  return parmBlock.GetVector(Resource(rp_fractureColorContent));
}

void idMaterial::UpdateVirtualMapping() const {
  if (devVmtr == nullptr)
    return;
  const idVec4 mapping = devVmtr->scaleAndBias;
  const idVec4 enabled{1.0f, 1.0f, 1.0f, 1.0f};
  SetVector(parmBlock, Resource(rp_virtualMapping), mapping);
  SetVector(parmBlock, Resource(rp_useVirtualMapping), enabled);
  virtualMapping = mapping;
  SetVector(renderParmBlock, Resource(rp_virtualMapping), mapping);
  SetVector(renderParmBlock, Resource(rp_useVirtualMapping), enabled);
}

bool idMaterial::MaterialIsMergableWith(
    const idMaterial* testMaterial) const {
  if (testMaterial == this)
    return true;
  if (RenderParmStrDiffer(parmBlock, testMaterial->parmBlock,
                          Resource(rp_landPageFile)))
    return false;

  const auto compatibleProgram = [this, testMaterial](materialProg_t slot) {
    const idDeclRenderProg* a = progs[slot];
    const idDeclRenderProg* b = testMaterial->progs[slot];
    return a == nullptr || b == nullptr || a->name.str == b->name.str;
  };
  if (!compatibleProgram(MPR_COVERAGE) ||
      !compatibleProgram(MPR_INTERACTION) ||
      !compatibleProgram(MPR_SHADOW) || !compatibleProgram(MPR_SSS))
    return false;

  if (MaterialHasMergeableBlendedPass()) {
    if (!compatibleProgram(MPR_ADD))
      return false;
    return compatibleProgram(MPR_BLEND);
  }
  if (progs[MPR_ADD] != nullptr || testMaterial->progs[MPR_ADD] != nullptr ||
      progs[MPR_BLEND] != nullptr ||
      testMaterial->progs[MPR_BLEND] != nullptr)
    return false;
  return compatibleProgram(MPR_STAGE);
}

bool idMaterial::IsEqualForRender(const idMaterial* other) const {
  if (other == this)
    return true;
  if (!renderParmBlock.IsEqualSimple(other->renderParmBlock) ||
      stageSortAdjust != other->stageSortAdjust)
    return false;
  for (int i = 0; i < MPR_MAX; ++i) {
    if (progs[i] != other->progs[i])
      return false;
  }
  return true;
}

const float* idMaterial::GetMaterialMapping(const idMaterial* material) const {
  if (materialMap == nullptr)
    return defaultScaleBias;
  for (int i = 0; i < materialMap->mappings.Num(); ++i) {
    if (idStr::Icmp(materialMap->mappings[i].materialName.str,
                    material->name.str) == 0)
      return &materialMap->mappings[i].scaleBias.x;
  }
  return defaultScaleBias;
}

bool idMaterial::MaterialMappingsAreCompatible(
    const idMaterial* material) const {
  if (materialMap == nullptr || material->materialMap == nullptr ||
      materialMap->mappings.Num() != material->materialMap->mappings.Num())
    return false;
  for (int i = 0; i < materialMap->mappings.Num(); ++i) {
    const idVec4& a = materialMap->mappings[i].scaleBias;
    const idVec4& b = material->materialMap->mappings[i].scaleBias;
    if (a.x != b.x || a.y != b.y || a.z != b.z || a.w != b.w)
      return false;
  }
  return true;
}

bool idMaterial::GetImageDimensions(int& width, int& height) const {
  if (materialMap != nullptr)
    return materialMap->GetImageDimensions(width, height);

  int transWidth = 0;
  int transHeight = 0;
  int diffuseWidth = 0;
  int diffuseHeight = 0;
  int bumpWidth = 0;
  int bumpHeight = 0;
  const bool transLoaded = R_LoadImageProgram(
      parmBlock.GetString(Resource(rp_virtualTransMap)), nullptr, &transWidth,
      &transHeight, nullptr);
  const bool diffuseLoaded = R_LoadImageProgram(
      parmBlock.GetString(Resource(rp_diffuseMap)), nullptr, &diffuseWidth,
      &diffuseHeight, nullptr);
  const bool bumpLoaded = R_LoadImageProgram(
      parmBlock.GetString(Resource(rp_bumpMap)), nullptr, &bumpWidth,
      &bumpHeight, nullptr);
  width = std::max({transWidth, diffuseWidth, bumpWidth});
  height = std::max({transHeight, diffuseHeight, bumpHeight});
  if (width == 0 || height == 0) {
    width = 1;
    height = 1;
  }
  return transLoaded && diffuseLoaded && bumpLoaded;
}

bool idMaterial::IsVMTRVirtualTexture() const {
  return pageFlavor != ID_VTF_BAKE && progs[MPR_INTERACTION] != nullptr;
}

idStr idMaterial::NameWithoutAutoMaterial() const {
  if (std::strncmp(name.str, AUTO_MATERIAL_PREFIX,
                   std::strlen(AUTO_MATERIAL_PREFIX)) != 0)
    return idStr(name.str);
  idStr result(name.str + std::strlen(AUTO_MATERIAL_PREFIX));
  result.StripFileExtension();
  return result;
}

idMaterial::idMaterial() {
  flags0 = 0;
  flags1 = 0;
  Clear();
}

idDeclInfo* idMaterial::GetDeclInfo() const {
  return &resourceList;
}

void idMaterial::ReParse(bool fromDisk) {
  idDecl::ReParse(fromDisk);
  if (devVmtr != nullptr) {
    devVmtr = virtualTextureSystem.VirtualMaterialForMaterial(this);
    UpdateVirtualMapping();
  }
}

void idMaterial::SetEditorImage() const {
  const idDeclRenderParm* imageStringParms[] = {
      Resource(rp_qer_editorImage), Resource(rp_transSortMap),
      Resource(rp_virtualTransMap), Resource(rp_transAtlasMap),
      Resource(rp_diffuseMap),
  };
  for (const idDeclRenderParm* parm : imageStringParms) {
    if (!parmBlock.SetsRenderParm(parm))
      continue;
    idStr imageString(parmBlock.GetString(parm));
    idParser parser;
    parser.SetFlags(MATERIAL_PARSER_FLAGS);
    parser.LoadMemory(imageString.data, imageString.Length(), parm->name.str);
    editorImage = parm->ParseImageLine(parser);
    if (editorImage != nullptr)
      return;
  }

  if (parmBlock.SetsRenderParm(Resource(rp_editorMap))) {
    editorImage = parmBlock.GetImage(Resource(rp_editorMap));
    if (editorImage != nullptr)
      return;
  }

  for (int i = 0; i < parmBlock.ops.Num(); ++i) {
    const expOp_t& operation = parmBlock.ops[i];
    const idDeclRenderParm* destination = operation.GetDest();
    if (destination == nullptr || destination->parmType < PT_TEXTURE_2D ||
        destination->parmType > PT_IMAGE_2D_WRITE)
      continue;
    const idImage* image = nullptr;
    if ((operation.type & 0x1Fu) != 0) {
      const idDeclRenderParm* source = operation.GetA();
      image = source != nullptr ? source->declaredValue.image : nullptr;
    } else if (i < parmBlock.constants.Num()) {
      image = parmBlock.constants[i].image;
    }
    if (image != nullptr && idStr::Cmp(image->name.str, "_vmtrpagetable") != 0 &&
        image->opts.textureType == TT_2D) {
      editorImage = image;
      return;
    }
  }
  editorImage = globalImages->defaultImage;
}

void idMaterial::ReloadImages(bool force) const {
  (void)force;
  for (int i = 0; i < parmBlock.ops.Num(); ++i) {
    const expOp_t& operation = parmBlock.ops[i];
    const idDeclRenderParm* destination = operation.GetDest();
    if (destination == nullptr || destination->parmType < PT_TEXTURE_2D ||
        destination->parmType > PT_IMAGE_2D_WRITE)
      continue;
    const idImage* image = nullptr;
    if ((operation.type & 0x1Fu) != 0) {
      const idDeclRenderParm* source = operation.GetA();
      image = source != nullptr ? source->declaredValue.image : nullptr;
    } else if (i < parmBlock.constants.Num()) {
      image = parmBlock.constants[i].image;
    }
    const_cast<idImage*>(image)->ReloadIfStale();
  }
}

void idMaterial::InitCompatibleGenericMaterials() {
  if (genericMaterials[0].mtr != nullptr)
    return;
  for (genericMtr_t* generic = genericMaterials; generic->name != nullptr;
       ++generic) {
    generic->mtr = static_cast<idMaterial*>(
        resourceList.FindWithInheritance(generic->name, false, true));
    if (generic->mtr == nullptr)
      idLib::FatalError("Couldn't find material: %s", generic->name);
    generic->mtr->pageFileName =
        virtualTextureSystem.vts[PAGESOURCE_ALLOC0].GetBaseName();
    generic->mtr->SetupVirtualTexture();
  }
}

const idMaterial* idMaterial::CompatibleGenericMaterial() const {
  if (!decl_mergeGenericMaterials.GetBool())
    return nullptr;
  if (genericMaterials[0].mtr == nullptr)
    InitCompatibleGenericMaterials();
  if (materialHasCheckedCompatible)
    return compatibleGenericMaterial;

  flags1 |= 0x40u;
  for (const genericMtr_t* generic = genericMaterials;
       generic->name != nullptr; ++generic) {
    if (!MaterialIsMergableWith(generic->mtr))
      continue;
    compatibleGenericMaterial = static_cast<const idMaterial*>(
        resourceList.FindWithInheritance(generic->name, false, true));
    break;
  }
  return compatibleGenericMaterial;
}

bool idMaterial::MaterialMappingNeedsReload(const idDeclSkins* skins) const {
  if (materialMap == nullptr || idLib::production == PROD_PRODUCTION)
    return false;
  for (int i = 0; i < materialMap->mappings.Num(); ++i) {
    const idMaterialMapping& mapping = materialMap->mappings[i];
    if (mapping.materialName.str == nullptr)
      return true;
    const idMaterial* material = static_cast<const idMaterial*>(
        resourceList.FindWithInheritance(mapping.materialName.str, false, true));
    if (material == nullptr)
      return true;
    if (mapping.materialChecksum != material->md5Checksum &&
        material->GetDeclTimestamp() > mapping.materialTimestamp)
      return true;
    if (materialMap->skinFileTimeStamp !=
        const_cast<idDeclSkins*>(skins)->GetDeclTimestamp())
      return true;

    idMaterialMapping& mutableMapping = materialMap->mappings[i];
    if (!mutableMapping.specularFile.NeedsReload() &&
        !mutableMapping.coverFile.NeedsReload() &&
        !mutableMapping.diffuseFile.NeedsReload() &&
        !mutableMapping.bumpFile.NeedsReload())
      mutableMapping.powerFile.NeedsReload();
  }
  return false;
}

bool idMaterial::MaterialForcesNormalSmoothing() const {
  const idDeclRenderParm* renderBump = static_cast<const idDeclRenderParm*>(
      idDeclRenderParm::resourceList.FindWithInheritance("renderbump", true,
                                                         true));
  const char* renderBumpString = parmBlock.GetString(renderBump);
  if (renderBumpString != nullptr && *renderBumpString != '\0')
    return std::strstr(renderBumpString, "-facet") == nullptr;
  return pageFlavor == ID_VTF_BAKE;
}

const idMaterial* FindMaterialOrAutoMaterial(const char* materialName) {
  if (materialName != nullptr) {
    const idMaterial* material = static_cast<const idMaterial*>(
        idMaterial::resourceList.FindWithInheritance(materialName, false,
                                                      true));
    if (material != nullptr && !material->IsImplicit())
      return material;
  }
  idLib::Warning(
      "Cannot find material '%s' replacing with material 'textures/common/missing_material'",
      materialName);
  const idMaterial* missing = static_cast<const idMaterial*>(
      idMaterial::resourceList.FindWithInheritance(
          "textures/common/missing_material", false, true));
  return missing != nullptr && !missing->IsImplicit() ? missing : nullptr;
}

int idMaterial::GetWidth() const {
  int width = 0;
  if (pageFlavor == ID_VTF_BAKE) {
    width = 960 * parmBlock.GetInteger(Resource(rp_landDimensions));
  } else if (devVmtr != nullptr) {
    width = (devVmtr->allocWidth << 7) / 120;
  } else if (virtualMapping.x != 0.0f || virtualMapping.y != 0.0f ||
             virtualMapping.z != 0.0f || virtualMapping.w != 0.0f) {
    width = static_cast<int>(std::floor(
        120.0f * virtualTextureSystem.vts[pageSource].VIRTUAL_MAPPING_PAGES_WIDE *
            virtualMapping.x +
        0.5f));
  } else {
    if (editorImage == nullptr)
      SetEditorImage();
    width = editorImage->opts.width;
  }
  return std::max(width, 1);
}

int idMaterial::GetHeight() const {
  int height = 0;
  if (pageFlavor == ID_VTF_BAKE) {
    height = 960 * parmBlock.GetInteger(Resource(rp_landDimensions));
  } else if (devVmtr != nullptr) {
    height = (devVmtr->allocHeight << 7) / 120;
  } else if (virtualMapping.x != 0.0f || virtualMapping.y != 0.0f ||
             virtualMapping.z != 0.0f || virtualMapping.w != 0.0f) {
    height = static_cast<int>(std::floor(
        120.0f * virtualTextureSystem.vts[pageSource].VIRTUAL_MAPPING_PAGES_WIDE *
            virtualMapping.y +
        0.5f));
  } else {
    if (editorImage == nullptr)
      SetEditorImage();
    height = editorImage->opts.height;
  }
  return std::max(height, 1);
}

idMaterialMap::idMaterialMap() = default;

bool idMaterial::ParseMaterialMap(idParser& source) {
  materialMap = new idMaterialMap;
  idToken token;
  source.ExpectTokenString("{");
  source.ExpectTokenType(TT_STRING, 0, token);
  materialMap->name = token.data;
  materialMap->width = source.ParseInt();
  materialMap->height = source.ParseInt();
  materialMap->skinFileTimeStamp =
      source.PeekTokenString("{") ? ~0u
                                   : static_cast<unsigned>(source.ParseInt());

  while (source.CheckTokenString("{")) {
    idMaterialMapping mapping{};
    source.ExpectTokenType(TT_STRING, 0, token);
    mapping.materialName.Set(token.data);
    mapping.materialChecksum = static_cast<unsigned>(source.ParseInt());
    mapping.materialTimestamp = static_cast<unsigned>(source.ParseInt());
    mapping.x = source.ParseInt();
    mapping.y = source.ParseInt();
    mapping.width = source.ParseInt();
    mapping.height = source.ParseInt();
    mapping.scaleBias = {
        static_cast<float>(mapping.width) / materialMap->width,
        static_cast<float>(mapping.height) / materialMap->height,
        static_cast<float>(mapping.x) / materialMap->width,
        static_cast<float>(mapping.y) / materialMap->height,
    };

    idVirtualImageFile* files[] = {
        &mapping.specularFile, &mapping.coverFile, &mapping.diffuseFile,
        &mapping.bumpFile, &mapping.powerFile,
    };
    for (idVirtualImageFile* file : files) {
      source.ExpectTokenType(TT_STRING, 0, token);
      file->fileName.Set(token.data);
      file->timeStamp = static_cast<unsigned>(source.ParseInt());
    }
    source.ExpectTokenString("}");
    materialMap->mappings.Append(mapping);
  }
  source.ExpectTokenString("}");
  return true;
}

void idMaterial::FreeData() {
  delete materialMap;
  Clear();
}

void idMaterial::Parse(idParser& source) {
  materialIsSpecialMissing =
      idStr::Icmp(name.str, "textures/common/missing_material") == 0;

  if (source.CheckTokenString("materialmap") && !ParseMaterialMap(source))
    return;

  const bool beganWithBrace = source.CheckTokenString("{");
  idParmBlockParser parmParser(source, parmBlock);
  if (source.HadWarning()) {
    idLib::Warning(
        "idMaterial::Parse: Material %s had parmblock parse warning, ill-formed material may result!!",
        name.str);
    return;
  }
  if (beganWithBrace && !source.CheckTokenString("}")) {
    idLib::Warning(
        "idMaterial::Parse: Material %s missing a brace, ill-formed material may result!!",
        name.str);
    return;
  }

  progs[MPR_COVERAGE] =
      parmBlock.GetProgram(Resource(rp_ambientProgram));
  progs[MPR_INTERACTION] =
      parmBlock.GetProgram(Resource(rp_interactionProgram));
  progs[MPR_SHADOW] = parmBlock.GetProgram(Resource(rp_shadowProgram));
  progs[MPR_SSS] = parmBlock.GetProgram(Resource(rp_sssProgram));
  progs[MPR_ADD] = parmBlock.GetProgram(Resource(rp_addProgram));
  progs[MPR_BLEND] = parmBlock.GetProgram(Resource(rp_blendProgram));
  progs[MPR_STAGE] = parmBlock.GetProgram(Resource(rp_stageProgram));
  progs[MPR_FADE_PERTURB] =
      parmBlock.GetProgram(Resource(rp_fadePerturbProgram));

  if (progs[MPR_INTERACTION] == nullptr && progs[MPR_STAGE] == nullptr &&
      (parmBlock.SetsRenderParm(Resource(rp_bumpMap)) ||
       parmBlock.SetsRenderParm(Resource(rp_diffuseMap)) ||
       parmBlock.SetsRenderParm(Resource(rp_virtualMapping))))
    progs[MPR_INTERACTION] = FindRenderProg("interBasic");

  if (progs[MPR_SHADOW] == nullptr) {
    progs[MPR_SHADOW] = FindRenderProg(
        parmBlock.SetsRenderParm(Resource(rp_coverMap))
            ? "occluderShadowTransVmtr"
            : "occluderShadowVmtr");
  }

  if (progs[MPR_COVERAGE] == nullptr && progs[MPR_INTERACTION] != nullptr) {
    const bool hasCoverMap = parmBlock.SetsRenderParm(Resource(rp_coverMap));
    const bool twoSided =
        (progs[MPR_INTERACTION]->data.glState & 0x8000u) != 0;
    const char* coverageProgram = nullptr;
    if (!hasCoverMap)
      coverageProgram = twoSided ? "outsideTwoSided" : "outside";
    else
      coverageProgram = twoSided ? "outsideTwoSided" : "outsideTrans";
    progs[MPR_COVERAGE] = FindRenderProg(coverageProgram);
  }

  if (progs[MPR_SSS] == nullptr) {
    const idDeclRenderProg* parent = progs[MPR_STAGE] != nullptr
                                         ? progs[MPR_STAGE]
                                         : progs[MPR_COVERAGE];
    if (parent != nullptr)
      progs[MPR_SSS] =
          parent->data.parmBlock.GetProgram(Resource(rp_sssProgram));
  }
  if (progs[MPR_SSSMASK] == nullptr) {
    const idDeclRenderProg* parent = progs[MPR_SSS] != nullptr
                                         ? progs[MPR_SSS]
                                     : progs[MPR_STAGE] != nullptr
                                         ? progs[MPR_STAGE]
                                         : progs[MPR_COVERAGE];
    if (parent != nullptr)
      progs[MPR_SSSMASK] =
          parent->data.parmBlock.GetProgram(Resource(rp_sssMaskProgram));
  }
  if (progs[MPR_FADE_PERTURB] == nullptr) {
    const idDeclRenderProg* parent = progs[MPR_STAGE] != nullptr
                                         ? progs[MPR_STAGE]
                                         : progs[MPR_COVERAGE];
    if (parent != nullptr)
      progs[MPR_FADE_PERTURB] = parent->data.parmBlock.GetProgram(
          Resource(rp_fadePerturbProgram));
  }

  {
    idParmBlock combined(parmBlock);
    const idDeclRenderProg* parent = progs[MPR_STAGE] != nullptr
                                         ? progs[MPR_STAGE]
                                         : progs[MPR_COVERAGE];
    if (parent != nullptr)
      combined.Append(parent->data.parmBlock);
    stageSortAdjust = combined.GetFloat(Resource(rp_stageSortAdjust));
    materialCastsShadows = combined.GetFloat(Resource(rp_NoShadows)) == 0.0f;
    materialAutospriteType = static_cast<autospriteType_t>(
        combined.GetInteger(Resource(rp_autoSpriteType)));
    materialDecalProjType = static_cast<decalProjType_t>(
        combined.GetInteger(Resource(rp_decalProjType)));
    materialHasClipSpheres =
        combined.GetFloat(Resource(rp_useClipSpheres)) == 1.0f;
  }

  materialIsTransparencySorted =
      parmBlock.SetsRenderParm(Resource(rp_transSortMap));
  if (materialIsTransparencySorted) {
    const char* atlasName = parmBlock.GetString(Resource(rp_transSortMap));
    const idAtlasResource* atlas = static_cast<const idAtlasResource*>(
        idAtlasResource::resourceList.Load(atlasName, true, false));
    atlasScaleBias = atlas->scaleBias;
    materialUsesTransSortAtlas = 1;
    parmBlock.SetVec4(Resource(rp_transAtlasScaleBias), atlasScaleBias);
    const idVec4 inverse{
        1.0f / std::max(atlasScaleBias.x, MIN_ATLAS_SCALE),
        1.0f / std::max(atlasScaleBias.y, MIN_ATLAS_SCALE),
        atlasScaleBias.z,
        atlasScaleBias.w,
    };
    SetVector(parmBlock, Resource(rp_transAtlasScaleBiasInv), inverse);
  }

  if (parmBlock.SetsRenderParm(Resource(rp_transAtlasMap))) {
    const char* atlasName = parmBlock.GetString(Resource(rp_transAtlasMap));
    const idAtlasResource* atlas = static_cast<const idAtlasResource*>(
        idAtlasResource::resourceList.Load(atlasName, true, false));
    atlasScaleBias = atlas->scaleBias;
    materialUsesTransSortAtlas = 1;
    parmBlock.SetImage(Resource(rp_transMap),
                       globalImages->transSortImageAtlas->atlas);
    parmBlock.SetVec4(Resource(rp_transAtlasScaleBias), atlasScaleBias);
    const idVec4 inverse{
        1.0f / std::max(atlasScaleBias.x, MIN_ATLAS_SCALE),
        1.0f / std::max(atlasScaleBias.y, MIN_ATLAS_SCALE),
        atlasScaleBias.z,
        atlasScaleBias.w,
    };
    SetVector(parmBlock, Resource(rp_transAtlasScaleBiasInv), inverse);
  }

  skipSurfaceInfo =
      parmBlock.GetInteger(Resource(rp_skipSurfaceInfo)) != 0;
  materialHasMergeableBlendedPass =
      parmBlock.GetInteger(Resource(rp_allowBlendedPassMerge)) != 0;
  materialIsMixedUnique = progs[MPR_STAGE] == nullptr &&
                          (progs[MPR_ADD] != nullptr ||
                           progs[MPR_BLEND] != nullptr);
  materialIsSky = progs[MPR_STAGE] != nullptr &&
                  (progs[MPR_STAGE]->data.flags & 2) != 0;

  flags0 &= 0x9Du;
  for (const idDeclRenderProg* prog : progs) {
    if (prog == nullptr)
      continue;
    if ((prog->data.glState & 0x8000u) != 0)
      materialIsTwoSided = 1;
    if (prog->ProgUsesParm(Resource(rp_primeLightDir)) ||
        prog->ProgUsesParm(Resource(rp_channelLight0)))
      materialUsesApproximatedLighting = 1;
    if (prog->ProgUsesParm(Resource(rp_wrinkleFactor0)))
      materialUsesWrinkleMaps = 1;
  }
  materialShouldBakeStaticSpecular =
      parmBlock.GetInteger(Resource(rp_bakeStaticSpecular)) != 0;
  materialUsesPostSpecular =
      parmBlock.GetInteger(Resource(rp_skinSSSPostSpecular)) != 0;

  coverage = MC_BAD;
  if (parmBlock.SetsRenderParm(Resource(rp_materialCoverage))) {
    coverage = static_cast<materialCoverage_t>(
        parmBlock.GetInteger(Resource(rp_materialCoverage)));
  } else {
    for (const idDeclRenderProg* prog : progs) {
      if (prog == nullptr)
        continue;
      if ((prog->data.glState & 0x8000u) != 0)
        materialIsTwoSided = 1;
      if ((prog->data.flags & 1) != 0) {
        coverage = MC_PERFORATED;
        break;
      }
      if ((prog->data.flags & 4) != 0) {
        coverage = MC_PERFORATED_FOR_MODELFADE;
        break;
      }
      if ((prog->data.flags & 2) != 0) {
        coverage = MC_OPAQUE;
        break;
      }
    }
  }

  if (coverage == MC_BAD) {
    const idDeclRenderProg* interaction = progs[MPR_INTERACTION];
    const idDeclRenderProg* ambient = progs[MPR_COVERAGE];
    const bool opaqueAmbient =
        interaction != nullptr && ambient != nullptr &&
        (ambient->data.glState & 0x38u) == 0 &&
        (ambient->data.glState & 0x20000u) == 0 &&
        (ambient->data.glState & 0x40u) == 0;
    if (opaqueAmbient) {
      coverage = MC_OPAQUE;
    } else {
      coverage = interaction != nullptr ? MC_TRANSLUCENT : MC_INVISIBLE;
      if (parmBlock.GetFloat(Resource(rp_If)) != 0.0f &&
          progs[MPR_STAGE] != nullptr) {
        const idDeclRenderProg* stage = progs[MPR_STAGE];
        const idDeclRenderParm* sortCoverage = Resource(rp_sortCoverage);
        const float coverageSort =
            sortCoverage != nullptr ? sortCoverage->declaredValue.value[0] : 0.0f;
        coverage = ((stage->data.glState & 0x38u) != 0 ||
                    (stage->data.glState & 0x20000u) != 0 ||
                    (stage->data.glState & 0x40u) != 0 ||
                    stage->data.stageSort != coverageSort)
                       ? MC_TRANSLUCENT
                       : MC_OPAQUE;
      }
    }
  }

  if (coverage == MC_INVISIBLE) {
    progs[MPR_COVERAGE] = nullptr;
    progs[MPR_SHADOW] = nullptr;
    progs[MPR_FADE_PERTURB] = nullptr;
  } else if (coverage == MC_TRANSLUCENT) {
    const char* queryProgram =
        materialAutospriteType == AUTOSPRITE_VIEW_ORIENTED
            ? "occluderQueryOnlyAutosprite1"
        : materialAutospriteType == AUTOSPRITE_LONGEST_AXIS_ALIGNED
            ? "occluderQueryOnlyAutosprite2"
            : "occluderQueryOnly";
    progs[MPR_COVERAGE] = FindRenderProg(queryProgram);
    progs[MPR_SHADOW] = nullptr;
    progs[MPR_FADE_PERTURB] = nullptr;
  } else if (progs[MPR_COVERAGE] == nullptr) {
    progs[MPR_COVERAGE] = progs[MPR_STAGE];
  }

  for (int i = 0; i < MPR_MAX; ++i) {
    if (progs[i] != nullptr)
      stageSort[i] = progs[i]->data.stageSort;
  }

  const char* parsedPageFile =
      parmBlock.GetString(Resource(rp_landPageFile));
  if (parsedPageFile != nullptr && *parsedPageFile != '\0') {
    idStr requestedPageFile(parsedPageFile);
    pageSource_t sourceIndex = PAGESOURCE_ALLOC0;
    for (; sourceIndex < PAGESOURCE_TOTAL; sourceIndex =
             static_cast<pageSource_t>(sourceIndex + 1)) {
      idStr sourcePageFile(
          virtualTextureSystem.vts[sourceIndex].GetBaseName());
      if (requestedPageFile == sourcePageFile)
        break;
    }
    if (sourceIndex < PAGESOURCE_TOTAL) {
      pageSource = sourceIndex;
      pageFlavor = virtualTextureSystem.vts[sourceIndex].flavor;
    } else if (std::strstr(parsedPageFile, "_vmtr") != nullptr) {
      pageSource = PAGESOURCE_ALLOC0;
      pageFlavor = ID_VTF_VMTR;
    } else {
      pageSource = PAGESOURCE_ALLOC1;
      pageFlavor = ID_VTF_BAKE;
    }
  } else {
    pageSource = PAGESOURCE_ALLOC0;
    pageFlavor = ID_VTF_VMTR;
  }

  pageFileName = parmBlock.GetString(Resource(rp_landPageFile));
  if (pageFileName == nullptr || *pageFileName == '\0')
    pageFileName = virtualTextureSystem.vts[pageSource].GetBaseName();

  if (pageFlavor == ID_VTF_BAKE) {
    const int diffuseOnly =
        parmBlock.GetInteger(Resource(rp_landDiffuseOnly));
    if (diffuseOnly == 1)
      physicalPagesPool = PHYSICAL_PAGES_POOL_UNIQUE_DIFFUSE_ONLY;
    else if (diffuseOnly == 2)
      physicalPagesPool = PHYSICAL_PAGES_POOL_UNIQUE_DIFFUSE_ONLY2;
    else
      physicalPagesPool = PHYSICAL_PAGES_POOL_UNIQUE;
  } else {
    physicalPagesPool = PHYSICAL_PAGES_POOL_VMTR;
  }

  if (idStr::Cmpn(name.str, "generic", 7) != 0) {
    if (parmBlock.SetsRenderParm(Resource(rp_virtualMapping))) {
      virtualMapping = parmBlock.GetVector(Resource(rp_virtualMapping));
      SetVector(parmBlock, Resource(rp_useVirtualMapping),
                idVec4{1.0f, 1.0f, 1.0f, 1.0f});
    } else if (idLib::production < PROD_PRODUCTION) {
      if (!CVarBool("r_skipVmtrs"))
        devVmtr = virtualTextureSystem.VirtualMaterialForMaterial(this);
      UpdateVirtualMapping();
    }
  } else {
    if (idLib::production != PROD_BUILDING)
      parmBlock.Clear();
    coverage = MC_OPAQUE;
  }

  if (pageFlavor == ID_VTF_BAKE)
    DefaultVirtualTextureRenderParms(parmBlock);
  else
    SetupVirtualTextureRenderParms(parmBlock,
                                   virtualTextureSystem.vts[pageSource]);
  OptimizeRenderParmBlock();
}

const idImage* idMaterial::GetEditorImage() const {
  if (editorImage == nullptr)
    SetEditorImage();
  return editorImage;
}

bool idMaterial::IsContentType(int contentMask) const {
  return (parmBlock.GetInteger(Resource(rp_contentFlags)) & contentMask) != 0;
}

bool idMaterial::IsAutoMaterial() const {
  return std::strncmp(name.str, AUTO_MATERIAL_PREFIX,
                      std::strlen(AUTO_MATERIAL_PREFIX)) == 0;
}

idMaterial::~idMaterial() {
  if (devVmtr != nullptr)
    devVmtr->material = nullptr;
  delete materialMap;
  Clear();
}
