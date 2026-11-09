#include "engine/renderer/parmblockoptimizer.h"

#include <cstring>

int idParmBlockOptimizer::c_retainedOps = 0;
int idParmBlockOptimizer::c_removedOps = 0;

namespace {

bool ContainsParm(const idParmBlockOptimizer& optimizer, int parmIndex) {
  for (int index = 0; index < optimizer.numUsedParms; ++index) {
    if (optimizer.usedParms[index] == parmIndex)
      return true;
  }
  return false;
}

void AddParm(idParmBlockOptimizer& optimizer, int parmIndex) {
  if (!ContainsParm(optimizer, parmIndex))
    optimizer.usedParms[optimizer.numUsedParms++] = parmIndex;
}

}  // namespace

void idParmBlockOptimizer::OptimizeParmBlockForUsedParms(
    idParmBlock& parmBlock) {
  const int originalCount = parmBlock.ops.num;
  int removedCount = 0;

  for (int opIndex = originalCount - 1; opIndex >= 0; --opIndex) {
    const expOp_t op = parmBlock.ops.list[opIndex];
    if (!ContainsParm(*this, op.parmIndexDest)) {
      const std::size_t followingOps =
          static_cast<std::size_t>(originalCount - opIndex - 1);
      std::memmove(parmBlock.ops.list + opIndex,
                   parmBlock.ops.list + opIndex + 1,
                   followingOps * sizeof(expOp_t));
      std::memmove(parmBlock.constants.list + opIndex,
                   parmBlock.constants.list + opIndex + 1,
                   followingOps * sizeof(parmValue_t));
      ++removedCount;
      continue;
    }

    switch (op.type & 0x1F) {
      case OP_TYPE_UNMASKED_MOVE_CONSTANT:
      case OP_TYPE_MOVE_CONSTANT:
        break;
      case OP_TYPE_UNMASKED_MOVE:
      case OP_TYPE_MOVE:
      case OP_TYPE_SWIZZLE:
        AddParm(*this, op.parmIndexA);
        break;
      case OP_TYPE_TABLE:
        AddParm(*this, op.parmIndexB);
        break;
      default:
        AddParm(*this, op.parmIndexA);
        AddParm(*this, op.parmIndexB);
        break;
    }
    ++c_retainedOps;
  }

  c_removedOps += removedCount;
  const int retainedCount = originalCount - removedCount;
  parmBlock.ops.num = retainedCount < parmBlock.ops.size
                           ? retainedCount
                           : parmBlock.ops.size;
  parmBlock.constants.num = retainedCount < parmBlock.constants.size
                                 ? retainedCount
                                 : parmBlock.constants.size;
}
