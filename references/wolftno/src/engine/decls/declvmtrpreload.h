#pragma once

#include "decl.h"

struct preloadMaterial_t {
  idAtomicString material;
  int minLod;
};

class idDeclVmtrPreload final : public idDeclTypeInfo {
public:
  struct spawnVmtrList_t {
    idAtomicString spawnSpot;
    idList<preloadMaterial_t> preloadList;
  };

  idDeclVmtrPreload();
  ~idDeclVmtrPreload() override;
  idDeclInfo* GetDeclInfo() const override;
  const idList<preloadMaterial_t>& GetListForSpawnspot(const char* spawnSpot) const;

  idList<preloadMaterial_t> sharedList;
  idList<spawnVmtrList_t> preloadList;

  static idDeclInfoTemplate<idDeclVmtrPreload> resourceList;
};

static_assert(sizeof(preloadMaterial_t) == 16, "preloadMaterial_t ABI drift");
static_assert(sizeof(idDeclVmtrPreload::spawnVmtrList_t) == 32,
              "idDeclVmtrPreload::spawnVmtrList_t ABI drift");
static_assert(sizeof(idDeclVmtrPreload) == 144, "idDeclVmtrPreload ABI drift");
