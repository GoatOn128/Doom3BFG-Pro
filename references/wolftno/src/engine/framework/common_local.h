#pragma once

#include "engine/framework/common.h"
#include "engine/framework/game_network.h"
#include "engine/framework/usercmd.h"
#include "engine/framework/usercmdmgr.h"
#include "engine/network/snapshot.h"
#include "shared/idlib/containers/hashindex.h"
#include "shared/idlib/containers/list.h"
#include "shared/idlib/filesystem/file.h"
#include "shared/idlib/lib.h"
#include "shared/idlib/parallelism/thread.h"
#include "shared/idlib/text/str.h"

#include <cstddef>
#include <cstdint>

class idBitMsg;
class idCmdArgs;
class idSerializer;
class idSWF;
class idSWFHelper;
class idRenderModelGui;
class idGameSpawnInfo;
struct sysEvent_t;
class idDialogInfo;
class idMapChangeListener;
class idRegression;
class idRenderWorld;
struct millisecondUnique_t;

struct idUniqueWarning {
  idUniqueWarning() : message(),count(0) {}
  explicit idUniqueWarning(const char* text) : message(text),count(1) {}
  explicit idUniqueWarning(const idStr& text) : message(text),count(1) {}
  idStr message;
  int count;
};

class idCommonLocal : public idCommon {
  friend class idRenderManager;
public:
  idCommonLocal();
  ~idCommonLocal() override;
  void Init(int,const char**,const char*) override;
  void Shutdown(int) override;
  bool IsTerminationThreadStopped() const override;
  void SyncAllBackgroundOperations(bool) override;
  void ActivateTool(bool) override;
  void RegisterMapChangeListener(idMapChangeListener*) override;
  void UnRegisterMapChangeListener(idMapChangeListener*) override;
  void Quit(int) override;
private:
  void InitCommands();
  void SetCommandLineCVars(const idCmdArgs&,bool);
  void ResetMapHeap(bool);
  void ResetNetworkingState();
  void PrepareForMapChange();
  void WriteSysEvent(const sysEvent_t&);
  bool ReadSysEvent(sysEvent_t&);
  void CheckStartupStorageRequirements();
public:
  gameFrameParms_t GameFrameParms(int) override;
private:
  void PurgeOverlays() override;
  int CalcSnapTimeBuffered(int&,int&);
public:
  void GetToolReferenceMap(toolFlag_t,idStr&) override {}
  idLocalUser* GetMasterLocalUser() const override;
  idPlayerProfile* GetProfileFromMasterLocalUser() override;
  int GetMasterLocalUserInputDevice() const override;
  int GetMasterLocalUserPlayerId() const override;
  idSWFHelper* GetSWFHelper() override;
  bool IsInGame() const override;
  snapshotState_t SnapshotState() const override;
  bool IsToolActive() const override;
  bool IsDemoPlaying() override { return Field<int>(0x6D0)==3; }
  bool IsDemoRecording() override { return Field<int>(0x6D0)==1; }
  void InitLanguageDict();
  void WriteConfigToFile(const char*) override;
  void RecordPlayback(bool,const char*) override;
  idFile* DetachActiveReadSnapshotFile() override;
  idFile* DetachActiveWriteSnapshotFile() override;
  void QueueSnapshotMemFile(idFile_Memory*,unsigned int) override;
  void ClearQueuedSnapshotFile() override;
  void PlayQueuedSnapshotFile() override;
  void StopSnapshotFileRecord() override;
  void StopSnapshotFilePlayback() override;
  void CloseSnapshotFiles() override;
  void PlaySnapshotFile(idFile*,unsigned int) override;
  void RecordSnapshotFile(idFile*,unsigned int) override;
  void ResetPlayerInput(int) override;
  void UpdateBotUserCmds(int,const usercmd_t&) override;
  void VoiceRecognition_ClearUnprocessedCmd() override;
  void VoiceRecognition_GetBareWordlist(idList<idStr,5>& outList) override;
  void UpdateConsoleDisplay(bool drawGameGui) override;
private:
  static void Hitch_f(const idCmdArgs&);
  static void Map_f(const idCmdArgs&);
  static void ComboMap_f(const idCmdArgs&);
  static void DevComboMap_f(const idCmdArgs&);
  static void CheckLoad_f(const idCmdArgs&);
  static void DevMap_f(const idCmdArgs&);
  static void RestartMap_f(const idCmdArgs&);
  static void RestartMapHere_f(const idCmdArgs&);
  static void NextMap_f(const idCmdArgs&);
  static void RestartLoadGame_f(const idCmdArgs&);
  static void RestartPassMainMenu_f(const idCmdArgs&);
  static void RestartMapAfterSwap_f(const idCmdArgs&);
  static void RestartMultiplayerInvite_f(const idCmdArgs&);
  static void Disconnect_f(const idCmdArgs&);
  static void LeaveGame_f(const idCmdArgs&);
  static void SaveGame_f(const idCmdArgs&);
  static void LoadGame_f(const idCmdArgs&);
  static void RecordDemo_f(const idCmdArgs&);
  static void PlayDemo_f(const idCmdArgs&);
  static void StopDemo_f(const idCmdArgs&);
  static void CaptureDemoFrames_f(const idCmdArgs&);
  static void CaptureDemoSounds_f(const idCmdArgs&);
  static void CompressFile_f(const idCmdArgs&);
  static void DecompressFile_f(const idCmdArgs&);
  static void DeleteGenerated_f(const idCmdArgs&);
public:
  class idLogFileWriter : public idPrintListener {
  public:
    idLogFileWriter()
        : idPrintListener(false,true,PRINT_INFO),logFile(nullptr),
          recursing(false),lock() {}
    ~idLogFileWriter() override = default;
    void Print(const char*) override;
    void Init() { logFile=nullptr;recursing=false; }
    void CloseLogFile();
    idFile* GetFile() { return logFile; }
    idFile* logFile;
    bool recursing;
    idSysMutex lock;
  };

