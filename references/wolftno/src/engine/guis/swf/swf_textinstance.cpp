#include "engine/guis/swf/swf_textinstance.h"

#include "engine/guis/font.h"
#include "engine/guis/swf/swf.h"
#include "engine/sound/soundshader.h"
#include "shared/idlib/csystems/cvarsystemlocal.h"
#include "shared/idlib/lib.h"
#include "shared/idlib/sys/sys_time.h"

const char* idSWFScriptText::Class() const { return CLASS; }
bool idSWFScriptText::DerivedFrom(const char* className) const {
  return className == CLASS || className == idSWFScriptObject::CLASS;
}

#include "engine/guis/swf/swf_spriteinstance.h"

#include <algorithm>
#include <cmath>

idCVar swf_textScrollSpeed("swf_textScrollSpeed", "80", CVAR_INTEGER, "scroll speed for text");
idCVar swf_textRndLetterSpeed("swf_textRndLetterSpeed", "8", CVAR_INTEGER,
                              "scroll speed for text");
idCVar swf_textRndLetterDelay("swf_textRndLetterDelay", "100", CVAR_INTEGER,
                              "scroll speed for text");
idCVar swf_textParagraphSpeed("swf_textParagraphSpeed", "15", CVAR_INTEGER,
                              "scroll speed for text");
idCVar swf_textParagraphInc("swf_textParagraphInc", "1.3", CVAR_FLOAT,
                            "scroll speed for text");
idCVar swf_subtitleExtraTime("swf_subtitleExtraTime", "3500", CVAR_INTEGER,
                             "time after subtitles vo is complete");
idCVar swf_subtitleEarlyTrans("swf_subtitleEarlyTrans", "3500", CVAR_INTEGER,
                              "early time out to switch the line");
idCVar swf_subtitleLengthGuess("swf_subtitleLengthGuess", "10000", CVAR_INTEGER,
                               "early time out to switch the line");
idCVar swf_textMaxInputLength("swf_textMaxInputLength", "104", CVAR_INTEGER,
                              "max number of characters that can go into the input line");

namespace {
int RandomInt(std::uint32_t& seed, int max) {
  if (max == 0)
    return 0;
  seed = 1664525u * seed + 1013904223u;
  return static_cast<int>((seed >> 10) & 0x7fffu) % max;
}

idSWFTextInstance* TextFromObject(idSWFScriptObject* object) {
  return object != nullptr && object->DerivedFrom(idSWFScriptText::CLASS)
             ? static_cast<idSWFScriptText*>(object)->text
             : nullptr;
}

idSWFScriptObject_TextInstancePrototype& TextInstancePrototype() {
  static idSWFScriptObject_TextInstancePrototype prototype;
  return prototype;
}
} // namespace

idSWFTextInstance::idSWFTextInstance(idSWF* swf_, idSWFEditText* editText_)
    : name(), scriptObject(nullptr), swf(swf_), editText(editText_), parent(nullptr), depth(0),
      numLines(1), forceAutoSize(false), centerTooltip(false), highlight(false),
      recoveredTextPadding{}, text(), randomtext(), variable(), color(), visible(true),
      tooltip(false), recoveredTooltipPadding{}, tooltipScale(0.0f), bindset(0),
      alignment(SWF_ET_ALIGN_LEFT), selectionStart(-1), selectionEnd(-1), scroll(0),
      scrollTime(0), maxscroll(0), forceScroll(false), shiftHeld(false),
      recoveredInputPadding{}, lastInputTime(0), allowNewlineInput(false),
      recoveredFontPadding{}, fontType(-1), textLength(0), lengthCalculated(false),
      recoveredEffectPadding{}, fontFXMaterialIndex(-1), renderMode(0), generatingText(false),
      recoveredRandomPadding{}, rndSpotsVisible(0), rndSpacesVisible(0), charMultiplier(0),
      textSpotsVisible(0), rndTime(0), startRndTime(0), prevReplaceIndex(0),
      triggerGenerate(false), recoveredDelayPadding{}, renderDelay(0), scrollUpdate(false),
      recoveredSoundPadding{}, soundClip(nullptr), needsSoundUpdate(false),
      recoveredIndexPadding{}, indexArray(), randomSeed(0), isSubtitle(false),
      recoveredSubtitlePadding{}, subLength(0), subCharDisplayTime(0), subAlign(0),
      subUpdating(false), recoveredSubtitleIndexPadding{}, subCharStartIndex(0),
      subNextStartIndex(0), subCharEndIndex(0), subDisplayTime(0), subStartTime(0),
      subSourceID(0), subtitleText(), subNeedsSwitch(false), subForceKillQueued(false),
      subForceKill(false), recoveredSubtitleKillPadding(0), subKillTimeDelay(0),
      subSwitchTime(0), subLastWordIndex(0), subPrevLastWordIndex(0),
      recoveredSpeakerPadding(0), subSpeaker(), subWaitClear(false), subInitialLine(true),
      recoveredFinalPadding{}, inputTextStartChar(0), subtitleTimingInfo() {
  indexArray.memTag = 72;
  subtitleTimingInfo.memTag = 72;
}

