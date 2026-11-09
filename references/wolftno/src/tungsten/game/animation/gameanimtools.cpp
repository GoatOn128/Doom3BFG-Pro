#include "tungsten/game/animation/gameanimtools.h"

#include <algorithm>

// DIA: idGameAnimTools::~idGameAnimTools
idGameAnimTools::~idGameAnimTools() = default;
// DIA: idGameAnimTools::`scalar deleting destructor'

// DIA: idGameAnimToolsLocal::DrawBlendTreeBranch
void idGameAnimToolsLocal::DrawBlendTreeBranch(float x, float y,
                                               const idGameAnimNode* branch) {
  if (branch == nullptr) return;
  guiModel.Add("branch " + branch->name + " alpha=" +
               std::to_string(branch->alpha) + " @" + std::to_string(x) +
               "," + std::to_string(y));
}

// DIA: idGameAnimToolsLocal::GetAnimNumFrames
int idGameAnimToolsLocal::GetAnimNumFrames(const idGameAnimTree*,
                                           const idGameAnimData* animation) const {
  return animation != nullptr ? animation->numFrames : 0;
}

// DIA: idGameAnimToolsLocal::~idGameAnimToolsLocal
idGameAnimToolsLocal::~idGameAnimToolsLocal() = default;
// DIA: `idGameAnimToolsLocal::~idGameAnimToolsLocal'::`1'::dtor$0
// DIA: idGameAnimToolsLocal::`scalar deleting destructor'

// DIA: idGameAnimToolsLocal::WalkTree_r
void idGameAnimToolsLocal::WalkTree_r(const idGameAnimNode* node) {
  if (node == nullptr) return;
  if (node->type == idGameAnimNode::Type::branch) {
    WalkTree_r(node->left);
    WalkTree_r(node->right);
    guiModel.Add("BRANCH: " + node->name);
  } else {
    guiModel.Add((node->type == idGameAnimNode::Type::play ? "PLAY: " : "PAUSE: ") +
                 node->name + " frame=" + std::to_string(node->frame));
  }
}

// DIA: idGameAnimToolsLocal::AnimDebugHud
void idGameAnimToolsLocal::AnimDebugHud(const idGameAnimTree*,
                                        const idGameAnimNode* tree) {
  guiModel.Clear();
  guiModel.Add("MD6 Debug HUD");
  WalkTree_r(tree);
}

// DIA: idGameAnimToolsLocal::DrawBlendTreeLeaf
void idGameAnimToolsLocal::DrawBlendTreeLeaf(float x, float y,
                                             const idGameAnimNode* leaf) {
  if (leaf == nullptr) return;
  guiModel.Add("leaf " + leaf->name + " frame=" + std::to_string(leaf->frame) +
               " @" + std::to_string(x) + "," + std::to_string(y));
}
// DIA: `idGameAnimToolsLocal::DrawBlendTreeLeaf'::`1'::dtor$0
// DIA: `idGameAnimToolsLocal::DrawBlendTreeLeaf'::`1'::dtor$1

// DIA: idGameAnimToolsLocal::DrawTree_r
void idGameAnimToolsLocal::DrawTree_r(float x, float y,
                                      const idGameAnimNode* node, int depth) {
  if (node == nullptr) return;
  if (node->type != idGameAnimNode::Type::branch) {
    DrawBlendTreeLeaf(x, y, node);
    return;
  }
  DrawBlendTreeBranch(x, y, node);
  const float spacing = std::min(nodeWidth, blendTree_maxNodeSpacing.GetFloat());
  const float spread = static_cast<float>((1 << std::max(0, depth)) - 1) * spacing * 0.5F;
  DrawTree_r(x - spread, y + nodeHeight + 32.0F, node->left, depth - 1);
  DrawTree_r(x + spread, y + nodeHeight + 32.0F, node->right, depth - 1);
}

// DIA: idGameAnimToolsLocal::TreeMaxNodeWidth_r
int idGameAnimToolsLocal::TreeMaxNodeWidth_r(const idGameAnimNode* node) const {
  if (node == nullptr) return 0;
  const int selfWidth = static_cast<int>(node->name.size() * 7 + 8);
  return std::max({selfWidth, TreeMaxNodeWidth_r(node->left),
                   TreeMaxNodeWidth_r(node->right)});
}

// DIA: idGameAnimToolsLocal::TreeDepth_r
int idGameAnimToolsLocal::TreeDepth_r(const idGameAnimNode* node) const {
  if (node == nullptr || node->type != idGameAnimNode::Type::branch) return 0;
  return std::max(TreeDepth_r(node->left), TreeDepth_r(node->right)) + 1;
}

// DIA: idGameAnimToolsLocal::TreeDepth
int idGameAnimToolsLocal::TreeDepth(const idGameAnimNode* root) const {
  return root != nullptr ? TreeDepth_r(root) : -1;
}

// DIA: idGameAnimToolsLocal::GetAnimData
const idGameAnimData* idGameAnimToolsLocal::GetAnimData(
    const idGameAnimTree* tree, const char* aliasName) const {
  if (tree == nullptr || aliasName == nullptr) return nullptr;
  const auto found = tree->aliases.find(aliasName);
  return found != tree->aliases.end() ? &found->second : nullptr;
}

