#pragma once

#include "shared/idlib/containers/blockalloc.h"
#include "shared/idlib/containers/list.h"
#include "shared/idlib/geometry/tracemodel.h"

#include <cstddef>

class idRecycledTraceModel {
public:
  idRecycledTraceModel() : trm(nullptr), index(-1) {}

  idTraceModel* GetTrm() const { return trm; }

  idTraceModel* trm;
  int index;
};

class idTraceModelRecycler {
public:
  idTraceModelRecycler() = default;
  ~idTraceModelRecycler() noexcept(false);

  void Shutdown();
  idRecycledTraceModel* GetTraceModel();
  void RecycleTraceModel(idRecycledTraceModel* recycledTrm, bool defer);
  void UpdateDeferredFreed();
  int GetNumTraceModels() const { return allocated.Num(); }

  idBlockAlloc<idRecycledTraceModel, 64, TAG_COLLISION> allocator;
  idList<idRecycledTraceModel*, TAG_COLLISION> allocated;
  idList<idRecycledTraceModel*, TAG_COLLISION> free[3];
};

static_assert(sizeof(idRecycledTraceModel) == 0x10,
              "recycled trace-model ABI drift");
static_assert(offsetof(idRecycledTraceModel, index) == 0x08,
              "recycled trace-model index offset drift");
static_assert(
    sizeof(idBlockAlloc<idRecycledTraceModel, 64, TAG_COLLISION>) == 0x20,
    "recycled trace-model allocator ABI drift");
static_assert(
    sizeof(idBlockAlloc<idRecycledTraceModel, 64,
                        TAG_COLLISION>::element_t) == 0x10,
    "recycled trace-model allocator element ABI drift");
static_assert(
    sizeof(idBlockAlloc<idRecycledTraceModel, 64,
                        TAG_COLLISION>::idBlock) == 0x418,
    "recycled trace-model allocator block ABI drift");
static_assert(sizeof(idTraceModelRecycler) == 0x80,
              "trace-model recycler ABI drift");
static_assert(offsetof(idTraceModelRecycler, allocated) == 0x20,
              "trace-model recycler allocated-list offset drift");
static_assert(offsetof(idTraceModelRecycler, free) == 0x38,
              "trace-model recycler free-list offset drift");
