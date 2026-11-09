#include "engine/framework/keyinput.h"
#include "engine/framework/usercmdgen.h"

#include "engine/models/serializer.h"
#include "shared/idlib/csystems/autocomplete.h"
#include "shared/idlib/csystems/cmdsystemlocal.h"
#include "shared/idlib/csystems/cvarsystemlocal.h"
#include "shared/idlib/filesystem/file.h"
#include "shared/idlib/langdict.h"
#include "shared/idlib/lib.h"

#include <algorithm>
#include <cctype>
#include <cstring>
#include <cwctype>
#include <vector>
#ifdef _WIN32
#include <Windows.h>
#endif

#ifndef WOLF_KEYINPUT_VALIDATION
extern idCVar in_bindset;
extern idCVar key_deviceBindOverride;
#endif
static void SplitStrByStr(const char*,const char*,idList<idStr,5>&);

#define KN(value, text) { value, #text, "#str_key_" #text }
#define KNA(value, text, id) { value, text, "#str_key_" id }
keyname_t keynames[] = {
  KN(K_ESCAPE,ESCAPE), KN(K_1,1), KN(K_2,2), KN(K_3,3), KN(K_4,4),
  KN(K_5,5), KN(K_6,6), KN(K_7,7), KN(K_8,8), KN(K_9,9), KN(K_0,0),
  KN(K_MINUS,MINUS), KN(K_EQUALS,EQUALS), KN(K_BACKSPACE,BACKSPACE),
  KN(K_TAB,TAB), KN(K_Q,Q), KN(K_W,W), KN(K_E,E), KN(K_R,R), KN(K_T,T),
  KN(K_Y,Y), KN(K_U,U), KN(K_I,I), KN(K_O,O), KN(K_P,P),
  KN(K_LBRACKET,LBRACKET), KN(K_RBRACKET,RBRACKET), KN(K_ENTER,ENTER),
  KN(K_LCTRL,LCTRL), KN(K_A,A), KN(K_S,S), KN(K_D,D), KN(K_F,F), KN(K_G,G),
  KN(K_H,H), KN(K_J,J), KN(K_K,K), KN(K_L,L), KN(K_SEMICOLON,SEMICOLON),
  KN(K_APOSTROPHE,APOSTROPHE), KN(K_GRAVE,GRAVE), KN(K_LSHIFT,LSHIFT),
  KN(K_BACKSLASH,BACKSLASH), KN(K_Z,Z), KN(K_X,X), KN(K_C,C), KN(K_V,V),
  KN(K_B,B), KN(K_N,N), KN(K_M,M), KN(K_COMMA,COMMA), KN(K_PERIOD,PERIOD),
  KN(K_SLASH,SLASH), KN(K_RSHIFT,RSHIFT), KN(K_KP_STAR,KP_STAR),
  KN(K_LALT,LALT), KN(K_SPACE,SPACE), KN(K_CAPSLOCK,CAPSLOCK), KN(K_F1,F1),
  KN(K_F2,F2), KN(K_F3,F3), KN(K_F4,F4), KN(K_F5,F5), KN(K_F6,F6),
  KN(K_F7,F7), KN(K_F8,F8), KN(K_F9,F9), KN(K_F10,F10),
  KN(K_NUMLOCK,NUMLOCK), KN(K_SCROLL,SCROLL), KN(K_KP_7,KP_7),
  KN(K_KP_8,KP_8), KN(K_KP_9,KP_9), KN(K_KP_MINUS,KP_MINUS),
  KN(K_KP_4,KP_4), KN(K_KP_5,KP_5), KN(K_KP_6,KP_6),
  KN(K_KP_PLUS,KP_PLUS), KN(K_KP_1,KP_1), KN(K_KP_2,KP_2),
  KN(K_KP_3,KP_3), KN(K_KP_0,KP_0), KN(K_KP_DOT,KP_DOT), KN(K_F11,F11),
  KN(K_F12,F12), KN(K_F13,F13), KN(K_F14,F14), KN(K_F15,F15),
  KN(K_KANA,KANA), KN(K_CONVERT,CONVERT), KN(K_NOCONVERT,NOCONVERT),
  KN(K_YEN,YEN), KN(K_KP_EQUALS,KP_EQUALS), KN(K_CIRCUMFLEX,CIRCUMFLEX),
  KN(K_AT,AT), KN(K_COLON,COLON), KN(K_UNDERLINE,UNDERLINE), KN(K_KANJI,KANJI),
  KN(K_STOP,STOP), KN(K_AX,AX), KN(K_UNLABELED,UNLABELED),
  KN(K_KP_ENTER,KP_ENTER), KN(K_RCTRL,RCTRL), KN(K_KP_COMMA,KP_COMMA),
  KN(K_KP_SLASH,KP_SLASH), KN(K_PRINTSCREEN,PRINTSCREEN), KN(K_RALT,RALT),
  KN(K_PAUSE,PAUSE), KN(K_HOME,HOME), KN(K_UPARROW,UPARROW), KN(K_PGUP,PGUP),
  KN(K_LEFTARROW,LEFTARROW), KN(K_RIGHTARROW,RIGHTARROW), KN(K_END,END),
  KN(K_DOWNARROW,DOWNARROW), KN(K_PGDN,PGDN), KN(K_INS,INS), KN(K_DEL,DEL),
  KN(K_LWIN,LWIN), KN(K_RWIN,RWIN), KN(K_APPS,APPS), KN(K_POWER,POWER),
  KN(K_SLEEP,SLEEP), KN(K_MOUSE1,MOUSE1), KN(K_MOUSE2,MOUSE2),
  KN(K_MOUSE3,MOUSE3), KN(K_MOUSE4,MOUSE4), KN(K_MOUSE5,MOUSE5),
  KN(K_MOUSE6,MOUSE6), KN(K_MOUSE7,MOUSE7), KN(K_MOUSE8,MOUSE8),
  KN(K_MWHEELDOWN,MWHEELDOWN), KN(K_MWHEELUP,MWHEELUP), KN(K_JOY1,JOY1),
  KN(K_JOY2,JOY2), KN(K_JOY3,JOY3), KN(K_JOY4,JOY4), KN(K_JOY5,JOY5),
  KN(K_JOY6,JOY6), KN(K_JOY7,JOY7), KN(K_JOY8,JOY8), KN(K_JOY9,JOY9),
  KN(K_JOY10,JOY10), KN(K_JOY11,JOY11), KN(K_JOY12,JOY12), KN(K_JOY13,JOY13),
  KN(K_JOY14,JOY14), KN(K_JOY15,JOY15), KN(K_JOY16,JOY16),
  KN(K_JOY_DPAD_UP,JOY_DPAD_UP), KN(K_JOY_DPAD_DOWN,JOY_DPAD_DOWN),
  KN(K_JOY_DPAD_LEFT,JOY_DPAD_LEFT), KN(K_JOY_DPAD_RIGHT,JOY_DPAD_RIGHT),
  KN(K_JOY_STICK1_UP,JOY_STICK1_UP), KN(K_JOY_STICK1_DOWN,JOY_STICK1_DOWN),
  KN(K_JOY_STICK1_LEFT,JOY_STICK1_LEFT), KN(K_JOY_STICK1_RIGHT,JOY_STICK1_RIGHT),
  KN(K_JOY_STICK2_UP,JOY_STICK2_UP), KN(K_JOY_STICK2_DOWN,JOY_STICK2_DOWN),
  KN(K_JOY_STICK2_LEFT,JOY_STICK2_LEFT), KN(K_JOY_STICK2_RIGHT,JOY_STICK2_RIGHT),
  KN(K_JOY_TRIGGER1,JOY_TRIGGER1), KN(K_JOY_TRIGGER2,JOY_TRIGGER2),
  KNA(K_JOY1,"JOYA","JOY1"), KNA(K_JOY2,"JOYB","JOY2"),
  KNA(K_JOY3,"JOYX","JOY3"), KNA(K_JOY4,"JOYY","JOY4"),
  KNA(K_LALT,"ALT","LALT"), KNA(K_LCTRL,"CTRL","LCTRL"),
  KNA(K_LSHIFT,"SHIFT","LSHIFT"), KNA(K_KP_7,"KP_HOME","KP_7"),
  KNA(K_KP_8,"KP_UPARROW","KP_8"), KNA(K_KP_9,"KP_PGUP","KP_9"),
  KNA(K_KP_4,"KP_LEFTARROW","KP_4"), KNA(K_KP_6,"KP_RIGHTARROW","KP_6"),
  KNA(K_KP_1,"KP_END","KP_1"), KNA(K_KP_2,"KP_DOWNARROW","KP_2"),
  KNA(K_KP_3,"KP_PGDN","KP_3"), KNA(K_KP_0,"KP_INS","KP_0"),
  KNA(K_KP_DOT,"KP_DEL","KP_DOT"), KNA(K_NUMLOCK,"KP_NUMLOCK","NUMLOCK"),
  KNA(K_MINUS,"-","MINUS"), KNA(K_EQUALS,"=","EQUALS"),
  KNA(K_LBRACKET,"[","LBRACKET"), KNA(K_RBRACKET,"]","RBRACKET"),
  KNA(K_BACKSLASH,"\\","BACKSLASH"), KNA(K_SLASH,"/","SLASH"),
  KNA(K_COMMA,",","COMMA"), KNA(K_PERIOD,".","PERIOD"),
  { K_NONE, nullptr, nullptr }
};
#undef KN
#undef KNA

