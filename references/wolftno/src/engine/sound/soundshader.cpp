#include "engine/sound/soundshader.h"

#include "engine/decls/decltable.h"
#include "shared/idlib/filesystem/file.h"
#include "shared/idlib/lib.h"

#include <algorithm>
#include <cstring>

namespace {

constexpr const char* groupNames[30] = {
    "ui", "music_menu", "music_ingame", "sfx_general", "amb_core", "amb_spot",
    "explosions", "key_events", "impacts", "rico_bys", "movement_ai", "movement_pl",
    "veh_ai", "veh_pl", "veh_collisions_ai", "veh_collisions_pl", "weapons_ai",
    "weapons_pl", "special_enemy_movement", "special_enemy_voice", "vo_player",
    "vo_event", "vo_ingame", "vo_radio", "vo_battle", "mix_group_1", "mix_group_2",
    "mix_group_3", "mix_group_4", "mix_group_5"};

std::uint32_t NextRandom() {
  soundRandomSeed = 1664525U * soundRandomSeed + 1013904223U;
  return soundRandomSeed;
}

int RandomIndex(int count) {
  return count > 0 ? static_cast<int>(((NextRandom() >> 10) & 0x7FFFU) % count) : 0;
}

void ClearSampleList(idList<const idSoundSample*, 38>& list) { list.ClearFree(); }

const idSoundSample* LoadSample(const char* name) {
  return static_cast<const idSoundSample*>(idSoundSample::resourceList.Load(name, true, false));
}

int FindGroup(const char* name) {
  for (int index = 0; index < 30; ++index)
    if (idStr::Icmp(name, groupNames[index]) == 0) return index;
  return -1;
}

void SkipAssignment(idParser& source) {
  source.CheckTokenString("=");
  if (source.PeekTokenString("{")) source.SkipBracedSection(true);
  else {
    idToken token;
    while (source.ReadToken(token) && token != ";" && token != "}") {}
    if (token == "}") source.UnreadToken(token);
  }
}

void ParseSoundList(idParser& source, idList<idStrSound, 38>& list) {
  list.ClearFree();
  source.CheckTokenString("=");
  if (!source.CheckTokenString("{")) return;
  idToken token;
  while (source.ReadToken(token) && token != "}") {
    if (token.Icmp("item") != 0) {
      SkipAssignment(source);
      continue;
    }
    if (source.CheckTokenString("[")) {
      source.ParseInt();
      source.CheckTokenString("]");
    }
    source.CheckTokenString("=");
    if (source.ReadToken(token)) {
      idStrSound value;
      value.Set(token.data);
      list.Append(value);
    }
    source.CheckTokenString(";");
  }
}

float ParseAssignedFloat(idParser& source) {
  source.CheckTokenString("=");
  const float value = source.ParseFloat();
  source.CheckTokenString(";");
  return value;
}

int ParseAssignedInt(idParser& source) {
  source.CheckTokenString("=");
  const int value = source.ParseInt();
  source.CheckTokenString(";");
  return value;
}

void ParseRandomValue(idParser& source, randomValue_t& value) {
  source.CheckTokenString("=");
  if (!source.CheckTokenString("{")) return;
  idToken token;
  while (source.ReadToken(token) && token != "}") {
    if (token.Icmp("min") == 0) value.min = ParseAssignedFloat(source);
    else if (token.Icmp("max") == 0) value.max = ParseAssignedFloat(source);
    else if (token.Icmp("offset") == 0) value.offset = ParseAssignedFloat(source);
    else if (token.Icmp("granularity") == 0) value.granularity = ParseAssignedFloat(source);
    else SkipAssignment(source);
  }
}

int FlagForName(const char* name) {
  struct pair_t { const char* name; int value; };
  static constexpr pair_t flags[] = {
      {"private", SSF_PRIVATE_SOUND}, {"antiPrivate", SSF_ANTI_PRIVATE_SOUND},
      {"noOcclusion", SSF_NO_OCCLUSION}, {"global", SSF_GLOBAL},
      {"omnidirectional", SSF_OMNIDIRECTIONAL}, {"frontDirectional", SSF_FRONTDIRECTIONAL},
      {"rearDirectional", SSF_REARDIRECTIONAL}, {"environmental", SSF_ENVIRONMENTAL},
      {"coneAttenuation", SSF_CONEATTENUATION}, {"looping", SSF_LOOPING},
      {"playOnce", SSF_PLAY_ONCE}, {"sequential", SSF_SEQUENTIAL},
      {"noRandomOffset", SSF_NO_RANDOM_OFFSET}, {"autoRestart", SSF_AUTO_RESTART},
      {"flicker", SSF_FLICKER}, {"noEffects", SSF_NO_EFFECTS},
      {"finiteSpeedOfSound", SSF_FINITE_SPEED_OF_SOUND},
      {"distanceLowPass", SSF_DISTANCE_LOW_PASS}, {"lfe", SSF_LFE},
      {"distanceBasedStereo", SSF_DISTANCE_BASED_STEREO},
      {"randomLooping", SSF_RANDOM_LOOPING}, {"passHDR", SSF_PASS_HDR},
      {"blockOnLoad", SSF_BLOCK_ON_LOAD}, {"samplePack", SSF_SAMPLEPACK}};
  for (const pair_t& flag : flags)
    if (idStr::Icmp(name, flag.name) == 0) return flag.value;
  return 0;
}

void ParseParms(idParser& source, soundShaderParms_t& parms) {
  source.CheckTokenString("=");
  if (!source.CheckTokenString("{")) return;
  idToken token;
  while (source.ReadToken(token) && token != "}") {
    if (token.Icmp("minDistance") == 0) parms.minDistance = ParseAssignedFloat(source);
    else if (token.Icmp("maxDistance") == 0) parms.maxDistance = ParseAssignedFloat(source);
    else if (token.Icmp("distanceFactor") == 0) parms.distanceFactor = ParseAssignedFloat(source);
    else if (token.Icmp("wetMinDistanceOffset") == 0) parms.wetMinDistanceOffset = ParseAssignedFloat(source);
    else if (token.Icmp("wetMaxDistanceOffset") == 0) parms.wetMaxDistanceOffset = ParseAssignedFloat(source);
    else if (token.Icmp("volume") == 0) ParseRandomValue(source, parms.volume);
    else if (token.Icmp("pitch") == 0) ParseRandomValue(source, parms.pitch);
    else if (token.Icmp("wetVolume") == 0) parms.wetVolume = ParseAssignedFloat(source);
    else if (token.Icmp("loopFadeInDuration") == 0) parms.loopFadeInDuration = ParseAssignedFloat(source);
    else if (token.Icmp("loopFadeOutDuration") == 0) parms.loopFadeOutDuration = ParseAssignedFloat(source);
    else if (token.Icmp("dopplerMultiplier") == 0) parms.dopplerMultiplier = ParseAssignedFloat(source);
    else if (token.Icmp("lowPassMinDistance") == 0) parms.lowPassMinDistance = ParseAssignedFloat(source);
    else if (token.Icmp("lowPassStrength") == 0) parms.lowPassStrength = ParseAssignedFloat(source);
    else if (token.Icmp("lfeVolume") == 0) parms.lfeVolume = ParseAssignedFloat(source);
    else if (token.Icmp("centerChannel") == 0) parms.centerChannel = ParseAssignedFloat(source);
    else if (token.Icmp("headroom") == 0) parms.headroom = ParseAssignedFloat(source);
    else if (token.Icmp("frontDirectionalAngle") == 0) parms.frontDirectionalAngle = ParseAssignedFloat(source);
    else if (token.Icmp("soundChannel") == 0) parms.soundChannel = static_cast<soundChannel_t>(ParseAssignedInt(source));
    else if (token.Icmp("groups") == 0) parms.groups = static_cast<soundShaderGroups_t>(ParseAssignedInt(source));
    else if (token.Icmp("priority") == 0) parms.priority = static_cast<soundPriority_t>(ParseAssignedInt(source));
    else if (token.Icmp("soundShaderFlags") == 0) {
      source.CheckTokenString("=");
      int flags = 0;
      do {
        if (!source.ReadToken(token)) break;
        if (token == ";") break;
        if (token == "|") continue;
        if (token.type == TT_NUMBER) {
          token.NumberValue();
          flags |= static_cast<int>(token.intvalue);
        }
        else flags |= FlagForName(token.data);
      } while (true);
      parms.soundShaderFlags = static_cast<soundShaderFlags_t>(flags);
    } else SkipAssignment(source);
  }
}

void ParseEdit(idParser& source, idSoundShader& shader) {
  source.CheckTokenString("=");
  if (!source.CheckTokenString("{")) return;
  idToken token;
  while (source.ReadToken(token) && token != "}") {
    if (token.Icmp("soundFiles") == 0) ParseSoundList(source, shader.soundFiles);
    else if (token.Icmp("preSoundFiles") == 0) ParseSoundList(source, shader.preSoundFiles);
    else if (token.Icmp("tailSoundFiles") == 0) ParseSoundList(source, shader.tailSoundFiles);
    else if (token.Icmp("parms") == 0) ParseParms(source, shader.parms);
    else SkipAssignment(source);
  }
}

}  // namespace

