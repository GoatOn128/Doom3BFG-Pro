#include "engine/framework/usercmdgen.h"

#include "engine/decls/decltable.h"
#include "engine/sys/sys_public.h"
#include "shared/idlib/csystems/cvarsystemlocal.h"
#include "shared/idlib/csystems/cmdsystemlocal.h"
#include "shared/idlib/filesystem/file.h"
#include "shared/idlib/lib.h"
#include "shared/idlib/sys/sys_time.h"
#include "shared/idlib/text/cmdargs.h"

#include <algorithm>
#include <cmath>
#include <cstring>

extern int com_editors;
extern bool Sys_IsGameWindowActive();

int lastUsercmdGenMicroSeconds=0;

idCVar ucmd_useSensitivityTable("ucmd_useSensitivityTable","0",CVAR_BOOL,
    " if true will use the sensitivity table set up in the editor");
idCVar in_joystick("in_joystick","1",CVAR_BOOL|CVAR_ARCHIVE,
    "enables joystick input",-1.0f,4.0f);
idCVar joy_newCode("joy_newCode","0",CVAR_BOOL|CVAR_ARCHIVE,"Use the new codepath");
idCVar joy_mergedThreshold("joy_mergedThreshold","1",CVAR_BOOL|CVAR_ARCHIVE,
    "If the thresholds aren't merged, you drift more off center");
idCVar joy_triggerThreshold("joy_triggerThreshold","0.05",CVAR_FLOAT|CVAR_ARCHIVE,
    "how far the joystick triggers have to be pressed before they register as down");
idCVar joy_deadZone("joy_deadZone","0.2",CVAR_FLOAT|CVAR_ARCHIVE,
    "specifies how large the dead-zone is on the joystick");
idCVar joy_range("joy_range","1.0",CVAR_FLOAT|CVAR_ARCHIVE,
    "allow full range to be mapped to a smaller offset");
idCVar joy_powerScale("joy_powerScale","2",CVAR_FLOAT|CVAR_ARCHIVE,
    "Raise joystick values to this power");
idCVar joy_yawSpeed("joy_yawSpeed","240",CVAR_FLOAT|CVAR_ARCHIVE,
    "yaw speed when pressing left or right on the joystick",100.0f,600.0f);
idCVar joy_pitchSpeed("joy_pitchSpeed","120",CVAR_FLOAT|CVAR_ARCHIVE,
    "pitch speed when pressing left or right on the joystick",100.0f,600.0f);
idCVar joy_gammaLook("joy_gammaLook","1",CVAR_BOOL|CVAR_ARCHIVE,
    "use a log curve instead of a power curve for movement");
idCVar joy_dampenLook("joy_dampenLook","1",CVAR_BOOL,
    "Do not allow full acceleration on look");
idCVar joy_deltaPerMSLook("joy_deltaPerMSLook","0.003",CVAR_FLOAT,
    "Max amount to be added on look per MS");
idCVar joy_circleToSquare("joy_circleToSquare","2",CVAR_INTEGER|CVAR_ARCHIVE,
    "Convert view input from circle to square to prevent diag slowdown.");
idCVar joy_circleToSquarePower("joy_circleToSquarePower","2",CVAR_FLOAT|CVAR_ARCHIVE,
    "Power exponent for the circle to square lerp");
idCVar in_yawSpeed("in_yawspeed","140",CVAR_FLOAT|CVAR_ARCHIVE,
    "yaw change speed when holding down _left or _right button");
idCVar in_pitchSpeed("in_pitchspeed","140",CVAR_FLOAT|CVAR_ARCHIVE,
    "pitch change speed when holding down look _lookUp or _lookDown button");
idCVar in_angleSpeedKey("in_anglespeedkey","1.5",CVAR_FLOAT|CVAR_ARCHIVE,
    "angle change scale when holding down _speed button");
idCVar in_invertLook("in_invertLook","0",CVAR_BOOL|CVAR_ARCHIVE,
    "inverts the look controls so the forward looks up (flight controls)");
idCVar in_controlInactiveWindow("in_controlInactiveWindow","0",CVAR_BOOL,
    "allow joystick input to control game instances that don't have win focus");
idCVar in_unlockMouseInMenus("in_unlockMouseInMenus","0",CVAR_BOOL|CVAR_ARCHIVE,
    "If true, the mouse cursor can move outside the game window while in menus.");
idCVar m_sensitivity("m_sensitivity","5",CVAR_FLOAT|CVAR_ARCHIVE,
    "mouse view sensitivity");
