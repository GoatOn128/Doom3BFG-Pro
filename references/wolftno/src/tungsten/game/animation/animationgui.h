#pragma once

#include "tungsten/game/game_runtime.h"

#include <string>
#include <vector>

enum class md6NodeType : unsigned char {
  branch = 0,
  pause = 1,
  play = 2,
  blend = 3,
  additive = 4,
  fusion = 5
};

enum class md6WrapMode : unsigned char { clamp = 0, loop = 1 };

struct idMD6Anim {
  std::string name;
  int numFrames{0};
  int frameRate{30};
};

struct idMD6LeafPlay {
  md6NodeType type{md6NodeType::play};
  md6WrapMode wrapMode{md6WrapMode::clamp};
  const idMD6Anim* anim{nullptr};
  float floatFrame{0.0F};
  bool syncEnabled{false};
  float blendWeight{1.0F};
  float fusionValue{0.0F};
  idMD6LeafPlay* firstChild{nullptr};
  idMD6LeafPlay* secondChild{nullptr};
};

class idRenderModelGui {
public:
  void Clear() { lines.clear(); }
  void AddLine(std::string value) { lines.push_back(std::move(value)); }
  int NumLines() const { return static_cast<int>(lines.size()); }
  const std::string& GetLine(int index) const { return lines.at(index); }
  void SetViewport(int newWidth, int newHeight) {
    width = newWidth;
    height = newHeight;
  }
  int GetWidth() const { return width; }
  int GetHeight() const { return height; }

private:
  std::vector<std::string> lines;
  int width{0};
  int height{0};
};

float GetAnimTime(const idMD6LeafPlay* leaf, int timeMSec, int ticksPerSec);

class idAnimationGUI {
public:
  idAnimationGUI();
  ~idAnimationGUI();

  void DrawAnimTree_r(const std::string& tabLevel, idMD6LeafPlay* treeNode);
  idRenderModelGui* DrawDebugGUI(idEntity* entity);
  void SetTree(idMD6LeafPlay* value) { tree = value; }
  const std::string& GetFont() const { return font; }

private:
  idRenderModelGui* guiModel{nullptr};
  std::string font;
  idMD6LeafPlay* tree{nullptr};
};

extern idCVar animationGui_show;
extern idCVar animationGui_scale;
extern idCVar animationGui_font;
extern idCVar animationGui_alpha;
extern idCVar animationGui_textAlpha;