#define UC(value, text) { text, static_cast<cmdGenButton_t>(value) }
userCmdString_t userCmdStrings[] = {
  UC(29,"_jump"), UC(30,"_crouch"), UC(48,"_moveleft"), UC(49,"_moveright"),
  UC(50,"_moveforward"), UC(51,"_moveback"), UC(52,"_lookup"), UC(53,"_lookdown"),
  UC(54,"_lookleft"), UC(55,"_lookright"), UC(4,"_sprint"), UC(0,"_attack1"),
  UC(5,"_useandreload"), UC(1,"_use"), UC(2,"_zoom"), UC(3,"_melee"),
  UC(6,"_changeWeapon"), UC(18,"_quickgrenadeandweaponsel"),
  UC(19,"_toggledwandweaponsel"), UC(7,"_reload"), UC(8,"_weap0"),
  UC(9,"_weap1"), UC(10,"_weap2"), UC(11,"_weap3"), UC(12,"_weap4"),
  UC(13,"_weap5"), UC(14,"_weap6"), UC(15,"_weap7"), UC(16,"_weapnextinv"),
  UC(17,"_weapprevinv"), UC(20,"_toggledualwield"), UC(21,"_dualwield"),
  UC(22,"_singlewield"), UC(23,"_covermode"), UC(24,"_inventory"),
  UC(25,"_togglefiremodeleft"), UC(26,"_togglefiremoderight"),
  UC(27,"_quickgrenade"), UC(28,"_map"), UC(31,"_prevequipweapon"),
  UC(4,"_speed"), UC(29,"_moveup"), UC(30,"_movedown"), UC(50,"_forward"),
  UC(51,"_back"), UC(54,"_left"), UC(55,"_right"), UC(56,"_menuaccept"),
  UC(56,"_menucancel"), UC(56,"_menualtaction"), UC(56,"_menutabright"),
  UC(56,"_menutableft"), UC(56,"_movestick"), UC(56,"_lookstick"),
  { nullptr, UB_NONE }
};
#undef UC

