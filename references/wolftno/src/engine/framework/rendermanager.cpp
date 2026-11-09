#include "engine/framework/rendermanager.h"

#include "engine/framework/common_local.h"
#include "engine/framework/console.h"
#include "engine/framework/debughud.h"
#include "engine/framework/game_network.h"
#include "engine/framework/session.h"
#include "engine/models/rendermodelgui.h"
#include "engine/renderer/renderersystem.h"
#include "shared/idlib/color.h"
#include "shared/idlib/csystems/cvarsystemlocal.h"
#include "shared/idlib/text/str.h"

#include <cstdint>
#include <cstring>

extern int com_editors;
extern idCVar com_skipGameRenderView;
extern idCVar net_drawDebugHud;
extern idCVar r_znear;
extern idCVar r_zfar;

idCVar stereoRender_separation(
    "stereoRender_separation", "1.5", CVAR_FLOAT,
    "world units from center to eyes");
idCVar stereoRender_screenSeparation(
    "stereoRender_screenSeparation", "-0.3", CVAR_FLOAT,
    "screen units from center to eyes");
idCVar stereoRender_guiOffset(
    "stereoRender_guiOffset", "-0.3", CVAR_FLOAT,
    "shift guis so they don't appear at infinity in HMDs");
idCVar stereoRender_swapEyes(
    "stereoRender_swapEyes", "0", CVAR_FLOAT,
    "swap target buffers for left and right eyes");
idCVar multiView_fullRender(
    "multiView_fullRender", "1", CVAR_BOOL,
    "0 = alternate frame rendering, 1 = render both each frame");
idCVar r_loadingIconSize("r_loadingIconSize", "0.1", CVAR_FLOAT,
                         "size of the loading icon");
idCVar r_loadingIconSpeed("r_loadingIconSpeed", "0.5", CVAR_FLOAT,
                          "rotation speed of the loading icon");
idCVar r_dialogIconX(
    "r_dialogIconX", "0.0", CVAR_FLOAT,
    "horizontal screen fraction at which the loading icon is placed");
idCVar r_dialogIconY(
    "r_dialogIconY", "-0.84", CVAR_FLOAT,
    "vertical screen fraction at which the loading icon is placed");

static const screenViewDef_t screenViewDefs[] = {
    {"single", 0, 0, 0, 0, 0, false, false, {}, 0, {}},
    {"", 0, 0, 0, 1, 1, false, false, {}, 0, {}},
    {"", -1, 0, 0, 1, 1, true, true, {}, 0, {}},
    {"splitScreen", 0, 0, 0, 0, 0, false, false, {}, 0, {}},
    {"", 0, 0, 0, 1, 0.5f, false, true, {}, 0, {}},
    {"", 1, 0, 0.5f, 1, 0.5f, false, true, {}, 0, {}},
    {"", -1, 0, 0, 1, 1, true, false, {}, 0, {}},
    {"topBottomStereo", 0, 0, 0, 0, 0, false, false, {}, 0, {}},
    {"", 0, 0, 0, 1, 0.5f, true, true, {}, -1, {}},
    {"", 0, 0, 0.5f, 1, 0.5f, true, true, {}, 1, {}},
    {"leftRightStereo", 0, 0, 0, 0, 0, false, false, {}, 0, {}},
    {"", 0, 0, 0, 0.5f, 1, true, true, {}, -1, {}},
    {"", 0, 0.5f, 0, 0.5f, 1, true, true, {}, 1, {}},
    {"HDMI3D", 0, 0, 0, 0, 0, false, false, {}, 0, {}},
    {"", 0, 0, 0, 1, 0.4897959f, true, true, {}, -1, {}},
    {"", 0, 0, 0.5102041f, 1, 0.4897959f, true, true, {}, 1, {}},
    {"HDMI3DtwoPlayer", 0, 0, 0, 0, 0, false, false, {}, 0, {}},
    {"", 0, 0, 0, 1, 0.4897959f, true, true, {}, -1, {}},
    {"", 1, 0, 0.5102041f, 1, 0.4897959f, true, true, {}, 1, {}},
    {"end of list", 0, 0, 0, 0, 0, false, false, {}, 0, {}},
    {nullptr, 0, 0, 0, 0, 0, false, false, {}, 0, {}}};

static const screenViewDef_t* FindViewDefinition(const char* name) {
  const screenViewDef_t* definition = screenViewDefs;
  while (definition->name != nullptr) {
    if (idStr::Icmp(name, definition->name) == 0) {
      return definition + 1;
    }
    ++definition;
  }
  return &screenViewDefs[1];
}

