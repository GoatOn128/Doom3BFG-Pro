#pragma once

#include "engine/common/contents.h"
#include "engine/common/surfacetypes.h"
#include "engine/decls/decl.h"
#include "engine/renderer/materialmap.h"
#include "engine/renderer/parmblock.h"
#include "engine/renderer/virtualtexture.h"
#include "shared/idlib/math/vector.h"

#include <cstddef>
#include <cstdint>

class idDeclRenderProg;
class idDeclSkins;
class idImage;

class idContents {
public:
  idContents();
  int ToFlags() const;
  void FromFlags(const int flags);
  static int ContentsFromString(const char* text, const char* separator = "|");

  union {
    struct {
      std::uint8_t solid : 1;
      std::uint8_t opaque : 1;
      std::uint8_t water : 1;
      std::uint8_t playerClip : 1;
      std::uint8_t monsterClip : 1;
      std::uint8_t vehicleClip : 1;
      std::uint8_t moveableClip : 1;
      std::uint8_t shotClip : 1;
      std::uint8_t ikClip : 1;
      std::uint8_t aiAware : 1;
      std::uint8_t ai : 1;
      std::uint8_t projectile : 1;
      std::uint8_t corpse : 1;
      std::uint8_t trigger : 1;
      std::uint8_t player : 1;
      std::uint8_t vehicle : 1;
      std::uint8_t obstacle : 1;
      std::uint8_t playerCoverClip : 1;
      std::uint8_t monsterCoverClip : 1;
      std::uint8_t playerFocus : 1;
      std::uint8_t pushable : 1;
      std::uint8_t shield : 1;
      std::uint8_t noCover : 1;
      std::uint8_t preserved : 1;
    };
    std::uint8_t bytes[3];
  };

  static const char* contentsNames[34];
};

enum surfaceFlags_t : std::uint32_t {
  SURF_UNUSED_1 = 0x00000001u,
  SURF_UNUSED_2 = 0x00000002u,
  SURF_UNUSED_3 = 0x00000004u,
  SURF_FRACTURE = 0x00000008u,
  SURF_NODAMAGE = 0x00000010u,
  SURF_SLICK = 0x00000020u,
  SURF_COLLISION = 0x00000040u,
  SURF_LADDER = 0x00000080u,
  SURF_NOIMPACT = 0x00000100u,
  SURF_NOSTEPS = 0x00000200u,
  SURF_STAIRS = 0x00000400u,
  SURF_OCCLUSION_TEST = 0x00000800u,
  SURF_SHADOW_CASTER = 0x00001000u,
  SURF_NULLNORMAL = 0x00002000u,
  SURF_NOAREAS = 0x00004000u,
  SURF_NON_PENETRABLE = 0x00008000u,
  SURF_NOSHOOT = 0x00010000u,
  SURF_CAULK = 0x00020000u,
  SURF_UNUSED_7 = 0x00040000u,
  SURF_UNUSED_8 = 0x00080000u,
  SURF_UNUSED_9 = 0x00100000u,
  SURF_LADDER_TOP = 0x40000000u,
};

enum fractureInfo_t : int {
  FRACTYPE_NONE = 0x00000000,
  FRACTYPE_SMALL = 0x00000001,
  FRACTYPE_MEDIUM = 0x00000002,
  FRACTYPE_LARGE = 0x00000003,
  FRACCONTENT_NONE = 0x00000000,
  FRACCONTENT_LIQUID = 0x00000100,
  FRACFLAG_NONE = 0x00000000,
  FRACFLAG_FRAGILE = 0x01000000,
  FRACTYPE_MASK = 0x000000FF,
  FRACCONTENT_MASK = 0x0000FF00,
  FRACFLAG_MASK = static_cast<int>(0xFF000000u),
};

enum materialCoverage_t : int {
  MC_BAD = 0,
  MC_INVISIBLE = 1,
  MC_PERFORATED = 2,
  MC_TRANSLUCENT = 3,
  MC_OPAQUE = 4,
  MC_PERFORATED_FOR_MODELFADE = 5,
};

enum materialProg_t : int {
  MPR_COVERAGE,
  MPR_INTERACTION,
  MPR_SHADOW,
  MPR_SSS,
  MPR_SSSMASK,
  MPR_ADD,
  MPR_BLEND,
  MPR_STAGE,
  MPR_FADE_PERTURB,
  MPR_MAX,
};

enum autospriteType_t : int {
  AUTOSPRITE_NONE,
  AUTOSPRITE_VIEW_ORIENTED,
  AUTOSPRITE_LONGEST_AXIS_ALIGNED,
  AUTOSPRITE_X_AXIS_ALIGNED,
  AUTOSPRITE_Y_AXIS_ALIGNED,
  AUTOSPRITE_Z_AXIS_ALIGNED,
};

enum decalProjType_t : int {
  DECALPROJ_PLANAR,
  DECALPROJ_SPHERICAL,
};