idHashMap<int> idKeyAction::keyBindingHash(TAG_HASHINDEX);
idHashIndex idKeyAction::keyActionsHash(1024,1024);
idList<idKeyAction*> idKeyAction::keyActionsList;

namespace {
bool keyDown[K_LAST_KEY]{};
std::int16_t userDeviceNum=0;

bool Same(const char* a,const char* b) { return idStr::Icmp(a?a:"",b?b:"")==0; }
cmdGenButton_t ActionForBinding(const char* binding) {
  if(binding==nullptr) return UB_NONE;
  for(auto* entry=userCmdStrings;entry->string!=nullptr;++entry)
    if(Same(entry->string,binding)) return entry->button;
  return UB_NONE;
}
}

static void AppendCommaSeparated(const char* text,idStr& output) {
  if(text==nullptr||*text==0) return;
  if(!output.IsEmpty()) output.Append(", ");
  output.Append(text);
}
static void KeyNumToKeyNames(keyNum_t keyNum,idStr& output,bool firstOnly) {
  for(auto* entry=keynames;entry->name!=nullptr;++entry) {
    if(entry->keynum==keyNum && keyNum>=K_JOY1 && keyNum<=K_JOY_DPAD_RIGHT) {
      output.Append("<"); output.Append(entry->name); output.Append(">");
      if(firstOnly) return;
    }
  }
}

namespace {
inputType_t TypeForKey(keyNum_t key) {
  if(key>=K_JOY1&&key<=K_JOY_DPAD_RIGHT) return INPUT_TYPE_GAMEPAD;
  if(key>=K_MOUSE1&&key<=K_MWHEELUP) return INPUT_TYPE_MOUSE;
  return INPUT_TYPE_KEYBOARD;
}
}

idKeyAction::idKeyAction(std::uint16_t keyHash)
  : nextAction(nullptr),binding(),usercmdAction(UB_NONE),hashKey(keyHash),
    hashBind(0),hasCompoundBind(false),modifierMask(0) {}

int idKeyAction::GetKeyActionIndex(int device,bindSet_t bindSet,keyNum_t keyNum) {
  const auto hashKey=GetKeyHash(device,bindSet,keyNum);
  for(int index=keyActionsHash.First(hashKey);index!=-1;index=keyActionsHash.Next(index))
    if(keyActionsList.list[index]&&keyActionsList.list[index]->hashKey==hashKey) return index;
  return -1;
}

idKeyAction* idKeyAction::GetKeyAction(int device,bindSet_t bindSet,keyNum_t keyNum,
                                       std::uint8_t mask) {
  const int index=GetKeyActionIndex(device,bindSet,keyNum);
  if(index<0) return nullptr;
  for(idKeyAction* action=keyActionsList.list[index];action;action=action->nextAction)
    if(action->modifierMask==mask) return action;
  return nullptr;
}