idRenderFrameInfo::idRenderFrameInfo()
    : screenViews(), worldViews(), captureFrame(false) {}

idRenderFrameInfo::~idRenderFrameInfo() = default;

void idRenderFrameInfo::Clear() {
  screenViews.ClearFree();
  worldViews.ClearFree();
  captureFrame = false;
}

idRenderManager::idRenderManager()
    : renderFrameInfo(), currentViewDef(FindViewDefinition("single")) {}

void idRenderManager::Clear() { renderFrameInfo.Clear(); }

void idRenderManager::RenderFrame() {
  const bool editorView = (com_editors & 2) != 0;
  renderSystem->RenderFrame(renderSystem->GetGameWindow(), renderFrameInfo,
                            !editorView, false, editorView);
}

void idRenderManager::ClearAllGuiModels() {
  if (console->Gui() != nullptr) {
    console->Gui()->Clear(0x28u);
  }
  for (int viewIndex = 0; viewIndex < renderFrameInfo.screenViews.num;
       ++viewIndex) {
    idScreenView& view = renderFrameInfo.screenViews[viewIndex];
    for (int guiIndex = 0; guiIndex < view.viewGuis.num; ++guiIndex) {
      idRenderModel* model = view.viewGuis[guiIndex];
      if (model != nullptr && model->renderModelType != RENDERMODEL_GUI) {
        static_cast<idRenderModelGui*>(model)->Clear(0x28u);
      }
    }
  }
}

void idRenderManager::RenderFrameAndBeginAutomaticBackgroundSwaps(
    bool loadingIcon, bool oldFrameInAuto) {
  const bool showLoadingIcon =
      !renderSystem->HasRenderFlag(0x310) && loadingIcon;
  renderSystem->SetLoadingIconInfo(
      r_dialogIconX.valueFloat, r_dialogIconY.valueFloat,
      r_loadingIconSize.valueFloat, r_loadingIconSpeed.valueFloat,
      showLoadingIcon);
  renderSystem->RenderFrame(renderSystem->GetGameWindow(), renderFrameInfo,
                            false, false, false);
  renderSystem->BeginAutomaticBackgroundSwaps(renderSystem->GetGameWindow(),
                                               loadingIcon, oldFrameInAuto);
}

void idRenderManager::AddGlobalGui(idRenderModelGui* gui) {
  if (gui == nullptr) {
    return;
  }
  for (int index = 0; index < renderFrameInfo.screenViews.num; ++index) {
    if (currentViewDef[index].getsGlobalGuis) {
      renderFrameInfo.screenViews[index].viewGuis.Append(gui);
    }
  }
}

void idRenderManager::BuildGuiFrame(idRenderModelGui* baseGui, bool loading) {
  renderFrameInfo.Clear();
  for (int index = 0; currentViewDef[index].name[0] == '\0'; ++index) {
    idScreenView* view = renderFrameInfo.screenViews.Alloc();
    const screenViewDef_t& definition = currentViewDef[index];
    view->screenRect.x1 =
        static_cast<int>(renderSystem->GetScanoutWidth() * definition.x);
    view->screenRect.y1 =
        static_cast<int>(renderSystem->GetScanoutHeight() * definition.y);
    view->screenRect.x2 = static_cast<int>(
        renderSystem->GetScanoutWidth() * (definition.width + definition.x) -
        1.0f);
    view->screenRect.y2 = static_cast<int>(
        renderSystem->GetScanoutHeight() * (definition.height + definition.y) -
        1.0f);
    view->viewIndex = -1;
    view->usePreviousRendering = false;
    view->world = nullptr;
    view->viewGuis.ClearFree();
    view->guiOriginOffset =
        stereoRender_guiOffset.valueFloat * definition.eyeSeparation;
  }

  if (!loading) {
    AddGlobalGui(baseGui);
    const int height = renderSystem->GetHeight();
    const int width = renderSystem->GetWidth();
    console->Gui()->SetViewport(0, 0, width, height);
    console->Draw(false, true);
    AddGlobalGui(console->Gui());
    if (commonLocal.DialogGuiModel() != nullptr) {
      commonLocal.RenderDialog(false);
      AddGlobalGui(commonLocal.DialogGuiModel());
    }
    renderFrameInfo.captureFrame = true;
  }
}