idCVar m_pitch("m_pitch","0.022",CVAR_FLOAT|CVAR_ARCHIVE,"mouse pitch scale");
idCVar m_yaw("m_yaw","0.022",CVAR_FLOAT|CVAR_ARCHIVE,"mouse yaw scale");
idCVar m_smooth("m_smooth","1",CVAR_INTEGER|CVAR_ARCHIVE,
    "number of samples blended for mouse viewing",1.0f,8.0f,
    idCmdSystem::ArgCompletion_Integer<1,8>);
idCVar m_showMouseRate("m_showMouseRate","0",CVAR_BOOL,"shows mouse movement");
idCVar m_vehicleMouseControl("m_vehicleMouseControl","0",CVAR_BOOL,
    "use the mouse to control the camera when in a vehicle in multiplayer");

namespace {
float mouseHistory[8][2]{};
int mouseHistoryCounter=0;

signed char ClampMove(int value) {
  return static_cast<signed char>(std::clamp(value,-128,127));
}
}

idVec2 JoypadFunction(idVec2 raw,float aimAssistScale,float threshold,
                      float range,transferFunction_t shape,
                      bool mergedThreshold) {
  if(threshold>=range) return {};
  idVec2 mapped{};
  if(mergedThreshold) {
    const float rawLength=std::sqrt(raw.x*raw.x+raw.y*raw.y);
    const float magnitude=std::max(0.0f,rawLength-threshold);
    if(rawLength>0.0f) {
      mapped.x=raw.x/rawLength*magnitude;
      mapped.y=raw.y/rawLength*magnitude;
    }
  } else {
    mapped.x=raw.x<=0.0f?std::min(0.0f,raw.x+threshold)
                       :std::max(0.0f,raw.x-threshold);
    mapped.y=raw.y<=0.0f?std::min(0.0f,raw.y+threshold)
                       :std::max(0.0f,raw.y-threshold);
  }
  const float inverseRange=1.0f/(range-threshold);
  mapped.x*=inverseRange;
  mapped.y*=inverseRange;
  float magnitude=std::sqrt(mapped.x*mapped.x+mapped.y*mapped.y);
  if(magnitude<=0.0f) return {};
  const idVec2 direction{mapped.x/magnitude,mapped.y/magnitude};
  if(shape==FUNC_EXPONENTIAL)
    magnitude=std::pow(1.0471286f,magnitude*100.0f)*0.01f;
  else if(shape==FUNC_LOGARITHMIC)
    magnitude=std::pow(magnitude,2.0f);
  magnitude=std::min(magnitude*aimAssistScale,1.0f);
  return {direction.x*magnitude,direction.y*magnitude};
}

void usercmd_t::ClearMoveCmds() {
  forwardmove=0;
  rightmove=0;
  upmove=0;
  forwardmoveSec=0;
  rightmoveSec=0;
}

void usercmd_t::Print() const {
  idLib::Printf("Frame: %d\n",clientGameFrame);
  idLib::Printf("Server game time: %d\n",serverGameTime);
  idLib::Printf("Buttons: %x, Buttons2: %x\n",buttons,buttons2);
  idLib::Printf("forward: %d, right: %d, up: %d\n",forwardmove,rightmove,upmove);
  idLib::Printf("forwardSec: %d, rightSec: %d\n",forwardmoveSec,rightmoveSec);
  idLib::Printf("inhibit: %d\n",inhibited);
  idLib::Printf("fromBot: %d\n",fromBot);
  idLib::Printf("angles: %d %d %d\n",angles[0],angles[1],angles[2]);
  idLib::Printf("pos: %f %f %f\n",pos.x,pos.y,pos.z);
  idLib::Printf("speed: %d\n",speed);
  idLib::Printf("pitch: %d, yaw: %d, roll: %d\n",pitch,yaw,roll);
  idLib::Printf("predictionStateBits: %x\n",predictionStateBits);
  idLib::Printf("vehicle throttle: %d\n",vehicleThrottle);
  idLib::Printf("fire count: %d\n",fireCount);
  idLib::Printf("fire angles: %d %d\n",fireAngles[0],fireAngles[1]);
  idLib::Printf("firePos: %d %d %d\n",firePos[0],firePos[1],firePos[2]);
}

