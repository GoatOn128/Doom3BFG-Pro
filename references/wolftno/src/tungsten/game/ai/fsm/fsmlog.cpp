#include "tungsten/game/ai/fsm/fsmlog.h"

// DIA: idFSMLog::FromLast
const idFSMLogEntry* idFSMLog::FromLast(int index) const {
  if (index < 0 || index >= list.Num()) return nullptr;
  int slot;
  if (list.Num() >= maxSize) {
    slot = first - index - 1;
    while (slot < 0) slot += maxSize;
  } else {
    slot = list.Num() - index - 1;
  }
  return slot >= 0 && slot < list.Num() ? &list[slot] : nullptr;
}

// DIA: idFSMLog::idFSMLog
idFSMLog::idFSMLog(int requestedMaxSize)
    : maxSize(requestedMaxSize > 0 ? requestedMaxSize : 1) {
  list.ResizeForPort(maxSize);
}

// DIA: idFSMLog::~idFSMLog
idFSMLog::~idFSMLog() {
  list.ClearFree();
  first = 0;
}

// DIA: idList<idFSMLogEntry,5>::Resize
template bool idList<idFSMLogEntry, 5>::Resize(int);

// DIA: idList<idFSMLogEntry,5>::Alloc
template idFSMLogEntry* idList<idFSMLogEntry, 5>::Alloc();

// DIA: idFSMLog::Alloc
idFSMLogEntry* idFSMLog::Alloc() {
  if (list.Num() < maxSize) return list.Alloc();
  idFSMLogEntry* result = &list[first];
  first = (first + 1) % maxSize;
  *result = {};
  return result;
}
