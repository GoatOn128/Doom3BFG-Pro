#pragma once

#include "engine/animation/md6.h"
#include "shared/idlib/containers/blockalloc.h"

#include <cstddef>

class idMD6BlockAlloc final : public idMD6Allocator {
public:
  idMD6BlockAlloc();
  ~idMD6BlockAlloc() override;

  idMD6Node* Alloc(idMD6Node::nodeType_t type) override;
  void FreeMem(idMD6Node* node) override;
  void Condense() override;
  std::size_t Size() const override;
  int NumNodes() const override;
  idMD6Node* NodeForIndex(int index) override;
  const idMD6Node* NodeForIndex(int index) const override;
  void DeferredFree();

  idBlockAlloc<idMD6LeafPlay, 32, 18> playLeaves;
  idBlockAlloc<idMD6LeafPause, 4, 18> pauseLeaves;
  idBlockAlloc<idMD6Branch, 16, 18> branches;
  idBlockAlloc<idMD6BlendBranch, 16, 18> blendBranches;
  idBlockAlloc<idMD6BlendAdditiveBranch, 16, 18> blendAdditiveBranches;
  idBlockAlloc<idMD6FusionBranch, 16, 18> fusionBranches;
  idBlockAlloc<idMD6BestLeaf, 16, 18> bestLeaves;
  idBlockAlloc<idMD6TagFilter, 16, 18> tagFilters;
  idList<idMD6Node*, 18> nodes;
  idList<idMD6Node*, 18> nodesToDelete;
};

static_assert(sizeof(idBlockAlloc<idMD6LeafPlay, 32, 18>) == 0x20);
static_assert(sizeof(idBlockAlloc<idMD6LeafPlay, 32, 18>::element_t) == 0x30);
static_assert(sizeof(idBlockAlloc<idMD6LeafPause, 4, 18>::element_t) == 0x70);
static_assert(sizeof(idBlockAlloc<idMD6Branch, 16, 18>::element_t) == 0x40);
static_assert(sizeof(idBlockAlloc<idMD6BlendBranch, 16, 18>::element_t) == 0xf0);
static_assert(
    sizeof(idBlockAlloc<idMD6BlendAdditiveBranch, 16, 18>::element_t) ==
    0x80);
static_assert(sizeof(idBlockAlloc<idMD6FusionBranch, 16, 18>::element_t) ==
              0x100);
static_assert(sizeof(idBlockAlloc<idMD6BestLeaf, 16, 18>::element_t) == 0x90);
static_assert(sizeof(idBlockAlloc<idMD6TagFilter, 16, 18>::element_t) == 0x20);
static_assert(sizeof(idBlockAlloc<idMD6LeafPlay, 32, 18>::idBlock) == 0x618);
static_assert(sizeof(idBlockAlloc<idMD6LeafPause, 4, 18>::idBlock) == 0x1d8);
static_assert(sizeof(idBlockAlloc<idMD6Branch, 16, 18>::idBlock) == 0x418);
static_assert(sizeof(idBlockAlloc<idMD6BlendBranch, 16, 18>::idBlock) == 0xf18);
static_assert(
    sizeof(idBlockAlloc<idMD6BlendAdditiveBranch, 16, 18>::idBlock) ==
    0x818);
static_assert(sizeof(idBlockAlloc<idMD6FusionBranch, 16, 18>::idBlock) ==
              0x1018);
static_assert(sizeof(idBlockAlloc<idMD6BestLeaf, 16, 18>::idBlock) == 0x918);
static_assert(sizeof(idBlockAlloc<idMD6TagFilter, 16, 18>::idBlock) == 0x218);
static_assert(sizeof(idMD6BlockAlloc) == 0x138);
static_assert(offsetof(idMD6BlockAlloc, playLeaves) == 0x08);
static_assert(offsetof(idMD6BlockAlloc, pauseLeaves) == 0x28);
static_assert(offsetof(idMD6BlockAlloc, branches) == 0x48);
static_assert(offsetof(idMD6BlockAlloc, blendBranches) == 0x68);
static_assert(offsetof(idMD6BlockAlloc, blendAdditiveBranches) == 0x88);
static_assert(offsetof(idMD6BlockAlloc, fusionBranches) == 0xa8);
static_assert(offsetof(idMD6BlockAlloc, bestLeaves) == 0xc8);
static_assert(offsetof(idMD6BlockAlloc, tagFilters) == 0xe8);
static_assert(offsetof(idMD6BlockAlloc, nodes) == 0x108);
static_assert(offsetof(idMD6BlockAlloc, nodesToDelete) == 0x120);