std::uint32_t soundRandomSeed = 0;
idDeclInfoTemplate<idSoundShader> idSoundShader::resourceList(
    "sound", "idSoundShader", "sndshd", true);
idDeclInfoTemplate<idDeclDuck> idDeclDuck::resourceList(
    "duck", "idDeclDuck", "sndshd", false);

soundShaderParms_t::soundShaderParms_t() { 
  std::memset(this, 0, sizeof(*this));
  distanceFactor = 1.0F;
  coneAttenuation.outerVolume = -60.0F;
  lfeVolume = -60.0F;
}

idSoundShader::idSoundShader()
    : rmsTable(nullptr), rmsPreTable(nullptr), rmsTailTable(nullptr), samplePack(nullptr),
      preSound(nullptr), postSound(nullptr), secondSound(nullptr), remoteSound(nullptr),
      nextSample(nullptr) {
  resourceListPtr = &resourceList;
  soundFiles.granularity = preSoundFiles.granularity = tailSoundFiles.granularity = 38;
  entries.granularity = preEntries.granularity = tailEntries.granularity = 38;
  FreeData();
}

idSoundShader::~idSoundShader() { FreeData(); }
idDeclInfo* idSoundShader::GetDeclInfo() const { return &resourceList; }

bool idSoundShader::SetImplicitText() {
  idStr waveName(name.str);
  waveName.DefaultFileExtension(".wav");
  idStr text;
  text.Format("{ // IMPLICITLY GENERATED\n\tedit = {\n\t\tsoundFiles = {\n"
              "\t\t\tnum = 1;\n\t\t\titem[0] = \"%s\";\n\t\t}\n\t}\n}\n",
              waveName.data);
  SetText(text.data);
  return true;
}