idSWFTextInstance::~idSWFTextInstance() {
  if (scriptObject != nullptr) {
    scriptObject->text = nullptr;
    scriptObject->Clear();
    scriptObject->Release();
  }
}

void idSWFTextInstance::Init(idSWFSpriteInstance* parent_, const char* name_, int depth_) {
  parent = parent_;
  depth = depth_;
  name.Set(name_ != nullptr ? name_ : "");
  ReInit();
  scriptObject = new idSWFScriptText();
  scriptObject->SetPrototype(&TextInstancePrototype());
  scriptObject->text = this;
  FindFontType();
}

void idSWFTextInstance::ReInit() {
  text = editText != nullptr && editText->initialText.str != nullptr ? editText->initialText.str : "";
  lengthCalculated = false;
  alignment = editText != nullptr ? editText->align : SWF_ET_ALIGN_LEFT;
  variable = editText != nullptr && editText->variable.str != nullptr ? editText->variable.str : "";
  color = editText != nullptr ? editText->color : swfColorRGBA_t();
  selectionStart = selectionEnd = -1;
  visible = true;
  tooltip = false;
  subStartTime = subSourceID = -1;
  scroll = scrollTime = maxscroll = 0;
  shiftHeld = false;
  lastInputTime = 0;
  tooltipScale = 0.0f;
  bindset = 0;
  renderMode = 0;
  generatingText = triggerGenerate = false;
  rndSpotsVisible = rndSpacesVisible = charMultiplier = textSpotsVisible = 0;
  rndTime = startRndTime = prevReplaceIndex = 0;
  scrollUpdate = false;
  soundClip = nullptr;
  needsSoundUpdate = false;
  isSubtitle = subUpdating = subNeedsSwitch = subForceKillQueued = subForceKill = false;
  subLength = subCharDisplayTime = subAlign = 0;
  subCharStartIndex = subNextStartIndex = subCharEndIndex = 0;
  subDisplayTime = subKillTimeDelay = subSwitchTime = 0;
  subLastWordIndex = subPrevLastWordIndex = 0;
  textLength = inputTextStartChar = 0;
  subInitialLine = true;
  renderDelay = swf_textRndLetterDelay.GetInteger();
}

idStr idSWFTextInstance::GetTargetPath() {
  idList<idStr, 5> names;
  names.Append(idStr(name.str != nullptr ? name.str : ""));
  for (idSWFSpriteInstance* current = parent; current != nullptr; current = current->parent)
    names.Insert(idStr(current->name.str != nullptr ? current->name.str : ""), 0);
  idStr result;
  for (int i = 0; i < names.Num(); ++i) {
    if (i != 0)
      result.Append("/");
    result.Append(names[i]);
  }
  return result;
}

void idSWFTextInstance::SetTextEffect(swfFontEffects_t type) {
  if (type == SWF_FONT_FX_INVALID) {
    fontFXMaterialIndex = SWF_FONT_FX_INVALID;
    return;
  }
  if (fontType == SWF_FONT_INVALID || swf == nullptr)
    return;
  fontFXMaterialIndex = swf->fontEffects[SWF_MAX_FONT_FX * fontType + type];
}

void idSWFTextInstance::StartRandomText(int time) {
  generatingText = true;
  textSpotsVisible = 0;
  randomtext = "";
  triggerGenerate = false;
  startRndTime = rndTime = time;
  randomSeed = static_cast<std::uint32_t>(time);
  rndSpotsVisible = 0;
  text = idLocalization::languageDict.GetString(text.data);
  lengthCalculated = false;
  for (int i = 0; i < text.Length(); ++i) {
    if (text[i] == ' ')
      randomtext.Append(' ');
    else {
      randomtext.Append('.');
      ++rndSpotsVisible;
    }
  }
}

idStr idSWFTextInstance::GetRandomText(int time) {
  if (triggerGenerate || time - startRndTime < renderDelay)
    return idStr(" ");
  if (!generatingText)
    return text;

  if (rndSpotsVisible > 0) {
    int wait = swf_textRndLetterSpeed.GetInteger();
    if (randomtext.Length() >= 10)
      wait /= 3;
    if (time - rndTime >= wait) {
      rndTime = time;
      const int spot = RandomInt(randomSeed, rndSpotsVisible);
      int seen = 0;
      for (int i = 0; i < randomtext.Length(); ++i) {
        if (i >= text.Length()) {
          rndSpotsVisible = 0;
          break;
        }
        if (randomtext[i] == '.')
          ++seen;
        if (seen == spot) {
          const bool caps = (i > 0 && text[i - 1] == ' ') || i == 0 ||
                            renderMode == SWF_TEXT_RENDER_RANDOM_APPEAR_CAPS;
          randomtext[i] = static_cast<char>(RandomInt(randomSeed, 26) + (caps ? 'A' : 'a'));
          --rndSpotsVisible;
          if (soundClip != nullptr)
            needsSoundUpdate = true;
          break;
        }
      }
    }
  } else if (rndSpotsVisible == 0 && textSpotsVisible < text.Length()) {
    if (textSpotsVisible >= randomtext.Length()) {
      ++textSpotsVisible;
    } else if (time - rndTime >= swf_textRndLetterSpeed.GetInteger()) {
      rndTime = time;
      randomtext[textSpotsVisible] = text[textSpotsVisible];
      ++textSpotsVisible;
      if (soundClip != nullptr)
        needsSoundUpdate = true;
    }
  } else {
    return idStr(" ");
  }
  if (rndSpotsVisible == 0 && textSpotsVisible == text.Length())
    generatingText = false;
  return randomtext;
}

