#include "tungsten/game/animation/animationgui.h"

#include <algorithm>
#include <iomanip>
#include <sstream>

// DIA: idAnimationGUI::idAnimationGUI
idAnimationGUI::idAnimationGUI() : font("arial_black") {}

// DIA: GetAnimTime
float GetAnimTime(const idMD6LeafPlay* leaf, int timeMSec, int ticksPerSec) {
  if (leaf == nullptr || leaf->anim == nullptr ||
      leaf->anim->frameRate <= 0 || leaf->anim->numFrames <= 0) return 0.0F;

  float frame = leaf->floatFrame;
  if (leaf->type == md6NodeType::play && ticksPerSec > 0)
    frame += static_cast<float>(timeMSec) * leaf->anim->frameRate /
             static_cast<float>(ticksPerSec);

  const int numFrames = leaf->anim->numFrames;
  if (leaf->wrapMode == md6WrapMode::loop && numFrames > 1) {
    const float loopFrames = static_cast<float>(numFrames - 1);
    frame -= loopFrames * static_cast<float>(static_cast<int>(frame / loopFrames));
    if (frame < 0.0F) frame += loopFrames;
  } else {
    frame = std::clamp(frame, 0.0F, static_cast<float>(numFrames - 1));
  }
  return frame / static_cast<float>(leaf->anim->frameRate);
}

// DIA: idAnimationGUI::~idAnimationGUI
idAnimationGUI::~idAnimationGUI() {
  delete guiModel;
  guiModel = nullptr;
  font.clear();
}

// DIA: idAnimationGUI::DrawAnimTree_r
void idAnimationGUI::DrawAnimTree_r(const std::string& tabLevel,
                                    idMD6LeafPlay* treeNode) {
  if (guiModel == nullptr || treeNode == nullptr) return;

  std::ostringstream line;
  line << tabLevel << std::fixed << std::setprecision(3);
  if (treeNode->type == md6NodeType::pause ||
      treeNode->type == md6NodeType::play) {
    const int gameTime = gameLocal != nullptr ? gameLocal->GetGameTimeMs() : 0;
    const int ticks = gameLocal != nullptr ? gameLocal->GetGameMsPerRealSec() : 1000;
    line << (treeNode->type == md6NodeType::pause ? "LeafPause: (" : "LeafPlay: (")
         << GetAnimTime(treeNode, gameTime, ticks) << ") "
         << (treeNode->anim != nullptr ? treeNode->anim->name : std::string{});
    guiModel->AddLine(line.str());
    return;
  }

  if (treeNode->type == md6NodeType::fusion)
    line << "FusionBranch - Val: " << treeNode->fusionValue;
  else
    line << "Branch";
  line << " - blend: " << treeNode->blendWeight
       << " sync: " << (treeNode->syncEnabled ? "on" : "off");
  guiModel->AddLine(line.str());
  DrawAnimTree_r(tabLevel + "\t", treeNode->firstChild);
  DrawAnimTree_r(tabLevel + "\t", treeNode->secondChild);
}

// DIA: `idAnimationGUI::DrawAnimTree_r'::`1'::dtor$0
// DIA: `idAnimationGUI::DrawAnimTree_r'::`1'::dtor$1

// DIA: idAnimationGUI::DrawDebugGUI
idRenderModelGui* idAnimationGUI::DrawDebugGUI(idEntity* entity) {
  if (animationGui_show.GetInteger() == 0 || entity == nullptr || font.empty())
    return nullptr;
  if (animationGui_font.valueString != font) font = animationGui_font.valueString;
  if (guiModel == nullptr) guiModel = new idRenderModelGui;
  guiModel->Clear();
  guiModel->SetViewport(640, 480);
  guiModel->AddLine("Name: " + std::string(entity->GetName()));
  guiModel->AddLine("Entity#: " + std::to_string(entity->GetEntityNumber()));
  if ((animationGui_show.GetInteger() & 1) != 0) DrawAnimTree_r("", tree);
  return guiModel;
}

// DIA: `idAnimationGUI::DrawDebugGUI'::`1'::dtor$0
// DIA: `idAnimationGUI::DrawDebugGUI'::`1'::dtor$1
// DIA: `idAnimationGUI::DrawDebugGUI'::`1'::dtor$2
// DIA: `idAnimationGUI::DrawDebugGUI'::`1'::dtor$3
// DIA: `idAnimationGUI::DrawDebugGUI'::`1'::dtor$4
// DIA: `idAnimationGUI::DrawDebugGUI'::`1'::dtor$5

// DIA: `dynamic initializer for 'animationGui_show''
// DIA: `dynamic atexit destructor for 'animationGui_show''
idCVar animationGui_show("animationGui_show", "0", 2,
                         "animation debug GUI selection mask");
// DIA: `dynamic initializer for 'animationGui_scale''
// DIA: `dynamic atexit destructor for 'animationGui_scale''
idCVar animationGui_scale("animationGui_scale", "0.2", 4,
                          "animation GUI text scale");
// DIA: `dynamic initializer for 'animationGui_font''
// DIA: `dynamic atexit destructor for 'animationGui_font''
idCVar animationGui_font("animationGui_font", "arial_black", 0,
                         "animation GUI font");
// DIA: `dynamic initializer for 'animationGui_alpha''
// DIA: `dynamic atexit destructor for 'animationGui_alpha''
idCVar animationGui_alpha("animationGui_alpha", "0.6", 4,
                          "animation GUI background alpha");
// DIA: `dynamic initializer for 'animationGui_textAlpha''
// DIA: `dynamic atexit destructor for 'animationGui_textAlpha''
idCVar animationGui_textAlpha("animationGui_textAlpha", "1.75", 4,
                              "animation GUI text alpha");