void idSoundShader::FreeData() {
  delete samplePack;
  samplePack = nullptr;
  parms = soundShaderParms_t{};
  parms.minDistance = 128.0F;
  parms.maxDistance = 1024.0F;
  parms.lfeVolume = -6.0F;
  soundFiles.ClearFree();
  preSoundFiles.ClearFree();
  tailSoundFiles.ClearFree();
  entries.ClearFree();
  preEntries.ClearFree();
  tailEntries.ClearFree();
}

const idSoundSample* idSoundShader::SelectTailSample() {
  return tailEntries.Num() == 0 ? nullptr : tailEntries[RandomIndex(tailEntries.Num())];
}

const idSoundSample* idSoundShader::GetNextEntry(soundShaderGetNextEntryMode_t mode) {
  const int count = entries.Num();
  if (count == 0) return nullptr;
  if ((parms.soundShaderFlags & SSF_RANDOM_LOOPING) != 0) {
    const idSoundSample* selected = nextSample;
    if (selected == nullptr) {
      const auto& candidates = preEntries.Num() > 0 ? preEntries : entries;
      selected = candidates[RandomIndex(candidates.Num())];
    } else if (mode != SNDSHD_ONLY_PRECACHE_NEXT_ENTRY) {
      nextSample = nullptr;
    }
    if (mode == SNDSHD_ONLY_PRECACHE_NEXT_ENTRY) {
      nextSample = selected;
      selected->PreCacheStreamData(false);
      return nullptr;
    }
    if (mode == SNDSHD_GET_NEXT_ENTRY_AND_PRECACHE) selected->PreCacheStreamData(true);
    return selected;
  }
  if (nextSample == nullptr)
    nextSample = (parms.soundShaderFlags & SSF_SEQUENTIAL) != 0
                     ? entries[0]
                     : entries[RandomIndex(count)];
  if (mode == SNDSHD_ONLY_PRECACHE_NEXT_ENTRY)
    return nextSample->PreCacheStreamData(false) > 0 ? nextSample : nullptr;
  int nextIndex = 0;
  int currentIndex = -1;
  for (int index = 0; index < count; ++index)
    if (entries[index] == nextSample) { currentIndex = index; break; }
  if (count == 2) {
    const std::uint32_t oldSeed = soundRandomSeed;
    NextRandom();
    nextIndex = ((26125 * static_cast<std::uint16_t>(oldSeed) - 3233) & 0x400) != 0;
  } else if (count > 2 && (parms.soundShaderFlags & SSF_SEQUENTIAL) != 0) {
    nextIndex = (currentIndex + 1) % count;
  } else if (count > 1 && currentIndex >= 0) {
    nextIndex = RandomIndex(count - 1);
    if (nextIndex >= currentIndex) ++nextIndex;
  } else if (currentIndex < 0) nextIndex = RandomIndex(count);
  const idSoundSample* result = nextSample;
  nextSample = entries[nextIndex];
  if (mode == SNDSHD_GET_NEXT_ENTRY_AND_PRECACHE) {
    result->PreCacheStreamData(true);
    nextSample->PreCacheStreamData(false);
  }
  return result;
}