void idSWFTextInstance::StartParagraphText(int time) {
  generatingText = true;
  textSpotsVisible = 0;
  randomtext = "";
  triggerGenerate = false;
  startRndTime = rndTime = time;
  randomSeed = static_cast<std::uint32_t>(time);
  prevReplaceIndex = 0;
  rndSpotsVisible = text.Length();
  indexArray.Clear();
  charMultiplier = 0;
  text = idLocalization::languageDict.GetString(text.data);
  lengthCalculated = false;
  for (int i = 0; i < text.Length(); ++i) {
    randomtext.Append(' ');
    indexArray.Append(i);
  }
  for (int i = 0; i < indexArray.Num(); ++i) {
    const int swap = RandomInt(randomSeed, indexArray.Num());
    std::swap(indexArray[i], indexArray[swap]);
  }
}

idStr idSWFTextInstance::GetParagraphText(int time) {
  if (triggerGenerate || time - startRndTime < renderDelay)
    return idStr(" ");
  if (!generatingText) {
    scrollUpdate = false;
    return text;
  }
  if (rndSpotsVisible != 0 && time - rndTime >= swf_textParagraphSpeed.GetInteger()) {
    rndTime = time;
    const int target = static_cast<int>(std::pow(swf_textParagraphInc.GetFloat(),
                                                 static_cast<float>(charMultiplier)));
    while (prevReplaceIndex < target) {
      if (prevReplaceIndex >= indexArray.Num()) {
        generatingText = false;
        return text;
      }
      const int index = indexArray[prevReplaceIndex];
      if (index >= randomtext.Length() || index >= text.Length()) {
        rndSpotsVisible = 0;
        break;
      }
      randomtext[index] = text[index];
      ++prevReplaceIndex;
    }
    ++charMultiplier;
  }
  return randomtext;
}

bool idSWFTextInstance::NeedsSoundPlayed() { return soundClip != nullptr && needsSoundUpdate; }
void idSWFTextInstance::SwitchSubtitleText(int) { subNeedsSwitch = false; }
bool idSWFTextInstance::UpdateSubtitle(int time) {
  if (subForceKillQueued) {
    subForceKillQueued = false;
    subForceKill = true;
    subKillTimeDelay = time + swf_subtitleExtraTime.GetInteger();
  }
  if (subUpdating && !subForceKill &&
      ((time >= subSwitchTime && !subNeedsSwitch) || (!subNeedsSwitch && subInitialLine))) {
    if (subInitialLine && subtitleTimingInfo.Num() > 0) {
      if (subStartTime == -1)
        subStartTime = time - 600;
      if (time < subStartTime + subtitleTimingInfo[0].startTime)
        return true;
      text = subtitleText;
      subInitialLine = false;
    }
    if (subNextStartIndex + 1 < text.Length()) {
      subCharStartIndex = subNextStartIndex;
      subNeedsSwitch = true;
    } else {
      subForceKillQueued = true;
    }
  }
  if (subForceKill && time >= subKillTimeDelay) {
    subForceKill = false;
    return false;
  }
  return true;
}

void idSWFTextInstance::SetSubEndIndex(int endChar, int time) {
  subCharEndIndex = endChar;
  if (subCharEndIndex + 1 >= text.Length())
    LastWordChanged(subtitleTimingInfo.Num(), time);
}

void idSWFTextInstance::LastWordChanged(int wordCount, int time) {
  const int count = subtitleTimingInfo.Num();
  subLastWordIndex = subPrevLastWordIndex + wordCount >= count
                         ? count - 1
                         : subPrevLastWordIndex + wordCount - 1;
  if (subStartTime == -1)
    subStartTime = count > 0 ? time + subtitleTimingInfo[0].startTime : time;
  if (subLastWordIndex >= 0 && subLastWordIndex < count)
    subSwitchTime = subStartTime + subtitleTimingInfo[subLastWordIndex].startTime;
}

void idSWFTextInstance::SetSubNextStartIndex(int value) { subNextStartIndex = value; }
int idSWFTextInstance::GetApproximateSubtitleBreak(int time) {
  if (subStartTime == -1)
    subStartTime = time;
  if (time < subSwitchTime)
    return subLastWordIndex;

  const int count = subtitleTimingInfo.Num();
  subPrevLastWordIndex = subLastWordIndex;
  const int first = subLastWordIndex;
  for (int i = first; i < count; ++i) {
    if (subtitleTimingInfo[i].forceBreak) {
      const int next = i + 1 < count ? i + 1 : i;
      subSwitchTime = subStartTime + subtitleTimingInfo[next].startTime;
      subLastWordIndex = i;
      return i;
    }
    if (subtitleTimingInfo[i].startTime - subtitleTimingInfo[first].startTime >
        swf_subtitleLengthGuess.GetInteger()) {
      subSwitchTime = subStartTime + subtitleTimingInfo[i].startTime;
      subLastWordIndex = i - 1 >= 0 ? i - 1 : i;
      return subLastWordIndex;
    }
  }
  if (count > 0) {
    subSwitchTime = subStartTime + subtitleTimingInfo[count - 1].startTime;
    subLastWordIndex = count;
  }
  return subLastWordIndex;
}

