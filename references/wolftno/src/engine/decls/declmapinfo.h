#pragma once

#include "decl.h"
#include "shared/idlib/color.h"
#include "shared/idlib/langdict.h"

class idDeclMapInfo final : public idDeclTypeInfo {
public:
  struct loadingVideos_t {
    idAtomicString layerMask;
    idAtomicString videoFile;
    idColor fadeFromColor;
  };

  idDeclMapInfo();
  ~idDeclMapInfo() override;
  idDeclInfo* GetDeclInfo() const override;

  idStrId prettyMapName;
  int discNumber;
  int dlcId;
  int vmtrChunkNumber;
  int installChunkAllocationNumber;
  idList<loadingVideos_t> loadingVideos;

  static idDeclInfoTemplate<idDeclMapInfo> resourceList;
};

static_assert(sizeof(idDeclMapInfo::loadingVideos_t) == 32,
              "idDeclMapInfo::loadingVideos_t ABI drift");
static_assert(sizeof(idDeclMapInfo) == 144, "idDeclMapInfo ABI drift");
