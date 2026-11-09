#pragma once

#include "shared/idlib/containers/hashindex.h"
#include "shared/idlib/containers/list.h"
#include "shared/idlib/text/str.h"
#include <cstdint>

class idAutoComplete;
class idFile;
class idSerializer;

enum keyNum_t : int {
  K_NONE=0, K_ESCAPE=1, K_1=2, K_2=3, K_3=4, K_4=5, K_5=6, K_6=7,
  K_7=8, K_8=9, K_9=10, K_0=11, K_MINUS=12, K_EQUALS=13,
  K_BACKSPACE=14, K_TAB=15, K_Q=16, K_W=17, K_E=18, K_R=19, K_T=20,
  K_Y=21, K_U=22, K_I=23, K_O=24, K_P=25, K_LBRACKET=26,
  K_RBRACKET=27, K_ENTER=28, K_LCTRL=29, K_A=30, K_S=31, K_D=32,
  K_F=33, K_G=34, K_H=35, K_J=36, K_K=37, K_L=38, K_SEMICOLON=39,
  K_APOSTROPHE=40, K_GRAVE=41, K_LSHIFT=42, K_BACKSLASH=43, K_Z=44,
  K_X=45, K_C=46, K_V=47, K_B=48, K_N=49, K_M=50, K_COMMA=51,
  K_PERIOD=52, K_SLASH=53, K_RSHIFT=54, K_KP_STAR=55, K_LALT=56,
  K_SPACE=57, K_CAPSLOCK=58, K_F1=59, K_F2=60, K_F3=61, K_F4=62,
  K_F5=63, K_F6=64, K_F7=65, K_F8=66, K_F9=67, K_F10=68,
  K_PAUSE=69, K_SCROLL=70, K_KP_7=71, K_KP_8=72, K_KP_9=73,
  K_KP_MINUS=74, K_KP_4=75, K_KP_5=76, K_KP_6=77, K_KP_PLUS=78,
  K_KP_1=79, K_KP_2=80, K_KP_3=81, K_KP_0=82, K_KP_DOT=83,
  K_F11=87, K_F12=88, K_F13=100, K_F14=101, K_F15=102, K_KANA=112,
  K_CONVERT=121, K_NOCONVERT=123, K_YEN=125, K_KP_EQUALS=141,
  K_CIRCUMFLEX=144, K_AT=145, K_COLON=146, K_UNDERLINE=147,
  K_KANJI=148, K_STOP=149, K_AX=150, K_UNLABELED=151, K_KP_ENTER=156,
  K_RCTRL=157, K_KP_COMMA=179, K_KP_SLASH=181, K_PRINTSCREEN=183,
  K_RALT=184, K_NUMLOCK=197, K_HOME=199, K_UPARROW=200, K_PGUP=201,
  K_LEFTARROW=203, K_RIGHTARROW=205, K_END=207, K_DOWNARROW=208,
  K_PGDN=209, K_INS=210, K_DEL=211, K_LWIN=219, K_RWIN=220,
  K_APPS=221, K_POWER=222, K_SLEEP=223,
  K_JOY1=256, K_JOY2, K_JOY3, K_JOY4, K_JOY5, K_JOY6, K_JOY7,
  K_JOY8, K_JOY9, K_JOY10, K_JOY11, K_JOY12, K_JOY13, K_JOY14,
  K_JOY15, K_JOY16, K_JOY_STICK1_UP, K_JOY_STICK1_DOWN,
  K_JOY_STICK1_LEFT, K_JOY_STICK1_RIGHT, K_JOY_STICK2_UP,
  K_JOY_STICK2_DOWN, K_JOY_STICK2_LEFT, K_JOY_STICK2_RIGHT,
  K_JOY_TRIGGER1, K_JOY_TRIGGER2, K_JOY_DPAD_UP, K_JOY_DPAD_DOWN,
  K_JOY_DPAD_LEFT, K_JOY_DPAD_RIGHT,
  K_MOUSE1=286, K_MOUSE2, K_MOUSE3, K_MOUSE4, K_MOUSE5, K_MOUSE6,
  K_MOUSE7, K_MOUSE8, K_MWHEELDOWN, K_MWHEELUP, K_LAST_KEY=296
};

enum bindSet_t : int {
  BIND_DEFAULT=0, BIND_SINGLEPLAYER_FIRST_PERSON=BIND_DEFAULT, BIND_MENUS=1,
  BIND_DRIVE=2, BIND_POSSESS=3, BIND_REMOTE=4, BIND_VEHICLE_GUNNER=5,
  BIND_VEHICLE_PASSENGER=6, BIND_MAX=7
};
using bindContext_t = bindSet_t;
enum inputType_t : int { INPUT_TYPE_KEYBOARD=0, INPUT_TYPE_MOUSE=1, INPUT_TYPE_GAMEPAD=2 };
enum cmdGenButton_t : int { UB_NONE=56 };

struct keyname_t { keyNum_t keynum; const char* name; const char* strId; };
struct userCmdString_t { const char* string; cmdGenButton_t button; };
struct keyBindings_t {
  idStr keyboard, mouse, gamepad, keyboardKey, mouseKey, gamepadKey;
};
struct serializedBind_t {
  std::uint16_t keyNum{};
  std::uint8_t bindSet{};
  std::uint8_t modifierMask{};
  idStr bindStr;
};

