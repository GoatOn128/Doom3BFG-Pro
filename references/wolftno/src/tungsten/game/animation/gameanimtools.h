#pragma once

#include "tungsten/game/game_runtime.h"

#include <string>
#include <unordered_map>
#include <vector>

struct idGameAnimData {
  int numFrames{0};
  int frameRate{0};
  std::vector<std::string> strongReferences;
};

struct idGameAnimNode {
  enum class Type { branch, pause, play } type{Type::branch};
  std::string name;
  float alpha{1.0F};
  int frame{0};
  idGameAnimNode* left{nullptr};
  idGameAnimNode* right{nullptr};
  const idGameAnimData* animData{nullptr};
};

struct idGameAnimTree {
  std::unordered_map<std::string, idGameAnimData> aliases;
};

class idGameAnimGui {
public:
  void Clear() { lines.clear(); }
  void Add(std::string line) { lines.push_back(std::move(line)); }
  int NumLines() const { return static_cast<int>(lines.size()); }
  const std::string& GetLine(int index) const { return lines.at(index); }
private:
  std::vector<std::string> lines;
};

class idGameAnimTools {
public:
  virtual ~idGameAnimTools();
};

class idGameAnimToolsLocal final : public idGameAnimTools {
public:
  idGameAnimToolsLocal() = default;
  ~idGameAnimToolsLocal() override;

  void DrawBlendTreeBranch(float x, float y, const idGameAnimNode* branch);
  int GetAnimNumFrames(const idGameAnimTree* tree,
                       const idGameAnimData* animation) const;
  void WalkTree_r(const idGameAnimNode* node);
  void AnimDebugHud(const idGameAnimTree* animator,
                    const idGameAnimNode* tree);
  void DrawBlendTreeLeaf(float x, float y, const idGameAnimNode* leaf);
  void DrawTree_r(float x, float y, const idGameAnimNode* node, int depth);
  int TreeMaxNodeWidth_r(const idGameAnimNode* node) const;
  int TreeDepth_r(const idGameAnimNode* node) const;
  int TreeDepth(const idGameAnimNode* root) const;
  const idGameAnimData* GetAnimData(const idGameAnimTree* tree,
                                    const char* aliasName) const;
  int GetAnimNumFrames(const idGameAnimTree* tree, const char* aliasName) const;
  int GetAnimFrameRate(const idGameAnimTree* tree, const char* aliasName) const;
  bool VerifyStrongAliasRefs(const idGameAnimTree* tree,
                             const char* aliasName,
                             const char* entityName) const;
  idGameAnimGui* DrawBlendTree(const idGameAnimTree* animator,
                               const idGameAnimNode* root);

  const idGameAnimGui& GetGui() const { return guiModel; }
  float GetNodeWidth() const { return nodeWidth; }

private:
  idGameAnimGui guiModel;
  float nodeWidth{120.0F};
  float nodeHeight{64.0F};
};

extern idCVar blendTree_x;
extern idCVar blendTree_y;
extern idCVar blendTree_scale;
extern idCVar blendTree_panZoomEnable;
extern idCVar blendTree_panZoomNodeWidth;
extern idCVar blendTree_panZoomXOffset;
extern idCVar blendTree_panZoomYOffset;
extern idCVar blendTree_panZoomScale;
extern idCVar blendTree_panZoomFixedDepth;
extern idCVar blendTree_maxNodeSpacing;
extern idGameAnimToolsLocal gameAnimToolsLocal;