idUsercmdGenLocal::idUsercmdGenLocal()
    : pollTime(0),devices{},buttonState{},lastCommandTime(0),initialized(false),
      currentDeviceNum(0),currentDevice(devices),currentSettings{},cmd(),
      mouseDx(0),mouseDy(0),lastInputFrame(0),cmdFiles{},recording{},
      aimAssist(false),joystickSnapDisabled(false),joystickDetected(false),
      mouseDetected(false),lastUsedLookValue{},lastUsedLookValuePitch{},
      sensitivityJoy1(nullptr),sensitivityJoy2(nullptr) {
  currentSettings.bindset=BIND_DEFAULT;
  currentSettings.mouseSensScale=1.0f;
  currentSettings.joySensScale=1.0f;
  currentSettings.joyPitchSpeed=joy_pitchSpeed.valueFloat;
  currentSettings.joyYawSpeed=joy_yawSpeed.valueFloat;
  currentSettings.mouseSens=m_sensitivity.valueFloat;
  currentSettings.absoluteAngles=false;
  currentSettings.lookInvert=in_invertLook.valueInteger!=0;
  Clear();
}

void idUsercmdGenLocal::Shutdown(){ initialized=false; }

void idUsercmdGenLocal::Clear() {
  for(auto& device:devices) {
    std::memset(device.keyState,0,sizeof(device.keyState));
    device.joystickAxis[4]=0.0f;
    device.joystickAxis[5]=0.0f;
    std::memset(device.touchPos,0,sizeof(device.touchPos));
    std::memset(device.touchActive,0,sizeof(device.touchActive));
  }
  lastInputFrame=0;
  currentSettings.bindset=BIND_DEFAULT;
  currentSettings.joySensScale=1.0f;
  currentSettings.mouseSensScale=1.0f;
}

void idUsercmdGenLocal::ClearAngles() {
  for(auto& device:devices) device.angles=idAngles{};
}

bool idUsercmdGenLocal::GetUseAimAssist() const{return aimAssist;}
bool idUsercmdGenLocal::GetMouseDetected() const{return mouseDetected;}
bool idUsercmdGenLocal::GetJoystickDetected() const{return joystickDetected;}

void idUsercmdGenLocal::Mouse(int inputDeviceNum) {
  if(currentDeviceNum!=0) return;
  idMouse* mouse=sys->Mouse();
  const int count=mouse->PollInputEvents();
  for(int index=0;index<count;++index) {
    int action=0,value=0;
    if(!mouse->ReturnInputEvent(index,action,value)) continue;
    if(action<=7) {
      const keyNum_t key=static_cast<keyNum_t>(K_MOUSE1+action);
      currentDevice->keyState[key]=value!=0;
      if(value!=0) buttonState[idKeyInput::GetUsercmdAction(
          inputDeviceNum,currentSettings.bindset,key,0)]=true;
    } else if(action==8) mouseDx+=static_cast<float>(value);
    else if(action==9) mouseDy+=static_cast<float>(value);
    else if(action==10 && value!=0) {
      const keyNum_t key=value>=0?K_MWHEELUP:K_MWHEELDOWN;
      currentDevice->keyState[key]=true;
      buttonState[idKeyInput::GetUsercmdAction(
          inputDeviceNum,currentSettings.bindset,key,0)]=true;
      currentDevice->keyState[key]=false;
    }
  }
  mouse->EndInputEvents();
}

void idUsercmdGenLocal::Keyboard(int inputDeviceNum) {
  if(currentDeviceNum!=0) return;
  idKeyboard* keyboard=sys->Keyboard();
  const int count=keyboard->PollInputEvents();
  for(int index=0;index<count;++index) {
    unsigned keyValue=0; bool state=false;
    if(!keyboard->ReturnInputEvent(index,keyValue,state)) continue;
    const keyNum_t key=static_cast<keyNum_t>(keyValue);
    currentDevice->keyState[key]=state;
    if(state) buttonState[idKeyInput::GetUsercmdAction(
        inputDeviceNum,currentSettings.bindset,key,0)]=true;
  }
  keyboard->EndInputEvents();
}

