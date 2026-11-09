#pragma once

#include "engine/guis/swf/swf_resources.h"
#include "engine/guis/swf/swf_scriptfunction.h"
#include "engine/guis/swf/swf_scriptobject.h"
#include "shared/idlib/color.h"
#include "shared/idlib/text/str.h"

class idMaterial;
class idSWF;
class idSWFBitStream;
class idSWFScriptFunctionAS2;

class idSWFSpriteInstance {
public:
  struct tween_t {
    idAtomicString variable;
    float (*function)(float, float, float, float);
    float startValue;
    float stopValue;
    int startTime;
    int duration;
    int pauseTime;
    bool snapBack;
  };

  struct swfAction_t {
    const std::uint8_t* data;
    std::uint32_t dataLength;
  };

  struct placeObject2Data_t {
    placeObject2Data_t()
        : flags(0), depth(0), characterID(-1), name(), matrix(), cxf(), ratio(0.0f),
          clipDepth(0) {}
    std::uint64_t flags;
    int depth;
    int characterID;
    idStr name;
    swfMatrix_t matrix;
    swfColorXform_t cxf;
    float ratio;
    std::uint16_t clipDepth;
  };

  idSWFSpriteInstance(idSWF* swf_, idSWFSprite* sprite_);
  virtual ~idSWFSpriteInstance();
  void Init(idSWFSpriteInstance* parent_, const char* name_, int depth_);
  bool Run();
  bool RunActions();
  void Set(const char* name_, idSWFScriptVar& value);
  idSWFScriptVar Get(const char* name_);
  idSWFScriptVar Get(const char* name1, const char* name2, const char* name3 = nullptr,
                     const char* name4 = nullptr, const char* name5 = nullptr,
                     const char* name6 = nullptr);
  idStr GetTargetPath();
  void SetMaterial(const char* materialName, int width, int height);
  void SetVisible(bool visible);
  void PlayFrame(int frameNum);
  void PlayFrame(const char* frameName);
  void StopFrame(int frameNum);
  void StopFrame(const char* frameName);
  idVec2 GetPos();
  idVec2 GetAbsolutePos();
  void SetPos(float xPos, float yPos);
  void SetPosX(float xPos);
  void SetPosY(float yPos);
  void SetColor(idColor& color);
  void SetColorRGB(idColor& color);
  void SetAlpha(float value);
  void SetScale(float x, float y);
  swfRect_t GetBounds();
  idVec2 GetSize();
  swfDisplayEntry_t* FindDisplayEntry(int displayDepth);
  swfDisplayEntry_t* ExecutePlaceObject2(const placeObject2Data_t& po2);
  void PlaceObject2(idSWFBitStream& bitstream);
  void PlaceObject3(idSWFBitStream& bitstream);
  void RemoveObject2(idSWFBitStream& bitstream);
  void StartSound(idSWFBitStream& bitstream);
  void NextFrame();
  void PrevFrame();
  void RunTo(int targetFrame);
  void Play();
  void Stop();
  void Tween(const char* variable, float from, float to, int duration,
             float (*function)(float, float, float, float), int pauseTime, bool snapBack);
  void RemoveTweens(const char* variable);
  void FreeDisplayList();
  swfDisplayEntry_t* AddDisplayEntry(int displayDepth, int characterID, idStr& objectName);
  void RemoveDisplayEntry(int displayDepth);
  void SwapDepths(int depth1, int depth2);
  idSWFSpriteInstance* FindChildSprite(const char* targetName);
  idSWFSpriteInstance* ResolveTarget(const char* targetName);
  std::uint32_t FindFrame(const char* labelName);
  int FindFrameByLabel(const char* labelName);
  bool FrameExists(const char* labelName);
  bool IsBetweenFrames(const char* frameLabel1, const char* frameLabel2);
  idSWFSpriteInstance* DuplicateMovieClip(int displayDepth, idStr& objectName);
  void RunQueuedActions();
  void ClearQueuedActions();
  bool HandleSWFCommand(const swfSpriteCommand_t& command);

  idAtomicString name;
  idSWFScriptSprite* scriptObject;
  idSWF* swf;
  idSWFSprite* sprite;
  idSWFSpriteInstance* parent;
  int depth;
  std::uint8_t isPlaying : 1;
  std::uint8_t isVisible : 1;
  std::uint8_t childrenRunning : 1;
  std::uint8_t firstRun : 1;
  std::uint8_t ignoreMouse : 1;
  std::uint8_t ignoreSnapToPixel : 1;
  std::uint8_t recoveredFlagsPadding;
  std::uint16_t currentFrame;
  std::uint16_t frameCount;
  std::uint16_t recoveredFramePadding;
  float z;
  idMaterial* materialOverride;
  std::uint16_t materialWidth;
  std::uint16_t materialHeight;
  float xOffset;
  float yOffset;
  std::uint32_t recoveredDisplayPadding;
  idList<swfDisplayEntry_t, 72> displayList;
  idSWFScriptVar onEnterFrame;
  int followBorder;
  std::uint32_t recoveredTweenPadding;
  idList<tween_t, 72> tweens;
  idList<swfAction_t, 72> actions;
  idSWFScriptFunctionAS2* actionScript;
};

