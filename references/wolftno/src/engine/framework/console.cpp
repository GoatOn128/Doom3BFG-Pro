#include "engine/framework/console.h"

#include "engine/framework/editfield.h"
#include "engine/framework/debuggraph.h"
#include "engine/framework/common.h"
#include "engine/framework/buildversion.h"
#include "engine/framework/keyinput.h"
#include "engine/framework/gamespawninfo.h"
#include "engine/framework/consolehistory.h"
#include "engine/framework/consolewatch.h"
#include "engine/framework/streamfilecache.h"
#include "engine/models/rendermodelgui.h"
#include "engine/network/snapshot.h"
#include "engine/network/snapshotprocessor.h"
#include "engine/renderer/renderersystem.h"
#include "engine/renderer/resolutionscale.h"
#include "engine/sound/sound_runtime.h"
#include "engine/sound/soundshader.h"
#include "engine/sys/sys_public.h"
#include "shared/idlib/containers/staticlist.h"
#include "shared/idlib/csystems/cvarsystemlocal.h"
#include "shared/idlib/csystems/cmdsystemlocal.h"
#include "shared/idlib/filesystem/filesystemlocal.h"
#include "shared/idlib/lib.h"
#include "shared/idlib/math/vector.h"
#include "shared/idlib/parallelism/thread.h"
#include "shared/idlib/parallelism/paralleljoblist.h"
#include "shared/idlib/runningaverage.h"
#include "shared/idlib/sys/sys_alloc.h"
#include "shared/idlib/sys/sys_time.h"
#include "shared/idlib/text/cmdargs.h"
#include "shared/idlib/text/str.h"
#include "shared/idlib/typeinfo/typeinfo.h"

#include <algorithm>
#include <cmath>
#include <cstdarg>
#include <cstdlib>
#include <cstring>
#include <new>

const char* const g_ignoreWarningTypes[]={
    "idSessionLocal::Pump was not called for"};
const char* const g_syntaxErrorWarningTypes[]={
    "Mismatched braces in decl",": warning : Unknown decl type",
    "' while loading "};
const char* const g_duplicateWarningTypes[]={
    " defined in files "," is defined more than once in decl "};
const char* const g_animWebWarningTypes[]={
    "does not contain a node with state "};
const char* const g_audioWarningTypes[]={
    " while loading sample","Resource sample ","is inside a sound portal",
    "has a completely blank .lang key. This should never happen"};
const char* const g_effectsWarningTypes[]={
    "while loading particle","Resource particle","Resource fx : ",
    " while loading fx "};
const char* const g_animWarningTypes[]={
    "StrongLoadAlias","md6Def","Unable to load or generate voice track",
    "Resource userchannel","Anim event","Anim file","while loading anim"};
const char* const g_mapWarningTypes[]={
    "Resource entityDef : ","invalid clip model size '",
    "Resource file : (maps","Encounter group '",
    "Could not find spawn spot named","failed to open (maps/","AAS ",
    "AAS2File ","GetPointOutsideObstacles: no valid ","Entity '",
    "map collision model load failed","FileResource 'maps/",
    "Static model 'maps","failed to load appropriate AAS.",
    "No MD6 model on ","actionScript entry ",
    "idEntity::BindToJoint: entity ",
    "has actionscript but spawn setting is not AIOVERRIDE_DEFAUL"};
const char* const g_scriptingWarningTypes[]={
    "idProp_WeaponStatic: ","idTarget_Count ","idUse '"};
const char* const g_modelWarningTypes[]={
    " while loading model ","Resource model ",
    "idTagInfo::FindJoint - joint ","idActor::AddAttachment - model ",
    "Unknown joint '","Resource cm : "};
const char* const g_missingFileWarningTypes[]={
    "Failed to open file ","File not found while loading ",
    "defaulted while loading ","couldn't find bink file ",
    " file not found","FileResource '","Production model not found "};
const char* const g_materialWarningTypes[]={
    "Missing diffuse image ","Missing power image","Missing bump image ",
    "Missing specular image ","Missing images for virtual material:",
    "Missing virtualTrans image","while loading image ",
    "Resource image : ","Cube map file not found: ",
    "Couldn't load cube image: ","Couldn't load image: ",
    "Cubic source file load failed ","Source file load failed for idImage ",
    "idMaterial::Parse: ","ImageAtlas:: Couldn't load image '"};
const char* const g_obsoleteWarningTypes[]={
    "skipping obsolete class variable"};
const char* const g_subtitleWarningTypes[]={
    "subtitle requested but no text found for"};

template <std::size_t Count>
bool ContainsAny(const char* text,const char* const (&types)[Count]) {
  for(const char* type:types) {
    if(idStr::Find(text,type,true,0,-1)!=-1) return true;
  }
  return false;
}

struct idWarningCollector {
  void AddText(const char* textIn);

  int numWarningsTotal{};
  int numSyntaxErrors{};
  int numDuplicate{};
  int numAnimWeb{};
  int numAudio{};
  int numEffects{};
  int numAnimation{};
  int numMap{};
  int numScripting{};
  int numModel{};
  int numMaterial{};
  int numMissingFile{};
  int numObsolete{};
  int numSubtitle{};
  int numOther{};
  int howLongToShow{600};
  int showTimer{};
};

void idWarningCollector::AddText(const char* textIn) {
  char text[1024];
  int length=0;
  while(*textIn!='\0') {
    if(idStr::IsColor(textIn)) {
      textIn+=2;
    } else {
      if(length>=1023) break;
      text[length++]=*textIn++;
    }
  }
  text[length]='\0';
  if(idStr::Find(text,"--------- Initializing Game ----------",true,0,-1)!=-1) {
    std::memset(&numWarningsTotal,0,
                static_cast<std::size_t>(15)*sizeof(int));
    showTimer=0;
    return;
  }
  if(idStr::Find(text,"WARNING: ",true,0,-1)==-1) return;
  const char* warning=text+8;
  if(ContainsAny(warning,g_ignoreWarningTypes)) return;
  ++numWarningsTotal;
  showTimer=0;
  if(ContainsAny(warning,g_syntaxErrorWarningTypes)) ++numSyntaxErrors;
  else if(ContainsAny(warning,g_duplicateWarningTypes)) ++numDuplicate;
  else if(ContainsAny(warning,g_animWebWarningTypes)) ++numAnimWeb;
  else if(ContainsAny(warning,g_audioWarningTypes)) ++numAudio;
  else if(ContainsAny(warning,g_effectsWarningTypes)) ++numEffects;
  else if(ContainsAny(warning,g_animWarningTypes)) ++numAnimation;
  else if(ContainsAny(warning,g_mapWarningTypes)) ++numMap;
  else if(ContainsAny(warning,g_scriptingWarningTypes)) ++numScripting;
  else if(ContainsAny(warning,g_modelWarningTypes)) ++numModel;
  else if(ContainsAny(warning,g_materialWarningTypes)) ++numMaterial;
  else if(ContainsAny(warning,g_missingFileWarningTypes)) ++numMissingFile;
  else if(ContainsAny(warning,g_obsoleteWarningTypes)) ++numObsolete;
  else if(ContainsAny(warning,g_subtitleWarningTypes)) ++numSubtitle;
  else ++numOther;
}

static_assert(sizeof(idWarningCollector)==68,"warning collector ABI drift");

struct showSoundDebugInfo_t {
  soundDebugInfo_t info;
  idStr shaderName;
  soundShaderParms_t soundShaderParms;
  int emitterIndex{};
  idVec3 emitterPos{};
  int runTime{};
  unsigned int padding{};
};

static_assert(sizeof(showSoundDebugInfo_t)==392,
              "shown sound debug info ABI drift");

struct overlayText_t {
  idStr text;
  justify_t justify{};
  int time{};
  float bgopacity{};
  float fgopacity{};
};

struct timeRange_t {
  rangeId_t id{};
  idColor color{};
  unsigned int padding{};
  std::uint64_t startTime{};
  std::uint64_t endTime{};
};

struct rangeLabel_t { char buf[48]; };

struct snapObject_t {
  const char* name{};
  int bytesTotal{};
  int bytesTotalForSingle{};
  int bytesChanged{};
  float avgBytesChanged{};
  int maxBytesChanged{};
  int snapObjectsTotal{};
  int snapObjectsChanged{};
  int maxSnapObjectsChanged{};
  float ms{};
  float avgMs{};
};

struct presentableObject_t {
  const char* name{};
  int presentableIndex{};
  bool skipReplication{};
  unsigned char padding[3]{};
};

struct logRanges_t {
  std::uint64_t microseconds[16];
  std::uint64_t total;
};

struct logColor_t {
  idColor color;
  const char* name{};
};

static_assert(sizeof(overlayText_t)==56,"overlay text ABI drift");
static_assert(sizeof(timeRange_t)==40,"time range ABI drift");
static_assert(sizeof(snapObject_t)==48,"snapshot object ABI drift");
static_assert(sizeof(presentableObject_t)==16,"presentable object ABI drift");
static_assert(sizeof(logRanges_t)==136,"logged ranges ABI drift");
static_assert(sizeof(logColor_t)==24,"logged color ABI drift");

const idColor idColor::colorDarkRed{0.5f,0.0f,0.0f,1.0f};
const idColor idColor::colorDarkGreen{0.0f,0.5f,0.0f,1.0f};
const idColor idColor::colorPink{1.0f,0.5f,0.5f,1.0f};
const idColor idColor::colorBrown{0.5f,0.25f,0.0f,1.0f};
const idColor idColor::colorMdGrey{0.5f,0.5f,0.5f,1.0f};
const idColor idColor::colorDkGrey{0.25f,0.25f,0.25f,1.0f};

logColor_t logColors[16]={
    {idColor::colorWhite},{idColor::colorRed},{idColor::colorDarkRed},
    {idColor::colorGreen},{idColor::colorDarkGreen},{idColor::colorBlue},
    {idColor::colorYellow},{idColor::colorMagenta},{idColor::colorCyan},
    {idColor::colorOrange},{idColor::colorPurple},{idColor::colorPink},
    {idColor::colorBrown},{idColor::colorLtGrey},{idColor::colorMdGrey},
    {idColor::colorDkGrey}};

int versionColorCounter=0;
bool printConsoleWatchText=false;

int CompareDrawSounds(const showSoundDebugInfo_t& first,
                      const showSoundDebugInfo_t& second) {
  if(first.info.volume>second.info.volume) return -1;
  if(first.info.volume<second.info.volume) return 1;
  return 0;
}

void SortDrawSounds(showSoundDebugInfo_t* base,unsigned int count) {
  if(count<=1) return;
  std::int64_t lo[128];
  std::int64_t hi[128];
  std::int64_t left=0;
  std::int64_t right=count-1;
  std::int64_t level=0;
  for(;;) {
    while(right-left<4 || level>=127) {
      if(left<right) {
        for(std::int64_t end=right;end>left;--end) {
          std::int64_t greatest=left;
          for(std::int64_t scan=left+1;scan<=end;++scan) {
            if(CompareDrawSounds(base[scan],base[greatest])>0) greatest=scan;
          }
          std::swap(base[greatest],base[end]);
        }
      }
      --level;
      if(level<0) return;
      left=lo[level];
      right=hi[level];
    }
    std::swap(base[right],base[(right+left)/2]);
    std::int64_t low=left;
    std::int64_t high=right-1;
    for(;;) {
      while(low<high && CompareDrawSounds(base[low],base[right])<0) ++low;
      while(high>low && CompareDrawSounds(base[high],base[right])>0) --high;
      if(low<high) {
        std::swap(base[low],base[high]);
        ++low;
        --high;
        if(low<high) continue;
      }
      break;
    }
    while(high>left && CompareDrawSounds(base[high],base[right])>=0) --high;
    while(low<right && CompareDrawSounds(base[low],base[right])<=0) ++low;
    std::swap(base[right],base[low]);
    if(right-low<=high-left) {
      lo[level]=left;
      hi[level++]=high;
      left=low;
    } else {
      hi[level]=right;
      lo[level++]=low;
      right=high;
    }
  }
}

class idConsoleLocal;

class ConsolePrintListener final : public idPrintListener {
public:
  ConsolePrintListener() : idPrintListener(true,false,PRINT_INFO) {}
  void Print(const char* text) override;
};

class idConsoleLocal final : public idConsole {
public:
  idConsoleLocal();
  ~idConsoleLocal() override;

