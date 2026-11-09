#pragma once

#include "engine/decls/decl.h"
#include "engine/models/detail/jobs/detailgen.h"

#include <cstddef>

struct paletteStats_t {
  int totalNumVerts;
  int totalNumIndices;
  int totalNumModels;
};

class idDetailModelData : public idResource {
public:
  __forceinline idDetailModelData() : paletteData(nullptr), paletteIdx(-1) {
    using FreeDataMethod = void (idDetailModelData::*)();
    FreeDataMethod volatile freeDataMethod = &idDetailModelData::FreeData;
    (this->*freeDataMethod)();
  }
  ~idDetailModelData() override { FreeData(); }

  void LoadResource() override;
  idResourceList* GetResourceList() const override;
  __declspec(noinline) void FreeData();
  bool Generate();

  idDetailPaletteData* paletteData;
  int paletteIdx;

  static idTypedResourceList<idDetailModelData> resourceList;
  static idList<idDetailPaletteData*, 98> palette;
};

template <>
idDetailModelData*
idTypedResourceList<idDetailModelData>::Alloc(const char* name) const;
template <>
int idTypedResourceList<idDetailModelData>::AllocSize() const;

static_assert(sizeof(paletteStats_t) == 12);
static_assert(sizeof(idDetailModelData) == 64);
static_assert(offsetof(idDetailModelData, paletteData) == 48);
static_assert(offsetof(idDetailModelData, paletteIdx) == 56);