void idSWFTextInstance::SubtitleComplete() {
  subInitialLine = true;
  subUpdating = false;
  isSubtitle = false;
  subStartTime = -1;
  subCharDisplayTime = 0;
  subNeedsSwitch = false;
  subForceKillQueued = false;
  subForceKill = false;
  subKillTimeDelay = subSwitchTime = 0;
  subLastWordIndex = subPrevLastWordIndex = 0;
  subSpeaker = "";
  subtitleText = "";
  text = "";
  subtitleTimingInfo.Clear();
}

void idSWFTextInstance::SubtitleCleanup() { subSourceID = subAlign = -1; text = ""; }
int idSWFTextInstance::GetNumLines() {
  if (editText != nullptr && (editText->flags & SWF_ET_MULTILINE) == 0)
    maxscroll = 0;
  return numLines;
}

float idSWFTextInstance::GetTextLength() {
  if (lengthCalculated && variable.IsEmpty())
    return static_cast<float>(textLength);

  idStr measured;
  if (variable.IsEmpty()) {
    measured = idLocalization::languageDict.GetString(text.data);
  } else {
    idSWFScriptVar value = swf != nullptr && swf->globals != nullptr
                               ? swf->globals->Get(variable.data)
                               : idSWFScriptVar();
    measured = value.IsUndefined() ? text : value.ToString();
    measured = idLocalization::languageDict.GetString(measured.data);
  }

  float length = 0.0f;
  if (swf != nullptr && editText != nullptr) {
    idSWFDictionaryEntry* entry = swf->FindDictionaryEntry(editText->fontID, SWF_DICT_FONT);
    idFont* font = entry != nullptr && entry->font != nullptr ? entry->font->fontID : nullptr;
    if (font != nullptr && font->GetPointSize() != 0) {
      const float width = std::fabs(editText->bounds.br.x - editText->bounds.tl.x);
      const float scale = (static_cast<float>(editText->fontHeight) * 0.05f) /
                          static_cast<float>(font->GetPointSize());
      int index = 0;
      while (index < measured.Length()) {
        const std::uint32_t code = measured.UTF8Char(&index);
        const glyphInfo_t& glyph = font->GetGlyph(code);
        if (code == '^' && index < measured.Length() && idStr::IsColor(&measured[index])) {
          ++index;
          continue;
        }
        length += static_cast<float>(glyph.xSkip) * scale;
        if ((editText->flags & SWF_ET_AUTOSIZE) == 0 && length >= width) {
          length = width;
          break;
        }
      }
    }
  }
  lengthCalculated = true;
  textLength = static_cast<int>(length);
  return static_cast<float>(textLength);
}

float idSWFTextInstance::GetMaxGlyphHeight() {
  if (swf == nullptr || editText == nullptr)
    return 0.0f;
  idSWFDictionaryEntry* entry = swf->FindDictionaryEntry(editText->fontID, SWF_DICT_FONT);
  idFont* font = entry != nullptr && entry->font != nullptr ? entry->font->fontID : nullptr;
  if (font == nullptr || font->GetPointSize() == 0)
    return 0.0f;
  const float scale = (static_cast<float>(editText->fontHeight) * 0.05f) /
                      static_cast<float>(font->GetPointSize());
  return static_cast<float>(font->GetAscender() - font->GetDescender()) * scale;
}

void idSWFTextInstance::InputCharacter(char letter) {
  if (selectionStart != selectionEnd) {
    const int start = std::min(selectionStart, selectionEnd);
    const int end = std::max(selectionStart, selectionEnd);
    const idStr left = text.Left(std::max(start, 0));
    const idStr right = text.Right(std::max(text.Length() - end, 0));
    text = left;
    text.Append(letter);
    text.Append(right);
    selectionStart = start + 1;
  } else if (selectionStart < swf_textMaxInputLength.GetInteger()) {
    const int insertAt = std::max(selectionStart, 0);
    text.Insert(letter, insertAt);
    selectionStart = insertAt + 1;
  }
  selectionEnd = selectionStart;
  lastInputTime = idLib::frameNumber;
}