  void Init() override;
  void Shutdown() override;
  void InitGuiModel() override;
  void LoadGraphics() override;
  bool ProcessEvent(const sysEvent_t* event,bool forceAccept) override;
  bool IsInitialized() override;
  bool IsActive() override;
  void ClearNotifyLines() override;
  void ClearEditLine() override;
  void Close(bool clearNotify) override;
  void Print(const char* text) override;
  void Dump(const char* fileName,idStr* outString) override;
  void Draw(bool forceFullScreen,bool skipNotifyLines) override;
  bool WasLastFullScreen() override;
  idRenderModelGui* Gui() override;
  void PrintOverlay(idOverlayHandle& handle,justify_t justify,float bgopacity,
                    float fgopacity,const char* format,...) override;
  void AddTimeRange(rangeId_t id,const idColor& color,std::uint64_t start,
                    std::uint64_t end) override;
  void SetTimeRangeLabel(rangeId_t id,const char* label) override;
  void AddJobListTimeRange(rangeId_t id,const idColor& color,
                           const idParallelJobList* jobList,
                           std::uint64_t explicitWaitMicroseconds) override;
  void BeginTimeRange(rangeId_t id,const idColor& color) override;
  idColor GetTimeRangeColor(rangeId_t id) override;
  idDebugGraph* CreateGraph(int numItems) override;
  void DestroyGraph(idDebugGraph* graph) override;
  idDebugHistoryGraph* CreateHistoryGraph(float minRange,float maxRange,
                                          int width,int height) override;
  void DestroyHistoryGraph(idDebugHistoryGraph* graph) override;
  void AddSnapObject(const char* name,int bytesTotal,int bytesChanged,
                     float ms) override;
  void ClearSnapshots() override;
  void AddSnapshots(const idSnapShot* newSnapshot,
                    const idSnapShot* oldSnapshot,int size,int peer,
                    int sequence,int baseSequence) override;
  void AddPresentableObject(const char* name,int presentableIndex,
                            bool skipReplication) override;
  void ClearPresentables() override;
  void SetBitmapConsole(unsigned char* frontBuffer0,
                        unsigned char* frontBuffer1,int width,int height,
                        int bytePitch) override;
  void DrawBitmapConsole() override;
  void Activate(float displayFraction);

private:
  void PageUp();
  void Scroll();
  void DrawOverlayText(float& leftY,float& rightY,float& centerY);
  void DrawSnapshotReport();
  void DrawTextOffsetLeftAlign(float x,float& y,float opacity,
                               const char* text,...);
  void DrawTextOffsetRightAlign(float x,float& y,float opacity,
                                const char* text,...);
  void DrawTextLeftAlign(float& y,float opacity,const char* text,...);
  void DrawTextRightAlign(float& y,float opacity,const char* text,...);
  void DrawFPS(float& leftY,float& rightY,float& centerY);
  void DrawWarnings(float& leftY,float& rightY,float& centerY);
  void DrawMemoryUsage(float& leftY,float& rightY,float& centerY);
  void DrawTimer(float& leftY,float& rightY,float& centerY);
  void Top();
  void KeyDownEvent(int key);
  void UpdateDisplayFraction();
  void DrawDebugGraphs();
  void DrawPresentables();
  void DrawComSpeeds(float& leftY,float& rightY,float& centerY);
  void DrawNotify();
  void DrawSolidConsole(float currentDisplayFrac) const;
  void DrawTimeRanges();
  void PumpThreadLines();
  void PrintInternal(const char* text,bool pumpThreadLines);
  void LogThreadSpeeds();
  void DrawSnapshots();
  void DrawVersionMapInfo();
  void DrawSounds(float& leftY,float& rightY,float& centerY,idSoundWorld* world,
                  idList<showSoundDebugInfo_t,5>& infoList);
  void MarkVBLTime() override;

public:
  ConsolePrintListener printListener;
  std::uint64_t lastRangeMicroseconds[40]{};
  idColor lastRangeColor[40];
  int timeLogFirstFrame{};
  unsigned int paddingTimeLog{};
  idList<unsigned char,5> timeLog;
  bool keyCatching{};
  unsigned char paddingKeyCatching[3]{};
  idWarningCollector warningCollector;
  idRenderModelGui* gui{};
  short text[3145729];
  short* lines[4096];
  int currentLine{};
  int lineOffset{};
  int textOffset{};
  int displayLine{};
  int nextKeyEvent{};
  float displayFrac{};
  float finalFrac{};
  int fracTime{};
  bool lastFullscreen{};
  bool isInitialized{};
  unsigned char paddingFullscreen[2]{};
  int times[4]{};
  idEditField consoleField;
  unsigned char* bitmapConsoleBuffers[2]{};
  int bitmapConsoleBytePitch{};
  int bitmapConsoleWidth{};
  int bitmapConsoleHeight{};
  unsigned int paddingBitmap{};
  idList<overlayText_t,5> overlayText;
  timeRange_t ranges[512];
  int currentRange{};
  rangeLabel_t rangeLabel[40];
  idSysMutex mutexRange;
  idDebugGraph* graphFPS{};
  idDebugGraph* graphTotalTime{};
  idDebugGraph* graphRenderTime{};
  idDebugGraph* graphGameTime{};
  idDebugGraph* graphGPUTime{};
  idDebugGraph* graphGPUMem[6]{};
  idStaticList<idDebugGraph*,32> debugGraphs;
  idStaticList<idDebugHistoryGraph*,32> debugHistoryGraphs;
  idRunningAverage avgFPS;
  idRunningAverage avgTotalTime;
  idRunningAverage avgRenderTime;
  idRunningAverage avgGameTime;
  idRunningAverage avgGPUTime;
  idList<showSoundDebugInfo_t,5> showSoundDebugInfoListGame;
  idList<showSoundDebugInfo_t,5> showSoundDebugInfoListGlobal;
  idStaticList<snapObject_t,128> snapObjects;
  float avgBytesChanged{};
  int maxSnapObjectString{};
  int lastDrawSnapshots{};
  unsigned int paddingBeforeSnapshots{};
  idSnapShot snapshots[2];
  int snapshotSeqs[2]{};
  int snapshotPeerNum{};
  bool snapshotsLocked{};
  unsigned char paddingSnapshotsLocked[3]{};
  int snapshotsMaxSize{};
  unsigned int paddingSnapshotsMaxSize{};
  idStaticList<presentableObject_t,32> presentableObjects;
  int maxPresentableObjectString{};
  int lastDrawPresentables{};
  std::uint64_t lastVblMicroseconds{};
  std::uint64_t vblIntervalMicroseconds{};
  std::uint64_t rangeSyncTime{};
  idList<idStr,5> strings;
  idSysMutex stringsMutex;
};

static_assert(offsetof(idConsoleLocal,lastRangeMicroseconds)==0x20,
              "console range-time offset drift");
static_assert(offsetof(idConsoleLocal,lastRangeColor)==0x160,
              "console range-color offset drift");
static_assert(offsetof(idConsoleLocal,timeLog)==0x3E8,
              "console time-log offset drift");
static_assert(offsetof(idConsoleLocal,keyCatching)==0x400,
              "console activation offset drift");
static_assert(offsetof(idConsoleLocal,warningCollector)==0x404,
              "console warning collector offset drift");
static_assert(offsetof(idConsoleLocal,gui)==0x448,
              "console GUI offset drift");
static_assert(offsetof(idConsoleLocal,text)==0x450,
              "console text offset drift");
static_assert(offsetof(idConsoleLocal,lines)==0x600458,
              "console line index offset drift");
static_assert(offsetof(idConsoleLocal,consoleField)==0x608490,
              "console edit-field offset drift");
static_assert(offsetof(idConsoleLocal,overlayText)==0x609D10,
              "console overlay offset drift");
static_assert(offsetof(idConsoleLocal,mutexRange)==0x60F4B0,
              "console range mutex offset drift");
static_assert(offsetof(idConsoleLocal,debugGraphs)==0x60F510,
              "console graph list offset drift");
static_assert(offsetof(idConsoleLocal,snapObjects)==0x60F810,
              "console snap-object offset drift");
static_assert(offsetof(idConsoleLocal,snapshots)==0x611038,
              "console snapshots offset drift");
static_assert(offsetof(idConsoleLocal,presentableObjects)==0x6110F0,
              "console presentables offset drift");
static_assert(offsetof(idConsoleLocal,strings)==0x611328,
              "console queued strings offset drift");
static_assert(offsetof(idConsoleLocal,stringsMutex)==0x611340,
              "console string mutex offset drift");
static_assert(sizeof(idConsoleLocal)==0x611348,
              "console local ABI drift");

extern idConsoleLocal consoleLocal;

extern float SMALLCHAR_HEIGHT;
extern float SMALLCHAR_WIDTH;
extern int time_FPS;
extern idCVar com_showMemoryUsage;
extern idCVar com_showFPS;
extern idCVar com_timer;
extern idCVar con_fontSize;
extern idCVar con_speed;
extern idCVar com_drawSnapshotMetrics;
extern idCVar com_drawSnapshotMetricsPeer;
extern idCVar com_speeds;
extern int com_editors;
extern idCVar com_drawThreadSpeeds;
extern idCVar con_logThreadSpeeds;
extern idCVar con_notifyTime;
extern idCVar r_consoleTransparency;
extern idCVar com_threadSpeedScale;
extern idCVar com_drawSnapshots;
extern idCVar con_drawVersion;
extern idCVar con_drawVersionOpaque;
extern idCVar g_consoleOverlayBackgroundOpacity;
extern idCVar g_consoleOverlayForegroundOpacity;
extern idCVar s_showSounds;
extern idCVar s_showSoundsFreeze;
extern idCVar s_showSoundsFilter;
extern idCVar s_soundGroupFilter;
extern idTypeInfoTools* typeInfoTools;
extern idCVar con_noPrint;
extern idCVar com_showWarningDisplay;
extern idCVar com_drawPresentables;
extern unsigned char consoleFont[];
extern int count_numGameFrames;
extern int time_gameFrame;
extern int time_maxGameFrame;
extern std::uint64_t time_frame;
extern int time_renderFrame;
extern int time_GPUFrameTime;
extern std::int64_t mem_GPUMem[];
extern const char* mem_GPUNames[];
extern idCVar rs_dropMilliseconds;
extern idCVar rs_raiseMilliseconds;

class ScopedConsoleSystemHeap {
public:
  ScopedConsoleSystemHeap() { idMem::Singleton()->PushHeap(HEAP_SYSTEMHEAP); }
  ~ScopedConsoleSystemHeap() { idMem::Singleton()->PopHeap(); }
};

int ConsoleGameMillisecondsPerFrame(idGame* game) {
  const auto slots=*reinterpret_cast<const void* const* const*>(game);
  using Fn=int(*)(idGame*,int);
  return reinterpret_cast<Fn>(slots[49])(game,0);
}

int ConsoleGameHz(idGame* game,int timeType) {
  const auto slots=*reinterpret_cast<const void* const* const*>(game);
  using Fn=int(*)(idGame*,int);
  return reinterpret_cast<Fn>(slots[44])(game,timeType);
}

float ConsoleGameRealMillisecondsPerFrame(idGame* game,int timeType) {
  const auto slots=*reinterpret_cast<const void* const* const*>(game);
  using Fn=float(*)(idGame*,int);
  return reinterpret_cast<Fn>(slots[51])(game,timeType);
}

const idGameSpawnInfo* ConsoleGameSpawnInfo(idGame* game) {
  const auto slots=*reinterpret_cast<const void* const* const*>(game);
  using Fn=const idGameSpawnInfo*(*)(idGame*);
  return reinterpret_cast<Fn>(slots[12])(game);
}

bool ConsoleSoundEmitterDebugInfo(
    const idSoundEmitter* emitter,unsigned int index,soundDebugInfo_t* info) {
  const auto slots=*reinterpret_cast<const void* const* const*>(emitter);
  using Fn=bool(*)(const idSoundEmitter*,unsigned int,soundDebugInfo_t*);
  return reinterpret_cast<Fn>(slots[21])(emitter,index,info);
}

int ConsoleSoundEmitterChannelCount(const idSoundEmitter* emitter) {
  const auto slots=*reinterpret_cast<const void* const* const*>(emitter);
  using Fn=int(*)(const idSoundEmitter*);
  return reinterpret_cast<Fn>(slots[22])(emitter);
}

bool idTypeInfoTools::GetEnumBitFlags(
    int flags,const char* enumTypeName,idStr& result,
    const char* separator) const {
  result="";
  const idEnumInfo* info=FindEnumInfo(enumTypeName);
  if(info!=nullptr) {
    for(const idEnumValueInfo& value:info->values) {
      if(value.value!=0 && (flags&value.value)==value.value) {
        result.Append(va("%s%s",result.IsEmpty() ? "" : separator,
                         value.name.c_str()));
      }
    }
  }
  return false;
}

idConsoleLocal::idConsoleLocal()
    : printListener(),timeLog(),consoleField(),overlayText(),mutexRange(),
      debugGraphs(),debugHistoryGraphs(),avgFPS(),avgTotalTime(),
      avgRenderTime(),avgGameTime(),avgGPUTime(),
      showSoundDebugInfoListGame(),showSoundDebugInfoListGlobal(),
      snapObjects(),snapshots(),presentableObjects(),strings(),stringsMutex() {
  for(idColor& color:lastRangeColor)
    color={-1.0f,-1.0f,-1.0f,-1.0f};
  for(timeRange_t& range:ranges)
    range.color={-1.0f,-1.0f,-1.0f,-1.0f};
  for(rangeLabel_t& label:rangeLabel) {
    label.buf[0]='\0';
    label.buf[47]='\0';
  }
  text[0]=0;
  lines[0]=text;
  text[3145728]=0;
}

idConsoleLocal::~idConsoleLocal() {
  for(int index=0;index<debugGraphs.Num();++index) {
    idDebugGraph* graph=debugGraphs[index];
    if(graph!=nullptr) {
      graph->~idDebugGraph();
      idMem::Singleton()->Free(graph,ALIGN_16);
      debugGraphs[index]=nullptr;
    }
  }
  debugGraphs.ClearFree();
  for(int index=0;index<debugHistoryGraphs.Num();++index) {
    idDebugHistoryGraph* graph=debugHistoryGraphs[index];
    if(graph!=nullptr) {
      graph->~idDebugHistoryGraph();
      idMem::Singleton()->Free(graph,ALIGN_16);
      debugHistoryGraphs[index]=nullptr;
    }
  }
  debugHistoryGraphs.ClearFree();
}

void ConsolePrintListener::Print(const char* text) {
  reinterpret_cast<idConsoleLocal*>(
      reinterpret_cast<unsigned char*>(this)-8)->Print(text);
}

void idConsoleLocal::Init() {
  printListener.threadSafe=true;
  printListener.RegisterPrintListener();
  keyCatching=false;
  nextKeyEvent=0;
  consoleField.Clear();
  snapshotsLocked=false;
}

bool idConsoleLocal::IsActive() {
  return keyCatching || lastFullscreen;
}

void idConsoleLocal::PageUp() {
  displayLine-=2;
  if(currentLine-displayLine>=4096) displayLine=currentLine-4095;
}

void idConsoleLocal::Scroll() {
  const int now=static_cast<int>(Sys_Milliseconds());
  if(nextKeyEvent>now) return;
  if(idKeyInput::IsDown(0,K_PGUP)) {
    while(nextKeyEvent<=now) {
      PageUp();
      nextKeyEvent+=ConsoleGameMillisecondsPerFrame(common->Game());
    }
  } else if(idKeyInput::IsDown(0,K_PGDN)) {
    while(nextKeyEvent<=now) {
      displayLine+=2;
      if(displayLine>currentLine) displayLine=currentLine;
      nextKeyEvent+=ConsoleGameMillisecondsPerFrame(common->Game());
    }
  }
}

void idConsoleLocal::Activate(float displayFraction) {
  finalFrac=displayFraction;
  fracTime=static_cast<int>(Sys_Milliseconds());
  keyCatching=displayFraction!=0.0f;
}

static void DrawVersionString(idRenderModelGui* gui,const char* version,
                              const int sx,const int width,const int sy) {
  const int length=idStr::Length(version);
  const float y=static_cast<float>(static_cast<int>(
      static_cast<float>(sy)-(SMALLCHAR_HEIGHT*0.5f+SMALLCHAR_HEIGHT)));
  for(int index=0;index<length;++index) {
    gui->DrawChar(
        static_cast<float>(sx+width)-
            static_cast<float>(length-index)*SMALLCHAR_WIDTH,
        y,static_cast<unsigned char>(version[index]),1.0f);
  }
}

int R_QsortSnapObjects(const void* first,const void* second) {
  const auto* const* a=static_cast<snapObject_t* const*>(first);
  const auto* const* b=static_cast<snapObject_t* const*>(second);
  return (*b)->maxBytesChanged-(*a)->maxBytesChanged;
}

void idConsoleLocal::DrawSnapshotReport() {
  snapshotMetrics.GenerateReport(
      &snapshots[0],&snapshots[1],snapshotSeqs[0],snapshotSeqs[1],
      snapshotPeerNum,Gui(),false,false);
}

void idConsoleLocal::SetBitmapConsole(
    unsigned char* frontBuffer0,unsigned char* frontBuffer1,int width,
    int height,int bytePitch) {
  bitmapConsoleBuffers[0]=frontBuffer0;
  bitmapConsoleBuffers[1]=frontBuffer1;
  bitmapConsoleHeight=height;
  bitmapConsoleWidth=width;
  bitmapConsoleBytePitch=bytePitch;
}

void print_f(const idCmdArgs& args) {
  int allocationSize=0;
  char* escaped=nullptr;
  for(int index=1;index<args.argc;++index) {
    const char* argument=args.Argv(index);
    const int length=idStr::Length(argument);
    const int needed=length+1;
    if(allocationSize<needed) {
      if(escaped!=nullptr) idMem::Singleton()->Free(escaped,ALIGN_16);
      allocationSize=needed;
      escaped=static_cast<char*>(idMem::Singleton()->AllocWithLocation(
          "w:\\build\\tech5\\engine\\framework\\Console.cpp(526) : TAG_TEMP",
          static_cast<std::size_t>(needed),TAG_TEMP,false,ALIGN_16,
          HEAP_DEFAULTHEAP));
    }
    idStr::ToEscapedString(argument,length,escaped,needed);
    idLib::Printf(escaped);
  }
  if(escaped!=nullptr) idMem::Singleton()->Free(escaped,ALIGN_16);
}