  class idWarningList : public idPrintListener {
  public:
    idWarningList()
        : idPrintListener(true,false,PRINT_WARNING),list(),hash(1024,1024),
          caption() {}
    ~idWarningList() override=default;
    void Print(const char*) override;
    idList<idUniqueWarning,3> list;
    idHashIndex hash;
    idStrStatic<128> caption;
  };

  class idRefreshOnPrint : public idPrintListener {
  public:
    idRefreshOnPrint()
        : idPrintListener(false,false,PRINT_INFO),cl(nullptr),set(false) {}
    ~idRefreshOnPrint() override=default;
    void Print(const char*) override;
    idCommonLocal* cl;
    bool set;
  };

  void SetRefreshOnPrint(bool) override;
  void PrintWarnings() override;
  void DumpWarnings(const char*);
  void ClearWarnings(const char*) override;
  idGame* Game() override;
  bool IsLosingConnectionToHost() override;
  bool AreWeRunningPC() const override;
  bool IsInitialized() const override;
  bool IsMainMenuActive() override;
  bool IsGamePaused() override;
  bool SnapshotsAreStalled() override;
  bool IsMultiplayer() const override;
  bool IsDialogPausing() override;
  int GetRecentInputDevice() override;
  int GetFocusDeviceNum() override;
  void SetFocusDeviceNum(int) override;
  int GetPauseMenuDeviceNum() override;
  void SetPauseMenuDeviceNum(int) override;
  bool GetAutosaveEnabled() override;
  void EnableAutosaves(bool) override;
  idRenderWorld* RW() override;
  idSoundWorld* SW() override;
  idRegression* GetRegression() override;
  bool ShouldDelayShutdown() override;
  void SetShouldDelayShutdown(bool) override;
  bool GetAbnormalFileRead() override;
  void SetAbnormalFileRead(bool) override;
  bool IsPurgeOverlaysPending() override;
  int GetSnapRate() override;
  idRenderVideoOverlay* VideoOverlay() override;
  void UpdateLevelLoadPacifier(float,pacifierState_t,bool,bool=false) override;
  millisecondUnique_t PlayLoadSound() override;
  void SetLoadScreenInfo(const idStr&,const idStr&) override;
  const char* GetLoadMaterialForMission(const char*,bool) override;
  void LoadVirtualTextures();
  void StopLevelLoad(bool);
  void StartLevelLoad(const idStr&,const char*,bool);
  void StartLevelLoadGame(const idGameSpawnInfo&,bool);
  void ExecuteMapChange(const idGameSpawnInfo&,idFile*) override;
  void LeaveGame(bool);
  void Disconnect(bool);
  void CanceledFinalizedMapChangeCleanUp();
  void FinalizeMapChange(const idGameSpawnInfo&,idFile*);
  void ExecuteMapChangeForSnapshotFile(const idGameSpawnInfo&,idFile*);
  void EndLevelLoad(bool);
  void ForceLoadingBlack();
  recordState_t GetRecordState() override;
  bool HasDialogMsg(gameDialogMessages_t,bool*) override;
  void ClearDialog(gameDialogMessages_t,const char*,int) override;
  void AddDialog(gameDialogMessages_t,dialogType_t,idSWFScriptFunction*,
                 idSWFScriptFunction*,bool,const char*,int,bool,bool,bool,int) override;
  void AddDynamicDialog(gameDialogMessages_t,
      const idStaticList<idSWFScriptFunction*,4>&,
      const idStaticList<idStrId,4>&,bool,idStrStatic<256>,bool,bool,bool) override;
  void AddDialogWithTextOverride(gameDialogMessages_t,dialogType_t,
      const idStaticList<idSWFScriptFunction*,4>&,
      const idStaticList<idStrId,4>&,bool,idStrStatic<256>,bool,bool,bool) override;
  void AddDialogIntVal(const char*,int) override;
  void ClearDialogs(bool) override;
  bool IsDialogActive() override;
  idStr GetDialogMsg(gameDialogMessages_t);
protected:
  void RemoveWaitDialogs();
  void ReleaseCallBacks(int);
public:
  bool HandleDialogEvent(const sysEvent_t*);
  bool CutScenePress(sysEvent_t,bool);
protected:
  void ShowDialog(idDialogInfo&,bool);
public:
  void KillDialog();
protected:
  void ShowNextDialog(bool);
public:
  void InitDialog();
  void LoadMainMenu();
  void RenderDialog(bool);
protected:
  void AddDialogInternal(idDialogInfo&);
public:
  void DrawSnapshotMetrics(bool);
  void LoadingScreenSetup(idSWF*,idRenderModelGui*,const idGameSpawnInfo&);
  void PrepareSnapshot(idSnapShot&,gameReturn_t&,idSnapShot*) override;
private:
  void RunNormalFrame(gameReturn_t&);
  void RunNetworkSnapshotFrame(gameReturn_t&);
  void ShowJobListTimeRanges(std::uint64_t);
  void CalcPerfTimings();
public:
  void Frame() override;

private:
  void NetReceiveReliable(int,idBitMsg&,int) override;
  void NetReceiveSnapshot(idSnapShot&) override;
  void NetReceiveUsercmds(idBitMsg&) override;
  void WriteUserCmdDelta(idBitMsg&,const usercmd_t&,const usercmd_t*);
  void ReadUserCmdDelta(const idBitMsg&,usercmd_t&,const usercmd_t*);
  void NetSendUsercmds();
  void SerializePlayer(idSerializer&,gameReturnPlayer_t&);
  void InterpolateSnapshot(gameReturn_t&,gameReturn_t&,gameReturn_t&,float,bool);
  void ProcessSnapshot(idSnapShot&);
  void ProcessNextSnapshot();

