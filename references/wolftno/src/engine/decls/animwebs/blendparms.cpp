#include "blendparms.h"
#include "engine/animation/animationpreload.h"

#include <algorithm>
#include <cmath>

namespace { std::int16_t ClampFrame(int v){return static_cast<std::int16_t>(std::clamp(v,-32767,32767));} }
blendParms_t::blendParms_t():parms{nullptr,nullptr,0,32767,0,0,0,0,0}{}
blendParms_t::blendParms_t(int ms):blendParms_t(){SetDurationMS(ms);}
int blendParms_t::GetDuration()const{return static_cast<std::uint16_t>(parms.destDuration);} int blendParms_t::GetDurationMS()const{return static_cast<int>(parms.destDuration*1000.0f/30.0f);}
bool blendParms_t::GetSourceFrames(const idMD6Anim*a,std::int16_t*f,std::int16_t*l)const{if(!f||!l)return false;std::int16_t first=parms.sourceStartFrame;if(parms.flags&1){if(!a)return false;first=static_cast<std::int16_t>((a->animData?a->animData->numFrames:0)-parms.sourceStartFrame-1);}*f=first;const int last=static_cast<int>(first)+parms.sourceDuration;*l=static_cast<std::int16_t>(last<0?0:last>32767?32767:last);return true;}
bool blendParms_t::GetDestFrames(const idMD6Anim*a,std::int16_t*f,std::int16_t*d)const{if(!f||!d)return false;*d=parms.destDuration;if(parms.flags&2){if(!a)return false;*f=static_cast<std::int16_t>((a->animData?a->animData->numFrames:0)-parms.destStartFrame-1);}else*f=parms.destStartFrame;return true;}
void blendParms_t::SetDestStartFrame(int f){parms.destStartFrame=ClampFrame(f);}void blendParms_t::SetDuration(int f){parms.destDuration=ClampFrame(f);}void blendParms_t::SetDurationMS(int ms){SetDuration(static_cast<int>(ms*30.0f/1000.0f));}
void blendParms_t::Parse(const char*,const char*,const char*,const char*,idParser&s){
  idToken t;s.ExpectTokenString("{");bool last=false;
  while(s.ReadToken(t)&&t!="}"){
    if(t=="sourceStartFrame")parms.sourceStartFrame=ClampFrame(s.ParseInt());
    else if(t=="sourceDuration")parms.sourceDuration=ClampFrame(s.ParseInt());
    else if(t=="destStartFrame")SetDestStartFrame(s.ParseInt());
    else if(t=="destDuration")SetDuration(s.ParseInt());
    else if(t=="duration"){parms.destDuration=static_cast<std::int16_t>(static_cast<int>(std::ceil(s.ParseInt()/1000.0f*30.0f)));parms.flags|=8;}
    else if(t=="durationf"){parms.destDuration=static_cast<std::int16_t>(s.ParseInt());parms.flags|=8;}
    else if(t=="srcFirstFrame"){int value=s.ParseInt();if(value>32767)value=32767;parms.sourceStartFrame=static_cast<std::int16_t>(value);parms.flags|=4;}
    else if(t=="srcLastFrame"){int value=s.ParseInt();if(value>32767)value=32767;parms.sourceDuration=static_cast<std::int16_t>(value);parms.flags|=8;last=true;}
    else if(t=="destFrame"){int value=s.ParseInt();if(value>32767)value=32767;parms.destStartFrame=static_cast<std::int16_t>(value);parms.flags|=8;}
    else if(t=="originBlend"){s.ExpectTokenType(TT_STRING,0,t);const char*n[]={"ORIGINBLEND_DEFAULT","ORIGINBLEND_SOURCE","ORIGINBLEND_DEST"};parms.originBlend=3;for(std::uint8_t i=0;i<3;++i)if(t==n[i])parms.originBlend=i;if(parms.originBlend==3)s.Error("Unknown origin blend type '%s'",t.data);}
    else if(t=="blendType"){s.ExpectTokenType(TT_STRING,0,t);const char*n[]={"BLEND_LINEAR","BLEND_EASEIN","BLEND_EASEOUT","BLEND_EASEIN_EASEOUT"};parms.blendType=4;for(std::uint8_t i=0;i<4;++i)if(t==n[i])parms.blendType=i;if(parms.blendType==4)s.Error("Unknown blend type '%s'",t.data);}
    else if(t=="sourceEndRelative")parms.flags|=1;
    else if(t=="destEndRelative")parms.flags|=2;
    else if(t=="matchPhase")parms.flags|=0x10;
    else if(t=="srcAnim"||t=="destAnim"){const bool isSource=t=="srcAnim";s.ExpectTokenType(TT_STRING,0,t);const idMD6Anim*anim=*t.data&&AnimationPreloadService()?AnimationPreloadService()->ResolveAnim(t.data):nullptr;if(isSource)parms.srcAnim=anim;else parms.destAnim=anim;}
    else s.Error("Unknown blendParms parameter '%s'",t.data);
  }
  if(last)parms.sourceDuration-=parms.sourceStartFrame;
}
