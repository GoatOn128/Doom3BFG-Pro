#pragma once

#include "engine/models/detail/detailmodel.h"
#include "engine/models/rendermodel.h"
#include "engine/renderer/bufferobject.h"
#include "shared/idlib/csystems/cvarsystemlocal.h"

#include <cstddef>
#include <cstdint>

class idMaterial;
class idRenderView;
class idRenderModelUpdateTools;

class alignas(16) idRenderModelDetail : public idRenderModel {
public:
  idRenderModelDetail();

  void SetDetailModel(idDetailModel* model) { detailBSP = model; }

protected:
  ~idRenderModelDetail() override;

private:
  bool UpdateInView(const idRenderView* currentView,
                    const idRenderView* nextView,
                    idRenderModelUpdateTools* updateTools) override;

  friend struct idRenderModelDetailLayoutVerifier;
  friend struct idRenderModelDetailValidationAccess;

  idDetailModel* detailBSP;
  deferredDetailGatherParms_t* detailGatherParms;
  deferredDetailGenParms_t* detailGenParms;
  detailRenderView_t* viewData;
  int currentIndex;
  std::uint16_t* detailIndices;
  detailSubTreeStats_t* subTreeDetailCounts;
  detailGenTotals_t* totalsUsed;
  idVertexBuffer vertexBuffers[3];
  idIndexBuffer indexBuffers[3];
  idDrawVert* deferredVerts;
  std::uint16_t* deferredIndices;
  const idMaterial* detailMaterial;
};

extern idCVar r_useDetailGenJobs;
extern idCVar r_detailGenJobsPerSubTree;
extern idCVar r_showDetailNodeBounds;

struct idRenderModelDetailLayoutVerifier {
  static_assert(offsetof(idRenderModelDetail, detailBSP) == 2176,
                "idRenderModelDetail::detailBSP ABI drift");
  static_assert(offsetof(idRenderModelDetail, detailGatherParms) == 2184,
                "idRenderModelDetail::detailGatherParms ABI drift");
  static_assert(offsetof(idRenderModelDetail, detailGenParms) == 2192,
                "idRenderModelDetail::detailGenParms ABI drift");
  static_assert(offsetof(idRenderModelDetail, viewData) == 2200,
                "idRenderModelDetail::viewData ABI drift");
  static_assert(offsetof(idRenderModelDetail, currentIndex) == 2208,
                "idRenderModelDetail::currentIndex ABI drift");
  static_assert(offsetof(idRenderModelDetail, detailIndices) == 2216,
                "idRenderModelDetail::detailIndices ABI drift");
  static_assert(offsetof(idRenderModelDetail, subTreeDetailCounts) == 2224,
                "idRenderModelDetail::subTreeDetailCounts ABI drift");
  static_assert(offsetof(idRenderModelDetail, totalsUsed) == 2232,
                "idRenderModelDetail::totalsUsed ABI drift");
  static_assert(offsetof(idRenderModelDetail, vertexBuffers) == 2240,
                "idRenderModelDetail::vertexBuffers ABI drift");
  static_assert(offsetof(idRenderModelDetail, indexBuffers) == 2312,
                "idRenderModelDetail::indexBuffers ABI drift");
  static_assert(offsetof(idRenderModelDetail, deferredVerts) == 2384,
                "idRenderModelDetail::deferredVerts ABI drift");
  static_assert(offsetof(idRenderModelDetail, deferredIndices) == 2392,
                "idRenderModelDetail::deferredIndices ABI drift");
  static_assert(offsetof(idRenderModelDetail, detailMaterial) == 2400,
                "idRenderModelDetail::detailMaterial ABI drift");
};
static_assert(sizeof(idRenderModelDetail) == 2416,
              "idRenderModelDetail ABI drift");
