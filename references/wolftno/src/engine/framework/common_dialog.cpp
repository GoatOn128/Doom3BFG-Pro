#include "engine/framework/common_local.h"

#include "engine/framework/keyinput.h"
#include "engine/framework/rendermanager.h"
#include "engine/framework/swfhelper.h"
#include "engine/guis/swf/swf.h"
#include "engine/models/rendermodelgui.h"
#include "engine/renderer/renderersystem.h"
#include "engine/sys/sys_public.h"
#include "shared/idlib/csystems/cvarsystemlocal.h"
#include "shared/idlib/lib.h"
#include "shared/idlib/sys/sys_time.h"

#include <new>

idCVar popupDialog_debug("popupDialog_debug","0",CVAR_BOOL|CVAR_ARCHIVE,
                         "display debug spam");
idCVar dialog_saveClearLevel1("dialog_saveClearLevel1","1000",CVAR_INTEGER,
                              "Time required to show short message");
idCVar dialog_saveClearLevel2("dialog_saveClearLevel2","3000",CVAR_INTEGER,
                              "Time required to show long message");

namespace {
struct dialogText_t { int msg; const char* text; bool pcPrefix; };
constexpr dialogText_t dialogText[] = {
  {1,"switch_disc_to_1",true},{2,"switch_disc_to_2",true},
  {3,"switch_disc_to_3",true},{4,"signin_request",true},
  {5,"online_signin_request",true},{6,"online_signin_request_leaderboards",true},
  {7,"storage_device_selection_request",true},{8,"incorrect_online_permissions",true},
  {9,"#str_dlg_quit_progress_lost",false},{10,"#str_dlg_restart_progress_lost",false},
  {11,"#str_dlg_signin_changed",false},{12,"game_server_unavailable",true},
  {13,"#str_dlg_opponent_connection_lost_ranking_not_counted",false},
  {14,"online_connection_lost_main_menu_return",true},
  {15,"#str_dlg_opponent_connection_lost",false},{16,"#str_dlg_host_connection_lost",false},
  {17,"#str_dlg_host_connection_lost_ranking_not_counted",false},
  {18,"#str_dlg_ranking_load_failed",false},{19,"#str_dlg_host_quit",false},
  {20,"#str_dlg_became_host_party",false},{21,"#str_dlg_new_host_party",false},
  {22,"#str_dlg_lobby_became_host_game",false},{23,"lobby_new_host_game",true},
  {24,"#str_dlg_new_host_game",false},{25,"#str_dlg_new_host_game_stats_dropped",false},
  {26,"became_host_game",true},{27,"#str_dlg_became_host_game_stats_dropped",false},
  {28,"lobby_disbanded",true},{29,"#str_dlg_leave_with_party",false},
  {30,"#str_dlg_leave_lobby_ret_main",false},{31,"leave_lobby_ret_new_party",true},
  {32,"#str_online_host_migration",false},{33,"#str_dlg_opponent_left",false},
  {34,"#str_dlg_matches_not_found",false},{35,"#str_dlg_invalid_game",false},
  {36,"#str_dlg_kicked",false},{37,"#str_dlg_banned",false},{38,"saving",true},
  {39,"#str_dlg_overwrite_save",false},{40,"load_request",true},
  {41,"storage_removed_autosave_disabled",true},{42,"storage_not_available",true},
  {44,"#str_dlg_connecting",false},{45,"#str_dlg_refreshing",false},
  {46,"delete_save",true},{47,"deleting",true},{48,"bind_exists",true},
  {49,"cannont_bind",true},{50,"overlay_disabled",true},
  {51,"#str_dlg_direct_map_change",false},{52,"delete_autosave",true},
  {53,"saving",true},{54,"#str_online_confirm_retry",false},
  {55,"#str_online_confirm_suicide",false},{56,"#str_online_confirm_quit_generic",false},
  {57,"#str_online_confirm_coop_quit_game_generic",false},
  {58,"#str_dlg_loading_profile",false},{59,"storage_required",true},
  {60,"#str_dlg_insufficient_space",false},{61,"#str_dlg_partner_left",false},
  {62,"#str_dlg_restore_corrupt_savegame",false},{63,"#str_dlg_unrecoverable_savegame",false},
  {64,"profile_save_error",true},{65,"#str_dlg_lobby_full",false},
  {66,"#str_dlg_confirm_quit",false},{67,"#str_online_connection_problems",false},
  {68,"voice_restricted",true},{69,"#str_dlg_corrupt_save_file",false},
  {70,"must_signin",true},{71,"online_connection_lost_no_leaderboard",true},
  {72,"signin_changed_post",true},{73,"#str_online_host_migration_waiting",false},
  {74,"#str_online_host_migration_relaunching",false},
  {75,"#str_online_host_migration_failed",false},
  {76,"#str_online_host_migration_failed_stats",false},
  {77,"#str_online_host_migration_failed_disbanded",false},
  {78,"#str_online_host_migration_failed_disbanded_stats",false},
  {79,"#str_online_host_migration_failed_partner_left",false},
  {80,"host_quit_to_lobby",true},{81,"host_quit_to_lobby_stats_dropped",true},
  {82,"#str_dlg_failed_join_local_session",false},
  {83,"#str_dlg_delete_corrupt_savegame",false},
  {84,"#str_dlg_leave_incomplete_instance",false},{85,"#str_dlg_bind_unbind",false},
  {86,"#str_dlg_bind_restore",false},{87,"new_host",true},
  {88,"#str_dlg_confirm_display_changes",false},
  {89,"unable_to_use_selected_storage_device",true},
  {90,"#str_dlg_error_loading_savegame",false},{91,"#str_dlg_error_saving_savegame",false},
  {92,"#str_dlg_confirm_discard",false},{93,"#str_online_leave_game_lobby_alt_02",false},
  {94,"#str_online_party_leave_game",false},
  {95,"#str_dlg_reconnect_controller",false},{96,"#str_dlg_reconnect_controller",false},
  {97,"#str_dlg_reconnect_controller",false},{98,"#str_dlg_reconnect_controller",false},
  {99,"#str_dlg_reconnect_controller",false},{100,"#str_dlg_reconnect_controller",false},
  {101,"#str_dlg_reconnect_controller",false},{102,"dlc_error_content_removed",true},
  {103,"dlc_error_content_corrupt",true},{104,"dlc_error_content_missing",true},
  {105,"#str_dlg_dlc_error_content_missing_generic",false},{106,"#str_dlg_disc_swap",false},
  {107,"#str_dlg_game_install_message",false},{108,"#str_dlg_no_savegames_available",false},
  {109,"online_join_error_two_profiles_one_box",true},
  {110,"#str_online_lotw_solo_warning_alt_05",false},
  {111,"#str_online_confirm_coop_quit_game",false},
  {112,"#str_dlg_corrupt_save_file_continue",false},
  {113,"#str_online_confirm_quit_game",false},{114,"#str_online_cr_custom_game_no_stats",false},
  {115,"#str_dlg_ps3_incorrect_online_permissions",false},
  {116,"#str_dlg_disc_swap_confirmation",false},{117,"error_loading_profile",true},
  {118,"#str_online_join_error_full",false},{119,"#str_dlg_360_new_device_selected",false},
  {120,"#str_online_connection_error_03",false},
  {121,"#str_dlg_newer_incompatible_savegame",false},
  {122,"#str_dlg_achievements_disabled_due_to_cheating",false},
  {123,"#str_dlg_pointer_size_mismatch",false},{124,"#str_swf_texture_restart",false},
  {125,"#str_swf_insufficient_cores",false},{127,"#str_swf_calc_benchmark",false},
  {128,"BENCHMARK SCORE = ",false},{129,"#str_swf_display_changes_failed",false},
  {130,"#str_swf_gpu_transcode_failed",false},{131,"#str_swf_failed_level_load",false},
  {132,"#str_dlg_corrupt_profile",false},
  {133,"#str_dlg_profile_too_out_of_date_development_only",false},
  {134,"#str_dlg_load_chapter_confirmation",false},
  {135,"#str_dlg_restart_checkpoint",false},{136,"#str_dlg_restart_map",false},
  {137,"#str_dlg_quit_progress_lost",false},{138,"switch_disc_to_4",true},
  {139,"#str_dlg_streaming_install_content_unavailable",false},
  {140,"#str_dlg_saving_icon_pc",false},{141,"#str_dlg_enumerating_dlc",false},
  {142,"#str_dlg_new_content_go_to_store",false},
  {143,"#str_dlg_season_pass_content_available",false},
  {144,"#str_dlg_bought_content_not_found",false},{145,"#str_dlg_not_online_no_dlc",false},
  {146,"installing_gamedata",true},{147,"#str_dlg_delete_newgame",false},
  {148,"#str_dlg_other_user_savegame",false},{149,"#str_dlg_error_content_not_allowed",false},
  {150,"#str_dlg_confirm_exit_bonusmode",false},{151,"#str_dlg_saving_profile",false},
  {152,"#str_dlg_profile_device_unavailable",false},{153,"#str_dlg_delete_chapterselect",false},
  {154,"connection_lost_to_online_service",true},
  {155,"#str_dlg_bonusmode_savegame_mismatch",false},
  {156,"#str_dlg_xbone_unsupportedlanguage",false},
  {157,"#str_dlg_xbone_need_online_service_but_offline",false},
  {158,"not_online_patch_available",true},{159,"not_online_patch_downloaded",true},
  {160,"#str_dlg_quit_progress_lost_ironman",false},
  {161,"#str_dlg_quit_progress_lost_ironman",false},
  {162,"#str_dlg_other_user_profile",false},{163,"storage_warning_insufficient",true},
  {164,"online_signin_request_leaderboards",true},
  {165,"#str_dlg_streaming_install_disc_content_unavailable",false},
  {166,"data_will_not_be_saved",true},{167,"autosave_data_not_available",true},
  {168,"#str_dlg_later_save_version",false},
  {169,"#str_dlg_xbone_sign_in_state_changed_while_ingame",false},
  {170,"#str_dlg_xbone_profile_change_request",false},{171,"deleting",true},
  {172,"GDM_CREATING_SAVE_DATA should not be used on this platform",false},
  {173,"signin_loading",true},
};

bool IsTimedWaitMessage(gameDialogMessages_t msg) {
  return msg==GDM_SAVING || msg==GDM_REFRESHING || msg==GDM_QUICK_SAVE ||
         msg==GDM_LOADING_PROFILE;
}
}