bool idSoundShader::PinCacheData(pinnedCacheLines_t* lines, bool highPriority) {
  if (samplePack != nullptr) return samplePack->PinCacheData(lines, highPriority);
  for (int index = 0; index < entries.Num(); ++index) entries[index]->PreCacheStreamData(false);
  for (int index = 0; index < preEntries.Num(); ++index) preEntries[index]->PreCacheStreamData(false);
  return true;
}

bool idSoundShader::PreCacheData() {
  bool result = true;
  for (idSoundShader* shader = this; shader != nullptr;
       shader = const_cast<idSoundShader*>(shader->secondSound)) {
    const idSoundSample* selected = nullptr;
    if (shader->entries.Num() > 0) {
      if ((shader->parms.soundShaderFlags & SSF_RANDOM_LOOPING) != 0) {
        selected = shader->nextSample;
        if (selected == nullptr) {
          const auto& candidates =
              shader->preEntries.Num() > 0 ? shader->preEntries : shader->entries;
          selected = candidates[RandomIndex(candidates.Num())];
          shader->nextSample = selected;
        }
        selected->PreCacheStreamData(false);
      } else {
        if (shader->nextSample == nullptr) {
          shader->nextSample =
              (shader->parms.soundShaderFlags & SSF_SEQUENTIAL) != 0
                  ? shader->entries[0]
                  : shader->entries[RandomIndex(shader->entries.Num())];
        }
        if (shader->nextSample->PreCacheStreamData(false) > 0)
          selected = shader->nextSample;
      }
    }
    result &= selected != nullptr;
  }
  return result;
}

int idSoundShader::GetMaxLength() const {
  if ((parms.soundShaderFlags & (SSF_LOOPING | SSF_RANDOM_LOOPING)) != 0) return 0;
  int maximum = 0;
  for (int index = 0; index < entries.Num(); ++index) {
    const idSoundSample* sample = entries[index];
    if (sample != nullptr && sample->format.basic.samplesPerSec >= 100) {
      const int milliseconds =
          10 * (sample->lastValidSample - sample->firstValidSample) /
          static_cast<int>(sample->format.basic.samplesPerSec / 100);
      maximum = std::max(maximum, milliseconds);
    }
  }
  return maximum;
}

