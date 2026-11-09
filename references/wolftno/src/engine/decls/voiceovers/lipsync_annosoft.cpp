#include "lipsync_annosoft.h"

#include "declvisemeset.h"
#include "shared/idlib/filesystem/filesystemlocal.h"
#include "shared/idlib/text/lexerutf8.h"
#include "voicetrack.h"

#include <algorithm>
#include <cmath>
#include <cstdlib>
#include <limits>
#include <vector>

namespace {

bool ParseInteger(const char* value, int& result) {
  if (value == nullptr || *value == '\0')
    return false;
  char* end = nullptr;
  const long parsed = std::strtol(value, &end, 10);
  if (end == value || *end != '\0' || parsed < std::numeric_limits<int>::min() ||
      parsed > std::numeric_limits<int>::max())
    return false;
  result = static_cast<int>(parsed);
  return true;
}

bool ParseWeight(const char* value, float& result) {
  if (value == nullptr || *value == '\0')
    return false;
  char* end = nullptr;
  result = std::strtof(value, &end);
  return end != value && *end == '\0' && std::isfinite(result);
}

void TokenizeAnnoLine(const char* line, std::vector<idStr>& tokens) {
  tokens.clear();
  if (line == nullptr)
    return;
  const char* cursor = line;
  while (*cursor != '\0') {
    while (*cursor != '\0' && static_cast<unsigned char>(*cursor) <= ' ')
      ++cursor;
    if (*cursor == '\0')
      break;
    const char quote = (*cursor == '\'' || *cursor == '"') ? *cursor++ : '\0';
    const char* first = cursor;
    if (quote != '\0') {
      while (*cursor != '\0' && *cursor != quote)
        ++cursor;
    } else {
      while (*cursor != '\0' && static_cast<unsigned char>(*cursor) > ' ')
        ++cursor;
    }
    tokens.emplace_back(first, 0, static_cast<int>(cursor - first));
    if (quote != '\0' && *cursor == quote)
      ++cursor;
  }
}

std::uint8_t ScaleWeight(float weight) {
  if (weight <= 1.0f)
    weight *= 255.0f;
  return static_cast<std::uint8_t>(std::clamp(std::lround(weight), 0L, 255L));
}

} // namespace

idPhonemeTrack_Annosoft::idPhonemeEvent::idPhonemeEvent()
    : type(-1), exType(EXPHONEME_MAX), startMS(0), endMS(0), numWeights(0),
      weights(nullptr) {}

idPhonemeTrack_Annosoft::idPhonemeEvent::idPhonemeEvent(const idPhonemeEvent& other)
    : idPhonemeEvent() {
  Copy(other);
}

idPhonemeTrack_Annosoft::idPhonemeEvent&
idPhonemeTrack_Annosoft::idPhonemeEvent::operator=(const idPhonemeEvent& other) {
  Copy(other);
  return *this;
}

idPhonemeTrack_Annosoft::idPhonemeEvent::~idPhonemeEvent() {
  delete[] weights;
  weights = nullptr;
  numWeights = 0;
}

void idPhonemeTrack_Annosoft::idPhonemeEvent::Copy(const idPhonemeEvent& other) {
  if (this == &other)
    return;
  type = other.type;
  exType = other.exType;
  startMS = other.startMS;
  endMS = other.endMS;
  text = other.text;
  delete[] weights;
  numWeights = other.numWeights;
  weights = numWeights > 0 ? new weight_t[numWeights] : nullptr;
  for (int index = 0; index < numWeights; ++index)
    weights[index] = other.weights[index];
}

