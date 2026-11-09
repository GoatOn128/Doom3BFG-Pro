#pragma once

#include "engine/models/rendermodel.h"
#include "engine/models/renderworld.h"

class idImage;
class idMaterial;

class idRenderModelGodRays final : public idRenderModel {
public:
  idRenderModelGodRays();
  ~idRenderModelGodRays() override;

  void SetParms(const idMaterial* texture, bool isRelativeToCamera,
                const idVec3& direction, const idVec3& color, int size,
                int sourceSize);
  bool UpdateInView(const idRenderView* currentView,
                    const idRenderView* nextView,
                    idRenderModelUpdateTools* updateTools) override;

  const idImage* imgGodrays = nullptr;
  bool isRelativeToCamera = false;
  std::uint8_t recoveredPadding[3]{};
  idVec3 direction{};
  idVec3 color{};
  int sourceSize = 0;
  int size = 0;
  float fadeVal = 0.0f;
};

static_assert(offsetof(idRenderModelGodRays, imgGodrays) == 2176,
              "idRenderModelGodRays image offset drift");
static_assert(offsetof(idRenderModelGodRays, direction) == 2188,
              "idRenderModelGodRays direction offset drift");
static_assert(sizeof(idRenderModelGodRays) == 2224,
              "idRenderModelGodRays ABI drift");