bool idKeyAction::GetKeyAndModifierFromString(const char* text,keyNum_t& keyNum,
                                              std::uint8_t& mask) {
  keyNum=idKeyInput::StringToKeyNum(text);
  if(keyNum!=K_NONE){mask=0;return true;}
  if(text==nullptr) return false;
  idList<idStr,5> keys;
  SplitStrByStr(text,"+",keys);
  for(int index=0;index<keys.num;++index)
    if(keys[index].IsEmpty()||idKeyInput::StringToKeyNum(keys[index].data)==K_NONE) return false;
  mask=0; bool foundKey=false;
  for(int index=0;index<keys.num;++index) {
    keyNum_t part=idKeyInput::StringToKeyNum(keys[index].data);
    if(part==K_NONE) return false;
    switch(part) {
      case K_LSHIFT: mask|=1; break; case K_RSHIFT: mask|=2; break;
      case K_LCTRL: mask|=4; break; case K_RCTRL: mask|=8; break;
      case K_LALT: mask|=0x10; break; case K_RALT: mask|=0x20; break;
      default: if(foundKey) return false; keyNum=part; foundKey=true; break;
    }
  }
  return foundKey;
}

void ArgCompletion_KeyName(idAutoComplete& completion) {
  for(auto* entry=keynames;entry->name!=nullptr;++entry) {
    idStr suggestion(entry->name); completion.Append(suggestion,-1);
  }
}
keyNum_t idKeyInput::StringToKeyNum(const char* string) {
  if(string==nullptr||*string==0) return K_NONE;
  for(auto* entry=keynames;entry->name!=nullptr;++entry)
    if(Same(string,entry->name)) return entry->keynum;
  return K_NONE;
}
const char* idKeyInput::KeyNumToString(keyNum_t keyNum) {
  for(auto* entry=keynames;entry->name!=nullptr;++entry) if(entry->keynum==keyNum) return entry->name;
  return "?";
}
idStr idKeyInput::GetUsercmdActionStr(int action) {
  for(auto* entry=userCmdStrings;entry->string!=nullptr;++entry)
    if(entry->button==action) return idStr(entry->string);
  return idStr();
}

const char* idKeyInput::LocalizedKeyName(keyNum_t keyNum) {
#ifdef _WIN32
  if(keyNum<256) {
    BYTE state[256]{}; wchar_t temp[12]{};
    const UINT vk=MapVirtualKeyA(static_cast<UINT>(keyNum),MAPVK_VSC_TO_VK_EX);
    if(ToUnicode(vk,static_cast<UINT>(keyNum),state,temp,5,0)>0&&temp[0]>L' '&&iswprint(temp[0])) {
      static idStr bindStr; bindStr.Empty(); bindStr.AppendUTF8Char(temp[0]); return bindStr.data;
    }
  }
#endif
  for(auto* entry=keynames;entry->name!=nullptr;++entry)
    if(entry->keynum==keyNum) return idLocalization::languageDict.GetString(entry->strId);
  return "????";
}

void idKeyInput::PreliminaryKeyEvent(int,keyNum_t keyNum,bool down) {
  keyDown[keyNum]=down;
}
bool idKeyInput::IsDown(int,keyNum_t keyNum) {
  return keyDown[keyNum];
}
bool idKeyInput::IsShiftDown(int device){return IsDown(device,K_LSHIFT)||IsDown(device,K_RSHIFT);}
bool idKeyInput::IsCtrlDown(int device){return IsDown(device,K_LCTRL)||IsDown(device,K_RCTRL);}
bool idKeyInput::IsAltDown(int device){return IsDown(device,K_LALT)||IsDown(device,K_RALT);}
void idKeyInput::ClearStates(){std::memset(keyDown,0,sizeof(keyDown));
#ifndef WOLF_KEYINPUT_VALIDATION
  usercmdGen->Clear();
#endif
}
bool idKeyInput::IsValidRebindKey(keyNum_t key){return key>K_NONE&&key<K_LAST_KEY;}
void idKeyInput::SetUserDeviceNumForBind(int device){userDeviceNum=static_cast<std::int16_t>(device);}

int idKeyInput::GetNumBindings(int device) {
  int count=0;
  for(int i=0;i<idKeyAction::keyActionsList.num;++i)
    for(auto* a=idKeyAction::keyActionsList.list[i];a;a=a->nextAction) if(a->GetDevice()==device) ++count;
  return count;
}
const char* idKeyInput::GetBinding(int device,bindSet_t bindSet,keyNum_t keyNum,std::uint8_t mask) {
  if(static_cast<unsigned>(bindSet)>BIND_VEHICLE_PASSENGER||static_cast<unsigned>(device)>3) return "";
  auto* action=idKeyAction::GetKeyAction(device,bindSet,keyNum,mask);
  return action?action->binding.data:"";
}
int idKeyInput::GetUsercmdAction(int device,bindSet_t bindSet,keyNum_t keyNum,std::uint8_t mask) {
  if(static_cast<unsigned>(bindSet)>BIND_VEHICLE_PASSENGER||static_cast<unsigned>(device)>3) return UB_NONE;
  auto* action=idKeyAction::GetKeyAction(device,bindSet,keyNum,mask);
  return action?action->usercmdAction:UB_NONE;
}