bool idPhonemeTrack_Annosoft::ParseLine(const idDeclVisemeSet* visemeSet, const char* line,
                                        bool inTextBlock) {
  idStr cleaned(line ? line : "");
  cleaned.StripLeadingWhiteSpace();
  cleaned.StripTrailingWhitespace();
  if (cleaned.IsEmpty() || cleaned.CmpPrefix("//") == 0 || cleaned[0] == '#')
    return true;

  std::vector<idStr> tokens;
  TokenizeAnnoLine(cleaned.data, tokens);
  if (tokens.empty())
    return true;

  bool phonemeRecord = tokens[0].Icmp("phn") == 0 || tokens[0].Icmp("phoneme") == 0;
  const bool wordRecord = tokens[0].Icmp("wrd") == 0 || tokens[0].Icmp("word") == 0;
  const bool punctuationRecord = tokens[0].Icmp("pnc") == 0 ||
                                 tokens[0].Icmp("punct") == 0 ||
                                 tokens[0].Icmp("punctuation") == 0;
  if (inTextBlock && !wordRecord && !punctuationRecord)
    return true;

  idPhonemeEvent event;
  int labelIndex = -1;
  int weightIndex = -1;
  if (phonemeRecord || wordRecord || punctuationRecord) {
    if (tokens.size() < 4 || !ParseInteger(tokens[1].data, event.startMS) ||
        !ParseInteger(tokens[2].data, event.endMS))
      return false;
    float ignoredWeight = 0.0f;
    if (tokens.size() >= 5 && ParseWeight(tokens[3].data, ignoredWeight)) {
      weightIndex = 3;
      labelIndex = 4;
    } else {
      labelIndex = 3;
    }
  } else {
    if (tokens.size() < 3 || !ParseInteger(tokens[1].data, event.startMS) ||
        !ParseInteger(tokens[2].data, event.endMS))
      return true;
    phonemeRecord = true;
    labelIndex = 0;
    weightIndex = tokens.size() > 3 ? 3 : -1;
  }

  if (event.endMS < event.startMS)
    std::swap(event.startMS, event.endMS);

  if (wordRecord || punctuationRecord) {
    event.exType = wordRecord ? EXPHONEME_WORD : EXPHONEME_PUNCTUATION;
    event.text = tokens[labelIndex];
  } else if (phonemeRecord) {
    event.exType = EXPHONEME_PHONEME;
    event.type = visemeSet->PhonemeForName(tokens[labelIndex].data);
    if (event.type < 0)
      event.type = visemeSet->silencePhoneme;
    if (event.type < 0)
      return false;

    float weight = 1.0f;
    if (weightIndex >= 0)
      ParseWeight(tokens[weightIndex].data, weight);
    event.numWeights = 1;
    event.weights = new weight_t[1]{{event.startMS, event.endMS, weight}};
  }
  phonemes.Append(event);
  return true;
}

bool idPhonemeTrack_Annosoft::Load(const idDeclVisemeSet* visemeSet, idLexerUTF8* source) {
  phonemes.Clear();
  if (visemeSet == nullptr || source == nullptr || !source->IsLoaded())
    return false;

  bool valid = true;
  bool inTextBlock = false;
  idStr line;
  while (source->ReadLine(line)) {
    idStr marker(line);
    marker.StripLeadingWhiteSpace();
    marker.StripTrailingWhitespace();
    if (marker.CmpPrefix("%%-begin-anno-text-%%") == 0) {
      inTextBlock = true;
      continue;
    }
    if (marker.CmpPrefix("%%-end-anno-text-%%") == 0) {
      inTextBlock = false;
      continue;
    }
    valid = ParseLine(visemeSet, line.data, inTextBlock) && valid;
  }
  return valid && !source->HadError() && phonemes.Num() > 0;
}

bool idPhonemeTrack_Annosoft::Load(const idDeclVisemeSet* visemeSet, const char* filename) {
  if (filename == nullptr || *filename == '\0' || fileSystem == nullptr)
    return false;
  std::vector<std::uint8_t> contents;
  if (!fileSystem->ReadFile(filename, contents))
    return false;
  idLexerUTF8 source;
  source.LoadMemory(contents.data(), contents.size(), filename);
  return Load(visemeSet, &source);
}

