#include "engine/framework/subtitles.h"

#ifndef WOLF_SUBTITLES_CORE_ONLY
#include "engine/framework/common.h"
#include "engine/framework/playerprofile.h"
#include "engine/framework/versionselector.h"
#include "engine/guis/swf/swf.h"
#include "engine/guis/swf/swf_spriteinstance.h"
#include "engine/guis/swf/swf_textinstance.h"
#include "engine/renderer/renderersystem.h"
#endif
#include "engine/sys/sys_public.h"
#include "shared/idlib/csystems/cvarsystemlocal.h"
#include "shared/idlib/langdict.h"
#include "shared/idlib/lib.h"
#include "shared/idlib/sys/sys_time.h"

#include <algorithm>
#include <cstdlib>
#include <cstring>

#ifndef WOLF_SUBTITLES_CORE_ONLY
extern idCVar in_joystickButtons;
#endif
extern idCVar sub_debug;

const char* idSubtitles::languagesIsoNames[13] = {
    "nul", "eng", "fre", "ita", "ger", "spa", "rus",
    "pol", "cze", "jpn", "heb", "afr", "vis"};

int idSubtitles::GetSystemLanguage() {
  struct language_t {
    const char* name;
    int id;
  };
  static const language_t languages[] = {
      {"english", 1}, {"french", 2},  {"italian", 3},
      {"german", 4},  {"spanish", 5}, {"russian", 6},
      {"polish", 7},  {"japanese", 9}, {nullptr, 0}};
  const char* systemLanguage = sys_lang.valueString.data;
  if (systemLanguage == nullptr || systemLanguage[0] == '\0')
    return 0;
  for (int index = 0; languages[index].name != nullptr; ++index) {
    if (idStr::Icmp(systemLanguage, languages[index].name) == 0)
      return languages[index].id;
  }
  return 0;
}

const char* idSubtitles::GetLanguageIsoName(int language) {
  return language >= 0 && language < 13 ? languagesIsoNames[language]
                                        : languagesIsoNames[0];
}

bool idSubtitles::StripTags(const char* text, idStr* result) {
  if (text == nullptr || result == nullptr)
    return false;
  result->Clear();
  for (const char* cursor = text; *cursor != '\0'; ++cursor) {
    if (*cursor != '[') {
      *result += *cursor;
      continue;
    }
    const char* close = std::strchr(cursor + 1, ']');
    if (close == nullptr)
      return false;
    cursor = close;
  }
  return true;
}

bool idSubtitles::ProcessText(const char* text, float endTime,
                              idList<idPart, 5>* parts) {
  int reserveCount = 2;
  for (const char* cursor = text; *cursor != '\0'; ++cursor) {
    if (*cursor == '[')
      ++reserveCount;
  }
  parts->SetNum(reserveCount);
  parts->Clear();

  idPart part;
  bool timeWasSet = false;
  const char* cursor = text;
  while (*cursor != '\0') {
    if (*cursor == '[') {
      const char* tagStart = ++cursor;
      while (*cursor != '\0' && *cursor != ' ' && *cursor != ']')
        ++cursor;
      if (*cursor == '\0')
        return false;
      idStrStatic<512> tag;
      tag.CopyRange(tagStart, 0, static_cast<int>(cursor - tagStart));
      if (tag.IsNumeric()) {
        if (timeWasSet || part.phrase.Length() != 0) {
          parts->Append(part);
          part.phrase = "";
        }
        part.startTime = static_cast<float>(std::atof(tag.data));
        timeWasSet = true;
      } else {
        int language = 0;
        for (; language < 13; ++language) {
          if (tag.Cmp(languagesIsoNames[language]) == 0)
            break;
        }
        if (language < 13 && part.language != language) {
          if (part.phrase.Length() != 0) {
            parts->Append(part);
            part.phrase = "";
            timeWasSet = false;
          }
          part.language = language;
        }
        if (tag.Length() == 2 && tag[0] == 'p' && tag[1] >= '0' &&
            tag[1] <= '9')
          part.priority = tag[1] - '0';
      }
    } else {
      part.phrase += *cursor;
    }
    ++cursor;
  }
  if (timeWasSet || part.phrase.Length() != 0)
    parts->Append(part);
  if (part.phrase.Length() != 0) {
    idPart endPart;
    endPart.startTime = endTime <= part.startTime ? part.startTime + 2.0f : endTime;
    parts->Append(endPart);
  }
  if (sub_debug.valueInteger != 0) {
    idLib::Printf("Subtitles: %s\n", text);
    for (int index = 0; index < parts->Num(); ++index) {
      const idPart& value = (*parts)[index];
      idLib::Printf("#%d %f %d %d: %s\n", index, value.startTime,
                    value.priority, value.language, value.phrase.data);
    }
  }
  return true;
}

idSubtitleSequencer::idSubtitleSequencer()
    : parts(), active(false), current(-1), highestPriority(0) {}

void idSubtitleSequencer::Start(const char* text, float endTime) {
  current = -1;
  active = true;
  idSubtitles::ProcessText(text, endTime, &parts);
  highestPriority = 0;
  for (int index = 0; index < parts.Num(); ++index)
    highestPriority = (std::max)(highestPriority, parts[index].priority);
}

