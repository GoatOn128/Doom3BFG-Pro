#include "tungsten/game/clientgame/clientgame_reliable.h"

#include <sstream>

// DIA: idClientGameMsg::~idClientGameMsg
// DIA: idClientGameMsg::`scalar deleting destructor'
// DIA: idClientGameMsg_HandsEquip::GetMsgType
// DIA: idClientGameMsg_HandsEquip::PrintDebug
// DIA: idClientGameMsg_HandsAmmoChange::GetMsgType
// DIA: idClientGameMsg_HandsAmmoChange::PrintDebug
// DIA: idPlayer::IsLocallyControlled
// DIA: idClientGameMsg_DropItem::Execute
// DIA: idClientGameMsg_DropItem::PrintDebug
// DIA: idClientGameMsg_HandsThrow::ShouldExecuteOnInstigator
// DIA: idClientGameMsg_HandsThrow::Execute
// DIA: idClientGameMsg_HandsThrow::PrintDebug
// DIA: idClientGameMsg_HitScanHit::AddHit
// DIA: idClientGameMsg_PostGameScoreRecap::PrintDebug
// DIA: idClientGameMsg_DropItem::GetMsgType
// DIA: idClientGameMsg_DropItem::ShouldExecuteOnInstigator
// DIA: idClientGameMsg_QuickUse::GetMsgType
// DIA: idClientGameMsg_UseFocus::GetMsgType
// DIA: idClientGameMsg_MeleeHit::GetMsgType
// DIA: idClientGameMsg_ActorModifier::GetMsgType
// DIA: idClientGameMsg_ActorModifier::PrintDebug
// DIA: idClientGame::HandleReliableGameMsg
// DIA: idClientGameMsg_DropItem::Serialize
// DIA: idClientGameMsg_HandsEquip::Serialize
// DIA: idClientGameMsg_HandsAmmoChange::Serialize
// DIA: idClientGameMsg_HandsThrow::Serialize
// DIA: idClientGameMsg_HitScanHit::idClientGameMsg_HitScanHit
// DIA: `idClientGameMsg_HitScanHit::idClientGameMsg_HitScanHit'::`1'::dtor$0
// DIA: idClientGameMsg_HitScanHit::GetMsgType
// DIA: idList<idClientGameMsg_PostGameScoreRecap::scoreItem_t,5>::Resize
// DIA: idClientGameMsg_HitScanHit::idClientGameMsg_HitScanHit
// DIA: idClientGameMsg_VehicleCrosshairChangeTarget::GetMsgType
// DIA: idClientGameMsg_VehicleCrosshairChangeTarget::PrintDebug
// DIA: idClientGameMsg_VehicleWeaponChange::PrintDebug
// DIA: idClientGameMsg_UpdateStat::GetMsgType
// DIA: idClientGameMsg_UpdateStat::PrintDebug
// DIA: idList<idClientGameMsg_PostGameScoreRecap::scoreItem_t,5>::operator=
// DIA: idList<killAssistData_t,5>::Resize
// DIA: idClientGame::ExecuteQueuedClientGameMsgs
// DIA: idClientGameMsg_HitScanHit::Serialize
// DIA: idClientGameMsg_HitScanHit::Execute
// DIA: idClientGameMsg_HitScanHit::PrintDebug
// DIA: idClientGameMsg_QuickUse::Serialize
// DIA: idClientGameMsg_QuickUse::Execute
// DIA: idClientGameMsg_QuickUse::PrintDebug
// DIA: idClientGameMsg_VehicleCrosshairChangeTarget::Serialize
// DIA: idClientGameMsg_VehicleCrosshairChangeTarget::Execute
// DIA: idClientGameMsg_VehicleWeaponChange::Serialize
// DIA: idClientGameMsg_VehicleWeaponChange::Execute
// DIA: idClientGameMsg_UpdateStat::Serialize
// DIA: idClientGameMsg_UseFocus::Serialize
// DIA: idClientGameMsg_UseFocus::Execute
// DIA: idClientGameMsg_UseFocus::PrintDebug
// DIA: idClientGameMsg_MeleeHit::Serialize
// DIA: idClientGameMsg_MeleeHit::Execute
// DIA: idClientGameMsg_MeleeHit::PrintDebug
// DIA: idClientGameMsg_ActorModifier::Serialize
// DIA: idClientGameMsg_ActorModifier::Execute
// DIA: idWeapon::GetPresentableWeapon
// DIA: idClientGameMsg_HandsThrow::idClientGameMsg_HandsThrow
// DIA: idClientGameMsg_HandsThrow::GetMsgType
// DIA: idClientGameMsg_HandsThrow::~idClientGameMsg_HandsThrow
// DIA: idClientGameMsg_HandsThrow::`scalar deleting destructor'
// DIA: idClientGameMsg_PostGameScoreRecap::idClientGameMsg_PostGameScoreRecap
// DIA: idClientGameMsg_PostGameScoreRecap::GetMsgType
// DIA: idClientGameMsg_PostGameScoreRecap::~idClientGameMsg_PostGameScoreRecap
// DIA: idClientGameMsg_HandsEquip::Execute
// DIA: idClientGameMsg_HandsAmmoChange::Execute
// DIA: idClientGameMsg_PostGameScoreRecap::Serialize
// DIA: idClientGameMsg_PostGameScoreRecap::Execute
// DIA: idClientGameMsg_PlayerKilled::Serialize
// DIA: idClientGameMsg_PostGameScoreRecap::`scalar deleting destructor'
// DIA: idClientGameMsg_PlayerKilled::idClientGameMsg_PlayerKilled
// DIA: idClientGameMsg_PlayerKilled::GetMsgType
// DIA: idClientGameMsg_PlayerKilled::PrintDebug
// DIA: idClientGameMsg_PlayerKilled::~idClientGameMsg_PlayerKilled
// DIA: idClientGameMsg_PlayerKilled::`scalar deleting destructor'
// DIA: idClientGame::QueueReliableClientGameMsg
// DIA: idClientGame::NetReceiveReliable
// DIA: `idClientGame::NetReceiveReliable'::`1'::dtor$2
// DIA: `idClientGame::NetReceiveReliable'::`1'::dtor$12
// DIA: `idClientGame::NetReceiveReliable'::`1'::dtor$13
// DIA: `dynamic initializer for 'cg_reliable_debug''
// DIA: `dynamic initializer for 'cg_projectile_ignoreGrenades''
// DIA: `dynamic initializer for 'cg_projectile_grenadeDeathDropVelocity''
// DIA: `dynamic atexit destructor for 'cg_reliable_debug''
// DIA: `dynamic atexit destructor for 'cg_projectile_ignoreGrenades''
// DIA: `dynamic atexit destructor for 'cg_projectile_grenadeDeathDropVelocity''

