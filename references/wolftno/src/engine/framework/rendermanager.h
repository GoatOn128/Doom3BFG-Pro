#pragma once

#include "engine/renderer/rendersystem_runtime.h"

class idGame;
class idRenderModelGui;
struct gameReturn_t;

struct screenViewDef_t {
  const char* name;
  int playerNum;
  float x;
  float y;
  float width;
  float height;
  bool getsGlobalGuis;
  bool getsLocalGuis;
  std::uint8_t alignment[2];
  float eyeSeparation;
  std::uint8_t alignmentTail[4];
};

class idRenderManager {
public:
  idRenderManager();
  ~idRenderManager() = default;

  void Clear();
  void BuildGuiFrame(idRenderModelGui*, bool);
  void BuildGameFrame(const gameReturn_t&, idGame&, int);
  void UpdateConsole();
  void RenderFrame();
  void RenderFrameAndBeginAutomaticBackgroundSwaps(bool, bool);
  void ClearAllGuiModels();

private:
  void AddGlobalGui(idRenderModelGui*);

public:
  idRenderFrameInfo renderFrameInfo;
  const screenViewDef_t* currentViewDef;
};

extern idRenderManager renderManager;
static_assert(sizeof(screenViewDef_t)==40,"screenViewDef_t ABI drift");
static_assert(offsetof(idRenderManager,currentViewDef)==0x8520,
              "idRenderManager current view offset drift");
static_assert(sizeof(idRenderManager)==0x8530,"idRenderManager ABI drift");
