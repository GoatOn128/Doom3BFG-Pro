#include "voicetrack.h"

#include "engine/animation/animationpreload.h"
#include "engine/animation/md6.h"
#include "engine/sound/voicepreload.h"
#include "shared/idlib/filesystem/filesystemlocal.h"
#include "shared/idlib/lib.h"

#include <algorithm>
#include <cstring>
#include <limits>
#include <memory>
#include <vector>

namespace {
template <typename T> T SwapBytes(T value) {
  unsigned char bytes[sizeof(T)];
  std::memcpy(bytes, &value, sizeof(T));
  std::reverse(bytes, bytes + sizeof(T));
  std::memcpy(&value, bytes, sizeof(T));
  return value;
}
template <typename T> bool WriteBE(idFile* file, T value) {
  value = SwapBytes(value);
  return file && file->Write(&value, sizeof(value)) == sizeof(value);
}
template <typename T> bool ReadBE(idFile* file, T& value) {
  if (!file || file->Read(&value, sizeof(value)) != sizeof(value)) return false;
  value = SwapBytes(value); return true;
}
bool WriteString(idFile* file, const char* value) {
  const std::int32_t length = idStr::Length(value);
  return file && file->Write(&length, sizeof(length)) == sizeof(length) &&
         (!length || file->Write(value, length) == static_cast<std::size_t>(length));
}
bool ReadString(idFile* file, idStr& value) {
  std::int32_t length = 0;
  if (!file || file->Read(&length, sizeof(length)) != sizeof(length) ||
      length < 0 || length > 16 * 1024 * 1024) return false;
  value.EnsureAlloced(static_cast<int>(length + 1), false);
  if (length && file->Read(value.data, length) != length) return false;
  value.data[length] = '\0'; value.len = static_cast<int>(length); return true;
}
idStr GeneratedVoiceTrackName(const char*source,const char*extension){idStr path("generated");path.AppendPath(source?source:"");path.SetFileExtension(extension);return path;}
}

idResourceList idVoiceTrack::resourceList;
const char* idVoiceTrack::VOICE_TRACK_NAME = "voiceTrack";
const char* idVoiceTrack::DEFAULT_VISEME_SET_NAME = "slowdrawl";
const char* const idVoiceTrack::moodNames[MOOD_MAX] = {
    "none", "clear", "happy", "sad", "angry", "suspicious", "drunk", "sleepy", "nervous", "dead"};
const char* const idVoiceTrack::moodAnimNames[MOOD_MAX] = {
    "<none>", "<none>", "facial/mood_happy", "facial/mood_sad", "facial/mood_angry",
    "facial/mood_suspicious", "facial/mood_drunk", "facial/mood_sleepy",
    "facial/mood_nervous", "facial/mood_dead"};
const char* const idVoiceTrack::faceChannelNames[FACE_CHANNEL_MAX] = {
    "body", "legs", "torso", "head", "eyes", "emoteMouth", "emoteBrow", "emoteFace",
    "face", "bodyweb", "userChannel"};

pinnedCacheLines_t::pinnedCacheLines_t()
    : cacheFile(nullptr), ofs(0), length(0), numLines(0), extra(0), lineData{},
      lineLength{} {}

void idVoiceTrack::idFrameRefIndexBuffer::Alloc(std::uint16_t count) {
  delete[] indices;
  indices = count ? new std::uint16_t[count]{} : nullptr;
  curOffset = 0;
  size = count;
}
idVoiceTrack::idFrameRefIndexBuffer::~idFrameRefIndexBuffer() { delete[] indices; }
bool idVoiceTrack::idFrameRefIndexBuffer::Write(idFile* file) const {
  if (!WriteBE(file, size) || !WriteBE(file, curOffset)) return false;
  for (int i = 0; i < size; ++i) if (!WriteBE(file, indices[i])) return false;
  return true;
}
bool idVoiceTrack::idFrameRefIndexBuffer::Read(idFile* file) {
  std::uint16_t count = 0, offset = 0;
  if (!ReadBE(file, count) || !ReadBE(file, offset) || offset > count) return false;
  Alloc(count); curOffset = offset;
  for (int i = 0; i < count; ++i) if (!ReadBE(file, indices[i])) return false;
  return true;
}

