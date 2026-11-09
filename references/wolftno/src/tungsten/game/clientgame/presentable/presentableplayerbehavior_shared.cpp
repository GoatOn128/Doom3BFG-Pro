#include "tungsten/game/clientgame/presentable/presentableplayer_shared.h"

#include <algorithm>

// DIA: idPresentablePlayer::PresentablePlayerBehavior_PostEvent
// DIA: idPresentablePlayer::PlayerBehavior_Shared_CheckBlocked

void idPresentablePlayer::PresentablePlayerBehavior_PostEvent(int event){lastBehaviorEvent=event;}
bool idPresentablePlayer::PlayerBehavior_Shared_CheckBlocked(const idVec3& start,const idVec3& end,float& blockDistance,int entityNumber,bool debug,float traceFraction){
  (void)entityNumber;(void)debug;
  const float distance=(end-start).Length();
  const float fraction=std::clamp(traceFraction,0.0F,1.0F);
  blockDistance=distance*fraction;
  return fraction<1.0F;
}
