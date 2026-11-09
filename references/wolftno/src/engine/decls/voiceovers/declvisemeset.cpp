#include "declvisemeset.h"

#include <algorithm>

idDeclInfoTemplate<idDeclVisemeSet> idDeclVisemeSet::resourceList(
    "visemeSet", "idDeclVisemeSet", "vset", false);

idViseme::idViseme() : viseme(255) {}
idPhoneme::idPhoneme()
    : phoneme(-1), viseme(255), weightScale(1.0f), durationScale(1.0f), timeOffsetMS(0) {}

idDeclVisemeSet::idDeclVisemeSet() : silenceViseme(255), silencePhoneme(-1) {
  resourceListPtr = &resourceList;
  visemes.granularity = 0;
  phonemes.granularity = 0;
}
idDeclVisemeSet::~idDeclVisemeSet() = default;
const char* idDeclVisemeSet::DefaultDefinition() const {
  return "{\n\tphonemeSet \"\"\n\tvisemeSilence viseme_REST\n\tphonemeSilence x\n"
         "\tvisemes {\n\t}\n\tphonemes {\n\t}\n}\n";
}
void idDeclVisemeSet::FreeData() {
  visemes.ClearFree();
  phonemes.ClearFree();
}
idDeclInfo* idDeclVisemeSet::GetDeclInfo() const { return &resourceList; }
std::uint64_t idDeclVisemeSet::Size() const {
  return 32ULL * (phonemes.size + 4LL) + 24ULL * visemes.size;
}
std::uint8_t idDeclVisemeSet::VisemeForName(const char* value) const {
  for (int i = 0; i < visemes.Num(); ++i)
    if (idStr::Icmp(visemes[i].name.str, value) == 0)
      return static_cast<std::uint8_t>(i);
  return 255;
}
int idDeclVisemeSet::PhonemeForName(const char* value) const {
  for (int i = 0; i < phonemes.Num(); ++i)
    if (idStr::Icmp(phonemes[i].name.str, value) == 0)
      return i;
  return -1;
}
void idDeclVisemeSet::Parse(idParser& source) {
  idToken token;
  if (source.PeekTokenString("}"))
    return;
  if (source.PeekTokenString("{"))
    source.ExpectTokenString("{");
  source.ExpectTokenString("phonemeSet");
  source.ExpectTokenType(TT_STRING, 0, token);
  phonemeSet = token;
  source.ExpectTokenString("visemeSilence");
  source.ExpectTokenType(TT_NAME, 0, token);
  idStr silenceVisemeName = token;
  source.ExpectTokenString("phonemeSilence");
  source.ExpectTokenType(TT_NAME, 0, token);
  idStr silencePhonemeName = token;

  source.ExpectTokenString("visemes");
  source.ExpectTokenString("{");
  while (source.ReadToken(token) && token != "}") {
    if (token.type != TT_NAME) {
      source.Error("Expected a viseme name, read '%s'", token.data);
      continue;
    }
    idViseme item;
    item.name.Set(token.data);
    item.viseme = static_cast<std::uint8_t>(visemes.Num());
    source.ExpectTokenString("{");
    source.ExpectTokenString("alias");
    source.ExpectTokenType(TT_STRING, 0, token);
    item.aliasName.Set(token.data);
    source.ExpectTokenString("}");
    visemes.Append(item);
  }
  if (visemes.Num() > 255)
    source.Error("There are too many visemes (%d; maximum 255)", visemes.Num());

  source.ExpectTokenString("phonemes");
  source.ExpectTokenString("{");
  while (source.ReadToken(token) && token != "}") {
    if (token.type != TT_NAME) {
      source.Error("Expected a phoneme name, but read '%s'", token.data);
      continue;
    }
    idPhoneme item;
    item.name.Set(token.data);
    item.phoneme = phonemes.Num();
    if (source.CheckTokenString("{")) {
      while (source.ReadToken(token) && token != "}") {
        if (token.Icmp("weightScale") == 0)
          item.weightScale = source.ParseFloat();
        else if (token.Icmp("durationScale") == 0)
          item.durationScale = source.ParseFloat();
        else if (token.Icmp("timeOffsetMS") == 0)
          item.timeOffsetMS = source.ParseInt();
        else {
          const std::uint8_t mapped = VisemeForName(token.data);
          if (mapped == 255)
            source.Error("Unknown phoneme property or viseme '%s'", token.data);
          else if (item.viseme != 255)
            source.Error("Multiple visemes specified in phoneme '%s'", item.name.str);
          else
            item.viseme = mapped;
        }
      }
    } else {
      source.ExpectTokenString("&");
      source.ExpectTokenType(TT_NAME, 0, token);
      item.viseme = VisemeForName(token.data);
      if (item.viseme == 255)
        source.Error("Phoneme '%s' maps to unknown viseme '%s'", item.name.str, token.data);
    }
    if (item.weightScale < 0.0f || item.durationScale < 0.0f)
      source.Error("Negative scale in phoneme '%s'", item.name.str);
    phonemes.Append(item);
  }
  source.ExpectTokenString("}");
  silenceViseme = VisemeForName(silenceVisemeName.data);
  silencePhoneme = PhonemeForName(silencePhonemeName.data);
}