bool idSWFTextInstance::CanBreakLine(std::uint32_t tc, std::uint32_t nexttc) {
  if (tc == 32 || tc == 13 || tc == 10 || tc == 45)
    return true;
  if (tc <= 0x24f || (tc >= 1024 && tc <= 0x52f))
    return false;
  if (nexttc > 0x31ff)
    return true;
  if (nexttc < 0x31f0) {
    switch (nexttc) {
      case 0x3001: case 0x3002: case 0x3005: case 0x303b:
      case 0x3041: case 0x3043: case 0x3045: case 0x3047: case 0x3049:
      case 0x3063: case 0x3083: case 0x3085: case 0x3087: case 0x308e:
      case 0x3095: case 0x3096: case 0x30a1: case 0x30a3: case 0x30a5:
      case 0x30a7: case 0x30a9: case 0x30c3: case 0x30e3: case 0x30e5:
      case 0x30e7: case 0x30ee: case 0x30f5: case 0x30f6: case 0x30fc:
      case 0x30fd: case 0x30fe: return false;
      default: return true;
    }
  }
  return false;
}

idColor idSWFTextInstance::GetColor() {
  return idColor{color.r / 255.0f, color.g / 255.0f, color.b / 255.0f, color.a / 255.0f};
}

void idSWFTextInstance::SetColor(idColor& value) {
  color.r = static_cast<std::uint8_t>(std::clamp(value.r, 0.0f, 1.0f) * 255.0f);
  color.g = static_cast<std::uint8_t>(std::clamp(value.g, 0.0f, 1.0f) * 255.0f);
  color.b = static_cast<std::uint8_t>(std::clamp(value.b, 0.0f, 1.0f) * 255.0f);
  color.a = static_cast<std::uint8_t>(std::clamp(value.a, 0.0f, 1.0f) * 255.0f);
}

void idSWFTextInstance::FindFontType() {
  const char* language = cvarSystem != nullptr ? cvarSystem->GetCVarString("sys_lang", "english")
                                               : "english";
  const bool japanese = cvarSystem != nullptr
                            ? cvarSystem->GetCVarBool("sys_langJapanese",
                                                     idStr::Icmp(language, "japanese") == 0)
                            : idStr::Icmp(language, "japanese") == 0;
  if (japanese) {
    fontType = SWF_FONT_JAPANESE;
    return;
  }
  if (idStr::Icmp(language, "russian") == 0) {
    fontType = SWF_FONT_CYRILLIC;
    return;
  }
  fontType = SWF_FONT_INVALID;
  if (swf == nullptr || editText == nullptr)
    return;
  idSWFDictionaryEntry* entry = swf->FindDictionaryEntry(editText->fontID, SWF_DICT_FONT);
  const char* fontName = entry != nullptr && entry->font != nullptr && entry->font->fontID != nullptr
                             ? entry->font->fontID->name.str
                             : nullptr;
  if (fontName == nullptr)
    return;
  struct FontEntry { const char* name; swfUsedFonts_t type; };
  static constexpr FontEntry fontTypes[] = {
      {"futura com medium", SWF_FONT_FUTURA_COM_MEDIUM},
      {"futura com book", SWF_FONT_FUTURA_COM_BOLD},
      {"arial", SWF_FONT_ARIAL},
      {"japanese", SWF_FONT_JAPANESE},
      {"cyrillic", SWF_FONT_CYRILLIC},
  };
  for (const FontEntry& candidate : fontTypes) {
    if (idStr::Icmp(fontName, candidate.name) == 0) {
      fontType = candidate.type;
      return;
    }
  }
}

idSWFScriptObject_TextInstancePrototype::idSWFScriptObject_TextInstancePrototype() {
#define SET_FUNCTION(name) Set(#name, idSWFScriptVar(&scriptFunction_##name))
  SET_FUNCTION(onKey);
  SET_FUNCTION(onChar);
  SET_FUNCTION(generateRnd);
  SET_FUNCTION(calcNumLines);
  SET_FUNCTION(clearTimingInfo);
  SET_FUNCTION(toString);
  SET_FUNCTION(getMaxGlyphHeight);
  SET_FUNCTION(subtitleSourceCheck);
  SET_FUNCTION(subtitleStart);
  SET_FUNCTION(subtitleLength);
  SET_FUNCTION(killSubtitle);
  SET_FUNCTION(forceKillSubtitle);
  SET_FUNCTION(subLastLine);
  SET_FUNCTION(addSubtitleInfo);
  SET_FUNCTION(terminateSubtitle);
#undef SET_FUNCTION
#define SET_NATIVE(name) SetNative(#name, &swfScriptVar_##name)
  SET_NATIVE(text);
  SET_NATIVE(_textLength);
  SET_NATIVE(autoSize);
  SET_NATIVE(variable);
  SET_NATIVE(_alpha);
  SET_NATIVE(textColor);
  SET_NATIVE(_visible);
  SET_NATIVE(selectionStart);
  SET_NATIVE(selectionEnd);
  SET_NATIVE(scroll);
  SET_NATIVE(maxscroll);
  SET_NATIVE(isTooltip);
  SET_NATIVE(tooltipScale);
  SET_NATIVE(tooltipBindset);
  SET_NATIVE(mode);
  SET_NATIVE(delay);
  SET_NATIVE(renderSound);
  SET_NATIVE(updateScroll);
  SET_NATIVE(subtitle);
  SET_NATIVE(subtitleAlign);
  SET_NATIVE(subtitleSourceID);
  SET_NATIVE(subtitleSpeaker);
  SET_NATIVE(scrollIgnoreTime);
  SET_NATIVE(_fontFXMaterialIndex);
#undef SET_NATIVE
}

