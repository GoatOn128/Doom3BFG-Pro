#define WOLFTNO_RETAIL_IDMEM_SINGLETON_ABI
#include "engine/gamelib/physics/tracemodelrecycler.h"
#undef WOLFTNO_RETAIL_IDMEM_SINGLETON_ABI

#include <cstdint>
#include <cstring>

template void
idBlockAlloc<idRecycledTraceModel, 64, TAG_COLLISION>::AllocNewBlock();

void idTraceModelRecycler::Shutdown() {
  for (int index = 0; index < allocated.num; ++index) {
    idTraceModel::operator delete(allocated.list[index]->trm);
    allocated.list[index]->trm = nullptr;
  }
  allocated.ClearFree();

  for (int index = 0; index < 3; ++index) {
    free[index].ClearFree();
  }
  allocator.Shutdown();
}

idTraceModelRecycler::~idTraceModelRecycler() noexcept(false) { Shutdown(); }

idRecycledTraceModel* idTraceModelRecycler::GetTraceModel() {
  if (free[0].num != 0) {
    const int last = free[0].num - 1;
    idRecycledTraceModel* const recycled = free[0].list[last];
    free[0].RemoveIndexFast(last);
    return recycled;
  }

  idRecycledTraceModel* const recycled = allocator.Alloc();
  auto* const traceBytes = static_cast<std::uint8_t*>(
      idTraceModel::operator new(sizeof(idTraceModel)));
  idTraceModel* traceModel = nullptr;
  if (traceBytes != nullptr) {
    std::memset(traceBytes + 0x5C0, 0, 0x40);
    traceModel = reinterpret_cast<idTraceModel*>(traceBytes);
  }
  recycled->trm = traceModel;
  recycled->index = allocated.Append(recycled);
  return recycled;
}

void idTraceModelRecycler::RecycleTraceModel(
    idRecycledTraceModel* recycledTrm, bool defer) {
  if (defer) {
    free[2].Append(recycledTrm);
  } else {
    free[0].Append(recycledTrm);
  }
}

void idTraceModelRecycler::UpdateDeferredFreed() {
  for (int queue = 1; queue < 3; ++queue) {
    for (int index = 0; index < free[queue].num; ++index) {
      free[queue - 1].Append(free[queue].list[index]);
    }
    free[queue].ClearFree();
  }
}
