#pragma once

#include "shared/idlib/containers/array.h"
#include "shared/idlib/text/str.h"

class idCmdArgs;
struct idCommandLink;

class idConsoleHistory {
public:
  static constexpr int COMMAND_HISTORY = 64;

  void LoadHistoryFile();
  void AddToHistory(const char* line, bool writeHistoryFile);
  idStr RetrieveFromHistory(bool backward);
  idStr LineForIndex(int index);
  int GetLineCount();
  void PrintHistory();
  void ClearHistory() {
    upPoint = 0;
    downPoint = 0;
    returnLine = 0;
    numHistory = 0;
  }

  int upPoint = 0;
  int downPoint = 0;
  int returnLine = 0;
  int numHistory = 0;
  idArray<idStr, COMMAND_HISTORY> historyLines;
};

void clearHistory_f(const idCmdArgs& args);
void history_f(const idCmdArgs& args);

extern const char* const HISTORY_FILE_NAME;
extern idConsoleHistory consoleHistory;
extern idCommandLink history_v;
extern idCommandLink clearHistory_v;

static_assert(sizeof(idConsoleHistory) == 0xA10,
              "console-history ABI drift");