template <typename T> class idHashMap {
public:
  idHashIndex hash;
  idList<T*> usedValues;
  idList<T> values;

  explicit idHashMap(memTag_t tag=TAG_IDLIST) : hash(1024,1024) {
    (void)tag;
    hash.memTag=TAG_HASHINDEX;
    usedValues.SetNum(hash.indexSize);
    values.SetNum(hash.indexSize);
    for(int index=0;index<usedValues.size;++index) usedValues.list[index]=nullptr;
  }
  void Add(int hashValue, const T& value) {
    int index=-1;
    for(int i=0;i<usedValues.num;++i) if(usedValues.list[i]==nullptr){index=i;break;}
    T* marker=reinterpret_cast<T*>(static_cast<std::uintptr_t>(1));
    if(index<0){
      index=usedValues.Append(marker);
      values.SetNum(index+1);
      for(int i=0;i<usedValues.num;++i)
        if(usedValues.list[i]!=nullptr) usedValues.list[i]=&values.list[i];
    } else usedValues.list[index]=&values.list[index];
    values.list[index]=value;
    hash.Add(hashValue,index);
  }
  void Remove(int hashValue, const T& value) {
    for(int index=hash.First(hashValue);index!=-1;index=hash.Next(index)) {
      if(values.list[index]==value){hash.Remove(hashValue,index);usedValues.list[index]=nullptr;return;}
    }
  }
};

class idKeyAction {
public:
  explicit idKeyAction(std::uint16_t keyHash);
  static std::uint16_t GetKeyHash(int device,int bindSet,int keyNum) {
    return static_cast<std::uint16_t>((keyNum&0x1FF)|(((bindSet&0xF)|(16*device))<<9));
  }
  static int GetKeyActionIndex(int,bindSet_t,keyNum_t);
  static idKeyAction* GetKeyAction(int,bindSet_t,keyNum_t,std::uint8_t);
  static bool GetKeyAndModifierFromString(const char*,keyNum_t&,std::uint8_t&);
  int GetDevice(){return hashKey>>13;}
  keyNum_t GetKeyNum(){return static_cast<keyNum_t>(hashKey&0x1FF);}
  bindSet_t GetBindSet(){return static_cast<bindSet_t>((hashKey>>9)&0xF);}
  std::uint8_t GetModifierMask(){return modifierMask;}

  idKeyAction* nextAction;
  idStr binding;
  cmdGenButton_t usercmdAction;
  std::uint16_t hashKey;
  int hashBind;
  bool hasCompoundBind;
  std::uint8_t modifierMask;

  static idHashMap<int> keyBindingHash;
  static idHashIndex keyActionsHash;
  static idList<idKeyAction*> keyActionsList;
};

class idKeyInput {
public:
  static keyNum_t StringToKeyNum(const char*);
  static const char* KeyNumToString(keyNum_t);
  static const char* LocalizedKeyName(keyNum_t);
  static void WriteBindings(idFile*);
  static void PreliminaryKeyEvent(int,keyNum_t,bool);
  static bool IsDown(int,keyNum_t);
  static bool IsShiftDown(int);
  static bool IsCtrlDown(int);
  static bool IsAltDown(int);
  static void ClearStates();
  static bool IsValidRebindKey(keyNum_t);
  static void SetUserDeviceNumForBind(int);
  static int GetNumBindings(int);
  static void SerializeBindings(int,idSerializer&);
  static int GetUsercmdAction(int,bindSet_t,keyNum_t,std::uint8_t);
  static idStr GetUsercmdActionStr(int);
  static void SetBinding(int,bindSet_t,keyNum_t,std::uint8_t,const char*);
  static const char* GetBinding(int,bindSet_t,keyNum_t,std::uint8_t);
  static bool UnbindBinding(int,bindSet_t,const char*);
  static void UnbindBinding(int,bindSet_t,const char*,inputType_t);
  static int NumBinds(int,bindSet_t,const char*,bool);
  static bool ExecKeyBinding(int,bindSet_t,keyNum_t,std::uint8_t);
  static idStr KeysFromBinding(int,bindSet_t,const char*,bool);
  static keyNum_t GetLastBoundKeyboardKey(int,bindSet_t,const char*);
  static keyBindings_t KeyBindingsFromBinding(int,bindSet_t,const char*);
  static bool KeyIsBoundTo(int,bindSet_t,keyNum_t,std::uint8_t,const char*);
  static bool KeyIsBoundToMulti(int,bindSet_t,keyNum_t,std::uint8_t,
                                const char*,const char*,const char*,const char*);
};

void ArgCompletion_KeyName(idAutoComplete&);

extern keyname_t keynames[];
extern userCmdString_t userCmdStrings[];

static_assert(sizeof(keyname_t)==24,"keyname_t ABI drift");
static_assert(sizeof(userCmdString_t)==16,"userCmdString_t ABI drift");
static_assert(sizeof(keyBindings_t)==0xF0,"keyBindings_t ABI drift");
static_assert(sizeof(serializedBind_t)==0x30,"serializedBind_t ABI drift");
static_assert(sizeof(idHashMap<int>)==0x58,"idHashMap ABI drift");
static_assert(sizeof(idKeyAction)==0x40,"idKeyAction ABI drift");