void idUsercmdGenLocal::Joystick(int inputDeviceNum) {
  idJoystick* joystick=sys->Joystick();
  const int count=joystick->PollInputEvents(currentDeviceNum);
  for(int index=0;index<count;++index) {
    int action=0,value=0;
    if(!joystick->ReturnInputEvent(index,action,value)) continue;
    if(action<=31 || (action>=44 && action<=47)) {
      const keyNum_t key=static_cast<keyNum_t>(action+(action<=31?256:238));
      currentDevice->keyState[key]=value!=0;
      if(value!=0) buttonState[idKeyInput::GetUsercmdAction(
          inputDeviceNum,currentSettings.bindset,key,0)]=true;
    } else if(action>=32 && action<=37) {
      currentDevice->joystickAxis[action-32]=static_cast<float>(value)/32767.0f;
    } else if(action>=38 && action<=43) {
      const int touch=(action-38)/3;
      const int component=(action-38)%3;
      if(component<2) {
        currentDevice->touchActive[touch]=true;
        currentDevice->touchPos[touch][component]=static_cast<short>(value);
      } else currentDevice->touchActive[touch]=false;
    } else if(action==48) {
      std::memset(currentDevice->keyState,0,sizeof(currentDevice->keyState));
      std::memset(currentDevice->joystickAxis,0,sizeof(currentDevice->joystickAxis));
      std::memset(currentDevice->touchPos,0,sizeof(currentDevice->touchPos));
      std::memset(currentDevice->touchActive,0,sizeof(currentDevice->touchActive));
    }
  }
  joystick->EndInputEvents();
}

void idUsercmdGenLocal::KeyMove() {
  const int forward=127*(buttonState[50]-buttonState[51]);
  const int right=127*(buttonState[49]-buttonState[48]);
  const int up=127*(buttonState[29]-buttonState[30]);
  cmd.forwardmove=ClampMove(forward);
  cmd.rightmove=ClampMove(right);
  cmd.upmove=ClampMove(up);
  const float scale=(pollTime-currentDevice->lastPollTime)*0.001f*
                    in_angleSpeedKey.valueFloat*currentSettings.joySensScale;
  currentDevice->angles.yaw-=buttonState[55]*in_yawSpeed.valueFloat*scale;
  currentDevice->angles.yaw+=buttonState[54]*in_yawSpeed.valueFloat*scale;
  currentDevice->angles.pitch-=buttonState[52]*in_pitchSpeed.valueFloat*scale;
  currentDevice->angles.pitch+=buttonState[53]*in_pitchSpeed.valueFloat*scale;
  if(forward!=0 || right!=0) {
    aimAssist=false;
    joystickDetected=false;
    mouseDetected=true;
  }
}

void idUsercmdGenLocal::MouseMove() {
  mouseHistory[mouseHistoryCounter&7][0]=mouseDx;
  mouseHistory[mouseHistoryCounter&7][1]=mouseDy;
  const float samples=std::clamp(m_smooth.valueFloat,1.0f,8.0f);
  float dx=0,dy=0;
  for(int index=0;static_cast<float>(index)<samples;++index) {
    dx+=mouseHistory[(mouseHistoryCounter-index)&7][0];
    dy+=mouseHistory[(mouseHistoryCounter-index)&7][1];
  }
  ++mouseHistoryCounter;
  dx/=samples; dy/=samples;
  if(std::fabs(dx)>1000.0f || std::fabs(dy)>1000.0f) dx=dy=0;
  const float sensitivity=currentSettings.mouseSens*currentSettings.mouseSensScale;
  dx*=sensitivity; dy*=sensitivity;
  if(m_showMouseRate.valueInteger!=0)
    idLib::Printf("[%f %f | %f %f ]\n",mouseDx,mouseDy,dx,dy);
  mouseDx=mouseDy=0;
  currentDevice->angles.yaw-=m_yaw.valueFloat*dx;
  currentDevice->angles.pitch+=(currentSettings.lookInvert?-dy:dy)*m_pitch.valueFloat;
  if(dx!=0 || dy!=0) {
    aimAssist=false; joystickSnapDisabled=true;
    joystickDetected=false; mouseDetected=true;
  }
}

void idUsercmdGenLocal::CircleToSquare(float& x,float& y) const {
  const bool negativeX=x<0,negativeY=y<0;
  x=std::fabs(x); y=std::fabs(y);
  const bool swapped=y>x;
  if(swapped) std::swap(x,y);
  if(x>=0.001f) {
    const float originalX=x;
    const float originalY=y;
    const float length=std::min(std::sqrt(x*x+y*y),1.0f);
    const float amount=std::pow(length,joy_circleToSquarePower.valueFloat);
    x+=(length-x)*amount;
    y+=((originalY/originalX)-originalY)*amount;
    if(swapped) std::swap(x,y);
    if(negativeX) x=-x;
    if(negativeY) y=-y;
  }
}