void activateConsole_f(const idCmdArgs& args) {
  if(args.argc==2) {
    float fraction=static_cast<float>(std::atof(args.argv[1]));
    if(fraction<0.0f) fraction=0.0f;
    else if(fraction>1.0f) fraction=1.0f;
    consoleLocal.Activate(fraction);
  } else {
    idLib::Printf("usage: activateConsole <fraction>\n");
  }
}

void idConsoleLocal::DrawTextOffsetLeftAlign(
    float x,float& y,float opacity,const char* format,...) {
  std::va_list arguments;
  va_start(arguments,format);
  if(gui->GetVirtualHeight()>y) {
    idColor textColor{
        idColor::colorWhite.r,idColor::colorWhite.g,idColor::colorWhite.b,
        opacity};
    char string[1024];
    idStr::vsnPrintf(string,sizeof(string),format,arguments);
    gui->DrawString(x,static_cast<float>(static_cast<int>(y+2.0f)),string,
                    textColor,false,1.0f);
  }
  va_end(arguments);
  y+=SMALLCHAR_HEIGHT+4.0f;
}

void idConsoleLocal::DrawTextOffsetRightAlign(
    float x,float& y,float opacity,const char* format,...) {
  std::va_list arguments;
  va_start(arguments,format);
  if(gui->GetVirtualHeight()>y) {
    idColor textColor{
        idColor::colorWhite.r,idColor::colorWhite.g,idColor::colorWhite.b,
        opacity};
    char string[1024];
    const int length=idStr::vsnPrintf(
        string,sizeof(string),format,arguments);
    gui->DrawString(x-static_cast<float>(length)*SMALLCHAR_WIDTH,
                    static_cast<float>(static_cast<int>(y+2.0f)),string,
                    textColor,false,1.0f);
  }
  va_end(arguments);
  y+=SMALLCHAR_HEIGHT+4.0f;
}

void idConsoleLocal::DrawTextLeftAlign(
    float& y,float opacity,const char* format,...) {
  std::va_list arguments;
  va_start(arguments,format);
  if(gui->GetVirtualHeight()>y) {
    idColor textColor{
        idColor::colorWhite.r,idColor::colorWhite.g,idColor::colorWhite.b,
        opacity};
    char string[1024];
    idStr::vsnPrintf(string,sizeof(string),format,arguments);
    gui->DrawString(0.0f,static_cast<float>(static_cast<int>(y+2.0f)),
                    string,textColor,false,1.0f);
  }
  va_end(arguments);
  y+=SMALLCHAR_HEIGHT+4.0f;
}

void idConsoleLocal::DrawTextRightAlign(
    float& y,float opacity,const char* format,...) {
  std::va_list arguments;
  va_start(arguments,format);
  if(gui->GetVirtualHeight()>y) {
    idColor textColor{
        idColor::colorWhite.r,idColor::colorWhite.g,idColor::colorWhite.b,
        opacity};
    char string[1024];
    idStr::vsnPrintf(string,sizeof(string),format,arguments);
    const int length=idStr::LengthWithoutColors(string);
    gui->DrawString(
        static_cast<float>(renderSystem->GetWidth())-
            static_cast<float>(length)*SMALLCHAR_WIDTH,
        static_cast<float>(static_cast<int>(y+2.0f)),string,textColor,false,
        1.0f);
  }
  va_end(arguments);
  y+=SMALLCHAR_HEIGHT+4.0f;
}

void idConsoleLocal::DrawFPS(float&,float& rightY,float&) {
  idStr value;
  value.Format("%ifps",time_FPS);
  int stringWidth=gui->GetStringWidth(value.c_str(),1.0f);
  gui->DrawString(
      static_cast<float>(renderSystem->GetWidth()-5-stringWidth),
      static_cast<float>(static_cast<int>(rightY)+2),value.c_str(),
      idColor::colorWhite,true,1.0f);
  if(graphFPS!=nullptr) {
    graphFPS->SetValue(-1,static_cast<float>(time_FPS)/60.0f,
                       time_FPS<59 ? idColor::colorRed : idColor::colorBlue);
  }
  rightY+=SMALLCHAR_HEIGHT+4.0f;
  if(renderSystem->IsAutomaticBackgroundSwapsEnabled()) {
    value="auto-bg-swap, no rs";
    stringWidth=gui->GetStringWidth(value.c_str(),1.0f);
    gui->DrawString(
        static_cast<float>(renderSystem->GetWidth()-5-stringWidth),
        static_cast<float>(static_cast<int>(rightY)+2),value.c_str(),
        idColor::colorWhite,true,1.0f);
    rightY+=SMALLCHAR_HEIGHT+4.0f;
  }
  resolutionScale.GetConsoleText(value);
  stringWidth=gui->GetStringWidth(value.c_str(),1.0f);
  gui->DrawString(
      static_cast<float>(renderSystem->GetWidth()-5-stringWidth),
      static_cast<float>(static_cast<int>(rightY)+2),value.c_str(),
      idColor::colorWhite,true,1.0f);
  rightY+=SMALLCHAR_HEIGHT+4.0f;
}

void DrawWarning(idRenderModelGui* gui,const char* text,int& width,
                 float yText) {
  const int stringWidth=gui->GetStringWidth(text,1.0f);
  width+=stringWidth;
  const float x=static_cast<float>(renderSystem->GetWidth()-5-width);
  gui->DrawFilled(idColor::colorBlack,x-8.0f,yText+1.5f,
                  static_cast<float>(stringWidth)+4.0f,
                  SMALLCHAR_HEIGHT+4.0f);
  gui->DrawString(x,yText,text,idColor::colorRed,true,1.0f);
}

void idConsoleLocal::DrawWarnings(float&,float& rightY,float&) {
  if(warningCollector.numWarningsTotal==0 &&
     warningCollector.showTimer>warningCollector.howLongToShow) return;
  ++warningCollector.showTimer;
  int width=0;
  const float y=static_cast<float>(static_cast<int>(rightY)+2);
  idStr label;
  label.Format("NumWarningsTotal (%d)",warningCollector.numWarningsTotal);
  DrawWarning(gui,label.c_str(),width,y);
  const auto append=[&](const char* name,int count) {
    if(count<=0) return;
    width+=4;
    label.Format("%s (%d)",name,count);
    DrawWarning(gui,label.c_str(),width,y);
  };
  append("NumAnimweb",warningCollector.numAnimWeb);
  append("NumAnimation",warningCollector.numAnimation);
  append("NumAudio",warningCollector.numAudio);
  append("NumMap",warningCollector.numMap);
  append("NumModel",warningCollector.numModel);
  append("NumMissingFile",warningCollector.numMissingFile);
  append("NumSubtitle",warningCollector.numSubtitle);
  rightY+=SMALLCHAR_HEIGHT+4.0f;
}

void idConsoleLocal::DrawMemoryUsage(float&,float& rightY,float&) {
  bool drewMemory=false;
  if(com_showMemoryUsage.valueInteger==1 ||
     (com_showMemoryUsage.valueInteger>=2 && com_showFPS.valueInteger!=0)) {
    drewMemory=true;
    const std::uint64_t usage=Sys_GetMemoryUsage();
    float usageFloat=static_cast<float>(static_cast<int>(usage));
    if(static_cast<std::int64_t>(usage)<0) usageFloat+=1.8446744e19f;
    DrawTextRightAlign(rightY,1.0f,"%.2fMB",
                       usageFloat*0.00000095367432f);
  }
  const float limit=static_cast<float>(static_cast<int>(
      streamFileCache->GetCurrentUsageLimit()))*0.00000095367432f;
  const char* color;
  if(drewMemory) {
    if(limit>=8.0f) color=limit>=16.0f ? "^0" : "^3";
    else color="^1";
  } else {
    if(com_showMemoryUsage.valueInteger!=2 || limit>=8.0f) return;
    color="^1";
  }
  const char* warning=limit<8.0f ? "(Warning: LOW)" : "";
  DrawTextRightAlign(rightY,1.0f,"SFC Limit:%s%s %.1fMB%s",color,warning,
                     limit,"^0");
}

void idConsoleLocal::DrawTimer(float& leftY,float& rightY,float& centerY) {
  int startFrame=com_timer.valueInteger;
  if(startFrame<0) {
    com_timer.SetInteger(idLib::frameNumber,true);
    startFrame=idLib::frameNumber;
  }
  (void)leftY;
  (void)centerY;
  const float elapsed=static_cast<float>(idLib::frameNumber)-
                      static_cast<float>(startFrame);
  const int frequency=ConsoleGameHz(common->Game(),1);
  idStr value;
  value.Format("%1.2f",elapsed/static_cast<float>(frequency));
  const int width=static_cast<int>(
      static_cast<float>(idStr::Length(value.c_str()))*SMALLCHAR_WIDTH);
  gui->DrawString(
      static_cast<float>(renderSystem->GetWidth()-5-width),
      static_cast<float>(static_cast<int>(rightY)+2),value.c_str(),
      idColor::colorWhite,true,1.0f);
  rightY+=SMALLCHAR_HEIGHT+4.0f;
}

idDebugGraph* idConsoleLocal::CreateGraph(int numItems) {
  ScopedConsoleSystemHeap systemHeap;
  void* memory=idMem::Singleton()->AllocWithLocation(
      "w:\\build\\tech5\\shared\\idlib\\Heap.h(95) : tag",
      sizeof(idDebugGraph),TAG_DEBUG,false,ALIGN_16,HEAP_DEFAULTHEAP);
  idDebugGraph* graph=nullptr;
  try {
    graph=memory!=nullptr ? new(memory) idDebugGraph(numItems) : nullptr;
  } catch(...) {
    if(memory!=nullptr) idMem::Singleton()->Free(memory,ALIGN_16);
    throw;
  }
  debugGraphs.Append(graph);
  return graph;
}

void idConsoleLocal::DestroyGraph(idDebugGraph* graph) {
  for(int index=0;index<debugGraphs.Num();++index) {
    if(debugGraphs[index]==graph) {
      debugGraphs.RemoveIndex(index);
      break;
    }
  }
  if(graph!=nullptr) {
    graph->~idDebugGraph();
    idMem::Singleton()->Free(graph,ALIGN_16);
  }
}

idDebugHistoryGraph* idConsoleLocal::CreateHistoryGraph(
    float minRange,float maxRange,int width,int height) {
  ScopedConsoleSystemHeap systemHeap;
  void* memory=idMem::Singleton()->AllocWithLocation(
      "w:\\build\\tech5\\shared\\idlib\\Heap.h(95) : tag",
      sizeof(idDebugHistoryGraph),TAG_DEBUG,false,ALIGN_16,HEAP_DEFAULTHEAP);
  idDebugHistoryGraph* graph=nullptr;
  try {
    graph=memory!=nullptr ? new(memory) idDebugHistoryGraph(
        minRange,maxRange,static_cast<float>(width),static_cast<float>(height))
        : nullptr;
  } catch(...) {
    if(memory!=nullptr) idMem::Singleton()->Free(memory,ALIGN_16);
    throw;
  }
  debugHistoryGraphs.Append(graph);
  return graph;
}

void idConsoleLocal::InitGuiModel() {
  void* memory=idMem::Singleton()->AllocWithLocation(
      "w:\\build\\tech5\\shared\\idlib\\Heap.h(95) : tag",
      sizeof(idRenderModelGui),TAG_GUI_MODEL,false,ALIGN_16,
      HEAP_DEFAULTHEAP);
  try {
    gui=memory!=nullptr ? new(memory) idRenderModelGui() : nullptr;
  } catch(...) {
    if(memory!=nullptr) idMem::Singleton()->Free(memory,ALIGN_16);
    throw;
  }
  gui->SetName("ConsoleGuiModel");
}

void idConsoleLocal::LoadGraphics() {
  const float graphX=static_cast<float>(renderSystem->GetWidth())-180.0f;
  graphFPS=CreateGraph(60);
  graphFPS->position={graphX,0.0f,120.0f,SMALLCHAR_HEIGHT};
  const auto initializeTimingGraph=[&](idDebugGraph*& graph,
                                       const char* caption) {
    graph=CreateGraph(180);
    graph->position={0.0f,0.0f,0.0f,0.0f};
    graph->AddGridLine(0.5f,idColor::colorLtGrey);
    graph->clamp=true;
    graph->caption=caption;
    graph->capPos=idDebugGraph::CAP_LEFT;
  };
  initializeTimingGraph(graphTotalTime,"T:");
  initializeTimingGraph(graphRenderTime,"RT:");
  initializeTimingGraph(graphGameTime,"GT:");
  initializeTimingGraph(graphGPUTime,"GPU:");
  for(idDebugGraph*& graph:graphGPUMem) {
    graph=CreateGraph(180);
    graph->position={0.0f,0.0f,0.0f,0.0f};
    graph->AddGridLine(0.5f,idColor::colorLtGrey);
    graph->clamp=false;
    graph->scaled=true;
    graph->caption="GPUMem:";
    graph->capPos=idDebugGraph::CAP_LEFT;
  }
  avgFPS.Init(120);
  avgTotalTime.Init(120);
  avgRenderTime.Init(120);
  avgGameTime.Init(120);
  avgGPUTime.Init(120);
  gui->SetupMonospaceFont(con_fontSize.valueFloat);
  isInitialized=true;
}

void idConsoleLocal::ClearNotifyLines() {
  std::memset(times,0,sizeof(times));
}

void idConsoleLocal::Close(bool clearNotify) {
  keyCatching=false;
  finalFrac=0.0f;
  displayFrac=0.0f;
  fracTime=static_cast<int>(Sys_Milliseconds());
  if(clearNotify) ClearNotifyLines();
  idKeyInput::ClearStates();
}

void idConsoleLocal::Top() {
  if(currentLine<4096) {
    displayLine=currentLine-4096;
    if(displayLine<0) displayLine=0;
    while(displayLine<=currentLine) {
      if(lines[displayLine]!=nullptr && *lines[displayLine]!=0) break;
      ++displayLine;
    }
  } else {
    displayLine=currentLine-4095;
  }
}