idVoiceTrack::idVoiceTrack(const char* defaultVisemeSetName)
    : frameRefs(nullptr), numFrameRefs(0), rebuildFrameRefs(true), visemeOffsetState(0),
      offsetState(0), textCRC(0), lipsyncTextCRC(0), soundShader(nullptr),
      pinnedStreamData(nullptr), numPinnedStreamData(0), sampleTimestamp(0), sampleCRC(0),
      faceChannel(FACE_CHANNEL_BODY), blendInMS(250), blendOutMS(250), soundOffsetMS(66),
      visemeOffsetMS(0), animEventOffsetMS(0), moodEventOffsetMS(0), gameEventOffsetMS(0),
      textEventOffsetMS(0), flags{}, weightScale(1.0f), defaultMood(MOOD_NONE),
      declVisemeSet(nullptr), timestamp(static_cast<std::uint32_t>(-1)), approximateDuration(-1) {
  resourceListPtr = &resourceList;
  events.granularity = 0;
  textId.Set("#str_empty"); subtitleId.Set("#str_empty");
  textLangKey.Set(textId.GetKey()); subtitleLangKey.Set(subtitleId.GetKey());
  flags.useRealTime = true;
  (void)defaultVisemeSetName;
}
idVoiceTrack::~idVoiceTrack() { Free(); }
idResourceList* idVoiceTrack::GetResourceList() const { return &resourceList; }

mood_t idVoiceTrack::GetMoodForName(const char* name) {
  for (unsigned i = 0; i < MOOD_MAX; ++i)
    if (idStr::Icmp(moodNames[i], name) == 0) return static_cast<mood_t>(i);
  return MOOD_NONE;
}
faceChannel_t idVoiceTrack::GetFaceChannelForName(const char* name) {
  for (int i = 0; i < FACE_CHANNEL_MAX; ++i)
    if (idStr::Icmp(faceChannelNames[i], name) == 0) return static_cast<faceChannel_t>(i);
  return FACE_CHANNEL_MAX;
}
const char* idVoiceTrack::GetMoodAnimName(unsigned mood) { return mood < MOOD_MAX ? moodAnimNames[mood] : moodAnimNames[0]; }
const char* idVoiceTrack::GetMoodName(unsigned mood) { return mood < MOOD_MAX ? moodNames[mood] : moodNames[0]; }
const char* idVoiceTrack::GetFaceChannelName(faceChannel_t channel) {
  return faceChannelNames[channel];
}

