#include "tungsten/game/ai/ai2_actions.h"

#include <unordered_map>

namespace {

std::unordered_map<int, idThread*>& ThreadRegistry() {
  static std::unordered_map<int, idThread*> registry;
  return registry;
}

idThread*& CurrentThread() {
  static idThread* currentThread = nullptr;
  return currentThread;
}

}  // namespace

idThread::idThread(int handleValue) : handle{handleValue} {
  if (handleValue != 0) ThreadRegistry()[handleValue] = this;
}

idThread::~idThread() {
  if (CurrentThread() == this) CurrentThread() = nullptr;
  const auto found = ThreadRegistry().find(handle.value);
  if (found != ThreadRegistry().end() && found->second == this) {
    ThreadRegistry().erase(found);
  }
}

idThread* idThread::Find(int handleValue) {
  const auto found = ThreadRegistry().find(handleValue);
  return found != ThreadRegistry().end() ? found->second : nullptr;
}

idThread* idThread::GetCurrentThread() { return CurrentThread(); }

void idThread::SetCurrentThreadForTest(idThread* thread) {
  CurrentThread() = thread;
}

// DIA: idAI2::ExecuteThread
void idAI2::ExecuteThread(idThread* thread) {
  if (thread == nullptr) return;
  ResetFSMWaitThreadIfPossible(thread);
  thread->Execute();
}

// DIA: idAI2::AllThreadsAreFinished
bool idAI2::AllThreadsAreFinished() {
  for (int index = 0; index < scriptState.threads.Num(); ++index) {
    idThread* thread = idThread::Find(scriptState.threads[index].value);
    if (thread != nullptr && !thread->IsDying()) return false;
  }
  return scriptState.fibers.Num() == 0;
}

// DIA: idAI2::IsExecutingScript
bool idAI2::IsExecutingScript() {
  return (scriptState.threads.Num() != 0 || scriptState.fibers.Num() != 0) &&
         !AllThreadsAreFinished();
}

// DIA: idAI2::RemoveThread
void idAI2::RemoveThread(idThreadHandle threadHandle) {
  for (int index = 0; index < scriptState.threads.Num(); ++index) {
    if (scriptState.threads[index].value == threadHandle.value) {
      scriptState.threads[index] =
          scriptState.threads[scriptState.threads.Num() - 1];
      --scriptState.threads.num;
      return;
    }
  }
}

// DIA: idAI2::GetThread
idThreadHandle idAI2::GetThread(int index) {
  if (index < 0 || index >= scriptState.threads.Num()) return {};
  return scriptState.threads[index];
}

// DIA: idAI2::NumThreads
std::int64_t idAI2::NumThreads() {
  return scriptState.threads.Num();
}

// DIA: idAI2::ResetFSMWaitThreadIfPossible
void idAI2::ResetFSMWaitThreadIfPossible(idThread* thread) {
  if (thread != nullptr && scriptState.waitThreadHandle == thread->GetHandle()) {
    scriptState.waitThreadHandle = 0;
  }
}

// DIA: idAI2::ForceFSMIntoExecutingCodeAction
bool idAI2::ForceFSMIntoExecutingCodeAction(const idEventDef* action) {
  constexpr int kMoveToPointEnterEvent = 1;
  if (action == nullptr || action->eventnum != kMoveToPointEnterEvent ||
      scriptState.forcedCodeAction) {
    return false;
  }
  scriptState.forcedCodeAction = true;
  return true;
}

// DIA: `idAI2::ForceFSMIntoExecutingCodeAction'::`1'::dtor$0

// DIA: idAI2::AddThread
std::int64_t idAI2::AddThread(idThreadHandle threadHandle) {
  for (int index = scriptState.threads.Num() - 1; index >= 0; --index) {
    const int existingHandle = scriptState.threads[index].value;
    idThread* existingThread = idThread::Find(existingHandle);
    if (existingThread == nullptr || existingThread->IsDying()) {
      RemoveThread({existingHandle});
      if (existingHandle == threadHandle.value) return -1;
      continue;
    }
    if (existingHandle == threadHandle.value) return index;
  }
  return scriptState.threads.Append(threadHandle);
}

// DIA: idAI2::AddFiber
std::int64_t idAI2::AddFiber(idFiberHandle fiberHandle) {
  for (int index = 0; index < scriptState.fibers.Num(); ++index) {
    if (scriptState.fibers[index].value == fiberHandle.value) return index;
  }
  scriptState.fibers.Append(fiberHandle);
  return scriptState.fibers.Num();
}

void idAI2::FinishFiber(idFiberHandle fiberHandle) {
  for (int index = 0; index < scriptState.fibers.Num(); ++index) {
    if (scriptState.fibers[index].value == fiberHandle.value) {
      scriptState.fibers[index] =
          scriptState.fibers[scriptState.fibers.Num() - 1];
      --scriptState.fibers.num;
      return;
    }
  }
}