void idConsoleLocal::KeyDownEvent(int key) {
  if(static_cast<unsigned>(key-K_F1)<=29u) {
    idKeyInput::ExecKeyBinding(0,BIND_DEFAULT,static_cast<keyNum_t>(key),0);
    return;
  }
  switch(key) {
    case K_L:
      if(idKeyInput::IsCtrlDown(0)) {
        for(short* line:lines) {
          if(line!=nullptr) *line=0;
        }
        displayLine=currentLine;
        return;
      }
      break;
    case K_ENTER:
    case K_KP_ENTER:
      if(!consoleField.AcceptAutoComplete()) {
        idLib::Printf("]%s\n",consoleField.GetBuffer());
        cmdSystem->AppendCommandText(consoleField.GetBuffer());
        cmdSystem->AppendCommandText("\n");
        consoleHistory.AddToHistory(consoleField.GetBuffer(),true);
        consoleField.Clear();
      }
      return;
    case K_M:
      if(idKeyInput::IsCtrlDown(0)) {
        if(!consoleField.AcceptAutoComplete()) {
          idLib::Printf("]%s\n",consoleField.GetBuffer());
          cmdSystem->AppendCommandText(consoleField.GetBuffer());
          cmdSystem->AppendCommandText("\n");
          consoleHistory.AddToHistory(consoleField.GetBuffer(),true);
          consoleField.Clear();
        }
        return;
      }
      break;
    case K_TAB:
      consoleField.AutoComplete(idKeyInput::IsShiftDown(0));
      return;
    case K_UPARROW:
    case K_P:
      if(key==K_P && !idKeyInput::IsCtrlDown(0)) break;
      {
        const idStr history=consoleHistory.RetrieveFromHistory(true);
        if(!history.IsEmpty()) consoleField.SetBuffer(history.c_str());
      }
      return;
    case K_DOWNARROW:
    case K_N:
      if(key==K_N && !idKeyInput::IsCtrlDown(0)) break;
      {
        const idStr history=consoleHistory.RetrieveFromHistory(false);
        if(!history.IsEmpty()) consoleField.SetBuffer(history.c_str());
      }
      return;
    case K_PGUP:
      PageUp();
      nextKeyEvent=static_cast<int>(Sys_Milliseconds())+200;
      return;
    case K_PGDN:
      displayLine+=2;
      if(displayLine>currentLine) displayLine=currentLine;
      nextKeyEvent=static_cast<int>(Sys_Milliseconds())+200;
      return;
    case K_MWHEELUP:
      PageUp();
      return;
    case K_MWHEELDOWN:
      displayLine+=2;
      if(displayLine>currentLine) displayLine=currentLine;
      return;
    case K_HOME:
      if(idKeyInput::IsCtrlDown(0)) {
        Top();
        return;
      }
      break;
    case K_END:
      if(idKeyInput::IsCtrlDown(0)) {
        displayLine=currentLine;
        return;
      }
      break;
    default:
      break;
  }
  consoleField.KeyDownEvent(key);
}

void idConsoleLocal::UpdateDisplayFraction() {
  const float speed=con_speed.valueFloat;
  if(speed<=0.1f) {
    fracTime=static_cast<int>(Sys_Milliseconds());
    displayFrac=finalFrac;
    return;
  }
  if(displayFrac<finalFrac) {
    const int now=static_cast<int>(Sys_Milliseconds());
    displayFrac+=static_cast<float>(now-fracTime)*speed*0.001f;
    if(displayFrac>finalFrac) displayFrac=finalFrac;
  } else if(displayFrac>finalFrac) {
    displayFrac-=static_cast<float>(
        static_cast<int>(Sys_Milliseconds())-fracTime)*speed*0.001f;
    if(displayFrac<finalFrac) displayFrac=finalFrac;
  } else {
    return;
  }
  fracTime=static_cast<int>(Sys_Milliseconds());
}

bool idConsoleLocal::ProcessEvent(const sysEvent_t* event,bool forceAccept) {
  bool controlDown=false;
  if(event->evType==SE_KEY && event->evValue==K_GRAVE)
    controlDown=idKeyInput::IsCtrlDown(0);
  if(com_drawSnapshotMetrics.valueInteger>0)
    snapshotMetrics.ProcessEvent(event);
  if(!forceAccept && controlDown) {
    if((event->evValue2&1)!=0 &&
       static_cast<unsigned char>(event->evValue2>>1)==0) {
      consoleField.ClearAutoComplete();
      if(IsActive()) {
        Close(true);
        if((com_editors&2)==0 || (com_editors&0x8000000)!=0)
          sys->Mouse()->GrabCursor(true);
      } else {
        consoleField.Clear();
        Activate(idKeyInput::IsShiftDown(0) ? 0.2f : 0.5f);
      }
    }
    return true;
  }
  if(forceAccept || IsActive()) {
    if(event->evType==SE_CHAR) {
      if(displayFrac>0.0f) consoleField.CharEvent(event->evValue);
      return true;
    }
    if(event->evType==SE_KEY) {
      if((event->evValue2&1)!=0 &&
         static_cast<unsigned char>(event->evValue2>>1)==0)
        KeyDownEvent(static_cast<keyNum_t>(event->evValue));
      return true;
    }
  }
  return false;
}

void idConsoleLocal::BeginTimeRange(rangeId_t id,const idColor& color) {
  const std::uint64_t now=Sys_Microseconds();
  idColor& previous=lastRangeColor[id];
  if(previous.r!=idColor::colorBlack.r ||
     previous.g!=idColor::colorBlack.g ||
     previous.b!=idColor::colorBlack.b ||
     previous.a!=idColor::colorBlack.a) {
    console->AddTimeRange(id,previous,lastRangeMicroseconds[id],now);
  }
  lastRangeMicroseconds[id]=now;
  previous=color;
}

void idConsoleLocal::AddTimeRange(rangeId_t id,const idColor& color,
                                  std::uint64_t start,std::uint64_t end) {
  if(com_drawThreadSpeeds.valueInteger==2 &&
     con_logThreadSpeeds.valueInteger==0) return;
  mutexRange.Lock(true);
  timeRange_t& range=ranges[currentRange++&0x1FF];
  range.id=id;
  range.color=color;
  range.startTime=start;
  range.endTime=end;
  mutexRange.Unlock();
}

void idConsoleLocal::SetTimeRangeLabel(rangeId_t id,const char* label) {
  if(com_drawThreadSpeeds.valueInteger==2 &&
     con_logThreadSpeeds.valueInteger==0) return;
  mutexRange.Lock(true);
  if(static_cast<unsigned>(id)<=0x27u)
    std::strncpy(rangeLabel[id].buf,label,47);
  mutexRange.Unlock();
}

void idConsoleLocal::AddJobListTimeRange(
    rangeId_t id,const idColor& color,const idParallelJobList* jobList,
    std::uint64_t explicitWaitMicroseconds) {
  AddTimeRange(id,idColor::colorOrange,jobList->GetSubmitTimeMicroSec(),
               jobList->GetStartTimeMicroSec());
  AddTimeRange(id,color,jobList->GetStartTimeMicroSec(),
               jobList->GetFinishTimeMicroSec());
  std::uint64_t wait=explicitWaitMicroseconds;
  if(wait==0) wait=jobList->GetWaitTimeMicroSec();
  const std::uint64_t wasted=jobList->GetTotalWastedTimeMicroSec();
  std::uint64_t processing=jobList->GetTotalProcessingTimeMicroSec()+wasted;
  const std::uint64_t wastedPercent=100*jobList->GetTotalWastedTimeMicroSec();
  if(processing==0) processing=1;
  const int percent=static_cast<int>(wastedPercent/processing);
  auto milliseconds=[](std::uint64_t value) {
    float converted=static_cast<float>(static_cast<int>(value));
    if(static_cast<std::int64_t>(value)<0) converted+=1.8446744e19f;
    return converted*0.001f;
  };
  idStr label;
  label.Format("%3d jobs, %2d syncs, %2.1f(%2.1f)ms, ^%c%1.1fw ^7%i%%",
      jobList->GetNumExecutedJobs(),jobList->GetNumSyncs(),
      milliseconds(jobList->GetTotalProcessingTimeMicroSec()),
      milliseconds(jobList->GetAvgTotalProcessingTimeMicroSec()),
      wait!=0 ? '1' : '7',milliseconds(wait),percent);
  SetTimeRangeLabel(id,label.c_str());
}

bool SameColor(const idColor& first,const idColor& second) {
  return first.r==second.r && first.g==second.g && first.b==second.b &&
         first.a==second.a;
}

logRanges_t FindLogRanges(const timeRange_t* const ranges,
                          const int maxRanges,const int currentRange,
                          rangeId_t rangeId,const idColor& delimiter) {
  logRanges_t result{};
  const int oldest=currentRange-maxRanges;
  int cursor=currentRange-1;
  while(cursor>=oldest) {
    const timeRange_t& range=ranges[cursor&(maxRanges-1)];
    if(range.id==rangeId && SameColor(range.color,delimiter)) break;
    --cursor;
  }
  if(cursor<oldest) return result;
  for(--cursor;cursor>=oldest;--cursor) {
    const timeRange_t& range=ranges[cursor&(maxRanges-1)];
    if(range.id!=rangeId) continue;
    if(SameColor(range.color,idColor::colorRed)) continue;
    int colorIndex=0;
    for(int index=0;index<16;++index) {
      if(SameColor(range.color,logColors[index].color)) {
        colorIndex=index;
        break;
      }
    }
    result.microseconds[colorIndex]+=range.endTime-range.startTime;
    if(SameColor(range.color,delimiter)) break;
  }
  for(std::uint64_t duration:result.microseconds) result.total+=duration;
  return result;
}

void idConsoleLocal::DrawDebugGraphs() {
  if(graphFPS!=nullptr) graphFPS->enable=com_showFPS.valueInteger>=2;
  const bool timingGraphs=com_speeds.valueInteger==3;
  if(graphTotalTime!=nullptr) graphTotalTime->enable=timingGraphs;
  if(graphRenderTime!=nullptr) graphRenderTime->enable=timingGraphs;
  if(graphGameTime!=nullptr) graphGameTime->enable=timingGraphs;
  if(graphGPUTime!=nullptr) graphGPUTime->enable=timingGraphs;
  for(idDebugGraph* graph:graphGPUMem) {
    if(graph!=nullptr) graph->enable=timingGraphs;
  }
  for(int index=0;index<debugGraphs.Num();++index) {
    if(debugGraphs[index]!=nullptr) debugGraphs[index]->Render(gui);
  }
  for(int index=0;index<debugHistoryGraphs.Num();++index) {
    idDebugHistoryGraph* graph=debugHistoryGraphs[index];
    if(graph!=nullptr && graph->enable) graph->Render(gui);
  }
}

void idConsoleLocal::ClearSnapshots() {
  for(int index=0;index<snapObjects.Num();++index) {
    snapObject_t& object=snapObjects[index];
    object.bytesTotal=0;
    object.bytesChanged=0;
    object.avgBytesChanged=0.0f;
    object.snapObjectsTotal=0;
    object.snapObjectsChanged=0;
  }
}

void idConsoleLocal::AddSnapshots(
    const idSnapShot* newSnapshot,const idSnapShot* oldSnapshot,int size,
    int peer,int sequence,int baseSequence) {
  if(com_drawSnapshotMetrics.valueInteger==0 ||
     (com_drawSnapshotMetricsPeer.valueInteger>=0 &&
      peer!=com_drawSnapshotMetricsPeer.valueInteger)) return;
  if(com_drawSnapshotMetrics.valueInteger==2) {
    if(snapshotsLocked) return;
    snapshotsLocked=true;
  } else {
    snapshotsLocked=false;
  }
  if(com_drawSnapshotMetrics.valueInteger==3) {
    if(size<snapshotsMaxSize) return;
    snapshotsMaxSize=size;
  } else {
    snapshotsMaxSize=0;
  }
  if(com_drawSnapshotMetrics.valueInteger==4 &&
     (newSnapshot==nullptr || newSnapshot->GetSerializerLog()==nullptr ||
      !newSnapshot->GetSerializerLog()->fragmented)) return;
  if(newSnapshot!=nullptr) {
    snapshots[0]=*newSnapshot;
    snapshotSeqs[0]=sequence;
  } else {
    snapshots[0].Clear();
  }
  if(oldSnapshot!=nullptr) {
    snapshots[1]=*oldSnapshot;
    snapshotSeqs[1]=baseSequence;
  } else {
    snapshots[1].Clear();
  }
  snapshotPeerNum=peer;
}

void idConsoleLocal::DrawPresentables() {
  int maxStringWidth=400;
  if(maxPresentableObjectString>400) {
    maxStringWidth=maxPresentableObjectString;
  }
  idColor color{1.0f,1.0f,1.0f,0.7f};
  gui->DrawFilled(color,290.0f,140.0f,
                  static_cast<float>(maxStringWidth+20),
                  static_cast<float>(presentableObjects.Num()+6)*15.0f+20.0f);
  gui->DrawString(300.0f,150.0f,"** Presentables **",
                  idColor::colorGreen,false,1.0f);
  gui->DrawString(300.0f,165.0f,"Index | SkipRep | Debug Name ",
                  idColor::colorGreen,false,1.0f);
  gui->DrawString(300.0f,180.0f,
                  "--------------------------------------------------",
                  idColor::colorGreen,false,1.0f);
  float y=195.0f;
  for(int index=0;index<presentableObjects.Num();++index) {
    const presentableObject_t& object=presentableObjects[index];
    color=idColor::colorGreen;
    const va formatted("%5i     %5i   %s",object.presentableIndex,
                       object.skipReplication,object.name);
    const idStr string(formatted);
    const int stringWidth=gui->GetStringWidth(string.c_str(),1.0f);
    gui->DrawString(300.0f,static_cast<float>(static_cast<int>(y)),
                    string.c_str(),color,false,1.0f);
    if(stringWidth>maxPresentableObjectString) {
      maxPresentableObjectString=stringWidth;
    }
    y+=15.0f;
  }
  gui->DrawString(300.0f,static_cast<float>(static_cast<int>(y)),
                  "--------------------------------------------------",
                  idColor::colorGreen,false,1.0f);
}

void idConsoleLocal::DrawBitmapConsole() {
  for(unsigned char* buffer:bitmapConsoleBuffers) {
    if(buffer==nullptr) continue;
    int current=currentLine;
    int rows=bitmapConsoleHeight/32;
    int y=bitmapConsoleHeight-96;
    const int columns=bitmapConsoleWidth/16-6;
    int drawnLines=0;
    while(rows>0 && current>=0 && y>=0) {
      if(currentLine-current<4096) {
        short* line=lines[current&0xFFF];
        int wraps=0;
        int column=0;
        int character=0;
        while(line[character]!=0) {
          if(column>=columns) {
            column=0;
            ++wraps;
          } else if(static_cast<unsigned char>(line[character])>0x20 &&
                    (character==0 ||
                     static_cast<unsigned char>(line[character-1])<=0x20)) {
            int wordLength=0;
            int wordColumn=column;
            while(static_cast<unsigned char>(line[character+wordLength])>0x20 &&
                  wordColumn<columns) {
              ++wordColumn;
              ++wordLength;
            }
            if(wordLength<columns && wordLength+column>=columns) {
              column=0;
              ++wraps;
            }
          }
          ++character;
          ++column;
        }
        int drawY=y-32*wraps;
        if(drawY<0) break;
        column=0;
        character=0;
        while(line[character]!=0) {
          if(column>=columns) {
            column=0;
            drawY+=32;
          } else if(static_cast<unsigned char>(line[character])>0x20 &&
                    (character==0 ||
                     static_cast<unsigned char>(line[character-1])<=0x20)) {
            int wordLength=0;
            int wordColumn=column;
            while(static_cast<unsigned char>(line[character+wordLength])>0x20 &&
                  wordColumn<columns) {
              ++wordColumn;
              ++wordLength;
            }
            if(wordLength<columns && wordLength+column>=columns) {
              column=0;
              drawY+=32;
            }
          }
          const idVec4& glyphColor=idStr::ColorForIndex(
              static_cast<char>(idStr::ColorIndex(
                  static_cast<char>(line[character]>>8))));
          const int red=static_cast<int>(glyphColor.x*256.0f);
          const int green=static_cast<int>(glyphColor.y*256.0f);
          const int blue=static_cast<int>(glyphColor.z*256.0f);
          bool fillToEnd=false;
          if(line[character+1]==0) {
            fillToEnd=true;
          } else if(static_cast<unsigned char>(line[character+1])>0x20 &&
                    static_cast<unsigned char>(line[character])<=0x20) {
            int wordLength=0;
            int wordColumn=column;
            while(static_cast<unsigned char>(line[character+1+wordLength])>0x20 &&
                  wordColumn<columns) {
              ++wordColumn;
              ++wordLength;
            }
            fillToEnd=wordLength<columns && wordLength+column>=columns;
          }
          int glyph=static_cast<unsigned char>(line[character]);
          do {
            if(glyph<32 || glyph>127) glyph=32;
            const unsigned char* source=&consoleFont[512*(glyph-32)];
            const int byteX=(column+3)*64;
            for(int glyphY=0;glyphY<32;++glyphY) {
              unsigned char* destination=
                  &buffer[bitmapConsoleBytePitch*(drawY+glyphY)+byteX];
              for(int glyphX=0;glyphX<16;++glyphX) {
                const int alpha=*source++;
                const std::uint32_t pixel=
                    0xFF000000u |
                    (static_cast<std::uint32_t>((blue*alpha)&0xFF00)<<8) |
                    static_cast<std::uint32_t>((green*alpha)&0xFF00) |
                    static_cast<std::uint32_t>((red*alpha)>>8);
                std::memcpy(destination+glyphX*4,&pixel,sizeof(pixel));
              }
            }
            ++column;
            glyph=32;
          } while(fillToEnd && column<columns);
          ++character;
        }
        y=drawY-32-32*wraps;
        rows-=wraps;
      }
      --current;
      ++drawnLines;
      if(drawnLines>=rows) break;
    }
  }
}