static_assert(sizeof(idSWFSpriteInstance::tween_t) == 40, "sprite tween ABI drift");
static_assert(sizeof(idSWFSpriteInstance::swfAction_t) == 16, "sprite action ABI drift");
static_assert(sizeof(idSWFSpriteInstance::placeObject2Data_t) == 120,
              "place object ABI drift");
static_assert(sizeof(idSWFSpriteInstance) == 192, "idSWFSpriteInstance ABI drift");

class idSWFScriptObject_SpriteInstancePrototype : public idSWFScriptObject {
public:
  idSWFScriptObject_SpriteInstancePrototype();
  ~idSWFScriptObject_SpriteInstancePrototype() override = default;

#define SWF_SPRITE_FUNCTION(name)                                                     \
  class idSWFScriptFunction_##name : public idSWFScriptFunction_NestedSWF {           \
  public:                                                                              \
    idSWFScriptVar Call(idSWFScriptObject* object, idSWFParmList& parms) override;     \
  } scriptFunction_##name

  SWF_SPRITE_FUNCTION(duplicateMovieClip);
  SWF_SPRITE_FUNCTION(gotoAndPlay);
  SWF_SPRITE_FUNCTION(gotoAndStop);
  SWF_SPRITE_FUNCTION(swapDepths);
  SWF_SPRITE_FUNCTION(nextFrame);
  SWF_SPRITE_FUNCTION(prevFrame);
  SWF_SPRITE_FUNCTION(play);
  SWF_SPRITE_FUNCTION(stop);
  SWF_SPRITE_FUNCTION(toString);
  SWF_SPRITE_FUNCTION(tween);
  SWF_SPRITE_FUNCTION(removeTweens);
#undef SWF_SPRITE_FUNCTION

#define SWF_SPRITE_NATIVE(name)                                                       \
  class idSWFScriptNativeVar_##name : public idSWFScriptNativeVariable {              \
  public:                                                                              \
    idSWFScriptVar Get(idSWFScriptObject* object) override;                           \
    void Set(idSWFScriptObject* object, const idSWFScriptVar& value) override;         \
  } swfScriptVar_##name
#define SWF_SPRITE_NATIVE_READONLY(name)                                              \
  class idSWFScriptNativeVar_##name : public idSWFScriptNativeVariable {              \
  public:                                                                              \
    bool IsReadOnly() override { return true; }                                       \
    idSWFScriptVar Get(idSWFScriptObject* object) override;                           \
    void Set(idSWFScriptObject*, const idSWFScriptVar&) override {}                    \
  } swfScriptVar_##name

  SWF_SPRITE_NATIVE(_z);
  SWF_SPRITE_NATIVE(_x);
  SWF_SPRITE_NATIVE(_y);
  SWF_SPRITE_NATIVE(_xscale);
  SWF_SPRITE_NATIVE(_yscale);
  SWF_SPRITE_NATIVE(_alpha);
  SWF_SPRITE_NATIVE(_brightness);
  SWF_SPRITE_NATIVE(_visible);
  SWF_SPRITE_NATIVE_READONLY(_width);
  SWF_SPRITE_NATIVE_READONLY(_height);
  SWF_SPRITE_NATIVE(_rotation);
  SWF_SPRITE_NATIVE_READONLY(_name);
  SWF_SPRITE_NATIVE_READONLY(_currentframe);
  SWF_SPRITE_NATIVE_READONLY(_totalframes);
  SWF_SPRITE_NATIVE_READONLY(_target);
  SWF_SPRITE_NATIVE_READONLY(_framesloaded);
  SWF_SPRITE_NATIVE_READONLY(_droptarget);
  SWF_SPRITE_NATIVE_READONLY(_url);
  SWF_SPRITE_NATIVE_READONLY(_highquality);
  SWF_SPRITE_NATIVE_READONLY(_focusrect);
  SWF_SPRITE_NATIVE_READONLY(_soundbuftime);
  SWF_SPRITE_NATIVE_READONLY(_quality);
  SWF_SPRITE_NATIVE_READONLY(_mousex);
  SWF_SPRITE_NATIVE_READONLY(_mousey);
  SWF_SPRITE_NATIVE(material);
  SWF_SPRITE_NATIVE(materialWidth);
  SWF_SPRITE_NATIVE(materialHeight);
  SWF_SPRITE_NATIVE(xOffset);
  SWF_SPRITE_NATIVE(onEnterFrame);
  SWF_SPRITE_NATIVE(_followBorder);
  SWF_SPRITE_NATIVE(_ignoreMouse);
#undef SWF_SPRITE_NATIVE_READONLY
#undef SWF_SPRITE_NATIVE
};

static_assert(sizeof(idSWFScriptObject_SpriteInstancePrototype) == 1680,
              "sprite prototype ABI drift");