  template <typename T> T& Field(std::size_t offset) {
    return *reinterpret_cast<T*>(reinterpret_cast<std::byte*>(this)+offset);
  }
  template <typename T> const T& Field(std::size_t offset) const {
    return *reinterpret_cast<const T*>(reinterpret_cast<const std::byte*>(this)+offset);
  }
  gameReturn_t& SnapCurrent() { return Field<gameReturn_t>(0x760); }
  gameReturn_t& SnapPrevious() { return Field<gameReturn_t>(0x1A6E0); }
  idSnapShot* ReceivedSnaps() { return &Field<idSnapShot>(0x34738); }
  idSnapShot& OldSnapshot() { return Field<idSnapShot>(0x34C58); }
  idUserCmdMgr& UserCmdMgr() { return Field<idUserCmdMgr>(0x34D68); }
  idSWF*& Dialog() { return Field<idSWF*>(0x5AE10); }
  bool& DialogWingstickActive() { return Field<bool>(0x5AE0C); }
  bool& DialogPause() { return Field<bool>(0x5AE0D); }
  idRenderModelGui*& DialogGuiModel() { return Field<idRenderModelGui*>(0x5AE20); }
  idStaticList<idDialogInfo,4>& MessageList() {
    return Field<idStaticList<idDialogInfo,4>>(0x5AE28);
  }
  idSWFHelper*& SwfHelper() { return Field<idSWFHelper*>(0x34CC0); }
  bool& PreventOverlayPurge() { return Field<bool>(0x34721); }

public:
#if defined(WOLF_COMMON_NETWORK_VALIDATION)
  void ValidationWriteUserCmdDelta(idBitMsg& msg,const usercmd_t& command,
                                   const usercmd_t* base) {
    WriteUserCmdDelta(msg,command,base);
  }
  void ValidationReadUserCmdDelta(const idBitMsg& msg,usercmd_t& command,
                                  const usercmd_t* base) {
    ReadUserCmdDelta(msg,command,base);
  }
  void ValidationSerializePlayer(idSerializer& serializer,gameReturnPlayer_t& player) {
    SerializePlayer(serializer,player);
  }
#endif
  idLogFileWriter logFileWriter;
  idWarningList warningList;
  idRefreshOnPrint refreshOnPrint;
  std::byte opaqueTail[375504-352];
};

extern idCommonLocal commonLocal;

static_assert(sizeof(idPrintListener)==24,"idPrintListener ABI drift");
static_assert(sizeof(idUniqueWarning)==48,"idUniqueWarning ABI drift");
static_assert(sizeof(idCommonLocal::idLogFileWriter)==48,"idLogFileWriter ABI drift");
static_assert(sizeof(idCommonLocal::idWarningList)==256,"idWarningList ABI drift");
static_assert(sizeof(idCommonLocal::idRefreshOnPrint)==40,"idRefreshOnPrint ABI drift");
static_assert(offsetof(idCommonLocal,logFileWriter)==8,"idCommonLocal::logFileWriter ABI drift");
static_assert(offsetof(idCommonLocal,warningList)==56,"idCommonLocal::warningList ABI drift");
static_assert(offsetof(idCommonLocal,refreshOnPrint)==312,"idCommonLocal::refreshOnPrint ABI drift");
static_assert(sizeof(idCommonLocal)==375504,"idCommonLocal ABI drift");
