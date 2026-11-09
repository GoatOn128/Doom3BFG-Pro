#include "voicetrack_events.h"
#include "voicetrack.h"
#include "shared/idlib/csystems/cvarsystemlocal.h"
#include "shared/idlib/lib.h"

#include <algorithm>
#include <cmath>
#include <cstring>

namespace {
template <typename T> T ByteSwap(T value) {
  unsigned char source[sizeof(T)];
  unsigned char result[sizeof(T)];
  std::memcpy(source, &value, sizeof(T));
  for (std::size_t i = 0; i < sizeof(T); ++i)
    result[i] = source[sizeof(T) - i - 1];
  std::memcpy(&value, result, sizeof(T));
  return value;
}

template <typename T> bool WriteBE(idFile* file, T value) {
  value = ByteSwap(value);
  return file && file->Write(&value, sizeof(value)) == sizeof(value);
}
template <typename T> bool ReadBE(idFile* file, T& value) {
  if (!file || file->Read(&value, sizeof(value)) != sizeof(value))
    return false;
  value = ByteSwap(value);
  return true;
}
bool WriteString(idFile* file, const char* value) {
  const std::int32_t length = idStr::Length(value);
  return file && file->Write(&length, sizeof(length)) == sizeof(length) &&
         (!length || file->Write(value, length) == static_cast<std::size_t>(length));
}
bool ReadString(idFile* file, idStr& value) {
  std::int32_t length = 0;
  if (!file || file->Read(&length, sizeof(length)) != sizeof(length) ||
      length < 0 || length > 16 * 1024 * 1024)
    return false;
  value.EnsureAlloced(static_cast<int>(length + 1), false);
  if (length && file->Read(value.data, length) != length)
    return false;
  value.data[length] = '\0';
  value.len = static_cast<int>(length);
  return true;
}

idCVar vo_extraVisemeDuration("vo_extraVisemeDuration", "0", CVAR_INTEGER,
                              "extra duration to add to visemes");
void Print(idFile_String* file, const char* format, ...) {
  if (!file)
    return;
  std::va_list args;
  va_start(args, format);
  file->VPrintf(format, args);
  va_end(args);
}
idStr Escape(const char* text) {
  idStr result;
  for (const char* p = text ? text : ""; *p; ++p) {
    if (*p == '\\' || *p == '\"' || *p == '\'')
      result.Append('\\');
    if (*p == '\r') result.Append("\\r");
    else if (*p == '\n') result.Append("\\n");
    else result.Append(*p);
  }
  return result;
}
template <typename T> void CondenseEvents(idList<T, 66>& list, idList<vtrEventHandle_t, 66>& handles) {
  int removed = 0;
  for (int index = list.Num() - 1; index >= 0; --index) {
    if (list[index].eventType != VOICE_EVENT_INVALID)
      continue;
    ++removed;
    for (int h = handles.Num() - 1; h >= 0; --h) {
      if (handles[h].index == index) {
        for (int n = h + 1; n < handles.Num(); ++n)
          handles[n - 1] = handles[n];
        handles.num--;
      } else if (handles[h].index > index) {
        handles[h].index--;
      }
    }
    for (int n = index + 1; n < list.Num(); ++n)
      list[n - 1] = list[n];
    list.num--;
  }
  // Retail's template performs this second contraction after each invalid
  // slot was removed, then shrinks capacity to the resulting count.
  if (removed > 0)
    list.SetNum(std::max(0, list.Num() - removed));
  list.ResizeForPort(list.Num());
}
}

const char* const idVoiceEvent::voiceEventNames[VOICE_EVENT_INVALID] = {
    "visemeSimple", "visemeFrame", "gameEvent", "text", "punctuation", "anim", "mood"};
const char* idViseme_Simple::NAME = idVoiceEvent::voiceEventNames[0];
const char* idViseme_Frame::NAME = idVoiceEvent::voiceEventNames[1];
const char* idVoiceGameEvent::NAME = idVoiceEvent::voiceEventNames[2];
const char* idVoiceTextEvent::NAME = idVoiceEvent::voiceEventNames[3];
const char* idVoicePunctuationEvent::NAME = idVoiceEvent::voiceEventNames[4];
const char* idVoiceAnimEvent::NAME = idVoiceEvent::voiceEventNames[5];
const char* idVoiceMoodEvent::NAME = idVoiceEvent::voiceEventNames[6];

