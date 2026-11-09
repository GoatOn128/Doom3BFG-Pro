#pragma once

#include "shared/idlib/text/str.h"

#include <cstdint>

class idStackItem {
public:
  idStackItem();
  virtual ~idStackItem() = default;
  virtual const char* Data() const { return data; }
  virtual void Data(const char* text);
  virtual unsigned MaxDataLen() const { return sizeof(data); }
  char data[256]{};
};

class idStaticStack {
public:
  virtual ~idStaticStack() = default;
  bool Item(int depth, char** data);
  bool filtered{};
  bool lastItemDisplayed{};
  int depth{};
  idStackItem stack[32];
};

class idStackTrace {
public:
  explicit idStackTrace(const char* sourcePath = nullptr);
  ~idStackTrace() = default;
  std::uint32_t Trace(void* thread, int ignoreFunctions = 0, bool outputToConsole = false,
                      bool getSourceFileInfo = true);
  bool Trace(void* thread, void* exceptionPointers, int ignoreFunctions = 0,
             bool outputToConsole = false, bool getSourceFileInfo = true);
  idStr GetTruncatedFunctionDetails() const;
  idStr GetFullFunctionDetails() const;
  idStaticStack stack;
  char basePath[256]{};
  char sourcePath[256]{};
  bool delimitedOutput{};
};

class idStackTracer {
public:
  static bool Init(char* applicationSymbolPath = nullptr, const char* basePath = nullptr);
  static bool Shutdown();
  static std::uint32_t Trace(void* thread, idStackTrace& trace, int ignoreFunctions = 0,
                             bool outputToConsole = false, bool getSourceFileInfo = true);
  static bool Trace(void* process, void* thread, const void* context, idStackTrace& trace,
                    int ignoreFunctions, bool outputToConsole, bool getSourceFileInfo);
  static bool GetSymbolName(std::uint64_t address, char* name, std::uint32_t maxLength);
  static bool GetModuleName(std::uint64_t address, char* module, int maxLength);
  static bool GetSource(idStackTrace& trace, bool outputToConsole, std::uint64_t address,
                        char* fileName, unsigned maxFileLength, char* line, unsigned maxLineLength,
                        char* lineAddress, unsigned maxLineAddressLength, char* offset,
                        unsigned maxOffsetLength, char* module, unsigned maxModuleLength);
  static bool GetSymbolPath(char* path, int maxLength, char* applicationSymbolPath = nullptr);
  static idStr GetTruncatedFunctionDetails();
  static idStr GetFullFunctionDetails();
};

static_assert(sizeof(idStackItem) == 264, "stack item ABI drift");