idStr idCommonLocal::GetDialogMsg(gameDialogMessages_t msg) {
  for (const dialogText_t& entry:dialogText) {
    if (entry.msg==static_cast<int>(msg)) {
      if (!entry.pcPrefix)
        return idStr(entry.text);
      idStr result("#str_dlg_pc_");
      result.Append(entry.text);
      return result;
    }
  }
  return idStr("MESSAGE TYPE NOT DEFINED");
}

void idCommonLocal::RemoveWaitDialogs() {
  bool topMessageCleared=false;
  for (int index=0;index<MessageList().Num();++index) {
    idDialogInfo& info=MessageList()[index];
    if (IsTimedWaitMessage(info.msg) &&
        static_cast<int>(Sys_Milliseconds())>=info.killTime && info.waitClear) {
      info.clear=true;
      info.waitClear=false;
      if (index==0)
        topMessageCleared=true;
    }
  }
  if (topMessageCleared && MessageList().Num()>0 && Dialog()!=nullptr) {
    Dialog()->Deactivate();
    if (DialogWingstickActive()) {
      DialogWingstickActive()=false;
      renderSystem->EndAutomaticBackgroundSwaps();
    }
  }
}

bool idCommonLocal::HasDialogMsg(gameDialogMessages_t msg,bool* isNowActive) {
  for (int index=0;index<MessageList().Num();++index) {
    const idDialogInfo& info=MessageList()[index];
    if (info.msg==msg && !info.clear) {
      if (isNowActive!=nullptr)
        *isNowActive=index==0;
      return true;
    }
  }
  if (isNowActive!=nullptr)
    *isNowActive=false;
  return false;
}