idVoiceEvent::~idVoiceEvent() = default;
float idVoiceEvent::GetWeight(int, int) const { return 1.0f; }
bool idVoiceEvent::IsWithinInterval(int begin, int end) const {
  const int eventEnd = startMS + durationMS;
  return (end >= startMS && end < eventEnd) || (begin >= startMS && begin < eventEnd) ||
         (startMS >= begin && startMS < end) || (eventEnd >= begin && eventEnd < end);
}
void idVoiceEvent::Write(const idVoiceTrack*, idFile_String&) const {}
void idVoiceEvent::Parse(idVoiceTrack*, idLexer&) {}
bool idVoiceEvent::WriteBinary(idFile* file) const {
  return WriteBE(file, static_cast<std::int32_t>(startMS)) && WriteBE(file, durationMS);
}
bool idVoiceEvent::LoadBinary(idFile* file) {
  std::int32_t start = 0;
  return ReadBE(file, start) && (startMS = start, ReadBE(file, durationMS));
}

idViseme_Simple::idViseme_Simple()
    : idVisemeEvent(VOICE_EVENT_VISEME_SIMPLE,255), peakTimeMS(0), peakWeight(0) {}
const char* idViseme_Simple::GetName() const { return NAME; }
float idViseme_Simple::GetWeight(int curTimeMS, int) const {
  const int relative = curTimeMS - startMS;
  const float shape=relative>=peakTimeMS
      ?1.0f-static_cast<float>(relative-peakTimeMS)/static_cast<float>(vo_extraVisemeDuration.GetInteger()+durationMS-peakTimeMS)
      :static_cast<float>(relative)/static_cast<float>(peakTimeMS);
  return(static_cast<float>(peakWeight)/255.0f)*shape;
}
void idViseme_Simple::Write(const idVoiceTrack* track, idFile_String& file) const {
  const char* name = track ? track->GetVisemeName(visemeType) : "invalid";
  Print(&file, "\t\t%s %s {\n\t\t\tstart %d\n\t\t\tduration %u\n\t\t\tpeakTime %u\n\t\t\tpeakWeight %u\n\t\t}\n",
        GetName(), name, startMS, durationMS, peakTimeMS,
        static_cast<unsigned>(peakWeight));
}
void idViseme_Simple::Parse(idVoiceTrack* track, idLexer& source) {
  if (!track || !track->declVisemeSet) {
    source.Error("idViseme_Simple::Parse - invalid viseme set.");
    return;
  }
  idToken token;
  source.ExpectTokenType(TT_NAME, 0, token);
  visemeType = track->GetVisemeForName(token.data);
  if (visemeType == 255) {
    idLib::Warning("idViseme_Simple::Parse - unknown viseme type '%s'. Viseme forced to silence.",
                   token.data);
    visemeType = track->declVisemeSet->silenceViseme;
  }
  source.ExpectTokenString("{");
  source.ExpectTokenString("start");
  startMS = source.ParseInt();
  source.ExpectTokenString("duration");
  durationMS = static_cast<std::uint16_t>(source.ParseInt());
  source.ExpectTokenString("peakTime");
  int peakTime = source.ParseInt();
  if (static_cast<unsigned>(peakTime) > 65535u)
    idLib::Warning("idViseme_Simple::Parse - peakTime out of range");
  peakTimeMS = static_cast<std::uint16_t>(std::clamp(peakTime, 0, 65535));
  source.ExpectTokenString("peakWeight");
  int weight = source.ParseInt();
  if (static_cast<unsigned>(weight) > 255u)
    idLib::Warning("idViseme_Simple::Parse - peakWeight out of range");
  peakWeight = static_cast<std::uint8_t>(std::clamp(weight, 0, 255));
  source.ExpectTokenString("}");
}
bool idViseme_Simple::WriteBinary(idFile* file) const {
  return idVoiceEvent::WriteBinary(file) && file->Write(&visemeType, 1) == 1 &&
         WriteBE(file, peakTimeMS) && file->Write(&peakWeight, 1) == 1;
}
bool idViseme_Simple::LoadBinary(idFile* file) {
  return idVoiceEvent::LoadBinary(file) && file->Read(&visemeType, 1) == 1 && visemeType != 255 &&
         ReadBE(file, peakTimeMS) && file->Read(&peakWeight, 1) == 1;
}
bool idViseme_Simple::operator==(const idViseme_Simple& o) const {
  return eventType == o.eventType && startMS == o.startMS && durationMS == o.durationMS &&
         visemeType == o.visemeType && peakTimeMS == o.peakTimeMS && peakWeight == o.peakWeight;
}
idViseme_Simple& idViseme_Simple::operator=(const idViseme_Simple& o) {
  if (this != &o) { startMS=o.startMS; durationMS=o.durationMS; eventType=o.eventType;
    visemeType=o.visemeType; peakTimeMS=o.peakTimeMS; peakWeight=o.peakWeight; }
  return *this;
}

