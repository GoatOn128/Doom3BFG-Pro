#pragma once

#include "engine/models/detail/decldetail.h"
#include "engine/models/detail/jobs/detailgen.h"
#include "shared/idlib/containers/staticlist.h"
#include "shared/idlib/csystems/cvarsystemlocal.h"

#include <cstddef>
#include <cstdint>

struct dm_subTree_t {
  dm_subTreeBounds_t bounds;
  std::uint16_t maxDetailHeight;
  std::uint16_t totalTreeNodeSize;
  int totalDataSize;
  dm_subTreeTreeNodes_t* treeNodes;
  dm_subTreeData_t* data;
};

struct dm_model_t {
  int numTreeNodes;
  int numSubTrees;
  dm_treeNode_t* treeNodes;
  dm_subTree_t* subTrees;
};

struct dm_modelStats_t {
  int numDetailNodes;
  int numTreeNodes;
  int numSubTrees;
};

class idDetailModel : public idResource {
public:
  idDetailModel();
  ~idDetailModel() override;

  void LoadResource() override;
  idResourceList* GetResourceList() const override;
  bool Load_Binary(const char* detailFileName);

  static void GetDetailSubTreesForView_r(
      const dm_model_t& model, const std::int16_t (*frustumVerts)[8],
      int* subTreeNums, int& numSubTrees, int nodeNum);
  static int GetDetailSubTreesForView(const dm_model_t& model,
                                      const detailRenderView_t* viewData,
                                      int* subTreeNums);

  dm_model_t detailModelData;
  idList<const idDeclDetail*, 5> sharedAttributes;
  dm_modelStats_t stats;

  static idTypedResourceList<idDetailModel> resourceList;
  static idStaticList<detailPaletteEntry_t, 128> paletteEntries;
  static idStaticList<detailPaletteVert_t, 1024> paletteVerts;
  static idStaticList<std::uint16_t, 4096> paletteIndices;

private:
  void FreeData();
  static void SwapModelTreeNodes(dm_model_t* model);
  static void SwapModelSubTrees(dm_subTreeData_t* data,
                                dm_subTreeTreeNodes_t* treeNodes);
};

template <>
idDetailModel* idTypedResourceList<idDetailModel>::Alloc(
    const char* resourceName) const;

extern idCVar r_detailFadeDistMax;

static_assert(sizeof(dm_subTree_t) == 40);
static_assert(offsetof(dm_subTree_t, maxDetailHeight) == 16);
static_assert(offsetof(dm_subTree_t, totalTreeNodeSize) == 18);
static_assert(offsetof(dm_subTree_t, totalDataSize) == 20);
static_assert(offsetof(dm_subTree_t, treeNodes) == 24);
static_assert(offsetof(dm_subTree_t, data) == 32);
static_assert(sizeof(dm_model_t) == 24);
static_assert(sizeof(dm_modelStats_t) == 12);
static_assert(sizeof(idDetailModel) == 112);
static_assert(offsetof(idDetailModel, detailModelData) == 48);
static_assert(offsetof(idDetailModel, sharedAttributes) == 72);
static_assert(offsetof(idDetailModel, stats) == 96);
