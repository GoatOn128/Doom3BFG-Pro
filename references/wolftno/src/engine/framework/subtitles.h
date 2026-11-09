#pragma once

#include "shared/idlib/containers/list.h"
#include "shared/idlib/text/str.h"

class idRenderModelGui;
class idSWF;

class idSubtitles {
public:
  class idPart {
  public:
    idPart() : priority(0), language(0), startTime(0.0f), phrase() {}
    int priority;
    int language;
    float startTime;
    idStr phrase;
  };

  static int GetSystemLanguage();
  static const char* GetLanguageIsoName(int language);
  static bool ProcessText(const char* text, float endTime,
                          idList<idPart, 5>* parts);
  static bool StripTags(const char* text, idStr* result);
  static const char* languagesIsoNames[];
};

class idSubtitleSequencer {
public:
  idSubtitleSequencer();
  ~idSubtitleSequencer() = default;
  void Start(const char* text, float endTime);
  void Update(float newTime);
  idStr GetCurrentString(unsigned languageMask) const;
  void End() { current = parts.Num(); }
  int GetHighestPriority() { return highestPriority; }
  bool IsActive() { return active; }
  void Deactivate() { active = false; }

  idList<idSubtitles::idPart, 5> parts;
  bool active;
  int current;
  int highestPriority;
};

class idRenderVideoOverlay {
public:
  virtual ~idRenderVideoOverlay() = default;
  virtual void Start(const char* videoName, float length) = 0;
  virtual void Render(idRenderModelGui* guiModel, float videoTime) = 0;
  virtual void End() = 0;
};

class idSubtitles_VideoOverlay final : public idRenderVideoOverlay {
public:
  idSubtitles_VideoOverlay();
  ~idSubtitles_VideoOverlay() override;
  void SetLanguageMask(unsigned mask) { languageMask = mask; }
  void Start(const char* videoName, float length) override;
  void Render(idRenderModelGui* guiModel, float videoTime) override;
  void End() override;
private:
  void SetText(const char* text);

public:
  idSWF* swf;
  unsigned languageMask;
  idSubtitleSequencer sequencer;
};

static_assert(sizeof(idSubtitles::idPart) == 0x38,
              "subtitle part ABI drift");
static_assert(sizeof(idSubtitleSequencer) == 0x28,
              "subtitle sequencer ABI drift");
static_assert(sizeof(idRenderVideoOverlay) == 8,
              "video overlay ABI drift");
static_assert(sizeof(idSubtitles_VideoOverlay) == 0x40,
              "subtitle video overlay ABI drift");