void idCommonLocal::ClearDialog(gameDialogMessages_t msg,const char* location,
                                int lineNumber) {
  for (int index=0;index<MessageList().Num();++index) {
    idDialogInfo& info=MessageList()[index];
    if (info.msg!=msg || info.clear)
      continue;
    bool topMessageCleared=false;
    if (!IsTimedWaitMessage(info.msg)) {
      info.clear=true;
      topMessageCleared=index==0;
    } else if (!info.waitClear) {
      const int elapsed=static_cast<int>(Sys_Milliseconds())-info.startTime;
      if (elapsed>=dialog_saveClearLevel2.valueInteger) {
        info.clear=true;
        topMessageCleared=index==0;
      } else {
        info.killTime=dialog_saveClearLevel2.valueInteger-elapsed+
                      static_cast<int>(Sys_Milliseconds());
        info.waitClear=true;
      }
    } else {
      continue;
    }
    if (location==nullptr)
      location="NULL";
    idLib::PrintfIf(popupDialog_debug.valueInteger!=0,
      "[%s] msg: %s, from: %s:%d, topMessageCleared = %d, m.clear = %d, m.waitClear = %d, m.killTime = %d\n",
      "idCommonLocal::ClearDialog",dialogStateToString[info.msg],location,
      lineNumber,topMessageCleared,info.clear,info.waitClear,info.killTime);
    if (topMessageCleared && MessageList().Num()>0 && Dialog()!=nullptr) {
      Dialog()->Deactivate();
      if (DialogWingstickActive()) {
        DialogWingstickActive()=false;
        renderSystem->EndAutomaticBackgroundSwaps();
      }
    }
    return;
  }
}