void idKeyInput::SetBinding(int device,bindSet_t bindSet,keyNum_t keyNum,
                            std::uint8_t mask,const char* text) {
  if(static_cast<unsigned>(bindSet)>BIND_VEHICLE_PASSENGER||static_cast<unsigned>(device)>3) return;
#ifndef WOLF_KEYINPUT_VALIDATION
  cvarSystem->SetModifiedFlags(0x10000);
#endif
  const int oldIndex=idKeyAction::GetKeyActionIndex(device,bindSet,keyNum);
  idKeyAction* action=idKeyAction::GetKeyAction(device,bindSet,keyNum,mask);
  if(text&&*text) {
    const int bindHash=idStr::Hash(text);
    int index=oldIndex;
    if(action==nullptr) {
      action=new idKeyAction(idKeyAction::GetKeyHash(device,bindSet,keyNum));
      if(index<0) {
        index=-1;
        for(int i=0;i<idKeyAction::keyActionsList.num;++i) if(!idKeyAction::keyActionsList.list[i]){index=i;break;}
        if(index<0) index=idKeyAction::keyActionsList.Append(action); else idKeyAction::keyActionsList.list[index]=action;
        idKeyAction::keyActionsHash.Add(action->hashKey,index);
      } else if(mask!=0) {
        action->nextAction=idKeyAction::keyActionsList.list[index]->nextAction;
        idKeyAction::keyActionsList.list[index]->nextAction=action;
      } else {
        action->nextAction=idKeyAction::keyActionsList.list[index];
        idKeyAction::keyActionsList.list[index]=action;
      }
    } else idKeyAction::keyBindingHash.Remove(action->hashBind,index);
    idKeyAction::keyBindingHash.Add(bindHash,index);
    action->modifierMask=mask; action->hashBind=bindHash; action->binding=text;
 #ifdef WOLF_KEYINPUT_VALIDATION
    action->usercmdAction=ActionForBinding(text);
    action->hasCompoundBind=std::strchr(text,';')!=nullptr;
 #else
    action->usercmdAction=usercmdGen->BindStringUsercmdAction(text);
    action->hasCompoundBind=usercmdGen->HasCompoundBinding(text);
 #endif
    return;
  }
  if(action==nullptr) return;
  const int index=oldIndex;
  idKeyAction* head=idKeyAction::keyActionsList.list[index];
  if(head==action) idKeyAction::keyActionsList.list[index]=action->nextAction;
  else for(auto* previous=head;previous;previous=previous->nextAction)
    if(previous->nextAction==action){previous->nextAction=action->nextAction;break;}
  idKeyAction::keyBindingHash.Remove(action->hashBind,index);
  delete action;
  if(idKeyAction::keyActionsList.list[index]==nullptr) idKeyAction::keyActionsHash.Remove(
    idKeyAction::GetKeyHash(device,bindSet,keyNum),index);
}

int idKeyInput::NumBinds(int device,bindSet_t bindSet,const char* binding,bool onlyKeyboard) {
  if(!binding||!*binding) return 0; int count=0;
  for(int i=0;i<idKeyAction::keyActionsList.num;++i) {
    auto* a=idKeyAction::keyActionsList.list[i];
    if(a&&a->GetDevice()==device&&a->GetBindSet()==bindSet&&Same(a->binding.data,binding)&&
       (!onlyKeyboard||a->GetKeyNum()<K_JOY1)) ++count;
  }
  return count;
}
bool idKeyInput::KeyIsBoundTo(int device,bindSet_t bindSet,keyNum_t key,std::uint8_t mask,
                              const char* binding) {
  if(key>K_MWHEELUP) return false;
  auto* action=idKeyAction::GetKeyAction(device,bindSet,key,mask);
  return action&&Same(action->binding.data,binding);
}
bool idKeyInput::KeyIsBoundToMulti(int device,bindSet_t bindSet,keyNum_t key,std::uint8_t mask,
                                   const char* a,const char* b,const char* c,const char* d) {
  auto* action=key<=K_MWHEELUP?idKeyAction::GetKeyAction(device,bindSet,key,mask):nullptr;
  if(!action) return false;
  const char* bindings[]={a,b,c,d,nullptr};
  for(auto* value:bindings) {if(!value) break;if(Same(action->binding.data,value)) return true;}
  return false;
}
bool idKeyInput::ExecKeyBinding(int device,bindSet_t bindSet,keyNum_t key,std::uint8_t mask) {
  auto* action=idKeyAction::GetKeyAction(device,bindSet,key,mask);
  if(!action||action->usercmdAction!=UB_NONE||action->binding.IsEmpty()) return false;
  cmdSystem->AppendCommandText(action->binding.data); cmdSystem->AppendCommandText("\n"); return true;
}

