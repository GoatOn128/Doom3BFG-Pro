#pragma once

#include "engine/models/static/jobs/statictransparencygen.h"
#include "engine/models/static/staticmodel.h"

#include <cstddef>

class idCVar;
struct idRenderModelStaticValidationAccess;
struct idRenderModelStaticLayoutAccess;

extern idCVar r_staticTransparencyFadeStartDist;
extern idCVar r_staticTransparencyFadeMaxDist;
extern idCVar r_useStaticTransparencyGenJobs;

class idRenderModelStatic : public idRenderModel {
public:
  explicit idRenderModelStatic(const idStaticModel* smod);

  void ChangeStaticModel(idStaticModel* model) {
    staticModel = model;
    reloadCount = -1;
  }
  const idStaticModel* GetStaticModel() const { return staticModel; }

  bool CommitSubclass() override;

protected:
  ~idRenderModelStatic() override;

private:
  friend struct idRenderModelStaticValidationAccess;
  friend struct idRenderModelStaticLayoutAccess;

  bool UpdateSurfaces();
  bool UpdateInView(const idRenderView* currentView,
                    const idRenderView* nextView,
                    idRenderModelUpdateTools* updateTools) override;
  const idList<sourceSurface_t, 5>* GetSourceSurfaces() const override;

  const idStaticModel* staticModel{};
  int skin{-1};
  int reloadCount{-1};
  idList<staticTransparencyGenParms_t*, 5> transparencyGenParms;
};

struct idRenderModelStaticLayoutAccess {
  static constexpr std::size_t StaticModelOffset() {
    return offsetof(idRenderModelStatic, staticModel);
  }
  static constexpr std::size_t SkinOffset() {
    return offsetof(idRenderModelStatic, skin);
  }
  static constexpr std::size_t ReloadCountOffset() {
    return offsetof(idRenderModelStatic, reloadCount);
  }
  static constexpr std::size_t TransparencyParmsOffset() {
    return offsetof(idRenderModelStatic, transparencyGenParms);
  }
};

static_assert(idRenderModelStaticLayoutAccess::StaticModelOffset() == 0x880,
              "idRenderModelStatic static-model offset drift");
static_assert(idRenderModelStaticLayoutAccess::SkinOffset() == 0x888,
              "idRenderModelStatic skin offset drift");
static_assert(idRenderModelStaticLayoutAccess::ReloadCountOffset() == 0x88C,
              "idRenderModelStatic reload-count offset drift");
static_assert(idRenderModelStaticLayoutAccess::TransparencyParmsOffset() ==
                  0x890,
              "idRenderModelStatic transparency parms offset drift");
static_assert(sizeof(idRenderModelStatic) == 0x8A8,
              "idRenderModelStatic ABI drift");