cmdGenButton_t idUsercmdGenLocal::BindStringUsercmdAction(const char* text) const {
  idCmdArgs args;
  if(text!=nullptr) args.TokenizeString(text,true);
  cmdGenButton_t button=UB_NONE;
  for(int argument=0;argument<args.argc;++argument) {
    for(userCmdString_t* entry=userCmdStrings;entry->string!=nullptr;++entry) {
      if(idStr::Icmp(args.argv[argument],entry->string)==0) {
        if(button==UB_NONE) button=entry->button;
        else {
          idLib::Warning("Mulitiple User Commands bound in: %s",text);
          idLib::Warning("%s, will not be bound properly.",args.argv[argument]);
        }
      }
    }
  }
  return button;
}

bool idUsercmdGenLocal::HasCompoundBinding(const char* text) const {
  idCmdArgs args;
  if(text!=nullptr) args.TokenizeString(text,true);
  int matches=0;
  for(int argument=0;argument<args.argc;++argument)
    for(userCmdString_t* entry=userCmdStrings;entry->string!=nullptr;++entry)
      if(idStr::Icmp(args.argv[argument],entry->string)==0) ++matches;
  return matches<args.argc;
}

void idUsercmdGenLocal::HandleJoystickAxis(
    int inputDeviceNum,keyNum_t keyNum,float value,float threshold,
    bool positive,const idDeclTable* sensitivity) {
  if((value>0.0f && !positive) || (value<0.0f && positive)) return;
  float magnitude=std::fabs(value);
  if(magnitude<=threshold) {
    if(magnitude>=-threshold) return;
    magnitude+=threshold;
  } else magnitude-=threshold;
  const float normalized=magnitude/(1.0f-threshold);
  if(normalized==0.0f) return;
  aimAssist=true;
  joystickSnapDisabled=false;
  joystickDetected=true;
  mouseDetected=false;
  const unsigned action=static_cast<unsigned>(idKeyInput::GetUsercmdAction(
      inputDeviceNum,currentSettings.bindset,keyNum,0));
  if(action<32) cmd.buttons|=1u<<action;
  else if(action<48) cmd.buttons2|=static_cast<std::uint16_t>(1u<<(action-32));

  float movement=normalized;
  if(joy_circleToSquare.valueInteger==1)
    movement=std::clamp(normalized,0.0f,0.70710678f)*1.41421356f;
  float look;
  if(ucmd_useSensitivityTable.valueInteger!=0 && sensitivity!=nullptr)
    look=sensitivity->TableLookup(normalized,true)*currentSettings.joySensScale*normalized;
  else {
    look=joy_gammaLook.valueInteger!=0
        ? std::pow(1.0470001f,movement*100.0f)*0.01f
        : std::pow(movement,joy_powerScale.valueFloat);
    look*=currentSettings.joySensScale;
  }
  const float delta=(pollTime-currentDevice->lastPollTime)*0.001f;
  switch(action) {
  case 29: cmd.upmove=ClampMove(cmd.upmove+static_cast<int>(normalized*127)); break;
  case 30: cmd.upmove=ClampMove(cmd.upmove-static_cast<int>(normalized*127)); break;
  case 48: cmd.rightmove=ClampMove(cmd.rightmove-static_cast<int>(movement*127)); break;
  case 49: cmd.rightmove=ClampMove(cmd.rightmove+static_cast<int>(movement*127)); break;
  case 50: cmd.forwardmove=ClampMove(cmd.forwardmove+static_cast<int>(movement*127)); break;
  case 51: cmd.forwardmove=ClampMove(cmd.forwardmove-static_cast<int>(movement*127)); break;
  case 52: {
    if(currentSettings.absoluteAngles)
      currentDevice->angles.pitch=normalized*90.0f*(currentSettings.lookInvert?-1.0f:1.0f);
    else {
      if(joy_dampenLook.valueInteger!=0) {
        look=std::min(look,(pollTime-currentDevice->lastPollTime)*joy_deltaPerMSLook.valueFloat+
                           lastUsedLookValuePitch[currentDeviceNum]);
        lastUsedLookValuePitch[currentDeviceNum]=look;
      }
      currentDevice->angles.pitch-=delta*look*currentSettings.joyPitchSpeed*
          (currentSettings.lookInvert?-1.0f:1.0f);
    }
    cmd.forwardmoveSec=ClampMove(cmd.forwardmoveSec+static_cast<int>(movement*127));
    break;
  }
  case 53: {
    if(currentSettings.absoluteAngles)
      currentDevice->angles.pitch=-normalized*90.0f*(currentSettings.lookInvert?-1.0f:1.0f);
    else {
      if(joy_dampenLook.valueInteger!=0) {
        look=std::min(look,(pollTime-currentDevice->lastPollTime)*joy_deltaPerMSLook.valueFloat+
                           lastUsedLookValuePitch[currentDeviceNum]);
        lastUsedLookValuePitch[currentDeviceNum]=look;
      }
      currentDevice->angles.pitch+=delta*look*currentSettings.joyPitchSpeed*
          (currentSettings.lookInvert?-1.0f:1.0f);
    }
    cmd.forwardmoveSec=ClampMove(cmd.forwardmoveSec-static_cast<int>(movement*127));
    break;
  }
  case 54:
    if(currentSettings.absoluteAngles) currentDevice->angles.yaw=-normalized*90.0f;
    else {
      if(joy_dampenLook.valueInteger!=0) {
        look=std::min(look,(pollTime-currentDevice->lastPollTime)*joy_deltaPerMSLook.valueFloat+
                           lastUsedLookValue[currentDeviceNum]);
        lastUsedLookValue[currentDeviceNum]=look;
      }
      currentDevice->angles.yaw+=delta*look*currentSettings.joyYawSpeed;
    }
    cmd.rightmoveSec=ClampMove(cmd.rightmoveSec-static_cast<int>(movement*127));
    break;
  case 55:
    if(currentSettings.absoluteAngles) currentDevice->angles.yaw=normalized*90.0f;
    else {
      if(joy_dampenLook.valueInteger!=0) {
        look=std::min(look,(pollTime-currentDevice->lastPollTime)*joy_deltaPerMSLook.valueFloat+
                           lastUsedLookValue[currentDeviceNum]);
        lastUsedLookValue[currentDeviceNum]=look;
      }
      currentDevice->angles.yaw-=delta*look*currentSettings.joyYawSpeed;
    }
    cmd.rightmoveSec=ClampMove(cmd.rightmoveSec+static_cast<int>(movement*127));
    break;
  default: break;
  }
}