idStr idKeyInput::KeysFromBinding(int device,bindSet_t bindSet,const char* binding,bool firstOnly) {
  idStr result;
  if(!binding||!*binding) return result;
  const int hash=idStr::Hash(binding);
  for(int node=idKeyAction::keyBindingHash.hash.First(hash);node!=-1;
      node=idKeyAction::keyBindingHash.hash.Next(node)) {
    auto* action=idKeyAction::keyActionsList.list[idKeyAction::keyBindingHash.values.list[node]];
    if(action&&action->GetDevice()==device&&action->GetBindSet()==bindSet&&Same(action->binding.data,binding)) {
      if(!result.IsEmpty()&&!firstOnly) result.Append(" or ");
      KeyNumToKeyNames(action->GetKeyNum(),result,firstOnly);
      if(firstOnly&&!result.IsEmpty()) break;
    }
  }
  return result;
}
keyNum_t idKeyInput::GetLastBoundKeyboardKey(int device,bindSet_t bindSet,const char* binding) {
  keyNum_t result=K_NONE;
  if(!binding||!*binding) return result;
  const int hash=idStr::Hash(binding);
  for(int node=idKeyAction::keyBindingHash.hash.First(hash);node!=-1;
      node=idKeyAction::keyBindingHash.hash.Next(node)) {
    auto* action=idKeyAction::keyActionsList.list[idKeyAction::keyBindingHash.values.list[node]];
    if(action&&action->GetDevice()==device&&action->GetBindSet()==bindSet&&
       action->GetKeyNum()<K_JOY1&&Same(action->binding.data,binding)) result=action->GetKeyNum();
  }
  return result;
}

keyBindings_t idKeyInput::KeyBindingsFromBinding(int device,bindSet_t bindSet,const char* binding) {
  keyBindings_t result;
  if(!binding||!*binding) return result;
  const int hash=idStr::Hash(binding);
  for(int node=idKeyAction::keyBindingHash.hash.First(hash);node!=-1;
      node=idKeyAction::keyBindingHash.hash.Next(node)) {
    auto* action=idKeyAction::keyActionsList.list[idKeyAction::keyBindingHash.values.list[node]];
    if(!action||action->GetDevice()!=device||action->GetBindSet()!=bindSet||!Same(action->binding.data,binding)) continue;
    const keyNum_t key=action->GetKeyNum(); const char* localized=LocalizedKeyName(key);
    if(TypeForKey(key)==INPUT_TYPE_GAMEPAD) {if(*localized)AppendCommaSeparated(localized,result.gamepad);KeyNumToKeyNames(key,result.gamepadKey,true);}
    else if(TypeForKey(key)==INPUT_TYPE_MOUSE) {if(*localized)AppendCommaSeparated(localized,result.mouse);KeyNumToKeyNames(key,result.mouseKey,true);}
    else {if(*localized&&!Same(localized,result.keyboard.data))AppendCommaSeparated(localized,result.keyboard);KeyNumToKeyNames(key,result.keyboardKey,true);}
  }
  return result;
}

bool idKeyInput::UnbindBinding(int device,bindSet_t bindSet,const char* binding) {
  if(!binding||!*binding) return false; bool removed=false;
  struct KeyMask {keyNum_t key;std::uint8_t mask;}; std::vector<KeyMask> matches;
  const int hash=idStr::Hash(binding);
  for(int node=idKeyAction::keyBindingHash.hash.First(hash);node!=-1;
      node=idKeyAction::keyBindingHash.hash.Next(node)) {
    auto* a=idKeyAction::keyActionsList.list[idKeyAction::keyBindingHash.values.list[node]];
    if(a&&a->GetDevice()==device&&a->GetBindSet()==bindSet&&Same(a->binding.data,binding))
      matches.push_back({a->GetKeyNum(),a->modifierMask});
  }
  for(const auto& match:matches){SetBinding(device,bindSet,match.key,match.mask,"");removed=true;}
  return removed;
}
void idKeyInput::UnbindBinding(int device,bindSet_t bindSet,const char* binding,inputType_t type) {
  if(!binding||!*binding) return; struct KeyMask{keyNum_t key;std::uint8_t mask;};std::vector<KeyMask> matches;
  for(int i=0;i<idKeyAction::keyActionsList.num;++i)
    for(auto* a=idKeyAction::keyActionsList.list[i];a;a=a->nextAction)
      if(a->GetDevice()==device&&a->GetBindSet()==bindSet&&TypeForKey(a->GetKeyNum())==type&&Same(a->binding.data,binding))
        matches.push_back({a->GetKeyNum(),a->modifierMask});
  for(const auto& match:matches) SetBinding(device,bindSet,match.key,match.mask,"");
}

