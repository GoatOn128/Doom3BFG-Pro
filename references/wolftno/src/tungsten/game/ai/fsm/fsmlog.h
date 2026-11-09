#pragma once

#include "shared/idlib/containers/list.h"

class idFiniteStateMachine;
class idTypeInfo;

struct idFSMLogEntry {
  enum fsmLogEntry_t : int {
    LOGENTRY_TRANSITION = 0,
    LOGENTRY_RESTART = 1,
    LOGENTRY_ERROR = 2,
  };

  fsmLogEntry_t type{LOGENTRY_TRANSITION};
  int time{0};
  const idFiniteStateMachine* fsm{nullptr};
  const idTypeInfo* curStateType{nullptr};
  const idTypeInfo* nextStateType{nullptr};
  const idTypeInfo* transitionType{nullptr};
  int transCode{0};
};
static_assert(sizeof(idFSMLogEntry) == 48, "idFSMLogEntry ABI drift");

class idFSMLog {
public:
  explicit idFSMLog(int maxSize);
  ~idFSMLog();
  const idFSMLogEntry* FromLast(int index) const;
  idFSMLogEntry* Alloc();
  int Num() const { return list.Num(); }
  int MaxSize() const { return maxSize; }
private:
  idList<idFSMLogEntry, 5> list;
  int first{0};
  int maxSize{0};
};