idViseme_Frame::idViseme_Frame():idVisemeEvent(VOICE_EVENT_VISEME_FRAME,255),numWeights(0),weights(nullptr){}
idViseme_Frame::idViseme_Frame(const idViseme_Frame&o):idViseme_Frame(){*this=o;}
idViseme_Frame::~idViseme_Frame(){delete[]weights;}
const char*idViseme_Frame::GetName()const{return NAME;}
float idViseme_Frame::GetWeight(int curTimeMS,int gameMsPerFrame)const{
  if(!weights||!numWeights||gameMsPerFrame<=0||curTimeMS<startMS)return 0.0f;
  const int frame=(curTimeMS-startMS)/gameMsPerFrame;
  return frame>=0&&frame<numWeights?static_cast<float>(weights[frame])/255.0f:0.0f;
}
void idViseme_Frame::Write(const idVoiceTrack*track,idFile_String&file)const{
  const char*name=track?track->GetVisemeName(visemeType):"invalid";
  Print(&file,"\t\t%s %s {\n\t\t\tstart %d\n\t\t\tduration %u\n\t\t\tweights",GetName(),name,startMS,durationMS);
  for(int i=0;i<numWeights;++i)Print(&file," %u",static_cast<unsigned>(weights[i]));
  Print(&file,"\n\t\t}\n");
}
void idViseme_Frame::Parse(idVoiceTrack*track,idLexer&source){
  idToken token;source.ExpectAnyToken(token);visemeType=track?track->GetVisemeForName(token.data):255;source.ExpectTokenString("{");idList<std::uint8_t> parsed;
  while(source.ReadToken(token)&&token!="}"){if(token=="start")startMS=source.ParseInt();else if(token=="duration")durationMS=static_cast<std::uint16_t>(source.ParseInt());else if(token=="weights"){while(source.ReadTokenOnLine(token)){token.NumberValue();parsed.Append(static_cast<std::uint8_t>(std::clamp<int>(static_cast<int>(token.intvalue),0,255)));}}else source.Error("Unknown token '%s' in frame viseme",token.data);}
  delete[]weights;numWeights=static_cast<std::uint8_t>(std::min(parsed.Num(),255));weights=numWeights?new std::uint8_t[numWeights]:nullptr;for(int i=0;i<numWeights;++i)weights[i]=parsed[i];
}
bool idViseme_Frame::WriteBinary(idFile*file)const{return idVoiceEvent::WriteBinary(file)&&file->Write(&visemeType,1)==1&&file->Write(&numWeights,1)==1&&(!numWeights||file->Write(weights,numWeights)==numWeights);}
bool idViseme_Frame::LoadBinary(idFile*file){if(!idVoiceEvent::LoadBinary(file)||file->Read(&visemeType,1)!=1||visemeType==255||file->Read(&numWeights,1)!=1)return false;delete[]weights;weights=numWeights?new std::uint8_t[numWeights]:nullptr;return!numWeights||file->Read(weights,numWeights)==numWeights;}
std::size_t idViseme_Frame::Memory()const{return sizeof(*this)+numWeights;}
bool idViseme_Frame::operator==(const idViseme_Frame&o)const{return eventType==o.eventType&&startMS==o.startMS&&durationMS==o.durationMS&&visemeType==o.visemeType&&numWeights==o.numWeights&&(!numWeights||std::memcmp(weights,o.weights,numWeights)==0);}
idViseme_Frame&idViseme_Frame::operator=(const idViseme_Frame&o){if(this!=&o){startMS=o.startMS;durationMS=o.durationMS;eventType=o.eventType;visemeType=o.visemeType;delete[]weights;numWeights=o.numWeights;weights=numWeights?new std::uint8_t[numWeights]:nullptr;if(numWeights)std::memcpy(weights,o.weights,numWeights);}return*this;}

