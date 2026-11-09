#pragma once

#include "engine/framework/game_network.h"
#include "engine/framework/keyinput.h"
#include "engine/framework/usercmd.h"
#include "shared/idlib/math/vector.h"

#include <cstddef>

class idDeclTable;
class idFile;

class idUsercmdGen {
public:
  virtual ~idUsercmdGen() = default;
  static void operator delete(void* pointer) noexcept {
    idMem::Singleton()->Free(pointer,ALIGN_16);
  }
  static void operator delete(void* pointer,std::size_t) noexcept {
    operator delete(pointer);
  }
  virtual void Init() = 0;
  virtual void Shutdown() = 0;
  virtual void Clear() = 0;
  virtual void ClearAngles() = 0;
  virtual void SaveGame(idFile*) = 0;
  virtual bool LoadGame(idFile*) = 0;
  virtual cmdGenButton_t BindStringUsercmdAction(const char*) const = 0;
  virtual bool HasCompoundBinding(const char*) const = 0;
  virtual usercmd_t GetUsercmd(int,const inputSettings_t&,bool,int) = 0;
  virtual void GetRecordedUserCmd(int,usercmd_t&) = 0;
  virtual bool RecordPlay(int,bool,const char*) = 0;
  virtual bool GetUseAimAssist() const = 0;
  virtual bool GetMouseDetected() const = 0;
  virtual bool GetJoystickDetected() const = 0;
};

enum transferFunction_t : int {
  FUNC_LINEAR=0,
  FUNC_EXPONENTIAL=1,
  FUNC_LOGARITHMIC=2,
};

idVec2 JoypadFunction(idVec2 raw,float aimAssistScale,float threshold,
                      float range,transferFunction_t shape,
                      bool mergedThreshold);

class idUsercmdGenLocal final : public idUsercmdGen {
public:
  struct usercmdDevice_t {
    usercmdDevice_t()
        : lastPollTime(0),angles(),keyState{},joystickAxis{},touchPos{},
          touchActive{} {}
    int lastPollTime;
    idAngles angles;
    bool keyState[K_LAST_KEY];
    float joystickAxis[6];
    short touchPos[2][2];
    bool touchActive[2];
    std::byte alignmentTail[2];
  };

  idUsercmdGenLocal();
  ~idUsercmdGenLocal() override=default;
  void Init() override;
  void Shutdown() override;
  void Clear() override;
  void ClearAngles() override;
  void SaveGame(idFile*) override;
  bool LoadGame(idFile*) override;
  cmdGenButton_t BindStringUsercmdAction(const char*) const override;
  bool HasCompoundBinding(const char*) const override;
  usercmd_t GetUsercmd(int,const inputSettings_t&,bool,int) override;
  void GetRecordedUserCmd(int,usercmd_t&) override;
  bool RecordPlay(int,bool,const char*) override;
  bool GetUseAimAssist() const override;
  bool GetMouseDetected() const override;
  bool GetJoystickDetected() const override;

  void SetPlayParms(int,bool,int);

private:
  void KeyMove();
  void MouseMove();
  void CircleToSquare(float&,float&) const;
  void HandleJoystickAxis(int,keyNum_t,float,float,bool,const idDeclTable*);
  void JoystickMove2(int);
  void JoystickMove(int);
  void Mouse(int);
  void Keyboard(int);
  void Joystick(int);
  void Key(int,keyNum_t,bool);
  void WriteCmd(int,const usercmd_t&);
  bool ReadCmd(int,usercmd_t&);

public:
  int pollTime;
  usercmdDevice_t devices[4];
  bool buttonState[57];
  std::byte alignmentBeforeLastCommand[3];
  int lastCommandTime;
  bool initialized;
  std::byte alignmentBeforeCurrentDevice[3];
  int currentDeviceNum;
  std::byte alignmentBeforeDevicePointer[4];
  usercmdDevice_t* currentDevice;
  inputSettings_t currentSettings;
  usercmd_t cmd;
  float mouseDx;
  float mouseDy;
  int lastInputFrame;
  idFile* cmdFiles[16];
  bool recording[16];
  int loopPlayCmds[16];
  bool aimAssist;
  bool joystickSnapDisabled;
  bool joystickDetected;
  bool mouseDetected;
  float lastUsedLookValue[4];
  float lastUsedLookValuePitch[4];
  bool allowButtonInputDuringPlay;
  std::byte alignmentBeforeSensitivity[3];
  const idDeclTable* sensitivityJoy1;
  const idDeclTable* sensitivityJoy2;

#if defined(WOLF_USERCMDGEN_VALIDATION)
  void ValidationKeyMove(){KeyMove();}
  void ValidationMouseMove(){MouseMove();}
  void ValidationCircleToSquare(float& x,float& y){CircleToSquare(x,y);}
  void ValidationHandleJoystickAxis(int device,keyNum_t key,float value,
                                    float threshold,bool positive) {
    HandleJoystickAxis(device,key,value,threshold,positive,nullptr);
  }
#endif
};

extern idUsercmdGen* usercmdGen;
extern idUsercmdGenLocal localUsercmdGen;
extern int lastUsercmdGenMicroSeconds;

static_assert(sizeof(idUsercmdGen)==8,"idUsercmdGen ABI drift");
static_assert(sizeof(idUsercmdGenLocal::usercmdDevice_t)==348,
              "usercmd device ABI drift");
static_assert(offsetof(idUsercmdGenLocal,devices)==12,
              "usercmd devices offset drift");
static_assert(offsetof(idUsercmdGenLocal,currentSettings)==1488,
              "usercmd settings offset drift");
static_assert(offsetof(idUsercmdGenLocal,cmd)==1528,
              "usercmd output offset drift");
static_assert(offsetof(idUsercmdGenLocal,cmdFiles)==1616,
              "usercmd files offset drift");
static_assert(offsetof(idUsercmdGenLocal,aimAssist)==1824,
              "usercmd state offset drift");
static_assert(sizeof(idUsercmdGenLocal)==1880,"idUsercmdGenLocal ABI drift");