void clear_f(const idCmdArgs&) {
  for(short* line:consoleLocal.lines) {
    if(line!=nullptr) *line=0;
  }
  consoleLocal.displayLine=consoleLocal.currentLine;
}

void idConsoleLocal::DrawComSpeeds(float&,float& rightY,float&) {
  idGame* game=common->Game();
  if(game!=nullptr) {
    ConsoleGameRealMillisecondsPerFrame(game,0);
  }
  const int gameFrame=count_numGameFrames>0
      ? time_gameFrame/count_numGameFrames : 0;
  avgTotalTime.Add(static_cast<float>(time_frame)*0.001f);
  avgGameTime.Add(static_cast<float>(time_maxGameFrame)*0.001f);
  for(int index=1;index<count_numGameFrames;++index) {
    avgGameTime.Add(static_cast<float>(time_gameFrame-time_maxGameFrame)*
                    0.001f/static_cast<float>(count_numGameFrames-1));
  }
  avgRenderTime.Add(static_cast<float>(time_renderFrame)*0.001f);
  avgGPUTime.Add(static_cast<float>(time_GPUFrameTime)*0.001f);
  DrawTextRightAlign(rightY,1.0f,"avg   min   max");

  const auto totalColor=[](float value) {
    return value<=13.666666f ? "^2" :
           value<=17.666666f ? "^3" : "^1";
  };
  const auto gameColor=[](float value) {
    return value<=11.666666f ? "^2" :
           value<=15.666666f ? "^3" : "^1";
  };
  float maximum=avgTotalTime.GetMax();
  float minimum=avgTotalTime.GetMin();
  float average=avgTotalTime.GetAverage();
  DrawTextRightAlign(rightY,1.0f,"%s%s%6.2f%s%6.2f%s%6.2f","T:  ",
                     totalColor(average),average,totalColor(minimum),minimum,
                     totalColor(maximum),maximum);
  maximum=avgGameTime.GetMax();
  minimum=avgGameTime.GetMin();
  average=avgGameTime.GetAverage();
  DrawTextRightAlign(rightY,1.0f,"%s%s%6.2f%s%6.2f%s%6.2f","GT: ",
                     gameColor(average),average,gameColor(minimum),minimum,
                     gameColor(maximum),maximum);
  maximum=avgRenderTime.GetMax();
  minimum=avgRenderTime.GetMin();
  average=avgRenderTime.GetAverage();
  DrawTextRightAlign(rightY,1.0f,"%s%s%6.2f%s%6.2f%s%6.2f","RT: ",
                     totalColor(average),average,totalColor(minimum),minimum,
                     totalColor(maximum),maximum);
  const float drop=rs_dropMilliseconds.valueFloat-0.1f;
  const float raise=rs_raiseMilliseconds.valueFloat-0.1f;
  const auto gpuColor=[drop,raise](float value) {
    return value<=raise ? "^2" : value<=drop ? "^3" : "^1";
  };
  maximum=avgGPUTime.GetMax();
  minimum=avgGPUTime.GetMin();
  average=avgGPUTime.GetAverage();
  DrawTextRightAlign(rightY,1.0f,"%s%s%6.2f%s%6.2f%s%6.2f","GPU:",
                     gpuColor(average),average,gpuColor(minimum),minimum,
                     gpuColor(maximum),maximum);

  const auto totalGraphColor=[](float value)->const idColor& {
    return value<=13.666666f ? idColor::colorGreen :
           value<=17.666666f ? idColor::colorYellow : idColor::colorRed;
  };
  const auto gameGraphColor=[](float value)->const idColor& {
    return value<=11.666666f ? idColor::colorGreen :
           value<=15.666666f ? idColor::colorYellow : idColor::colorRed;
  };
  const float totalMilliseconds=static_cast<float>(time_frame)*0.001f;
  graphTotalTime->SetValue(-1,totalMilliseconds/16.666666f*0.5f,
                           totalGraphColor(totalMilliseconds));
  const float gameMilliseconds=static_cast<float>(gameFrame)*0.001f;
  graphGameTime->SetValue(-1,gameMilliseconds/16.666666f*0.5f,
                          gameGraphColor(gameMilliseconds));
  const float renderMicroseconds=renderSystem->GetUsingSMP()
      ? static_cast<float>(time_renderFrame)
      : static_cast<float>(gameFrame+time_renderFrame);
  const float renderColorMilliseconds=renderSystem->GetUsingSMP()
      ? totalMilliseconds : renderMicroseconds*0.001f;
  graphRenderTime->SetValue(-1,renderMicroseconds/16666.666f*0.5f,
                            totalGraphColor(renderColorMilliseconds));
  const float gpuMilliseconds=static_cast<float>(time_GPUFrameTime)*0.001f;
  const idColor& gpuGraphColorValue=gpuMilliseconds<=raise
      ? idColor::colorGreen : gpuMilliseconds<=drop
      ? idColor::colorYellow : idColor::colorRed;
  graphGPUTime->SetValue(-1,
      static_cast<float>(time_GPUFrameTime)/16666.666f*0.5f,
      gpuGraphColorValue);

  for(int index=0;mem_GPUNames[index]!=nullptr;++index) {
    idDebugGraph* graph=graphGPUMem[index];
    if(graph!=nullptr) {
      graph->caption=mem_GPUNames[index];
      graph->capPos=idDebugGraph::CAP_LEFT;
      graph->SetValue(-1,static_cast<float>(mem_GPUMem[index]),
                      idColor::colorGreen);
      const va label("%lld",mem_GPUMem[index]);
      graph->SetLabel(0,label.buffer);
    }
  }
  rightY+=8.0f;
  const float screenWidth=static_cast<float>(renderSystem->GetWidth());
  idDebugGraph* timingGraphs[]={
      graphTotalTime,graphGameTime,graphRenderTime,graphGPUTime};
  for(idDebugGraph* graph:timingGraphs) {
    const float graphWidth=static_cast<float>(graph->bars.Num());
    graph->position={screenWidth-graphWidth,rightY,graphWidth,30.0f};
    rightY+=graph->position.w+8.0f;
  }
  for(int index=0;mem_GPUNames[index]!=nullptr;++index) {
    idDebugGraph* graph=graphGPUMem[index];
    if(graph!=nullptr) {
      const float graphWidth=static_cast<float>(graph->bars.Num());
      graph->position={screenWidth-graphWidth,rightY,graphWidth,30.0f};
      rightY+=graph->position.w+24.0f;
    }
  }
}

void idConsoleLocal::Shutdown() {
  if(gui!=nullptr) {
    gui->CommitThisFrame();
    auto* modelBytes=reinterpret_cast<unsigned char*>(gui);
    if((modelBytes[75]&8)==0) {
      gui->CommitThisFrame();
      gui->UnlinkSubclass();
      modelBytes[75]|=8;
    }
    gui->DeleteSubclass();
    modelBytes[74]|=4;
    gui=nullptr;
  }
}

void idConsoleLocal::DrawNotify() {
  const int charactersWide=static_cast<int>(
      (static_cast<float>(renderSystem->GetWidth())-SMALLCHAR_WIDTH)/
      SMALLCHAR_WIDTH);
  int colorIndex=idStr::ColorIndex('7');
  gui->currentVertexColor=idColor::PackColor(idStr::ColorForIndex(
      static_cast<char>(colorIndex)));
  const int now=static_cast<int>(Sys_Milliseconds());
  int y=0;
  for(int lineNumber=currentLine-3;lineNumber<=currentLine;++lineNumber) {
    if(lineNumber<1) continue;
    const int lineTime=times[lineNumber&3];
    if(lineTime==0 || static_cast<float>(now-lineTime)>
                      con_notifyTime.valueFloat*1000.0f) continue;
    short* line=lines[lineNumber&0xFFF];
    int column=static_cast<int>(0.0f/SMALLCHAR_WIDTH);
    for(int character=0;line[character]!=0;++character) {
      if(column>=charactersWide) {
        column=0;
        y=static_cast<int>(static_cast<float>(y)+SMALLCHAR_HEIGHT);
      } else if(static_cast<unsigned char>(line[character])>0x20 &&
                (character==0 ||
                 static_cast<unsigned char>(line[character-1])<=0x20)) {
        int wordLength=0;
        int wordColumn=column;
        while(static_cast<unsigned char>(line[character+wordLength])>0x20 &&
              wordColumn<charactersWide) {
          ++wordColumn;
          ++wordLength;
        }
        if(wordLength<charactersWide &&
           wordLength+column>=charactersWide) {
          column=0;
          y=static_cast<int>(static_cast<float>(y)+SMALLCHAR_HEIGHT);
        }
      }
      const int nextColor=idStr::ColorIndex(
          static_cast<char>(line[character]>>8));
      if(nextColor!=colorIndex) {
        gui->currentVertexColor=idColor::PackColor(idStr::ColorForIndex(
            static_cast<char>(nextColor)));
        colorIndex=nextColor;
      }
      gui->DrawChar(static_cast<float>(++column)*SMALLCHAR_WIDTH,
                    static_cast<float>(y),
                    static_cast<unsigned char>(line[character]),1.0f);
    }
    y=static_cast<int>(static_cast<float>(y)+SMALLCHAR_HEIGHT);
  }
  gui->currentVertexColor=idColor::PackColor(idColor::colorCyan);
}

void idConsoleLocal::DrawSolidConsole(float currentDisplayFrac) const {
  int consoleHeight=static_cast<int>(
      static_cast<float>(renderSystem->GetHeight())*currentDisplayFrac);
  if(consoleHeight<=0) return;
  if(consoleHeight>renderSystem->GetHeight()) {
    consoleHeight=renderSystem->GetHeight();
  }
  const int screenWidth=renderSystem->GetWidth();
  const idColor background{0.0f,0.0f,0.0f,
                           r_consoleTransparency.valueFloat};
  float backgroundHeight=
      static_cast<float>(renderSystem->GetHeight())*currentDisplayFrac;
  if(backgroundHeight>=1.0f) {
    gui->DrawFilled(background,0.0f,0.0f,
                    static_cast<float>(renderSystem->GetWidth()),
                    backgroundHeight);
  } else {
    backgroundHeight=0.0f;
  }
  gui->DrawFilled(idColor::colorOrange,0.0f,backgroundHeight,
                  static_cast<float>(renderSystem->GetWidth()),2.0f);

  ++versionColorCounter;
  const idVec4& fullVersionColor=idStr::ColorForIndex('1');
  const idVec4 dimVersionColor{
      fullVersionColor.x*0.75f,fullVersionColor.y*0.75f,
      fullVersionColor.z*0.75f,fullVersionColor.w*0.75f};
  int colorStep=versionColorCounter%360;
  if(colorStep>=180) colorStep=360-colorStep;
  const float colorFraction=static_cast<float>(colorStep)*0.0055555557f;
  const idVec4 currentVersionColor{
      dimVersionColor.x+(fullVersionColor.x-dimVersionColor.x)*colorFraction,
      dimVersionColor.y+(fullVersionColor.y-dimVersionColor.y)*colorFraction,
      dimVersionColor.z+(fullVersionColor.z-dimVersionColor.z)*colorFraction,
      dimVersionColor.w+(fullVersionColor.w-dimVersionColor.w)*colorFraction};
  gui->currentVertexColor=idColor::PackColor(currentVersionColor);
  const float consoleHeightFloat=static_cast<float>(consoleHeight);
  if(engineVersionInfo.IsForgedBuild()) {
    const idStr binaryDescription=buildVersionInfo.GetBinaryDesc();
    DrawVersionString(gui,binaryDescription.c_str(),0,screenWidth,
        static_cast<int>(consoleHeightFloat-SMALLCHAR_HEIGHT*2.0f));
    const idStr packageDescription=buildVersionInfo.GetPackageDesc();
    DrawVersionString(gui,packageDescription.c_str(),0,screenWidth,
        static_cast<int>(consoleHeightFloat-SMALLCHAR_HEIGHT));
    const idStr componentDetails=buildVersionInfo.GetComponentDetails();
    DrawVersionString(gui,componentDetails.c_str(),0,screenWidth,
                      consoleHeight);
  } else {
    DrawVersionString(gui,versionInfo.fullVersion,0,screenWidth,consoleHeight);
    DrawVersionString(gui,versionInfo.netVersion,0,screenWidth,
        static_cast<int>(consoleHeightFloat-SMALLCHAR_HEIGHT));
  }

  gui->currentVertexColor=idColor::PackColor(
      idStr::ColorForIndex(static_cast<char>(idStr::ColorIndex('7'))));
  const int inputY=static_cast<int>(
      consoleHeightFloat-SMALLCHAR_HEIGHT*2.0f);
  gui->currentVertexColor=idColor::PackColor(idColor::colorWhite);
  gui->DrawChar(SMALLCHAR_WIDTH,static_cast<float>(inputY),']',1.0f);
  consoleField.Draw(gui,static_cast<int>(SMALLCHAR_WIDTH*2.0f),inputY,
                    static_cast<int>(static_cast<float>(screenWidth)-
                                     SMALLCHAR_WIDTH*3.0f),true);

  int rows=static_cast<int>((consoleHeightFloat-SMALLCHAR_HEIGHT)/
                            SMALLCHAR_HEIGHT);
  const int charactersWide=static_cast<int>(
      (static_cast<float>(screenWidth)-SMALLCHAR_WIDTH)/SMALLCHAR_WIDTH);
  float y=consoleHeightFloat-SMALLCHAR_HEIGHT*3.0f;
  if(displayLine!=currentLine) {
    gui->currentVertexColor=idColor::PackColor(idColor::colorWhite);
    for(int character=0;character<charactersWide;character+=4) {
      gui->DrawChar(static_cast<float>(character+1)*SMALLCHAR_WIDTH,
                    static_cast<float>(static_cast<int>(y)),'^',1.0f);
    }
    y-=SMALLCHAR_HEIGHT;
    --rows;
  }

  int lineToDraw=displayLine;
  const int lineNumberColor=idStr::ColorIndex('2');
  const int separatorColor=idStr::ColorIndex('4');
  const idStr separator(" : ");
  for(int row=0;row<rows;--lineToDraw,++row) {
    if(lineToDraw<0) break;
    if(currentLine-lineToDraw>=4096) continue;
    int column=0;
    const idStr lineNumber(va("%d",lineToDraw));
    short* line=lines[lineToDraw&0xFFF];
    gui->currentVertexColor=idColor::PackColor(idStr::ColorForIndex(
        static_cast<char>(lineNumberColor)));
    for(int index=0;index<lineNumber.Length();++index) {
      gui->DrawChar(static_cast<float>(index+1)*SMALLCHAR_WIDTH,y,
                    static_cast<unsigned char>(lineNumber[index]),1.0f);
      ++column;
    }
    gui->currentVertexColor=idColor::PackColor(idStr::ColorForIndex(
        static_cast<char>(separatorColor)));
    int separatorColumn=column+1;
    for(int index=0;index<separator.Length();++index) {
      gui->DrawChar(static_cast<float>(separatorColumn)*SMALLCHAR_WIDTH,y,
                    static_cast<unsigned char>(separator[index]),1.0f);
      ++column;
      ++separatorColumn;
    }
    const int firstTextColumn=column;
    int wraps=0;
    int measureColumn=column;
    for(int character=0;line[character]!=0;++character) {
      if(measureColumn>=charactersWide) {
        measureColumn=firstTextColumn;
        ++wraps;
      } else if(static_cast<unsigned char>(line[character])>0x20 &&
                (character==0 ||
                 static_cast<unsigned char>(line[character-1])<=0x20)) {
        int wordLength=0;
        int wordColumn=measureColumn;
        while(static_cast<unsigned char>(line[character+wordLength])>0x20 &&
              wordColumn<charactersWide) {
          ++wordLength;
          ++wordColumn;
        }
        if(wordLength<charactersWide &&
           wordLength+measureColumn>=charactersWide) {
          measureColumn=firstTextColumn;
          ++wraps;
        }
      }
      ++measureColumn;
    }
    float drawY=y-static_cast<float>(wraps)*SMALLCHAR_HEIGHT;
    column=firstTextColumn;
    int currentColor=separatorColor;
    for(int character=0;line[character]!=0;++character) {
      if(column>=charactersWide) {
        column=firstTextColumn;
        drawY+=SMALLCHAR_HEIGHT;
      } else if(static_cast<unsigned char>(line[character])>0x20 &&
                (character==0 ||
                 static_cast<unsigned char>(line[character-1])<=0x20)) {
        int wordLength=0;
        int wordColumn=column;
        while(static_cast<unsigned char>(line[character+wordLength])>0x20 &&
              wordColumn<charactersWide) {
          ++wordLength;
          ++wordColumn;
        }
        if(wordLength<charactersWide &&
           wordLength+column>=charactersWide) {
          column=firstTextColumn;
          drawY+=SMALLCHAR_HEIGHT;
        }
      }
      const int newColor=idStr::ColorIndex(
          static_cast<char>(line[character]>>8));
      if(newColor!=currentColor) {
        currentColor=newColor;
        gui->currentVertexColor=idColor::PackColor(idStr::ColorForIndex(
            static_cast<char>(currentColor)));
      }
      gui->DrawChar(static_cast<float>(++column)*SMALLCHAR_WIDTH,drawY,
                    static_cast<unsigned char>(line[character]),1.0f);
    }
    y=drawY-static_cast<float>(wraps+1)*SMALLCHAR_HEIGHT;
    rows-=wraps;
  }
}

