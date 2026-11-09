#include "engine/framework/consolewatch.h"

#include "shared/idlib/commandlink.h"
#include "shared/idlib/text/cmdargs.h"

#include <cstdlib>
#include <cstring>

idList<idConsoleWatch, 5> cwatchList;
idList<idStr, 5> cwatchResultTextList;

static void Cmd_ConUnwatch_f(const idCmdArgs& args) {
  if (args.argc < 1)
    return;

  const char* watchName = args.argc <= 1 ? "" : args.argv[1];
  if (std::strcmp(watchName, "all") == 0) {
    cwatchList.ClearFree();
    return;
  }

  for (int index = 0; index < cwatchList.num; ++index) {
    if (cwatchList.list[index]._watchString == watchName) {
      cwatchList.RemoveIndex(index);
      return;
    }
  }
}

static void Cmd_ConWatch_f(const idCmdArgs& args) {
  idConsoleWatch newWatch;
  newWatch._watchString = args.argc <= 1 ? "" : args.argv[1];
  if (args.argc >= 3)
    newWatch._drawX = static_cast<int>(std::atol(args.argv[2]));
  if (args.argc >= 4)
    newWatch._drawY = static_cast<int>(std::atol(args.argv[3]));

  for (int index = 0; index < cwatchList.num; ++index) {
    if (idStr::Cmp(cwatchList.list[index]._watchString.data,
                   newWatch._watchString.data) == 0) {
      cwatchList.list[index] = newWatch;
      return;
    }
  }
  cwatchList.Append(newWatch);
}

static void ConWatchCommand(const idCmdArgs* args) { Cmd_ConWatch_f(*args); }
static void ConUnwatchCommand(const idCmdArgs* args) { Cmd_ConUnwatch_f(*args); }

idCommandLink cwatch(
    "con_watch", ConWatchCommand,
    "adds a new console watch - displays per frame results of console var/function printed output",
    nullptr);
idCommandLink cunwatch(
    "con_unwatch", ConUnwatchCommand,
    "removes the specified console watch. Specify 'all' to remove all watches.",
    nullptr);
