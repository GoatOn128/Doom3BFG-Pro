#pragma once

#include "lipsync.h"
#include "shared/idlib/containers/list.h"
#include "shared/idlib/text/str.h"

#include <cstdint>

class idPhonemeTrack_Annosoft final : public idPhonemeTrack {
public:
  enum extendedPhoneme_t {
    EXPHONEME_PHONEME,
    EXPHONEME_WORD,
    EXPHONEME_PUNCTUATION,
    EXPHONEME_MAX
  };

  struct weight_t {
    int startMS;
    int endMS;
    float weight;
  };

  struct idPhonemeEvent {
    idPhonemeEvent();
    idPhonemeEvent(const idPhonemeEvent& other);
    idPhonemeEvent& operator=(const idPhonemeEvent& other);
    ~idPhonemeEvent();
    void Copy(const idPhonemeEvent& other);

    int type;
    extendedPhoneme_t exType;
    int startMS;
    int endMS;
    std::int16_t numWeights;
    weight_t* weights;
    idStr text;
  };

  idPhonemeTrack_Annosoft() = default;
  ~idPhonemeTrack_Annosoft() override = default;
  bool Load(const idDeclVisemeSet* visemeSet, idLexerUTF8* source) override;
  bool Load(const idDeclVisemeSet* visemeSet, const char* filename) override;
  bool GetVoiceTrack(idVoiceTrack* voiceTrack, int timeOffsetMS) override;

  const idList<idPhonemeEvent, 68>& GetEvents() const { return phonemes; }

private:
  bool ParseLine(const idDeclVisemeSet* visemeSet, const char* line, bool inTextBlock);

public:
  idList<idPhonemeEvent, 68> phonemes;
};

static_assert(sizeof(idPhonemeTrack_Annosoft::weight_t) == 12,
              "Annosoft weight ABI drift");
static_assert(sizeof(idPhonemeTrack_Annosoft::idPhonemeEvent) == 72,
              "Annosoft phoneme event ABI drift");
static_assert(sizeof(idPhonemeTrack_Annosoft) == 32, "Annosoft track ABI drift");
