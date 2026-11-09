#include "engine/models/skeletalanimation/md6parsetree.h"

#include <algorithm>

namespace {

idMD6ParseTreeModelCompatible g_modelCompatible = nullptr;
idMD6ParseTreeBranchUpdate g_branchUpdate = nullptr;

}

void SetMD6ParseTreeModelCompatible(idMD6ParseTreeModelCompatible compatible) {
  g_modelCompatible = compatible;
}

void SetMD6ParseTreeBranchUpdate(idMD6ParseTreeBranchUpdate update) {
  g_branchUpdate = update;
}

bool DispatchMD6ParseTreeBranchUpdate(idMD6Branch* branch, bool forceUpdate,
                                      int timeMSec, int ticksPerSec,
                                      const idMD6Skel* skeleton) {
  if (g_branchUpdate == nullptr)
    return false;
  g_branchUpdate(branch, forceUpdate, timeMSec, ticksPerSec, skeleton);
  return true;
}

void idBounds::AddBounds(const idBounds& bounds) {
  for (int axis = 0; axis < 3; ++axis) {
    b[0][axis] = std::min(b[0][axis], bounds[0][axis]);
    b[1][axis] = std::max(b[1][axis], bounds[1][axis]);
  }
}