#define TEXT_GET(name)                                                                \
  idSWFScriptVar idSWFScriptObject_TextInstancePrototype::idSWFScriptNativeVar_##name::Get( \
      idSWFScriptObject* object)
#define TEXT_SET(name)                                                                \
  void idSWFScriptObject_TextInstancePrototype::idSWFScriptNativeVar_##name::Set(      \
      idSWFScriptObject* object, const idSWFScriptVar& value)
#define TEXT_CALL(name)                                                               \
  idSWFScriptVar idSWFScriptObject_TextInstancePrototype::idSWFScriptFunction_##name::Call( \
      idSWFScriptObject* object, idSWFParmList& parms)

TEXT_GET(text) { auto* t = TextFromObject(object); return t ? idSWFScriptVar(t->text) : idSWFScriptVar(); }
TEXT_SET(text) {
  auto* t = TextFromObject(object); if (!t) return;
  const idStr string = value.ToString();
  t->text = idLocalization::languageDict.GetString(string.data);
  if (t->text.IsEmpty()) {
    t->selectionStart = t->selectionEnd = -1;
    t->inputTextStartChar = 0;
  }
  t->lengthCalculated = false;
}
TEXT_GET(autoSize) { auto* t = TextFromObject(object); return t ? idSWFScriptVar(t->forceAutoSize) : idSWFScriptVar(); }
TEXT_SET(autoSize) { auto* t = TextFromObject(object); if (t) { t->forceAutoSize = value.ToBool(); t->lengthCalculated = false; } }
TEXT_GET(scrollIgnoreTime) { auto* t = TextFromObject(object); return t ? idSWFScriptVar(t->forceScroll) : idSWFScriptVar(); }
TEXT_SET(scrollIgnoreTime) { auto* t = TextFromObject(object); if (t) t->forceScroll = value.ToBool(); }
TEXT_GET(variable) { auto* t = TextFromObject(object); return t ? idSWFScriptVar(t->variable) : idSWFScriptVar(); }
TEXT_SET(variable) { auto* t = TextFromObject(object); if (t) t->variable = value.ToString(); }
TEXT_GET(_alpha) { auto* t = TextFromObject(object); return t ? idSWFScriptVar(t->color.a / 255.0f) : idSWFScriptVar(); }
TEXT_SET(_alpha) {
  auto* t = TextFromObject(object); if (!t) return;
  t->color.a = static_cast<std::uint8_t>(std::clamp(value.ToNumber(), 0.0, 1.0) * 255.0);
}
TEXT_GET(_visible) { auto* t = TextFromObject(object); return t ? idSWFScriptVar(t->visible) : idSWFScriptVar(); }
TEXT_SET(_visible) { auto* t = TextFromObject(object); if (t) t->visible = value.ToBool(); }
TEXT_GET(selectionStart) { auto* t = TextFromObject(object); return t ? idSWFScriptVar(t->selectionStart) : idSWFScriptVar(); }
TEXT_SET(selectionStart) { auto* t = TextFromObject(object); if (t) t->selectionStart = static_cast<int>(value.ToInteger()); }
TEXT_GET(selectionEnd) { auto* t = TextFromObject(object); return t ? idSWFScriptVar(t->selectionEnd) : idSWFScriptVar(); }
TEXT_SET(selectionEnd) { auto* t = TextFromObject(object); if (t) t->selectionEnd = static_cast<int>(value.ToInteger()); }
TEXT_GET(isTooltip) { auto* t = TextFromObject(object); return t ? idSWFScriptVar(t->tooltip) : idSWFScriptVar(); }
TEXT_SET(isTooltip) { auto* t = TextFromObject(object); if (t) t->tooltip = value.ToBool(); }
TEXT_GET(tooltipScale) { auto* t = TextFromObject(object); return t ? idSWFScriptVar(t->tooltipScale) : idSWFScriptVar(); }
TEXT_SET(tooltipScale) { auto* t = TextFromObject(object); if (t) t->tooltipScale = static_cast<float>(value.ToNumber()); }
TEXT_GET(tooltipBindset) { auto* t = TextFromObject(object); return t ? idSWFScriptVar(t->bindset) : idSWFScriptVar(); }
TEXT_SET(tooltipBindset) { auto* t = TextFromObject(object); if (t) t->bindset = static_cast<int>(value.ToInteger()); }
TEXT_GET(_fontFXMaterialIndex) { auto* t = TextFromObject(object); return t ? idSWFScriptVar(t->fontFXMaterialIndex) : idSWFScriptVar(); }
TEXT_SET(_fontFXMaterialIndex) { auto* t = TextFromObject(object); if (t) t->fontFXMaterialIndex = static_cast<int>(value.ToInteger()); }
TEXT_GET(delay) { auto* t = TextFromObject(object); return t ? idSWFScriptVar(t->renderDelay) : idSWFScriptVar(); }
TEXT_SET(delay) { auto* t = TextFromObject(object); if (t) t->renderDelay = static_cast<int>(value.ToInteger()); }
TEXT_GET(updateScroll) { auto* t = TextFromObject(object); return t ? idSWFScriptVar(t->scrollUpdate) : idSWFScriptVar(); }
TEXT_SET(updateScroll) { auto* t = TextFromObject(object); if (t) t->scrollUpdate = value.ToBool(); }
TEXT_GET(mode) { auto* t = TextFromObject(object); return t ? idSWFScriptVar(t->renderMode) : idSWFScriptVar(); }
TEXT_SET(mode) {
  auto* t = TextFromObject(object); if (!t) return;
  unsigned mode = static_cast<unsigned>(value.ToInteger());
  t->renderMode = mode < SWF_TEXT_RENDER_MODE_COUNT ? static_cast<int>(mode) : SWF_TEXT_RENDER_NORMAL;
}
TEXT_GET(scroll) { auto* t = TextFromObject(object); return t ? idSWFScriptVar(t->scroll) : idSWFScriptVar(); }
TEXT_SET(scroll) {
  auto* t = TextFromObject(object); if (!t) return;
  const int time = t->swf != nullptr ? t->swf->lastRenderTime : static_cast<int>(Sys_Milliseconds());
  if (t->forceScroll || time >= t->scrollTime) {
    t->scrollTime = time + swf_textScrollSpeed.GetInteger();
    t->scroll = static_cast<int>(value.ToInteger());
  }
}
TEXT_GET(maxscroll) { auto* t = TextFromObject(object); return t ? idSWFScriptVar(t->maxscroll) : idSWFScriptVar(); }
TEXT_SET(maxscroll) { auto* t = TextFromObject(object); if (t) t->maxscroll = static_cast<int>(value.ToInteger()); }
TEXT_GET(textColor) {
  auto* t = TextFromObject(object); if (!t) return idSWFScriptVar();
  return idSWFScriptVar((static_cast<int>(t->color.r) << 16) |
                        (static_cast<int>(t->color.g) << 8) | t->color.b);
}
TEXT_SET(textColor) {
  auto* t = TextFromObject(object); if (!t) return;
  const int rgb = static_cast<int>(value.ToInteger());
  t->color.r = static_cast<std::uint8_t>((rgb >> 16) & 255);
  t->color.g = static_cast<std::uint8_t>((rgb >> 8) & 255);
  t->color.b = static_cast<std::uint8_t>(rgb & 255);
}
TEXT_GET(_textLength) { auto* t = TextFromObject(object); return t ? idSWFScriptVar(t->GetTextLength()) : idSWFScriptVar(); }
TEXT_GET(renderSound) {
  auto* t = TextFromObject(object);
  return t ? idSWFScriptVar(t->soundClip != nullptr ? t->soundClip->name.str : "") : idSWFScriptVar();
}
TEXT_SET(renderSound) {
  auto* t = TextFromObject(object); if (!t) return;
  const idStr soundName = value.ToString();
  t->soundClip = soundName.IsEmpty() ? nullptr : static_cast<idSoundShader*>(
      idSoundShader::resourceList.FindWithInheritance(soundName.data, true, true));
}
TEXT_GET(subtitle) { auto* t = TextFromObject(object); return t ? idSWFScriptVar(t->isSubtitle) : idSWFScriptVar(); }
TEXT_SET(subtitle) { auto* t = TextFromObject(object); if (t) t->isSubtitle = value.ToBool(); }
TEXT_GET(subtitleAlign) { auto* t = TextFromObject(object); return t ? idSWFScriptVar(t->subAlign) : idSWFScriptVar(); }
TEXT_SET(subtitleAlign) { auto* t = TextFromObject(object); if (t) t->subAlign = static_cast<int>(value.ToInteger()); }
TEXT_GET(subtitleSourceID) { auto* t = TextFromObject(object); return t ? idSWFScriptVar(t->subSourceID) : idSWFScriptVar(); }
TEXT_SET(subtitleSourceID) { auto* t = TextFromObject(object); if (t) t->subSourceID = static_cast<int>(value.ToInteger()); }
TEXT_GET(subtitleSpeaker) { auto* t = TextFromObject(object); return t ? idSWFScriptVar(t->subSpeaker) : idSWFScriptVar(); }
TEXT_SET(subtitleSpeaker) { auto* t = TextFromObject(object); if (t) t->subSpeaker = value.ToString(); }

