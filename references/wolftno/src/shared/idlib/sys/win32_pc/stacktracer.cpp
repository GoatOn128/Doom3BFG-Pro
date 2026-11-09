#include "shared/idlib/sys/win32_pc/stacktracer.h"

#define WIN32_LEAN_AND_MEAN
#include <Windows.h>
#include <DbgHelp.h>
#include <Psapi.h>

#include <array>
#include <cstdio>
#include <cstring>
#include <mutex>

namespace {
std::mutex symbolMutex;
bool symbolsInitialized = false;
idStr truncatedDetails;
idStr fullDetails;

idStr JoinStack(const idStaticStack& stack, bool full) {
  idStr result;
  for (int i = 0; i < stack.depth; ++i) {
    if (i != 0)
      result.Append(full ? "\n" : " <- ");
    result.Append(stack.stack[i].Data());
  }
  return result;
}
} // namespace

idStackItem::idStackItem() {
  data[0] = '\0';
}
void idStackItem::Data(const char* text) {
  idStr::Copynz(data, text ? text : "", sizeof(data));
}
bool idStaticStack::Item(int index, char** item) {
  if (index < 0 || index >= depth || !item)
    return false;
  *item = stack[index].data;
  return true;
}

idStackTrace::idStackTrace(const char* source) {
  idStr::Copynz(sourcePath, source ? source : "", sizeof(sourcePath));
}
std::uint32_t idStackTrace::Trace(void* thread, int ignore, bool output, bool source) {
  return idStackTracer::Trace(thread, *this, ignore, output, source);
}
bool idStackTrace::Trace(void* thread, void* exceptionPointers, int ignore, bool output,
                         bool source) {
  const CONTEXT* context = exceptionPointers
                               ? static_cast<EXCEPTION_POINTERS*>(exceptionPointers)->ContextRecord
                               : nullptr;
  return idStackTracer::Trace(GetCurrentProcess(), thread, context, *this, ignore, output, source);
}
idStr idStackTrace::GetTruncatedFunctionDetails() const {
  return JoinStack(stack, false);
}
idStr idStackTrace::GetFullFunctionDetails() const {
  return JoinStack(stack, true);
}

bool idStackTracer::Init(char* applicationSymbolPath, const char*) {
  std::lock_guard<std::mutex> lock(symbolMutex);
  if (symbolsInitialized)
    return true;
  char path[2048]{};
  GetSymbolPath(path, sizeof(path), applicationSymbolPath);
  SymSetOptions(SYMOPT_UNDNAME | SYMOPT_DEFERRED_LOADS | SYMOPT_LOAD_LINES);
  symbolsInitialized = SymInitialize(GetCurrentProcess(), path, TRUE) != FALSE;
  return symbolsInitialized;
}
bool idStackTracer::Shutdown() {
  std::lock_guard<std::mutex> lock(symbolMutex);
  if (symbolsInitialized)
    SymCleanup(GetCurrentProcess());
  symbolsInitialized = false;
  return true;
}

bool idStackTracer::GetSymbolName(std::uint64_t address, char* name, std::uint32_t maxLength) {
  if (!name || maxLength == 0)
    return false;
  Init();
  std::lock_guard<std::mutex> lock(symbolMutex);
  std::array<unsigned char, sizeof(SYMBOL_INFO) + 1024> storage{};
  auto* symbol = reinterpret_cast<SYMBOL_INFO*>(storage.data());
  symbol->SizeOfStruct = sizeof(SYMBOL_INFO);
  symbol->MaxNameLen = 1023;
  DWORD64 displacement = 0;
  if (!SymFromAddr(GetCurrentProcess(), address, &displacement, symbol)) {
    std::snprintf(name, maxLength, "0x%llx", static_cast<unsigned long long>(address));
    return false;
  }
  idStr::Copynz(name, symbol->Name, static_cast<int>(maxLength));
  return true;
}

