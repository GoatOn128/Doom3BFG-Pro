#pragma once

#include "engine/models/rendermodel.h"

#include <cstddef>

class idCVar;
class idDeclModelLOD;
class RenderModelLODValidationAccess;

class alignas(16) idRenderModelLOD final : public idRenderModel {
public:
  struct lod_t {
    idList<idRenderModel*> models;
    float maxDistanceSqr = 0.0f;
    float fade = 0.0f;
    bool visible = false;
    unsigned char recoveredPadding[3]{};
    dimShadow_t dimShadows = DIMSHADOW_OFF;
  };

  struct propagatedParms_t {
    idVec3 origin{};
    idVec3 scale{};
    idMat3 axis{};
  };

  idRenderModelLOD(const idDeclModelLOD* decl,
                   const idList<idRenderModel*>& models);
  ~idRenderModelLOD() override;

  const idDeclModelLOD* GetDecl() const { return decl; }
  int NumLods() const { return LODs.Num(); }
  int GetNumLODModels(int lod) const { return LODs[lod].models.Num(); }
  idRenderModel* GetLodModel(int lod, int model) const {
    return LODs[lod].models[model];
  }
  static idRenderModelLOD* CastToRenderModel(idRenderModel* model) {
    return model != nullptr && model->renderModelType == RENDERMODEL_LOD
               ? static_cast<idRenderModelLOD*>(model)
               : nullptr;
  }

private:
  friend class RenderModelLODValidationAccess;

  void UnlinkSubclass() override;
  void DeleteSubclass() override;
  bool UpdateInView(const idRenderView* currentView,
                    const idRenderView* nextView,
                    idRenderModelUpdateTools* updateTools) override;

  const idDeclModelLOD* decl = nullptr;
  idList<lod_t> LODs;
  int currentLOD = -1;
  int newLOD = -1;
  int keepLOD = -1;
  int forceVis = 0;
  int forceVisFrameCount = 0;
  int fadeTime = 1;
  int fadeTimestamp = 0;
  propagatedParms_t prevParm{};
};

extern idCVar r_forceLODIndex;
extern idCVar r_keepLowestLOD;
extern idCVar r_distanceScaleLOD;
extern idCVar r_fadeScaleLOD;

static_assert(sizeof(idRenderModelLOD::lod_t) == 40,
              "idRenderModelLOD::lod_t ABI drift");
static_assert(offsetof(idRenderModelLOD::lod_t, maxDistanceSqr) == 0x18,
              "idRenderModelLOD::lod_t distance offset drift");
static_assert(offsetof(idRenderModelLOD::lod_t, dimShadows) == 0x24,
              "idRenderModelLOD::lod_t shadow offset drift");
static_assert(sizeof(idRenderModelLOD::propagatedParms_t) == 60,
              "idRenderModelLOD propagated parameters ABI drift");
static_assert(sizeof(idRenderModelLOD) == 2304,
              "idRenderModelLOD ABI drift");
