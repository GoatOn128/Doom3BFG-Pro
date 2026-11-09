#pragma once

#include "decl.h"

class idMaterial;

struct idSkinMapping {
  const idMaterial* from;
  const idMaterial* to;
};

struct idSkin {
  const idMaterial* Remap(const idMaterial* material) const;

  idAtomicString name;
  idList<idSkinMapping, 46> mappings;
};

class idDeclSkins final : public idDecl {
public:
  idDeclSkins();
  ~idDeclSkins() override;
  const char* DefaultDefinition() const override;
  bool RebuildTextSource() override;
  void Parse(idParser& source) override;
  void FreeData() override;
  idDeclInfo* GetDeclInfo() const override;
  int IndexForSkin(const char* skinName) const;

  idList<idSkin, 46> skins;
  static idDeclInfoTemplate<idDeclSkins> resourceList;
};

static_assert(sizeof(idSkinMapping) == 16, "idSkinMapping ABI drift");
static_assert(sizeof(idSkin) == 32, "idSkin ABI drift");
static_assert(sizeof(idDeclSkins) == 104, "idDeclSkins ABI drift");