void idUsercmdGenLocal::JoystickMove(int inputDeviceNum) {
  if(joy_newCode.valueInteger!=0) { JoystickMove2(inputDeviceNum); return; }
  if(currentSettings.absoluteAngles && !joystickSnapDisabled)
    currentDevice->angles=idAngles{};
  float leftX=currentDevice->joystickAxis[0],leftY=currentDevice->joystickAxis[1];
  if(joy_circleToSquare.valueInteger==2) CircleToSquare(leftX,leftY);
  HandleJoystickAxis(inputDeviceNum,K_JOY_STICK1_UP,leftY,joy_deadZone.valueFloat,false,sensitivityJoy1);
  HandleJoystickAxis(inputDeviceNum,K_JOY_STICK1_DOWN,leftY,joy_deadZone.valueFloat,true,sensitivityJoy1);
  HandleJoystickAxis(inputDeviceNum,K_JOY_STICK1_LEFT,leftX,joy_deadZone.valueFloat,false,sensitivityJoy1);
  HandleJoystickAxis(inputDeviceNum,K_JOY_STICK1_RIGHT,leftX,joy_deadZone.valueFloat,true,sensitivityJoy1);
  float rightX=currentDevice->joystickAxis[2],rightY=currentDevice->joystickAxis[3];
  if(joy_circleToSquare.valueInteger==2) CircleToSquare(rightX,rightY);
  HandleJoystickAxis(inputDeviceNum,K_JOY_STICK2_UP,rightY,joy_deadZone.valueFloat,false,sensitivityJoy2);
  HandleJoystickAxis(inputDeviceNum,K_JOY_STICK2_DOWN,rightY,joy_deadZone.valueFloat,true,sensitivityJoy2);
  HandleJoystickAxis(inputDeviceNum,K_JOY_STICK2_LEFT,rightX,joy_deadZone.valueFloat,false,sensitivityJoy2);
  HandleJoystickAxis(inputDeviceNum,K_JOY_STICK2_RIGHT,rightX,joy_deadZone.valueFloat,true,sensitivityJoy2);
  HandleJoystickAxis(inputDeviceNum,K_JOY_TRIGGER1,currentDevice->joystickAxis[4],joy_triggerThreshold.valueFloat,true,nullptr);
  HandleJoystickAxis(inputDeviceNum,K_JOY_TRIGGER2,currentDevice->joystickAxis[5],joy_triggerThreshold.valueFloat,true,nullptr);
}