void idKeyInput::WriteBindings(idFile* file) {
  if(!file) return;
  int device=userDeviceNum;
#ifndef WOLF_KEYINPUT_VALIDATION
  if(key_deviceBindOverride.valueInteger>=0) device=key_deviceBindOverride.valueInteger;
#endif
  for(int bindSet=BIND_VEHICLE_PASSENGER;bindSet>=BIND_DEFAULT;--bindSet) {
    file->Printf("//========================================\n");
    file->Printf("bindset %d\n",bindSet); file->Printf("unbindall\n");
    for(int key=K_NONE;key<K_LAST_KEY;++key) {
      const int index=idKeyAction::GetKeyActionIndex(device,static_cast<bindSet_t>(bindSet),static_cast<keyNum_t>(key));
      if(index<0) continue;
      for(auto* action=idKeyAction::keyActionsList.list[index];action;action=action->nextAction) {
        idStr combo;
        if(action->modifierMask&1)combo.Append("LSHIFT+"); if(action->modifierMask&2)combo.Append("RSHIFT+");
        if(action->modifierMask&4)combo.Append("LCTRL+"); if(action->modifierMask&8)combo.Append("RCTRL+");
        if(action->modifierMask&0x10)combo.Append("LALT+"); if(action->modifierMask&0x20)combo.Append("RALT+");
        const char* name=KeyNumToString(static_cast<keyNum_t>(key)); combo.Append(Same(name,"\\")?"\\\\":name);
        file->Printf("bind \"%s\" \"%s\"\n",combo.data,action->binding.data);
      }
    }
  }
  file->Printf("//========================================\n");
}

void idKeyInput::SerializeBindings(int device,idSerializer& serializer) {
  if(serializer.writing) {
    const int count=GetNumBindings(device); serializer.msg->WriteBits(count,32);
    for(int i=0;i<idKeyAction::keyActionsList.num;++i) {
      auto* action=idKeyAction::keyActionsList.list[i];
      if(!action||action->GetDevice()!=device) continue;
      for(;action;action=action->nextAction) {
        serializer.msg->WriteBits(action->GetBindSet(),8);
        serializer.msg->WriteBits(action->GetKeyNum(),16);
        serializer.msg->WriteBits(action->modifierMask,8);
        serializer.msg->WriteString(action->binding.data,-1,true);
      }
    }
  } else {
    const int count=serializer.msg->ReadBits(32); idList<serializedBind_t,5> bindings;
    bindings.SetNum(count);
    for(int index=0;index<bindings.num;++index) {
      auto& bind=bindings[index];
      bind.bindSet=static_cast<std::uint8_t>(serializer.msg->ReadBits(8));
      bind.keyNum=static_cast<std::uint16_t>(serializer.msg->ReadBits(16));
      bind.modifierMask=static_cast<std::uint8_t>(serializer.msg->ReadBits(8));
      serializer.msg->ReadString(bind.bindStr);
      UnbindBinding(device,static_cast<bindSet_t>(bind.bindSet),bind.bindStr.data);
    }
    for(int index=0;index<bindings.num;++index) {
      const auto& bind=bindings[index];
      SetBinding(device,static_cast<bindSet_t>(bind.bindSet),
        static_cast<keyNum_t>(bind.keyNum),bind.modifierMask,bind.bindStr.data);
    }
  }
}

idStrDynStatic<256> GetKeyComboStr(keyNum_t key,std::uint8_t modifierMask,bool escapeChar) {
  idStrDynStatic<256> result;
  if(modifierMask&1) result.Append("LSHIFT+");
  if(modifierMask&2) result.Append("RSHIFT+");
  if(modifierMask&4) result.Append("LCTRL+");
  if(modifierMask&8) result.Append("RCTRL+");
  if(modifierMask&0x10) result.Append("LALT+");
  if(modifierMask&0x20) result.Append("RALT+");
  const char* name=idKeyInput::KeyNumToString(key);
  result.Append(escapeChar&&std::strcmp(name,"\\")==0?"\\\\":name);
  return result;
}

static void SplitStrByStr(const char* text,const char* separator,idList<idStr,5>& strings) {
  int start=0;
  for(;;) {
    const int split=idStr::Find(text,separator,false,start,-1);
    if(split==-1) break;
    strings.Append(idStr(text,start,split));
    start=split+1;
    if(split==-2) return;
  }
  strings.Append(idStr(&text[start]));
}

void ArgCompletion_Bind(idAutoComplete& completion) {
  if(completion.args.argc<=2) {
    ArgCompletion_KeyName(completion);
    return;
  }
  if(*completion.args.argv[2]==0) return;
  if(completion.args.argc<=3) {
    for(auto* entry=userCmdStrings;entry->string;++entry)
      completion.Append(idStr(entry->string),-1);
  }
  idAutoComplete commands;
  for(int index=2;index<completion.args.argc;++index)
    commands.args.AppendArg(completion.args.Argv(index));
  cvarSystem->CommandCompletion(&commands);
  cmdSystem->CommandCompletion(&commands);
  for(int index=0;index<commands.suggestions.num;++index)
    completion.Append(commands.suggestions[index],2);
}

