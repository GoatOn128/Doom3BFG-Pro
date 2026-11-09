#include "shared/idlib/parallelism/thread.h"

idSysThread::idSysThread() : signalWorkerDone(true), signalMoreWorkToDo(false), signalMutex() {}

idSysThread::~idSysThread() {
  StopThread(true);
}

unsigned idSysThread::ThreadProc(void* context) {
  auto* thread = static_cast<idSysThread*>(context);
  Sys_SetCurrentThreadName(thread->name.data);
  thread->isRunning = true;
  if (thread->isWorker) {
    while (!thread->isTerminating) {
      thread->signalMoreWorkToDo.Wait();
      if (thread->isTerminating)
        break;
      thread->moreWorkToDo = false;
      thread->Run();
      thread->signalWorkerDone.Raise();
    }
  } else {
    thread->Run();
  }
  thread->isRunning = false;
  thread->signalWorkerDone.Raise();
  return 0;
}

bool idSysThread::StartThread(const char* threadName, core_t core, xthreadPriority priority,
                              int stackSize) {
  if (threadHandle != 0)
    return false;
  name = threadName ? threadName : "idSysThread";
  isWorker = false;
  isTerminating = false;
  moreWorkToDo = false;
  signalWorkerDone.Clear();
  threadHandle = reinterpret_cast<std::uint64_t>(
      Sys_CreateThread(&idSysThread::ThreadProc, this, priority, name.data, core, stackSize));
  return threadHandle != 0;
}

bool idSysThread::StartWorkerThread(const char* threadName, core_t core, xthreadPriority priority,
                                    int stackSize) {
  if (threadHandle != 0)
    return false;
  name = threadName ? threadName : "idSysWorkerThread";
  isWorker = true;
  isTerminating = false;
  moreWorkToDo = false;
  signalWorkerDone.Clear();
  threadHandle = reinterpret_cast<std::uint64_t>(
      Sys_CreateThread(&idSysThread::ThreadProc, this, priority, name.data, core, stackSize));
  return threadHandle != 0;
}

void idSysThread::SignalWork() {
  if (!isWorker || isTerminating)
    return;
  signalMutex.Lock();
  moreWorkToDo = true;
  signalWorkerDone.Clear();
  signalMoreWorkToDo.Raise();
  signalMutex.Unlock();
}

void idSysThread::WaitForThread() {
  if (threadHandle != 0)
    signalWorkerDone.Wait();
}

void idSysThread::StopThread(bool wait) {
  if (threadHandle == 0)
    return;
  isTerminating = true;
  signalMoreWorkToDo.Raise();
  if (wait) {
    Sys_JoinThread(reinterpret_cast<void*>(threadHandle));
    threadHandle = 0;
  }
}