void idUsercmdGenLocal::JoystickMove2(int inputDeviceNum) {
  idVec2 left{currentDevice->joystickAxis[0],currentDevice->joystickAxis[1]};
  idVec2 right{currentDevice->joystickAxis[2],currentDevice->joystickAxis[3]};
  if(idKeyInput::GetUsercmdAction(inputDeviceNum,currentSettings.bindset,
                                  K_JOY_STICK1_LEFT,0)==54)
    std::swap(left,right);
  if(currentSettings.lookInvert) right.y=-right.y;
  left=JoypadFunction(left,1.0f,joy_deadZone.valueFloat,joy_range.valueFloat,
                      static_cast<transferFunction_t>(joy_gammaLook.valueInteger),
                      joy_mergedThreshold.valueInteger!=0);
  right=JoypadFunction(right,1.0f,joy_deadZone.valueFloat,joy_range.valueFloat,
                       static_cast<transferFunction_t>(joy_gammaLook.valueInteger),
                       joy_mergedThreshold.valueInteger!=0);
  CircleToSquare(left.x,left.y);
  cmd.forwardmove=ClampMove(cmd.forwardmove-static_cast<int>(left.y*127.0f));
  cmd.rightmove=ClampMove(cmd.rightmove+static_cast<int>(left.x*127.0f));
  const float vertical=currentSettings.lookInvert?right.y:-right.y;
  cmd.forwardmoveSec=ClampMove(cmd.forwardmoveSec+static_cast<int>(vertical*127.0f));
  cmd.rightmoveSec=ClampMove(cmd.rightmoveSec+static_cast<int>(right.x*127.0f));
  const float seconds=(pollTime-currentDevice->lastPollTime)*0.001f;
  currentDevice->angles.pitch+=seconds*right.y*currentSettings.joySensScale*
                               currentSettings.joyPitchSpeed;
  currentDevice->angles.yaw-=seconds*right.x*currentSettings.joySensScale*
                             currentSettings.joyYawSpeed;
  HandleJoystickAxis(inputDeviceNum,K_JOY_TRIGGER1,currentDevice->joystickAxis[4],
                     joy_triggerThreshold.valueFloat,true,nullptr);
  HandleJoystickAxis(inputDeviceNum,K_JOY_TRIGGER2,currentDevice->joystickAxis[5],
                     joy_triggerThreshold.valueFloat,true,nullptr);
  if(left.x!=0 || left.y!=0 || right.x!=0 || right.y!=0) {
    aimAssist=true; joystickSnapDisabled=false;
    joystickDetected=false; mouseDetected=true;
  }
}

bool idUsercmdGenLocal::RecordPlay(int,bool,const char*) { return true; }

void idUsercmdGenLocal::GetRecordedUserCmd(int fileNum,usercmd_t& recorded) {
  if(cmdFiles[fileNum]!=nullptr && !recording[fileNum] && allowButtonInputDuringPlay) {
    recorded.buttons|=recorded.buttons;
    recorded.buttons2|=recorded.buttons2;
    recorded.forwardmove=ClampMove(recorded.forwardmove+recorded.forwardmove);
    recorded.rightmove=ClampMove(recorded.rightmove+recorded.rightmove);
    recorded.upmove=ClampMove(recorded.upmove+recorded.upmove);
    recorded.forwardmoveSec=ClampMove(recorded.forwardmoveSec+recorded.forwardmoveSec);
    recorded.rightmoveSec=ClampMove(recorded.rightmoveSec+recorded.rightmoveSec);
  }
  currentDevice->lastPollTime=pollTime;
}

template<>
std::size_t idFile::WriteBig<idAngles>(const idAngles& value) {
  idAngles big=value;
  for(int index=0;index<3;++index) {
    std::uint32_t bits;
    std::memcpy(&bits,&big[index],sizeof(bits));
    bits=((bits&0x000000FFu)<<24)|((bits&0x0000FF00u)<<8)|
         ((bits&0x00FF0000u)>>8)|((bits&0xFF000000u)>>24);
    std::memcpy(&big[index],&bits,sizeof(bits));
  }
  return Write(&big,sizeof(big));
}

template<>
std::size_t idFile::ReadBig<idAngles>(idAngles& value) {
  const std::size_t count=Read(&value,sizeof(value));
  for(int index=0;index<3;++index) {
    std::uint32_t bits;
    std::memcpy(&bits,&value[index],sizeof(bits));
    bits=((bits&0x000000FFu)<<24)|((bits&0x0000FF00u)<<8)|
         ((bits&0x00FF0000u)>>8)|((bits&0xFF000000u)>>24);
    std::memcpy(&value[index],&bits,sizeof(bits));
  }
  return count;
}