class idMaterial : public idDecl {
public:
  idMaterial();
  ~idMaterial() override;

  idDeclInfo* GetDeclInfo() const override;
  void Parse(idParser& source) override;
  void FreeData() override;
  const char* DefaultDefinition() const override;
  bool SetImplicitText() override;
  std::uint32_t GetDeclTimestamp() override {
    return static_cast<const idMaterial*>(this)->GetDeclTimestamp();
  }
  std::uint32_t GetDeclTimestamp() const;
  void ReParse(bool fromDisk) override;
  void List() const override;
  void Print() override { static_cast<const idMaterial*>(this)->Print(); }
  void Print() const;

  static bool IsValidMaterialName(const char* name, bool allowUnderscore,
                                  bool allowAmpersand);
  idStr* Write() const;
  void SetupVirtualTexture() const;
  surfTypes_t GetSurfaceType() const;
  surfaceFlags_t GetSurfaceFlags() const;
  fractureInfo_t GetFractureInfo() const;
  idVec4 GetFractureColorType() const;
  idVec4 GetFractureColorContent() const;
  void UpdateVirtualMapping() const;
  bool MaterialIsMergableWith(const idMaterial* testMaterial) const;
  bool IsEqualForRender(const idMaterial* other) const;
  const float* GetMaterialMapping(const idMaterial* material) const;
  bool MaterialMappingsAreCompatible(const idMaterial* material) const;
  bool GetImageDimensions(int& width, int& height) const;
  bool IsVMTRVirtualTexture() const;
  idStr NameWithoutAutoMaterial() const;
  void ReloadImages(bool force) const;
  static void InitCompatibleGenericMaterials();
  static const idMaterial* PackMaterials(
      const char* name, const idList<const idMaterial*, 5>& materials,
      unsigned int skinFileTimeStamp);
  const idMaterial* CompatibleGenericMaterial() const;
  bool MaterialMappingNeedsReload(const idDeclSkins* skins) const;
  bool MaterialForcesNormalSmoothing() const;
  int GetWidth() const;
  int GetHeight() const;
  void OptimizeRenderParmBlock();

  materialCoverage_t Coverage() const { return coverage; }
  bool MaterialHasInteractions() const { return progs[MPR_INTERACTION] != nullptr; }
  bool MaterialHasSubSurfaceScattering() const { return progs[MPR_SSS] != nullptr; }
  bool SkipSurfaceInfo() const { return (flags1 & 0x02u) != 0; }
  bool MaterialCastsShadows() const { return (flags0 & 0x01u) != 0; }
  bool MaterialUsesApproximatedLighting() const { return (flags0 & 0x20u) != 0; }
  bool MaterialHasMergeableBlendedPass() const { return (flags1 & 0x01u) != 0; }
  bool MaterialIsTransparencySorted() const { return (flags0 & 0x08u) != 0; }
  bool MaterialUsesTransSortAtlas() const { return (flags0 & 0x10u) != 0; }
  bool MaterialUsesWrinkleMaps() const { return (flags0 & 0x40u) != 0; }
  bool MaterialUsesPostSpecular() const { return (flags0 & 0x80u) != 0; }
  bool MaterialIsTwoSided() const { return (flags0 & 0x02u) != 0; }
  bool MaterialIsMixedUnique() const { return (flags0 & 0x04u) != 0; }
  bool MaterialIsSky() const { return (flags1 & 0x20u) != 0; }
  bool MaterialHasClipSpheres() const { return (flags1 & 0x04u) != 0; }
  bool MaterialShouldBakeStaticSpecular() const { return (flags1 & 0x08u) != 0; }
  bool MaterialIsFracture() const { return GetFractureInfo() != FRACTYPE_NONE; }
  bool MaterialIsSpecialMissing() const { return (flags1 & 0x10u) != 0; }
  pageSource_t VirtualTexturePageSource() const { return pageSource; }
  virtualTextureFlavor_t VirtualTexturePageFlavor() const { return pageFlavor; }
  bool IsUniqueVirtualTexture() const { return pageFlavor == ID_VTF_BAKE; }
  bool IsVirtualTexture() const { return pageFileName != nullptr; }
  bool IsAutoMaterial() const;
  const char* PageFileName() const { return pageFileName; }
  bool PageFileDiffuseOnly() const {
    return physicalPagesPool == PHYSICAL_PAGES_POOL_UNIQUE_DIFFUSE_ONLY ||
           physicalPagesPool == PHYSICAL_PAGES_POOL_UNIQUE_DIFFUSE_ONLY2;
  }
  physicalPagesPool_t GetPhysicalPagesPool() const { return physicalPagesPool; }
  autospriteType_t GetAutoSpriteType() const { return materialAutospriteType; }
  decalProjType_t GetDecalProjType() const { return materialDecalProjType; }
  const idDeclRenderProg* GetProg(materialProg_t index) const { return progs[index]; }
  const idImage* GetEditorImage() const;
  bool IsContentType(int flags) const;
  bool HasMaterialMap() const { return materialMap != nullptr; }
  const idMaterialMap* GetMaterialMap() const { return materialMap; }
  float GetFloat(const idDeclRenderParm* parm) const { return parmBlock.GetFloat(parm); }
  int GetInteger(const idDeclRenderParm* parm) const { return parmBlock.GetInteger(parm); }
  idVec4 GetVector(const idDeclRenderParm* parm) const { return parmBlock.GetVector(parm); }
  const idImage* GetImage(const idDeclRenderParm* parm) const { return parmBlock.GetImage(parm); }
  const char* GetString(const idDeclRenderParm* parm) const { return parmBlock.GetString(parm); }
  const idParmBlock& GetParmBlock() const { return parmBlock; }
  const idParmBlock& GetRenderParmBlock() const { return renderParmBlock; }
  int StorageSize() const { return sizeof(*this); }
  unsigned TimeStamp() const { return GetDeclTimestamp(); }
  idVirtualMaterial* GetVmtr() const { return devVmtr; }
  bool GetVirtualMapping(idVec4& mapping) const {
    mapping = virtualMapping;
    return mapping.x != 0.0f || mapping.y != 0.0f ||
           mapping.z != 0.0f || mapping.w != 0.0f;
  }