void idRenderManager::UpdateConsole() {
  BuildGuiFrame(nullptr, false);
  renderSystem->RenderFrame(renderSystem->GetGameWindow(), renderFrameInfo,
                            true, false, false);
}

void idRenderManager::BuildGameFrame(const gameReturn_t& gameReturnRef,
                                     idGame& gameRef, int frameNumber) {
  const gameReturn_t* gameReturn=&gameReturnRef;
  idGame* game=&gameRef;
  idRenderWorld* world = game->GetRenderWorld();
  int viewIndexToPlayerIndex[4]{};
  int playerCount = 0;
  for (int inputDevice = 0; inputDevice < 4; ++inputDevice) {
    const int playerIndex = game->GetPlayerIndexFromInputDevice(inputDevice);
    if (playerIndex >= 0 && gameReturn->players[playerIndex].valid) {
      viewIndexToPlayerIndex[playerCount++] = playerIndex;
    }
  }

  if (playerCount == 2) {
    currentViewDef = FindViewDefinition("splitscreen");
  }

  renderFrameInfo.Clear();
  int playerMask = 0;
  int worldViewIndex = 0;
  for (int screenIndex = 0; currentViewDef[screenIndex].name[0] == '\0';
       ++screenIndex) {
    const screenViewDef_t& definition = currentViewDef[screenIndex];
    idScreenView* view = renderFrameInfo.screenViews.Alloc();
    view->screenRect.x1 =
        static_cast<int>(renderSystem->GetScanoutWidth() * definition.x);
    view->screenRect.y1 =
        static_cast<int>(renderSystem->GetScanoutHeight() * definition.y);
    view->screenRect.x2 = static_cast<int>(
        renderSystem->GetScanoutWidth() * (definition.width + definition.x) -
        1.0f);
    view->screenRect.y2 = static_cast<int>(
        renderSystem->GetScanoutHeight() * (definition.height + definition.y) -
        1.0f);
    view->usePreviousRendering = false;

    if (definition.playerNum == -1) {
      std::memset(&view->g, 0, sizeof(view->g));
      view->world = nullptr;
      view->viewIndex = 0;
    } else {
      const int playerIndex = viewIndexToPlayerIndex[definition.playerNum];
      view->world = world;
      view->viewIndex = worldViewIndex;
      view->g = gameReturn->players[playerIndex].view;
      if (definition.eyeSeparation != 0.0f) {
        const float eyeDirection =
            stereoRender_swapEyes.valueInteger != 0 ? 1.0f : -1.0f;
        const float eyeOffset =
            eyeDirection * definition.eyeSeparation *
            stereoRender_separation.valueFloat;
        view->g.vieworg.x += eyeOffset * view->g.viewaxis[1].x;
        view->g.vieworg.y += eyeOffset * view->g.viewaxis[1].y;
        view->g.vieworg.z += eyeOffset * view->g.viewaxis[1].z;
        view->g.inhibitModelFovScale = true;
        const float zNear =
            view->g.cramZNear ? r_znear.valueFloat * 0.25f
                              : r_znear.valueFloat;
        idRenderMatrix::CreateProjectionMatrixFov(
            view->g.fov_x, view->g.fov_y, zNear, r_zfar.valueFloat,
            eyeDirection * definition.eyeSeparation *
                stereoRender_screenSeparation.valueFloat,
            0.0f, view->g.explicitProjectionMatrix);
        view->g.useExplicitProjectionMatrix = true;
      }
      view->g.renderTime = gameReturn->localTime;

      idScreenView* worldView = renderFrameInfo.worldViews.Alloc();
      worldView->screenRect = view->screenRect;
      worldView->viewIndex = worldViewIndex;
      worldView->usePreviousRendering = false;
      worldView->world = world;
      worldView->g = gameReturn->players[playerIndex].view;
      worldView->g.renderTime = gameReturn->localTime;
      worldView->g.vieworg = view->g.vieworg;
      worldView->g.inhibitModelFovScale = view->g.inhibitModelFovScale;
      worldView->g.explicitProjectionMatrix = view->g.explicitProjectionMatrix;
      worldView->g.useExplicitProjectionMatrix =
          view->g.useExplicitProjectionMatrix;
      ++worldViewIndex;
      playerMask |= 1 << playerIndex;
    }
    view->viewGuis.ClearFree();
    view->guiOriginOffset =
        stereoRender_guiOffset.valueFloat * definition.eyeSeparation;
  }

  if (worldViewIndex > 1) {
    unsigned mask = static_cast<unsigned>(playerMask);
    int population = 0;
    while (mask != 0) {
      population += static_cast<int>(mask & 1u);
      mask >>= 1;
    }
    if (population > 1 || multiView_fullRender.valueInteger == 0) {
      const int renderedView = frameNumber % worldViewIndex;
      for (int index = 0; index < worldViewIndex; ++index) {
        if (index != renderedView) {
          renderFrameInfo.screenViews[index].usePreviousRendering = true;
          renderFrameInfo.worldViews[index].usePreviousRendering = true;
        }
      }
    }
  }

  for (int localView = 0; localView < playerCount; ++localView) {
    const int playerIndex = viewIndexToPlayerIndex[localView];
    idScreenView& playerView = renderFrameInfo.screenViews[localView];
    if (!renderSystem->BinkVideoIsPlaying()) {
      idRenderModelGui* gui =
          static_cast<idRenderModelGui*>(gameReturn->players[playerIndex].gui);
      if (gui != nullptr) {
        for (int index = 0; index < renderFrameInfo.screenViews.num; ++index) {
          if (currentViewDef[index].getsLocalGuis &&
              (currentViewDef[index].playerNum == localView ||
               currentViewDef[index].playerNum == -1)) {
            renderFrameInfo.screenViews[index].viewGuis.Append(gui);
          }
        }
      }
      if (com_skipGameRenderView.valueInteger == 0) {
        gui = game->RenderLocalViewForPlayer(playerIndex, gameReturn->localTime,
                                             &playerView.g, world);
        if (gui != nullptr) {
          for (int index = 0; index < renderFrameInfo.screenViews.num; ++index) {
            if (currentViewDef[index].getsLocalGuis &&
                (currentViewDef[index].playerNum == localView ||
                 currentViewDef[index].playerNum == -1)) {
              renderFrameInfo.screenViews[index].viewGuis.Append(gui);
            }
          }
        }
      }
      idRenderModelGui* inventory = static_cast<idRenderModelGui*>(
          gameReturn->players[playerIndex].inventoryModel);
      if (inventory != nullptr) {
        for (int index = 0; index < renderFrameInfo.screenViews.num; ++index) {
          if (currentViewDef[index].getsLocalGuis &&
              (currentViewDef[index].playerNum == localView ||
               currentViewDef[index].playerNum == -1)) {
            renderFrameInfo.screenViews[index].viewGuis.Append(inventory);
          }
        }
      }
      idRenderModelGui* hud = static_cast<idRenderModelGui*>(
          gameReturn->players[playerIndex].hudModel);
      if (hud != nullptr) {
        for (int index = 0; index < renderFrameInfo.screenViews.num; ++index) {
          if (currentViewDef[index].getsLocalGuis &&
              (currentViewDef[index].playerNum == localView ||
               currentViewDef[index].playerNum == -1)) {
            renderFrameInfo.screenViews[index].viewGuis.Append(hud);
          }
        }
      }
    }
  }

  AddGlobalGui(game->RenderLocalViewForGame(gameReturn->localTime, world));
  for (int index = 0; index < gameReturn->debugGuis.num; ++index) {
    AddGlobalGui(gameReturn->debugGuis[index]);
  }
  AddGlobalGui(game->Shell_Render());
  if (commonLocal.DialogGuiModel() != nullptr) {
    commonLocal.RenderDialog(false);
    AddGlobalGui(commonLocal.DialogGuiModel());
  }

  console->BeginTimeRange(RANGE_MAIN, idColor::colorBlack);
  const int height = renderSystem->GetHeight();
  const int width = renderSystem->GetWidth();
  console->Gui()->SetViewport(0, 0, width, height);
  console->Draw(false, world == nullptr);
  debugHUD->Render(console->Gui());

  idLobbyBase& lobby = session->GetActingGameStateLobbyBase();
  if (lobby.HasActivePeers()) {
    commonLocal.DrawSnapshotMetrics(net_drawDebugHud.valueInteger == 3);
  }
  if (net_drawDebugHud.valueInteger == 1) {
    idLobbyBase& debugLobby = session->GetActingGameStateLobbyBase();
    if (debugLobby.HasActivePeers()) {
      session->GetActingGameStateLobbyBase().DrawDebugNetworkHUD(console->Gui());
    }
  }
  if (console->Gui() != nullptr) {
    AddGlobalGui(console->Gui());
  }
}

idRenderManager renderManager;