void idSubtitleSequencer::Update(float newTime) {
  if (!active)
    return;
  const int next = current + 1;
  if (next < parts.Num()) {
    int nextDifferent = next;
    while (current != -1 && nextDifferent < parts.Num() &&
           parts[current].startTime == parts[nextDifferent].startTime)
      ++nextDifferent;
    if (nextDifferent < parts.Num() &&
        newTime >= parts[nextDifferent].startTime)
      current = next;
  }
  if (current >= parts.Num() - 1)
    active = false;
}

idStr idSubtitleSequencer::GetCurrentString(unsigned languageMask) const {
  if (current == -1 || current >= parts.Num())
    return idStr("");
  idStr text;
  for (int index = current; index < parts.Num(); ++index) {
    if (parts[index].startTime != parts[current].startTime)
      break;
    if ((languageMask & (1u << parts[index].language)) != 0)
      text += parts[index].phrase;
  }
  return text;
}

#ifndef WOLF_SUBTITLES_CORE_ONLY
idSubtitles_VideoOverlay::idSubtitles_VideoOverlay()
    : swf(nullptr), languageMask(~0u), sequencer() {}

idSubtitles_VideoOverlay::~idSubtitles_VideoOverlay() { End(); }

void idSubtitles_VideoOverlay::SetText(const char* text) {
  if (text == nullptr)
    return;
  idSWFScriptVar value = swf->mainspriteInstance->Get("subtitles", "text0");
  idSWFTextInstance* textInstance = value.GetText();
  if (textInstance != nullptr) {
    textInstance->SetTextEffect(SWF_FONT_FX_OUTLINE_NORMAL_BLACK);
    textInstance->text = text;
    textInstance->lengthCalculated = false;
    textInstance->visible = std::strlen(text) != 0;
  }
}

void idSubtitles_VideoOverlay::End() {
  delete swf;
  swf = nullptr;
}

void idSubtitles_VideoOverlay::Start(const char* videoName, float length) {
  idPlayerProfile* profile = common->GetProfileFromMasterLocalUser();
  if (profile != nullptr)
    languageMask = profile->GetSubtitleLanguageMask();
  swf = new idSWF("vo", nullptr, false);
  swf->LoadAtlasImage(true);
  swf->Activate(static_cast<int>(Sys_Milliseconds()));
  idStr filename(videoName);
  idStr rawName;
  filename.ExtractFileBase(rawName);
  char taggedName[512];
  idStr::snPrintf(taggedName, sizeof(taggedName), "#str_video_%s", rawName.data);
  char strippedName[512];
  idStrId stringId(idVersionSelector::StripTags(strippedName,
                                                sizeof(strippedName),
                                                taggedName));
  sequencer.Start(stringId.GetLocalizedString(), length);
  SetText("");
}

void idSubtitles_VideoOverlay::Render(idRenderModelGui* guiModel,
                                      float videoTime) {
  idSWFScriptVar exitVideo = swf->mainspriteInstance->Get("exitVideo");
  idSWFSpriteInstance* exitSprite = exitVideo.GetSprite();
  if (renderSystem->HasRenderFlag(RENDERFLAG_BINKEXIT)) {
    idSWFParmList parameters;
    swf->Invoke("HideAll", parameters);
    swf->Render(guiModel, static_cast<int>(videoTime), false);
    return;
  }

  sequencer.Update(videoTime);
  const idStr currentText = sequencer.GetCurrentString(languageMask);
  SetText(currentText.data);
  const bool keyboard = in_joystickButtons.valueInteger == 2;
  if (exitSprite != nullptr) {
    idSWFParmList parameters;
    parameters.Append(idSWFScriptVar(true));
    parameters.Append(idSWFScriptVar(
        renderSystem->HasRenderFlag(RENDERFLAG_LOADCOMPLETE)));
    parameters.Append(idSWFScriptVar(static_cast<int>(keyboard)));
    parameters.Append(idSWFScriptVar(renderSystem->HasRenderFlag(
        RENDERFLAG_BINKSTARTEDFROMMAIN)));
    const bool pressed =
        renderSystem->HasRenderFlag(RENDERFLAG_PRESSEDDURINGCUTSCENE);
    parameters.Append(idSWFScriptVar(static_cast<int>(pressed)));
    if (pressed) {
      renderSystem->RemoveRenderFlag(RENDERFLAG_PRESSEDDURINGCUTSCENE);
      if (renderSystem->HasRenderFlag(RENDERFLAG_LOADCOMPLETE))
        renderSystem->SetRenderFlag(RENDERFLAG_NEXTPRESSEXITS);
    }
    swf->Invoke("FadeExitButton", parameters);
    if (renderSystem->HasRenderFlag(RENDERFLAG_LOADCOMPLETE)) {
      const idSWFScriptVar exitFade = swf->GetGlobal("runExitFade");
      if (exitFade.type == idSWFScriptVar::SWF_VAR_BOOL && !exitFade.ToBool())
        renderSystem->RemoveRenderFlag(RENDERFLAG_NEXTPRESSEXITS);
    }
  }
  swf->Render(guiModel, static_cast<int>(videoTime), false);
}
#endif