void idCommonLocal::ReleaseCallBacks(int index) {
  if (index>=MessageList().Num())
    return;
  idDialogInfo& info=MessageList()[index];
  idSWFScriptFunction** callbacks[]={&info.acceptCB,&info.cancelCB,
                                     &info.altCBOne,&info.altCBTwo};
  for (idSWFScriptFunction** callback:callbacks) {
    if (*callback!=nullptr) {
      (*callback)->Release();
      *callback=nullptr;
    }
  }
}

bool idCommonLocal::HandleDialogEvent(const sysEvent_t* event) {
  if (Dialog()==nullptr || Dialog()->frameRate==0 || !Dialog()->isActive)
    return false;
  if (Dialog()->HandleEvent(event)) {
    idKeyInput::ClearStates();
    sys->ClearEvents();
  }
  return true;
}

bool idCommonLocal::IsDialogActive() {
  return Dialog()!=nullptr && Dialog()->isActive;
}

void idCommonLocal::AddDialogIntVal(const char* name,int value) {
  if (Dialog()!=nullptr)
    Dialog()->SetGlobal(name,idSWFScriptVar(value));
}

void idCommonLocal::ClearDialogs(bool forceClear) {
  bool removedTop=false;
  for (int index=0;index<MessageList().Num();++index) {
    if (!MessageList()[index].leaveOnClear || forceClear) {
      ReleaseCallBacks(index);
      MessageList().RemoveIndex(index);
      if (index==0)
        removedTop=true;
      --index;
    }
  }
  if (removedTop && Dialog()!=nullptr) {
    Dialog()->Deactivate();
    if (DialogWingstickActive()) {
      DialogWingstickActive()=false;
      renderSystem->EndAutomaticBackgroundSwaps();
    }
  }
}