TEXT_CALL(generateRnd) {
  auto* t = TextFromObject(object); if (t) { t->triggerGenerate = true; t->rndSpotsVisible = -1; t->generatingText = false; }
  return idSWFScriptVar();
}
TEXT_CALL(calcNumLines) { auto* t = TextFromObject(object); return t ? idSWFScriptVar(t->GetNumLines()) : idSWFScriptVar(); }
TEXT_CALL(getMaxGlyphHeight) { auto* t = TextFromObject(object); return t ? idSWFScriptVar(t->GetMaxGlyphHeight()) : idSWFScriptVar(); }
TEXT_CALL(clearTimingInfo) { auto* t = TextFromObject(object); if (t) t->subtitleTimingInfo.Clear(); return idSWFScriptVar(); }
TEXT_CALL(toString) {
  auto* t = TextFromObject(object); if (!t) return idSWFScriptVar();
  idStr path = t->GetTargetPath(); path.ReplaceChar('/', '.'); return idSWFScriptVar(path);
}
TEXT_CALL(onChar) {
  auto* t = TextFromObject(object); if (!t || parms.Num() < 1) return idSWFScriptVar();
  const int key = static_cast<int>(parms[0].ToInteger());
  if (((key < 32 || key > 126) && key != 13) || key == '`' || key == '~')
    return idSWFScriptVar(false);
  t->InputCharacter(static_cast<char>(key));
  return idSWFScriptVar(true);
}
TEXT_CALL(onKey) {
  auto* t = TextFromObject(object); if (!t || parms.Num() < 2) return idSWFScriptVar();
  const int key = static_cast<int>(parms[0].ToInteger());
  const bool down = parms[1].ToBool();
  if (down) {
    if (key == 1) return idSWFScriptVar(false);
    if (key == 42 || key == 54) t->shiftHeld = true;
    else if (key == 14 || key == 211) {
      if (t->selectionStart == t->selectionEnd) {
        if (key == 14) t->selectionStart = t->selectionEnd - 1;
        else t->selectionEnd = t->selectionStart + 1;
      }
      const int start = std::min(t->selectionStart, t->selectionEnd);
      const int end = std::max(t->selectionStart, t->selectionEnd);
      t->text = t->text.Left(std::max(start, 0)) + t->text.Right(std::max(t->text.Length() - end, 0));
      t->selectionStart = t->selectionEnd = start;
    } else if (key == 28) {
      if (!t->allowNewlineInput) return idSWFScriptVar(false);
      t->InputCharacter('\n');
      return idSWFScriptVar(true);
    } else if (key == 199) {
      t->selectionEnd = 0; if (!t->shiftHeld) t->selectionStart = 0;
    } else if (key == 203 && t->selectionEnd > 0) {
      --t->selectionEnd; if (!t->shiftHeld) t->selectionStart = t->selectionEnd;
    } else if (key == 205 && t->selectionEnd < t->text.Length()) {
      ++t->selectionEnd; if (!t->shiftHeld) t->selectionStart = t->selectionEnd;
    } else if (key == 207) {
      t->selectionEnd = t->text.Length(); if (!t->shiftHeld) t->selectionStart = t->selectionEnd;
    }
  } else if (key == 42 || key == 54) {
    t->shiftHeld = false;
  }
  t->lastInputTime = idLib::frameNumber;
  return idSWFScriptVar(true);
}