void idSoundShader::List() const {
  int bytes = 0;
  for (int index = 0; index < entries.Num(); ++index)
    if (entries[index] != nullptr) bytes += entries[index]->dataLength;
  idLib::Printf("%4.1fkb %s\n", static_cast<float>(bytes) / 1024.0F, name.str);
  for (int index = 0; index < entries.Num(); ++index)
    if (entries[index] != nullptr) const_cast<idSoundSample*>(entries[index])->List();
}

void idSoundShader::ReloadSamples() {
  if (idLib::production != PROD_DEVELOPMENT &&
      (parms.soundShaderFlags & SSF_SAMPLEPACK) != 0) return;
  ClearSampleList(entries);
  ClearSampleList(preEntries);
  ClearSampleList(tailEntries);
  for (int index = 0; index < soundFiles.Num(); ++index)
    if (soundFiles[index].str[0] != '\0') entries.Append(LoadSample(soundFiles[index].str));
  for (int index = 0; index < preSoundFiles.Num(); ++index)
    if (preSoundFiles[index].str[0] != '\0') preEntries.Append(LoadSample(preSoundFiles[index].str));
  for (int index = 0; index < tailSoundFiles.Num(); ++index)
    if (tailSoundFiles[index].str[0] != '\0') tailEntries.Append(LoadSample(tailSoundFiles[index].str));
}

void idSoundShader::LoadResource() {
  idDecl::LoadResource();
  delete samplePack;
  samplePack = nullptr;
  if ((parms.soundShaderFlags & SSF_SAMPLEPACK) != 0 && idLib::production >= PROD_PRODUCTION) {
    samplePack = idSoundSamplePack::Load(this, "vorbis", false);
    entries.ClearFree(); preEntries.ClearFree(); tailEntries.ClearFree();
    if (samplePack != nullptr) {
      int sampleIndex = 0;
      for (int index = 0; index < preSoundFiles.Num() && sampleIndex < samplePack->samples.Num(); ++index)
        preEntries.Append(samplePack->samples[sampleIndex++]);
      for (int index = 0; index < soundFiles.Num() && sampleIndex < samplePack->samples.Num(); ++index)
        entries.Append(samplePack->samples[sampleIndex++]);
      for (int index = 0; index < tailSoundFiles.Num() && sampleIndex < samplePack->samples.Num(); ++index)
        tailEntries.Append(samplePack->samples[sampleIndex++]);
    }
  }
}

void idSoundShader::Parse(idParser& source) {
  idToken token;
  while (source.ReadToken(token) && token != "}") {
    if (token.Icmp("edit") == 0) ParseEdit(source, *this);
    else SkipAssignment(source);
  }
  for (int index = soundFiles.Num() - 1; index >= 0; --index) {
    if (soundFiles[index].str[0] == '\0') {
      for (int move = index + 1; move < soundFiles.Num(); ++move)
        soundFiles[move - 1] = soundFiles[move];
      --soundFiles.num;
    }
  }
  hasBeenParsedAtLeastOnce = true;
  ReloadSamples();
}

idDeclDuck::idDeclDuck() : layer(0), groupVolume{} { resourceListPtr = &resourceList; }
idDeclInfo* idDeclDuck::GetDeclInfo() const { return &resourceList; }
const char* idDeclDuck::DefaultDefinition() const { return "{ }\n"; }

void idDeclDuck::Parse(idParser& source) {
  idToken token;
  while (source.ReadToken(token) && token != "}") {
    if (token.Icmp("layer") == 0) layer = ParseAssignedInt(source);
    else {
      const int group = FindGroup(token.data);
      if (group >= 0) groupVolume[group] = ParseAssignedFloat(source);
      else SkipAssignment(source);
    }
  }
}

bool idDeclDuck::RebuildTextSource() {
  idFile_String file("duck-text");
  file.Printf("{\nlayer = %d\n", layer);
  for (int index = 0; index < 30; ++index)
    if (groupVolume[index] != 0.0F)
      file.Printf("%s = %f\n", groupNames[index], groupVolume[index]);
  file.Printf("}");
  const char terminator = '\0';
  file.Write(&terminator, sizeof(terminator));
  const idStr text = file.ToString();
  SetText(text.data);
  return true;
}