void idCommonLocal::AddDynamicDialog(
    gameDialogMessages_t msg,const idStaticList<idSWFScriptFunction*,4>& callbacks,
    const idStaticList<idStrId,4>& optionText,bool pause,idStrStatic<256> overrideMsg,
    bool leaveOnMapHeapReset,bool waitOnAtlas,bool renderDuringLoad) {
  AddDialogWithTextOverride(msg,DIALOG_DYNAMIC,callbacks,optionText,pause,
                            overrideMsg,leaveOnMapHeapReset,waitOnAtlas,
                            renderDuringLoad);
}

void idCommonLocal::ShowNextDialog(bool waitForAtlas) {
  for (int index=0;index<MessageList().Num();++index) {
    if (!MessageList()[index].clear) {
      idDialogInfo info=MessageList()[index];
      ShowDialog(info,waitForAtlas || info.waitOnAtlas);
      return;
    }
  }
}

void idCommonLocal::AddDialogInternal(idDialogInfo& info) {
  if (HasDialogMsg(info.msg,nullptr))
    return;
  if (info.msg==GDM_STORAGE_REQUIRED) {
    for (int msg:{46,52,69})
      if (HasDialogMsg(static_cast<gameDialogMessages_t>(msg),nullptr))
        ClearDialog(static_cast<gameDialogMessages_t>(msg),nullptr,0);
  }
  idSWFScriptFunction* callbacks[]={info.acceptCB,info.cancelCB,
                                    info.altCBOne,info.altCBTwo};
  for (idSWFScriptFunction* callback:callbacks)
    if (callback!=nullptr)
      callback->AddRef();
  if (MessageList().Num()==0) {
    MessageList().Append(info);
  } else {
    if (MessageList().Num()>=4)
      MessageList().RemoveIndex(3);
    if (MessageList().Num()>0) {
      idLib::PrintfIf(popupDialog_debug.valueInteger!=0,
        "[%s] msg: %s new dialog added over old\n",
        "idCommonLocal::AddDialogInternal",dialogStateToString[info.msg]);
      const idDialogInfo& top=MessageList()[0];
      if (top.type==DIALOG_QUICK_SAVE || top.type==DIALOG_CRAWL_SAVE ||
          top.msg==GDM_CALCULATING_BENCHMARK) {
        DialogWingstickActive()=false;
        renderSystem->EndAutomaticBackgroundSwaps();
      }
    }
    if (info.msg==GDM_ENUMERATING_DLC) {
      MessageList().Append(info);
    } else {
      Dialog()->Deactivate();
      int lastPriority=-1;
      int insertIndex=0;
      for (int index=0;index<MessageList().Num();++index) {
        if (MessageList()[index].prio<=0)
          insertIndex=index;
        else
          lastPriority=index;
      }
      if (lastPriority==insertIndex)
        insertIndex=MessageList().Num();
      if (lastPriority==-1)
        insertIndex=0;
      MessageList().Insert(info,insertIndex);
    }
  }
  if (MessageList().Num()<=0)
    return;
  if (info.type==DIALOG_QUICK_SAVE || info.type==DIALOG_CRAWL_SAVE ||
      MessageList()[0].msg==GDM_CALCULATING_BENCHMARK)
    ShowNextDialog(false);
  if (!Dialog()->isActive && info.waitOnAtlas)
    ShowNextDialog(false);
}

void idCommonLocal::AddDialog(gameDialogMessages_t msg,dialogType_t type,
    idSWFScriptFunction* acceptCallback,idSWFScriptFunction* cancelCallback,
    bool pause,const char* location,int lineNumber,bool leaveOnMapHeapReset,
    bool waitOnAtlas,bool renderDuringLoad,int prio) {
  idKeyInput::ClearStates();
  sys->ClearEvents();
  if (common->GetRecordState()!=RECORD_NONE)
    return;
  if (location==nullptr)
    location="NULL";
  idLib::PrintfIf(popupDialog_debug.valueInteger!=0,
    "[%s] msg: %s, pause: %d from: %s:%d\n",
    "idCommonLocal::AddDialog",dialogStateToString[msg],pause,location,lineNumber);
  if (Dialog()==nullptr)
    return;
  idDialogInfo info;
  info.msg=msg;
  info.type=type;
  info.acceptCB=acceptCallback;
  info.cancelCB=cancelCallback;
  info.clear=false;
  info.prio=prio;
  info.pause=pause;
  info.startTime=static_cast<int>(Sys_Milliseconds());
  info.killTime=0;
  info.leaveOnClear=leaveOnMapHeapReset;
  info.waitOnAtlas=waitOnAtlas;
  info.renderDuringLoad=renderDuringLoad;
  AddDialogInternal(info);
}