  idVec4 atlasScaleBias{};
  union {
    struct {
      std::uint8_t materialCastsShadows : 1;
      std::uint8_t materialIsTwoSided : 1;
      std::uint8_t materialIsMixedUnique : 1;
      std::uint8_t materialIsTransparencySorted : 1;
      std::uint8_t materialUsesTransSortAtlas : 1;
      std::uint8_t materialUsesApproximatedLighting : 1;
      std::uint8_t materialUsesWrinkleMaps : 1;
      std::uint8_t materialUsesPostSpecular : 1;
      std::uint8_t materialHasMergeableBlendedPass : 1;
      std::uint8_t skipSurfaceInfo : 1;
      std::uint8_t materialHasClipSpheres : 1;
      std::uint8_t materialShouldBakeStaticSpecular : 1;
      std::uint8_t materialIsSpecialMissing : 1;
      std::uint8_t materialIsSky : 1;
      std::uint8_t materialHasCheckedCompatible : 1;
      std::uint8_t preservedFlag : 1;
    };
    struct {
      mutable std::uint8_t flags0;
      mutable std::uint8_t flags1;
    };
  };
  std::uint8_t alignmentBeforeAutosprite[2]{};
  autospriteType_t materialAutospriteType{AUTOSPRITE_NONE};
  decalProjType_t materialDecalProjType{DECALPROJ_PLANAR};
  std::uint8_t alignmentBeforeParmBlock[4]{};
  mutable idParmBlock parmBlock{};
  mutable idParmBlock renderParmBlock{};
  mutable pageSource_t pageSource{PAGESOURCE_INVALID};
  mutable virtualTextureFlavor_t pageFlavor{ID_VTF_UNKNOWN};
  mutable const char* pageFileName{};
  mutable physicalPagesPool_t physicalPagesPool{PHYSICAL_PAGES_POOL_VMTR};
  std::uint8_t alignmentBeforePrograms[4]{};
  const idDeclRenderProg* progs[MPR_MAX]{};
  float stageSort[MPR_MAX]{};
  float stageSortAdjust{};
  idMaterialMap* materialMap{};
  mutable idVirtualMaterial* devVmtr{};
  mutable idVec4 virtualMapping{};
  mutable const idImage* editorImage{};
  materialCoverage_t coverage{MC_BAD};
  std::uint8_t alignmentBeforeCompatible[4]{};
  mutable const idMaterial* compatibleGenericMaterial{};

  static idDeclInfoTemplate<idMaterial> resourceList;

private:
  void Clear();
  void SetEditorImage() const;
  bool ParseMaterialMap(idParser& source);
};

const idMaterial* FindMaterialOrAutoMaterial(const char* name);

static_assert(sizeof(idContents) == 3, "idContents ABI drift");
static_assert(offsetof(idMaterial, parmBlock) == 112,
              "idMaterial parmBlock offset drift");
static_assert(offsetof(idMaterial, renderParmBlock) == 160,
              "idMaterial render parm-block offset drift");
static_assert(offsetof(idMaterial, pageSource) == 208,
              "idMaterial page-source offset drift");
static_assert(offsetof(idMaterial, progs) == 232,
              "idMaterial program offset drift");
static_assert(offsetof(idMaterial, stageSort) == 304,
              "idMaterial stage-sort offset drift");
static_assert(offsetof(idMaterial, materialMap) == 344,
              "idMaterial material-map offset drift");
static_assert(offsetof(idMaterial, coverage) == 384,
              "idMaterial coverage offset drift");
static_assert(sizeof(idMaterial) == 400, "idMaterial ABI drift");
