#include "lib.h"
#include "shared/idlib/filesystem/filesystemlocal.h"

#include <Windows.h>

#include <cstdio>
#include <cstdlib>

std::uint32_t idLib::mainThreadID = 0;
int idLib::production = 0;
int idLib::frameNumber = 0;
bool idLib::headless = false;
bool idLib::dontTrack = false;
idPrintListener* idLib::printListener = nullptr;
idSourceControl* idLib::sourceControl = nullptr;

bool idLib::IsMainThread() {
  return mainThreadID == 0 || mainThreadID == ::GetCurrentThreadId();
}

int idLib::AvoidLinkErrorGetProduction() {
  return production;
}

void idLib::Init() {
  const std::uint32_t current = ::GetCurrentThreadId();
  if (mainThreadID != 0 && mainThreadID != current) {
    FatalError("idLib::Init called from different thread!");
  }
  mainThreadID = current;
}

void idLib::ShutDown() {
  if (fileSystem != nullptr)
    fileSystem->ClearZipCache();
}