void idCommonLocal::AddDialogWithTextOverride(
    gameDialogMessages_t msg,dialogType_t type,
    const idStaticList<idSWFScriptFunction*,4>& callbacks,
    const idStaticList<idStrId,4>& optionText,bool pause,
    idStrStatic<256> overrideMsg,bool leaveOnMapHeapReset,bool waitOnAtlas,
    bool renderDuringLoad) {
  if (Dialog()==nullptr)
    return;
  idDialogInfo info;
  info.msg=msg;
  info.overrideMsg=overrideMsg;
  info.type=type;
  info.pause=pause;
  info.leaveOnClear=leaveOnMapHeapReset;
  info.acceptCB=callbacks.Num()>0?callbacks[0]:nullptr;
  info.cancelCB=callbacks.Num()>1?callbacks[1]:nullptr;
  info.altCBOne=callbacks.Num()>2?callbacks[2]:nullptr;
  info.altCBTwo=callbacks.Num()>3?callbacks[3]:nullptr;
  info.txt1=optionText.Num()>0?optionText[0]:idStrId();
  info.txt2=optionText.Num()>1?optionText[1]:idStrId();
  info.txt3=optionText.Num()>2?optionText[2]:idStrId();
  info.txt4=optionText.Num()>3?optionText[3]:idStrId();
  info.waitOnAtlas=waitOnAtlas;
  info.renderDuringLoad=renderDuringLoad;
  info.clear=false;
  info.startTime=static_cast<int>(Sys_Milliseconds());
  info.killTime=0;
  AddDialogInternal(info);
}