idCVar cg_reliable_debug("cg_reliable_debug","0",0,"reliable message diagnostics");
idCVar cg_projectile_ignoreGrenades("cg_projectile_ignoreGrenades","0",0,"ignore grenade messages");
idCVar cg_projectile_grenadeDeathDropVelocity("cg_projectile_grenadeDeathDropVelocity","0",0,"grenade drop velocity");

namespace {
std::string Debug(const char* name,int a,int b=0){return std::string(name)+" a="+std::to_string(a)+" b="+std::to_string(b);}
void SerializeInts(idSerializer* s,int& a,int& b){if(s){s->SerializeInt(a);s->SerializeInt(b);}}
}

idClientGameMsg::~idClientGameMsg()=default;
bool idPlayer::IsLocallyControlled()const{return locallyControlled;}
idPresentableWeapon* idWeapon::GetPresentableWeapon()const{return presentableWeapon;}

int idClientGameMsg_HandsEquip::GetMsgType()const{return 34;}
std::string idClientGameMsg_HandsEquip::PrintDebug()const{return Debug("HandsEquip",playerNumber,itemId);}
void idClientGameMsg_HandsEquip::PrintDebugLine()const{(void)PrintDebug();}
void idClientGameMsg_HandsEquip::Serialize(idSerializer*s){if(s){s->SerializeInt(playerNumber);s->SerializeInt(itemId);s->SerializeInt(ammoId);}}
void idClientGameMsg_HandsEquip::Execute(idClientGame&g){g.HandleReliableOnlineGameEvent(PrintDebug());}
int idClientGameMsg_HandsAmmoChange::GetMsgType()const{return 35;}
std::string idClientGameMsg_HandsAmmoChange::PrintDebug()const{return Debug("HandsAmmoChange",playerNumber,amount);}
void idClientGameMsg_HandsAmmoChange::PrintDebugLine()const{(void)PrintDebug();}
void idClientGameMsg_HandsAmmoChange::Serialize(idSerializer*s){if(s){s->SerializeInt(playerNumber);s->SerializeInt(ammoId);s->SerializeInt(amount);}}
void idClientGameMsg_HandsAmmoChange::Execute(idClientGame&g){g.HandleReliableOnlineGameEvent(PrintDebug());}
int idClientGameMsg_DropItem::GetMsgType()const{return 42;}
bool idClientGameMsg_DropItem::ShouldExecuteOnInstigator()const{return false;}
std::string idClientGameMsg_DropItem::PrintDebug()const{return Debug("DropItem",playerNumber,itemId);}
void idClientGameMsg_DropItem::Serialize(idSerializer*s){if(s){s->SerializeInt(playerNumber);s->SerializeInt(itemId);s->SerializeInt(amount);}}
void idClientGameMsg_DropItem::Execute(idClientGame&g){g.HandleReliableOnlineGameEvent(PrintDebug());}

