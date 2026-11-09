#include "tungsten/game/clientgame/presentable/presentableplayer_shared.h"

#include <algorithm>

// DIA: idPresentablePlayer::EvaluateClientFire
// DIA: idPresentablePlayer::RecordClientFire
// DIA: idPresentablePlayer::ClientHitScanHit
// DIA: `dynamic initializer for 'p_fire_clientHitscans''
// DIA: `dynamic initializer for 'p_fire_clientHitscansNotify''
// DIA: `dynamic initializer for 'p_fire_clientFire''
// DIA: `dynamic initializer for 'p_fire_clientFireDebug''
// DIA: `dynamic initializer for 'p_fire_clientAmmoDebug''
// DIA: `dynamic initializer for 'cg_net_clientDeathMode''
// DIA: `dynamic initializer for 'cg_net_clientDeathDebug''
// DIA: `dynamic atexit destructor for 'p_fire_clientHitscans''
// DIA: `dynamic atexit destructor for 'p_fire_clientHitscansNotify''
// DIA: `dynamic atexit destructor for 'p_fire_clientFire''
// DIA: `dynamic atexit destructor for 'p_fire_clientFireDebug''
// DIA: `dynamic atexit destructor for 'p_fire_clientAmmoDebug''
// DIA: `dynamic atexit destructor for 'cg_net_clientDeathMode''
// DIA: `dynamic atexit destructor for 'cg_net_clientDeathDebug''

void idPresentablePlayer::EvaluateClientFire(unsigned int requestedFireCount,int startTime,int endTime){
  if(p_fire_clientFire.GetInteger()==0||endTime<startTime)return;
  if(requestedFireCount>clientFireCount+3)clientFireCount=requestedFireCount-1;
  if(clientFireCount<requestedFireCount){
    ++clientFireCount;
    WeaponFireFeedback();
  }else if(clientFireCount>requestedFireCount&&requestedFireCount!=0)clientFireCount=requestedFireCount;
}

void idPresentablePlayer::RecordClientFire(const idWeapon* weapon,const idVec3& start,const idMat3& fireAxis,const std::vector<clientHitScan_t>& hits){
  if(weapon==nullptr||p_fire_clientFire.GetInteger()==0)return;
  if(weapon!=GetEquippedWeapon())return;
  ++clientFireCount;
  clientFirePosition=start;
  clientFireAxis=fireAxis;
  if(p_fire_clientHitscans.GetInteger()!=0){
    for(const auto& hit:hits)ClientHitScanHit(hit.entityNumber,hit.jointNumber,hit.damage,hit.serverTime);
  }
}

void idPresentablePlayer::ClientHitScanHit(unsigned int hitEntityNumber,int jointNumber,float damage,int serverTimeOfHit){
  if(p_fire_clientHitscans.GetInteger()==0||hitEntityNumber==static_cast<unsigned int>(GetPeerIndex())||damage<0.0F)return;
  clientHits.push_back({hitEntityNumber,jointNumber,damage,serverTimeOfHit});
  DamageDealtFeedback(damage);
}

idCVar p_fire_clientHitscans("p_fire_clientHitscans","1",1,"allow client-authoritative hitscans");
idCVar p_fire_clientHitscansNotify("p_fire_clientHitscansNotify","0",1,"show client hitscan notifications");
idCVar p_fire_clientFire("p_fire_clientFire","1",1,"allow deferred client fire records");
idCVar p_fire_clientFireDebug("p_fire_clientFireDebug","0",2,"debug client fire reconciliation");
idCVar p_fire_clientAmmoDebug("p_fire_clientAmmoDebug","0",1,"debug client ammo reconciliation");
idCVar cg_net_clientDeathMode("cg_net_clientDeathMode","1",2,"client death hit acceptance mode");
idCVar cg_net_clientDeathDebug("cg_net_clientDeathDebug","0",1,"debug dead-client damage messages");
