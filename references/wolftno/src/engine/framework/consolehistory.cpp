#include "engine/framework/consolehistory.h"

#include "shared/idlib/commandlink.h"
#include "shared/idlib/filesystem/file.h"
#include "shared/idlib/filesystem/filesystemlocal.h"
#include "shared/idlib/lib.h"
#include "shared/idlib/sys/sys_alloc.h"
#include "shared/idlib/text/lexer.h"

const char* const HISTORY_FILE_NAME = "consoleHistory.txt";
idConsoleHistory consoleHistory;

namespace {

bool HasVisibleText(const char* line) {
  if (line == nullptr)
    return false;
  const char* cursor = line;
  while (*cursor != '\0' && static_cast<signed char>(*cursor) <= 32)
    ++cursor;
  return *cursor != '\0';
}

void ClearHistoryCommand(const idCmdArgs* args) {
  clearHistory_f(*args);
}

void HistoryCommand(const idCmdArgs* args) {
  history_f(*args);
}

} // namespace

idCommandLink history_v(
    "history", HistoryCommand, "Displays the console command history", nullptr);
idCommandLink clearHistory_v(
    "clearHistory", ClearHistoryCommand, "Clears the console history", nullptr);

int idConsoleHistory::GetLineCount() { return numHistory; }

void clearHistory_f(const idCmdArgs&) {
  consoleHistory.upPoint = 0;
  consoleHistory.downPoint = 0;
  consoleHistory.returnLine = 0;
  consoleHistory.numHistory = 0;
}

void idConsoleHistory::AddToHistory(const char* line, bool writeHistoryFile) {
  if (!HasVisibleText(line))
    return;

  if (idStr::Icmp(historyLines.ptr[(numHistory - 1) & 63].data, line) == 0) {
    if (idStr::Icmp(historyLines.ptr[returnLine & 63].data, line) == 0)
      upPoint = returnLine;
    return;
  }

  upPoint = numHistory;
  returnLine = numHistory;
  downPoint = numHistory + 1;
  idMem::Singleton()->PushHeap(HEAP_SYSTEMHEAP);
  historyLines.ptr[numHistory & 63] = line;
  idMem::Singleton()->PopHeap();
  ++numHistory;

  if (!writeHistoryFile)
    return;

  idFile* file = fileSystem->OpenFileWrite(HISTORY_FILE_NAME, FSPATH_BASE);
  if (file == nullptr)
    return;
  for (int index = numHistory - COMMAND_HISTORY; index < numHistory; ++index) {
    if (index >= 0)
      file->Printf("%s\n", historyLines.ptr[index & 63].data);
  }
  delete file;
}

idStr idConsoleHistory::RetrieveFromHistory(bool backward) {
  if (numHistory == 0)
    return idStr("");

  int lineIndex;
  if (backward) {
    lineIndex = upPoint;
    if (upPoint < numHistory - COMMAND_HISTORY || lineIndex < 0)
      return idStr("");
    downPoint = lineIndex + 1;
    upPoint = lineIndex - 1;
  } else {
    lineIndex = downPoint;
    if (lineIndex >= numHistory)
      return idStr("");
    upPoint = lineIndex - 1;
    downPoint = lineIndex + 1;
  }
  returnLine = lineIndex;
  return historyLines.ptr[lineIndex & 63];
}

idStr idConsoleHistory::LineForIndex(int index) {
  const std::int32_t nextIndex = static_cast<std::int32_t>(
      static_cast<std::uint32_t>(index) + 1u);
  if (numHistory == 0 || nextIndex > numHistory ||
      static_cast<unsigned>(index) > 63u)
    return idStr("");
  return historyLines.ptr[index];
}

void idConsoleHistory::LoadHistoryFile() {
  idLexer lexer(0);
  if (!lexer.LoadFile(HISTORY_FILE_NAME, false))
    return;

  for (;;) {
    idStr line;
    lexer.ParseCompleteLine(line);
    if (line.IsEmpty())
      break;
    line.StripTrailingWhitespace();
    AddToHistory(line.data, false);
  }
}

void idConsoleHistory::PrintHistory() {
  for (int index = numHistory - COMMAND_HISTORY; index < numHistory; ++index) {
    if (index < 0)
      continue;
    const int upMarker = index == upPoint ? 'U' : ' ';
    const int downMarker = index == downPoint ? 'D' : ' ';
    const int returnMarker = index == returnLine ? 'R' : ' ';
    idLib::Printf("%c%c%c%4i: %s\n", upMarker, downMarker, returnMarker,
                  index, historyLines.ptr[index & 63].data);
  }
}

void history_f(const idCmdArgs&) { consoleHistory.PrintHistory(); }