std::uint8_t idVoiceTrack::GetVisemeForName(const char* value) const {
  return declVisemeSet ? declVisemeSet->VisemeForName(value) : 255;
}
const char* idVoiceTrack::GetVisemeName(std::uint8_t index) const {
  return declVisemeSet && index < declVisemeSet->visemes.Num() ? declVisemeSet->visemes[index].name.str : "invalid";
}
void idVoiceTrack::Free() {
  voiceEventManager.Free(events);
  delete[] frameRefs; frameRefs = nullptr;
  if(pinnedStreamData){if(VoiceSoundPreloadService())VoiceSoundPreloadService()->Unpin(pinnedStreamData,numPinnedStreamData);else delete[] pinnedStreamData;}
  pinnedStreamData = nullptr; numPinnedStreamData = 0;
}
bool idVoiceTrack::ReloadIfStale() { if(!fileSystem||fileSystem->GetTimestamp(name.str)==timestamp)return false;const idStr binary=GeneratedVoiceTrackName(name.str,"bvtr");fileSystem->RemoveFile(binary.data);LoadResource();return true; }
void idVoiceTrack::LoadResource() {
  if (!fileSystem || !name.str || !*name.str) return;
  Free();
  idVoiceSoundPreload* sound = VoiceSoundPreloadService();
  idStr sourceName(name.str);
  if (sound && sound->ShouldLocalizeVoiceTracks())
    sourceName = sound->GetLocalizedVoiceTrackName(name.str);
  if (!sound || !sound->IsProduction()) {
    const idStr binary = GeneratedVoiceTrackName(sourceName.data, "bvtr");
    if (!sound || !sound->IsSoundDisabled()) {
      const std::uint32_t sourceTimestamp = fileSystem->GetTimestamp(sourceName.data);
      if (!LoadBinary(binary.data, sourceTimestamp)) {
        if (LoadText(sourceName.data))
          WriteBinary(binary.data);
        else
          SetResourceError("Voicetrack '%s' does not exist.", sourceName.data);
      }
    }
    return;
  }
  idStr binary = GeneratedVoiceTrackName(name.str, "bmvtr");
  if (!LoadBinary(binary.data, 0)) {
    binary = GeneratedVoiceTrackName(sourceName.data, "bvtr");
    if (!LoadBinary(binary.data, 0)) {
      idLib::Warning("Pre-load '%s' failed.", binary.data);
      return;
    }
  }
  if (idStr::Find(name.str, "/bj_", false) != -1)
    PinStreamedSoundData();
}
bool idVoiceTrack::LoadText(const char*fileName){
  if(!fileSystem||!fileName)return false;std::vector<std::uint8_t>data;if(!fileSystem->ReadFile(fileName,data)||data.empty())return false;timestamp=fileSystem->GetTimestamp(fileName);return LoadText(reinterpret_cast<const char*>(data.data()),static_cast<int>(data.size()));
}
bool idVoiceTrack::LoadText(const char* text, int length) {
  idLexer source(LEXFL_ALLOWPATHNAMES | LEXFL_NOSTRINGCONCAT);
  if (!source.LoadMemory(text, static_cast<std::size_t>(length), name.str)) return false;
  idToken token;if(!source.ExpectTokenString(VOICE_TRACK_NAME)||!source.ExpectTokenString("{"))return false;
  Parse(source);
  return !source.HadError();
}
void idVoiceTrack::Parse(idLexer& source) {
  visemeOffsetState = EVENTOFFSET_NONE;
  offsetState = EVENTOFFSET_NONE;
  events.Clear();
  idToken token;
  bool explicitVisemeSet = false;
  while (source.ReadToken(token) && token != "}") {
    if (token == "defaultMood") {
      source.ExpectTokenType(TT_STRING, 0, token);
      defaultMood = GetMoodForName(token.data);
    } else if (token == "visemeSet") {
      explicitVisemeSet = true;
      source.ExpectTokenType(TT_STRING, 0, token);
      declVisemeSet = static_cast<const idDeclVisemeSet*>(
          idDeclVisemeSet::resourceList.FindWithInheritance(token.data, true, true));
    } else if (token == "weightScale") {
      weightScale = source.ParseFloat();
    } else if (token == "soundOffsetMS") {
      soundOffsetMS = static_cast<std::uint32_t>(source.ParseInt());
    } else if (token == "eventOffsetMS" || token == "visemeOffsetMS") {
      visemeOffsetMS = static_cast<std::uint32_t>(source.ParseInt());
    } else if (token == "animEventOffsetMS") {
      animEventOffsetMS = static_cast<std::uint32_t>(source.ParseInt());
    } else if (token == "moodEventOffsetMS") {
      moodEventOffsetMS = static_cast<std::uint32_t>(source.ParseInt());
    } else if (token == "gameEventOffsetMS") {
      gameEventOffsetMS = static_cast<std::uint32_t>(source.ParseInt());
    } else if (token == "textEventOffsetMS") {
      textEventOffsetMS = static_cast<std::uint32_t>(source.ParseInt());
    } else if (token == "useExactDuration") {
      flags.useExactDuration = source.ParseBool();
    } else if (token == "edited") {
      flags.edited = source.ParseBool();
    } else if (token == "characterIdentifier") {
      source.ExpectTokenType(TT_STRING, 0, token);
      characterIdentifier.Set(token.data);
    } else if (token == "text") {
      source.ExpectTokenString("{");
      source.ExpectTokenString("id");
      source.ExpectTokenType(TT_STRING, 0, token);
      textLangKey.Set(token.data);
      if (!*token.data)
        idLib::Warning("vtr '%s' has a completely blank text .lang key. This should never happen. Use #str_empty instead.", name.str);
      textId.Set(*token.data ? token.data : "#str_empty");
      source.ExpectTokenString("crc");
      textCRC = static_cast<std::uint32_t>(source.ParseInt());
      if (source.CheckTokenString("lipsyncTextCrc"))
        lipsyncTextCRC = static_cast<std::uint32_t>(source.ParseInt());
      source.ExpectTokenString("}");
    } else if (token == "subtitles") {
      source.ExpectTokenString("{");
      source.ExpectTokenString("id");
      source.ExpectTokenType(TT_STRING, 0, token);
      subtitleLangKey.Set(token.data);
      if (!*token.data)
        idLib::Warning("vtr '%s' has a completely blank subtitles .lang key. This should never happen. Use #str_empty instead.", name.str);
      subtitleId.Set(*token.data ? token.data : "#str_empty");
      source.ExpectTokenString("}");
    } else if (token == "sound") {
      source.ExpectTokenString("{");
      source.ExpectTokenString("shader");
      source.ExpectTokenType(TT_STRING, 0, token);
      soundShader = VoiceSoundPreloadService()
                        ? VoiceSoundPreloadService()->ResolveSoundShader(token.data)
                        : nullptr;
      if (source.CheckTokenString("timestamp"))
        sampleTimestamp = static_cast<std::uint32_t>(source.ParseInt());
      if (source.CheckTokenString("crc"))
        sampleCRC = static_cast<std::uint32_t>(source.ParseInt());
      if (source.CheckTokenString("compressed"))
        flags.compressed = source.ParseBool();
      source.ExpectTokenString("}");
    } else if (token == "anim") {
      source.ExpectTokenString("{");
      source.ExpectTokenString("alias");
      source.ExpectTokenType(TT_STRING, 0, token);
      animName = token;
      faceChannel = FACE_CHANNEL_HEAD;
      while (source.ReadToken(token) && token != "}") {
        if (token == "channel") {
          source.ExpectTokenType(TT_STRING, 0, token);
          faceChannel = GetFaceChannelForName(token.data);
        } else if (token == "blendInMS") {
          blendInMS = static_cast<std::int16_t>(source.ParseInt());
        } else if (token == "blendOutMS") {
          blendOutMS = static_cast<std::int16_t>(source.ParseInt());
        } else if (token == "cycle") {
          flags.cycleAnim = source.ParseInt() != 0;
        } else if (token == "clear") {
          flags.clearAnimAfter = source.ParseInt() != 0;
        } else if (token == "nolipsync") {
          flags.noLipSync = source.ParseInt() != 0;
        } else if (token == "suppressBlinking") {
          flags.suppressBlinking = source.ParseInt() != 0;
        } else if (token == "useRealTime") {
          flags.useRealTime = source.ParseInt() != 0;
        } else {
          source.Error("Unknown token '%s' in anim block", token.data);
        }
      }
    } else if (token == "events" || token == "visemes") {
      const int declaredCount = source.ParseInt();
      if (declaredCount < 0) {
        source.Error("Negative event count in voice track");
        break;
      }
      events.EnsureCapacity(events.Num() + declaredCount);
      source.ExpectTokenString("{");
      while (source.ReadToken(token) && token != "}") {
        vtrEventHandle_t handle = voiceEventManager.AllocEvent(token.data);
        if (!handle.IsValid()) { source.Error("Unknown voice event '%s'", token.data); break; }
        idVoiceEvent* event = voiceEventManager.GetEvent(handle);
        event->Parse(this, source);
        events.Append(handle);
      }
    } else {
      source.Error("Unknown token '%s' in voice track", token.data);
    }
  }
  voiceEventManager.Condense(events);
  if (!explicitVisemeSet)
    declVisemeSet=static_cast<const idDeclVisemeSet*>(idDeclVisemeSet::resourceList.FindWithInheritance(DEFAULT_VISEME_SET_NAME,true,true));
  OffsetEvents(EVENTOFFSET_FORWARDS, false);
  LoadAnims();
  for (int i = 0; i < events.Num(); ++i) {
    idVoiceEvent* event = voiceEventManager.GetEvent(events[i]);
    if (event && event->eventType == VOICE_EVENT_ANIM &&
        static_cast<idVoiceAnimEvent*>(event)->channel == FACE_CHANNEL_USER) {
      event->durationMS = static_cast<std::uint16_t>(
          std::clamp(approximateDuration, 0, 32767));
      break;
    }
  }
  BuildFrameRefs();
}
void idVoiceTrack::OffsetEvents(eventOffset_t direction, bool visemesOnly) {
  if (direction == EVENTOFFSET_NONE) return;
  if(direction==EVENTOFFSET_FORWARDS){
    if(visemesOnly){if(visemeOffsetState==EVENTOFFSET_FORWARDS)return;visemeOffsetState=EVENTOFFSET_FORWARDS;}
    else{if(offsetState==EVENTOFFSET_FORWARDS)return;if(visemeOffsetState==EVENTOFFSET_FORWARDS)OffsetEvents(EVENTOFFSET_BACKWARDS,true);visemeOffsetState=EVENTOFFSET_FORWARDS;offsetState=EVENTOFFSET_FORWARDS;}
  }else if(direction==EVENTOFFSET_BACKWARDS){
    if(visemesOnly){if(visemeOffsetState==EVENTOFFSET_BACKWARDS)return;visemeOffsetState=EVENTOFFSET_BACKWARDS;}
    else{if(offsetState==EVENTOFFSET_BACKWARDS)return;if(visemeOffsetState==EVENTOFFSET_BACKWARDS)OffsetEvents(EVENTOFFSET_FORWARDS,true);visemeOffsetState=EVENTOFFSET_BACKWARDS;offsetState=EVENTOFFSET_BACKWARDS;}
  }
  const std::uint32_t animOffset=static_cast<std::uint32_t>(direction)*animEventOffsetMS;
  const std::uint32_t visemeOffset=static_cast<std::uint32_t>(direction)*visemeOffsetMS;
  const std::uint32_t moodOffset=static_cast<std::uint32_t>(direction)*moodEventOffsetMS;
  const std::uint32_t gameOffset=static_cast<std::uint32_t>(direction)*gameEventOffsetMS;
  const std::uint32_t textOffset=static_cast<std::uint32_t>(direction)*textEventOffsetMS;
  for (int i = 0; i < events.Num(); ++i) {
    idVoiceEvent* event = voiceEventManager.GetEvent(events[i]); if (!event) continue;
    if(visemesOnly){if(event->eventType<=VOICE_EVENT_VISEME_FRAME)event->startMS+=visemeOffset;else if(event->eventType==VOICE_EVENT_TEXT||event->eventType==VOICE_EVENT_PUNCTUATION)event->startMS+=textOffset;continue;}
    std::uint32_t amount = 0;
    switch (event->eventType) {
      case VOICE_EVENT_VISEME_SIMPLE: case VOICE_EVENT_VISEME_FRAME: amount=visemeOffset; break;
      case VOICE_EVENT_GAME: amount=gameOffset; break;
      case VOICE_EVENT_TEXT: case VOICE_EVENT_PUNCTUATION: amount=textOffset; break;
      case VOICE_EVENT_ANIM: amount=animOffset; break;
      case VOICE_EVENT_MOOD: amount=moodOffset; break;
    }
    event->startMS += amount;
  }
}
void idVoiceTrack::LoadAnims(){idAnimationPreload*service=AnimationPreloadService();if(!service)return;auto preload=[service](const char*value){if(!value||!*value)return;idStr name(value),extension;name.ExtractFileExtension(extension);if(extension.IsEmpty())service->AddStrongAliasRef(value);else service->LoadUserChannelAnim(value);};preload(animName.data);for(int i=0;i<events.Num();++i){const idVoiceEvent*event=voiceEventManager.GetEvent(events[i]);if(event&&event->eventType==VOICE_EVENT_ANIM)preload(static_cast<const idVoiceAnimEvent*>(event)->animName.str);}}
void idVoiceTrack::PinStreamedSoundData(){if(pinnedStreamData){if(VoiceSoundPreloadService())VoiceSoundPreloadService()->Unpin(pinnedStreamData,numPinnedStreamData);else delete[]pinnedStreamData;pinnedStreamData=nullptr;numPinnedStreamData=0;}if(soundShader&&VoiceSoundPreloadService())numPinnedStreamData=VoiceSoundPreloadService()->Pin(soundShader,pinnedStreamData);}
int idVoiceTrack::FindLastEvent() const {
  int result=-1,end=-1; for(int i=0;i<events.Num();++i){const idVoiceEvent*e=voiceEventManager.GetEvent(events[i]);if(e&&e->startMS+e->durationMS>=end){end=e->startMS+e->durationMS;result=i;}}return result;
}
int idVoiceTrack::FindNextEventForFrame(int startIndex,int frame)const{const int begin=frame*66,end=begin+66;for(int i=startIndex+1;i<events.Num();++i){const idVoiceEvent*e=voiceEventManager.GetEvent(events[i]);if(e&&e->IsWithinInterval(begin,end))return i;}return-1;}
int idVoiceTrack::FindApproximateDuration()const{int duration=static_cast<int>(soundOffsetMS)+(soundShader&&VoiceSoundPreloadService()?VoiceSoundPreloadService()->GetMaxLength(soundShader):0);for(int i=0;i<events.Num();++i){const idVoiceEvent*e=voiceEventManager.GetEvent(events[i]);if(e)duration=std::max(duration,e->startMS+e->durationMS);}return duration;}
int idVoiceTrack::FindExactDuration(const idDeclMD6*declMD6)const{
  if(!declMD6)return approximateDuration<0?FindApproximateDuration():approximateDuration;
  int duration=static_cast<int>(soundOffsetMS)+(soundShader&&VoiceSoundPreloadService()?VoiceSoundPreloadService()->GetMaxLength(soundShader):0);
  idAnimationPreload*animation=AnimationPreloadService();
  for(int i=0;i<events.Num();++i){const idVoiceEvent*event=voiceEventManager.GetEvent(events[i]);if(!event)continue;int eventDuration=event->durationMS;if(event->eventType==VOICE_EVENT_ANIM&&animation){const char*eventAnim=static_cast<const idVoiceAnimEvent*>(event)->animName.str;const idMD6Anim*anim=animation->ResolveAliasAnim(declMD6,eventAnim);if(!anim)anim=animation->ResolveAnim(eventAnim);if(anim&&anim->animData&&anim->animData->frameRate)eventDuration=1000*anim->animData->numFrames/anim->animData->frameRate;}duration=std::max(duration,event->startMS+eventDuration);}
  return duration;
}
void idVoiceTrack::BuildFrameRefs(){approximateDuration=FindApproximateDuration();if(!rebuildFrameRefs||events.Num()==0)return;auto byTime=[this](const vtrEventHandle_t&a,const vtrEventHandle_t&b){const idVoiceEvent*ea=voiceEventManager.GetEvent(a);const idVoiceEvent*eb=voiceEventManager.GetEvent(b);return ea&&eb?ea->startMS<eb->startMS:ea!=nullptr;};std::sort(events.list,events.list+events.Num(),byTime);std::sort(events.list,events.list+events.Num(),byTime);delete[]frameRefs;frameRefs=nullptr;const int last=FindLastEvent();if(last<0)return;const idVoiceEvent*lastEvent=voiceEventManager.GetEvent(events[last]);const int endMS=lastEvent?lastEvent->startMS+lastEvent->durationMS:0;numFrameRefs=static_cast<std::int16_t>(endMS/66+1);std::vector<std::uint16_t>indices;frameRefs=new idFrameRef[numFrameRefs]{};for(int f=0;f<numFrameRefs;++f){frameRefs[f].offset=static_cast<std::uint16_t>(indices.size());for(int i=0;i<events.Num();++i){const idVoiceEvent*e=voiceEventManager.GetEvent(events[i]);if(e&&e->IsWithinInterval(f*66,f*66+66))indices.push_back(static_cast<std::uint16_t>(i));}frameRefs[f].numIndices=static_cast<std::uint16_t>(indices.size()-frameRefs[f].offset);}if(!indices.empty()){indexBuffer.Alloc(static_cast<std::uint16_t>(indices.size()));indexBuffer.curOffset=static_cast<std::uint16_t>(indices.size());for(int i=0;i<indexBuffer.curOffset;++i)indexBuffer.indices[i]=indices[i];}else numFrameRefs=0;}
void idVoiceTrack::GetEvents(int frame,const int*cur,const int*last,voiceEventType_t type,idStaticList<const idVoiceEvent*,32>*viseme,idStaticList<const idVoiceEvent*,32>*other)const{
  if(!cur||!last||frame<0||frame>=numFrameRefs||!frameRefs)return;const idFrameRef&ref=frameRefs[frame];if(ref.offset+ref.numIndices>indexBuffer.curOffset)return;
  for(int i=0;i<ref.numIndices;++i){const int eventIndex=indexBuffer.indices[ref.offset+i];if(eventIndex<0||eventIndex>=events.Num())continue;const idVoiceEvent*event=voiceEventManager.GetEvent(events[eventIndex]);if(!event||(type!=VOICE_EVENT_INVALID&&event->eventType!=type)||!event->IsWithinInterval(*last,*cur))continue;idStaticList<const idVoiceEvent*,32>*out=event->eventType<=VOICE_EVENT_VISEME_FRAME?viseme:other;if(!out||out->Num()>=32)continue;bool found=false;for(int n=0;n<out->Num();++n)if((*out)[n]==event){found=true;break;}if(!found)out->Append(event);}
}
void idVoiceTrack::GetEvents(int*cur,int*last,int*loop,voiceEventType_t type,idStaticList<const idVoiceEvent*,32>&viseme,idStaticList<const idVoiceEvent*,32>&other)const{
  if(!cur||!last||!loop||*cur<0||numFrameRefs<=0)return;const int lastLoop=*loop?*last/ *loop:0;const int curLoop=*loop?*cur/ *loop:0;int firstFrame=*last>0?(*loop?*last% *loop:*last)/66:0;const int finalFrame=*cur>0?(*loop?*cur% *loop:*cur)/66:0;if(lastLoop==curLoop){for(int frame=firstFrame;frame<=finalFrame;++frame)GetEvents(frame,cur,last,type,&viseme,&other);}else if(lastLoop<curLoop){for(;firstFrame<numFrameRefs;++firstFrame)GetEvents(firstFrame,cur,last,type,&viseme,&other);for(int frame=0;frame<=finalFrame;++frame)GetEvents(frame,cur,last,type,&viseme,&other);}
}
int idVoiceTrack::GetSubtitles(idList<idSubtitleText>&result,bool merge)const{result.Clear();idSubtitleText*last=nullptr;for(int i=0;i<events.Num();++i){const idVoiceEvent*e=voiceEventManager.GetEvent(events[i]);if(!e)continue;if(e->eventType==VOICE_EVENT_TEXT){const auto*t=static_cast<const idVoiceTextEvent*>(e);idSubtitleText s;s.startMS=t->startMS;s.text=t->text;int index=result.Append(s);last=&result[index];}else if(e->eventType==VOICE_EVENT_PUNCTUATION){char p=static_cast<const idVoicePunctuationEvent*>(e)->punctuation;if(!merge||p=='-'){idSubtitleText s;s.startMS=e->startMS;s.text=p;int index=result.Append(s);last=&result[index];}else if(p=='\"'||p=='\''){if(last){last->text.Append(p);last->startMS=e->startMS;}}else if((p=='('||p=='[')||!last){idSubtitleText s;s.startMS=e->startMS;s.text=p;int index=result.Append(s);last=&result[index];}else{last->text.Append(p);last->startMS=e->startMS;}}}return result.Num();}
std::size_t idVoiceTrack::Memory()const{return 440u+4u*static_cast<std::size_t>(events.size+numFrameRefs);}

