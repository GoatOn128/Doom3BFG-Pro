#pragma once

// NEEDS_WORK(engine/framework): PDB-derived console vtable prefix required by
// renderer job timing. The complete console interface is recovered with the
// framework folder; this preserves the retail AddJobListTimeRange slot.

#include <cstdint>

struct idColor;
class idParallelJobList;
class idDebugGraph;
class idDebugHistoryGraph;
class idRenderModelGui;
class idStr;
class idSnapShot;
struct sysEvent_t;
struct idConsole;

class idOverlayHandle {
public:
  int index{-1};
  int time{};
};

enum rangeId_t : int {
  RANGE_MAIN = 0,
  RANGE_RENDER = 1,
  RANGE_GPU = 2,
  RANGE_SOUND = 3,
  RANGE_JBL_COLL = 4,
  RANGE_JBL_GAME = 5,
  RANGE_JBL_RMDL = 6,
  RANGE_JBL_RNDR = 7,
  RANGE_JBL_VDEO = 8,
  RANGE_JBL_ANLZ = 9,
  RANGE_JBL_VIRT = 10,
  RANGE_JBL_SNDO = 11,
  RANGE_JBL_SNAP = 12,
  RANGE_STREAM_CT = 13,
  RANGE_STREAM_HD = 14,
  RANGE_STREAM_OD = 15,
};

enum justify_t : int {
  JUSTIFY_LEFT,
  JUSTIFY_RIGHT,
  JUSTIFY_CENTER_LEFT,
  JUSTIFY_CENTER_RIGHT
};

struct idConsoleVTablePrefix {
  void* slotsBeforeAddTimeRange[17];
  void (*AddTimeRange)(idConsole* self, rangeId_t range,
                       const idColor* color, std::uint64_t start,
                       std::uint64_t end);
  void* slotBeforeAddJobListTimeRange;
  void (*AddJobListTimeRange)(idConsole* self, rangeId_t range,
                              const idColor* color,
                              const idParallelJobList* jobList,
                              std::uint64_t offset);
  void (*BeginTimeRange)(idConsole* self, rangeId_t range,
                         const idColor* color);
};

struct idConsole {
  virtual ~idConsole() = default;
  virtual void Init() = 0;
  virtual void Shutdown() = 0;
  virtual void InitGuiModel() = 0;
  virtual void LoadGraphics() = 0;
  virtual bool ProcessEvent(const sysEvent_t* event, bool forceAccept) = 0;
  virtual bool IsInitialized() = 0;
  virtual bool IsActive() = 0;
  virtual void ClearNotifyLines() = 0;
  virtual void ClearEditLine() = 0;
  virtual void Close(bool clearNotify) = 0;
  virtual void Print(const char* text) = 0;
  virtual void Dump(const char* fileName, idStr* outString) = 0;
  virtual void Draw(bool forceFullScreen, bool skipNotifyLines) = 0;
  virtual bool WasLastFullScreen() = 0;
  virtual idRenderModelGui* Gui() = 0;
  virtual void PrintOverlay(idOverlayHandle& handle, justify_t justify,
                            float bgopacity, float fgopacity,
                            const char* format, ...) = 0;
  virtual void AddTimeRange(rangeId_t range, const idColor& color,
                            std::uint64_t start, std::uint64_t end) = 0;
  virtual void SetTimeRangeLabel(rangeId_t range, const char* label) = 0;
  virtual void AddJobListTimeRange(rangeId_t range, const idColor& color,
                                   const idParallelJobList* jobList,
                                   std::uint64_t offset) = 0;
  virtual void BeginTimeRange(rangeId_t range, const idColor& color) = 0;
  virtual idColor GetTimeRangeColor(rangeId_t range) = 0;
  virtual idDebugGraph* CreateGraph(int count) = 0;
  virtual void DestroyGraph(idDebugGraph* graph) = 0;
  virtual idDebugHistoryGraph* CreateHistoryGraph(float minRange,
                                                   float maxRange, int width,
                                                   int height) = 0;
  virtual void DestroyHistoryGraph(idDebugHistoryGraph* graph) = 0;
  virtual void AddSnapObject(const char* name, int size, int changed,
                             float fraction) = 0;
  virtual void ClearSnapshots() = 0;
  virtual void AddSnapshots(const idSnapShot* newSnapshot,
                            const idSnapShot* oldSnapshot, int size, int peer,
                            int sequence, int baseSequence) = 0;
  virtual void AddPresentableObject(const char* name, int index,
                                    bool skipReplication) = 0;
  virtual void ClearPresentables() = 0;
  virtual void SetBitmapConsole(unsigned char* frontBuffer0,
                                unsigned char* frontBuffer1, int width,
                                int height, int bytePitch) = 0;
  virtual void DrawBitmapConsole() = 0;
  virtual void MarkVBLTime() = 0;
};

extern idConsole* console;

static_assert(sizeof(idConsole) == 8, "idConsole ABI drift");
static_assert(sizeof(idOverlayHandle) == 8, "overlay handle ABI drift");