#ifndef WOLF_KEYINPUT_VALIDATION
namespace {
int BindDevice() {
  return key_deviceBindOverride.valueInteger>=0?key_deviceBindOverride.valueInteger:userDeviceNum;
}
}

void unbindAll_f(const idCmdArgs&) {
  const int device=BindDevice();
  const bindSet_t set=static_cast<bindSet_t>(in_bindset.valueInteger);
  for(int key=K_NONE;key<K_LAST_KEY;++key) {
    std::uint8_t masks[64]{}; int count=0;
    const int index=idKeyAction::GetKeyActionIndex(device,set,static_cast<keyNum_t>(key));
    if(index>=0) for(auto* action=idKeyAction::keyActionsList.list[index];action;action=action->nextAction)
      masks[count++]=action->modifierMask;
    for(int i=0;i<count;++i) idKeyInput::SetBinding(device,set,static_cast<keyNum_t>(key),masks[i],"");
  }
}

void listBinds_f(const idCmdArgs&) {
  const int device=BindDevice();
  for(int index=0;index<idKeyAction::keyActionsList.num;++index) {
    auto* action=idKeyAction::keyActionsList.list[index];
    if(!action||action->GetDevice()!=device) continue;
    for(;action;action=action->nextAction) {
      idStrDynStatic<256> combo=GetKeyComboStr(action->GetKeyNum(),action->modifierMask,false);
      idLib::Printf("%s \"%s\"\n",combo.data,action->binding.data);
    }
  }
}

void bind_f(const idCmdArgs& args) {
  if(args.argc<2) {idLib::Printf("bind <key> [command] : attach a command to a key\n");return;}
  keyNum_t key=K_NONE;std::uint8_t mask=0;
  if(!idKeyAction::GetKeyAndModifierFromString(args.Argv(1),key,mask)) {
    idLib::Printf("\"%s\" isn't a valid key\n",args.Argv(1));return;
  }
  if(static_cast<unsigned>(in_bindset.valueInteger)>BIND_VEHICLE_PASSENGER) return;
  const int device=BindDevice();
  if(args.argc==2) {
    auto* action=idKeyAction::GetKeyAction(device,static_cast<bindSet_t>(in_bindset.valueInteger),key,mask);
    if(action) idLib::Printf("\"%s\" = \"%s\"\n",args.Argv(1),action->binding.data);
    else idLib::Printf("\"%s\" is not bound\n",args.Argv(1));
    return;
  }
  char command[1024]{};
  for(int index=2;index<args.argc;++index) {
    idStr::Append(command,sizeof(command),args.Argv(index));
    if(index!=args.argc-1) idStr::Append(command,sizeof(command)," ");
  }
  idKeyInput::SetBinding(device,static_cast<bindSet_t>(in_bindset.valueInteger),key,mask,command);
}

void unbind_f(const idCmdArgs& args) {
  if(args.argc!=2) {idLib::Printf("unbind <key> : remove commands from a key\n");return;}
  keyNum_t key=K_NONE;std::uint8_t mask=0;const int device=BindDevice();
  if(idKeyAction::GetKeyAndModifierFromString(args.Argv(1),key,mask)) {
    idKeyInput::SetBinding(device,static_cast<bindSet_t>(in_bindset.valueInteger),key,mask,"");
  } else if(!idKeyInput::UnbindBinding(device,static_cast<bindSet_t>(in_bindset.valueInteger),args.Argv(1))) {
    idLib::Printf("\"%s\" isn't a valid key\n",args.Argv(1));
  }
}

idCVar in_bindset("bindset","0",CVAR_INTEGER|CVAR_NOCHEAT,
                  "value of current bind set",0.0f,7.0f,nullptr);
idCVar key_deviceBindOverride("key_deviceBindOverride","-1",CVAR_INTEGER,
  " if > -1 will use that device to exec bind commmands on",-1.0f,3.0f,nullptr);
idCommandLink unbind_v("unbind",reinterpret_cast<commandFunction_t>(&unbind_f),"unbinds any command from a key",
                       reinterpret_cast<argCompletion_t>(&ArgCompletion_KeyName));
idCommandLink unbindAll_v("unbindAll",reinterpret_cast<commandFunction_t>(&unbindAll_f),"unbinds any commands from all keys",nullptr);
idCommandLink bind_v("bind",reinterpret_cast<commandFunction_t>(&bind_f),"binds a command to a key",
                     reinterpret_cast<argCompletion_t>(&ArgCompletion_Bind));
idCommandLink listBinds_v("listBinds",reinterpret_cast<commandFunction_t>(&listBinds_f),"lists key bindings",nullptr);
#endif