idClientGameMsg_HandsThrow::idClientGameMsg_HandsThrow()=default;
idClientGameMsg_HandsThrow::~idClientGameMsg_HandsThrow()=default;
int idClientGameMsg_HandsThrow::GetMsgType()const{return 36;}
bool idClientGameMsg_HandsThrow::ShouldExecuteOnInstigator()const{return false;}
std::string idClientGameMsg_HandsThrow::PrintDebug()const{return Debug("HandsThrow",playerNumber);}
void idClientGameMsg_HandsThrow::Serialize(idSerializer*s){if(s){s->SerializeInt(playerNumber);s->SerializeVec3(velocity);}}
void idClientGameMsg_HandsThrow::Execute(idClientGame&g){g.HandleReliableOnlineGameEvent(PrintDebug());}

idClientGameMsg_HitScanHit::idClientGameMsg_HitScanHit()=default;
idClientGameMsg_HitScanHit::idClientGameMsg_HitScanHit(int player):playerNumber(player){}
int idClientGameMsg_HitScanHit::GetMsgType()const{return 37;}
void idClientGameMsg_HitScanHit::AddHit(const clientHitScan_t&hit){hits.Append(hit);}
std::string idClientGameMsg_HitScanHit::PrintDebug()const{return Debug("HitScanHit",playerNumber,hits.Num());}
void idClientGameMsg_HitScanHit::Serialize(idSerializer*s){if(!s)return;s->SerializeInt(playerNumber);int count=hits.Num();s->SerializeInt(count);if(!s->writing)hits.SetNum(count);for(int i=0;i<count;++i){s->SerializeInt(reinterpret_cast<int&>(hits[i].entityNumber));s->SerializeInt(hits[i].jointNumber);s->SerializeFloat(hits[i].damage);s->SerializeInt(hits[i].serverTime);}}
void idClientGameMsg_HitScanHit::Execute(idClientGame&g){for(int i=0;i<hits.Num();++i){idTestFireResults result;result.hit=true;result.hitEntity=static_cast<int>(hits[i].entityNumber);result.damage=hits[i].damage;g.FinishHitScan(result);}g.HandleReliableOnlineGameEvent(PrintDebug());}

