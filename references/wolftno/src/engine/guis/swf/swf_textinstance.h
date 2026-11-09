#pragma once

#include "engine/decls/declswfresource.h"
#include "engine/guis/swf/swf_resources.h"
#include "engine/guis/swf/swf_scriptfunction.h"
#include "engine/guis/swf/swf_scriptobject.h"
#include "shared/idlib/color.h"
#include "shared/idlib/text/str.h"

class idSoundShader;
class idSWF;
class idSWFSpriteInstance;

enum swfTextRenderMode_t : int {
  SWF_TEXT_RENDER_NORMAL = 0,
  SWF_TEXT_RENDER_RANDOM_APPEAR = 1,
  SWF_TEXT_RENDER_RANDOM_APPEAR_CAPS = 2,
  SWF_TEXT_RENDER_PARAGRAPH = 3,
  SWF_TEXT_RENDER_AUTOSCROLL = 4,
  SWF_TEXT_RENDER_MODE_COUNT = 5,
};

struct subTimingWordData_t {
  subTimingWordData_t() : phrase(), startTime(0), forceBreak(false) {}
  idStr phrase;
  int startTime;
  bool forceBreak;
};

class idSWFTextInstance {
public:
  idSWFTextInstance(idSWF* swf_, idSWFEditText* editText_);
  ~idSWFTextInstance();
  void Init(idSWFSpriteInstance* parent_, const char* name_, int depth_);
  void ReInit();
  idStr GetTargetPath();
  void SetTextEffect(swfFontEffects_t type);
  static bool CanBreakLine(std::uint32_t tc, std::uint32_t nexttc);
  void StartRandomText(int time);
  idStr GetRandomText(int time);
  void StartParagraphText(int time);
  idStr GetParagraphText(int time);
  bool NeedsSoundPlayed();
  void SwitchSubtitleText(int time);
  bool UpdateSubtitle(int time);
  void SetSubEndIndex(int endChar, int time);
  void LastWordChanged(int wordCount, int time);
  void SetSubNextStartIndex(int value);
  int GetApproximateSubtitleBreak(int time);
  void SubtitleComplete();
  idStr GetSpeaker();
  void SubtitleCleanup();
  float GetTextLength();
  int GetNumLines();
  float GetMaxGlyphHeight();
  void InputCharacter(char letter);
  idColor GetColor();
  void SetColor(idColor& color_);
  void FindFontType();

  idAtomicString name;
  idSWFScriptText* scriptObject;
  idSWF* swf;
  idSWFEditText* editText;
  idSWFSpriteInstance* parent;
  int depth;
  int numLines;
  bool forceAutoSize;
  bool centerTooltip;
  bool highlight;
  std::uint8_t recoveredTextPadding[5];
  idStr text;
  idStr randomtext;
  idStr variable;
  swfColorRGBA_t color;
  bool visible;
  bool tooltip;
  std::uint8_t recoveredTooltipPadding[2];
  float tooltipScale;
  int bindset;
  swfEditTextAlign_t alignment;
  int selectionStart;
  int selectionEnd;
  int scroll;
  int scrollTime;
  int maxscroll;
  bool forceScroll;
  bool shiftHeld;
  std::uint8_t recoveredInputPadding[2];
  int lastInputTime;
  bool allowNewlineInput;
  std::uint8_t recoveredFontPadding[3];
  int fontType;
  int textLength;
  bool lengthCalculated;
  std::uint8_t recoveredEffectPadding[3];
  int fontFXMaterialIndex;
  int renderMode;
  bool generatingText;
  std::uint8_t recoveredRandomPadding[3];
  int rndSpotsVisible;
  int rndSpacesVisible;
  int charMultiplier;
  int textSpotsVisible;
  int rndTime;
  int startRndTime;
  int prevReplaceIndex;
  bool triggerGenerate;
  std::uint8_t recoveredDelayPadding[3];
  int renderDelay;
  bool scrollUpdate;
  std::uint8_t recoveredSoundPadding[7];
  idSoundShader* soundClip;
  bool needsSoundUpdate;
  std::uint8_t recoveredIndexPadding[7];
  idList<int, 72> indexArray;
  std::uint32_t randomSeed;
  bool isSubtitle;
  std::uint8_t recoveredSubtitlePadding[3];
  int subLength;
  int subCharDisplayTime;
  int subAlign;
  bool subUpdating;
  std::uint8_t recoveredSubtitleIndexPadding[3];
  int subCharStartIndex;
  int subNextStartIndex;
  int subCharEndIndex;
  int subDisplayTime;
  int subStartTime;
  int subSourceID;
  idStr subtitleText;
  bool subNeedsSwitch;
  bool subForceKillQueued;
  bool subForceKill;
  std::uint8_t recoveredSubtitleKillPadding;
  int subKillTimeDelay;
  int subSwitchTime;
  int subLastWordIndex;
  int subPrevLastWordIndex;
  std::uint32_t recoveredSpeakerPadding;
  idStr subSpeaker;
  bool subWaitClear;
  bool subInitialLine;
  std::uint8_t recoveredFinalPadding[2];
  int inputTextStartChar;
  idList<subTimingWordData_t, 72> subtitleTimingInfo;
};