void idVoiceTrackFileHeader::Swap(){timestamp=SwapBytes(timestamp);textCRC=SwapBytes(textCRC);lipsyncTextCRC=SwapBytes(lipsyncTextCRC);sampleTimestamp=SwapBytes(sampleTimestamp);sampleCRC=SwapBytes(sampleCRC);blendInMS=SwapBytes(blendInMS);blendOutMS=SwapBytes(blendOutMS);soundOffsetMS=SwapBytes(soundOffsetMS);visemeOffsetMS=SwapBytes(visemeOffsetMS);animEventOffsetMS=SwapBytes(animEventOffsetMS);moodEventOffsetMS=SwapBytes(moodEventOffsetMS);gameEventOffsetMS=SwapBytes(gameEventOffsetMS);textEventOffsetMS=SwapBytes(textEventOffsetMS);weightScale=SwapBytes(weightScale);numFrameRefs=SwapBytes(numFrameRefs);flags=SwapBytes(flags);approximateDuration=SwapBytes(approximateDuration);numEvents=SwapBytes(numEvents);}
bool idVoiceTrack::WriteBinary(const char*fileName)const{if(!fileSystem||!fileName)return false;std::unique_ptr<idFile>file(fileSystem->OpenFileWrite(fileName));return file&&WriteBinary(file.get());}
bool idVoiceTrack::WriteBinary(idFile*f)const{
  if(!f)return false;constexpr std::uint32_t magic=0x22525456u;std::uint32_t packedFlags=0;std::memcpy(&packedFlags,&flags,sizeof(flags));
  idVoiceTrackFileHeader h{timestamp,textCRC,lipsyncTextCRC,sampleTimestamp,sampleCRC,blendInMS,blendOutMS,soundOffsetMS,visemeOffsetMS,animEventOffsetMS,moodEventOffsetMS,gameEventOffsetMS,textEventOffsetMS,weightScale,numFrameRefs,static_cast<std::uint8_t>(defaultMood),static_cast<std::uint8_t>(faceChannel),packedFlags,approximateDuration,events.Num()};
  h.Swap();if(!WriteBE(f,magic)||f->Write(&h,sizeof(h))!=sizeof(h))return false;
  const char*visemeName=declVisemeSet?declVisemeSet->name.str:"";const char*shaderName=soundShader&&VoiceSoundPreloadService()?VoiceSoundPreloadService()->GetShaderName(soundShader):"";
  if(!WriteString(f,visemeName)||!WriteString(f,characterIdentifier.str)||!WriteString(f,textLangKey.str)||!WriteString(f,subtitleLangKey.str)||!WriteString(f,shaderName)||!WriteString(f,animName.data)||!indexBuffer.Write(f))return false;
  for(int i=0;i<numFrameRefs;++i)if(!WriteBE(f,frameRefs[i].offset)||!WriteBE(f,frameRefs[i].numIndices))return false;
  voiceEventManager.WriteBinary(f);if(!vtrEventHandle_t::WriteArray(f,events.list,events.Num()))return false;return WriteBE(f,magic);
}
bool idVoiceTrack::LoadBinary(const char*fileName,std::uint32_t sourceTimestamp){if(!fileSystem||!fileName)return false;std::unique_ptr<idFile>file(fileSystem->OpenFileRead(fileName));if(!file||!LoadBinary(file.get()))return false;if(sourceTimestamp!=0&&sourceTimestamp!=static_cast<std::uint32_t>(-1)&&timestamp!=sourceTimestamp){Free();return false;}return true;}
bool idVoiceTrack::LoadBinary(idFile*f){
  Free();if(!f)return false;constexpr std::uint32_t expectedMagic=0x22525456u;constexpr std::uint32_t multiMagic=0x22565452u;std::uint32_t magic=0;if(!ReadBE(f,magic))return false;if(magic==multiMagic){std::int32_t count=0;if(!ReadBE(f,count)||count<=0||count>4096)return false;const char*requestedLanguage=VoiceSoundPreloadService()?VoiceSoundPreloadService()->GetLanguage():"english";std::int32_t selectedOffset=-1;for(int i=0;i<count;++i){char language[16]{};std::int32_t size=0,offset=0;if(f->Read(language,sizeof(language))!=sizeof(language)||!ReadBE(f,size)||!ReadBE(f,offset)||size<0||offset<0)return false;language[15]='\0';if(selectedOffset<0||idStr::Icmp(language,requestedLanguage)==0)selectedOffset=offset;}if(selectedOffset<0||selectedOffset>=f->Length()||f->Seek(selectedOffset,FS_SEEK_SET)<0||!ReadBE(f,magic))return false;}if(magic!=expectedMagic)return false;idVoiceTrackFileHeader h{};if(f->Read(&h,sizeof(h))!=sizeof(h))return false;h.Swap();if(h.numEvents<0||h.numFrameRefs<0)return false;
  idStr visemeName,character,textKey,subtitleKey,shaderName,animation;if(!ReadString(f,visemeName)||!ReadString(f,character)||!ReadString(f,textKey)||!ReadString(f,subtitleKey)||!ReadString(f,shaderName)||!ReadString(f,animation))return false;
  declVisemeSet=visemeName.IsEmpty()?nullptr:static_cast<const idDeclVisemeSet*>(idDeclVisemeSet::resourceList.FindWithInheritance(visemeName.data,true,true));characterIdentifier.Set(character.data);textLangKey.Set(textKey.data);textId.Set(textKey.data);subtitleLangKey.Set(subtitleKey.data);subtitleId.Set(subtitleKey.data);soundShader=shaderName.IsEmpty()||!VoiceSoundPreloadService()?nullptr:VoiceSoundPreloadService()->ResolveSoundShader(shaderName.data);animName=animation;
  timestamp=h.timestamp;textCRC=h.textCRC;lipsyncTextCRC=h.lipsyncTextCRC;sampleTimestamp=h.sampleTimestamp;sampleCRC=h.sampleCRC;blendInMS=h.blendInMS;blendOutMS=h.blendOutMS;soundOffsetMS=h.soundOffsetMS;visemeOffsetMS=h.visemeOffsetMS;animEventOffsetMS=h.animEventOffsetMS;moodEventOffsetMS=h.moodEventOffsetMS;gameEventOffsetMS=h.gameEventOffsetMS;textEventOffsetMS=h.textEventOffsetMS;weightScale=h.weightScale;numFrameRefs=h.numFrameRefs;defaultMood=static_cast<mood_t>(h.defaultMood);faceChannel=static_cast<faceChannel_t>(h.faceChannel);std::memcpy(&flags,&h.flags,sizeof(flags));approximateDuration=h.approximateDuration;
  if(!indexBuffer.Read(f))return false;if(numFrameRefs){frameRefs=new idFrameRef[numFrameRefs]{};for(int i=0;i<numFrameRefs;++i)if(!ReadBE(f,frameRefs[i].offset)||!ReadBE(f,frameRefs[i].numIndices))return false;}
  voiceEventManager.LoadBinary(f);events.SetNum(h.numEvents);if(!vtrEventHandle_t::ReadArray(f,events.list,events.Num())||!ReadBE(f,magic)||magic!=expectedMagic)return false;LoadAnims();visemeOffsetState=EVENTOFFSET_FORWARDS;offsetState=EVENTOFFSET_FORWARDS;rebuildFrameRefs=false;return true;
}