idVarArgs2::idVarArgs2():numArgs(0),argSize(0),buffSize(0),argOffsets{},argTypes{},argExTypes{},args(nullptr){}
idVarArgs2::idVarArgs2(const idVarArgs2&other):idVarArgs2(){*this=other;}
idVarArgs2&idVarArgs2::operator=(const idVarArgs2&other){if(this==&other)return*this;Clear();if(other.buffSize){args=new std::uint8_t[other.buffSize];if(other.argSize)std::memcpy(args,other.args,other.argSize);argSize=other.argSize;buffSize=other.buffSize;}numArgs=other.numArgs;std::memcpy(argOffsets,other.argOffsets,sizeof(argOffsets));std::memcpy(argTypes,other.argTypes,sizeof(argTypes));std::memcpy(argExTypes,other.argExTypes,sizeof(argExTypes));return*this;}
idVarArgs2::~idVarArgs2(){delete[]args;}
void idVarArgs2::Clear(){delete[]args;args=nullptr;numArgs=argSize=buffSize=0;std::memset(argOffsets,0,sizeof(argOffsets));std::memset(argTypes,0,sizeof(argTypes));std::memset(argExTypes,0,sizeof(argExTypes));}
bool idVarArgs2::Append(const char*value){if(numArgs>=2)return false;if(!value)value="";const std::size_t length=static_cast<std::size_t>(idStr::Length(value))+1;if(static_cast<std::size_t>(argSize)+length>65535)return false;const std::uint16_t needed=static_cast<std::uint16_t>(argSize+length);if(needed>buffSize){std::uint16_t capacity=static_cast<std::uint16_t>(std::max<int>(needed,std::max<int>(16,buffSize*2)));auto*replacement=new std::uint8_t[capacity];if(argSize)std::memcpy(replacement,args,argSize);delete[]args;args=replacement;buffSize=capacity;}argOffsets[numArgs]=argSize;std::memcpy(args+argSize,value,length);argSize=needed;++numArgs;return true;}
const char*idVarArgs2::operator[](int index)const{return index>=0&&index<numArgs&&args?reinterpret_cast<const char*>(args+argOffsets[index]):"";}

idVoiceGameEvent::idVoiceGameEvent() : idVoiceEvent(VOICE_EVENT_GAME), eventNum(-1) {}
idVoiceGameEvent::~idVoiceGameEvent() = default;
const char* idVoiceGameEvent::GetName() const { return NAME; }
float idVoiceGameEvent::GetWeight(int,int)const{return 1.0f;}
void idVoiceGameEvent::Write(const idVoiceTrack*, idFile_String&) const {}
void idVoiceGameEvent::Parse(idVoiceTrack*, idLexer&) {}
bool idVoiceGameEvent::WriteBinary(idFile* file) const {
  return idVoiceEvent::WriteBinary(file);
}
bool idVoiceGameEvent::LoadBinary(idFile* file) {
  return idVoiceEvent::LoadBinary(file);
}
std::size_t idVoiceGameEvent::Memory() const { return args.Memory()+74; }
bool idVoiceGameEvent::operator==(const idVoiceGameEvent& o) const { if(eventType!=o.eventType||startMS!=o.startMS||durationMS!=o.durationMS||eventNum!=o.eventNum||args.Num()!=o.args.Num())return false; for(int i=0;i<args.Num();++i)if(idStr::Cmp(args[i],o.args[i])!=0)return false;return true; }
idVoiceGameEvent& idVoiceGameEvent::operator=(const idVoiceGameEvent& o) { if(this!=&o){startMS=o.startMS;durationMS=o.durationMS;eventType=o.eventType;eventNum=o.eventNum;args=o.args;}return *this; }