bool idPhonemeTrack_Annosoft::GetVoiceTrack(idVoiceTrack* voiceTrack, int timeOffsetMS) {
  if (voiceTrack == nullptr || voiceTrack->declVisemeSet == nullptr)
    return false;
  const idDeclVisemeSet* visemeSet = voiceTrack->declVisemeSet;
  bool generated = false;

  for (int index = 0; index < phonemes.Num(); ++index) {
    const idPhonemeEvent& source = phonemes[index];
    if (source.exType == EXPHONEME_PHONEME) {
      if (source.type < 0 || source.type >= visemeSet->phonemes.Num())
        continue;
      const idPhoneme& mapping = visemeSet->phonemes[source.type];
      if (mapping.viseme == 255)
        continue;
      const vtrEventHandle_t handle = voiceTrack->voiceEventManager.AllocEvent("visemeSimple");
      idViseme_Simple* event = static_cast<idViseme_Simple*>(
          voiceTrack->voiceEventManager.GetEvent(handle));
      if (event == nullptr)
        continue;

      const int mappedStart = source.startMS + mapping.timeOffsetMS + timeOffsetMS;
      const int sourceDuration = std::max(0, source.endMS - source.startMS);
      const int mappedDuration = std::max(
          0, static_cast<int>(std::lround(sourceDuration * mapping.durationScale)));
      event->startMS = std::max(0, mappedStart);
      event->durationMS = static_cast<std::uint16_t>(std::min(mappedDuration, 65535));
      event->visemeType = mapping.viseme;

      float peakWeight = 0.0f;
      int peakTime = sourceDuration / 2;
      for (int weight = 0; weight < source.numWeights; ++weight) {
        if (source.weights[weight].weight >= peakWeight) {
          peakWeight = source.weights[weight].weight;
          peakTime = (source.weights[weight].startMS + source.weights[weight].endMS) / 2 -
                     source.startMS;
        }
      }
      if (source.numWeights == 0)
        peakWeight = 1.0f;
      peakTime = static_cast<int>(std::lround(peakTime * mapping.durationScale));
      event->peakTimeMS = static_cast<std::uint16_t>(
          std::clamp(peakTime, 0, static_cast<int>(event->durationMS)));
      event->peakWeight = ScaleWeight(peakWeight * mapping.weightScale * voiceTrack->weightScale);
      voiceTrack->events.Append(handle);
      generated = true;
    } else if (source.exType == EXPHONEME_WORD) {
      const vtrEventHandle_t handle = voiceTrack->voiceEventManager.AllocEvent("text");
      idVoiceTextEvent* event = static_cast<idVoiceTextEvent*>(
          voiceTrack->voiceEventManager.GetEvent(handle));
      if (event == nullptr)
        continue;
      event->startMS = std::max(0, source.startMS + timeOffsetMS);
      event->durationMS = static_cast<std::uint16_t>(
          std::clamp(source.endMS - source.startMS, 0, 65535));
      event->text = source.text;
      voiceTrack->events.Append(handle);
      generated = true;
    } else if (source.exType == EXPHONEME_PUNCTUATION && !source.text.IsEmpty()) {
      const vtrEventHandle_t handle = voiceTrack->voiceEventManager.AllocEvent("punctuation");
      idVoicePunctuationEvent* event = static_cast<idVoicePunctuationEvent*>(
          voiceTrack->voiceEventManager.GetEvent(handle));
      if (event == nullptr)
        continue;
      event->startMS = std::max(0, source.startMS + timeOffsetMS);
      event->durationMS = static_cast<std::uint16_t>(
          std::clamp(source.endMS - source.startMS, 0, 65535));
      event->punctuation = source.text[0];
      voiceTrack->events.Append(handle);
      generated = true;
    }
  }
  voiceTrack->rebuildFrameRefs = generated || voiceTrack->rebuildFrameRefs;
  return generated;
}
