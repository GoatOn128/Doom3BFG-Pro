#pragma once

#include <cstddef>

class idPresentable;

// Header-emitted idEntity slice present in SWFHelper.obj.  Only the recovered
// presentable field is named; the rest remains owned by the game recovery.
class idEntity {
public:
  idPresentable* GetPresentable();
  void InitPresentableInternal();

  std::byte opaqueToPresentable[0x210];
  idPresentable* presentable;
  std::byte opaqueTail[0x378-0x218];
};

static_assert(offsetof(idEntity,presentable)==0x210,
              "entity presentable offset drift");
static_assert(sizeof(idEntity)==0x378,"entity ABI drift");