void idUsercmdGenLocal::Init() {
  initialized=true;
  sensitivityJoy1=static_cast<const idDeclTable*>(
      idDeclTable::resourceList.FindWithInheritance("input/joy1",false,true));
  sensitivityJoy2=static_cast<const idDeclTable*>(
      idDeclTable::resourceList.FindWithInheritance("input/joy2",false,true));
}

usercmd_t idUsercmdGenLocal::GetUsercmd(int inputDeviceNum,
    const inputSettings_t& settings,bool inhibit,int) {
  lastUsercmdGenMicroSeconds=static_cast<int>(Sys_Microseconds());
  lastInputFrame=inputDeviceNum==0?idLib::frameNumber:lastInputFrame;
  currentDeviceNum=inputDeviceNum;
  currentDevice=&devices[inputDeviceNum];
  currentSettings=settings;
  pollTime=static_cast<int>(Sys_Microseconds()/1000);
  currentDevice->lastPollTime=std::clamp(currentDevice->lastPollTime,
                                         pollTime-80,pollTime);
  std::memset(&cmd,0,sizeof(cmd));
  std::memset(buttonState,0,sizeof(buttonState));
  Mouse(inputDeviceNum);
  Keyboard(inputDeviceNum);
  if(in_joystick.valueInteger!=0 &&
     (Sys_IsGameWindowActive() || com_editors!=0 ||
      in_controlInactiveWindow.valueInteger!=0))
    Joystick(inputDeviceNum);
  for(int key=K_NONE;key<K_LAST_KEY;++key)
    if(currentDevice->keyState[key])
      buttonState[idKeyInput::GetUsercmdAction(
          inputDeviceNum,BIND_DEFAULT,static_cast<keyNum_t>(key),0)]=true;
  if(currentSettings.bindset!=BIND_DEFAULT)
    for(int key=K_JOY1;key<K_JOY_DPAD_RIGHT;++key)
      if(currentDevice->keyState[key])
        buttonState[idKeyInput::GetUsercmdAction(
            inputDeviceNum,currentSettings.bindset,static_cast<keyNum_t>(key),0)]=true;
  for(int action=0;action<32;++action)
    if(buttonState[action]) cmd.buttons|=1u<<action;
  for(int action=32;action<48;++action)
    if(buttonState[action]) cmd.buttons2|=static_cast<std::uint16_t>(1u<<(action-32));
  cmd.inhibited=inhibit;
  if(inhibit) {
    mouseDx=mouseDy=0;
    HandleJoystickAxis(inputDeviceNum,K_JOY_TRIGGER1,currentDevice->joystickAxis[4],
                       joy_triggerThreshold.valueFloat,true,nullptr);
    HandleJoystickAxis(inputDeviceNum,K_JOY_TRIGGER2,currentDevice->joystickAxis[5],
                       joy_triggerThreshold.valueFloat,true,nullptr);
  } else {
    KeyMove();
    MouseMove();
    if(in_joystick.valueInteger!=0) JoystickMove(inputDeviceNum);
    if(aimAssist) {
      currentDevice->angles.pitch+=currentSettings.aimAssistAngles.pitch;
      currentDevice->angles.yaw+=currentSettings.aimAssistAngles.yaw;
      currentDevice->angles.roll+=currentSettings.aimAssistAngles.roll;
    }
  }
  currentDevice->angles.Normalize360();
  cmd.angles[0]=static_cast<std::int16_t>(currentDevice->angles.pitch*182.04445f);
  cmd.angles[1]=static_cast<std::int16_t>(currentDevice->angles.yaw*182.04445f);
  cmd.angles[2]=static_cast<std::int16_t>(currentDevice->angles.roll*182.04445f);
  for(int touch=0;touch<2;++touch) {
    cmd.touch[touch].pos[0]=currentDevice->touchPos[touch][0];
    cmd.touch[touch].pos[1]=currentDevice->touchPos[touch][1];
    cmd.touch[touch].active=currentDevice->touchActive[touch];
  }
  currentDevice->lastPollTime=pollTime;
  return cmd;
}

void idUsercmdGenLocal::SaveGame(idFile* file) {
  for(auto& device:devices) file->WriteBig(device.angles);
}

bool idUsercmdGenLocal::LoadGame(idFile* file) {
  Clear();
  for(auto& device:devices)
    if(file->ReadBig(device.angles)==0) return false;
  return true;
}

idUsercmdGenLocal localUsercmdGen;
idUsercmdGen* usercmdGen=&localUsercmdGen;