void idCommonLocal::ShowDialog(idDialogInfo& info,bool waitOnAtlas) {
  idLib::PrintfIf(popupDialog_debug.valueInteger!=0,
    "[%s] msg: %s, m.clear = %d, m.waitClear = %d, m.killTime = %d\n",
    "idCommonLocal::ShowDialog",dialogStateToString[info.msg],info.clear,
    info.waitClear,info.killTime);
  if (IsDialogActive()) {
    Dialog()->Deactivate();
    if (DialogWingstickActive()) {
      DialogWingstickActive()=false;
      renderSystem->EndAutomaticBackgroundSwaps();
    }
  }

  idStr message=GetDialogMsg(info.msg);
  Dialog()->SetGlobal("showReplacements",idSWFScriptVar(false));
  if (info.msg==GDM_QUIT_GAME) {
    info.type=DIALOG_DYNAMIC;
    const int index=static_cast<int>(
        ((1103515245u*static_cast<unsigned int>(Sys_Milliseconds())+12345u)>>16)&3u);
    info.txt1.Set((&Field<idStr>(0x5BA00))[index].data);
    info.txt2.Set((&Field<idStr>(0x5B938))[index].data);
    Dialog()->SetGlobal("showReplacements",idSWFScriptVar(true));
    info.cancelCB=SwfHelper()->GetContinue(info.msg);
  }

  Dialog()->SetGlobal("messageInfo",
      idSWFScriptVar(info.overrideMsg.IsEmpty()?message:static_cast<const idStr&>(info.overrideMsg)));
  Dialog()->SetGlobal("Infotype",idSWFScriptVar(static_cast<int>(info.type)));

  idSWFScriptFunction* accept=info.acceptCB;
  if (accept==nullptr && info.type!=DIALOG_WAIT && info.type!=DIALOG_WAIT_BLACKOUT)
    accept=SwfHelper()->GetContinue(info.msg);
  Dialog()->SetGlobal("acceptCallBack",idSWFScriptVar(accept));

  idSWFScriptFunction* cancel=info.cancelCB;
  if (cancel==nullptr && (info.type==DIALOG_YES_NO || info.type==DIALOG_ACCEPT_CANCEL))
    cancel=SwfHelper()->GetContinue(info.msg);
  Dialog()->SetGlobal("cancelCallBack",idSWFScriptVar(cancel));
  Dialog()->SetGlobal("altCBOne",idSWFScriptVar(info.altCBOne));
  Dialog()->SetGlobal("altCBTwo",idSWFScriptVar(info.altCBTwo));

  idSWFScriptVar text;
  text.SetString(info.txt1); Dialog()->SetGlobal("opt1Txt",text);
  text.SetString(info.txt2); Dialog()->SetGlobal("opt2Txt",text);
  text.SetString(info.txt3); Dialog()->SetGlobal("opt3Txt",text);
  text.SetString(info.txt4); Dialog()->SetGlobal("opt4Txt",text);

  const int msg=static_cast<int>(info.msg);
  if ((msg>=1 && msg<=3) || info.msg==GDM_SWAP_DISKS_TO4)
    PreventOverlayPurge()=true;

  const bool automatic=info.type==DIALOG_QUICK_SAVE ||
                       info.type==DIALOG_CRAWL_SAVE ||
                       info.msg==GDM_CALCULATING_BENCHMARK;
  if (automatic) {
    Dialog()->LoadAtlasImage(true);
    if (Dialog()!=nullptr)
      Dialog()->Activate(static_cast<int>(Sys_Milliseconds()));
    if (DialogGuiModel()!=nullptr) {
      DialogGuiModel()->Clear(0x28);
      const int height=renderSystem->GetHeight();
      const int width=renderSystem->GetWidth();
      DialogGuiModel()->SetViewport(0,0,width,height);
      const float virtualWidth=static_cast<float>(renderSystem->GetWidth());
      const int virtualHeight=renderSystem->GetHeight();
      const float pixelAspect=renderSystem->GetPixelAspect();
      DialogGuiModel()->SetVirtualSize(
          static_cast<int>(pixelAspect*virtualWidth),virtualHeight);
      Dialog()->Render(DialogGuiModel(),static_cast<int>(Sys_Milliseconds()),false);
      renderManager.BuildGuiFrame(DialogGuiModel(),false);
      renderManager.RenderFrameAndBeginAutomaticBackgroundSwaps(false,true);
      DialogWingstickActive()=true;
    }
  } else {
    if (waitOnAtlas)
      Dialog()->LoadAtlasImage(true);
    if (Dialog()!=nullptr)
      Dialog()->Activate(static_cast<int>(Sys_Milliseconds()));
  }
}

void idCommonLocal::KillDialog() {
  idLib::PrintfIf(popupDialog_debug.valueInteger!=0,"[%s]\n",
                  "idCommonLocal::KillDialog");
  ClearDialogs(false);
  renderManager.ClearAllGuiModels();
  if (Dialog()!=nullptr) {
    idSWF* old=Dialog();
    old->~idSWF();
    idMem::Singleton()->Free(old,ALIGN_16);
    Dialog()=nullptr;
  }
  if (DialogGuiModel()!=nullptr) {
    idRenderModelGui* old=DialogGuiModel();
    old->~idRenderModelGui();
    idMem::Singleton()->Free(old,ALIGN_16);
    DialogGuiModel()=nullptr;
  }
}

