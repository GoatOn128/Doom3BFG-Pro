#pragma once

#include "engine/models/rendermodel.h"

class alignas(16) idRenderModelImposter final : public idRenderModel {
public:
  idRenderModelImposter();
  ~idRenderModelImposter() override;

  bool UpdateInView(const idRenderView* currentView,
                    const idRenderView* nextView,
                    idRenderModelUpdateTools* updateTools) override;
  bool MimicRenderModel(idRenderModel* parentModel);

  idRenderModel* parent = nullptr;
};

void UnlinkFromImposterChain(idRenderModel* imposter);

static_assert(offsetof(idRenderModelImposter, parent) == 2176,
              "idRenderModelImposter parent offset drift");
static_assert(sizeof(idRenderModelImposter) == 2192,
              "idRenderModelImposter ABI drift");