void idConsoleLocal::DrawTimeRanges() {
  static const char* const rangeNames[40]={
      "main","render","GPU","sound","jbl coll","jbl game","jbl rmdl",
      "jbl rndr","jbl vdeo","jbl anlz","jbl virt","jbl sndo","jbl snap",
      "stream CT","stream HD","stream OD","JPU  0","JPU  1","JPU  2",
      "JPU  3","JPU  4","JPU  5","JPU  6","JPU  7","JPU  8","JPU  9",
      "JPU 10","JPU 11","JPU 12","JPU 13","JPU 14","JPU 15","JPU 16",
      "JPU 17","JPU 18","JPU 19","JPU 20","JPU 21","JPU 22","JPU 23"};
  for(int index=0;index<40;++index) {
    gui->DrawString(20.0f,static_cast<float>(static_cast<int>(
                        (SMALLCHAR_WIDTH+8.0f)*static_cast<float>(index)+
                        70.0f-4.0f)),rangeNames[index],idColor::colorWhite,
                    false,1.0f);
  }
  if(com_drawThreadSpeeds.valueInteger!=2) {
    rangeSyncTime=lastVblMicroseconds!=0
        ? lastVblMicroseconds+vblIntervalMicroseconds : Sys_Microseconds();
  }
  const bool showDurations=com_drawThreadSpeeds.valueInteger==2;
  const float rightEdge=static_cast<float>(renderSystem->GetWidth())*0.7f;
  const float scale=com_threadSpeedScale.valueFloat/1000.0f;
  for(int rangeIndex=currentRange-512;rangeIndex<currentRange;++rangeIndex) {
    const timeRange_t& range=ranges[rangeIndex&0x1FF];
    float startX=rightEdge-static_cast<float>(
        static_cast<std::uint32_t>(rangeSyncTime)-
        static_cast<std::uint32_t>(range.startTime))*scale;
    float endX=rightEdge-static_cast<float>(
        static_cast<std::uint32_t>(rangeSyncTime)-
        static_cast<std::uint32_t>(range.endTime))*scale;
    if(endX-startX<1.0f) endX=startX+1.0f;
    if(startX<100.0f) startX=100.0f;
    if(endX<=100.0f) continue;
    const float rangeY=(SMALLCHAR_WIDTH+8.0f)*
                       static_cast<float>(range.id)+70.0f;
    const float rangeWidth=endX-startX;
    gui->DrawFilled(range.color,startX,rangeY-2.0f,rangeWidth,
                    SMALLCHAR_WIDTH+5.0f);
    if(showDurations) {
      const std::uint64_t duration=range.endTime-range.startTime;
      idStr durationString(va("%.1f",static_cast<float>(duration)*0.001f));
      int length=durationString.Length();
      float stringWidth=(static_cast<float>(length)-0.5f)*SMALLCHAR_WIDTH;
      if(durationString[0]=='0') {
        stringWidth=SMALLCHAR_WIDTH*1.5f;
        durationString.StripLeadingOnce("0");
        --length;
      }
      if(rangeWidth>stringWidth) {
        int x=static_cast<int>((rangeWidth-stringWidth)*0.5f+startX);
        const float textY=rangeY-4.0f;
        gui->currentVertexColor=idColor::PackColor(idColor::colorBlack);
        for(int index=0;index<length-2;++index) {
          gui->DrawChar(static_cast<float>(x),
                        static_cast<float>(static_cast<int>(textY)),
                        static_cast<unsigned char>(durationString[index]),1.0f);
          x=static_cast<int>(static_cast<float>(x)+SMALLCHAR_WIDTH);
        }
        const float decimalAdvance=durationString[0]=='.'
            ? SMALLCHAR_WIDTH : SMALLCHAR_WIDTH*3.0f;
        const float decimalX=static_cast<float>(static_cast<int>(
            static_cast<float>(x)-decimalAdvance*0.125f));
        gui->DrawChar(decimalX,static_cast<float>(static_cast<int>(textY)),
                      '.',1.0f);
        const int digitX=static_cast<int>(
            SMALLCHAR_WIDTH*5.0f*0.125f+decimalX);
        gui->DrawChar(static_cast<float>(digitX),
                      static_cast<float>(static_cast<int>(textY)),
                      static_cast<unsigned char>(durationString[length-1]),
                      1.0f);
        gui->currentVertexColor=idColor::PackColor(idColor::colorWhite);
      }
    }
  }
  if(lastVblMicroseconds!=0) {
    for(int index=0;index<10;++index) {
      const std::uint64_t interval=vblIntervalMicroseconds*
                                   static_cast<std::uint64_t>(index);
      const float x=rightEdge-static_cast<float>(interval)*scale;
      if(x<100.0f) break;
      gui->DrawFilled(idColor::colorDkGrey,x,70.0f-4.0f,3.0f,
                      (SMALLCHAR_WIDTH+8.0f)*3.0f);
    }
  }
  for(int index=0;index<40;++index) {
    if(idStr::Length(rangeLabel[index].buf)!=0) {
      gui->DrawString(static_cast<float>(static_cast<int>(rightEdge+8.0f)),
          static_cast<float>(static_cast<int>(
              (SMALLCHAR_WIDTH+8.0f)*static_cast<float>(index)+70.0f-4.0f)),
          rangeLabel[index].buf,idColor::colorWhite,false,1.0f);
    }
  }
}

void idConsoleLocal::ClearPresentables() {
  presentableObjects.ClearFree();
}

void AddConsoleWatchText(const char* text) {
  if(text==nullptr) return;
  static idStr string;
  ScopedConsoleSystemHeap systemHeap;
  string.Append(text);
  if(string.Find('\n',0,-1)!=-1) {
    string.StripTrailingWhitespace();
    cwatchResultTextList.Append(string);
    string.Clear();
  }
}

void idConsoleLocal::PumpThreadLines() {
  if(strings.Num()<=0 || !idLib::IsMainThread()) return;
  idList<idStr,5> batchedStrings;
  {
    idScopedCriticalSection lock(stringsMutex);
    batchedStrings.num=strings.num;
    strings.num=0;
    batchedStrings.size=strings.size;
    strings.size=0;
    batchedStrings.list=strings.list;
    strings.list=nullptr;
  }
  for(int index=0;index<batchedStrings.Num();++index) {
    PrintInternal(batchedStrings[index].c_str(),false);
  }
}

void idConsoleLocal::PrintInternal(const char* input,bool pumpThreadLines) {
  if(!idLib::IsMainThread()) {
    idScopedCriticalSection lock(stringsMutex);
    const idStr string(input);
    strings.Append(string);
    return;
  }
  if(pumpThreadLines) PumpThreadLines();
  warningCollector.AddText(input);
  if(printConsoleWatchText) {
    AddConsoleWatchText(input);
    return;
  }
  const int now=static_cast<int>(Sys_Milliseconds());
  short color=static_cast<short>(idStr::ColorIndex('7'));
  short* currentText=lines[currentLine&0xFFF];
  if(currentLine>=0) times[currentLine&3]=now;
  while(*input!='\0') {
    const int character=static_cast<unsigned char>(*input);
    if(idStr::IsColor(input)) {
      const char colorCharacter=input[1]=='0' ? '7' : input[1];
      color=static_cast<short>(idStr::ColorIndex(colorCharacter));
      input+=2;
      continue;
    }
    ++input;
    if(lineOffset+textOffset>=3145723) {
      short* firstUpper=text+lineOffset;
      const int activeLine=currentLine&0xFFF;
      for(int index=0;index<4096;++index) {
        const std::uintptr_t pointer=
            reinterpret_cast<std::uintptr_t>(lines[index]);
        if(pointer>=reinterpret_cast<std::uintptr_t>(currentText) &&
           pointer<=reinterpret_cast<std::uintptr_t>(firstUpper) &&
           index!=activeLine) {
          lines[index]=text+3145728;
        }
      }
      short* secondUpper=text+lineOffset;
      for(int index=0;index<4096;++index) {
        const std::uintptr_t pointer=
            reinterpret_cast<std::uintptr_t>(lines[index]);
        if(pointer>=reinterpret_cast<std::uintptr_t>(text) &&
           pointer<=reinterpret_cast<std::uintptr_t>(secondUpper) &&
           index!=activeLine) {
          lines[index]=text+3145728;
        }
      }
      std::memmove(text,currentText,
                   static_cast<std::size_t>(lineOffset)*sizeof(short));
      lines[activeLine]=text;
      currentText=text;
      textOffset=0;
    }
    switch(character) {
      case '\t':
        do {
          currentText[lineOffset++]=static_cast<short>((color<<8)|' ');
        } while((lineOffset&3)!=0);
        break;
      case '\n': {
        currentText[lineOffset]=0;
        textOffset+=lineOffset+1;
        short* const newText=text+textOffset;
        const int activeLine=currentLine&0xFFF;
        for(int index=0;index<4096;++index) {
          const std::uintptr_t pointer=
              reinterpret_cast<std::uintptr_t>(lines[index]);
          if(pointer>=reinterpret_cast<std::uintptr_t>(currentText) &&
             pointer<=reinterpret_cast<std::uintptr_t>(newText) &&
             index!=activeLine) {
            lines[index]=text+3145728;
          }
        }
        lineOffset=0;
        if(displayLine==currentLine) ++displayLine;
        ++currentLine;
        if(currentLine>=0) times[currentLine%4]=now;
        lines[currentLine&0xFFF]=newText;
        currentText=newText;
        currentText[0]=0;
        break;
      }
      case '\r':
        if(*input!='\n') lineOffset=0;
        break;
      default:
        currentText[lineOffset++]=static_cast<short>(character|(color<<8));
        break;
    }
  }
  currentText[lineOffset]=0;
  if(currentLine>=0) times[currentLine&3]=now;
}

void idConsoleLocal::LogThreadSpeeds() {
  if(timeLog.Num()==0) {
    timeLogFirstFrame=0;
    ScopedConsoleSystemHeap systemHeap;
    if(timeLog.size<864000) timeLog.ResizeForPort(864000);
  }
  static const idColor rangeDelimiters[3]={
      idColor::colorWhite,idColor::colorOrange,idColor::colorGreen};
  for(int range=RANGE_MAIN;range<=RANGE_GPU;++range) {
    const logRanges_t logRange=FindLogRanges(
        ranges,512,currentRange,static_cast<rangeId_t>(range),
        rangeDelimiters[range]);
    for(int index=0;index<16;++index) {
      const std::uint32_t scaled=255u*
          static_cast<std::uint32_t>(logRange.microseconds[index])/50000u;
      const unsigned char clamped=scaled<=255u
          ? static_cast<unsigned char>(scaled) : 255u;
      timeLog.Append(clamped);
    }
  }
}

bool idConsoleLocal::IsInitialized() {
  return isInitialized;
}

bool idConsoleLocal::WasLastFullScreen() {
  return lastFullscreen;
}

idRenderModelGui* idConsoleLocal::Gui() {
  return gui;
}

idColor idConsoleLocal::GetTimeRangeColor(rangeId_t id) {
  return lastRangeColor[id];
}

void idConsoleLocal::MarkVBLTime() {
  const std::uint64_t now=Sys_Microseconds();
  vblIntervalMicroseconds=now-lastVblMicroseconds;
  lastVblMicroseconds=now;
}

void idConsoleLocal::ClearEditLine() {
  consoleField.Clear();
  overlayText.ClearFree();
}

