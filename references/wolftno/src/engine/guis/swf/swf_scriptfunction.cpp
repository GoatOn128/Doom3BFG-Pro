#include "engine/guis/swf/swf_scriptfunction.h"

idSWFConstantPoolAS2::idSWFConstantPoolAS2() { pool.ClearFree(); }

void idSWFConstantPoolAS2::Clear() {
  for (int i = 0; i < pool.Num(); ++i) {
    idStrRefCounted* value = pool[i];
    if (value != nullptr && value->refCount-- == 1)
      delete value;
  }
  pool.ClearFree();
}

void idSWFConstantPoolAS2::Copy(const idSWFConstantPoolAS2& other) {
  Clear();
  pool.SetNum(other.pool.Num());
  for (int i = 0; i < pool.Num(); ++i) {
    pool[i] = other.pool[i];
    ++pool[i]->refCount;
  }
}