idVoiceTextEvent::idVoiceTextEvent() : idVoiceEvent(VOICE_EVENT_TEXT) {}
const char* idVoiceTextEvent::GetName() const { return NAME; }
float idVoiceTextEvent::GetWeight(int time, int) const { return time>=startMS&&time<=startMS+durationMS?1.0f:0.0f; }
void idVoiceTextEvent::Write(const idVoiceTrack*, idFile_String& file) const { idStr escaped=Escape(text.data); Print(&file,"\t\t%s \"%s\" {\n\t\t\tstart %d\n\t\t\tduration %u\n\t\t}\n",GetName(),escaped.data,startMS,durationMS); }
void idVoiceTextEvent::Parse(idVoiceTrack*, idLexer& source) { idToken token;source.ExpectTokenType(TT_STRING,0,token);text=token;source.ExpectTokenString("{");source.ExpectTokenString("start");startMS=source.ParseInt();source.ExpectTokenString("duration");durationMS=static_cast<std::uint16_t>(source.ParseInt());source.ExpectTokenString("}"); }
bool idVoiceTextEvent::WriteBinary(idFile* file) const { return idVoiceEvent::WriteBinary(file)&&WriteString(file,text.data); }
bool idVoiceTextEvent::LoadBinary(idFile* file) { return idVoiceEvent::LoadBinary(file)&&ReadString(file,text); }
std::size_t idVoiceTextEvent::Memory() const { return text.Size()+16; }
bool idVoiceTextEvent::operator==(const idVoiceTextEvent& o) const { return eventType==o.eventType&&startMS==o.startMS&&durationMS==o.durationMS&&text==o.text; }
idVoiceTextEvent& idVoiceTextEvent::operator=(const idVoiceTextEvent& o){if(this!=&o){startMS=o.startMS;durationMS=o.durationMS;eventType=o.eventType;text=o.text;}return *this;}

idVoicePunctuationEvent::idVoicePunctuationEvent() : idVoiceEvent(VOICE_EVENT_PUNCTUATION), punctuation(0) {}
const char* idVoicePunctuationEvent::GetName() const{return NAME;}
void idVoicePunctuationEvent::Write(const idVoiceTrack*,idFile_String& file)const{Print(&file,"\t\t%s \"%s%c\" {\n\t\t\tstart %d\n\t\t\tduration %u\n\t\t}\n",GetName(),punctuation=='\"'||punctuation=='\\'?"\\":"",punctuation,startMS,durationMS);}
void idVoicePunctuationEvent::Parse(idVoiceTrack*,idLexer& source){idToken token;source.ExpectTokenType(TT_STRING,0,token);punctuation=token.Length()?token[0]:0;if(!punctuation)source.Error("NULL punctuation found");source.ExpectTokenString("{");source.ExpectTokenString("start");startMS=source.ParseInt();source.ExpectTokenString("duration");durationMS=static_cast<std::uint16_t>(source.ParseInt());source.ExpectTokenString("}");}
bool idVoicePunctuationEvent::WriteBinary(idFile* file)const{return idVoiceEvent::WriteBinary(file)&&file->Write(&punctuation,1)==1;}
bool idVoicePunctuationEvent::LoadBinary(idFile* file){return idVoiceEvent::LoadBinary(file)&&file->Read(&punctuation,1)==1;}
bool idVoicePunctuationEvent::operator==(const idVoicePunctuationEvent&o)const{return eventType==o.eventType&&startMS==o.startMS&&durationMS==o.durationMS&&punctuation==o.punctuation;}
idVoicePunctuationEvent&idVoicePunctuationEvent::operator=(const idVoicePunctuationEvent&o){if(this!=&o){startMS=o.startMS;durationMS=o.durationMS;eventType=o.eventType;punctuation=o.punctuation;}return *this;}

