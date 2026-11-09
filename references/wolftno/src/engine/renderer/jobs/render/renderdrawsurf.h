#pragma once

#include "engine/renderer/jobs/render/render_types.h"
#include "engine/renderer/jobs/render/rendercapture.h"

#include <cstddef>
#include <cstdint>

class idDeclRenderProg;
class idRenderDestination;
struct idRenderModelCommitted;
class idRenderModelSurface;
class idRenderView;
struct renderSettings_t;

struct renderDrawSurfParms_t {
  const renderSettings_t* settings;
  const idRenderView* renderView;
  bool allowInGameGUIs;
  std::uint8_t alignmentBeforeCaptureParms[7];
  captureParms_t captureParms;
  const idRenderDestination* renderDestDefault;
  const idRenderDestination* renderDestGui;
};

class idRenderDrawSurf {
public:
  ~idRenderDrawSurf();

  void PopSurface();
  void PopModel();
  void RenderDrawSurf(const renderDrawSurfParms_t* parameters,
                      const drawSurf_t* surface, std::uint64_t extraState);
  void RenderDrawSurfProgram(const renderDrawSurfParms_t* parameters,
                             const drawSurf_t* surface,
                             const idDeclRenderProg* program);

  const idRenderModelCommitted* currentModel = nullptr;
  const idRenderModelSurface* currentSurface = nullptr;

private:
  void PushModel(const renderDrawSurfParms_t* parameters,
                 const idRenderModelCommitted* model);
};

void RenderGui(const idRenderModelCommitted* model,
               const renderSettings_t* settings,
               const idRenderView* renderView, bool inGameGui);

static_assert(offsetof(renderDrawSurfParms_t, captureParms) == 0x18,
              "draw-surface capture parameters offset drift");
static_assert(offsetof(renderDrawSurfParms_t, renderDestDefault) == 0x1A8,
              "draw-surface default destination offset drift");
static_assert(offsetof(renderDrawSurfParms_t, renderDestGui) == 0x1B0,
              "draw-surface GUI destination offset drift");
static_assert(sizeof(renderDrawSurfParms_t) == 0x1B8,
              "draw-surface parameters ABI drift");
static_assert(sizeof(idRenderDrawSurf) == 0x10,
              "draw-surface renderer ABI drift");
