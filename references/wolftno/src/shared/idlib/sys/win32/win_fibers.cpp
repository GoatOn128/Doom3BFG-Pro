#include "shared/idlib/sys/win32/win_fibers.h"

#define WIN32_LEAN_AND_MEAN
#include <Windows.h>

#include <cstring>

idSysFiber::idSysFiber(const char* n) {
  const char* source = n ? n : "idSysFiber";
  name = new char[std::strlen(source) + 1];
  std::memcpy(name, source, std::strlen(source) + 1);
}
idSysFiber::~idSysFiber() {
  if (fiber)
    DeleteFiber(fiber);
  delete[] name;
}
bool idSysFiber::Execute() {
  if (!alive)
    return false;
  parent = IsThreadAFiber() ? GetCurrentFiber() : ConvertThreadToFiber(nullptr);
  if (!parent)
    return false;
  if (!fiber)
    fiber = CreateFiber(0, FiberRoutine, this);
  if (!fiber)
    return false;
  SwitchToFiber(fiber);
  return alive;
}
void idSysFiber::YieldFiber() {
  if (parent)
    SwitchToFiber(parent);
}
void __stdcall idSysFiber::FiberRoutine(void* data) {
  auto* self = static_cast<idSysFiber*>(data);
  self->Run();
  self->alive = false;
  self->YieldFiber();
}