#define WOLF_SIMPLE_MESSAGE_IMPL(Class,Type,A,B) \
int Class::GetMsgType()const{return Type;} \
std::string Class::PrintDebug()const{return Debug(#Class,A,B);} \
void Class::Serialize(idSerializer*s){SerializeInts(s,A,B);} \
void Class::Execute(idClientGame&g){g.HandleReliableOnlineGameEvent(PrintDebug());}
WOLF_SIMPLE_MESSAGE_IMPL(idClientGameMsg_QuickUse,38,playerNumber,itemId)
WOLF_SIMPLE_MESSAGE_IMPL(idClientGameMsg_UseFocus,39,playerNumber,focusEntity)
WOLF_SIMPLE_MESSAGE_IMPL(idClientGameMsg_VehicleCrosshairChangeTarget,43,vehicle,target)
#undef WOLF_SIMPLE_MESSAGE_IMPL
std::string idClientGameMsg_VehicleWeaponChange::PrintDebug()const{return Debug("idClientGameMsg_VehicleWeaponChange",vehicle,weapon);}
void idClientGameMsg_VehicleWeaponChange::Serialize(idSerializer*s){SerializeInts(s,vehicle,weapon);}
void idClientGameMsg_VehicleWeaponChange::Execute(idClientGame&g){g.HandleReliableOnlineGameEvent(PrintDebug());}

int idClientGameMsg_MeleeHit::GetMsgType()const{return 41;}
std::string idClientGameMsg_MeleeHit::PrintDebug()const{return Debug("MeleeHit",attacker,victim);}
void idClientGameMsg_MeleeHit::Serialize(idSerializer*s){SerializeInts(s,attacker,victim);if(s)s->SerializeFloat(damage);}
void idClientGameMsg_MeleeHit::Execute(idClientGame&g){idTestFireResults hit;hit.hit=true;hit.hitEntity=victim;hit.damage=damage;g.FinishHitScan(hit);g.HandleReliableOnlineGameEvent(PrintDebug());}
int idClientGameMsg_ActorModifier::GetMsgType()const{return 46;}
std::string idClientGameMsg_ActorModifier::PrintDebug()const{return Debug("ActorModifier",actor);}
void idClientGameMsg_ActorModifier::Serialize(idSerializer*s){if(s){s->SerializeInt(actor);s->SerializeFloat(speed);s->SerializeFloat(damage);s->SerializeBool(disableOverdrive);}}
void idClientGameMsg_ActorModifier::Execute(idClientGame&g){if(auto*p=dynamic_cast<idPresentableActor*>(g.GetPresentableByIndex(actor)))p->HandleActorModifierMessage(speed,damage,disableOverdrive);g.HandleReliableOnlineGameEvent(PrintDebug());}
int idClientGameMsg_UpdateStat::GetMsgType()const{return 45;}
std::string idClientGameMsg_UpdateStat::PrintDebug()const{return Debug("UpdateStat",player,value);}
void idClientGameMsg_UpdateStat::Serialize(idSerializer*s){if(s){s->SerializeInt(player);s->SerializeInt(stat);s->SerializeInt(value);}}
void idClientGameMsg_UpdateStat::Execute(idClientGame&g){g.HandleReliableOnlineGameEvent(PrintDebug());}

idClientGameMsg_PostGameScoreRecap::idClientGameMsg_PostGameScoreRecap()=default;
idClientGameMsg_PostGameScoreRecap::~idClientGameMsg_PostGameScoreRecap()=default;
int idClientGameMsg_PostGameScoreRecap::GetMsgType()const{return 53;}
std::string idClientGameMsg_PostGameScoreRecap::PrintDebug()const{return Debug("PostGameScoreRecap",scores.Num());}
void idClientGameMsg_PostGameScoreRecap::Serialize(idSerializer*s){if(!s)return;int count=scores.Num();s->SerializeInt(count);if(!s->writing)scores.SetNum(count);for(int i=0;i<count;++i){s->SerializeInt(scores[i].player);s->SerializeInt(scores[i].score);}}
void idClientGameMsg_PostGameScoreRecap::Execute(idClientGame&g){g.HandleReliableOnlineGameEvent(PrintDebug());}
idClientGameMsg_PlayerKilled::idClientGameMsg_PlayerKilled()=default;
idClientGameMsg_PlayerKilled::~idClientGameMsg_PlayerKilled()=default;
int idClientGameMsg_PlayerKilled::GetMsgType()const{return 47;}
std::string idClientGameMsg_PlayerKilled::PrintDebug()const{return Debug("PlayerKilled",killer,victim);}
void idClientGameMsg_PlayerKilled::Serialize(idSerializer*s){if(!s)return;SerializeInts(s,killer,victim);int count=assists.Num();s->SerializeInt(count);if(!s->writing)assists.SetNum(count);for(int i=0;i<count;++i){s->SerializeInt(assists[i].player);s->SerializeFloat(assists[i].damage);}}
void idClientGameMsg_PlayerKilled::Execute(idClientGame&g){g.HandleKillMessage(PrintDebug());}

void idClientGame::HandleReliableGameMsg(idClientGameMsg&message){message.Execute(*this);}
void idClientGame::ExecuteQueuedClientGameMsgs(){auto pending=std::move(reliableQueue);reliableQueue.clear();for(auto*message:pending)if(message)HandleReliableGameMsg(*message);}
void idClientGame::QueueReliableClientGameMsg(idClientGameMsg*message){if(message)reliableQueue.push_back(message);}
void idClientGame::NetReceiveReliable(idClientGameMsg*message){if(!message)return;if(message->ShouldExecuteOnInstigator())HandleReliableGameMsg(*message);else QueueReliableClientGameMsg(message);}

template bool idList<idClientGameMsg_PostGameScoreRecap::scoreItem_t,5>::Resize(int);
template idList<idClientGameMsg_PostGameScoreRecap::scoreItem_t,5>& idList<idClientGameMsg_PostGameScoreRecap::scoreItem_t,5>::operator=(const idList&);
template bool idList<killAssistData_t,5>::Resize(int);


