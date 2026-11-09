#pragma once

class idDeclVisemeSet;
class idLexerUTF8;
class idVoiceTrack;

class idPhonemeTrack {
public:
  virtual ~idPhonemeTrack() = default;
  virtual bool Load(const idDeclVisemeSet* visemeSet, idLexerUTF8* source) = 0;
  virtual bool Load(const idDeclVisemeSet* visemeSet, const char* filename) = 0;
  virtual bool GetVoiceTrack(idVoiceTrack* voiceTrack, int timeOffsetMS) = 0;
};

static_assert(sizeof(idPhonemeTrack) == 8, "idPhonemeTrack ABI drift");