void idCommonLocal::InitDialog() {
  idLib::PrintfIf(popupDialog_debug.valueInteger!=0,"[%s]\n",
                  "idCommonLocal::InitDialog");
  KillDialog();
  void* swfMemory=idMem::Singleton()->AllocWithLocation(
      "w:\\build\\tech5\\shared\\idlib\\Heap.h(95) : tag",sizeof(idSWF),
      TAG_SWF,false,ALIGN_16,HEAP_DEFAULTHEAP);
  Dialog()=swfMemory!=nullptr?::new(swfMemory) idSWF("tungdialog",nullptr,true):nullptr;
  void* guiMemory=idMem::Singleton()->AllocWithLocation(
      "w:\\build\\tech5\\shared\\idlib\\Heap.h(95) : tag",sizeof(idRenderModelGui),
      TAG_GUI_MODEL,false,ALIGN_16,HEAP_DEFAULTHEAP);
  DialogGuiModel()=guiMemory!=nullptr?::new(guiMemory) idRenderModelGui():nullptr;
  DialogGuiModel()->SetName("dialogGuiModel");
  if (Dialog()==nullptr)
    return;
  constexpr const char* names[]={
    "DIALOG_ACCEPT","DIALOG_CONTINUE","DIALOG_SHOWSAVEINFO",
    "DIALOG_ACCEPT_CANCEL","DIALOG_YES_NO","DIALOG_CANCEL","DIALOG_WAIT",
    "DIALOG_WAIT_BLACKOUT","DIALOG_WAIT_CANCEL","DIALOG_DYNAMIC",
    "DIALOG_QUICK_SAVE","DIALOG_TIMER_ACCEPT_REVERT","DIALOG_CRAWL_SAVE",
    "DIALOG_CONTINUE_LARGE","DIALOG_BENCHMARK"};
  for (int index=0;index<15;++index)
    Dialog()->SetGlobal(names[index],idSWFScriptVar(index));
}

void idCommonLocal::RenderDialog(bool loading) {
  DialogPause()=false;
  if (Dialog()==nullptr || DialogGuiModel()==nullptr)
    return;
  RemoveWaitDialogs();
  bool pause=false;
  for (int index=0;index<MessageList().Num();++index) {
    if (MessageList()[index].clear) {
      idLib::PrintfIf(popupDialog_debug.valueInteger!=0,"[%s] removing %s\n",
                      "idCommonLocal::RenderDialog",
                      dialogStateToString[MessageList()[index].msg]);
      ReleaseCallBacks(index);
      MessageList().RemoveIndex(index--);
    } else if (MessageList()[index].pause) {
      pause=true;
    }
  }
  DialogPause()=pause;
  if (MessageList().Num()>0 && !Dialog()->isActive)
    ShowNextDialog(false);
  if (MessageList().Num()==0 && Dialog()->isActive)
    Dialog()->Deactivate();

  if (MessageList().Num()>0 && MessageList()[0].type==DIALOG_TIMER_ACCEPT_REVERT) {
    const int seconds=(MessageList()[0].startTime+20000-
                       static_cast<int>(Sys_Milliseconds()))/1000;
    if (seconds>0) {
      idStrId timeText("#str_menu_revert_time");
      Dialog()->SetGlobal("countdownInfo",
          idSWFScriptVar(va(timeText.GetLocalizedString(),seconds)));
    } else {
      if (MessageList()[0].cancelCB!=nullptr) {
        idSWFParmList parms;
        MessageList()[0].cancelCB->Call(nullptr,parms);
      }
      MessageList()[0].clear=true;
    }
  }

  if (MessageList().Num()>0 && loading && !MessageList()[0].renderDuringLoad)
    return;
  if (Dialog()->isActive) {
    DialogGuiModel()->Clear(0x28);
    const int height=renderSystem->GetHeight();
    const int width=renderSystem->GetWidth();
    DialogGuiModel()->SetViewport(0,0,width,height);
    const float virtualWidth=static_cast<float>(renderSystem->GetWidth());
    const int virtualHeight=renderSystem->GetHeight();
    const float pixelAspect=renderSystem->GetPixelAspect();
    DialogGuiModel()->SetVirtualSize(
        static_cast<int>(pixelAspect*virtualWidth),virtualHeight);
    Dialog()->Render(DialogGuiModel(),static_cast<int>(Sys_Milliseconds()),false);
  } else if (DialogWingstickActive()) {
    DialogWingstickActive()=false;
    renderSystem->EndAutomaticBackgroundSwaps();
  }
}