TEXT_CALL(subtitleLength) { auto* t = TextFromObject(object); if (t && parms.Num()) t->subLength = static_cast<int>(parms[0].ToInteger()); return idSWFScriptVar(); }
TEXT_CALL(subtitleStart) {
  auto* t = TextFromObject(object); if (!t) return idSWFScriptVar();
  t->subUpdating = true; t->subNeedsSwitch = false; t->subForceKillQueued = false;
  t->subForceKill = false; t->subKillTimeDelay = 0; t->subtitleText = t->text; t->text = "";
  t->subCharStartIndex = t->subNextStartIndex = t->subCharEndIndex = 0;
  t->subSwitchTime = t->subLastWordIndex = t->subPrevLastWordIndex = 0;
  t->subStartTime = -1; t->subInitialLine = true; return idSWFScriptVar();
}
TEXT_CALL(forceKillSubtitle) { auto* t = TextFromObject(object); if (t) { t->subForceKill = true; t->subKillTimeDelay = 0; } return idSWFScriptVar(); }
TEXT_CALL(killSubtitle) { auto* t = TextFromObject(object); if (t) t->subForceKillQueued = true; return idSWFScriptVar(); }
TEXT_CALL(subtitleSourceCheck) {
  auto* t = TextFromObject(object); if (!t || parms.Num() < 1) return idSWFScriptVar();
  const int source = static_cast<int>(parms[0].ToInteger());
  if (t->subSourceID == -1) { t->subSourceID = source; return idSWFScriptVar(1); }
  if (t->subSourceID == source) { t->SubtitleComplete(); t->subSourceID = source; return idSWFScriptVar(-1); }
  return idSWFScriptVar(0);
}
TEXT_CALL(terminateSubtitle) { auto* t = TextFromObject(object); if (t) { t->SubtitleComplete(); t->SubtitleCleanup(); } return idSWFScriptVar(); }
TEXT_CALL(subLastLine) {
  auto* t = TextFromObject(object); if (!t) return idSWFScriptVar();
  idStr line; line.CopyRange(t->text.data, t->subCharStartIndex, t->subCharEndIndex); return idSWFScriptVar(line);
}
TEXT_CALL(addSubtitleInfo) {
  auto* t = TextFromObject(object); if (!t || parms.Num() != 3) return idSWFScriptVar();
  subTimingWordData_t info; info.phrase = parms[0].ToString();
  info.startTime = static_cast<int>(parms[1].ToInteger()); info.forceBreak = parms[2].ToBool();
  t->subtitleTimingInfo.Append(info); return idSWFScriptVar();
}

#undef TEXT_CALL
#undef TEXT_SET
#undef TEXT_GET