void idConsoleLocal::Dump(const char* fileName,idStr* outString) {
  PumpThreadLines();
  if(fileName==nullptr && outString==nullptr) return;
  idFile* file=nullptr;
  if(fileName!=nullptr) {
    file=fileSystem->OpenFileWriteRetail(fileName,FSPATH_SAVE);
    if(file==nullptr) {
      idLib::Warning("couldn't open %s",fileName);
      return;
    }
  }
  int firstLine=currentLine-4095;
  if(firstLine<0) firstLine=0;
  while(firstLine<=currentLine) {
    short* line=lines[firstLine&0xFFF];
    int character=0;
    while(line[character]!=0 &&
          static_cast<unsigned char>(line[character])<=0x20) {
      ++character;
    }
    if(line[character]!=0) break;
    ++firstLine;
  }
  char buffer[1024];
  for(int lineNumber=firstLine;lineNumber<=currentLine;++lineNumber) {
    short* line=lines[lineNumber&0xFFF];
    int offset=0;
    while(line[offset]!=0) {
      int length=0;
      while(line[offset+length]!=0 && length<1023) {
        buffer[length]=static_cast<char>(line[offset+length]);
        ++length;
      }
      offset+=length;
      buffer[length]='\0';
      if(outString!=nullptr) outString->Append(buffer);
      if(file!=nullptr) file->Write(buffer,static_cast<std::size_t>(length));
    }
    if(outString!=nullptr) outString->Append("\r\n");
    if(file!=nullptr) file->Write("\r\n",2);
  }
  if(file!=nullptr) {
    idLib::Printf("Dumped console text to %s.\n",file->GetFullPath());
    delete file;
  }
}

void idConsoleLocal::Print(const char* input) {
  PrintInternal(input,true);
}

void idConsoleLocal::DrawOverlayText(
    float& leftY,float& rightY,float& centerY) {
  int textIndex=0;
  for(int overlayIndex=0;overlayIndex<overlayText.Num();++overlayIndex) {
    overlayText_t& overlay=overlayText[overlayIndex];
    int maximumLineLength=0;
    int lineCount=0;
    if(overlay.text.Length()>0) {
      do {
        int lineLength=1;
        while(textIndex<overlay.text.Length()) {
          if(overlay.text[textIndex]=='\n') break;
          ++lineLength;
          ++textIndex;
        }
        ++lineCount;
        if(lineLength>maximumLineLength) maximumLineLength=lineLength;
        ++textIndex;
      } while(textIndex<overlay.text.Length());
    }
    const idColor background{0.0f,0.0f,0.0f,overlay.bgopacity};
    const float width=static_cast<float>(maximumLineLength)*SMALLCHAR_WIDTH;
    const float height=(SMALLCHAR_HEIGHT+4.0f)*
                       static_cast<float>(lineCount);
    const float halfCharacter=-0.5f*SMALLCHAR_WIDTH;
    switch(overlay.justify) {
      case JUSTIFY_LEFT:
        gui->DrawFilled(background,halfCharacter,leftY,width,height);
        break;
      case JUSTIFY_RIGHT:
        gui->DrawFilled(background,
            static_cast<float>(renderSystem->GetWidth())-width+halfCharacter,
            rightY,width,height);
        break;
      case JUSTIFY_CENTER_LEFT:
      case JUSTIFY_CENTER_RIGHT:
        gui->DrawFilled(background,
            (static_cast<float>(renderSystem->GetWidth())-width+
             halfCharacter)*0.5f,centerY,width,height);
        break;
      default:
        break;
    }
    idStr singleLine;
    textIndex=0;
    int lineStart=0;
    if(overlay.text.Length()>0) {
      do {
        singleLine="";
        int index=lineStart;
        while(index<overlay.text.Length() && overlay.text[index]!='\n') {
          singleLine.Append(overlay.text[index]);
          ++index;
        }
        switch(overlay.justify) {
          case JUSTIFY_LEFT:
            DrawTextLeftAlign(leftY,overlay.fgopacity,"%s",singleLine.c_str());
            break;
          case JUSTIFY_RIGHT:
            DrawTextRightAlign(rightY,overlay.fgopacity,"%s",singleLine.c_str());
            break;
          case JUSTIFY_CENTER_LEFT:
            DrawTextOffsetLeftAlign(
                (static_cast<float>(renderSystem->GetWidth())-width)*0.5f,
                centerY,overlay.fgopacity,"%s",singleLine.c_str());
            break;
          case JUSTIFY_CENTER_RIGHT:
            DrawTextOffsetRightAlign(
                (static_cast<float>(renderSystem->GetWidth())+width)*0.5f,
                centerY,overlay.fgopacity,"%s",singleLine.c_str());
            break;
          default:
            break;
        }
        lineStart+=singleLine.Length()+1;
      } while(lineStart<overlay.text.Length());
      textIndex=0;
    }
  }
  overlayText.Clear();
  cwatchResultTextList.ClearFree();
  if(cwatchList.Num()>0) {
    printConsoleWatchText=true;
    for(int watchIndex=0;watchIndex<cwatchList.Num();++watchIndex) {
      idConsoleWatch& watch=cwatchList[watchIndex];
      cmdSystem->ExecuteCommandText(watch._watchString.c_str());
      const int drawX=watch._drawX==-1 ? 0 : watch._drawX;
      int drawY=watch._drawY==-1 ? static_cast<int>(leftY) : watch._drawY;
      for(int resultIndex=0;resultIndex<cwatchResultTextList.Num();
          ++resultIndex) {
        gui->DrawString(static_cast<float>(drawX),
                        static_cast<float>(drawY+2),
                        cwatchResultTextList[resultIndex].c_str(),
                        idColor::colorWhite,false,1.0f);
        drawY=static_cast<int>(static_cast<float>(drawY)+
                               SMALLCHAR_HEIGHT+4.0f);
      }
      if(watch._drawY==-1) leftY=static_cast<float>(drawY);
      cwatchResultTextList.ClearFree();
    }
    printConsoleWatchText=false;
  }
}

void idConsoleLocal::DrawSnapshots() {
  int bytesChangedTotal=0;
  int maxBytesChangedTotal=0;
  int bytesTotal=0;
  int objectsChangedTotal=0;
  int objectsTotal=0;
  float millisecondsTotal=0.0f;
  const int backgroundWidth=maxSnapObjectString>400
      ? maxSnapObjectString : 400;
  const idColor background{0.0f,0.0f,0.0f,0.7f};
  gui->DrawFilled(background,290.0f,140.0f,
                  static_cast<float>(backgroundWidth+20),
                  static_cast<float>(snapObjects.Num()+6)*15.0f+20.0f);
  const char* heading=nullptr;
  switch(com_drawSnapshots.valueInteger) {
    case 1: heading="** Presentables **"; break;
    case 2: heading="** Sounds **"; break;
    case 3: heading="** All **"; break;
    default: break;
  }
  if(heading!=nullptr) {
    gui->DrawString(300.0f,150.0f,heading,idColor::colorGreen,false,1.0f);
  }
  gui->DrawString(300.0f,165.0f,
                  "Bytes Changed |  Bytes | Presentables",
                  idColor::colorGreen,false,1.0f);
  gui->DrawString(300.0f,180.0f,
                  "  Avg /   Max |  Total | Changed / Total",
                  idColor::colorGreen,false,1.0f);
  gui->DrawString(300.0f,195.0f,
                  "--------------------------------------------------",
                  idColor::colorGreen,false,1.0f);
  float y=210.0f;
  idStaticList<snapObject_t*,128> sortedSnapObjects;
  for(int index=0;index<snapObjects.Num();++index) {
    sortedSnapObjects.Append(&snapObjects[index]);
  }
  if(sortedSnapObjects.Num()>0) {
    std::qsort(sortedSnapObjects.list,
               static_cast<std::size_t>(sortedSnapObjects.Num()),
               sizeof(snapObject_t*),R_QsortSnapObjects);
  }
  for(int index=0;index<snapObjects.Num();++index) {
    snapObject_t* object=sortedSnapObjects[index];
    object->avgBytesChanged+=(static_cast<float>(object->bytesChanged)-
                             object->avgBytesChanged)*0.2f;
    object->avgMs+=(object->ms-object->avgMs)*0.2f;
    bytesChangedTotal+=object->bytesChanged;
    maxBytesChangedTotal+=object->maxBytesChanged;
    bytesTotal+=object->bytesTotal;
    objectsChangedTotal+=object->snapObjectsChanged;
    objectsTotal+=object->snapObjectsTotal;
    millisecondsTotal+=object->ms;
    const idStr values(va(
        "%4i  / %4i  | %5.02fk |    %3ix / %3ix - %2.2f ms",
        static_cast<int>(object->avgBytesChanged+0.5f),
        object->maxBytesChanged,
        static_cast<float>(object->bytesTotal)*0.0009765625f,
        object->snapObjectsChanged,object->snapObjectsTotal,object->avgMs));
    const idStr name(va(" [%s]",object->name));
    const idColor& valueColor=object->bytesChanged>10
        ? idColor::colorRed : idColor::colorGreen;
    const idColor& nameColor=object->bytesTotalForSingle==-1
        ? idColor::colorBlue : valueColor;
    const int valuesWidth=gui->GetStringWidth(values.c_str(),1.0f);
    const int nameWidth=gui->GetStringWidth(name.c_str(),1.0f);
    const float drawY=static_cast<float>(static_cast<int>(y));
    gui->DrawString(300.0f,drawY,values.c_str(),valueColor,false,1.0f);
    gui->DrawString(static_cast<float>(static_cast<int>(
                        static_cast<float>(valuesWidth)+300.0f)),
                    drawY,name.c_str(),nameColor,false,1.0f);
    if(valuesWidth+nameWidth>maxSnapObjectString) {
      maxSnapObjectString=valuesWidth+nameWidth;
    }
    y+=15.0f;
  }
  gui->DrawString(300.0f,static_cast<float>(static_cast<int>(y)),
                  "--------------------------------------------------",
                  idColor::colorGreen,false,1.0f);
  y+=15.0f;
  avgBytesChanged+=(static_cast<float>(bytesChangedTotal)-
                    avgBytesChanged)*0.2f;
  const idStr totals(va(
      "%4i  / %4i  | %5.02fk |    %3ix / %3ix - %2.2f ms",
      static_cast<int>(avgBytesChanged+0.5f),maxBytesChangedTotal,
      static_cast<float>(bytesTotal)*0.0009765625f,objectsChangedTotal,
      objectsTotal,millisecondsTotal));
  gui->DrawString(300.0f,static_cast<float>(static_cast<int>(y)),
                  totals.c_str(),idColor::colorGreen,false,1.0f);
}

void idConsoleLocal::DrawVersionMapInfo() {
  idList<idStr,5> stringsToDraw;
  if(con_drawVersion.valueInteger>=1) {
    stringsToDraw.Append(com_version.resetString);
  }
  if(con_drawVersion.valueInteger>=2) {
    const idGameSpawnInfo* spawnInfo=ConsoleGameSpawnInfo(common->Game());
    idStr mapLine(spawnInfo->mapName);
    bool firstDetail=true;
    if(!spawnInfo->checkpointName.IsEmpty()) {
      mapLine.Append(": ");
      firstDetail=false;
      mapLine.Append(va("ch: %s",spawnInfo->checkpointName.c_str()));
    }
    idStr active;
    idStr deActive;
    spawnInfo->WriteLayersToCommaSeparatedStrings(active,deActive);
    if(!active.IsEmpty()) {
      mapLine.Append(firstDetail ? ": " : ", ");
      firstDetail=false;
      active.Replace("layer/tungsten/","../");
      mapLine.Append(va("act: %s",active.c_str()));
    }
    if(!deActive.IsEmpty()) {
      mapLine.Append(firstDetail ? ": " : ", ");
      deActive.Replace("layer/tungsten/","../");
      mapLine.Append(va("deact: %s",deActive.c_str()));
    }
    stringsToDraw.Append(mapLine);
  }
  const float backgroundOpacity=con_drawVersionOpaque.valueInteger!=0
      ? 1.0f : g_consoleOverlayBackgroundOpacity.valueFloat;
  int maximumWidth=0;
  int remaining=stringsToDraw.Num();
  for(int index=0;index<stringsToDraw.Num();++index,--remaining) {
    const idStr& string=stringsToDraw[index];
    const idColor background{0.0f,0.0f,0.0f,backgroundOpacity};
    const idColor foreground{1.0f,0.0f,0.0f,1.0f};
    const int stringWidth=gui->GetStringWidth(string.c_str(),1.0f);
    const int stringHeight=gui->GetStringHeight(1.0f);
    const int boxWidth=static_cast<int>(
        static_cast<float>(string.Length())*SMALLCHAR_WIDTH)+4;
    const int boxHeight=static_cast<int>(SMALLCHAR_HEIGHT)+4;
    const int rightAlignedX=renderSystem->GetWidth()-2-boxWidth;
    const int y=renderSystem->GetHeight()-2-boxHeight*remaining;
    if(boxWidth>maximumWidth) maximumWidth=boxWidth;
    gui->DrawFilled(background,
        static_cast<float>(boxWidth+rightAlignedX-maximumWidth),
        static_cast<float>(y),static_cast<float>(maximumWidth),
        static_cast<float>(boxHeight));
    const int textX=rightAlignedX+(boxWidth-stringWidth)/2;
    const int textY=(boxHeight-stringHeight)/2+y-2;
    gui->DrawString(static_cast<float>(textX),static_cast<float>(textY),
                    string.c_str(),foreground,false,1.0f);
  }
}

void idConsoleLocal::PrintOverlay(
    idOverlayHandle& handle,justify_t justify,float bgopacity,float fgopacity,
    const char* format,...) {
  std::va_list arguments;
  va_start(arguments,format);
  const int index=handle.index;
  if(index<0 || index>=overlayText.Num() ||
     overlayText[index].time!=handle.time) {
    char string[16384];
    idStr::vsnPrintf(string,sizeof(string),format,arguments);
    overlayText_t* overlay=overlayText.Alloc();
    overlay->text=string;
    overlay->justify=justify;
    overlay->time=static_cast<int>(Sys_Milliseconds());
    overlay->bgopacity=g_consoleOverlayBackgroundOpacity.valueFloat*bgopacity;
    overlay->fgopacity=g_consoleOverlayForegroundOpacity.valueFloat*fgopacity;
    handle.index=overlayText.Num()-1;
    handle.time=overlay->time;
  }
  va_end(arguments);
}

void idConsoleLocal::AddSnapObject(
    const char* name,int totalBytes,int changedBytes,float milliseconds) {
  int index=0;
  while(index<snapObjects.Num() && snapObjects[index].name!=name) ++index;
  if(index>=snapObjects.Num()) {
    if(snapObjects.Num()<128) {
      snapObject_t* object=snapObjects.Alloc();
      const int changed=changedBytes>0 ? 1 : 0;
      object->name=name;
      object->bytesTotal=totalBytes;
      object->bytesTotalForSingle=totalBytes;
      object->bytesChanged=changedBytes;
      object->ms=milliseconds;
      object->maxBytesChanged=changedBytes;
      object->snapObjectsTotal=1;
      object->snapObjectsChanged=changed;
      object->maxSnapObjectsChanged=changed;
    }
    return;
  }
  snapObject_t& object=snapObjects[index];
  if(object.bytesTotalForSingle!=totalBytes) object.bytesTotalForSingle=-1;
  object.bytesTotal+=totalBytes;
  object.bytesChanged+=changedBytes;
  object.ms+=milliseconds;
  ++object.snapObjectsTotal;
  object.snapObjectsChanged+=changedBytes>0 ? 1 : 0;
  if(object.bytesChanged>object.maxBytesChanged) {
    object.maxBytesChanged=object.bytesChanged;
  }
  if(object.snapObjectsChanged>object.maxSnapObjectsChanged) {
    object.maxSnapObjectsChanged=object.snapObjectsChanged;
  }
}