constexpr int RATE_FRACTIONAL_BITS=8; constexpr std::uint16_t RATE_FRACTIONAL_MASK=0x00ff; constexpr std::uint16_t RATE_WHOLE_MASK=static_cast<std::uint16_t>(~RATE_FRACTIONAL_MASK); constexpr float FRACTIONAL_STEP=1.0f/static_cast<float>(RATE_FRACTIONAL_MASK);
idVoiceAnimEvent::idVoiceAnimEvent():idVoiceEvent(VOICE_EVENT_ANIM),channel(static_cast<std::int16_t>(FACE_CHANNEL_MAX)),blendInDurationMS(250),blendOutDurationMS(250),startFrame(0),rate(1<<RATE_FRACTIONAL_BITS),looping(false){}
const char*idVoiceAnimEvent::GetName()const{return NAME;} float idVoiceAnimEvent::GetRate()const{return ((rate&RATE_WHOLE_MASK)>>RATE_FRACTIONAL_BITS)+(rate&RATE_FRACTIONAL_MASK)*FRACTIONAL_STEP;}
void idVoiceAnimEvent::Write(const idVoiceTrack*,idFile_String&file)const{Print(&file,"\t\t%s \"%s\" {\n\t\t\tstart %d\n\t\t\tduration %u\n\t\t\tblendInDurationMS %u\n\t\t\tblendOutDurationMS %u\n\t\t\tchannel \"%s\"\n\t\t\tstartFrame %u\n\t\t\trate %.2f\n\t\t\tloop %d\n\t\t}\n",GetName(),animName.str,startMS,durationMS,blendInDurationMS,blendOutDurationMS,idVoiceTrack::GetFaceChannelName(static_cast<faceChannel_t>(channel)),startFrame,GetRate(),looping?1:0);}
void idVoiceAnimEvent::Parse(idVoiceTrack*,idLexer&source){idToken token;source.ExpectTokenType(TT_STRING,0,token);animName.Set(token.data);if(!animName.str||!*animName.str)source.Error("idVoiceAnimEvent - empty animation name");source.ExpectTokenString("{");int out=-1;while(source.ReadToken(token)&&token!="}"){if(token=="start")startMS=source.ParseInt();else if(token=="duration")durationMS=static_cast<std::uint16_t>(source.ParseInt());else if(token=="blendInDurationMS")blendInDurationMS=static_cast<std::uint16_t>(source.ParseInt());else if(token=="blendOutDurationMS")out=source.ParseInt();else if(token=="channel"){source.ExpectTokenType(TT_STRING,0,token);channel=static_cast<std::int16_t>(idVoiceTrack::GetFaceChannelForName(token.data));if(channel==FACE_CHANNEL_MAX)source.Error("Invalid face channel '%s'",token.data);}else if(token=="loop")looping=source.ParseBool();else if(token=="startFrame")startFrame=static_cast<std::uint16_t>(source.ParseInt());else if(token=="rate"){float v=source.ParseFloat();float whole=std::floor(v);rate=static_cast<std::uint16_t>((static_cast<int>(whole)<<8)|static_cast<int>((v-whole)/FRACTIONAL_STEP));}else source.Error("Unknown token '%s' in anim event.",token.data);}blendOutDurationMS=out>=0?static_cast<std::uint16_t>(out):blendInDurationMS;}
bool idVoiceAnimEvent::WriteBinary(idFile*f)const{return idVoiceEvent::WriteBinary(f)&&WriteString(f,animName.str)&&WriteBE(f,static_cast<std::int16_t>(channel))&&WriteBE(f,blendInDurationMS)&&WriteBE(f,blendOutDurationMS)&&WriteBE(f,startFrame)&&WriteBE(f,rate)&&f->Write(&looping,1)==1;}
bool idVoiceAnimEvent::LoadBinary(idFile*f){std::int16_t c;idStr n;if(!idVoiceEvent::LoadBinary(f)||!ReadString(f,n)||!ReadBE(f,c)||!ReadBE(f,blendInDurationMS)||!ReadBE(f,blendOutDurationMS)||!ReadBE(f,startFrame)||!ReadBE(f,rate)||f->Read(&looping,1)!=1)return false;animName.Set(n.data);channel=c;return true;}
std::size_t idVoiceAnimEvent::Memory()const{return sizeof(*this)+idStr::Length(animName.str);} bool idVoiceAnimEvent::operator==(const idVoiceAnimEvent&o)const{return eventType==o.eventType&&startMS==o.startMS&&durationMS==o.durationMS&&animName.str==o.animName.str;}
idVoiceAnimEvent&idVoiceAnimEvent::operator=(const idVoiceAnimEvent&o){if(this!=&o){startMS=o.startMS;durationMS=o.durationMS;eventType=o.eventType;animName=o.animName;channel=o.channel;blendInDurationMS=o.blendInDurationMS;blendOutDurationMS=o.blendOutDurationMS;startFrame=o.startFrame;rate=o.rate;looping=o.looping;}return*this;}