static_assert(sizeof(subTimingWordData_t) == 48, "subtitle timing ABI drift");
static_assert(sizeof(idSWFTextInstance) == 520, "idSWFTextInstance ABI drift");

class idSWFScriptObject_TextInstancePrototype : public idSWFScriptObject {
public:
  idSWFScriptObject_TextInstancePrototype();
  ~idSWFScriptObject_TextInstancePrototype() override = default;

#define SWF_TEXT_FUNCTION(name)                                                       \
  class idSWFScriptFunction_##name : public idSWFScriptFunction_NestedSWF {           \
  public:                                                                              \
    idSWFScriptVar Call(idSWFScriptObject* object, idSWFParmList& parms) override;     \
  } scriptFunction_##name
  SWF_TEXT_FUNCTION(onKey);
  SWF_TEXT_FUNCTION(onChar);
  SWF_TEXT_FUNCTION(generateRnd);
  SWF_TEXT_FUNCTION(calcNumLines);
  SWF_TEXT_FUNCTION(toString);
#undef SWF_TEXT_FUNCTION

#define SWF_TEXT_NATIVE(name)                                                         \
  class idSWFScriptNativeVar_##name : public idSWFScriptNativeVariable {              \
  public:                                                                              \
    idSWFScriptVar Get(idSWFScriptObject* object) override;                           \
    void Set(idSWFScriptObject* object, const idSWFScriptVar& value) override;         \
  } swfScriptVar_##name
#define SWF_TEXT_NATIVE_READONLY(name)                                                \
  class idSWFScriptNativeVar_##name : public idSWFScriptNativeVariable {              \
  public:                                                                              \
    bool IsReadOnly() override { return true; }                                       \
    idSWFScriptVar Get(idSWFScriptObject* object) override;                           \
    void Set(idSWFScriptObject*, const idSWFScriptVar&) override {}                    \
  } swfScriptVar_##name
  SWF_TEXT_NATIVE(text);
  SWF_TEXT_NATIVE(autoSize);
  SWF_TEXT_NATIVE(variable);
  SWF_TEXT_NATIVE(_alpha);
  SWF_TEXT_NATIVE(textColor);
  SWF_TEXT_NATIVE(_visible);
  SWF_TEXT_NATIVE(scroll);
  SWF_TEXT_NATIVE(maxscroll);
  SWF_TEXT_NATIVE(selectionStart);
  SWF_TEXT_NATIVE(selectionEnd);
  SWF_TEXT_NATIVE(isTooltip);
  SWF_TEXT_NATIVE(tooltipScale);
  SWF_TEXT_NATIVE(tooltipBindset);
  SWF_TEXT_NATIVE(mode);
  SWF_TEXT_NATIVE(delay);
  SWF_TEXT_NATIVE(renderSound);
  SWF_TEXT_NATIVE(updateScroll);
  SWF_TEXT_NATIVE(subtitle);
  SWF_TEXT_NATIVE(subtitleAlign);
  SWF_TEXT_NATIVE(subtitleSourceID);
  SWF_TEXT_NATIVE(subtitleSpeaker);
  SWF_TEXT_NATIVE(scrollIgnoreTime);
  SWF_TEXT_NATIVE(_fontFXMaterialIndex);
  SWF_TEXT_NATIVE_READONLY(_textLength);
#undef SWF_TEXT_NATIVE_READONLY
#undef SWF_TEXT_NATIVE

#define SWF_TEXT_FUNCTION(name)                                                       \
  class idSWFScriptFunction_##name : public idSWFScriptFunction_NestedSWF {           \
  public:                                                                              \
    idSWFScriptVar Call(idSWFScriptObject* object, idSWFParmList& parms) override;     \
  } scriptFunction_##name
  SWF_TEXT_FUNCTION(subtitleSourceCheck);
  SWF_TEXT_FUNCTION(subtitleStart);
  SWF_TEXT_FUNCTION(subtitleLength);
  SWF_TEXT_FUNCTION(killSubtitle);
  SWF_TEXT_FUNCTION(forceKillSubtitle);
  SWF_TEXT_FUNCTION(subLastLine);
  SWF_TEXT_FUNCTION(addSubtitleInfo);
  SWF_TEXT_FUNCTION(terminateSubtitle);
  SWF_TEXT_FUNCTION(clearTimingInfo);
  SWF_TEXT_FUNCTION(getMaxGlyphHeight);
#undef SWF_TEXT_FUNCTION
};

static_assert(sizeof(idSWFScriptObject_TextInstancePrototype) == 2104,
              "text prototype ABI drift");
