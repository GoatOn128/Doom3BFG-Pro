#pragma once

class idSysFiber {
public:
  explicit idSysFiber(const char* name);
  virtual ~idSysFiber();
  virtual void Run() = 0;
  bool Execute();
  void YieldFiber();
  static void __stdcall FiberRoutine(void* data);
  char* name{};
  bool alive{true};
  void* fiber{};
  void* parent{};
};