// DIA: idGameAnimToolsLocal::GetAnimNumFrames
int idGameAnimToolsLocal::GetAnimNumFrames(const idGameAnimTree* tree,
                                           const char* aliasName) const {
  const idGameAnimData* data = GetAnimData(tree, aliasName);
  return data != nullptr ? data->numFrames : 0;
}

// DIA: idGameAnimToolsLocal::GetAnimFrameRate
int idGameAnimToolsLocal::GetAnimFrameRate(const idGameAnimTree* tree,
                                           const char* aliasName) const {
  const idGameAnimData* data = GetAnimData(tree, aliasName);
  return data != nullptr ? data->frameRate : 0;
}

// DIA: idGameAnimToolsLocal::VerifyStrongAliasRefs
bool idGameAnimToolsLocal::VerifyStrongAliasRefs(const idGameAnimTree* tree,
                                                 const char* aliasName,
                                                 const char* entityName) const {
  (void)entityName;
  const idGameAnimData* data = GetAnimData(tree, aliasName);
  if (data == nullptr) return false;
  return std::all_of(data->strongReferences.begin(), data->strongReferences.end(),
                     [](const std::string& reference) { return !reference.empty(); });
}

// DIA: idGameAnimToolsLocal::DrawBlendTree
idGameAnimGui* idGameAnimToolsLocal::DrawBlendTree(const idGameAnimTree*,
                                                   const idGameAnimNode* root) {
  guiModel.Clear();
  if (root == nullptr) return nullptr;
  nodeWidth = std::max(blendTree_panZoomNodeWidth.GetFloat(),
                       static_cast<float>(TreeMaxNodeWidth_r(root)));
  DrawTree_r(blendTree_x.GetFloat(), blendTree_y.GetFloat(), root,
             TreeDepth(root));
  return &guiModel;
}
// DIA: `idGameAnimToolsLocal::DrawBlendTree'::`1'::dtor$0
// DIA: `idGameAnimToolsLocal::DrawBlendTree'::`1'::dtor$1

#define GAME_ANIM_CVAR(name, value, description) idCVar name(#name, value, 4, description)
// DIA: `dynamic initializer for 'blendTree_x''
// DIA: `dynamic atexit destructor for 'blendTree_x''
GAME_ANIM_CVAR(blendTree_x, "320", "blend-tree horizontal origin");
// DIA: `dynamic initializer for 'blendTree_y''
// DIA: `dynamic atexit destructor for 'blendTree_y''
GAME_ANIM_CVAR(blendTree_y, "32", "blend-tree vertical origin");
// DIA: `dynamic initializer for 'blendTree_scale''
// DIA: `dynamic atexit destructor for 'blendTree_scale''
GAME_ANIM_CVAR(blendTree_scale, "1", "blend-tree scale");
// DIA: `dynamic initializer for 'blendTree_panZoomEnable''
// DIA: `dynamic atexit destructor for 'blendTree_panZoomEnable''
GAME_ANIM_CVAR(blendTree_panZoomEnable, "1", "enable pan and zoom");
// DIA: `dynamic initializer for 'blendTree_panZoomNodeWidth''
// DIA: `dynamic atexit destructor for 'blendTree_panZoomNodeWidth''
GAME_ANIM_CVAR(blendTree_panZoomNodeWidth, "120", "minimum node width");
// DIA: `dynamic initializer for 'blendTree_panZoomXOffset''
// DIA: `dynamic atexit destructor for 'blendTree_panZoomXOffset''
GAME_ANIM_CVAR(blendTree_panZoomXOffset, "0", "pan horizontal offset");
// DIA: `dynamic initializer for 'blendTree_panZoomYOffset''
// DIA: `dynamic atexit destructor for 'blendTree_panZoomYOffset''
GAME_ANIM_CVAR(blendTree_panZoomYOffset, "0", "pan vertical offset");
// DIA: `dynamic initializer for 'blendTree_panZoomScale''
// DIA: `dynamic atexit destructor for 'blendTree_panZoomScale''
GAME_ANIM_CVAR(blendTree_panZoomScale, "1", "pan zoom scale");
// DIA: `dynamic initializer for 'blendTree_panZoomFixedDepth''
// DIA: `dynamic atexit destructor for 'blendTree_panZoomFixedDepth''
GAME_ANIM_CVAR(blendTree_panZoomFixedDepth, "-1", "fixed display depth");
// DIA: `dynamic initializer for 'blendTree_maxNodeSpacing''
// DIA: `dynamic atexit destructor for 'blendTree_maxNodeSpacing''
GAME_ANIM_CVAR(blendTree_maxNodeSpacing, "200", "maximum node spacing");
#undef GAME_ANIM_CVAR

// DIA: `dynamic initializer for 'gameAnimToolsLocal''
// DIA: `dynamic atexit destructor for 'gameAnimToolsLocal''
idGameAnimToolsLocal gameAnimToolsLocal;
// DIA: ``dynamic atexit destructor for 'gameAnimToolsLocal'''::`1'::dtor$0
