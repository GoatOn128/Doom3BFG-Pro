#pragma once

#include "shared/idlib/text/str.h"

#include <string>
#include <vector>

struct idFaceVoiceTrack {
  std::string sample;
  int durationMs{0};
  bool streamedReady{true};
};

struct idDeclVoiceOver {
  idStr name;
  int durationMs{0};
  std::vector<idFaceVoiceTrack> tracks;
  int nextSampleToPlay{-1};
  const char* declTypeName{"voiceover"};
  bool resourceValid{true};
};