idVoiceMoodEvent::idVoiceMoodEvent():idVoiceEvent(VOICE_EVENT_MOOD),mood(MOOD_NONE){} const char*idVoiceMoodEvent::GetName()const{return NAME;}
void idVoiceMoodEvent::Write(const idVoiceTrack*,idFile_String&f)const{Print(&f,"\t\t%s \"%s\" {\n\t\t\tstart %d\n\t\t\tduration %u\n\t\t}\n",GetName(),idVoiceTrack::GetMoodName(mood),startMS,durationMS);}
void idVoiceMoodEvent::Parse(idVoiceTrack*,idLexer&s){idToken t;s.ExpectTokenType(TT_STRING,0,t);mood=idVoiceTrack::GetMoodForName(t.data);s.ExpectTokenString("{");s.ExpectTokenString("start");startMS=s.ParseInt();s.ExpectTokenString("duration");durationMS=static_cast<std::uint16_t>(s.ParseInt());s.ExpectTokenString("}");}
bool idVoiceMoodEvent::WriteBinary(idFile*f)const{std::uint8_t m=static_cast<std::uint8_t>(mood);return idVoiceEvent::WriteBinary(f)&&f->Write(&m,1)==1;} bool idVoiceMoodEvent::LoadBinary(idFile*f){std::uint8_t m;if(!idVoiceEvent::LoadBinary(f)||f->Read(&m,1)!=1||m>=MOOD_MAX){mood=MOOD_NONE;return false;}mood=static_cast<mood_t>(m);return true;}
bool idVoiceMoodEvent::operator==(const idVoiceMoodEvent&o)const{return eventType==o.eventType&&startMS==o.startMS&&durationMS==o.durationMS&&mood==o.mood;} idVoiceMoodEvent&idVoiceMoodEvent::operator=(const idVoiceMoodEvent&o){if(this!=&o){startMS=o.startMS;durationMS=o.durationMS;eventType=o.eventType;mood=o.mood;}return*this;}

