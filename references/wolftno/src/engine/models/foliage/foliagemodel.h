#pragma once

#include "engine/decls/decl.h"
#include "engine/models/foliage/declfoliage.h"
#include "engine/models/foliage/jobs/foliagegather.h"
#include "shared/idlib/csystems/cvarsystemlocal.h"

#include <cstddef>
#include <cstdint>

struct fm_subTree_t {
  fm_subTreeBounds_t bounds;
  std::uint16_t maxFoliageHeight;
  std::uint16_t totalTreeNodeSize;
  int totalDataSize;
  fm_subTreeTreeNodes_t* treeNodes;
  fm_subTreeData_t* data;
};

struct fm_model_t {
  int numTreeNodes;
  int numSubTrees;
  fm_treeNode_t* treeNodes;
  fm_subTree_t* subTrees;
};

struct fm_modelStats_t {
  int numFoliageNodes;
  int numTreeNodes;
  int numSubTrees;
};

class idFoliageModel : public idResource {
public:
  idFoliageModel();
  ~idFoliageModel() override;

  void LoadResource() override;
  idResourceList* GetResourceList() const override;
  bool Load_Binary(const char* foliageFileName);
  void FreeData();

  static void GetFoliageSubTreesForView_r(
      const fm_model_t& model, const std::int16_t (*const frustumVerts)[8],
      int* subTreeNums, int& numSubTrees, int nodeNum);
  static int GetFoliageSubTreesForView(const fm_model_t& model,
                                       const foliageRenderView_t* viewData,
                                       int* subTreeNums);

  fm_model_t foliageModelData;
  idList<const idDeclFoliage*, 5> sharedAttributes;
  fm_modelStats_t stats;

  static idTypedResourceList<idFoliageModel> resourceList;

private:
  static void InitializeAttributes(fm_model_t* model,
                                   idList<const idDeclFoliage*, 5>& uniqueDecls,
                                   unsigned int magic);
  static void SwapModelTreeNodes(fm_model_t* model);
  static void SwapModelSubTrees(fm_subTreeData_t* data,
                                fm_subTreeTreeNodes_t* treeNodes);
};

extern idCVar r_foliageStartFadeDist;
extern idCVar r_foliageSmallFadeDistMax;
extern idCVar r_foliageBigFadeDistMax;

static_assert(sizeof(fm_subTree_t) == 40);
static_assert(offsetof(fm_subTree_t, maxFoliageHeight) == 16);
static_assert(offsetof(fm_subTree_t, treeNodes) == 24);
static_assert(offsetof(fm_subTree_t, data) == 32);
static_assert(sizeof(fm_model_t) == 24);
static_assert(sizeof(fm_modelStats_t) == 12);
static_assert(sizeof(idFoliageModel) == 112);
static_assert(offsetof(idFoliageModel, foliageModelData) == 48);
static_assert(offsetof(idFoliageModel, sharedAttributes) == 72);
static_assert(offsetof(idFoliageModel, stats) == 96);