void idConsoleLocal::AddPresentableObject(
    const char* name,int presentableIndex,bool skipReplication) {
  if(presentableObjects.Num()<32) {
    presentableObject_t* object=presentableObjects.Alloc();
    object->name=name;
    object->presentableIndex=presentableIndex;
    object->skipReplication=skipReplication;
  }
}

void idConsoleLocal::DrawSounds(
    float& leftYReference,float&,float&,idSoundWorld* soundWorld,
    idList<showSoundDebugInfo_t,5>& debugInfoListReference) {
  float& leftY=leftYReference;
  idList<showSoundDebugInfo_t,5>* debugInfoList=&debugInfoListReference;
  if(soundWorld==nullptr) return;
  auto* localWorld=reinterpret_cast<idSoundWorldLocal*>(soundWorld);
  int firstEmitter=0;
  int endEmitter=0;
  if(s_showSounds.valueInteger>=0) {
    endEmitter=localWorld->NumEmitters();
  } else {
    firstEmitter=-s_showSounds.valueInteger;
    endEmitter=1-s_showSounds.valueInteger;
    if(firstEmitter>=localWorld->NumEmitters()) return;
  }
  if((s_showSoundsFreeze.flags&0x20000)!=0 ||
     s_showSoundsFreeze.valueInteger==0) {
    ScopedConsoleSystemHeap systemHeap;
    debugInfoList->Clear();
    s_showSoundsFreeze.flags&=~0x20000;
    const int soundTime=static_cast<int>(localWorld->GetSoundTime());
    for(int emitterIndex=firstEmitter;emitterIndex<endEmitter;++emitterIndex) {
      idSoundEmitter* emitter=localWorld->EmitterForIndex(emitterIndex);
      if(emitter==nullptr || !emitter->IsCurrentlyPlaying(SND_CHANNEL_ANY)) {
        continue;
      }
      const int channelCount=ConsoleSoundEmitterChannelCount(emitter);
      for(int channelIndex=0;channelIndex<channelCount;++channelIndex) {
        soundDebugInfo_t info;
        if(!ConsoleSoundEmitterDebugInfo(
               emitter,static_cast<unsigned int>(channelIndex),&info)) {
          continue;
        }
        if(!info.playing || info.volume==0.0f || info.voiceVolume==0.0f) {
          continue;
        }
        bool include=false;
        if(s_showSounds.valueInteger==3) {
          include=!info.hardwareVoice &&
                  std::exp(-6.9077554f)<info.volume;
        } else {
          include=info.hardwareVoice;
        }
        if(!include) continue;
        showSoundDebugInfo_t* shown=debugInfoList->Alloc();
        shown->info=info;
        if(info.shader!=nullptr) {
          shown->shaderName=info.hardwareVoice ? "" : "^8";
          shown->shaderName.Append(info.shader->GetName());
          shown->soundShaderParms=info.shader->parms;
        } else {
          shown->shaderName="^1<NULL>^0";
        }
        shown->emitterIndex=const_cast<idSoundEmitter*>(info.emitter)->Index();
        shown->emitterPos=
            *static_cast<const idSoundEmitterLocal*>(info.emitter)->GetPosition();
        shown->runTime=soundTime-shown->info.startTime;
        shown->info.emitter=nullptr;
        shown->info.shader=nullptr;
      }
    }
    if(debugInfoList->list!=nullptr) {
      SortDrawSounds(debugInfoList->list,
                     static_cast<unsigned int>(debugInfoList->Num()));
    }
  }

  for(int index=0;index<debugInfoList->Num();++index) {
    showSoundDebugInfo_t& shown=(*debugInfoList)[index];
    const char loopCharacter=shown.info.looping ? 'O' : ' ';
    const float volumeDb=shown.info.volume<=0.0f
        ? -999.0f : std::log(shown.info.volume)*8.6858892f;
    const float pitchSemitones=shown.info.pitch<=0.0f
        ? -999.0f : std::log(shown.info.pitch)*17.31234f;
    const char* volumeColor="^1";
    if(volumeDb<=0.0f) {
      if(volumeDb<=-15.0f) {
        if(volumeDb<=-30.0f) {
          volumeColor=volumeDb<=-60.0f ? "^8" : "^5";
        } else {
          volumeColor="^7";
        }
      } else {
        volumeColor="^3";
      }
    }
    const idStr filter(s_showSoundsFilter.valueString.c_str());
    if(!filter.IsEmpty() &&
       shown.shaderName.Find(filter.c_str(),false,0,-1)==-1) {
      continue;
    }
    const idStr groupFilter(s_soundGroupFilter.valueString.c_str());
    if(!groupFilter.IsEmpty() && idStr::Cmp(groupFilter.c_str(),"off")!=0) {
      idStr groupNames;
      typeInfoTools->GetEnumBitFlags(
          shown.soundShaderParms.groups,"soundShaderGroups_t",groupNames," ");
      if(groupNames.Find(groupFilter.c_str(),false,0,-1)==-1) continue;
    }
    idStr displayText;
    if(s_showSounds.valueInteger==2) {
      const float wetDb=shown.info.voiceWetVolume<=0.0f
          ? -999.0f : std::log(shown.info.voiceWetVolume)*8.6858892f;
      const float dryDb=shown.info.voiceDryVolume<=0.0f
          ? -999.0f : std::log(shown.info.voiceDryVolume)*8.6858892f;
      const float voiceDb=shown.info.voiceVolume<=0.0f
          ? -999.0f : std::log(shown.info.voiceVolume)*8.6858892f;
      displayText=va(
          "r:%s% 4.0fdb^0 m:% 4.0f (d:% 4.0f / w:% 4.0f) p:% 6.1f d:%5.0f t:% 7.2f %s",
          volumeColor,volumeDb,voiceDb,dryDb,wetDb,pitchSemitones,
          shown.info.distance,static_cast<float>(shown.runTime)*0.001f,
          shown.shaderName.c_str());
    } else {
      displayText=va(
          "%3i,%02i: %c %s% 3.0fdb^0 p% 6.1f t% 6.2f (% 5d,% 5d,% 5d ) %s",
          shown.emitterIndex,shown.info.channel,loopCharacter,volumeColor,
          volumeDb,pitchSemitones,static_cast<float>(shown.runTime)*0.001f,
          static_cast<int>(shown.emitterPos.x),
          static_cast<int>(shown.emitterPos.y),
          static_cast<int>(shown.emitterPos.z),shown.shaderName.c_str());
    }
    idStr textWithoutColor(displayText);
    textWithoutColor.RemoveColors();
    float shadowY=leftY+2.0f;
    DrawTextLeftAlign(shadowY,1.0f,"^9%s",textWithoutColor.c_str());
    DrawTextLeftAlign(leftY,1.0f,"%s",displayText.c_str());
    if(s_showSounds.valueInteger<0) {
      idStr sampleName(shown.info.sample);
      if(shown.info.isdefault) {
        idStr colored("^1");
        colored.Append(sampleName);
        sampleName=colored;
      } else if(shown.info.iscompressed) {
        idStr colored("^2");
        colored.Append(sampleName);
        sampleName=colored;
      }
      const char* stereo=shown.info.isstereo ? "ST" : "  ";
      DrawTextLeftAlign(leftY,1.0f,"                 %s %s",stereo,
                        sampleName.c_str());
    }
  }
  if(s_showSoundsFreeze.valueInteger!=0) {
    DrawTextLeftAlign(leftY,1.0f,"^7frozen");
  }
}

void idConsoleLocal::Draw(bool forceFullScreen,bool skipNotifyLines) {
  if(!IsInitialized() || gui==nullptr) return;
  if((con_fontSize.flags&0x20000)!=0) {
    float fontSize=con_fontSize.valueFloat;
    if(fontSize<4.0f) {
      con_fontSize.SetFloat(4.0f,true);
      fontSize=con_fontSize.valueFloat;
    }
    gui->SetupMonospaceFont(fontSize);
    con_fontSize.flags&=~0x20000;
  }
  lastFullscreen=forceFullScreen;
  gui->Clear(0x28u);
  PumpThreadLines();
  UpdateDisplayFraction();
  if(forceFullScreen) {
    DrawSolidConsole(1.0f);
  } else if(displayFrac!=0.0f) {
    Scroll();
    DrawSolidConsole(displayFrac);
  } else {
    if(con_noPrint.valueInteger!=0) return;
    if(!skipNotifyLines) DrawNotify();
  }
  if(con_noPrint.valueInteger!=0) return;
  float rightY=0.0f;
  const float initialY=std::fmax(
      SMALLCHAR_HEIGHT*4.0f,
      static_cast<float>(renderSystem->GetHeight())*displayFrac);
  float leftY=initialY;
  float centerY=initialY;
  soundSystem->RenderDebugPage(Gui());
  if(com_showWarningDisplay.valueInteger!=0) {
    DrawWarnings(leftY,rightY,centerY);
  }
  if(s_showSounds.valueInteger!=0) {
    DrawSounds(leftY,rightY,centerY,
               soundSystem->GetGlobalSoundWorld(),
               showSoundDebugInfoListGlobal);
    if(leftY>initialY) leftY+=32.0f;
    DrawSounds(leftY,rightY,centerY,
               soundSystem->GetPlayingSoundWorld(),
               showSoundDebugInfoListGame);
  }
  if(com_showFPS.valueInteger!=0) DrawFPS(leftY,rightY,centerY);
  if(com_timer.valueInteger!=0) DrawTimer(leftY,rightY,centerY);
  if(com_showMemoryUsage.valueInteger>=1) {
    DrawMemoryUsage(leftY,rightY,centerY);
  }
  if(com_speeds.valueInteger==2 || com_speeds.valueInteger==3) {
    DrawComSpeeds(leftY,rightY,centerY);
  }
  DrawOverlayText(leftY,rightY,centerY);
  if(com_drawThreadSpeeds.valueInteger!=0) DrawTimeRanges();
  if(con_logThreadSpeeds.valueInteger!=0) LogThreadSpeeds();
  DrawDebugGraphs();
  if(lastDrawSnapshots!=com_drawSnapshots.valueInteger) {
    avgBytesChanged=0.0f;
    maxSnapObjectString=0;
    snapObjects.ClearFree();
    lastDrawSnapshots=com_drawSnapshots.valueInteger;
  }
  if(com_drawSnapshots.valueInteger>0) DrawSnapshots();
  if(com_drawSnapshotMetrics.valueInteger>0 && displayFrac==0.0f) {
    DrawSnapshotReport();
  }
  if(com_drawPresentables.valueInteger!=0) DrawPresentables();
  if(con_drawVersion.valueInteger!=0) DrawVersionMapInfo();
}

void idConsoleLocal::DestroyHistoryGraph(idDebugHistoryGraph* graph) {
  for(int index=0;index<debugHistoryGraphs.Num();++index) {
    if(debugHistoryGraphs[index]==graph) {
      debugHistoryGraphs.RemoveIndex(index);
      break;
    }
  }
  if(graph!=nullptr) {
    graph->~idDebugHistoryGraph();
    idMem::Singleton()->Free(graph,ALIGN_16);
  }
}

unsigned char consoleFont[96*16*32]{};

idCVar com_allowConsole(
    "com_allowConsole","0",1,
    "allow toggling console with the tilde key",nullptr);
idCVar com_timer(
    "com_timer","0",2,
    "replaces FPS with a seconds counter, set to -1 to start, set to 0 to turn off",
    nullptr);
idCVar com_showFPS(
    "com_showFPS","0",65538,"show frames rendered per second",
    0.0f,2.0f,nullptr);
idCVar com_showWarningDisplay(
    "com_showWarningDisplay","1",1,"show warning display",nullptr);
idCVar com_showMemoryUsage(
    "com_showMemoryUsage","0",65538,
    "show memory usage. 1 = always, 2 = fc low warnings + with com_showFPS, 3 = only with com_showFPS",
    0.0f,3.0f,nullptr);
idCVar s_showSounds(
    "s_showSounds","0",65538,
    "draw sound information on the hud. negative = a specific emitter",nullptr);
idCVar s_showSoundsFreeze(
    "s_showSoundsFreeze","0",65538,
    "freeze the showSound output while 1",nullptr);
idCVar s_showSoundsFilter(
    "s_showSoundsFilter","",0,
    "filter s_showSounds data by name based on this string",nullptr);
idCVar com_drawSnapshotMetrics(
    "com_drawSnapshotMetrics","0",2,
    "Draw snapshot metrics on screen. 1 = dynamic draw, 2 = static draw, 3 = dynamic raw for largest delta, 4 = draw on fragmentation",
    nullptr);
idCVar com_drawSnapshotMetricsPeer(
    "com_drawSnapshotMetricsPeer","0",2,
    "Peer to show with com_drawSnapshotMetrics",nullptr);
idCVar r_consoleTransparency(
    "r_consoleTransparency","1.0",65540,
    "transparency of the console background 0-1",0.0f,1.0f,nullptr);
idCVar g_consoleOverlayBackgroundOpacity(
    "g_consoleOverlayBackgroundOpacity","0.75",65540,
    "opacity scale of text overlay background console windows",
    0.0f,1.0f,nullptr);
idCVar g_consoleOverlayForegroundOpacity(
    "g_consoleOverlayForegroundOpacity","1.0",65540,
    "opacity scale of text overlay foreground console windows",
    0.0f,1.0f,nullptr);

idConsoleLocal consoleLocal;
idConsole* console=reinterpret_cast<idConsole*>(&consoleLocal);

idCVar con_noPrint(
    "con_noPrint","1",17,
    "print on the console but not onscreen when console is pulled up",nullptr);
idCVar con_speed(
    "con_speed","3",4,"speed at which the console moves up and down",nullptr);
idCVar con_notifyTime(
    "con_notifyTime","3",4,
    "time messages are displayed onscreen when console is pulled up",nullptr);
idCVar com_drawSnapshots(
    "com_drawSnapshots","0",2,
    "0 = off, 1 = presentables, 2 = sound 3 = all",nullptr);
idCVar com_drawPresentables(
    "com_drawPresentables","0",1,
    "0 = off, 1 = draw all presentables we know about.",nullptr);
idCVar con_fontSize(
    "con_fontSize","10",65540,
    "Use con_fontName at this many pixels wide per char",nullptr);
idCVar con_logThreadSpeeds(
    "con_logThreadSpeeds","0",1,
    "log the thread speeds for offline graphing",nullptr);
idCVar con_drawVersion(
    "con_drawVersion","0",2,
    "0 = 0ff, 1 = draw build version, 2 = draw build version and mapinfo",
    0.0f,2.0f,nullptr);
idCVar con_drawVersionOpaque(
    "con_drawVersionOpaque","0",1,
    "0 draw drawVersion with opacity background, 2 = draw with solid background",
    nullptr);
idCVar com_superScriptDLLPath(
    "com_superScriptDLLPath","",0,"path for superscript to pull from",nullptr);
idCVar com_gameDLLPath(
    "com_gameDLLPath","",0,"path for game DLL to pull from",nullptr);

idCommandLink clear_v("clear",clear_f,"clears the console",nullptr);
idCommandLink print_v(
    "print",print_f,"Prints the arguments to the console",nullptr);
idCommandLink activateConsole_v(
    "activateConsole",activateConsole_f,
    "forces the console down ( or up ) and the desired fraction.. 0.5 is normal 0.2 is shifted",
    nullptr);
