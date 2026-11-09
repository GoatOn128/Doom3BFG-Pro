#include "shared/idlib/text/str.h"

#include <list>

class idDiaStackWalker {
public:
  ~idDiaStackWalker() {
    addresses.clear();
    thisAddresses.clear();
  }
  void* diaStackWalker{};
  void* enumFrames{};
  void* diaStackWalkHelper{};
  void* contextManager{};
  void* moduleManager{};
  idStr buffer;
  unsigned symbolPointerLevel{};
  void* address{};
  void* thisAddress{};
  std::list<void*> addresses;
  std::list<void*> thisAddresses;
};