bool idStackTracer::GetModuleName(std::uint64_t address, char* module, int maxLength) {
  if (!module || maxLength <= 0)
    return false;
  MEMORY_BASIC_INFORMATION info{};
  if (!VirtualQuery(reinterpret_cast<void*>(address), &info, sizeof(info)))
    return false;
  char path[MAX_PATH]{};
  if (!GetModuleFileNameA(static_cast<HMODULE>(info.AllocationBase), path, sizeof(path)))
    return false;
  const char* leaf = std::strrchr(path, '\\');
  idStr::Copynz(module, leaf ? leaf + 1 : path, maxLength);
  return true;
}

bool idStackTracer::GetSource(idStackTrace&, bool, std::uint64_t address, char* fileName,
                              unsigned maxFileLength, char* line, unsigned maxLineLength,
                              char* lineAddress, unsigned maxLineAddressLength, char* offset,
                              unsigned maxOffsetLength, char* module, unsigned maxModuleLength) {
  if (module && maxModuleLength)
    GetModuleName(address, module, static_cast<int>(maxModuleLength));
  Init();
  std::lock_guard<std::mutex> lock(symbolMutex);
  IMAGEHLP_LINE64 info{};
  info.SizeOfStruct = sizeof(info);
  DWORD displacement = 0;
  const bool found =
      SymGetLineFromAddr64(GetCurrentProcess(), address, &displacement, &info) != FALSE;
  if (fileName && maxFileLength)
    idStr::Copynz(fileName, found ? info.FileName : "?", static_cast<int>(maxFileLength));
  if (line && maxLineLength)
    std::snprintf(line, maxLineLength, found ? "%lu" : "", found ? info.LineNumber : 0);
  if (lineAddress && maxLineAddressLength)
    std::snprintf(lineAddress, maxLineAddressLength, "0x%llx",
                  static_cast<unsigned long long>(found ? info.Address : address));
  if (offset && maxOffsetLength)
    std::snprintf(offset, maxOffsetLength, "+0x%lx", displacement);
  return found;
}

bool idStackTracer::GetSymbolPath(char* path, int maxLength, char* applicationPath) {
  if (!path || maxLength <= 0)
    return false;
  char executable[MAX_PATH]{};
  GetModuleFileNameA(nullptr, executable, sizeof(executable));
  char* slash = std::strrchr(executable, '\\');
  if (slash)
    *slash = '\0';
  std::snprintf(path, maxLength, "%s;%s", applicationPath ? applicationPath : "", executable);
  return true;
}

std::uint32_t idStackTracer::Trace(void*, idStackTrace& trace, int ignore, bool output, bool) {
  void* frames[32]{};
  const USHORT count = CaptureStackBackTrace(static_cast<DWORD>(ignore + 1), 32, frames, nullptr);
  trace.stack.depth = count;
  for (unsigned i = 0; i < count; ++i) {
    char symbol[192]{}, module[64]{};
    GetSymbolName(reinterpret_cast<std::uint64_t>(frames[i]), symbol, sizeof(symbol));
    GetModuleName(reinterpret_cast<std::uint64_t>(frames[i]), module, sizeof(module));
    char text[256]{};
    std::snprintf(text, sizeof(text), "%s!%s", module, symbol);
    trace.stack.stack[i].Data(text);
    if (output)
      std::fprintf(stderr, "%s\n", text);
  }
  truncatedDetails = trace.GetTruncatedFunctionDetails();
  fullDetails = trace.GetFullFunctionDetails();
  return count;
}
bool idStackTracer::Trace(void*, void* thread, const void*, idStackTrace& trace, int ignore,
                          bool output, bool source) {
  return Trace(thread, trace, ignore + 1, output, source) != 0;
}
idStr idStackTracer::GetTruncatedFunctionDetails() {
  return truncatedDetails;
}
idStr idStackTracer::GetFullFunctionDetails() {
  return fullDetails;
}