void idVoiceEventManagerFileHeader::Swap(){version=ByteSwap(version);numViseme=ByteSwap(numViseme);numGame=ByteSwap(numGame);numText=ByteSwap(numText);numPunc=ByteSwap(numPunc);numAnim=ByteSwap(numAnim);numMood=ByteSwap(numMood);}
idVoiceEventManager::idVoiceEventManager(){eventsVisemeSimple.granularity=0;eventsGame.granularity=0;eventsText.granularity=0;eventsPunc.granularity=0;eventsAnim.granularity=0;eventsMood.granularity=0;} idVoiceEventManager::~idVoiceEventManager()=default;
idVoiceEvent*idVoiceEventManager::GetEvent(vtrEventHandle_t h){return const_cast<idVoiceEvent*>(static_cast<const idVoiceEventManager*>(this)->GetEvent(h));}
const idVoiceEvent*idVoiceEventManager::GetEvent(vtrEventHandle_t h)const{if(h.index<0)return nullptr;switch(h.eventType){case 0:return h.index<eventsVisemeSimple.Num()?&eventsVisemeSimple[h.index]:nullptr;case 2:return h.index<eventsGame.Num()?&eventsGame[h.index]:nullptr;case 3:return h.index<eventsText.Num()?&eventsText[h.index]:nullptr;case 4:return h.index<eventsPunc.Num()?&eventsPunc[h.index]:nullptr;case 5:return h.index<eventsAnim.Num()?&eventsAnim[h.index]:nullptr;case 6:return h.index<eventsMood.Num()?&eventsMood[h.index]:nullptr;default:return nullptr;}}
vtrEventHandle_t idVoiceEventManager::AllocEvent(const char*n){vtrEventHandle_t h{VOICE_EVENT_INVALID,-1};int type=0;while(type<VOICE_EVENT_INVALID&&idStr::Cmp(n,idVoiceEvent::voiceEventNames[type])!=0)++type;auto slot=[](auto&list){for(int i=0;i<list.Num();++i)if(list[i].eventType==VOICE_EVENT_INVALID)return i;return list.Num();};switch(type){case 0:h.index=static_cast<std::int16_t>(slot(eventsVisemeSimple));if(h.index==eventsVisemeSimple.Num())eventsVisemeSimple.Alloc();h.eventType=0;break;case 2:h.index=static_cast<std::int16_t>(slot(eventsGame));if(h.index==eventsGame.Num())eventsGame.Alloc();h.eventType=2;break;case 3:h.index=static_cast<std::int16_t>(slot(eventsText));if(h.index==eventsText.Num())eventsText.Alloc();h.eventType=3;break;case 4:h.index=static_cast<std::int16_t>(slot(eventsPunc));if(h.index==eventsPunc.Num())eventsPunc.Alloc();h.eventType=4;break;case 5:h.index=static_cast<std::int16_t>(slot(eventsAnim));if(h.index==eventsAnim.Num())eventsAnim.Alloc();h.eventType=5;break;case 6:h.index=static_cast<std::int16_t>(slot(eventsMood));if(h.index==eventsMood.Num())eventsMood.Alloc();h.eventType=6;break;}return h;}
void idVoiceEventManager::Free(idList<vtrEventHandle_t,66>&l){eventsVisemeSimple.ClearFree();eventsGame.ClearFree();eventsText.ClearFree();eventsPunc.ClearFree();eventsAnim.ClearFree();eventsMood.ClearFree();l.ClearFree();}
void idVoiceEventManager::Condense(idList<vtrEventHandle_t,66>&l){CondenseEvents(eventsVisemeSimple,l);CondenseEvents(eventsGame,l);CondenseEvents(eventsText,l);CondenseEvents(eventsPunc,l);CondenseEvents(eventsAnim,l);CondenseEvents(eventsMood,l);}
void idVoiceEventManager::WriteBinary(idFile*f)const{idVoiceEventManagerFileHeader h{1u,(std::uint32_t)eventsVisemeSimple.Num(),(std::uint32_t)eventsGame.Num(),(std::uint32_t)eventsText.Num(),(std::uint32_t)eventsPunc.Num(),(std::uint32_t)eventsAnim.Num(),(std::uint32_t)eventsMood.Num()};h.Swap();f->Write(&h,sizeof(h));for(int i=0;i<eventsVisemeSimple.Num();++i)eventsVisemeSimple[i].WriteBinary(f);for(int i=0;i<eventsGame.Num();++i)eventsGame[i].WriteBinary(f);for(int i=0;i<eventsText.Num();++i)eventsText[i].WriteBinary(f);for(int i=0;i<eventsPunc.Num();++i)eventsPunc[i].WriteBinary(f);for(int i=0;i<eventsAnim.Num();++i)eventsAnim[i].WriteBinary(f);for(int i=0;i<eventsMood.Num();++i)eventsMood[i].WriteBinary(f);}
void idVoiceEventManager::LoadBinary(idFile*f){idVoiceEventManagerFileHeader h{};if(f->Read(&h,sizeof(h))!=sizeof(h))return;h.Swap();eventsVisemeSimple.SetNum(h.numViseme);eventsGame.SetNum(h.numGame);eventsText.SetNum(h.numText);eventsPunc.SetNum(h.numPunc);eventsAnim.SetNum(h.numAnim);eventsMood.SetNum(h.numMood);for(int i=0;i<eventsVisemeSimple.Num();++i)eventsVisemeSimple[i].LoadBinary(f);for(int i=0;i<eventsGame.Num();++i)eventsGame[i].LoadBinary(f);for(int i=0;i<eventsText.Num();++i)eventsText[i].LoadBinary(f);for(int i=0;i<eventsPunc.Num();++i)eventsPunc[i].LoadBinary(f);for(int i=0;i<eventsAnim.Num();++i)eventsAnim[i].LoadBinary(f);for(int i=0;i<eventsMood.Num();++i)eventsMood[i].LoadBinary(f);}
bool vtrEventHandle_t::WriteArray(idFile*f,const vtrEventHandle_t*h,int n){for(int i=0;i<n;++i)if(!WriteBE(f,h[i].eventType)||!WriteBE(f,h[i].index))return false;return true;}
bool vtrEventHandle_t::ReadArray(idFile*f,vtrEventHandle_t*h,int n){for(int i=0;i<n;++i)if(!ReadBE(f,h[i].eventType)||!ReadBE(f,h[i].index))return false;return true;}
