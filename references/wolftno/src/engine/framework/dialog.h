#pragma once

#include "engine/guis/swf/swf_scriptobject.h"
#include "shared/idlib/containers/staticlist.h"
#include "shared/idlib/langdict.h"
#include "shared/idlib/text/str.h"

// Values used by the framework SWF helper.  The retail enum continues through
// GDM_MAX (0xAF); retaining the original type name and values preserves the ABI
// of the idCommon and idSWFHelper calls recovered here.
enum gameDialogMessages_t : int {
  GDM_INVALID = 0x00,
  GDM_SAVING = 0x26,
  GDM_OVERWRITE_SAVE = 0x27,
  GDM_REFRESHING = 0x2D,
  GDM_DELETE_SAVE = 0x2E,
  GDM_QUICK_SAVE = 0x35,
  GDM_LOADING_PROFILE = 0x3A,
  GDM_STORAGE_REQUIRED = 0x3B,
  GDM_INSUFFICENT_STORAGE_SPACE = 0x3C,
  GDM_QUIT_GAME = 0x42,
  GDM_MUST_SIGNIN = 0x46,
  GDM_ERROR_LOADING_SAVEGAME = 0x5A,
  GDM_ERROR_SAVING_SAVEGAME = 0x5B,
  GDM_CORRUPT_PROFILE = 0x84,
  GDM_CALCULATING_BENCHMARK = 0x7F,
  GDM_SWAP_DISKS_TO1 = 0x01,
  GDM_SWAP_DISKS_TO2 = 0x02,
  GDM_SWAP_DISKS_TO3 = 0x03,
  GDM_SWAP_DISKS_TO4 = 0x8A,
  GDM_STREAMING_INSTALL_CONTENT_UNAVAILABLE = 0x8B,
  GDM_ENUMERATING_DLC = 0x8D,
  GDM_NEW_CONTENT_GO_TO_STORE = 0x8E,
  GDM_SEASON_PASS_CONTENT_AVAILABLE = 0x8F,
  GDM_BOUGHT_CONTENT_NOT_FOUND = 0x90,
  GDM_DISCONNECTED = 0x78,
  GDM_OUT_OF_MEMORY = 0x83,
  GDM_MAX = 0xAF,
};

enum dialogType_t : int {
  DIALOG_INVALID = -1,
  DIALOG_ACCEPT = 0,
  DIALOG_CONTINUE = 1,
  DIALOG_SHOWSAVEINFO = 2,
  DIALOG_ACCEPT_CANCEL = 3,
  DIALOG_YES_NO = 4,
  DIALOG_CANCEL = 5,
  DIALOG_WAIT = 6,
  DIALOG_WAIT_BLACKOUT = 7,
  DIALOG_WAIT_CANCEL = 8,
  DIALOG_DYNAMIC = 9,
  DIALOG_QUICK_SAVE = 10,
  DIALOG_TIMER_ACCEPT_REVERT = 11,
  DIALOG_CRAWL_SAVE = 12,
  DIALOG_CONTINUE_LARGE = 13,
  DIALOG_BENCHMARK = 14,
};

class idDialogInfo {
public:
  idDialogInfo()
      : msg(GDM_INVALID),type(DIALOG_ACCEPT),acceptCB(nullptr),cancelCB(nullptr),
        altCBOne(nullptr),altCBTwo(nullptr),clear(false),waitClear(false),
        pause(false),leaveOnClear(false),waitOnAtlas(false),
        renderDuringLoad(false),startTime(0),killTime(0),prio(0),overrideMsg(),
        txt1(),txt2(),txt3(),txt4() {}

  gameDialogMessages_t msg;
  dialogType_t type;
  idSWFScriptFunction* acceptCB;
  idSWFScriptFunction* cancelCB;
  idSWFScriptFunction* altCBOne;
  idSWFScriptFunction* altCBTwo;
  bool clear;
  bool waitClear;
  bool pause;
  bool forcePause;
  bool leaveOnClear;
  bool waitOnAtlas;
  bool renderDuringLoad;
  unsigned char padding;
  int startTime;
  int killTime;
  int prio;
  unsigned int alignment;
  idStrStatic<256> overrideMsg;
  idStrId txt1;
  idStrId txt2;
  idStrId txt3;
  idStrId txt4;
};

static_assert(sizeof(gameDialogMessages_t) == 4,
              "game-dialog message ABI drift");
static_assert(sizeof(dialogType_t)==4,"dialog type ABI drift");
static_assert(sizeof(idDialogInfo)==376,"dialog info ABI drift");
static_assert(offsetof(idDialogInfo,overrideMsg)==64,
              "dialog override offset drift");

class idCVar;
extern idCVar popupDialog_debug;
extern idCVar dialog_saveClearLevel1;
extern idCVar dialog_saveClearLevel2;
extern const char* dialogStateToString[];
